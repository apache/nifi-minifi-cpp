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
#ifndef EXTENSIONS_JNI_JVM_JNIMETHOD_H_
#define EXTENSIONS_JNI_JVM_JNIMETHOD_H_

#include <string>
#include <vector>
#include <sstream>
#include <iterator>
#include <algorithm>
#include <jni.h>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace jni {

/**
 * Purpose and Justification: Represents a java method signature. Is used
 * to contain the method signatures in internal objects.
 */
class JavaMethodSignature {

 public:
  JavaMethodSignature(const JavaMethodSignature &other) = delete;
  JavaMethodSignature(JavaMethodSignature &&other) = default;
  JavaMethodSignature(const std::string &name, const std::string &params, void *ptr)
      : name_(name),
        params_(params),
        ptr_(ptr) {
  }

  /**
   * Returns the method name.
   * Const cast here to clean up the caller's interface ( and java would force the loss of const )
   */
  const char *getName() const {
    return name_.c_str();
  }

  /**
   * Returns the parameters.
   * Const cast here to clean up the caller's interface ( and java would force the loss of const )
   */
  const char *getParameters() const {
    return params_.c_str();
  }

  /**
   * Returns the function pointer. can't be const.
   */
  const void *getPointer() const {
    return ptr_;
  }

  JavaMethodSignature &operator=(const JavaMethodSignature &other) = delete;
  JavaMethodSignature &operator=(JavaMethodSignature &&other) = default;

 private:

  std::string name_;
  std::string params_;
  void *ptr_;
};

/**
 * Class to keep a list of signatures
 */
class JavaSignatures {
 public:
  JavaSignatures()
      : method_ptr_(nullptr),
        size_(0) {
  }
  void addSignature(JavaMethodSignature &&signature) {
    methods_.emplace_back(std::move(signature));
  }

  bool empty() const {
    return methods_.empty() && size_ == 0;
  }

  const JNINativeMethod *getSignatures() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (method_ptr_ == nullptr || size_ != methods_.size()) {
      method_ptr_ = std::unique_ptr<JNINativeMethod[]>(new JNINativeMethod[methods_.size()]);
      size_ = methods_.size();
      for(int i=0; i < methods_.size(); i++) {
        method_ptr_[i].fnPtr = const_cast<void*>(methods_[i].getPointer());
        method_ptr_[i].name = const_cast<char*>(methods_[i].getName());
        method_ptr_[i].signature = const_cast<char*>(methods_[i].getParameters());
      }
    }
    return method_ptr_.get();
  }

  size_t getSize() const {
    return size_;
  }
 private:
  mutable std::mutex mutex_;
  mutable std::unique_ptr<JNINativeMethod[]> method_ptr_;
  mutable size_t size_;
  std::vector<JavaMethodSignature> methods_;
};

} /* namespace jni */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* EXTENSIONS_JNI_JVM_JNIMETHOD_H_ */
