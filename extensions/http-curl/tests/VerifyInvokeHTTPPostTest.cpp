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
#include <optional>

#include "VerifyInvokeHTTP.h"

#include "HTTPHandlers.h"
#include "utils/IntegrationTestUtils.h"

class VerifyInvokeHTTPOKResponse : public VerifyInvokeHTTP {
 public:
  void runAssertions() override {
    using org::apache::nifi::minifi::utils::verifyLogLinePresenceInPollTime;
    assert(verifyLogLinePresenceInPollTime(std::chrono::seconds(6),
        "key:invokehttp.status.code value:201",
        "response code 201"));
  }
};

class VerifyInvokeHTTPOK200Response : public VerifyInvokeHTTP {
 public:
  void runAssertions() override {
    using org::apache::nifi::minifi::utils::verifyLogLinePresenceInPollTime;
    assert(verifyLogLinePresenceInPollTime(std::chrono::seconds(6),
        "key:invokehttp.status.code value:200",
        "response code 200"));
  }
};

class VerifyInvokeHTTPRedirectResponse : public VerifyInvokeHTTP {
 public:
  void setupFlow(const std::optional<std::filesystem::path>& flow_yml_path) override {
    VerifyInvokeHTTP::setupFlow(flow_yml_path);
    setProperty(minifi::processors::InvokeHTTP::FollowRedirects, "false");
  }

  void runAssertions() override {
    using org::apache::nifi::minifi::utils::verifyLogLinePresenceInPollTime;
    assert(verifyLogLinePresenceInPollTime(std::chrono::seconds(6),
        "key:invokehttp.status.code value:301",
        "response code 301"));
  }
};

class VerifyCouldNotConnectInvokeHTTP : public VerifyInvokeHTTP {
 public:
  void runAssertions() override {
    using org::apache::nifi::minifi::utils::verifyLogLinePresenceInPollTime;
    assert(verifyLogLinePresenceInPollTime(std::chrono::seconds(6), "key:invoke_http value:failure"));
  }
};

class VerifyNoRetryInvokeHTTP : public VerifyInvokeHTTP {
 public:
  void runAssertions() override {
    using org::apache::nifi::minifi::utils::verifyLogLinePresenceInPollTime;
    assert(verifyLogLinePresenceInPollTime(std::chrono::seconds(6),
        "key:invokehttp.status.message value:HTTP/1.1 404 Not Found",
        "isSuccess: 0, response code 404"));
  }
};

class VerifyRetryInvokeHTTP : public VerifyInvokeHTTP {
 public:
  void runAssertions() override {
    using org::apache::nifi::minifi::utils::verifyLogLinePresenceInPollTime;
    assert(verifyLogLinePresenceInPollTime(std::chrono::seconds(6),
        "key:invokehttp.status.message value:HTTP/1.1 501 Not Implemented",
        "isSuccess: 0, response code 501"));
  }
};

class VerifyRWTimeoutInvokeHTTP : public VerifyInvokeHTTP {
 public:
  void runAssertions() override {
    using org::apache::nifi::minifi::utils::verifyLogLinePresenceInPollTime;
    assert(verifyLogLinePresenceInPollTime(std::chrono::seconds(6),
        "key:invoke_http value:failure",
        "limit (1000ms) reached, terminating connection"));
  }
};

int main(int argc, char ** argv) {
  const cmd_args args = parse_cmdline_args(argc, argv);

  // Stop civet server to simulate
  // unreachable remote end point
  {
    InvokeHTTPCouldNotConnectHandler handler;
    VerifyCouldNotConnectInvokeHTTP harness;
    harness.setKeyDir(args.key_dir);
    harness.setUrl(args.url, &handler);
    harness.setupFlow(args.test_file);
    harness.shutdownBeforeFlowController();
    harness.startFlowController();
    harness.runAssertions();
    harness.stopFlowController();
  }

  {
    InvokeHTTPResponseOKHandler handler;
    VerifyInvokeHTTPOKResponse harness;
    harness.run(args.url, args.test_file, args.key_dir, &handler);
  }

  {
    InvokeHTTPRedirectHandler handler;
    VerifyInvokeHTTPOK200Response harness;
    harness.run(args.url, args.test_file, args.key_dir, &handler);
  }

  {
    InvokeHTTPRedirectHandler handler;
    VerifyInvokeHTTPRedirectResponse harness;
    harness.run(args.url, args.test_file, args.key_dir, &handler);
  }

  {
    InvokeHTTPResponse404Handler handler;
    VerifyNoRetryInvokeHTTP harness;
    harness.run(args.url, args.test_file, args.key_dir, &handler);
  }

  {
    InvokeHTTPResponse501Handler handler;
    VerifyRetryInvokeHTTP harness;
    harness.run(args.url, args.test_file, args.key_dir, &handler);
  }

  {
    TimeoutingHTTPHandler handler({std::chrono::seconds(2)});
    VerifyRWTimeoutInvokeHTTP harness;
    harness.run(args.url, args.test_file, args.key_dir, &handler);
  }

  return 0;
}
