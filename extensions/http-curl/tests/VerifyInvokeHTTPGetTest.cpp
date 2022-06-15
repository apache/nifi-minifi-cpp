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
#include "VerifyInvokeHTTP.h"

#include "HTTPHandlers.h"
#include "utils/IntegrationTestUtils.h"

#define CURLOPT_SSL_VERIFYPEER_DISABLE 1

class VerifyHTTPGet : public VerifyInvokeHTTP {
 public:
  void runAssertions() override {
    assert(org::apache::nifi::minifi::utils::verifyLogLinePresenceInPollTime(
        std::chrono::seconds(10),
        "key:invokehttp.status.code value:200",
        "key:flow.id"));
  }
};

class VerifyRetryHTTPGet : public VerifyInvokeHTTP {
 public:
  void runAssertions() override {
    assert(org::apache::nifi::minifi::utils::verifyLogLinePresenceInPollTime(
        std::chrono::seconds(10),
        "isSuccess: 0, response code 501"));
    assert(org::apache::nifi::minifi::utils::verifyLogLinePresenceInPollTime(
        std::chrono::seconds(10),
        "from InvokeHTTP to relationship retry"));
  }
};

int main(int argc, char **argv) {
  const cmd_args args = parse_cmdline_args(argc, argv);

  {
    HttpGetResponder http_handler;
    VerifyHTTPGet harness;
    harness.run(args.url, args.test_file, args.key_dir, &http_handler);
  }

  {
    RetryHttpGetResponder http_handler;
    VerifyRetryHTTPGet harness;
    harness.run(args.url, args.test_file, args.key_dir, &http_handler);
  }

  return 0;
}
