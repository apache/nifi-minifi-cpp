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

#include <jni.h>

#include <string>
#include <vector>
#include <sstream>
#include <iterator>
#include <algorithm>
#include <functional>
#include <memory>
#include <utility>

#include "JavaServicer.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/WeakReference.h"
#include "utils/gsl.h"
#include "range/v3/algorithm/remove_if.hpp"
#include "range/v3/algorithm/all_of.hpp"

namespace org::apache::nifi::minifi::jni {

/**
 * Represents a flow file. Exists to provide the ability to remove
 * global references amongst the
 */
class JniFlowFile : public core::WeakReference {
 public:
  JniFlowFile(std::shared_ptr<core::FlowFile> ref, std::shared_ptr<JavaServicer> servicer, jobject ff)
      : ff_object(ff),
        ref_(std::move(ref)),
        servicer_(std::move(servicer)) {
  }

  ~JniFlowFile() override = default;

  void remove() override;

  [[nodiscard]] std::shared_ptr<core::FlowFile> get() const {
    return ref_;
  }

  jobject getJniReference() {
    return ff_object;
  }

  bool operator==(const JniFlowFile &other) const {
    // compare the pointers
    return ref_ == other.ref_;
  }

  [[nodiscard]] bool empty() const {
    return removed;
  }

 protected:
  bool removed = false;

  jobject ff_object;

  std::mutex session_mutex_;

  std::shared_ptr<core::FlowFile> ref_;

  std::shared_ptr<JavaServicer> servicer_;
};

/**
 * Jni byte input stream
 */
class JniByteInputStream {
 public:
  explicit JniByteInputStream(uint64_t size)
      : buffer_(size) {
  }
  int64_t operator()(const std::shared_ptr<minifi::io::InputStream>& stream) {
    stream_ = stream;
    return 0;
  }

  int64_t read(JNIEnv *env, jbyteArray arr, int offset, int size) {
    gsl_Expects(size >= 0);
    if (stream_ == nullptr) {
      return -1;
    }

    // seek to offset
    auto remaining = gsl::narrow<size_t>(size);
    int writtenOffset = 0;
    int read = 0;
    do {
      // JNI takes size as int, there's not much we can do here to support 2GB+ sizes
      int actual = static_cast<int>(stream_->read(std::span(buffer_).subspan(0, std::min(remaining, buffer_.size()))));
      if (actual <= 0) {
        if (read == 0) {
          stream_ = nullptr;
          return -1;
        }
        break;
      }

      read += actual;
      env->SetByteArrayRegion(arr, offset + writtenOffset, actual, reinterpret_cast<jbyte*>(buffer_.data()));
      writtenOffset += actual;

      remaining -= actual;
    } while (remaining > 0);

    return read;
  }

  int64_t read(uint8_t& value) const {
    auto read_result = stream_->read(value);
    if (io::isError(read_result))
      return -1;
    return gsl::narrow<int64_t>(read_result);
  }

  std::shared_ptr<minifi::io::InputStream> stream_;
  std::vector<std::byte> buffer_;
};

class JniInputStream : public core::WeakReference {
 public:
  JniInputStream(std::unique_ptr<JniByteInputStream> jbi, jobject in_instance, std::shared_ptr<JavaServicer> servicer)
      : in_instance_(in_instance),
        jbi_(std::move(jbi)),
        servicer_(std::move(servicer)) {
  }

  void remove() override {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!removed_) {
      servicer_->attach()->DeleteGlobalRef(in_instance_);
      removed_ = true;
      jbi_ = nullptr;
    }
  }

  int64_t read(uint8_t& value) {
    if (!removed_) {
      return jbi_->read(value);
    }
    return -1;
  }

  int64_t read(JNIEnv *env, jbyteArray arr, int offset, int size) {
    if (!removed_) {
      return jbi_->read(env, arr, offset, size);
    }
    return -1;
  }

 private:
  std::mutex mutex_;
  bool removed_ = false;
  jobject in_instance_;
  std::unique_ptr<JniByteInputStream> jbi_;
  std::shared_ptr<JavaServicer> servicer_;
};

class JniSession : public core::WeakReference {
 public:
  JniSession(std::shared_ptr<core::ProcessSession> session, jobject session_instance, std::shared_ptr<JavaServicer> servicer)
      : session_instance_(session_instance),
        session_(std::move(session)),
        servicer_(std::move(servicer)) {
  }

  void remove() override {
    std::lock_guard<std::mutex> guard(session_mutex_);
    if (!removed_) {
      for (const auto& ff : global_ff_objects_) {
        ff->remove();
      }

      for (const auto& in : input_streams_) {
        in->remove();
      }
      global_ff_objects_.clear();
      servicer_->attach()->DeleteGlobalRef(session_instance_);
      removed_ = true;
    }
  }

  std::shared_ptr<core::ProcessSession> &getSession() {
    return session_;
  }

  JniFlowFile *getFlowFileReference(const std::shared_ptr<core::FlowFile> &ff) {
    for (auto &jni_ff : global_ff_objects_) {
      if (jni_ff->get().get() == ff.get()) {
        return jni_ff.get();
      }
    }
    return nullptr;
  }

  JniFlowFile * addFlowFile(const std::shared_ptr<JniFlowFile>& ff) {
    std::lock_guard<std::mutex> guard(session_mutex_);
    global_ff_objects_.push_back(ff);
    return ff.get();
  }

  void addInputStream(const std::shared_ptr<JniInputStream>& in) {
    std::lock_guard<std::mutex> guard(session_mutex_);
    input_streams_.push_back(in);
  }

  std::shared_ptr<JavaServicer> getServicer() const {
    return servicer_;
  }

  bool prune() {
    ranges::remove_if(global_ff_objects_, [](const std::shared_ptr<JniFlowFile>& flow_file) { return flow_file->empty(); });
    if (global_ff_objects_.empty()) {
      remove();
    }
    return global_ff_objects_.empty();
  }
  bool empty() const {
    std::lock_guard<std::mutex> guard(session_mutex_);
    return ranges::all_of(global_ff_objects_, [](const auto& ff) -> bool { return ff->empty(); });
  }

 protected:
  bool removed_ = false;
  mutable std::mutex session_mutex_;
  jobject session_instance_;
  std::shared_ptr<core::ProcessSession> session_;
  std::shared_ptr<JavaServicer> servicer_;
  std::vector<std::shared_ptr<JniInputStream>> input_streams_;
  // we own
  std::vector<std::shared_ptr<JniFlowFile>> global_ff_objects_;
};

class JniSessionFactory : public core::WeakReference {
 public:
  JniSessionFactory(std::shared_ptr<core::ProcessSessionFactory> factory, std::shared_ptr<JavaServicer> servicer, jobject java_object)
      : servicer_(std::move(servicer)),
        factory_(std::move(factory)),
        java_object_(java_object) {
  }

  void remove() override {
    std::lock_guard<std::mutex> guard(session_mutex_);
    // remove all sessions
    // this should spark their destructor
    for (const auto& session : sessions_) {
      session->remove();
    }
    sessions_.clear();

    if (java_object_) {
      // detach the global reference and let Java take care of this object.
      servicer_->attach()->DeleteGlobalRef(java_object_);
    }
  }

  [[nodiscard]] jobject getJavaReference() const {
    return java_object_;
  }

  [[nodiscard]] std::shared_ptr<JavaServicer> getServicer() const {
    return servicer_;
  }

  [[nodiscard]] std::shared_ptr<core::ProcessSessionFactory> getFactory() const {
    return factory_;
  }

  /**
   */
  JniSession *addSession(const std::shared_ptr<JniSession>& session) {
    std::lock_guard<std::mutex> guard(session_mutex_);

    ranges::remove_if(sessions_, [](const auto& session) { return session->prune(); });

    sessions_.push_back(session);

    return session.get();
  }

 protected:
  std::shared_ptr<JavaServicer> servicer_;
  std::mutex session_mutex_;
  // we do not own this shared ptr
  std::shared_ptr<core::ProcessSessionFactory> factory_;
  // we own the sessions
  std::vector<std::shared_ptr<JniSession>> sessions_;
  // we own the java object
  jobject java_object_;
};



}  // namespace org::apache::nifi::minifi::jni
