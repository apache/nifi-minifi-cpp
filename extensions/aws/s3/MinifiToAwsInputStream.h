// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
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
  MinifiInputStreamBuf(std::shared_ptr<io::InputStream> stream, uint64_t content_length, gsl::not_null<std::ios*> owner)
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
  gsl::not_null<std::ios*> owner_;
};

class MinifiToAwsInputStream : private MinifiInputStreamBuf, public std::iostream {
 public:
  MinifiToAwsInputStream(std::shared_ptr<io::InputStream> stream, uint64_t content_length)
      : MinifiInputStreamBuf(std::move(stream), content_length, gsl::not_null<std::ios*>(static_cast<std::ios*>(this))),
        std::iostream(static_cast<MinifiInputStreamBuf*>(this)) {}
};

}  // namespace org::apache::nifi::minifi::aws::s3
