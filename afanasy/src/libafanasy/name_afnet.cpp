#include "name_af.h"

#ifndef WINNT
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#define closesocket close
#endif

#include "../include/afanasy.h"

#include "logger.h"
#include "address.h"
#include "environment.h"
#include "msg.h"
#include "msgstat.h"
#include "socketpool.h"

#define AFOUTPUT
#undef AFOUTPUT
#include "../include/macrooutput.h"

af::MsgStat mgstat;

/**
 * @brief Read data from file descriptor. This call is blocking.
 * @param fd file descriptor to read from
 * @param data buffer to write read data to
 * @param data_len length of data to read
 * @param buffer_maxlen buffer length
 * @return bytes than was written or -1 on any error
 */
int readdata( int fd, char* data, int data_len, int buffer_maxlen)
{
    assert(buffer_maxlen >= data_len); // This call will never end! (or with EINVAL)
    //AF_DEBUG << "trying to recieve " << data_len << " bytes.";
    int read_bytes = 0;
    while( read_bytes < data_len )
	{
        int r = recv( fd, data + read_bytes, buffer_maxlen - read_bytes, MSG_WAITALL);
		if( r < 0)
		{
            if (errno == EWOULDBLOCK)
                continue;
            AF_ERR << "recv: " << strerror(errno);
            return -1;
		}
        //AF_DEBUG << "read " << r << " bytes.";
        if( r == 0) return read_bytes;
        read_bytes += r;
	}

    return read_bytes;
}

/**
 * @brief Write data to file descriptor.
 * @param fd file to write into
 * @param data data buffer to be written
 * @param len length of the data buffer
 * @return status
 */
bool writedata( int fd, const char * data, int len)
{
	int written_bytes = 0;
	while( written_bytes < len)
	{
        int w = send( fd, data+written_bytes, len, MSG_NOSIGNAL);
		if( w < 0)
		{
            AF_ERR << "send: " << strerror(errno);
			return false;
		}
		written_bytes += w;
	}
	return true;
}

/**
 * @brief Parse header and build message depending on this
 * Shouldn't this be a method of Msg?
 * @param io_msg message to build and from which reading the buffer
 * @param i_bytes number of bytes available in buffer
 * @return header offset, or -1 if it was not recognized.
 */
int af::processHeader( af::Msg * io_msg, int i_bytes)
{
	//printf("name_afnet.cpp processHeader: Received %d bytes:\n", i_bytes);
	char * buffer = io_msg->buffer();
	int offset = 0;

	// Process HTTP header:
	if( strncmp( buffer, "POST", 4) == 0 )
	{
        offset += 4;
		int size;
		bool header_processed = false;
        for( offset++ ; offset < i_bytes; ++offset)
		{
            // If we are at the beginning of a line:
            if( buffer[offset - 1] == '\n' )
			{
                // If header found, body can start in the same data packet:
				if( header_processed && ( buffer[offset] == '{' ))
                    break;

				// Look for a special header:
                if( strncmp("AFANASY: ", buffer + offset, 9) == 0)
				{
                    offset += 9;
					if( 1 == sscanf( buffer + offset, "%d", &size))
					{
                        header_processed = true;
					}
					else
					{
                        AF_ERR << "HTTP POST request has a bad AFANASY header.";
						return -1;
					}
				}
			}
		}

		// If header found, construct message:
        if( false == header_processed )
		{
            AF_ERR << "HTTP POST request header was not recongnized";
            return -1;
		}

        io_msg->setHeader( af::Msg::THTTP, size, offset, i_bytes);
        return offset;
	}

	// Simple header for JSON (used for example in python api and afcmd)
	if( strncmp("AFANASY", buffer, 7) == 0 )
	{
        offset += 7;
		int size;
        if( 1 == sscanf( buffer + offset, "%d", &size))
		{
            for( ; offset < i_bytes ; ++offset)
			{
				if( strncmp( buffer+offset, "JSON", 4) == 0)
				{
					offset += 4;
                    while(offset < i_bytes && buffer[offset] != '{')
                        ++offset;
					io_msg->setHeader( af::Msg::TJSON, size, offset, i_bytes);
					return offset;
				}
			}

			// Header not recongnized:
            AF_ERR << "JSON message header was not recongnized.";
			return -1;
		}
	}

	if( strncmp( buffer, "GET", 3) == 0 )
	{
        char * get = new char[i_bytes];
		memcpy( get, buffer, i_bytes);
		io_msg->setData( i_bytes, get, af::Msg::THTTPGET);
		delete []  get;

		return 0; // no offset, reading finished
	}

	io_msg->readHeader( i_bytes);

	return af::Msg::SizeHeader;
}


af::Msg * msgsendtoaddress( const af::Msg * i_msg, const af::Address & i_address,
						    bool & o_ok, af::VerboseMode i_verbose)
{
	o_ok = true;
    af::SocketPool &sp = af::Environment::getSocketPool();

	if( i_address.isEmpty() )
	{
        AF_ERR << "Address is empty";
		o_ok = false;
		return NULL;
	}

    int socketfd;
    if( false == sp.get(i_address, socketfd, true /* check */))
    {
        AF_ERR << "connect failure for msgType '" << af::Msg::TNAMES[i_msg->type()] << "': " << i_address;
        o_ok = false;
        return NULL;
    }

    // send
	if( false == af::msgwrite( socketfd, i_msg))
	{
        AF_ERR << "can't send message to client: " << i_address;
		o_ok = false;
        sp.close( i_address);
		return NULL;
	}

	if( false == i_msg->isReceiving())
	{
        if( i_msg->type() == af::Msg::TRenderDeregister)
            AF_DEBUG << "TRenderDeregister sent, not receiving!";
        sp.release( i_address);
		return NULL;
	}


	// Read JSON answer:
	if( i_msg->type() == af::Msg::TJSON )
	{
		static const int read_buf_len = 4096;
		char read_buf[read_buf_len];
		std::string buffer;
		while( buffer.size() <= af::Msg::SizeDataMax )
		{
			#ifdef WINNT
			int r = recv( socketfd, read_buf, read_buf_len, 0);
			#else
			int r = read( socketfd, read_buf, read_buf_len);
			#endif
			if( r <= 0 )
				break;
			buffer += std::string( read_buf, r);
		}

		af::Msg * o_msg = NULL;
		if( buffer.size())
		{
			o_msg = new af::Msg();
			o_msg->setData( buffer.size(), buffer.c_str(), af::Msg::TJSON);
			o_ok = true;
		}
		else
		{
            AF_ERR << "Reading JSON answer failed.";
			o_ok = false;
		}

        sp.release(i_address);
		return o_msg;
	}

    // Read binary answer:
    af::Msg * o_msg = new af::Msg();
    if( false == af::msgread( socketfd, o_msg))
    {
        AF_ERR << "Reading binary answer failed (in response to " << i_msg << ")";
        sp.close( i_address);
        delete o_msg;
        o_ok = false;
        return NULL;
	}

    sp.release(i_address);
	return o_msg;
}

void af::statwrite( af::Msg * msg)
{
   mgstat.writeStat( msg);
}
void af::statread( af::Msg * msg)
{
   mgstat.readStat( msg);
}

void af::statout( int columns, int sorting)
{
   mgstat.v_stdOut( columns, sorting);
}

const af::Address af::solveNetName( const std::string & i_name, int i_port, int i_type, VerboseMode i_verbose)
{
	if( i_verbose == af::VerboseOn )
	{
        std::stringstream ss;
        ss << "Solving '" << i_name << "' ";
		switch( i_type)
		{
			case AF_UNSPEC: break;
            case AF_INET:  ss << "and IPv4 forced"; break;
            case AF_INET6: ss << "and IPv6 forced"; break;
            default: ss << "(unknown protocol forced)";
		}
        AF_LOG << ss.str() << "...";
	}

	struct addrinfo *res;
	struct addrinfo hints;
	memset( &hints, 0, sizeof(hints));
	hints.ai_flags = AI_ADDRCONFIG;
    hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	char service_port[16];
	sprintf( service_port, "%u", i_port);
	int err = getaddrinfo( i_name.c_str(), service_port, &hints, &res);
	if( err != 0 )
	{
        AF_ERR << "getaddrinfo: " << gai_strerror(err);
		return af::Address();
	}

	for( struct addrinfo *r = res; r != NULL; r = r->ai_next)
	{
		// Skip address if type is forced
		if(( i_type != AF_UNSPEC ) && ( i_type != r->ai_family)) continue;

		if( i_verbose == af::VerboseOn )
		{
			switch( r->ai_family)
			{
				case AF_INET:
				{
					struct sockaddr_in * sa = (struct sockaddr_in*)(r->ai_addr);
                    AF_LOG << "IP = " << inet_ntoa( sa->sin_addr);
					break;
				}
				case AF_INET6:
				{
					static const int buffer_len = 256;
					char buffer[buffer_len];
					struct sockaddr_in6 * sa = (struct sockaddr_in6*)(r->ai_addr);
					const char * addr_str = inet_ntop( AF_INET6, &(sa->sin6_addr), buffer, buffer_len);
                    AF_LOG << "IPv6 = " << addr_str;
					break;
				}
				default:
                    AF_LOG << "Unknown address family type = " << r->ai_family;
					continue;
			}
		}

		af::Address addr((struct sockaddr_storage*)(r->ai_addr));

		if( i_verbose == af::VerboseOn )
		{
            AF_LOG << "Address = " << addr;
		}

		// Free memory allocated for addresses:
		freeaddrinfo( res);

		return addr;

	}

	// Free memory allocated for addresses:
	freeaddrinfo( res);

	return af::Address();
}

/**
 * @brief Read message from file descriptor. This call is blocking.
 * @param desc socket to read from
 * @param msg message to build with a correctly allocated buffer
 * @return status
 */
bool af::msgread( int desc, af::Msg* msg)
{
	char * buffer = msg->buffer();

    // Read message header data
	int bytes = ::readdata( desc, buffer, af::Msg::SizeHeader, af::Msg::SizeBuffer );

	if( bytes < af::Msg::SizeHeader)
	{
        AF_ERR << "can't read message header, bytes = " << bytes << " (< Msg::SizeHeader = " << Msg::SizeHeader << ")";
		msg->setInvalid();
		return false;
	}

	// Header offset is variable on not binary header (for example HTTP)
	int header_offset = af::processHeader( msg, bytes);
	if( header_offset < 0)
		return false;

	// Read message data if any
	if( msg->type() >= af::Msg::TDATA)
	{
        buffer = msg->buffer(); // buffer size may have been changed when processing header
		bytes -= header_offset;
		int readlen = msg->dataLen() - bytes;
		if( readlen > 0)
		{
            bytes = ::readdata( desc, buffer + af::Msg::SizeHeader + bytes, readlen, readlen);
			if( bytes < readlen)
			{
                AF_ERR << "read message data: ( bytes < readlen : " << bytes << " < " << readlen << ")";
				msg->setInvalid();
				return false;
			}
		}
	}

	mgstat.put( msg->type(), msg->writeSize());

	return true;
}

/**
 * @brief Write a message to a file descriptor. This call is blocking.
 * Prepend special header when sending as HTTP of afanasy's JSON format
 * @param i_desc file to write into
 * @param i_msg message to write
 * @return status
 */
bool af::msgwrite( int i_desc, const af::Msg * i_msg)
{
    size_t content_length = i_msg->writeSize() - i_msg->getHeaderOffset();

	if( i_msg->type() == af::Msg::THTTP )
	{
        std::stringstream header;
        header << "HTTP/1.1 200 OK\r\n"
               << "Content-Type: application/json\r\n"
               << "Connection: close\r\n"
               << "Content-Length: " << content_length << "\r\n"
               << "\r\n";
        if (false == ::writedata( i_desc, header.str().data(), header.str().length()))
            return false;
	}
	else if( i_msg->type() == af::Msg::TJSON )
	{
        std::stringstream header;
        header << "AFANASY " << content_length << " JSON";
        if (false == ::writedata( i_desc, header.str().data(), header.str().length()))
            return false;
	}

    if( false == ::writedata( i_desc, i_msg->buffer() + i_msg->getHeaderOffset(), content_length ))
        return false;

	mgstat.put( i_msg->type(), i_msg->writeSize());

	return true;
}


/**
 * @brief Send a message. This call is blocking.
 * The message contains information about the addresses to send it to.
 * If there is only one address, it will potentially wait for an answer and
 * return it.
 * If there are several addresses to send the message to, it is broadcasted and
 * no answer is returned.
 * @param i_msg message to send
 * @param o_ok returned status (whether everything went good)
 * @param i_verbose verbose mode
 * @return response from destination
 */
af::Msg * af::msgsend( Msg * i_msg, bool & o_ok, VerboseMode i_verbose )
{
	if( i_msg->isReceiving() && ( i_msg->addressesCount() > 0 ))
	{
        AF_WARN << "Receiving message has several addresses: " << i_msg;
	}

	if( i_msg->addressIsEmpty() && ( i_msg->addressesCount() == 0 ))
	{
        AF_ERR << "Message has no addresses to send to: " << i_msg;
		o_ok = false;
		return NULL;
	}

	if( false == i_msg->addressIsEmpty())
	{
		af::Msg * o_msg = ::msgsendtoaddress( i_msg, i_msg->getAddress(), o_ok, i_verbose);
		if( o_msg != NULL )
        {
            // Force rid, in case remote host did not set it correctly
            // This is mostly a backward compatibility feature
            o_msg->setRid( i_msg->getId());
			return o_msg;
        }
	}

	if( i_msg->addressesCount() < 1)
		return NULL;

    AF_DEBUG << "not returned!";

	bool ok;
	const std::list<af::Address> * addresses = i_msg->getAddresses();
    std::list<af::Address>::const_iterator it;
    for(it = addresses->begin() ; it != addresses->end() ; it++)
	{
		::msgsendtoaddress( i_msg, *it, ok, i_verbose);
		if( false == ok )
		{
			o_ok = false;
			// Store an address that message was failed to send to
			i_msg->setAddress( *it);
		}
	}

	return NULL;
}

/**
 * @brief close socket
 * If protocol was HTTP, flush remaining data and wait for the client to close
 * the connection
 * @param i_sd socket to close
 * @param i_response_type type of message sent through it
 */
void af::socketDisconnect( int i_sd, uint32_t i_response_type)
{
	if( af::Environment::isServer() && 
		( i_response_type != af::Msg::THTTP ) &&
		( i_response_type != af::Msg::THTTPGET ))
	{
		// Server waits client have closed socket first:
		char buf[256];
		int r = 1;
		while( r > 0 )
		{
			#ifdef WINNT
			r = recv( i_sd, buf, af::Msg::SizeHeader, 0);
			#else
			r = read( i_sd, buf, af::Msg::SizeHeader);
			#endif
		}
	}

	closesocket( i_sd);
}

