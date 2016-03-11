#ifndef SOCKETPOOL_H
#define SOCKETPOOL_H

#include "common/dlMutex.h"
#include "address.h"
#include "receivingmsgqueue.h"

#include <cstring>
#include <map>

namespace af {

/**
 * @brief Thread-safe pool of sockets openned toward different targets.
 * It ensures that no other process will use the same socket at the same time,
 * and that the data structure will remain consistent even when multiple
 * process try to connect new sockets.
 * The basic way to use it is:
 *
 *     // `address` is supposed to be defined
 *     SocketPool sp;
 *     int socketfd
 *     sp.get(address, socketfd);
 *     // do something with socketfd
 *     sp.release(address);
 *
 * Getting the socket can eventually go wrong, so it returns a boolean status.
 * If you encounter errors or unexpected behavior with the socket provided by
 * the socket pool, or if you just don't need it any more, you can call close()
 * instead of release().
 *
 *     if (false == sp.get(address, socketfd))
 *         AF_ERR << "unable to connect to " << address;
 *     if (do_something_with(socketfd))
 *         sp.release(address);
 *     else
 *         sp.close(address);
 *
 */
class SocketPool
{
public:
    SocketPool();

    /**
     * @brief get a socket connected to an address.
     * If it does not exist, it is created on the fly.
     * A socket got using this method MUST be released after usage with
     * either release() or error(), otherwise no other process will be enable
     * to use it.
     * @param i_address address to connect to
     * @param socketfd returned socket connected to the given address
     * @param check (depreciated, see checkSocket()) whether to do additional
     * checks on the socket and potentially reconnect it.
     * @return whether everything went well
     */
    bool get(const af::Address & i_address, int &socketfd, bool check=false);
    /**
     * @brief add an already open socket to a given address.
     * @param i_address address to which this socket is (supposed to be)
     * connected
     * @param socketfd socket file descriptor to save in the pool
     * @return status. It especially returns false when there is already a
     * socket registered at the address and the `force` mode is not enabled.
     */
    bool set(const af::Address & i_address, int socketfd, bool force=false);
    /**
     * @brief release a socket previously retrieved using the get() method
     * @param i_address address indexing the socket to release
     */
    void release(const af::Address & i_address);
    /**
     * @brief close the socket and remove it from pool
     * This is usually used to signal an error with the provided socket.
     * It will close the socket and remove the entry in the pool so that a new
     * socket will be openned at the next attempt.
     * @param i_address address indexing the socket to close
     */
    void close( const af::Address & i_address);

    inline void subscribe(ReceivingMsgQueue *queue) { m_subscribers.push_back(queue); }

private:
    /**
     * @brief initialize a socket and connect it
     * @param addr address to bind the new socket to
     * @param socketfd returned socket
     * @return whether everything went well
     */
    static bool initSocket(const af::Address &i_address, int &socketfd);

    /**
     * @brief check that a socket is still 'valid'
     * The exact definition of 'valid' may vary, but it basically perform some
     * checks ensuring that it is possible to read and/or write to the socket.
     * The aproximate meaning of 'valid' is intentional because whatever it is,
     * the socket being valid when this is called does not mean that it will
     * still be when actually using the socket.
     * Hence the result of this method should be used only as a hint.
     *
     * N.-B.: We should not need this. It is implemented as a temporary
     * solution to reuse parts of the code that have not been wrote to use the
     * socket pool, but a good final design would be not to suppose that the
     * socket is valid in e.g. `msgsendtoaddress`.
     * @param socketfd descriptor of the socket to check
     * @return whether it is 'valid' or not
     */
    static bool checkSocket(const int &socketfd);

private:
    /// Used to lock structure when new socket is added
    DlMutex m_global_mutex;

    /// For each address, store a connected socket along with a mutex to
    /// prevent several processes to use the same socket simultaneously
    std::map<af::Address, std::pair<int, DlMutex>, AddressCompare> m_table;

    /// This is terribly hacky but should not last (yeah, it should not)
    std::vector<ReceivingMsgQueue*> m_subscribers;
};

} // namespace af


#endif // SOCKETPOOL_H
