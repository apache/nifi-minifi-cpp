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
#ifndef MAIN_MAIN_H_
#define MAIN_MAIN_H_


#ifdef WIN32
#define FILE_SEPARATOR "\\"


extern "C" {
	FILE* __cdecl _imp____iob_func()
	{
		struct _iobuf_VS2012 { // ...\Microsoft Visual Studio 11.0\VC\include\stdio.h #56
			char *_ptr;
			int   _cnt;
			char *_base;
			int   _flag;
			int   _file;
			int   _charbuf;
			int   _bufsiz;
			char *_tmpfname;
		};
		// VS2015 has FILE = struct {void* _Placeholder}

		static struct _iobuf_VS2012 bufs[3];
		static char initialized = 0;

		if (!initialized) {
			bufs[0]._ptr = (char*)stdin->_Placeholder;
			bufs[1]._ptr = (char*)stdout->_Placeholder;
			bufs[2]._ptr = (char*)stderr->_Placeholder;
			initialized = 1;
		}

		return (FILE*)&bufs;
	}
	FILE* __cdecl __imp___iob_func()
	{
		struct _iobuf_VS2012 { // ...\Microsoft Visual Studio 11.0\VC\include\stdio.h #56
			char *_ptr;
			int   _cnt;
			char *_base;
			int   _flag;
			int   _file;
			int   _charbuf;
			int   _bufsiz;
			char *_tmpfname;
	};
		// VS2015 has FILE = struct {void* _Placeholder}

		static struct _iobuf_VS2012 bufs[3];
		static char initialized = 0;

		if (!initialized) {
			bufs[0]._ptr = (char*)stdin->_Placeholder;
			bufs[1]._ptr = (char*)stdout->_Placeholder;
			bufs[2]._ptr = (char*)stderr->_Placeholder;
			initialized = 1;
		}

		return (FILE*)&bufs;
}
}

#else
#ifndef FILE_SEPARATOR
#define FILE_SEPARATOR "/"
#endif
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
 //! Main thread sleep interval 1 second
#define SLEEP_INTERVAL 1
//! Main thread stop wait time
#define STOP_WAIT_TIME_MS 30*1000
//! Default YAML location
#ifdef WIN32
#define DEFAULT_NIFI_CONFIG_YML "\\conf\\config.yml"
//! Default properties file paths
#define DEFAULT_NIFI_PROPERTIES_FILE "\\conf\\minifi.properties"
#define DEFAULT_LOG_PROPERTIES_FILE "\\conf\\minifi-log.properties"
#define DEFAULT_UID_PROPERTIES_FILE "\\conf\\minifi-uid.properties"
#else
#define DEFAULT_NIFI_CONFIG_YML "./conf/config.yml"
//! Default properties file paths
#define DEFAULT_NIFI_PROPERTIES_FILE "./conf/minifi.properties"
#define DEFAULT_LOG_PROPERTIES_FILE "./conf/minifi-log.properties"
#define DEFAULT_UID_PROPERTIES_FILE "./conf/minifi-uid.properties"
#endif
//! Define home environment variable
#define MINIFI_HOME_ENV_KEY "MINIFI_HOME"

/* Define Parser Values for Configuration YAML sections */
#define CONFIG_YAML_PROCESSORS_KEY "Processors"
#define CONFIG_YAML_FLOW_CONTROLLER_KEY "Flow Controller"
#define CONFIG_YAML_CONNECTIONS_KEY "Connections"
#define CONFIG_YAML_REMOTE_PROCESSING_GROUPS_KEY "Remote Processing Groups"


#ifdef _MSC_VER
#ifndef PATH_MAX
#define PATH_MAX 260
#endif
#endif

/**
 * Validates a MINIFI_HOME value.
 * @param home_path
 * @return true if home_path represents a valid MINIFI_HOME
 */
bool validHome(const std::string &home_path) {
  struct stat stat_result { };
  std::string sep;
  sep += FILE_SEPARATOR;
#ifdef WIN32
	sep = "";
#endif
  auto properties_file_path = home_path + sep + DEFAULT_NIFI_PROPERTIES_FILE;
  return (stat(properties_file_path.c_str(), &stat_result) == 0);
}





#endif /* MAIN_MAIN_H_ */
