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

std::future<bool> SchedulingAgent::enableControllerService(std::shared_ptr<core::controller::ControllerServiceNode> &serviceNode) {
  logger_->log_info("Enabling CSN in SchedulingAgent %s", serviceNode->getName());
  // reference the enable function from serviceNode
  std::function< bool()> f_ex = [serviceNode] {
    return serviceNode->enable();
  };
  // create a functor that will be submitted to the thread pool.
  utils::Worker<bool> functor(f_ex, serviceNode->getUUIDStr());
  // move the functor into the thread pool. While a future is returned
  // we aren't terribly concerned with the result.
  std::future<bool> future;
  component_lifecycle_thread_pool_.execute(std::move(functor), future);
  future.wait();
  return future;
}

std::future<bool> SchedulingAgent::disableControllerService(std::shared_ptr<core::controller::ControllerServiceNode> &serviceNode) {
  logger_->log_info("Disabling CSN in SchedulingAgent %s", serviceNode->getName());
  // reference the disable function from serviceNode
  std::function< bool()> f_ex = [serviceNode] {
    return serviceNode->disable();
  };
  // create a functor that will be submitted to the thread pool.
  utils::Worker<bool> functor(f_ex, serviceNode->getUUIDStr());
  // move the functor into the thread pool. While a future is returned
  // we aren't terribly concerned with the result.
  std::future<bool> future;
  component_lifecycle_thread_pool_.execute(std::move(functor), future);
  future.wait();
  return future;
}

bool SchedulingAgent::hasTooMuchOutGoing(std::shared_ptr<core::Processor> processor) {
  return processor->flowFilesOutGoingFull();
}

bool SchedulingAgent::onTrigger(std::shared_ptr<core::Processor> processor, std::shared_ptr<core::ProcessContext> processContext, std::shared_ptr<core::ProcessSessionFactory> sessionFactory) {
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
    logger_->log_debug("backpressure applied because too much outgoing");
    // need to apply backpressure
    return true;
  }

  processor->incrementActiveTasks();
  try {
    logger_->log_debug("Triggering %s", processor->getName());
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
