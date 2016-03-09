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

bool SocketPool::get(const af::Address & i_address, int &socketfd)
{
    struct sockaddr_storage addr;
    if( false == i_address.setSocketAddress( &addr))
        return false;

    if( m_table.count(addr) == 0)
    {
        if ( false == SocketPool::initSocket( i_address, socketfd))
            return false;
        m_global_mutex.Acquire();
        // We check again because another process could have populated the same address
        // But we did  not lock before because we assume initSocket to take some time
        // and try to reduce as much as possible the locking time.
        if( m_table.count(addr) == 0)
        {
            m_table[addr] = std::make_pair<int, DlMutex>(socketfd, DlMutex());
        }
        m_global_mutex.Release();
    }

    std::pair<int, DlMutex> p = m_table[addr];
    DlMutex mutex = p.second;
    mutex.Acquire();
    socketfd = p.first;

    return true;
}

void SocketPool::release( const af::Address & i_address)
{
    struct sockaddr_storage addr;
    assert(i_address.setSocketAddress( &addr));
    assert(m_table.count(addr) != 0);  // do not release a socket that you did not get!

    DlMutex mutex = m_table[addr].second;
    mutex.Release();
}

void SocketPool::error( const af::Address & i_address)
{
    this->release( i_address);
    struct sockaddr_storage addr;
    assert(i_address.setSocketAddress( &addr));
    // Close erroneous socket
    closesocket(m_table[addr].first);
    // Remove socket from list. Next time we try to use it, it will be reopenned
    m_table.erase(addr);
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
        closesocket(socketfd);
        return false;
    }

    AF_DEBUG << "connected";
    return true;
}
