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

#include "unit/Catch.h"
#include "unit/TestBase.h"
#include "unit/ProvenanceTestHelper.h"
#include "unit/TestUtils.h"
#include "utils/TimeUtil.h"
#include "core/ProcessorNode.h"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::testing {

using minifi::core::controller::StandardControllerServiceProvider;

class CountOnTriggersProcessor : public minifi::core::ProcessorImpl {
 public:
  using minifi::core::ProcessorImpl::ProcessorImpl;

  static constexpr bool SupportsDynamicProperties = false;
  static constexpr bool SupportsDynamicRelationships = false;
  static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_ALLOWED;
  static constexpr bool IsSingleThreaded = false;
  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void onTrigger(core::ProcessContext& context, core::ProcessSession&) override {
    if (on_trigger_duration_ > 0ms)
      std::this_thread::sleep_for(on_trigger_duration_);
    ++number_of_triggers;
    if (should_yield_)
      context.yield();
  }

  size_t getNumberOfTriggers() const { return number_of_triggers; }
  void setOnTriggerDuration(std::chrono::steady_clock::duration on_trigger_duration) { on_trigger_duration_ = on_trigger_duration; }
  void setShouldYield(bool should_yield) { should_yield_ = should_yield; }

 private:
  bool should_yield_ = false;
  std::chrono::steady_clock::duration on_trigger_duration_ = 0ms;
  std::atomic<size_t> number_of_triggers = 0;
};

#ifdef __GNUC__
// array-bounds warnings in GCC produce a lot of false positives: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=56456
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"
#endif
class SchedulingAgentTestFixture {
 public:
  SchedulingAgentTestFixture() {
    count_proc_->incrementActiveTasks();
    count_proc_->setScheduledState(core::RUNNING);

#ifdef WIN32
    minifi::utils::timeutils::dateSetInstall(TZ_DATA_DIR);
    date::set_install(TZ_DATA_DIR);
#endif
  }

 protected:
  std::shared_ptr<core::Repository> test_repo_ = std::make_shared<TestThreadedRepository>();
  std::shared_ptr<core::ContentRepository> content_repo_ = std::make_shared<core::repository::VolatileContentRepository>();
  std::shared_ptr<minifi::FlowController> controller_ = std::make_shared<TestFlowController>(test_repo_, test_repo_, content_repo_);

  TestController test_controller_;
  std::shared_ptr<TestPlan> test_plan = test_controller_.createPlan();
  std::shared_ptr<minifi::Configure> configuration_ = std::make_shared<minifi::ConfigureImpl>();
  std::shared_ptr<StandardControllerServiceProvider> controller_services_provider_ = std::make_shared<StandardControllerServiceProvider>(
      std::make_unique<minifi::core::controller::ControllerServiceNodeMap>(), configuration_);
  utils::ThreadPool thread_pool_;

  std::shared_ptr<CountOnTriggersProcessor> count_proc_ = std::make_shared<CountOnTriggersProcessor>("count_proc");
  std::shared_ptr<core::ProcessorNode> node_ = std::make_shared<core::ProcessorNodeImpl>(count_proc_.get());
  std::shared_ptr<core::ProcessContext> context_ = std::make_shared<core::ProcessContextImpl>(node_, nullptr, test_repo_, test_repo_, content_repo_);
  std::shared_ptr<core::ProcessSessionFactory> factory_ = std::make_shared<core::ProcessSessionFactoryImpl>(context_);
};
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif


TEST_CASE_METHOD(SchedulingAgentTestFixture, "TimerDrivenSchedulingAgent") {
  count_proc_->setSchedulingPeriod(125ms);
  auto timer_driven_agent = std::make_shared<TimerDrivenSchedulingAgent>(gsl::make_not_null(controller_services_provider_.get()), test_repo_, test_repo_, content_repo_, configuration_, thread_pool_);
  timer_driven_agent->start();
  auto first_task_reschedule_info = timer_driven_agent->run(count_proc_.get(), context_, factory_);
  CHECK(!first_task_reschedule_info.isFinished());
  CHECK(first_task_reschedule_info.getNextExecutionTime() <= std::chrono::steady_clock::now() + 125ms);
  CHECK(count_proc_->getNumberOfTriggers() == 1);

  count_proc_->setOnTriggerDuration(50ms);
  auto second_task_reschedule_info = timer_driven_agent->run(count_proc_.get(), context_, factory_);

  CHECK(!second_task_reschedule_info.isFinished());
  CHECK(first_task_reschedule_info.getNextExecutionTime() <= std::chrono::steady_clock::now() + 75ms);
  CHECK(count_proc_->getNumberOfTriggers() == 2);

  count_proc_->setOnTriggerDuration(150ms);
  auto third_task_reschedule_info = timer_driven_agent->run(count_proc_.get(), context_, factory_);
  CHECK(!third_task_reschedule_info.isFinished());
  CHECK(first_task_reschedule_info.getNextExecutionTime() < std::chrono::steady_clock::now());
  CHECK(count_proc_->getNumberOfTriggers() == 3);
}

TEST_CASE_METHOD(SchedulingAgentTestFixture, "EventDrivenSchedulingAgent") {
  auto event_driven_agent = std::make_shared<EventDrivenSchedulingAgent>(gsl::make_not_null(controller_services_provider_.get()), test_repo_, test_repo_, content_repo_, configuration_, thread_pool_);
  event_driven_agent->start();
  auto first_task_reschedule_info = event_driven_agent->run(count_proc_.get(), context_, factory_);
  CHECK(!first_task_reschedule_info.isFinished());
  CHECK(first_task_reschedule_info.getNextExecutionTime() < std::chrono::steady_clock::now());
  auto count_num_after_one_schedule = count_proc_->getNumberOfTriggers();
  CHECK(count_num_after_one_schedule > 100);

  auto second_task_reschedule_info = event_driven_agent->run(count_proc_.get(), context_, factory_);
  CHECK(!second_task_reschedule_info.isFinished());
  CHECK(second_task_reschedule_info.getNextExecutionTime() < std::chrono::steady_clock::now());
  auto count_num_after_two_schedule = count_proc_->getNumberOfTriggers();
  CHECK(count_num_after_two_schedule > count_num_after_one_schedule+100);
}

TEST_CASE_METHOD(SchedulingAgentTestFixture, "Cron Driven every year") {
  count_proc_->setCronPeriod("0 0 0 1 1 ?");
  auto cron_driven_agent = std::make_shared<CronDrivenSchedulingAgent>(gsl::make_not_null(controller_services_provider_.get()), test_repo_, test_repo_, content_repo_, configuration_, thread_pool_);
  cron_driven_agent->start();
  auto first_task_reschedule_info = cron_driven_agent->run(count_proc_.get(), context_, factory_);
  CHECK(!first_task_reschedule_info.isFinished());
  if (first_task_reschedule_info.getNextExecutionTime() > std::chrono::steady_clock::now() + 1min) {  // To avoid possibly failing around dec 31 23:59:59
    auto wait_time_till_next_execution_time = std::chrono::round<std::chrono::seconds>(first_task_reschedule_info.getNextExecutionTime() - std::chrono::steady_clock::now());

    auto current_time = date::make_zoned<std::chrono::seconds>(date::current_zone(), std::chrono::time_point_cast<std::chrono::seconds>(std::chrono::system_clock::now()));
    auto current_year_month_day = date::year_month_day(date::floor<date::days>(current_time.get_local_time()));
    auto new_years_day = date::make_zoned<std::chrono::seconds>(date::current_zone(), date::local_days{date::year{current_year_month_day.year()+date::years(1)}/date::January/1});

    auto time_until_new_years_day = new_years_day.get_local_time() - current_time.get_local_time();

    CHECK(std::chrono::abs(time_until_new_years_day - wait_time_till_next_execution_time) < 1min);
    CHECK(count_proc_->getNumberOfTriggers() == 0);

    auto second_task_reschedule_info = cron_driven_agent->run(count_proc_.get(), context_, factory_);
    CHECK(!second_task_reschedule_info.isFinished());
    CHECK(std::chrono::abs(first_task_reschedule_info.getNextExecutionTime() - second_task_reschedule_info.getNextExecutionTime()) < 1min);

    CHECK(count_proc_->getNumberOfTriggers() == 0);
  }
}

TEST_CASE_METHOD(SchedulingAgentTestFixture, "Cron Driven every sec") {
  count_proc_->setCronPeriod("* * * * * *");
  auto cron_driven_agent = std::make_shared<CronDrivenSchedulingAgent>(gsl::make_not_null(controller_services_provider_.get()), test_repo_, test_repo_, content_repo_, configuration_, thread_pool_);
  cron_driven_agent->start();
  auto first_task_reschedule_info = cron_driven_agent->run(count_proc_.get(), context_, factory_);
  CHECK(!first_task_reschedule_info.isFinished());
  CHECK(first_task_reschedule_info.getNextExecutionTime() <= std::chrono::steady_clock::now() + 1s);
  CHECK(count_proc_->getNumberOfTriggers() == 0);

  std::this_thread::sleep_until(first_task_reschedule_info.getNextExecutionTime());
  auto second_task_reschedule_info = cron_driven_agent->run(count_proc_.get(), context_, factory_);
  CHECK(!second_task_reschedule_info.isFinished());
  CHECK(second_task_reschedule_info.getNextExecutionTime() <= std::chrono::steady_clock::now() + 1s);
  CHECK(count_proc_->getNumberOfTriggers() == 1);
}

TEST_CASE_METHOD(SchedulingAgentTestFixture, "Cron Driven no future triggers") {
  count_proc_->setCronPeriod("* * * * * * 2012");
  auto cron_driven_agent = std::make_shared<CronDrivenSchedulingAgent>(gsl::make_not_null(controller_services_provider_.get()), test_repo_, test_repo_, content_repo_, configuration_, thread_pool_);
  cron_driven_agent->start();
  auto first_task_reschedule_info = cron_driven_agent->run(count_proc_.get(), context_, factory_);
  CHECK(first_task_reschedule_info.isFinished());
}

TEST_CASE_METHOD(SchedulingAgentTestFixture, "Timer driven should respect both yield and run schedule") {
  SECTION("Fast yield slow schedule") {
    count_proc_->setSchedulingPeriod(1min);
    count_proc_->setYieldPeriodMsec(10ms);
  }
  SECTION("Slow yield fast schedule") {
    count_proc_->setSchedulingPeriod(10ms);
    count_proc_->setYieldPeriodMsec(1min);
  }
  count_proc_->setShouldYield(true);
  auto timer_driven_agent = std::make_shared<TimerDrivenSchedulingAgent>(gsl::make_not_null(controller_services_provider_.get()), test_repo_, test_repo_, content_repo_, configuration_, thread_pool_);
  timer_driven_agent->start();
  auto first_task_reschedule_info = timer_driven_agent->run(count_proc_.get(), context_, factory_);
  CHECK(!first_task_reschedule_info.isFinished());
  CHECK(first_task_reschedule_info.getNextExecutionTime() > std::chrono::steady_clock::now() + 100ms);
  CHECK(count_proc_->getNumberOfTriggers() == 1);
}

}  // namespace org::apache::nifi::minifi::testing
