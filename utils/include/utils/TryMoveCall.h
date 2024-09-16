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
#pragma once

#include <type_traits>
#include <utility>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {

// TryMoveCall calls an
//  - unary function of a lvalue reference-type argument by passing a ref
//  - unary function of any other argument type by moving into it
template<typename /* FunType */, typename T, typename = void>
struct TryMoveCall {
    template<typename Fun>
    static auto call(Fun&& fun, T& elem) -> decltype(std::forward<Fun>(fun)(elem)) { return std::forward<Fun>(fun)(elem); }
};

// 1.) std::declval looks similar to this: template<typename T> T&& declval();.
//     Not defined, therefore it's only usable in unevaluated context.
//     No requirements regarding T, therefore makes it possible to create hypothetical objects
//     without requiring e.g. a default constructor and a destructor, like the T{} expression does.
// 2.) std::declval<FunType>() resolves to an object of type FunType. If FunType is an lvalue reference,
//     then this will also result in an lvalue reference due to reference collapsing.
// 3.) std::declval<FunType>()(std::declval<T>()) resolves to an object of the result type of
//     a call on a function object of type FunType with an rvalue argument of type T.
//     It is ill-formed if the function object expect an lvalue reference.
//         - Example: FunType is a pointer to a bool(int) and T is int. This expression will result in a bool object.
//         - Example: FunType is a function object modeling bool(int&) and T is int. This expression will be ill-formed because it's illegal to bind an int rvalue to an int&.
// 4.) void_t<decltype(*3*)> checks for the well-formedness of 3., then discards it.
//     If 3. is ill-formed, then this specialization is ignored through SFINAE.
//     If well-formed, then it's considered more specialized than the other and takes precedence.
template<typename FunType, typename T>
struct TryMoveCall<FunType, T, std::void_t<decltype(std::declval<FunType>()(std::declval<T>()))>> {
    template<typename Fun>
    static auto call(Fun&& fun, T& elem) -> decltype(std::forward<Fun>(fun)(std::move(elem))) { return std::forward<Fun>(fun)(std::move(elem)); }
};
} /* namespace utils */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
