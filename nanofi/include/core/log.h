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

#ifndef NIFI_MINIFI_CPP_LOG_H
#define NIFI_MINIFI_CPP_LOG_H

#include <stdio.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
  off = 0,
  critical = 1,
  err = 2,
  warn = 3,
  info = 4,
  debug = 5,
  trace = 6,
} log_level;

extern volatile int global_log_level;

static const char *log_level_str[trace+1] = { "OFF", "CRITICAL", "ERROR", "WARN", "INFO", "DEBUG", "TRACE" };

#if __STDC_VERSION__ >= 199901L //C99 compiler support for __func__
#if defined(__GNUC__)
#define logc(level, format, ...) \
  if (level <= global_log_level && level > off) do { \
    fprintf(stderr, "%s:%u: [%s] %s: " format "\n", __FILE__, __LINE__, __func__, log_level_str[level], ##__VA_ARGS__); \
  } while (0)
#else  // no __GNUC__
#define logc(level, format, ...) \
  if (level <= global_log_level && level > off) do { \
    fprintf(stderr, "%s:%u: [%s] %s: " format "\n", __FILE__, __LINE__, __func__, log_level_str[level],  __VA_ARGS__); \
  } while (0)
#endif //__GNUC__
#else // no C99
#define logc(level, ...) \
  if (level <= global_log_level && level > off) do { \
    fprintf(stderr, "%s:%d: %s:", __FILE__, __LINE__, log_level_str[level]); \
            fprintf(stderr, __VA_ARGS__); \
            fprintf(stderr, "\n"); \
  } while (0)
#endif //C99 compiler support

static void set_log_level(log_level lvl) {
  if(lvl >= off && lvl <= trace) {
    global_log_level = lvl;
    logc(info, "Log level was set to %s", log_level_str[lvl]);
  }
}

#ifdef __cplusplus
}
#endif

#endif //NIFI_MINIFI_CPP_LOG_H
