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
#include "Exception.h"

#ifndef WIN32
#include <cxxabi.h>
#endif  // !WIN32

#include <typeinfo>

namespace org::apache::nifi::minifi {
std::string getCurrentExceptionTypeName() {
#ifndef WIN32
  const std::type_info* exception_type = abi::__cxa_current_exception_type();
  if (exception_type) {
    return exception_type->name();
  }
#endif  // !WIN32
  try {
    std::rethrow_exception(std::current_exception());
  } catch (const std::exception& ex) {
    return typeid(ex).name();
  } catch (...) { }

  return {};
}
}  // namespace org::apache::nifi::minifi
