/**
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

#ifndef NIFI_MINIFI_CPP_FRAMEIO_H
#define NIFI_MINIFI_CPP_FRAMEIO_H

#include "utils/gsl.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace opencv {

class FrameWriteCallback : public OutputStreamCallback {
  public:
    explicit FrameWriteCallback(cv::Mat image_mat, std::string image_encoding_)
    // TODO - Nghia: Check std::move(img_mat).
        : image_mat_(std::move(image_mat)), image_encoding_(image_encoding_) {
    }
    ~FrameWriteCallback() override = default;

    int64_t process(const std::shared_ptr<io::BaseStream>& stream) override {
      imencode(image_encoding_, image_mat_, image_buf_);
      const auto ret = stream->write(image_buf_.data(), image_buf_.size());
      return io::isError(ret) ? -1 : gsl::narrow<int64_t>(ret);
    }

  private:
    std::vector<uchar> image_buf_;
    cv::Mat image_mat_;
    std::string image_encoding_;
};

class FrameReadCallback : public InputStreamCallback {
  public:
    explicit FrameReadCallback(cv::Mat &image_mat)
        : image_mat_(image_mat) {
    }
    ~FrameReadCallback() override = default;

    int64_t process(const std::shared_ptr<io::BaseStream>& stream) override {
      int64_t ret = 0;
      image_buf_.resize(stream->size());
      ret = stream->read(image_buf_.data(), static_cast<int>(stream->size()));
      if (ret < 0 || static_cast<std::size_t>(ret) != stream->size()) {
        throw std::runtime_error("ImageReadCallback failed to fully read flow file input stream");
      }
      image_mat_ = cv::imdecode(image_buf_, -1);
      return ret;
    }

  private:
    std::vector<uchar> image_buf_;
    cv::Mat &image_mat_;
};

} /* namespace opencv */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif  // NIFI_MINIFI_CPP_FRAMEIO_H
