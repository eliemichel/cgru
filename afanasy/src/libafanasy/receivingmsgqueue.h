#ifndef RECEIVINGMSGQUEUE_H
#define RECEIVINGMSGQUEUE_H

#include <set>

#include "afqueue.h"
#include "msg.h"

namespace af
{

/**
 * @brief ReceivingMsgQueue is a queue from which you can only pop messages.
 * The queue is populated by messages incoming from the socket pool.
 */
class ReceivingMsgQueue : public AfQueue
{
private:
    /**
     * @brief Information stored internally about a socket
     * It what the epoll event data pointer points at.
     */
    struct SocketInfo
    {
        SocketInfo(int socketfd)
            : sfd(socketfd)
            , closed(false)
            , reading_state(0)
        {}
        int sfd;           ///< Socket File Descriptor
        bool closed;       ///< Whether this socket has been closed
        af::Msg *msg;      ///< Msg being received
        int to_read;       ///< How many bytes we have to read
        int read_pos;      ///< How many bytes we already read
        char *buffer;      ///< Reading buffer
        int reading_state; ///< logical state of the message reading automaton (@see read_from_socket)
    };

public:
    ReceivingMsgQueue( const std::string & QueueName, StartTread i_start_thread);

    /// Return first message from queue. BLOCKING FUNCTION if \c block==AfQueue::e_wait.
    inline Msg* popMsg( WaitMode i_block ) { return (Msg*)pop(i_block); }

    bool addSocket(int socketfd);

protected:
    void processItem( AfQueueItem* item);

    void run();

private:
    int m_epfd; ///< epoll file descriptor
    std::set<SocketInfo*> m_sockets_info;

private:
    /**
     * @brief Set non-blocking mode to file descriptor
     * @param fd File descriptor
     * @return status
     */
    static bool setnonblocking(int fd);
    /**
     * @brief Receive data from client and consolidate it into header buffer or
     * forward it to the client's incoming message struct.
     * @param ci client info corresponding to this socket
     * @return -1 in case of error,
     *          1 if the message if for the low level server (e.g. client quit),
     *          0 otherwise
     */
    int read_from_socket(SocketInfo *si);

};

} // namespace af

#endif // RECEIVINGMSGQUEUE_H
