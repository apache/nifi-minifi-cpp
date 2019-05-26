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

#include <ProcessContext.h>
#include <memory>
#include <impl/expression/Expression.h>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {

/**
 * Purpose and Justification: Used to inject EL functionality into the classloader and remove EL as
 * a core functionality. This created linking issues as it was always required even in a disabled
 * state. With this case, we can rely on instantiation of a builder to create the necessary
 * ProcessContext. *
 */
class ProcessContextExpr : public core::ProcessContext {
 public:

  /**
   std::forward of argument list did not work on all platform.
   **/
  ProcessContextExpr(const std::shared_ptr<ProcessorNode> &processor, std::shared_ptr<controller::ControllerServiceProvider> &controller_service_provider,
                     const std::shared_ptr<core::Repository> &repo, const std::shared_ptr<core::Repository> &flow_repo,
                     const std::shared_ptr<core::ContentRepository> &content_repo = std::make_shared<core::repository::FileSystemRepository>())
      : core::ProcessContext(processor, controller_service_provider, repo, flow_repo, content_repo),
        logger_(logging::LoggerFactory<ProcessContextExpr>::getLogger()) {
  }

  ProcessContextExpr(const std::shared_ptr<ProcessorNode> &processor, std::shared_ptr<controller::ControllerServiceProvider> &controller_service_provider,
                     const std::shared_ptr<core::Repository> &repo, const std::shared_ptr<core::Repository> &flow_repo, const std::shared_ptr<minifi::Configure> &configuration,
                     const std::shared_ptr<core::ContentRepository> &content_repo = std::make_shared<core::repository::FileSystemRepository>())
      : core::ProcessContext(processor, controller_service_provider, repo, flow_repo, configuration, content_repo),
        logger_(logging::LoggerFactory<ProcessContextExpr>::getLogger()) {
  }
  // Destructor
  virtual ~ProcessContextExpr() {
  }
  /**
   * Retrieves property using EL
   * @param property property
   * @param value that will be filled
   * @param flow_file flow file.
   */
  virtual bool getProperty(const Property &property, std::string &value, const std::shared_ptr<FlowFile> &flow_file) override;

  virtual bool getDynamicProperty(const Property &property, std::string &value, const std::shared_ptr<FlowFile> &flow_file) override;
 protected:

  std::map<std::string, org::apache::nifi::minifi::expression::Expression> expressions_;
  std::map<std::string, org::apache::nifi::minifi::expression::Expression> dynamic_property_expressions_;

 private:
  std::shared_ptr<logging::Logger> logger_;
};

} /* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
