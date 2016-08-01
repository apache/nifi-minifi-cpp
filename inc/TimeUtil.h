/**
 * @file TimeUtil.h
 * Basic Time Utility 
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
#ifndef __TIME_UTIL_H__
#define __TIME_UTIL_H__

#include <time.h>
#include <sys/time.h>
#include <string.h>
#include <unistd.h>
#include <string.h>
#include <iostream>

#ifdef __MACH__
#include <mach/clock.h>
#include <mach/mach.h>
#endif

inline uint64_t getTimeMillis()
{
	uint64_t value;

	timeval time;
	gettimeofday(&time, NULL);
	value = ((uint64_t) (time.tv_sec) * 1000) + (time.tv_usec / 1000);

	return value;
}

inline uint64_t getTimeNano()
{
	struct timespec ts;
	
#ifdef __MACH__ // OS X does not have clock_gettime, use clock_get_time
	clock_serv_t cclock;
	mach_timespec_t mts;
	host_get_clock_service(mach_host_self(), CALENDAR_CLOCK, &cclock);
	clock_get_time(cclock, &mts);
	mach_port_deallocate(mach_task_self(), cclock);
	ts.tv_sec = mts.tv_sec;
	ts.tv_nsec = mts.tv_nsec;
#else
	clock_gettime(CLOCK_REALTIME, &ts);
#endif

	return ((uint64_t) (ts.tv_sec) * 1000000000 + ts.tv_nsec);
}

//! Convert millisecond since UTC to a time display string
inline std::string getTimeStr(uint64_t msec)
{
	char date[120];
	time_t second = (time_t) (msec/1000);
	msec = msec % 1000;
	strftime(date, sizeof(date) / sizeof(*date), "%Y-%m-%d %H:%M:%S",
	             localtime(&second));

	std::string ret = date;
	date[0] = '\0';
	sprintf(date, ".%03llu", (unsigned long long) msec);

	ret += date;
	return ret;
}

#endif
