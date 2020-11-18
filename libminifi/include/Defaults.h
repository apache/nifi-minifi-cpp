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

#ifdef WIN32
#define DEFAULT_NIFI_CONFIG_YML "\\conf\\config.yml"
#define DEFAULT_NIFI_PROPERTIES_FILE "\\conf\\minifi.properties"
#define DEFAULT_LOG_PROPERTIES_FILE "\\conf\\minifi-log.properties"
#define DEFAULT_UID_PROPERTIES_FILE "\\conf\\minifi-uid.properties"
#define DEFAULT_BOOTSTRAP_FILE "\\conf\\bootstrap.conf"
#else
#define DEFAULT_NIFI_CONFIG_YML "./conf/config.yml"
#define DEFAULT_NIFI_PROPERTIES_FILE "./conf/minifi.properties"
#define DEFAULT_LOG_PROPERTIES_FILE "./conf/minifi-log.properties"
#define DEFAULT_UID_PROPERTIES_FILE "./conf/minifi-uid.properties"
#define DEFAULT_BOOTSTRAP_FILE "./conf/bootstrap.conf"
#endif
