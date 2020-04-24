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

#ifdef _WIN32
#define NOMINMAX
#include <winsock2.h>
#else
#include <sys/socket.h>	// socket
#include <arpa/inet.h> // inet_addr
#include <netdb.h> // hostent
#include <unistd.h> // close
#endif

#include <errno.h>
#include <string.h>

#include "api/nanofi.h"
#include "core/cstructs.h"
#include "core/cstream.h"
#include "core/log.h"

int write_uint32_t(uint32_t value, cstream * stream) {
  value = htonl(value);
  return write_buffer((uint8_t*)(&value), sizeof(uint32_t), stream);
}

int write_uint16_t(uint16_t value, cstream * stream) {
  value = htons(value);
  return write_buffer((uint8_t*)(&value), sizeof(uint16_t), stream);
}

int write_buffer(const uint8_t *value, int len, cstream * stream) {
  int ret = 0, bytes = 0;

  while (bytes < len) {
    ret = send(stream->socket_, value + bytes, len - bytes, 0);
    // check for errors
    if (ret <= 0) {
      if (ret < 0 && errno == EINTR) {
        continue;
      }
      logc(err, "Could not send to %" PRI_SOCKET ", error: %s", stream->socket_, strerror(errno));
      close_stream(stream);
      return ret;
    }
    bytes += ret;
  }

  if (bytes)
    logc(trace, "Sent data size %d over socket %" PRI_SOCKET, bytes, stream->socket_);
  return bytes;
}

int read_buffer(uint8_t *buf, int len, cstream * stream) {
  int32_t total_read = 0;
  while (len) {
    int bytes_read = recv(stream->socket_, buf, len, 0);
    if (bytes_read <= 0) {
      if (bytes_read == 0) {
        logc(debug, "Other side hung up on %" PRI_SOCKET, stream->socket_);
      } else {
        if (errno == EINTR) {
          continue;
        }
        logc(err, "Could not recv on %" PRI_SOCKET ", error: %s", stream->socket_, strerror(errno));
      }
      return -1;
    }
    len -= bytes_read;
    buf += bytes_read;
    total_read += bytes_read;
  }
  if(total_read)
    logc(trace, "Received data size %d over socket %" PRI_SOCKET, total_read, stream->socket_);
  return total_read;
}

int writeUTF(const char * cstr, uint64_t len, enum Bool widen, cstream * stream) {
  if (len > 65535) {
    return -1;
  }

  int ret;
  if (!widen) {
    uint16_t shortlen = len;
    ret = write_uint16_t(shortlen, stream);
  } else {
    ret = write_uint32_t(len, stream);
  }

  if(len == 0 || ret < 0) {
    return ret;
  }

  const uint8_t *underlyingPtr = (const uint8_t *)cstr;

  if (!widen) {
    uint16_t short_length = len;
    ret = write_buffer(underlyingPtr, short_length, stream);
  } else {
    ret = write_buffer(underlyingPtr, len, stream);
  }
  return ret;
}

int read_uint8_t(uint8_t *value, cstream * stream) {
  uint8_t val;
  int ret = read_buffer(&val, sizeof(uint8_t), stream);
  if(ret == sizeof(uint8_t)) {
    *value = val;
  }
  return ret;
}
int read_uint16_t(uint16_t *value, cstream * stream) {
  uint16_t val;
  int ret = read_buffer((uint8_t*)&val, sizeof(uint16_t), stream);
  if(ret == sizeof(uint16_t)) {
    *value = ntohs(val);
  }
  return ret;
}
int read_uint32_t(uint32_t *value, cstream * stream) {
  uint32_t val;
  int ret = read_buffer((uint8_t*)&val, sizeof(uint32_t), stream);
  if(ret == sizeof(uint32_t)) {
    *value = ntohl(val);
  }
  return ret;
}

int readUTFLen(uint32_t * utflen, cstream * stream) {
  int ret = 1;
  uint16_t shortLength = 0;
  ret = read_uint16_t(&shortLength, stream);
  if (ret > 0) {
    *utflen = shortLength;
  }
  return ret;
}

int readUTF(char * buf, uint64_t buflen, cstream * stream) {
  //return stream->impl->readData((uint8_t*)buf, buflen);
  return read_buffer((uint8_t*)buf, buflen, stream);
}

void close_stream(cstream * stream) {
  if(stream != NULL && stream->socket_ != -1) {
#ifdef _WIN32
    shutdown(stream->socket_, SD_BOTH);
    closesocket(stream->socket_);
    WSACleanup();
#else
    shutdown(stream->socket_, SHUT_RDWR);
    close(stream->socket_);
#endif
    stream->socket_ = -1;
  }
}

cstream * create_socket(const char * host, uint16_t portnum) {
  logc(trace, "Creating socket to connect to: %s:%d", host, portnum);

#ifdef _WIN32
  WSADATA wsa;
  if (WSAStartup(MAKEWORD(2,2),&wsa) != 0)
  {
    logc(err, "%s", "WSAStartup failed");
    return NULL;
  }
#endif

  struct addrinfo *result, *rp;
  struct addrinfo hints;
  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
  hints.ai_socktype = SOCK_STREAM; /* Datagram socket */
  hints.ai_flags = 0;
  hints.ai_protocol = 0;          /* Any protocol */

  char portstr[6];
  snprintf(portstr, 6, "%d", portnum);

  if (getaddrinfo(host, portstr, &hints, &result) != 0) {
    logc(err, "%s%s", "Failed to resolve hostname: ", host);
    return NULL;
  }

  SOCKET sock;

  for (rp = result; rp != NULL; rp = rp->ai_next) {
    sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
    if (sock == -1) {
      continue;
    }

    if (connect(sock, rp->ai_addr, rp->ai_addrlen) != -1) {
      break;
    }

    close(sock);
  }

  freeaddrinfo(result);

  if (rp == NULL) {
    logc(err, "Failed to connect to %s:%u", host, portnum);
    return NULL;
  }

  cstream *stream = (cstream *) malloc(sizeof(cstream));
  stream->socket_ = sock;
  logc(debug, "%s", "Socket successfully connected");
  return stream;
}

void free_socket(cstream * stream) {
  if(stream != NULL) {
    close_stream(stream);
    free(stream);
  }
}
