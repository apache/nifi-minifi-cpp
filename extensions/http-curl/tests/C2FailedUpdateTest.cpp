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

#undef NDEBUG
#include "HTTPIntegrationBase.h"
#include "HTTPHandlers.h"

int main(int argc, char **argv) {
  const cmd_args args = parse_cmdline_args(argc, argv, "update");
  C2FailedUpdateHandler handler(args.bad_test_file);
  VerifyC2FailedUpdate harness(10000);
  harness.setKeyDir(args.key_dir);
  harness.setUrl(args.url, &handler);
  handler.setC2RestResponse(harness.getC2RestUrl(), "configuration");
  harness.run(args.test_file);
  return 0;
}
