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

#include "../Catch.h"
#include "../TestBase.h"
#include "ProvenanceTestHelper.h"
#include "utils/TestUtils.h"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::testing {

class CountOnTriggersProcessor : public minifi::core::Processor {
 public:
  using minifi::core::Processor::Processor;

  static constexpr bool SupportsDynamicProperties = false;
  static constexpr bool SupportsDynamicRelationships = false;
  static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_ALLOWED;
  static constexpr bool IsSingleThreaded = false;
  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void onTrigger(core::ProcessContext*, core::ProcessSession*) override {
    ++number_of_triggers;
  }

  size_t getNumberOfTriggers() const { return number_of_triggers; }

 private:
  std::atomic<size_t> number_of_triggers = 0;
};


TEST_CASE("SchedulingAgentTests", "[SchedulingAgent]") {
  std::shared_ptr<core::Repository> test_repo = std::make_shared<TestThreadedRepository>();
  std::shared_ptr<core::ContentRepository> content_repo = std::make_shared<core::repository::VolatileContentRepository>();
  auto repo = std::static_pointer_cast<TestThreadedRepository>(test_repo);
  std::shared_ptr<minifi::FlowController> controller =
      std::make_shared<TestFlowController>(test_repo, test_repo, content_repo);

  TestController testController;
  auto test_plan = testController.createPlan();
  auto controller_services_ = std::make_shared<minifi::core::controller::ControllerServiceMap>();
  auto configuration = std::make_shared<minifi::Configure>();
  auto controller_services_provider_ = std::make_shared<minifi::core::controller::StandardControllerServiceProvider>(controller_services_, nullptr, configuration);
  utils::ThreadPool<utils::TaskRescheduleInfo> thread_pool;
  auto count_proc = std::make_shared<CountOnTriggersProcessor>("count_proc");
  count_proc->incrementActiveTasks();
  count_proc->setScheduledState(core::RUNNING);
  auto node = std::make_shared<core::ProcessorNode>(count_proc.get());
  auto context = std::make_shared<core::ProcessContext>(node, nullptr, repo, repo, content_repo);
  std::shared_ptr<core::ProcessSessionFactory> factory = std::make_shared<core::ProcessSessionFactory>(context);
  count_proc->setSchedulingPeriodNano(1250ms);
#ifdef WIN32
  utils::dateSetInstall(TZ_DATA_DIR);
#endif

  SECTION("Timer Driven") {
    auto timer_driven_agent = std::make_shared<TimerDrivenSchedulingAgent>(gsl::make_not_null(controller_services_provider_.get()), test_repo, test_repo, content_repo, configuration, thread_pool);
    timer_driven_agent->start();
    auto first_task_reschedule_info = timer_driven_agent->run(count_proc.get(), context, factory);
    CHECK(!first_task_reschedule_info.finished_);
    CHECK(first_task_reschedule_info.wait_time_ == 1250ms);
    CHECK(count_proc->getNumberOfTriggers() == 1);

    auto second_task_reschedule_info = timer_driven_agent->run(count_proc.get(), context, factory);

    CHECK(!second_task_reschedule_info.finished_);
    CHECK(second_task_reschedule_info.wait_time_ == 1250ms);
    CHECK(count_proc->getNumberOfTriggers() == 2);
  }

  SECTION("Event Driven") {
    auto event_driven_agent = std::make_shared<EventDrivenSchedulingAgent>(gsl::make_not_null(controller_services_provider_.get()), test_repo, test_repo, content_repo, configuration, thread_pool);
    event_driven_agent->start();
    auto first_task_reschedule_info = event_driven_agent->run(count_proc.get(), context, factory);
    CHECK(!first_task_reschedule_info.finished_);
    CHECK(first_task_reschedule_info.wait_time_ == 0ms);
    auto count_num_after_one_schedule = count_proc->getNumberOfTriggers();
    CHECK(count_num_after_one_schedule > 100);

    auto second_task_reschedule_info = event_driven_agent->run(count_proc.get(), context, factory);
    CHECK(!second_task_reschedule_info.finished_);
    CHECK(second_task_reschedule_info.wait_time_ == 0ms);
    auto count_num_after_two_schedule = count_proc->getNumberOfTriggers();
    CHECK(count_num_after_two_schedule > count_num_after_one_schedule+100);
  }

  SECTION("Cron Driven every year") {
    count_proc->setCronPeriod("0 0 0 1 1 ?");
    auto cron_driven_agent = std::make_shared<CronDrivenSchedulingAgent>(gsl::make_not_null(controller_services_provider_.get()), test_repo, test_repo, content_repo, configuration, thread_pool);
    cron_driven_agent->start();
    auto first_task_reschedule_info = cron_driven_agent->run(count_proc.get(), context, factory);
    CHECK(!first_task_reschedule_info.finished_);
    if (first_task_reschedule_info.wait_time_ > 1min) {  // To avoid possibly failing around dec 31 23:59:59
      auto next_run_time_point = std::chrono::round<std::chrono::years>(std::chrono::system_clock::now() + first_task_reschedule_info.wait_time_);
      CHECK(next_run_time_point == std::chrono::ceil<std::chrono::years>(std::chrono::system_clock::now()));
      CHECK(count_proc->getNumberOfTriggers() == 0);

      auto second_task_reschedule_info = cron_driven_agent->run(count_proc.get(), context, factory);
      CHECK(!second_task_reschedule_info.finished_);
      next_run_time_point = std::chrono::round<std::chrono::years>(std::chrono::system_clock::now() + first_task_reschedule_info.wait_time_);
      CHECK(next_run_time_point == std::chrono::ceil<std::chrono::years>(std::chrono::system_clock::now()));
      CHECK(count_proc->getNumberOfTriggers() == 0);
    }
  }

  SECTION("Cron Driven every sec") {
    count_proc->setCronPeriod("* * * * * *");
    auto cron_driven_agent = std::make_shared<CronDrivenSchedulingAgent>(gsl::make_not_null(controller_services_provider_.get()), test_repo, test_repo, content_repo, configuration, thread_pool);
    cron_driven_agent->start();
    auto first_task_reschedule_info = cron_driven_agent->run(count_proc.get(), context, factory);
    CHECK(!first_task_reschedule_info.finished_);
    CHECK(first_task_reschedule_info.wait_time_ <= 1s);
    CHECK(count_proc->getNumberOfTriggers() == 0);

    std::this_thread::sleep_for(first_task_reschedule_info.wait_time_ + 1ms);
    auto second_task_reschedule_info = cron_driven_agent->run(count_proc.get(), context, factory);
    CHECK(!second_task_reschedule_info.finished_);
    CHECK(second_task_reschedule_info.wait_time_ <= 1s);
    CHECK(count_proc->getNumberOfTriggers() == 1);
  }

  SECTION("Cron Driven no future triggers") {
    count_proc->setCronPeriod("* * * * * * 2012");
    auto cron_driven_agent = std::make_shared<CronDrivenSchedulingAgent>(gsl::make_not_null(controller_services_provider_.get()), test_repo, test_repo, content_repo, configuration, thread_pool);
    cron_driven_agent->start();
    auto first_task_reschedule_info = cron_driven_agent->run(count_proc.get(), context, factory);
    CHECK(first_task_reschedule_info.finished_);
  }
}
}  // namespace org::apache::nifi::minifi::testing
