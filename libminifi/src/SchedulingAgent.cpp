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
#include <chrono>
#include <thread>
#include <iostream>
#include "Exception.h"
#include "SchedulingAgent.h"

bool SchedulingAgent::hasWorkToDo(Processor *processor) {
  // Whether it has work to do
  if (processor->getTriggerWhenEmpty() || !processor->hasIncomingConnections()
      || processor->flowFilesQueued())
    return true;
  else
    return false;
}

bool SchedulingAgent::hasTooMuchOutGoing(Processor *processor) {
  return processor->flowFilesOutGoingFull();
}

bool SchedulingAgent::onTrigger(Processor *processor,
                                ProcessContext *processContext,
                                ProcessSessionFactory *sessionFactory) {
  if (processor->isYield())
    return false;

  // No need to yield, reset yield expiration to 0
  processor->clearYield();

  if (!hasWorkToDo(processor))
    // No work to do, yield
    return true;

  if (hasTooMuchOutGoing(processor))
    // need to apply backpressure
    return true;

  //TODO runDuration

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
    processor->yield(_administrativeYieldDuration);
    processor->decrementActiveTask();
  } catch (...) {
    logger_->log_debug("Caught Exception during SchedulingAgent::onTrigger");
    processor->yield(_administrativeYieldDuration);
    processor->decrementActiveTask();
  }

  return false;
}
