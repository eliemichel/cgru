#ifndef WINNT
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <cstdlib>
#include <errno.h>
#include <sys/types.h>
#define closesocket close
#endif


#include "../include/afanasy.h"

#include <sstream>

#include "socketpool.h"
#include "logger.h"
#include "address.h"
#include "environment.h"

using namespace af;

SocketPool::SocketPool()
{}

bool SocketPool::get(const af::Address & i_address, int &socketfd, bool check)
{
    if( m_table.count(i_address) == 0)
    {
        if ( false == SocketPool::initSocket( i_address, socketfd))
            return false;
        m_global_mutex.Acquire();
        // We check again because another process could have populated the same address
        // But we did  not lock before because we assume initSocket to take some time
        // and try to reduce as much as possible the locking time.
        if( m_table.count(i_address) == 0)
        {
            m_table[i_address] = std::make_pair<int, DlMutex>(socketfd, DlMutex());
            for(std::vector<ReceivingMsgQueue*>::iterator it = m_subscribers.begin() ; it != m_subscribers.end() ; it++)
                (*it)->addSocket(socketfd);
        }
        else
            closesocket(socketfd);
        m_global_mutex.Release();
    }

    std::pair<int, DlMutex> p = m_table[i_address];
    DlMutex mutex = p.second;
    mutex.Acquire();

    // The socket may have been closed while acquiring the mutex
    if( m_table.count(i_address) == 0)
        return false;

    socketfd = p.first;

    if( check)
    {
        if( false == SocketPool::checkSocket( socketfd))
        {
            // Close the socket and try to get it again, but with no checking
            // this time to ensure that no infinite reccursion is possible
            AF_DEBUG << "close";
            this->close( i_address);
            return this->get( i_address, socketfd, false /* check */);
        }
    }

    return true;
}

bool SocketPool::set(const Address &i_address, int socketfd, bool force)
{
    m_global_mutex.Acquire();
    if( m_table.count(i_address) > 0)
    {
        if (false == force)
        {
            m_global_mutex.Release();
            return false;
        }
        else
        {
            std::pair<int, DlMutex> p = m_table[i_address];
            p.second.Acquire();
            ::close(p.first);
            p.second.Release();
        }
    }

    m_table[i_address] = std::make_pair<int, DlMutex>(socketfd, DlMutex());
    for(std::vector<ReceivingMsgQueue*>::iterator it = m_subscribers.begin() ; it != m_subscribers.end() ; it++)
        (*it)->addSocket(socketfd);

    m_global_mutex.Release();
    return true;
}

void SocketPool::release( const af::Address & i_address)
{
    assert(m_table.count(i_address) != 0);  // do not release a socket that you did not get!

    DlMutex mutex = m_table[i_address].second;
    mutex.Release();
}

void SocketPool::close( const af::Address & i_address)
{
    assert(m_table.count(i_address) != 0);  // do not release a socket that you did not get!

    std::pair<int, DlMutex> p = m_table[i_address];
    ::close(p.first);

    m_global_mutex.Acquire();
    m_table.erase(i_address);
    m_global_mutex.Release();

    p.second.Release();
    AF_DEBUG << "closed socket to " << i_address;
    assert(m_table.count(i_address) == 0);
}

std::ostream& SocketPool::output( std::ostream& stream) const
{
    stream << "af::SocketPool [";
    std::map<af::Address, std::pair<int, DlMutex>, AddressCompare>::const_iterator it;
    for( it = m_table.begin() ; it != m_table.end() ; it++)
    {
        if( it != m_table.begin())
            stream << ", ";
        stream << it->first;
    }
    stream << "]";
    return stream;
}

bool SocketPool::initSocket( const af::Address & i_address, int &socketfd)
{
    struct sockaddr_storage addr;
    if( false == i_address.setSocketAddress( &addr))
        return false;

    socketfd = socket( addr.ss_family, SOCK_STREAM, 0);
    if( socketfd < 0 )
    {
        AF_ERR << "socket: " << strerror(errno);
        return false;
    }

    AF_DEBUG << "trying to connect to " << i_address;

    if( af::Environment::getSockOpt_Dispatch_SO_RCVTIMEO_SEC() != -1 )
    {
        timeval so_timeo;
        so_timeo.tv_usec = 0;
        so_timeo.tv_sec = af::Environment::getSockOpt_Dispatch_SO_RCVTIMEO_SEC();
        if( setsockopt( socketfd, SOL_SOCKET, SO_RCVTIMEO, WINNT_TOCHAR(&so_timeo), sizeof(so_timeo)) != 0)
        {
            AF_WARN << "set socket SO_RCVTIMEO option failed (" << strerror(errno) << ") " << i_address;
        }
    }

    if( af::Environment::getSockOpt_Dispatch_SO_SNDTIMEO_SEC() != -1 )
    {
        timeval so_timeo;
        so_timeo.tv_usec = 0;
        so_timeo.tv_sec = af::Environment::getSockOpt_Dispatch_SO_SNDTIMEO_SEC();
        if( setsockopt( socketfd, SOL_SOCKET, SO_SNDTIMEO, WINNT_TOCHAR(&so_timeo), sizeof(so_timeo)) != 0)
        {
            AF_WARN << "set socket SO_SNDTIMEO option failed (" << strerror(errno) << ") " << i_address;
        }
    }

    if( af::Environment::getSockOpt_Dispatch_TCP_NODELAY() != -1 )
    {
        int nodelay = af::Environment::getSockOpt_Dispatch_TCP_NODELAY();
        if( setsockopt( socketfd, IPPROTO_TCP, TCP_NODELAY, WINNT_TOCHAR(&nodelay), sizeof(nodelay)) != 0)
        {
           AF_WARN << "set socket TCP_NODELAY option failed (" << strerror(errno) << ") " << i_address;
        }
    }

    // connect to address
    if( connect( socketfd, (struct sockaddr*)&addr, i_address.sizeofAddr()) != 0 )
    {
        AF_ERR << "connect: " << strerror(errno);
        ::close(socketfd);
        return false;
    }

    AF_DEBUG << "connected";

    return true;
}

bool SocketPool::checkSocket( const int &socketfd)
{
    int err;
    socklen_t err_len = sizeof(err);
    if (getsockopt(socketfd, SOL_SOCKET, SO_ERROR, &err, &err_len) == -1)
        return false;
    return err == 0;
}
