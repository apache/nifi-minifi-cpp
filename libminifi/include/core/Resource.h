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

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN 1
#endif

#include "ClassLoader.h"
#include "agent/agent_docs.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {

#define MKSOC(x) #x
#define MAKESTRING(x) MKSOC(x)

template<class T>
class StaticClassType {
 public:

  StaticClassType(const std::string &name, const std::string &description = "") {
    // Notify when the static member is created
    if (!description.empty()){
      minifi::AgentDocs::putDescription(name,description);
    }
#ifdef MODULE_NAME
    ClassLoader::getDefaultClassLoader().registerClass(name, std::unique_ptr<ObjectFactory>(new DefautObjectFactory<T>(MAKESTRING(MODULE_NAME))));
#else
    ClassLoader::getDefaultClassLoader().registerClass(name, std::unique_ptr<ObjectFactory>(new DefautObjectFactory<T>("minifi-system")));
#endif
  }
};

#define REGISTER_RESOURCE(CLASSNAME,DESC) \
        static core::StaticClassType<CLASSNAME> \
        CLASSNAME##_registrar( #CLASSNAME, DESC );

#define REGISTER_RESOURCE_AS(CLASSNAME,NAME) \
        static core::StaticClassType<CLASSNAME> \
        CLASSNAME##_registrar( #NAME );

}/* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* LIBMINIFI_INCLUDE_CORE_RESOURCE_H_ */
