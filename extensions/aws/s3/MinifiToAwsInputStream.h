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
#pragma once

#include <streambuf>
#include <iostream>
#include <memory>
#include <vector>

#include "utils/ConfigurationUtils.h"
#include "minifi-cpp/io/InputStream.h"
#include "minifi-cpp/utils/gsl.h"

namespace org::apache::nifi::minifi::aws::s3 {

class MinifiInputStreamBuf : public std::streambuf {
 public:
  MinifiInputStreamBuf(std::shared_ptr<io::InputStream> stream, uint64_t content_length, gsl::not_null<std::basic_ios<char>*> owner)
      : stream_(std::move(stream)),
        start_pos_(stream_->tell()),
        content_length_(content_length),
        buffer_(utils::configuration::DEFAULT_BUFFER_SIZE),
        owner_(owner) {}

 protected:
  int_type underflow() override;
  pos_type seekoff(off_type off, std::ios_base::seekdir way, std::ios_base::openmode which) override;
  pos_type seekpos(pos_type pos, std::ios_base::openmode which) override;

 private:
  std::shared_ptr<io::InputStream> stream_;
  uint64_t start_pos_;
  uint64_t content_length_;
  std::vector<char> buffer_;
  gsl::not_null<std::basic_ios<char>*> owner_;
};

class MinifiToAwsInputStream : private MinifiInputStreamBuf, public std::basic_iostream<char> {
 public:
  MinifiToAwsInputStream(std::shared_ptr<io::InputStream> stream, uint64_t content_length)
      : MinifiInputStreamBuf(std::move(stream), content_length, gsl::not_null<std::basic_ios<char>*>(static_cast<std::basic_ios<char>*>(this))),
        std::basic_iostream<char>(static_cast<MinifiInputStreamBuf*>(this)) {}
};

}  // namespace org::apache::nifi::minifi::aws::s3
