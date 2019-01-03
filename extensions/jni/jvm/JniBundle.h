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
#ifndef EXTENSIONS_JNIBUNDLE_H
#define EXTENSIONS_JNIBUNDLE_H

#include <string>
#include <vector>
#include <sstream>
#include <iterator>
#include <algorithm>
#include <jni.h>
#include "JniProcessContext.h"
#include "JniFlowFile.h"
#include "JniProcessSession.h"
#include "agent/build_description.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace jni {

/**
 * Purpose and Justification: JniBundle represents the interconnect between NiFi Java
 * bundles and MiNiFi C++ bundles.
 */
class JniBundle {

 public:

  explicit JniBundle(struct BundleDetails details)
      : details_(details) {
  }

  JniBundle() {
  }

  /**
   * Add a description to this bundle
   * @param description
   */
  void addDescription(ClassDescription description) {
    descriptions_.push_back(description);
  }

  /**
   * Retrives a copy of the descriptions.
   */
  std::vector<ClassDescription> getDescriptions() const {
    return descriptions_;
  }

  /**
   * Returns a copy of BundleDetails.
   */
  struct BundleDetails getDetails() const {
    return details_;
  }

 private:
  std::vector<ClassDescription> descriptions_;
  struct BundleDetails details_;

};

} /* namespace jni */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* EXTENSIONS_JNIBUNDLE_H */
