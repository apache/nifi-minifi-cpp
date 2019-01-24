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

#include "api/nanofi.h"
#include "core/cstructs.h"
#include "core/cxxstructs.h"
#include "core/cstream.h"
#include "io/BaseStream.h"
#include "io/DataStream.h"
#include "io/ClientSocket.h"
#include "cxx/Instance.h"

int write_uint64_t(uint64_t value, cstream * stream) {
  return stream->impl->Serializable::write(value, stream->impl);
}
int write_uint32_t(uint32_t value, cstream * stream) {
  return stream->impl->Serializable::write(value, stream->impl);
}
int write_uint16_t(uint16_t value, cstream * stream) {
  return stream->impl->Serializable::write(value, stream->impl);
}
int write_uint8_t(uint8_t value, cstream * stream) {
  return stream->impl->Serializable::write(value, stream->impl);
}
int write_char(char value, cstream * stream) {
  return stream->impl->Serializable::write(value, stream->impl);
}
int write_buffer(const uint8_t *value, int len, cstream * stream) {
  int ret_val = stream->impl->Serializable::write(const_cast<uint8_t *>(value), len, stream->impl);
  return ret_val;
}

int writeUTF(const char * cstr, uint64_t len, Bool widen, cstream * stream) {
  std::string str(cstr, len);
  return stream->impl->Serializable::writeUTF(str, stream->impl, widen == True);
}

int read_char(char *value, cstream * stream) {
  char val;
  int ret = stream->impl->Serializable::read(val, stream->impl);
  if(ret == sizeof(char)) {
    *value = val;
  }
  return ret;
}
int read_uint8_t(uint8_t *value, cstream * stream) {
  uint8_t val;
  int ret = stream->impl->Serializable::read(val, stream->impl);
  if(ret == sizeof(uint8_t)) {
    *value = val;
  }
  return ret;
}
int read_uint16_t(uint16_t *value, cstream * stream) {
  uint16_t val;
  int ret = stream->impl->Serializable::read(val, stream->impl);
  if(ret == sizeof(uint16_t)) {
    *value = val;
  }
  return ret;
}
int read_uint32_t(uint32_t *value, cstream * stream) {
  uint32_t val;
  int ret = stream->impl->Serializable::read(val, stream->impl);
  if(ret == sizeof(uint32_t)) {
    *value = val;
  }
  return ret;
}
int read_uint64_t(uint64_t *value, cstream * stream) {
  uint64_t val;
  int ret = stream->impl->Serializable::read(val, stream->impl);
  if(ret == sizeof(uint64_t)) {
    *value = val;
  }
  return ret;
}

int read_buffer(uint8_t *value, int len, cstream * stream) {
  return stream->impl->Serializable::read(value, len, stream->impl);
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
  return stream->impl->readData((uint8_t*)buf, buflen);
}

void close_stream(cstream * stream) {
  if(stream != NULL && stream->impl != NULL) {
    stream->impl->closeStream();
  }
}

int open_stream(cstream * stream) {
  if(stream != NULL && stream->impl != NULL) {
    return stream->impl->initialize();
  }
  return -1;
}

cstream * create_socket(const char * host, uint16_t portnum) {
  nifi_port nport;

  char random_port[6] = "65443";

  nport.port_id = random_port;

  nifi_instance *instance = create_instance(host, &nport);

  auto minifi_instance_ref = static_cast<minifi::Instance*>(instance->instance_ptr);

  auto stream_factory_ = minifi::io::StreamFactory::getInstance(minifi_instance_ref->getConfiguration());

  cstream * stream = (cstream*)malloc(sizeof(cstream));

  auto socket = stream_factory_->createSocket(host, portnum);

  free_instance(instance);

  if(socket) {
    if(socket->initialize() == 0) {
      stream->impl = socket.release();
      return stream;
    }
  }
  return NULL;
}

void free_socket(cstream * stream) {
  if(stream != NULL) {
    if(stream->impl != NULL) {
      auto socket = static_cast<minifi::io::Socket*>(stream->impl);
      if(socket) {
        socket->closeStream();
        delete socket; //This is ugly, but only sockets get deleted this way
      }
    }
    free(stream);
  }
}
