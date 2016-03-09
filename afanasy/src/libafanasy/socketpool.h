#ifndef SOCKETPOOL_H
#define SOCKETPOOL_H

#include "common/dlMutex.h"

#include <cstring>
#include <map>

namespace af {

class Address;

/**
 * StructCompare is used as a comparator when using structs as std::map key.
 * It performs a raw data comparison as if it was a string, so in lexicographic order.
 *
 * ex: if when using `std::map<A, B>`, the compiler complains because type `A`
 * does not implement comparison operator, you can use
 * `std::map<A, B, StructCompare<A> >` instead.
 * Don't do this with big structs, and make sure that the whole struct data
 * will remain a good identifier.
 */
template<typename T>
struct StructCompare
{
    bool operator() (const T &a, const T &b) const
    {
        return std::strncmp((char*)(&a), (char*)(&b), sizeof(T)) < 0;
    }
};

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
     * @param addr address to connect to
     * @param socketfd returned socket connected to the given address
     * @return whether everything went well
     */
    bool get(const af::Address & i_address, int &socketfd);
    /**
     * @brief release a socket previously retrieved using the get() method
     * @param addr address indexing the socket to release
     */
    void release(const af::Address & i_address);
    /**
     * @brief signal an error with the provided socket.
     * This will release it and handle the issue. Basically, it will close the
     * socket and remove the entry in the pool so that a new socket will be
     * openned at the next attempt
     * @param i_address
     */
    void error( const af::Address & i_address);

private:
    /**
     * @brief initialize a socket and connect it
     * @param addr address to bind the new socket to
     * @param socketfd returned socket
     * @return whether everything went well
     */
    static bool initSocket(const af::Address & i_address, int &socketfd);

private:
    /// Used to lock structure when new socket is added
    DlMutex m_global_mutex;

    /// For each address, store a connected socket along with a mutex to
    /// prevent several processes to use the same socket simultaneously
    std::map<struct sockaddr_storage, std::pair<int, DlMutex>, StructCompare<struct sockaddr_storage> > m_table;
};

} // namespace af


#endif // SOCKETPOOL_H
