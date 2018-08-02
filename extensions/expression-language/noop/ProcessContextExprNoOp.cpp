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

#include <core/ProcessContext.h>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {

bool ProcessContext::getProperty(const Property &property, std::string &value,
                                 const std::shared_ptr<FlowFile> &flow_file) {
  return getProperty(property.getName(), value);
}

bool ProcessContext::getDynamicProperty(const Property &property, std::string &value,
                                 const std::shared_ptr<FlowFile> &flow_file) {
  return getDynamicProperty(property.getName(), value);
}

} /* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
