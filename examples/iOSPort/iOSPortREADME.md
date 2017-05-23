<!--
  Licensed to the Apache Software Foundation (ASF) under one or more
  contributor license agreements.  See the NOTICE file distributed with
  this work for additional information regarding copyright ownership.
  The ASF licenses this file to You under the Apache License, Version 2.0
  (the "License"); you may not use this file except in compliance with
  the License.  You may obtain a copy of the License at
      http://www.apache.org/licenses/LICENSE-2.0
  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
-->

1) Install XCode
2) Build libminfi.a
cd libminifi, mkdir lib, cd lib
cmake -DCMAKE_TOOLCHAIN_FILE=../../cmake/iOS.cmake -DIOS_PLATFORM=SIMULATOR64 ..
make
after that it will create libminifi.a for Xcode Simulator
3) Create a XCode iphone simulator project, in build stage add link library for libminfi.a
4) In IOS project file, you can call MiNiFi APIs list in libminifi/include  
