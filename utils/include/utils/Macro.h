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

#define COMMA(...) ,
#define MSVC_HACK(x) x

#define PICK_(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, ...) _15
#define COUNT(...) \
  MSVC_HACK(PICK_(__VA_ARGS__, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0))

#define CONCAT_(a, b) a ## b
#define CONCAT(a, b) CONCAT_(a, b)

#define CALL(Fn, ...) MSVC_HACK(Fn(__VA_ARGS__))
#define SPREAD(...) __VA_ARGS__

#define FOR_EACH(fn, delim, ARGS) \
  CALL(CONCAT(FOR_EACH_, COUNT ARGS), fn, delim, SPREAD ARGS)

#define FOR_EACH_0(...)
#define FOR_EACH_1(fn, delim, _1) \
  fn(_1)
#define FOR_EACH_2(fn, delim, _1, _2) \
  fn(_1) delim() fn(_2)
#define FOR_EACH_3(fn, delim, _1, _2, _3) \
  fn(_1) delim() FOR_EACH_2(fn, delim, _2, _3)
#define FOR_EACH_4(fn, delim, _1, _2, _3, _4) \
  fn(_1) delim() FOR_EACH_3(fn, delim, _2, _3, _4)
#define FOR_EACH_5(fn, delim, _1, _2, _3, _4, _5) \
  fn(_1) delim() FOR_EACH_4(fn, delim, _2, _3, _4, _5)
#define FOR_EACH_6(fn, delim, _1, _2, _3, _4, _5, _6) \
  fn(_1) delim() FOR_EACH_5(fn, delim, _2, _3, _4, _5, _6)
#define FOR_EACH_7(fn, delim, _1, _2, _3, _4, _5, _6, _7) \
  fn(_1) delim() FOR_EACH_6(fn, delim, _2, _3, _4, _5, _6, _7)
#define FOR_EACH_8(fn, delim, _1, _2, _3, _4, _5, _6, _7, _8) \
  fn(_1) delim() FOR_EACH_7(fn, delim, _2, _3, _4, _5, _6, _7, _8)
#define FOR_EACH_9(fn, delim, _1, _2, _3, _4, _5, _6, _7, _8, _9) \
  fn(_1) delim() FOR_EACH_8(fn, delim, _2, _3, _4, _5, _6, _7, _8, _9)
#define FOR_EACH_10(fn, delim, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10) \
  fn(_1) delim() FOR_EACH_9(fn, delim, _2, _3, _4, _5, _6, _7, _8, _9, _10)
#define FOR_EACH_11(fn, delim, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11) \
  fn(_1) delim() FOR_EACH_10(fn, delim, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11)
#define FOR_EACH_12(fn, delim, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12) \
  fn(_1) delim() FOR_EACH_11(fn, delim, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12)

#define FIRST_(a, b) a
#define FIRST(x, ...) FIRST_ x
#define SECOND_(a, b) b
#define SECOND(x, ...) SECOND_ x
#define NOTHING()
#define IDENTITY(x) x
