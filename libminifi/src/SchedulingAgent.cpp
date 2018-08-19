/**
 * @file SchedulingAgent.cpp
 * SchedulingAgent class implementation
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
#include "SchedulingAgent.h"
#include <chrono>
#include <thread>
#include <utility>
#include <memory>
#include <iostream>
#include "Exception.h"
#include "core/Processor.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {

bool SchedulingAgent::hasWorkToDo(std::shared_ptr<core::Processor> processor) {
  // Whether it has work to do
  if (processor->getTriggerWhenEmpty() || !processor->hasIncomingConnections() || processor->flowFilesQueued())
    return true;
  else
    return false;
}

std::future<uint64_t> SchedulingAgent::enableControllerService(std::shared_ptr<core::controller::ControllerServiceNode> &serviceNode) {
  logger_->log_info("Enabling CSN in SchedulingAgent %s", serviceNode->getName());
  // reference the enable function from serviceNode
  std::function<uint64_t()> f_ex = [serviceNode] {
      serviceNode->enable();
      return 0;
    };

  // only need to run this once.
  std::unique_ptr<SingleRunMonitor> monitor = std::unique_ptr<SingleRunMonitor>(new SingleRunMonitor(&running_));
  utils::Worker<uint64_t> functor(f_ex, serviceNode->getUUIDStr(), std::move(monitor));
  // move the functor into the thread pool. While a future is returned
  // we aren't terribly concerned with the result.
  std::future<uint64_t> future;
  thread_pool_.execute(std::move(functor), future);
  if (future.valid())
    future.wait();
  return future;
}

std::future<uint64_t> SchedulingAgent::disableControllerService(std::shared_ptr<core::controller::ControllerServiceNode> &serviceNode) {
  logger_->log_info("Disabling CSN in SchedulingAgent %s", serviceNode->getName());
  // reference the disable function from serviceNode
  std::function<uint64_t()> f_ex = [serviceNode] {
    serviceNode->disable();
    return 0;
  };

  // only need to run this once.
  std::unique_ptr<SingleRunMonitor> monitor = std::unique_ptr<SingleRunMonitor>(new SingleRunMonitor(&running_));
  utils::Worker<uint64_t> functor(f_ex, serviceNode->getUUIDStr(), std::move(monitor));

  // move the functor into the thread pool. While a future is returned
  // we aren't terribly concerned with the result.
  std::future<uint64_t> future;
  thread_pool_.execute(std::move(functor), future);
  if (future.valid())
    future.wait();
  return future;
}

bool SchedulingAgent::hasTooMuchOutGoing(std::shared_ptr<core::Processor> processor) {
  return processor->flowFilesOutGoingFull();
}

bool SchedulingAgent::onTrigger(const std::shared_ptr<core::Processor> &processor, const std::shared_ptr<core::ProcessContext> &processContext,
                                const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) {
  if (processor->isYield()) {
    logger_->log_debug("Not running %s since it must yield", processor->getName());
    return false;
  }

  // No need to yield, reset yield expiration to 0
  processor->clearYield();

  if (!hasWorkToDo(processor)) {
    // No work to do, yield
    return true;
  }
  if (hasTooMuchOutGoing(processor)) {
    logger_->log_debug("backpressure applied because too much outgoing for %s", processor->getUUIDStr());
    // need to apply backpressure
    return true;
  }

  processor->incrementActiveTasks();
  try {
    processor->onTrigger(processContext, sessionFactory);
    processor->decrementActiveTask();
  } catch (Exception &exception) {
    // Normal exception
    logger_->log_debug("Caught Exception %s", exception.what());
    processor->decrementActiveTask();
  } catch (std::exception &exception) {
    logger_->log_debug("Caught Exception %s", exception.what());
    processor->yield(admin_yield_duration_);
    processor->decrementActiveTask();
  } catch (...) {
    logger_->log_debug("Caught Exception during SchedulingAgent::onTrigger");
    processor->yield(admin_yield_duration_);
    processor->decrementActiveTask();
  }

  return false;
}

} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
