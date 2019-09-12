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
#include <assert.h>
#include <core/threadutils.h>

int create_thread(thread_handle_t * hnd, thread_proc_t tproc, void * args) {
	if (!hnd || tproc.threadfunc == NULL) {
		return -1;
	}

#ifndef WIN32
	if (pthread_create(&hnd->thread, NULL, tproc.threadfunc, args) != 0) {
		return -1;
	}
#else
	uintptr_t ret = _beginthreadex(NULL, 0, tproc.threadfunc, args, 0, NULL);
	if (ret == 0) {
		hnd->thread = 0;
		return -1;
	}
	hnd->thread = ret;
#endif
	return 0;
}

void wait_thread_complete(thread_handle_t * hnd) {
	assert(hnd != NULL);
#ifndef WIN32
	pthread_join(hnd->thread, NULL);
#else
	WaitForSingleObject((void *)(&hnd->thread), INFINITE);
#endif
}

void thread_sleep_ms(uint64_t millis) {
#ifndef WIN32
	usleep(millis * 1000L);
#else
	Sleep(millis);
#endif
}
