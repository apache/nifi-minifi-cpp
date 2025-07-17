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

#include <minifi-cpp/core/FlowFile.h>

#include "aws/kinesis/model/PutRecordsRequest.h"
#include "core/Resource.h"
#include "processors/PutKinesisStream.h"
#include "unit/Catch.h"
#include "unit/SingleProcessorTestController.h"
#include "unit/TestBase.h"
#include "unit/ProcessorUtils.h"

namespace org::apache::nifi::minifi::aws::processors::test {

class MockKinesisClient final : public Aws::Kinesis::KinesisClient {
 public:
  enum class KinesisBehaviour {
    HappyPath,
    Failure,
    RecordSizeMismatch,
    OddsFail,
  };

  Aws::Kinesis::Model::PutRecordsOutcome PutRecords(const Aws::Kinesis::Model::PutRecordsRequest& request) const override {
    switch (behaviour_) {
      case KinesisBehaviour::HappyPath: {
        Aws::Kinesis::Model::PutRecordsResult result;
        for ([[maybe_unused]] const auto& request_entry : request.GetRecords()) {
          Aws::Kinesis::Model::PutRecordsResultEntry result_entry;
          result_entry.SetSequenceNumber(fmt::format("sequence_number_{}", ++sequence_number_));
          result_entry.SetShardId("shard_id");
          result.AddRecords(result_entry);
        }
        return result;
      }
      case KinesisBehaviour::Failure: {
        auto err = Aws::Kinesis::KinesisError();
        err.SetResponseCode(Aws::Http::HttpResponseCode::UNAUTHORIZED);
        err.SetMessage("Unauthorized");
        return err;
      }
      case KinesisBehaviour::RecordSizeMismatch: {
        Aws::Kinesis::Model::PutRecordsResult result;
        return result;
      }
      case KinesisBehaviour::OddsFail: {
        Aws::Kinesis::Model::PutRecordsResult result;
        uint8_t i = 0;
        for ([[maybe_unused]] const auto& request_entry : request.GetRecords()) {
          Aws::Kinesis::Model::PutRecordsResultEntry result_entry;
          if (++i%2 == 0) {
            result_entry.SetErrorCode("8");
            result_entry.SetErrorMessage("Some error message");
          } else {
            result_entry.SetSequenceNumber(fmt::format("sequence_number_{}", ++sequence_number_));
            result_entry.SetShardId("shard_id");
          }
          result.AddRecords(result_entry);
        }
        return result;
      }
      default: { throw std::invalid_argument("Unknown behaviour"); }
    }
  }

  KinesisBehaviour behaviour_ = KinesisBehaviour::HappyPath;
  mutable uint32_t sequence_number_ = 0;
};

class PutKinesisStreamMocked final : public aws::processors::PutKinesisStream {  // NOLINT(cppcoreguidelines-special-member-functions)
 public:
  static constexpr const char* Description = "PutKinesisStreamMocked";

  using PutKinesisStream::PutKinesisStream;

  ~PutKinesisStreamMocked() override = default;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  std::unique_ptr<Aws::Kinesis::KinesisClient> getClient(const Aws::Auth::AWSCredentials&) override {
    auto client = std::make_unique<MockKinesisClient>();
    client->behaviour_ = behaviour_;
    return client;
  }

  MockKinesisClient::KinesisBehaviour behaviour_ = MockKinesisClient::KinesisBehaviour::HappyPath;
};
REGISTER_RESOURCE(PutKinesisStreamMocked, Processor);

TEST_CASE("PutKinesisStream record size mismatch path") {
  minifi::test::SingleProcessorTestController controller(minifi::test::utils::make_processor<PutKinesisStreamMocked>("PutKinesisStream"));
  auto put_kinesis_stream = controller.getProcessor<PutKinesisStreamMocked>();
  REQUIRE(put_kinesis_stream);
  put_kinesis_stream.get().behaviour_ = MockKinesisClient::KinesisBehaviour::RecordSizeMismatch;

  controller.plan->setProperty(put_kinesis_stream, PutKinesisStream::AccessKey, "access_key");
  controller.plan->setProperty(put_kinesis_stream, PutKinesisStream::SecretKey, "secret_key");
  controller.plan->setProperty(put_kinesis_stream, PutKinesisStream::AmazonKinesisStreamName, "stream_name");


  const auto result = controller.trigger({{.content = "foo"}, {.content = "bar"}});
  CHECK(result.at(PutKinesisStream::Success).empty());
  REQUIRE(result.at(PutKinesisStream::Failure).size() == 2);
  const auto res_ff_1 = result.at(PutKinesisStream::Failure).at(0);
  const auto res_ff_2 = result.at(PutKinesisStream::Failure).at(1);

  CHECK(controller.plan->getContent(res_ff_1) == "foo");
  CHECK(controller.plan->getContent(res_ff_2) == "bar");

  CHECK(res_ff_1->getAttribute(PutKinesisStream::AwsKinesisErrorMessage.name) == "Record size mismatch");
  CHECK(res_ff_2->getAttribute(PutKinesisStream::AwsKinesisErrorMessage.name) == "Record size mismatch");
}

TEST_CASE("PutKinesisStream record size failure path") {
  minifi::test::SingleProcessorTestController controller(minifi::test::utils::make_processor<PutKinesisStreamMocked>("PutKinesisStream"));
  auto put_kinesis_stream = controller.getProcessor<PutKinesisStreamMocked>();
  REQUIRE(put_kinesis_stream);
  put_kinesis_stream.get().behaviour_ = MockKinesisClient::KinesisBehaviour::Failure;

  controller.plan->setProperty(put_kinesis_stream, PutKinesisStream::AccessKey, "access_key");
  controller.plan->setProperty(put_kinesis_stream, PutKinesisStream::SecretKey, "secret_key");
  controller.plan->setProperty(put_kinesis_stream, PutKinesisStream::AmazonKinesisStreamName, "stream_name");


  const auto result = controller.trigger({{.content = "foo"}, {.content = "bar"}});
  CHECK(result.at(PutKinesisStream::Success).empty());
  REQUIRE(result.at(PutKinesisStream::Failure).size() == 2);
  const auto res_ff_1 = result.at(PutKinesisStream::Failure).at(0);
  const auto res_ff_2 = result.at(PutKinesisStream::Failure).at(1);

  CHECK(controller.plan->getContent(res_ff_1) == "foo");
  CHECK(controller.plan->getContent(res_ff_2) == "bar");

  CHECK(res_ff_1->getAttribute(PutKinesisStream::AwsKinesisErrorMessage.name) == "Unauthorized");
  CHECK(res_ff_1->getAttribute(PutKinesisStream::AwsKinesisErrorCode.name) == "401");
  CHECK(res_ff_2->getAttribute(PutKinesisStream::AwsKinesisErrorCode.name) == "401");
  CHECK(res_ff_2->getAttribute(PutKinesisStream::AwsKinesisErrorMessage.name) == "Unauthorized");
}

TEST_CASE("PutKinesisStream partial failure path") {
  minifi::test::SingleProcessorTestController controller(minifi::test::utils::make_processor<PutKinesisStreamMocked>("PutKinesisStream"));
  auto put_kinesis_stream = controller.getProcessor<PutKinesisStreamMocked>();
  put_kinesis_stream.get().behaviour_ = MockKinesisClient::KinesisBehaviour::OddsFail;

  controller.plan->setProperty(put_kinesis_stream, PutKinesisStream::AccessKey, "access_key");
  controller.plan->setProperty(put_kinesis_stream, PutKinesisStream::SecretKey, "secret_key");
  controller.plan->setProperty(put_kinesis_stream, PutKinesisStream::AmazonKinesisStreamName, "stream_name");


  const auto result = controller.trigger({{.content = "foo"}, {.content = "bar"}});
  REQUIRE(result.at(PutKinesisStream::Success).size() == 1);
  REQUIRE(result.at(PutKinesisStream::Failure).size() == 1);
  const auto succ_ff = result.at(PutKinesisStream::Success).at(0);
  const auto fail_ff = result.at(PutKinesisStream::Failure).at(0);

  CHECK(controller.plan->getContent(succ_ff) == "foo");
  CHECK(controller.plan->getContent(fail_ff) == "bar");

  CHECK(succ_ff->getAttribute(PutKinesisStream::AwsKinesisSequenceNumber.name) == "sequence_number_1");
  CHECK(succ_ff->getAttribute(PutKinesisStream::AwsKinesisShardId.name) == "shard_id");
  CHECK(fail_ff->getAttribute(PutKinesisStream::AwsKinesisErrorCode.name) == "8");
  CHECK(fail_ff->getAttribute(PutKinesisStream::AwsKinesisErrorMessage.name) == "Some error message");
}


TEST_CASE("PutKinesisStream simple happy path") {
  minifi::test::SingleProcessorTestController controller(minifi::test::utils::make_processor<PutKinesisStreamMocked>("PutKinesisStream"));
  auto put_kinesis_stream = controller.getProcessor();
  controller.plan->setProperty(put_kinesis_stream, PutKinesisStream::AccessKey, "access_key");
  controller.plan->setProperty(put_kinesis_stream, PutKinesisStream::SecretKey, "secret_key");
  controller.plan->setProperty(put_kinesis_stream, PutKinesisStream::AmazonKinesisStreamName, "stream_name");


  const auto result = controller.trigger({{.content = "foo"}, {.content = "bar"}});
  CHECK(result.at(PutKinesisStream::Failure).empty());
  REQUIRE(result.at(PutKinesisStream::Success).size() == 2);
  const auto res_ff_1 = result.at(PutKinesisStream::Success).at(0);
  const auto res_ff_2 = result.at(PutKinesisStream::Success).at(1);

  CHECK(controller.plan->getContent(res_ff_1) == "foo");
  CHECK(controller.plan->getContent(res_ff_2) == "bar");

  CHECK(res_ff_1->getAttribute(PutKinesisStream::AwsKinesisSequenceNumber.name) == "sequence_number_1");
  CHECK(res_ff_1->getAttribute(PutKinesisStream::AwsKinesisShardId.name) == "shard_id");
  CHECK(res_ff_2->getAttribute(PutKinesisStream::AwsKinesisSequenceNumber.name) == "sequence_number_2");
  CHECK(res_ff_2->getAttribute(PutKinesisStream::AwsKinesisShardId.name) == "shard_id");
}

TEST_CASE("PutKinesisStream smaller batch size than available ffs") {
  minifi::test::SingleProcessorTestController controller(minifi::test::utils::make_processor<PutKinesisStreamMocked>("PutKinesisStream"));
  auto put_kinesis_stream = controller.getProcessor();
  controller.plan->setProperty(put_kinesis_stream, PutKinesisStream::AccessKey, "access_key");
  controller.plan->setProperty(put_kinesis_stream, PutKinesisStream::SecretKey, "secret_key");
  controller.plan->setProperty(put_kinesis_stream, PutKinesisStream::AmazonKinesisStreamName, "stream_name");
  controller.plan->setProperty(put_kinesis_stream, PutKinesisStream::MessageBatchSize, "10");

  const auto result = controller.trigger({
    {.content = "Lorem"},
    {.content = "ipsum"},
    {.content = "dolor"},
    {.content = "sit"},
    {.content = "amet"},
    {.content = "consectetur"},
    {.content = "adipiscing"},
    {.content = "elit"},
    {.content = "Morbi"},
    {.content = "dapibus"},
    {.content = "risus"},
    {.content = "a"},
    {.content = "bibendum"},
    {.content = "luctus"}});

  CHECK(result.at(PutKinesisStream::Success).size() == 10);
}

TEST_CASE("PutKinesisStream max batch data size fills up") {
  minifi::test::SingleProcessorTestController controller(minifi::test::utils::make_processor<PutKinesisStreamMocked>("PutKinesisStream"));
  auto put_kinesis_stream = controller.getProcessor();
  controller.plan->setProperty(put_kinesis_stream, PutKinesisStream::AccessKey, "access_key");
  controller.plan->setProperty(put_kinesis_stream, PutKinesisStream::SecretKey, "secret_key");
  controller.plan->setProperty(put_kinesis_stream, PutKinesisStream::AmazonKinesisStreamName, "stream_name");
  controller.plan->setProperty(put_kinesis_stream, PutKinesisStream::MessageBatchSize, "10");
  controller.plan->setProperty(put_kinesis_stream, PutKinesisStream::MaxBatchDataSize, "12 B");

  const auto result = controller.trigger({
    {.content = "Lorem"},
    {.content = "ipsum"},
    {.content = "dolor"},
    {.content = "sit"},
    {.content = "amet"},
    {.content = "consectetur"},
    {.content = "adipiscing"},
    {.content = "elit"},
    {.content = "Morbi"},
    {.content = "dapibus"},
    {.content = "risus"},
    {.content = "a"},
    {.content = "bibendum"},
    {.content = "luctus"}});

  REQUIRE(result.at(PutKinesisStream::Success).size() == 3);
  CHECK(controller.plan->getContent(result.at(PutKinesisStream::Success).at(0)) == "Lorem");
  CHECK(controller.plan->getContent(result.at(PutKinesisStream::Success).at(1)) == "ipsum");
  CHECK(controller.plan->getContent(result.at(PutKinesisStream::Success).at(2)) == "dolor");
}

TEST_CASE("PutKinesisStream max batch data size to different streams") {
  minifi::test::SingleProcessorTestController controller(minifi::test::utils::make_processor<PutKinesisStreamMocked>("PutKinesisStream"));
  auto put_kinesis_stream = controller.getProcessor();
  controller.plan->setProperty(put_kinesis_stream, PutKinesisStream::AccessKey, "access_key");
  controller.plan->setProperty(put_kinesis_stream, PutKinesisStream::SecretKey, "secret_key");
  controller.plan->setProperty(put_kinesis_stream, PutKinesisStream::MessageBatchSize, "10");
  controller.plan->setProperty(put_kinesis_stream, PutKinesisStream::MaxBatchDataSize, "12 B");
  controller.plan->setProperty(put_kinesis_stream, PutKinesisStream::AmazonKinesisStreamName, "${stream_name}");

  const auto result = controller.trigger({
    {.content = "Lorem", .attributes = {{"stream_name", "stream_one"}}},
    {.content = "ipsum", .attributes = {{"stream_name", "stream_two"}}},
    {.content = "dolor", .attributes = {{"stream_name", "stream_three"}}},
    {.content = "sit", .attributes = {{"stream_name", "stream_four"}}},
    {.content = "amet", .attributes = {{"stream_name", "stream_five"}}},
    {.content = "consectetur", .attributes = {{"stream_name", "stream_six"}}},
    {.content = "adipiscing", .attributes = {{"stream_name", "stream_seven"}}},
    {.content = "elit", .attributes = {{"stream_name", "stream_eight"}}},
    {.content = "Morbi", .attributes = {{"stream_name", "stream_nine"}}},
    {.content = "dapibus", .attributes = {{"stream_name", "stream_ten"}}},
    {.content = "risus", .attributes = {{"stream_name", "stream_eleven"}}},
    {.content = "a", .attributes = {{"stream_name", "stream_twelve"}}},
    {.content = "bibendum", .attributes = {{"stream_name", "stream_thirteen"}}},
    {.content = "luctus", .attributes = {{"stream_name", "stream_fourteen"}}}});

  CHECK(result.at(PutKinesisStream::Success).size() == 10);
}

TEST_CASE("PutKinesisStream with too large message") {
  minifi::test::SingleProcessorTestController controller(minifi::test::utils::make_processor<PutKinesisStreamMocked>("PutKinesisStream"));
  auto put_kinesis_stream = controller.getProcessor();
  controller.plan->setProperty(put_kinesis_stream, PutKinesisStream::AccessKey, "access_key");
  controller.plan->setProperty(put_kinesis_stream, PutKinesisStream::SecretKey, "secret_key");
  controller.plan->setProperty(put_kinesis_stream, PutKinesisStream::AmazonKinesisStreamName, "stream_name");

  std::string too_large_msg((1_MB + 10), 'x');
  const auto result = controller.trigger(too_large_msg);
  REQUIRE(result.at(PutKinesisStream::Failure).size() == 1);
  CHECK(result.at(PutKinesisStream::Success).empty());

  const auto res_ff_1 = result.at(PutKinesisStream::Failure).at(0);

  CHECK(controller.plan->getContent(res_ff_1) == too_large_msg);

  CHECK(res_ff_1->getAttribute(PutKinesisStream::AwsKinesisErrorMessage.name) == "record too big 1000010, max allowed 1000000");
  CHECK(res_ff_1->getAttribute(PutKinesisStream::AwsKinesisErrorCode.name) == std::nullopt);
}

}  // namespace org::apache::nifi::minifi::aws::processors::test
