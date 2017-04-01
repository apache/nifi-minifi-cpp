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
#ifndef LIBMINIFI_INCLUDE_CORE_RESOURCE_H_
#define LIBMINIFI_INCLUDE_CORE_RESOURCE_H_

#include "ClassLoader.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {

template<class T>
class StaticClassType {
 public:

  StaticClassType(const std::string &name) {
    // Notify when the static member is created
    ClassLoader::getDefaultClassLoader().registerClass(
        name, std::unique_ptr<ObjectFactory>(new DefautObjectFactory<T>()));
  }
};

#define REGISTER_RESOURCE(CLASSNAME) \
        static core::StaticClassType<CLASSNAME> \
        CLASSNAME##_registrar( #CLASSNAME );

#define REGISTER_RESOURCE_AS(CLASSNAME,NAME) \
        static core::StaticClassType<CLASSNAME> \
        CLASSNAME##_registrar( #NAME );

}/* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* LIBMINIFI_INCLUDE_CORE_RESOURCE_H_ */
