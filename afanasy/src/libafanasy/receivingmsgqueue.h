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
public:
    /**
     * @brief Type of socket waited by the epoll object
     * This can be either a socket to read or a socket to accept from.
     */
    enum SocketType {
        STListening,
        STReceiving
    };

private:
    /**
     * @brief Information stored internally about a socket
     * It what the epoll event data pointer points at.
     */
    struct SocketInfo
    {
        SocketInfo(int socketfd)
            : sfd(socketfd)
            , type(STReceiving)
            , closed(false)
            , msg(NULL)
            , reading_state(0)
        {}
        ~SocketInfo()
        {
            if ( NULL != msg)
                delete msg;
        }

        void close()
        {
            ::close(sfd);
            closed = 1;
        }

        enum SocketType type; ///< Socket type (receiving or listening)
        int sfd;              ///< Socket File Descriptor
        bool closed;          ///< Whether this socket has been closed
        af::Msg *msg;         ///< Msg being received
        int to_read;          ///< How many bytes we have to read
        int read_pos;         ///< How many bytes we already read
        char *buffer;         ///< Reading buffer
        int reading_state;    ///< logical state of the message reading automaton (@see read_from_socket)
    };

    /**
     * @brief Status returned by read_from_socket to the epoll waiting loop
     */
    enum SocketInfoRead
    {
        SIR_ERR = -1,   ///< An error occurred
        SIR_OK = 0,     ///< Socket has been correctly processed, nothing to say
        SIR_QUIT,       ///< Client closed the connection cleanly
        SIR_DIRTY_QUIT, ///< Client closed the connection in the middle of a message
    };

public:
    ReceivingMsgQueue(const std::string & QueueName, StartTread i_start_thread);

    /**
     * @brief Return the first message from queue and remove it.
     * This call is blocking if `i_block` is `AfQueue::e_wait`.
     * @param i_block Whether the call should block or return NULL when the
     * queue is empty.
     * @return Popped message or NULL
     */
    inline Msg* popMsg( WaitMode i_block ) { return (Msg*)pop(i_block); }

    /**
     * @brief Add an already openned and connected socket to the pool of
     * sockets to read from.
     * If type is STListening, every time a new connection incomes, its client
     * socket is added using addSocket().
     * @param socketfd descriptor of the socket to add
     * @param type STReceiving or STListening
     * @return status
     */
    bool addSocket(int socketfd, SocketType type=STReceiving);

    /**
     * @brief Listen to a given port
     * This is a short for `open_listening_socket()` followed by
     * `addListeningSocket()`.
     * @param port port to listen to
     * @return status
     */
    bool listenTo(int port);
protected:
    /**
     * @brief Process new item.
     * This function is called whenever a new message is ready
     * @param item new message, to be casted into Msg*
     */
    void processItem( AfQueueItem* item);

    /**
     * @brief Main loop of the independent receiving thread
     */
    void run();

private:
    int m_epfd; ///< epoll file descriptor
    std::set<SocketInfo*> m_sockets_info;

private:
    /**
     * @brief Receive data from client and consolidate it into header buffer or
     * forward it to the client's incoming message struct.
     * @param ci client info corresponding to this socket
     * @return -1 in case of error,
     *          1 if the message if for the low level server (e.g. client quit),
     *          0 otherwise
     */
    int read_from_socket(SocketInfo *si);

    /**
     * @brief Accept new client and add it to the socket pool
     * Note: This function has been partly imported from the main loop of
     * server's `threadAcceptPort` function.
     * @param listen_socket Listening socket bound to the incoming connection
     * @return status
     */
    bool accept_new_client(int listen_sock);

private:
    /**
     * @brief Set non-blocking mode to file descriptor
     * @param fd File descriptor
     * @return status
     */
    static bool setnonblocking(int fd);

    /**
     * @brief Open a new socket listening on a given port
     * @param sfd reference to return the new socket
     * @param port port to listen to
     * @return status
     */
    static bool open_listening_socket(int &sfd, int port);

};

} // namespace af

#endif // RECEIVINGMSGQUEUE_H
