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
#ifndef __SERIALIZABLE_H__
#define __SERIALIZABLE_H__

#include <cstdint>
#include <vector>
#include <string>


/**
 * Mechanism to determine endianness of host.
 * Accounts for only BIG/LITTLE/BIENDIAN
 **/
class EndiannessCheck
{
public:
    static bool IS_LITTLE;
private:

    static bool is_little_endian() {
        /* do whatever is needed at static init time */
        unsigned int x = 1;
        char *c = (char*) &x;
        IS_LITTLE=*c==1;
        return IS_LITTLE;
    }
};


/**
 * DataStream defines the mechanism through which
 * binary data will be written to a sink
 */
class DataStream
{
public:

    DataStream() : readBuffer(0)
    {

    }

    /**
     * Constructor
     **/
    DataStream(const uint8_t *buf, const uint32_t buflen) : DataStream()
    {
        writeData((uint8_t*)buf,buflen);

    }

	/**
	 * Reads data and places it into buf
	 * @param buf buffer in which we extract data
	 * @param buflen
	 */
	int readData(std::vector<uint8_t> &buf, int buflen);
	/**
	 * Reads data and places it into buf
	 * @param buf buffer in which we extract data
	 * @param buflen
	 */
    int readData(uint8_t *buf, int buflen);

    /**
     * writes valiue to buffer
     * @param value value to write
     * @param size size of value
     */
    int writeData(uint8_t *value, int size);


    /**
     * Reads a system word
     * @param value value to write
     */
    inline int readLongLong(uint64_t &value, bool is_little_endian =
                                EndiannessCheck::IS_LITTLE);


    /**
     * Reads a uint32_t
     * @param value value to write
     */
    inline int readLong(uint32_t &value, bool is_little_endian =
                            EndiannessCheck::IS_LITTLE);


    /**
     * Reads a system short
     * @param value value to write
     */
    inline int readShort(uint16_t &value,bool is_little_endian =
                             EndiannessCheck::IS_LITTLE);


    /**
     * Returns the underlying buffer
     * @return vector's array
     **/
    const uint8_t *getBuffer() const
    {
        return &buffer[0];
    }

    /**
     * Retrieve size of data stream
     * @return size of data stream
     **/
    const uint32_t getSize() const
    {
        return buffer.size();
    }

private:
    // All serialization related method and internal buf
    std::vector<uint8_t> buffer;
    uint32_t readBuffer;
};

/**
 * Serializable instances provide base functionality to
 * write certain objects/primitives to a data stream.
 *
 */
class Serializable {

public:

    /**
     * Inline function to write T to stream
     **/
    template<typename T>
    inline int writeData(const T &t,DataStream *stream);

    /**
     * Inline function to write T to to_vec
     **/
    template<typename T>
    inline int writeData(const T &t, uint8_t *to_vec);

    /**
     * Inline function to write T to to_vec
     **/
    template<typename T>
    inline int writeData(const T &t, std::vector<uint8_t> &to_vec);


    /**
     * write byte to stream
     * @return resulting write size
     **/
    int write(uint8_t value,DataStream *stream);

    /**
     * write byte to stream
     * @return resulting write size
     **/
    int write(char value,DataStream *stream);

    /**
     * write 4 bytes to stream
     * @param base_value non encoded value
     * @param stream output stream
     * @param is_little_endian endianness determination
     * @return resulting write size
     **/
    int write(uint32_t base_value,DataStream *stream, bool is_little_endian =
                  EndiannessCheck::IS_LITTLE);

    /**
     * write 2 bytes to stream
     * @param base_value non encoded value
     * @param stream output stream
     * @param is_little_endian endianness determination
     * @return resulting write size
     **/
    int write(uint16_t base_value,DataStream *stream, bool is_little_endian =
                  EndiannessCheck::IS_LITTLE);

    /**
     * write valueto stream
     * @param value non encoded value
     * @param len length of value
     * @param strema output stream
     * @return resulting write size
     **/
    int write(uint8_t *value, int len,DataStream *stream);

    /**
     * write 8 bytes to stream
     * @param base_value non encoded value
     * @param stream output stream
     * @param is_little_endian endianness determination
     * @return resulting write size
     **/
    int write(uint64_t base_value,DataStream *stream, bool is_little_endian =
                  EndiannessCheck::IS_LITTLE);

    /**
    * write bool to stream
     * @param value non encoded value
     * @return resulting write size
     **/
    int write(bool value);

    /**
     * write UTF string to stream
     * @param str string to write
     * @return resulting write size
     **/
    int writeUTF(std::string str,DataStream *stream, bool widen = false);

    /**
     * reads a byte from the stream
     * @param value reference in which will set the result
     * @param stream stream from which we will read
     * @return resulting read size
     **/
    int read(uint8_t &value,DataStream *stream);

    /**
     * reads two bytes from the stream
     * @param value reference in which will set the result
     * @param stream stream from which we will read
     * @return resulting read size
     **/
    int read(uint16_t &base_value,DataStream *stream, bool is_little_endian =
                 EndiannessCheck::IS_LITTLE);

    /**
     * reads a byte from the stream
     * @param value reference in which will set the result
     * @param stream stream from which we will read
     * @return resulting read size
     **/
    int read(char &value,DataStream *stream);

    /**
     * reads a byte array from the stream
     * @param value reference in which will set the result
     * @param len length to read
     * @param stream stream from which we will read
     * @return resulting read size
     **/
    int read(uint8_t *value, int len,DataStream *stream);

    /**
     * reads four bytes from the stream
     * @param value reference in which will set the result
     * @param stream stream from which we will read
     * @return resulting read size
     **/
    int read(uint32_t &value,DataStream *stream,
             bool is_little_endian = EndiannessCheck::IS_LITTLE);

    /**
     * reads eight byte from the stream
     * @param value reference in which will set the result
     * @param stream stream from which we will read
     * @return resulting read size
     **/
    int read(uint64_t &value,DataStream *stream,
             bool is_little_endian = EndiannessCheck::IS_LITTLE);

    /**
     * read UTF from stream
     * @param str reference string
     * @param stream stream from which we will read
     * @return resulting read size
     **/
    int readUTF(std::string &str,DataStream *stream, bool widen = false);

protected:

};

#endif
