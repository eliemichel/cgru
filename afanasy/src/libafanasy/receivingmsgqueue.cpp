#include "receivingmsgqueue.h"
#include "logger.h"
#include "msg.h"
#include "msgstat.h"
#include "environment.h"

#include "../include/afanasy.h"

#include <sys/epoll.h>
#include <fcntl.h>

extern bool AFRunning;

using namespace af;
af::MsgStat mgstat2;

ReceivingMsgQueue::ReceivingMsgQueue(const std::string &QueueName, AfQueue::StartTread i_start_thread)
    : AfQueue::AfQueue( QueueName, i_start_thread)
{
    // Initialize epoll
    // This is the core of the running loop, waiting until any socket is ready
    m_epfd = epoll_create(0xCAFE);
    if (m_epfd == -1) {
        AF_ERR << "epoll_create: " << strerror(errno);
        return;
    }
}

bool ReceivingMsgQueue::addSocket(int socketfd, SocketType type)
{
    SocketInfo *si;
    int s;
    struct epoll_event ev;

    ReceivingMsgQueue::setnonblocking(socketfd);

    if (type == STListening) {
        AF_DEBUG << "Adding listening socket to receiving queue";
    } else {
        AF_DEBUG << "Adding socket to receiving queue";
    }

    si = new SocketInfo(socketfd);
    si->type = type;

    // If this is receiving, we start reading it because connections are edge
    // triggered so nothing would trigger for the already queued bytes.
    if (type == STReceiving) {
        s = read_from_socket(si);
        if (s == -1) {
            AF_ERR << "remote host I/O error, closing connection";
            close(si->sfd);
            return false;
        }
    }

    lock();
    m_sockets_info.insert(si);
    unlock();

    ev.events = EPOLLIN;
    if (type == STReceiving)
        ev.events |= EPOLLET; // edge triggering
    ev.data.ptr = static_cast<void*>(si);
    if(epoll_ctl(m_epfd, EPOLL_CTL_ADD, socketfd, &ev) == -1) {
        AF_ERR << "epoll_ctl: " << strerror(errno);
        return false;
    }
    return true;
}

bool ReceivingMsgQueue::listenTo(int port)
{
    int sfd;

    if (false == ReceivingMsgQueue::open_listening_socket(sfd, port)) {
        AF_ERR << "open_listening_socket failed";
        return false;
    }

    if (false == this->addSocket(sfd, STListening)) {
        AF_ERR << "addSocket failed";
        close(sfd);
        return false;
    }

    return true;
}

void ReceivingMsgQueue::processItem(AfQueueItem *item)
{
    Msg *msg = static_cast<Msg*>(item);
    mgstat2.put( msg->type(), msg->writeSize());
    AF_DEBUG << "---> " << msg;
}

void ReceivingMsgQueue::run()
{
    // All var declarations are here because this comes from a C89 draft
    static const size_t MAX_EVENTS = 10;
    struct epoll_event events[MAX_EVENTS];
    SocketInfo *si;
    int nfds, s, n;

    while( AFRunning )
    {
        nfds = epoll_wait(m_epfd, events, MAX_EVENTS, -1);
        //AF_DEBUG << "epoll ready: nfds=" << nfds;
        if (nfds == -1) {
            switch (errno) {
            case EINTR:
                break;
            default:
                AF_ERR << "epoll_wait: " << strerror(errno);
                AFRunning = false;
                continue;
            }
        }

        for (n = 0 ; n < nfds ; ++n) {
            si = static_cast<SocketInfo*>(events[n].data.ptr);
            /* happens when several events are amitted for a given fd
               and that a previous one made the fd close */
            if (si->closed)
                continue;

            switch (si->type) {
            case STListening:
                AF_DEBUG << "new connection";
                accept_new_client(si->sfd);
                break;
            case STReceiving:
                switch(read_from_socket(si)) {
                case SIR_ERR:
                    AF_ERR << "remote host I/O error, closing connection";
                    si->close();
                    break;
                case SIR_OK:
                    break;
                case SIR_DIRTY_QUIT:
                    AF_WARN << "remote host interrupted the connection in the middle of a message";
                    si->close();
                    break;
                case SIR_QUIT:
                    si->close();
                    break;
                default:
                    assert(0); // Every case should be handled
                }
                break;
            }
        }

        /* destroy clientinfo associated with closed clients */
        for (n = 0 ; n < nfds ; ++n) {
            si = static_cast<SocketInfo*>(events[n].data.ptr);
            if (si->closed) {
                m_sockets_info.erase(si);
                delete si;
            }
        }
    }

    // Free remaining sockets
    for (std::set<SocketInfo*>::iterator it = m_sockets_info.begin() ; it != m_sockets_info.end() ; it++)
    {
        si = *it;
        close(si->sfd);
        delete si;
    }
    m_sockets_info.clear();
}


// This is a stateful version of af::msgread, alowing one to manage several
// partialy received messages at the same time
int ReceivingMsgQueue::read_from_socket(SocketInfo *si) {
    int remaining, header_end;
    ssize_t read;
    AfQueueItem *item;

    remaining = si->to_read - si->read_pos;
    //AF_DEBUG << "Receiving message on #" << si->sfd << ": step " << si->reading_state << ", remaining: " << remaining;

    // When the current step is finished (or if it's the initial step)
    // We loop because a step can be finished instantaneously (like reading the
    // payload of a Msg < Msg::TDATA, i.e. w/o payload)
    while( remaining <= 0 || si->reading_state == 0)
    {
        switch( si->reading_state)
        {
        case 0: // Init step
            si->msg = new Msg();
            si->msg->setSocket(si->sfd);
            si->buffer = si->msg->buffer();
            si->to_read = af::Msg::SizeHeader;
            si->read_pos = 0;
            break;
        case 1: // Header buffer read
            header_end = af::processHeader( si->msg, si->read_pos);
            if( header_end < 0)
                return -1;
            if( si->msg->type() >= af::Msg::TDATA)
            {
                si->buffer = si->msg->buffer() + Msg::SizeHeader;
                si->to_read = si->msg->dataLen();
                si->read_pos = si->read_pos - header_end; // part of what have been read as buffer which is actually content
            }
            break;
        case 2: // Payload read
            af::Environment::getSocketPool().set(si->msg->getAddress(), si->sfd);
            item  = (AfQueueItem *)si->msg;
            this->processItem(item);
            this->push(item);
            break;
        default:
            assert(0);  // reading state should loop
        }

        // Next step
        si->reading_state++;
        si->reading_state %= 3;

        remaining = si->to_read - si->read_pos;
    }

    read = recv(si->sfd, si->buffer + si->read_pos, (size_t)remaining, 0);

    if (read == -1) {
        switch (errno) {
        case EWOULDBLOCK:
            return SIR_OK;
        case EINTR:
            read = 0;
            break;
        default:
            AF_ERR << "recv: " << strerror(errno);
            return SIR_ERR;
        }
    } else if (read == 0) {
        if (si->reading_state == 0 || si->reading_state == 1 && si->read_pos == 0) {
            return SIR_QUIT;
        } else {
            AF_WARN << "Remaining: " << remaining << " bytes";
            return SIR_DIRTY_QUIT;
        }
    }

    si->read_pos += read;

    // Loop until EWOULDBLOCK or an error is raised.
    // This is important because we use edge triggered sockets so they must be
    // completely flushed.
    return read_from_socket(si);
}

bool ReceivingMsgQueue::accept_new_client(int listen_sock)
{
    int sfd;
    struct sockaddr_storage addr;
    socklen_t addr_len = sizeof(addr);

    sfd = accept(listen_sock, (struct sockaddr*)&addr, &addr_len);
    if( sfd == -1) {
        switch( errno) {
        case EMFILE: // Very bad, probably main reading thread is locked, most likely server mutexes bug
            AF_ERR << "The per-process limit of open file descriptors has been reached.";
            return false;
        case ENFILE: // Very bad, probably main reading thread is locked, most likely server mutexes bug
            AF_ERR << "The system limit on the total number of open files has been reached.";
            return false;
        case EINTR:
            AF_LOG << "Server was interrupted.";
            AFRunning = false;  // elie: doesn't this depend on the type of interruption?
            return false;
        default:
            AF_WARN << "accept: " << strerror(errno);
            break;
        }
    }

    return this->addSocket(sfd);
}


bool ReceivingMsgQueue::setnonblocking(int fd) {
    int flags;
    flags = fcntl(fd, F_GETFL);
    if (flags == -1) {
        AF_ERR << "fcntl: F_GETFL: " << strerror(errno);
        return false;
    }
    if (fcntl(fd, F_SETFL, flags|O_NONBLOCK) == -1) {
        AF_ERR << "fcntl: F_SETFL: " << strerror(errno);
        return false;
    }
    return true;
}

static std::string addrinfo_to_str(struct addrinfo *ai)
{
    char host[256];
    if (getnameinfo(ai->ai_addr, ai->ai_addrlen, host, 256, NULL, 0, 0) == -1)
    {
        AF_WARN << "getnameinfo: " << strerror(errno);
        return "<unknown>";
    }
    return std::string(host);
}

bool ReceivingMsgQueue::open_listening_socket(int &sfd, int port)
{
    int family, s;
    struct addrinfo hints, *res, *ai;

    family = AF_UNSPEC;
    if( af::Environment::isIPv6Disabled())
        family = AF_INET;

    // Check for available local network addresses
    memset( &hints, 0, sizeof(hints));
    hints.ai_family = family;                     /* Allow IPv4 or IPv6 */
    hints.ai_socktype = SOCK_STREAM;              /* TCP socket */
    hints.ai_flags = AI_PASSIVE | AI_ADDRCONFIG;
    hints.ai_protocol = 0;                        /* Any protocol */
    hints.ai_canonname = NULL;
    hints.ai_addr = NULL;
    hints.ai_next = NULL;
    std::stringstream port_ss;
    port_ss << port;
    s = getaddrinfo( NULL, port_ss.str().c_str(), &hints, &res);
    if (s != 0) {
        AF_ERR << "getaddrinfo: " << gai_strerror(s);
        return false;
    }

    AF_LOG << "Available addresses:";

    // Scan all returned addresses and fill address pools to sort them by
    // custom priority (namely IPv6 first)
    // This is why we need to loop twice over the addresses
    typedef std::set<struct addrinfo*> AddrSet;
    AddrSet ipv4;
    AddrSet ipv6;
    for(ai = res; ai != NULL; ai = ai->ai_next) {
        AF_LOG << addrinfo_to_str(ai);
        switch (ai->ai_family) {
        case AF_INET:
            ipv4.insert(ai);
            break;
        case AF_INET6:
            ipv6.insert(ai);
            break;
        default:
            AF_WARN << "Unsupported address family (" << ai->ai_family << "), skipping.";
            continue;
        }
    }

    family = AF_INET6;
    AddrSet::iterator it;
    // Loop over ipv6 and then ipv4 (two loops in one)
    for (it = ipv6.begin() ; it != ipv4.end() ; it++) {
        // If all IPv6 have been tested, go to IPv4
        if (it == ipv6.end()) {
            family = AF_INET;
            it = ipv4.begin();
        }

        ai = *it;

        sfd = socket(ai->ai_family, ai->ai_socktype | SOCK_NONBLOCK, ai->ai_protocol);
        if (sfd == -1) {
            AF_WARN << "socket: " << strerror(errno);
            continue;
        }

        if (bind(sfd, ai->ai_addr, ai->ai_addrlen) == -1) {
            AF_WARN << "bind: " << strerror(errno);
        } else {
            break; /* Success */
        }

        close(sfd);
    }

    freeaddrinfo(res);

    // No IPv4 available
    if (it == ipv4.end())
    {
        AF_ERR << "No addresses found";
        return false;
    }

    // Display IP family used
    if (family == AF_INET) {
        AF_LOG << "Using IPv4 addresses family.";
    } else {
        AF_LOG << "Using IPv6 addresses family.";
        AF_LOG << "IPv4 connections addresses will be mapped to IPv6.";
    }

    // set socket options for reuseing address immediatly after bind
    int value = 1;
    if( setsockopt( sfd, SOL_SOCKET, SO_REUSEADDR, WINNT_TOCHAR(&value), sizeof(value)) != 0)
        AF_WARN << "set socket SO_REUSEADDR option failed";

    if (listen(sfd, SOMAXCONN) == -1) {
        AF_ERR << "listen: " << strerror(errno);
        return false;
    }

    return true;
}

