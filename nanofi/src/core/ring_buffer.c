/*
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

#include <core/ring_buffer.h>

size_t min(size_t x, size_t y) {
    if (x <= y) {
        return x;
    }
    return y;
}

size_t write_ring_buffer(ring_buffer_t * rb, const char * payload, size_t length) {
    if (!rb || !rb->data || !payload || !length) {
        return 0;
    }

    if (rb->capacity - rb->size == 0) {
        //buffer is full
        return 0;
    }

    size_t capacity = rb->capacity;
    size_t available = capacity - rb->size;
    size_t bytes_to_write = min(available, length);

    if (bytes_to_write <= (capacity - rb->write_index)) {
        memcpy(rb->data + rb->write_index, payload, bytes_to_write);
        rb->write_index += bytes_to_write;
        if (rb->write_index == rb->capacity) {
            rb->write_index = 0;
        }
    } else {
        //rotate and write
        size_t size1 = capacity - rb->write_index;
        memcpy(rb->data + rb->write_index, payload, size1);
        size_t size2 = bytes_to_write - size1;
        memcpy(rb->data, payload + size1, size2);
        rb->write_index = size2;
    }
    rb->size += bytes_to_write;
    return bytes_to_write;
}

size_t read_ring_buffer(ring_buffer_t * rb, char * payload, size_t length) {
    if (!rb || !rb->data || !payload || !length) {
        return 0;
    }

    if (rb->size == 0) {
        //buffer is empty
        return 0;
    }

    size_t capacity = rb->capacity;
    size_t bytes_to_read = min(rb->size, length);

    if (bytes_to_read <= (capacity - rb->read_index)) {
        memcpy(payload, rb->data + rb->read_index, bytes_to_read);
        rb->read_index += bytes_to_read;
        if (rb->read_index == rb->capacity) {
            rb->read_index = 0;
        }
    } else {
        size_t size1 = capacity - rb->read_index;
        memcpy(payload, rb->data + rb->read_index, size1);
        size_t size2 = bytes_to_read - size1;
        memcpy(payload + size1, rb->data, size2);
        rb->read_index = size2;
    }
    rb->size -= bytes_to_read;
    return bytes_to_read;
}

size_t check_available_space(ring_buffer_t * rb) {
    //this is a hypothetical situation. no harm in safety check
    if (rb->capacity < rb->size)
        return 0;
    return rb->capacity - rb->size;
}

void free_ring_buffer(ring_buffer_t * rb) {
    free(rb->data);
    free(rb);
}
