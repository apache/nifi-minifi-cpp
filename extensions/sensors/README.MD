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

Sensors is intended to be an extension whereby we retrieve sensor data through various libraries within MiNiFi C++ Processors.

Currently RTIMULib is used to gather environmental and movement based data and place them into flow files. As this package progresses, more 
libraries may be used to gather sensor data. In the current approach a single FlowFile is created when OnTrigger(..) is called. This may change 
at any time. Future updates should allow for decorating FlowFiles with attributes or referencing parent FlowFiles if necessary.