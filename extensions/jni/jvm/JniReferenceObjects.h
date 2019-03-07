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

#ifndef EXTENSIONS_JNI_JVM_REFERNCEOBJECTS_H_
#define EXTENSIONS_JNI_JVM_REFERNCEOBJECTS_H_

#include <string>
#include <vector>
#include <sstream>
#include <iterator>
#include <algorithm>
#include <jni.h>
#include "JavaServicer.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/WeakReference.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace jni {

/**
 * Represents a flow file. Exists to provide the ability to remove
 * global references amongst the
 */
class JniFlowFile : public core::WeakReference {
 public:
  JniFlowFile(std::shared_ptr<core::FlowFile> ref, const std::shared_ptr<JavaServicer> &servicer, jobject ff)
      : removed(false),
        ff_object(ff),
        ref_(ref),
        servicer_(servicer) {

  }

  virtual ~JniFlowFile() {

  }

  virtual void remove() override;

  std::shared_ptr<core::FlowFile> get() const {
    return ref_;
  }

  jobject getJniReference() {
    return ff_object;
  }

  bool operator==(const JniFlowFile &other) const {
    // compare the pointers
    return ref_ == other.ref_;
  }

  bool empty() const {
    return removed;
  }

 protected:

  bool removed;

  jobject ff_object;

  std::mutex session_mutex_;

  std::shared_ptr<core::FlowFile> ref_;

  std::shared_ptr<JavaServicer> servicer_;

};

/**
 * Quick check to determine if a FF is empty.
 */
struct check_empty_ff : public std::unary_function<std::shared_ptr<JniFlowFile>, bool> {
  bool operator()(std::shared_ptr<JniFlowFile> session) const {
    return session->empty();
  }
};

class JniByteOutStream : public minifi::OutputStreamCallback {
 public:
  JniByteOutStream(jbyte *bytes, size_t length)
      : bytes_(bytes),
        length_(length) {

  }

  virtual ~JniByteOutStream() {

  }
  virtual int64_t process(std::shared_ptr<minifi::io::BaseStream> stream) {
    return stream->write((uint8_t*) bytes_, length_);
  }
 private:
  jbyte *bytes_;
  size_t length_;
};

/**
 * Jni byte input stream
 */
class JniByteInputStream : public minifi::InputStreamCallback {
 public:
  JniByteInputStream(uint64_t size)
      : read_size_(0),
        stream_(nullptr) {
    buffer_size_ = size;
    buffer_ = new uint8_t[buffer_size_];
  }
  ~JniByteInputStream() {
    if (buffer_)
      delete[] buffer_;
  }
  int64_t process(std::shared_ptr<minifi::io::BaseStream> stream) {
    stream_ = stream;
    return 0;
  }

  int64_t read(JNIEnv *env, jbyteArray arr, int offset, int size) {
    if (stream_ == nullptr) {
      return -1;
    }

    // seek to offset
    int remaining = size;
    int writtenOffset = 0;
    int read = 0;
    do {

      int actual = (int) stream_->read(buffer_, remaining <= buffer_size_ ? remaining : buffer_size_);
      if (actual <= 0) {
        if (read == 0) {
          stream_ = nullptr;
          return -1;
        }
        break;
      }

      read += actual;
      env->SetByteArrayRegion(arr, offset + writtenOffset, actual, (jbyte*) buffer_);
      writtenOffset += (int) actual;

      remaining -= actual;

    } while (remaining > 0);

    return read;
  }

  int64_t read(char &arr) {
    return stream_->read(arr);
  }

  std::shared_ptr<minifi::io::BaseStream> stream_;
  uint8_t *buffer_;
  uint64_t buffer_size_;
  uint64_t read_size_;
};

class JniInputStream : public core::WeakReference {
 public:
  JniInputStream(std::unique_ptr<JniByteInputStream> jbi, jobject in_instance, const std::shared_ptr<JavaServicer> &servicer)
      : removed_(false),
        jbi_(std::move(jbi)),
        in_instance_(in_instance),
        servicer_(servicer) {

  }

  virtual void remove() override {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!removed_) {
      servicer_->attach()->DeleteGlobalRef(in_instance_);
      removed_ = true;
      jbi_ = nullptr;
    }

  }

  int64_t read(char &arr) {
    if (!removed_) {
      return jbi_->read(arr);
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
  bool removed_;
  jobject in_instance_;
  std::unique_ptr<JniByteInputStream> jbi_;
  std::shared_ptr<JavaServicer> servicer_;
};

class JniSession : public core::WeakReference {
 public:
  JniSession(const std::shared_ptr<core::ProcessSession> &session, jobject session_instance, const std::shared_ptr<JavaServicer> &servicer)
      : removed_(false),
        session_(session),
        servicer_(servicer),
        session_instance_(session_instance) {
  }

  virtual void remove() override {
    std::lock_guard<std::mutex> guard(session_mutex_);
    if (!removed_) {
      for (auto ff : global_ff_objects_) {
        ff->remove();
      }

      for (auto in : input_streams_) {
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

  JniFlowFile * addFlowFile(std::shared_ptr<JniFlowFile> ff) {
    std::lock_guard<std::mutex> guard(session_mutex_);
    global_ff_objects_.push_back(ff);
    return ff.get();
  }

  void addInputStream(std::shared_ptr<JniInputStream> in) {
    std::lock_guard<std::mutex> guard(session_mutex_);
    input_streams_.push_back(in);
  }

  std::shared_ptr<JavaServicer> getServicer() const {
    return servicer_;
  }

  bool prune() {
    global_ff_objects_.erase(std::remove_if(global_ff_objects_.begin(), global_ff_objects_.end(), check_empty_ff()), global_ff_objects_.end());
    if (global_ff_objects_.empty()) {
      remove();
    }
    return global_ff_objects_.empty();
  }
  bool empty() const {
    std::lock_guard<std::mutex> guard(session_mutex_);
    for (auto ff : global_ff_objects_) {
      if (!ff->empty())
        return false;
    }
    return true;
  }

 protected:
  bool removed_;
  mutable std::mutex session_mutex_;
  jobject session_instance_;
  std::shared_ptr<core::ProcessSession> session_;
  std::shared_ptr<JavaServicer> servicer_;
  std::vector<std::shared_ptr<JniInputStream>> input_streams_;
  // we own
  std::vector<std::shared_ptr<JniFlowFile>> global_ff_objects_;
};

struct check_empty : public std::unary_function<std::shared_ptr<JniSession>, bool> {
  bool operator()(std::shared_ptr<JniSession> session) const {
    return session->prune();
  }
};

class JniSessionFactory : public core::WeakReference {
 public:

  JniSessionFactory(const std::shared_ptr<core::ProcessSessionFactory> &factory, const std::shared_ptr<JavaServicer> &servicer, jobject java_object)
      : servicer_(servicer),
        java_object_(java_object),
        factory_(factory) {
  }

  virtual void remove() override {
    std::lock_guard<std::mutex> guard(session_mutex_);
    // remove all of the sessions
    // this should spark their destructor
    for (auto session : sessions_) {
      session->remove();
    }
    sessions_.clear();

    if (java_object_) {
      // detach the global reference and let Java take care of this object.
      servicer_->attach()->DeleteGlobalRef(java_object_);
    }
  }

  jobject getJavaReference() const {
    return java_object_;
  }

  std::shared_ptr<JavaServicer> getServicer() const {
    return servicer_;
  }

  std::shared_ptr<core::ProcessSessionFactory> getFactory() const {
    return factory_;
  }

  /**
   */
  JniSession *addSession(std::shared_ptr<JniSession> session) {
    std::lock_guard<std::mutex> guard(session_mutex_);

    sessions_.erase(std::remove_if(sessions_.begin(), sessions_.end(), check_empty()), sessions_.end());

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



} /* namespace jni */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* EXTENSIONS_JNI_JVM_REFERNCEOBJECTS_H_ */
