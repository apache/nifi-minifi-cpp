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
#ifndef LIBMINIFI_INCLUDE_CORE_DEPRECATED_H_
#define LIBMINIFI_INCLUDE_CORE_DEPRECATED_H_

#ifdef _MSC_VER
#define DEPRECATED(v, ev) __declspec(deprecated)
#elif defined(__GNUC__) | defined(__clang__)
#define DEPRECATED(v, ev) __attribute__((__deprecated__))
#else
#define DEPRECATED(v, ev)
#endif

#endif /* LIBMINIFI_INCLUDE_CORE_DEPRECATED_H_ */
