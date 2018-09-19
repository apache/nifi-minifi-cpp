/**
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef LIBMINIFI_INCLUDE_IO_WIN__CLIENTSOCKET_H_
#define LIBMINIFI_INCLUDE_IO_WIN__CLIENTSOCKET_H_

#include <cstdint>
#include <sys/types.h>
#ifdef WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
//#include <WinSock.h>
//#include <WS2tcpip.h>
#include <Windows.h>
#include <WS2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#endif
#include <mutex>
#include <atomic>
#include "io/BaseStream.h"
#include "core/Core.h"
#include "core/logging/Logger.h"
#include "io/validation.h"
#include "properties/Configure.h"
#include "io/NetworkPrioritizer.h"


namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace io {

#ifdef WIN32
	typedef SOCKET SocketDescriptor;
#else
	typedef int SocketDescriptor;
	#define INVALID_SOCKET -1
#endif


/**
 * Context class for socket. This is currently only used as a parent class for TLSContext.  It is necessary so the Socket and TLSSocket constructors
 * can be the same.  It also gives us a common place to set timeouts, etc from the Configure object in the future.
 */
class SocketContext {
 public:
  SocketContext(const std::shared_ptr<Configure> &configure) {
  }
};
/**
 * Socket class.
 * Purpose: Provides a general purpose socket interface that abstracts
 * connecting information from users
 * Design: Extends DataStream and allows us to perform most streaming
 * operations against a BSD socket
 *
 *
 */
class Socket : public BaseStream {
public:
	/**
	 * Constructor that creates a client socket.
	 * @param context the SocketContext
	 * @param hostname hostname we are connecting to.
	 * @param port port we are connecting to.
	 */
	explicit Socket(const std::shared_ptr<SocketContext> &context, const std::string &hostname, const uint16_t port);

	/**
	 * Move constructor.
	 */
	explicit Socket(const Socket &&);

	/**
	 * Static function to return the current machine's host name
	 */
	static std::string getMyHostName() {
		static std::string HOSTNAME = init_hostname();
		return HOSTNAME;
	}

	/**
	 * Destructor
	 */

	virtual ~Socket();

	virtual void closeStream();
	/**
	 * Initializes the socket
	 * @return result of the creation operation.
	 */
	virtual int16_t initialize();

	virtual void setInterface(io::NetworkInterface &&ifc) {
		local_network_interface_ = std::move(ifc);
	}

	/**
	 * Sets the non blocking flag on the file descriptor.
	 */
	void setNonBlocking();

	std::string getHostname() const;

	/**
	 * Return the port for this socket
	 * @returns port
	 */
	uint16_t getPort();

	// data stream extensions
	/**
	 * Reads data and places it into buf
	 * @param buf buffer in which we extract data
	 * @param buflen
	 * @param retrieve_all_bytes determines if we should read all bytes before returning
	 */
	virtual int readData(std::vector<uint8_t> &buf, int buflen) {
		return readData(buf, buflen, true);
	}
	/**
	 * Reads data and places it into buf
	 * @param buf buffer in which we extract data
	 * @param buflen
	 * @param retrieve_all_bytes determines if we should read all bytes before returning
	 */
	virtual int readData(uint8_t *buf, int buflen) {
		return readData(buf, buflen, true);
	}

	/**
	 * Reads data and places it into buf
	 * @param buf buffer in which we extract data
	 * @param buflen
	 * @param retrieve_all_bytes determines if we should read all bytes before returning
	 */
	virtual int readData(std::vector<uint8_t> &buf, int buflen, bool retrieve_all_bytes);
	/**
	 * Reads data and places it into buf
	 * @param buf buffer in which we extract data
	 * @param buflen
	 * @param retrieve_all_bytes determines if we should read all bytes before returning
	 */
	virtual int readData(uint8_t *buf, int buflen, bool retrieve_all_bytes);

	/**
	 * Write value to the stream using std::vector
	 * @param buf incoming buffer
	 * @param buflen buffer to write
	 *
	 */
	virtual int writeData(std::vector<uint8_t> &buf, int buflen);

	/**
	 * writes value to stream
	 * @param value value to write
	 * @param size size of value
	 */
	virtual int writeData(uint8_t *value, int size);

	/**
	 * Writes a system word
	 * @param value value to write
	 */
	virtual int write(uint64_t value, bool is_little_endian = EndiannessCheck::IS_LITTLE);

	/**
	 * Writes a uint32_t
	 * @param value value to write
	 */
	virtual int write(uint32_t value, bool is_little_endian = EndiannessCheck::IS_LITTLE);

	/**
	 * Writes a system short
	 * @param value value to write
	 */
	virtual int write(uint16_t value, bool is_little_endian = EndiannessCheck::IS_LITTLE);

	/**
	 * Reads a system word
	 * @param value value to write
	 */
	virtual int read(uint64_t &value, bool is_little_endian = EndiannessCheck::IS_LITTLE);

	/**
	 * Reads a uint32_t
	 * @param value value to write
	 */
	virtual int read(uint32_t &value, bool is_little_endian = EndiannessCheck::IS_LITTLE);

	/**
	 * Reads a system short
	 * @param value value to write
	 */
	virtual int read(uint16_t &value, bool is_little_endian = EndiannessCheck::IS_LITTLE);

	/**
	 * Returns the underlying buffer
	 * @return vector's array
	 **/
	const uint8_t *getBuffer() const {
		return DataStream::getBuffer();
	}

	/**
	 * Retrieve size of data stream
	 * @return size of data stream
	 **/
	const uint64_t getSize() const {
		return DataStream::getSize();
	}

protected:

	/**
	 * Constructor that accepts host name, port and listeners. With this
	 * contructor we will be creating a server socket
	 * @param context the SocketContext
	 * @param hostname our host name
	 * @param port connecting port
	 * @param listeners number of listeners in the queue
	 */
	explicit Socket(const std::shared_ptr<SocketContext> &context, const std::string &hostname, const uint16_t port, const uint16_t listeners);

	/**
	 * Creates a vector and returns the vector using the provided
	 * type name.
	 * @param t incoming object
	 * @returns vector.
	 */
	template<typename T>
	std::vector<uint8_t> readBuffer(const T&);

	/**
	 * Creates a connection using the address info object.
	 * @param p addrinfo structure.
	 * @returns fd.
	 */
#ifdef WIN32
	virtual int8_t createConnection(const addrinfo *p, struct in_addr &addr);
#else
	virtual int8_t createConnection(const addrinfo *p, in_addr_t &addr);
#endif

	/**
	 * Sets socket options depending on the instance.
	 * @param sock socket file descriptor.
	 */
	virtual int16_t setSocketOptions(const SocketDescriptor sock);

	/**
	 * Attempt to select the socket file descriptor
	 * @param msec timeout interval to wait
	 * @returns file descriptor
	 */
	virtual int16_t select_descriptor(const uint16_t msec);


	addrinfo *addr_info_;

	std::recursive_mutex selection_mutex_;

	std::string requested_hostname_;
	std::string canonical_hostname_;
	uint16_t port_;

	bool is_loopback_only_;
	io::NetworkInterface local_network_interface_;

	// connection information
	SocketDescriptor socket_file_descriptor_;

	fd_set total_list_;
	fd_set read_fds_;
	std::atomic<uint16_t> socket_max_;
	std::atomic<uint64_t> total_written_;
	std::atomic<uint64_t> total_read_;
	uint16_t listeners_;

	bool nonBlocking_;
private:

	class SocketInitializer
	{
	public:
		SocketInitializer() {
			#ifdef WIN32
			static WSADATA s_wsaData;
			int iWinSockInitResult = WSAStartup(MAKEWORD(2, 2), &s_wsaData);
			#endif
		}
		~SocketInitializer() {
		#ifdef WIN32
			WSACleanup();
		#endif
		}
	};
	 static void initialize_socket() {
		 static SocketInitializer initialized;
		 }
	 

  std::shared_ptr<logging::Logger> logger_;


  static std::string init_hostname() {
    char hostname[1024];
    gethostname(hostname, 1024);
    Socket mySock(nullptr, hostname, 0);
    mySock.initialize();
    auto resolved_hostname = mySock.getHostname();
    return !IsNullOrEmpty(resolved_hostname) ? resolved_hostname : hostname;
  }
};

} /* namespace io */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
#endif /* LIBMINIFI_INCLUDE_IO_WIN__CLIENTSOCKET_H_ */
