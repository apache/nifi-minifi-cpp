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

#include <core/threadpool.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

task_t create_task(function_t function, void * args, void * state) {
  task_t task;
  memset(&task, 0, sizeof(task_t));
  task.function = function;
  task.args = args;
  task.state = state;
  task.start_time_ms = get_now_ms();
  return task;
}

task_node_t * create_repeatable_task(function_t function, void * args,
    void * state, uint64_t interval_ms) {
  task_t task = create_task(function, args, state);
  task.interval_ms = interval_ms;
  task_node_t * task_node = (task_node_t *) malloc(sizeof(task_node_t));
  memset(task_node, 0, sizeof(task_node_t));
  task_node->task = task;
  return task_node;
}

int is_task_repeatable(task_t * task) {
  assert(task);
  return task->interval_ms > 0;
}

uint64_t get_task_repeat_interval(task_t * task) {
  assert(task);
  return task->interval_ms;
}

uint64_t get_expiry_time_millis(task_t * task) {
  assert(task);
  uint64_t now = get_now_ms();
  if (now >= task->start_time_ms + task->interval_ms) {
    return 0;
  }
  return task->start_time_ms + task->interval_ms - now;
}

int in_order_cmp(task_node_t * task1, task_node_t * task2) {
  if (!task1 || !task2) {
    return 0;
  }
  uint64_t t1 = get_expiry_time_millis(&task1->task);
  uint64_t t2 = get_expiry_time_millis(&task2->task);
  if (t1 >= t2) {
    return 1;
  }
  return -1;
}

int threadpool_add(threadpool_t * pool, task_node_t * task) {
  acquire_lock(&pool->task_queue_lock);
  if (pool->shuttingdown) {
    release_lock(&pool->task_queue_lock);
    return -1;
  }
  LL_APPEND(pool->task_queue, task);
  pool->num_tasks++;
  condition_variable_signal(&pool->task_queue_cond);
  release_lock(&pool->task_queue_lock);
  return 0;
}

int threadpool_add_delayed(threadpool_t * pool, task_node_t * task) {
  if (is_timer_expired(&task->task)) {
    threadpool_add(pool, task);
    return 0;
  }

  acquire_lock(&pool->task_queue_lock);
  if (pool->shuttingdown) {
    release_lock(&pool->task_queue_lock);
    return -1;
  }
  LL_INSERT_INORDER(pool->wait_queue, task, in_order_cmp);
  condition_variable_signal(&pool->wait_queue_cond);
  release_lock(&pool->task_queue_lock);
  return 0;
}

uint64_t get_num_tasks(threadpool_t * pool) {
  uint64_t ret = 0;
  acquire_lock(&pool->task_queue_lock);
  ret = pool->num_tasks;
  release_lock(&pool->task_queue_lock);
  return ret;
}

threadpool_t * threadpool_create(uint64_t num_threads) {
  if (num_threads == 0) {
    return NULL;
  }
  threadpool_t * pool = (threadpool_t *) malloc(sizeof(threadpool_t));
  memset(pool, 0, sizeof(threadpool_t));
  pool->num_threads = num_threads;
  initialize_lock(&pool->task_queue_lock);
  initialize_cv(&pool->task_queue_cond, NULL);

#ifndef WIN32
  initialize_cvattr(&pool->wait_queue_cond_attr);
#ifndef __APPLE__
  condition_attr_set_clock(&pool->wait_queue_cond_attr, CLOCK_MONOTONIC);
#endif
  initialize_cv(&pool->wait_queue_cond, &pool->wait_queue_cond_attr);
#else
  initialize_cv(&pool->wait_queue_cond, NULL);
#endif

  pool->shuttingdown = 0;
  pool->num_tasks = 0;
  return pool;
}

task_node_t * get_task(task_node_t ** queue) {
  if (!queue || !*queue) {
    return NULL;
  }

  task_node_t * task = *queue;
  *queue = task->next;
  task->next = NULL;
  return task;
}

int is_timer_expired(task_t * task) {
  if (task->interval_ms == 0) {
    return 1;
  }

  uint64_t now = get_now_ms();
  if ((now - task->start_time_ms) >= task->interval_ms) {
    return 1;
  }
  return 0;
}

int is_task_queue_empty(task_node_t * queue) {
  task_node_t * el;
  size_t counter = 0;
  LL_COUNT(queue, el, counter);
  return counter == 0;
}

#ifndef WIN32
void * process_wait_queue(void * pool) {
#else
  unsigned __stdcall process_wait_queue(PVOID pool) {
#endif
  threadpool_t * thpool = (threadpool_t *) pool;

  for (;;) {
    acquire_lock(&thpool->task_queue_lock);
    if (thpool->shuttingdown) {
      break;
    }
    while (!is_task_queue_empty(thpool->wait_queue)
        && is_timer_expired(&(thpool->wait_queue->task))) {
      task_node_t * el = thpool->wait_queue;
      LL_DELETE(thpool->wait_queue, el);
      LL_APPEND(thpool->task_queue, el);
      thpool->num_tasks++;
    }
    condition_variable_broadcast(&thpool->task_queue_cond);

    if(is_task_queue_empty(thpool->wait_queue)) {
      condition_variable_wait(&thpool->wait_queue_cond, &thpool->task_queue_lock);
    } else {
      uint64_t time_to_expire = 0;
      if (thpool->wait_queue) {
        time_to_expire = get_expiry_time_millis(&(thpool->wait_queue->task));
      }
      condition_variable_timedwait(&thpool->wait_queue_cond, &thpool->task_queue_lock,
          !time_to_expire ? 1 : time_to_expire);
    }
    release_lock(&thpool->task_queue_lock);
  }
  release_lock(&thpool->task_queue_lock);
  return 0;
}

#ifndef WIN32
void * threadpool_thread_function(void * pool) {
#else
  unsigned __stdcall threadpool_thread_function(PVOID pool) {
#endif
  if (!pool) {
    return 0;
  }

  threadpool_t * thpool = (threadpool_t *) pool;
  for (;;) {
    acquire_lock(&thpool->task_queue_lock);

    //while there are no tasks in the queue and the pool
    //is not shutting down wait on the condition variable
    while (!thpool->shuttingdown && !thpool->num_tasks) {
      condition_variable_wait(&thpool->task_queue_cond, &thpool->task_queue_lock);
    }

    if (thpool->shuttingdown) {
      break;
    }

    task_node_t * task = get_task(&thpool->task_queue);

    if (!task) {
      release_lock(&thpool->task_queue_lock);
      continue;
    }

    if (thpool->num_tasks > 0)
      thpool->num_tasks--;
    release_lock(&thpool->task_queue_lock);

    if (is_task_repeatable(&task->task)) {
      task_state_t ret = (*task->task.function)(task->task.args, task->task.state);
      task->task.start_time_ms = get_now_ms();
      if (ret == RUN_AGAIN) {
        threadpool_add_delayed(pool, task);
      } else {
        free(task);
      }
    } else {
      (*(task->task.function))(task->task.args, task->task.state);
      free(task->task.args);
      free(task->task.state);
      free(task);
    }
  }
  release_lock(&thpool->task_queue_lock);
  return 0;
}

int threadpool_start(threadpool_t * pool) {
  if (!pool) {
    return -1;
  }
  if (!pool->started) {
    pool->threads =
        (thread_handle_t *)malloc(sizeof(thread_handle_t) * pool->num_threads);
    int i;
    for (i = 0; i < pool->num_threads; ++i) {
      thread_proc_t proc;
      proc.threadfunc = &threadpool_thread_function;
      if (create_thread(&pool->threads[i], proc, (void *)pool) < 0) {
        threadpool_shutdown(pool);
        return -1;
      }
    }
    pool->wait_queue_thread = (thread_handle_t *)malloc(sizeof(thread_handle_t));
    thread_proc_t proc;
    proc.threadfunc = &process_wait_queue;
    if (create_thread(pool->wait_queue_thread, proc, (void *)pool) < 0) {
      threadpool_shutdown(pool);
      return -1;
    }
    pool->started = 1;
  }
  return 0;
}

void threadpool_shutdown(threadpool_t * pool) {
  if (!pool || !pool->started) {
    return;
  }
  acquire_lock(&(pool->task_queue_lock));
  if (pool->shuttingdown) {
    release_lock(&(pool->task_queue_lock));
    return;
  }
  pool->shuttingdown = 1;
  condition_variable_broadcast(&(pool->task_queue_cond));
  condition_variable_signal(&pool->wait_queue_cond);
  release_lock(&(pool->task_queue_lock));

  int i;
  for (i = 0; i < pool->num_threads; ++i) {
    wait_thread_complete(&pool->threads[i]);
  }
  wait_thread_complete(pool->wait_queue_thread);

  acquire_lock(&pool->task_queue_lock);
  destroy_cv(&pool->task_queue_cond);
  destroy_cvattr(&pool->wait_queue_cond_attr);
  destroy_cv(&pool->wait_queue_cond);
  destroy_lock(&pool->task_queue_lock);
  free(pool->threads);
  free(pool->wait_queue_thread);

  task_node_t * head = pool->task_queue;
  while (head) {
    task_node_t * tmp = head;
    head = head->next;
    free(tmp);
  }

  head = pool->wait_queue;
  while (head) {
    task_node_t * tmp = head;
    head = head->next;
    free(tmp);
  }
}
