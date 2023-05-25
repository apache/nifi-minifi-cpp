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

#include <chrono>

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
    if (on_trigger_duration_ > 0ms)
      std::this_thread::sleep_for(on_trigger_duration_);
    ++number_of_triggers;
  }

  size_t getNumberOfTriggers() const { return number_of_triggers; }
  void setOnTriggerDuration(std::chrono::steady_clock::duration on_trigger_duration) { on_trigger_duration_ = on_trigger_duration; }

 private:
  std::chrono::steady_clock::duration on_trigger_duration_ = 0ms;
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
  auto controller_services_provider_ = std::make_shared<minifi::core::controller::StandardControllerServiceProvider>(controller_services_, configuration);
  utils::ThreadPool thread_pool;
  auto count_proc = std::make_shared<CountOnTriggersProcessor>("count_proc");
  count_proc->incrementActiveTasks();
  count_proc->setScheduledState(core::RUNNING);
  auto node = std::make_shared<core::ProcessorNode>(count_proc.get());
  auto context = std::make_shared<core::ProcessContext>(node, nullptr, repo, repo, content_repo);
  std::shared_ptr<core::ProcessSessionFactory> factory = std::make_shared<core::ProcessSessionFactory>(context);
  count_proc->setSchedulingPeriod(125ms);
#ifdef WIN32
  utils::dateSetInstall(TZ_DATA_DIR);
#endif

  SECTION("Timer Driven") {
    auto timer_driven_agent = std::make_shared<TimerDrivenSchedulingAgent>(gsl::make_not_null(controller_services_provider_.get()), test_repo, test_repo, content_repo, configuration, thread_pool);
    timer_driven_agent->start();
    auto first_task_reschedule_info = timer_driven_agent->run(count_proc.get(), context, factory);
    CHECK(!first_task_reschedule_info.isFinished());
    CHECK(first_task_reschedule_info.getNextExecutionTime() <= std::chrono::steady_clock::now() + 125ms);
    CHECK(count_proc->getNumberOfTriggers() == 1);

    count_proc->setOnTriggerDuration(50ms);
    auto second_task_reschedule_info = timer_driven_agent->run(count_proc.get(), context, factory);

    CHECK(!second_task_reschedule_info.isFinished());
    CHECK(first_task_reschedule_info.getNextExecutionTime() <= std::chrono::steady_clock::now() + 75ms);
    CHECK(count_proc->getNumberOfTriggers() == 2);

    count_proc->setOnTriggerDuration(150ms);
    auto third_task_reschedule_info = timer_driven_agent->run(count_proc.get(), context, factory);
    CHECK(!third_task_reschedule_info.isFinished());
    CHECK(first_task_reschedule_info.getNextExecutionTime() < std::chrono::steady_clock::now());
    CHECK(count_proc->getNumberOfTriggers() == 3);
  }

  SECTION("Event Driven") {
    auto event_driven_agent = std::make_shared<EventDrivenSchedulingAgent>(gsl::make_not_null(controller_services_provider_.get()), test_repo, test_repo, content_repo, configuration, thread_pool);
    event_driven_agent->start();
    auto first_task_reschedule_info = event_driven_agent->run(count_proc.get(), context, factory);
    CHECK(!first_task_reschedule_info.isFinished());
    CHECK(first_task_reschedule_info.getNextExecutionTime() < std::chrono::steady_clock::now());
    auto count_num_after_one_schedule = count_proc->getNumberOfTriggers();
    CHECK(count_num_after_one_schedule > 100);

    auto second_task_reschedule_info = event_driven_agent->run(count_proc.get(), context, factory);
    CHECK(!second_task_reschedule_info.isFinished());
    CHECK(second_task_reschedule_info.getNextExecutionTime() < std::chrono::steady_clock::now());
    auto count_num_after_two_schedule = count_proc->getNumberOfTriggers();
    CHECK(count_num_after_two_schedule > count_num_after_one_schedule+100);
  }

  SECTION("Cron Driven every year") {
#ifdef WIN32
    date::set_install(TZ_DATA_DIR);
#endif
    count_proc->setCronPeriod("0 0 0 1 1 ?");
    auto cron_driven_agent = std::make_shared<CronDrivenSchedulingAgent>(gsl::make_not_null(controller_services_provider_.get()), test_repo, test_repo, content_repo, configuration, thread_pool);
    cron_driven_agent->start();
    auto first_task_reschedule_info = cron_driven_agent->run(count_proc.get(), context, factory);
    CHECK(!first_task_reschedule_info.isFinished());
    if (first_task_reschedule_info.getNextExecutionTime() > std::chrono::steady_clock::now() + 1min) {  // To avoid possibly failing around dec 31 23:59:59
      auto wait_time_till_next_execution_time = std::chrono::round<std::chrono::seconds>(first_task_reschedule_info.getNextExecutionTime() - std::chrono::steady_clock::now());

      auto current_time = date::make_zoned<std::chrono::seconds>(date::current_zone(), std::chrono::time_point_cast<std::chrono::seconds>(std::chrono::system_clock::now()));
      auto current_year_month_day = date::year_month_day(date::floor<date::days>(current_time.get_local_time()));
      auto new_years_day = date::make_zoned<std::chrono::seconds>(date::current_zone(), date::local_days{date::year{current_year_month_day.year()+date::years(1)}/date::January/1});

      auto time_until_new_years_day = new_years_day.get_local_time() - current_time.get_local_time();

      CHECK(std::chrono::round<std::chrono::minutes>(time_until_new_years_day - wait_time_till_next_execution_time) == 0min);
      CHECK(count_proc->getNumberOfTriggers() == 0);

      auto second_task_reschedule_info = cron_driven_agent->run(count_proc.get(), context, factory);
      CHECK(!second_task_reschedule_info.isFinished());
      CHECK(std::chrono::round<std::chrono::minutes>(first_task_reschedule_info.getNextExecutionTime() - second_task_reschedule_info.getNextExecutionTime()) == 0min);
      CHECK(count_proc->getNumberOfTriggers() == 0);
    }
  }

  SECTION("Cron Driven every sec") {
    count_proc->setCronPeriod("* * * * * *");
    auto cron_driven_agent = std::make_shared<CronDrivenSchedulingAgent>(gsl::make_not_null(controller_services_provider_.get()), test_repo, test_repo, content_repo, configuration, thread_pool);
    cron_driven_agent->start();
    auto first_task_reschedule_info = cron_driven_agent->run(count_proc.get(), context, factory);
    CHECK(!first_task_reschedule_info.isFinished());
    CHECK(first_task_reschedule_info.getNextExecutionTime() <= std::chrono::steady_clock::now() + 1s);
    CHECK(count_proc->getNumberOfTriggers() == 0);

    std::this_thread::sleep_until(first_task_reschedule_info.getNextExecutionTime());
    auto second_task_reschedule_info = cron_driven_agent->run(count_proc.get(), context, factory);
    CHECK(!second_task_reschedule_info.isFinished());
    CHECK(second_task_reschedule_info.getNextExecutionTime() <= std::chrono::steady_clock::now() + 1s);
    CHECK(count_proc->getNumberOfTriggers() == 1);
  }

  SECTION("Cron Driven no future triggers") {
    count_proc->setCronPeriod("* * * * * * 2012");
    auto cron_driven_agent = std::make_shared<CronDrivenSchedulingAgent>(gsl::make_not_null(controller_services_provider_.get()), test_repo, test_repo, content_repo, configuration, thread_pool);
    cron_driven_agent->start();
    auto first_task_reschedule_info = cron_driven_agent->run(count_proc.get(), context, factory);
    CHECK(first_task_reschedule_info.isFinished());
  }
}
}  // namespace org::apache::nifi::minifi::testing
