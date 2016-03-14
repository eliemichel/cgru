#include "receivingmsgqueue.h"
#include "logger.h"
#include "msg.h"
#include "msgstat.h"

#include <sys/epoll.h>
#include <fcntl.h>

extern bool AFRunning;

using namespace af;
af::MsgStat mgstat2;

ReceivingMsgQueue::ReceivingMsgQueue(const std::string &QueueName, AfQueue::StartTread i_start_thread)
    : AfQueue::AfQueue( QueueName, i_start_thread)
{
    m_epfd = epoll_create(0xCAFE);
    if (m_epfd == -1) {
        AF_ERR << "epoll_create: " << strerror(errno);
        return;
    }
}

bool ReceivingMsgQueue::addSocket(int socketfd)
{
    AF_DEBUG << "Adding socket to receiving queue";
    SocketInfo *si = new SocketInfo(socketfd);
    lock();
    m_sockets_info.insert(si);
    unlock();

    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.ptr = static_cast<void*>(si);
    if(epoll_ctl(m_epfd, EPOLL_CTL_ADD, socketfd, &ev) == -1) {
        AF_ERR << "epoll_ctl: " << strerror(errno);
        return false;
    }
    return true;
}

void ReceivingMsgQueue::processItem(AfQueueItem *item)
{
    Msg *msg = static_cast<Msg*>(item);
    mgstat2.put( msg->type(), msg->writeSize());
    AF_DEBUG << "Received message: " << msg;
}

void ReceivingMsgQueue::run()
{
    // All var declarations are here because this comes from a C89 draft
    static const size_t MAX_EVENTS = 10;
    struct epoll_event events[MAX_EVENTS];
    SocketInfo *si;
    int nfds, s, n;

    // For future use, when the server's listenning socket will be managed
    // by ReceivingMsgQueue as well.
    int listen_sock = -2;

    while( AFRunning )
    {
        nfds = epoll_wait(m_epfd, events, MAX_EVENTS, -1);
        AF_DEBUG << "epoll ready";
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
            if (events[n].data.fd == listen_sock) {
                AF_DEBUG << "new connection";
                //accept_new_client(listen_sock, epfd);
            } else {
                si = static_cast<SocketInfo*>(events[n].data.ptr);
                if (si->closed) {
                    /* happens when several events are amitted for a given fd
                       and that a previous one made the fd close */
                    continue;
                }
                s = read_from_socket(si);
                if (s == -1) {
                    AF_ERR << "client I/O error, closing connection";
                    si->closed = 1;
                    close(si->sfd);
                } else if (s == 1) { // TODO: 1 means that the message has to be handled by this loop
                    /*switch (si->msg->type) {
                    case Msg::TRenderDeregister:
                        AF_DEBUG << "client closed connection or quit";
                        si->closed = 1;
                        close(si->sock);
                        break;
                    case MT_KILL_SERVER:
                        AF_DEBUG << "client requested server to terminate";
                        terminate();
                        break;
                    default:
                        assert(0); /* read_from_socket upwarded a message that epoll_wait loop does not understand *
                    }*/
                }
            }

            /* destroy clientinfo associated with closed clients */
            for (n = 0 ; n < nfds ; ++n) {
                if (events[n].data.fd != listen_sock) {
                    si = static_cast<SocketInfo*>(events[n].data.ptr);
                    if (si->closed) {
                        m_sockets_info.erase(si);
                        delete si;
                    }
                }
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

// This is a stateful version of af::msgread, alowing one to manage several
// partialy received messages at the same time
int ReceivingMsgQueue::read_from_socket(SocketInfo *si) {
    int remaining, header_end;
    ssize_t read;
    AfQueueItem *item;

    remaining = si->to_read - si->read_pos;
    AF_DEBUG << "Receiving message on #" << si->sfd << ": step " << si->reading_state << ", remaining: " << remaining;

    // When the current step is finished (or if it's the initial step)
    // We loop because a step can be finished instantaneously (like reading the
    // payload of a Msg < Msg::TDATA, i.e. w/o payload)
    while( remaining <= 0 || si->reading_state == 0)
    {
        switch( si->reading_state)
        {
        case 0: // Init step
            si->msg = new Msg();
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
            break;
        default:
            AF_ERR << "recv: " << strerror(errno);
            return -1;
        }
    } else if (read == 0) {
        //si->msg->type = MT_QUIT;
        return 1;
    }

    si->read_pos += read;

    return 0;
}
