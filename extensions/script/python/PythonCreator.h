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

#ifndef EXTENSIONS_SCRIPT_PYTHONCREATOR_H_
#define EXTENSIONS_SCRIPT_PYTHONCREATOR_H_

#include <vector>
#include <string>
#include <memory>
#include "core/Core.h"
#include "core/logging/LoggerConfiguration.h"
#include "ExecutePythonProcessor.h"
#include "PyProcCreator.h"
#include "agent/agent_version.h"
#include "agent/build_description.h"
#include "utils/file/FileUtils.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace python {

/**
 * Can be used to load the python processors from NiFi properties.
 */
class PythonCreator : public minifi::core::CoreComponent {
 public:

  explicit PythonCreator(const std::string &name, utils::Identifier uuid = utils::Identifier())
      : minifi::core::CoreComponent(name, uuid),
        logger_(logging::LoggerFactory<PythonCreator>::getLogger()) {
  }

  virtual ~PythonCreator();

  void configure(const std::vector<std::string> &pythonFiles) {
    std::vector<std::string> pathOrFiles;
    for (const auto &path : pythonFiles) {
      const auto vec = utils::StringUtils::split(path, ",");
      pathOrFiles.insert(pathOrFiles.end(), vec.begin(), vec.end());
    }

    for (const auto &path : pathOrFiles) {
      utils::file::FileUtils::addFilesMatchingExtension(logger_, path, ".py", classpaths_);
    }

  }

  virtual void configure(const std::shared_ptr<Configure> &configuration) override {

    python::PythonScriptEngine::initialize();

    auto engine = std::make_shared<python::PythonScriptEngine>();
    std::string pathListings;

    // assuming we have the options set and can access the PythonCreator

    if (configuration->get("nifi.python.processor.dir", pathListings)) {
      std::vector<std::string> paths;
      paths.emplace_back(pathListings);

      configure(paths);

      for (const auto &path : classpaths_) {
        const auto &scriptname = getScriptName(path);
        PyProcCreator::getPythonCreator()->addClassName(scriptname, path);
      }

      core::ClassLoader::getDefaultClassLoader().registerResource("", "createPyProcFactory");

      for (const auto &path : classpaths_) {
        const auto &scriptName = getScriptName(path);
        utils::Identifier uuid;
        auto processor = std::dynamic_pointer_cast<core::Processor>(core::ClassLoader::getDefaultClassLoader().instantiate(scriptName, uuid));
        if (processor) {
          try {
            processor->initialize();
            auto proc = std::dynamic_pointer_cast<python::processors::ExecutePythonProcessor>(processor);
            minifi::BundleDetails details;
            details.artifact = getFileName(path);
            details.version = minifi::AgentBuild::VERSION;
            details.group = "python";
            minifi::ClassDescription description("org.apache.nifi.minifi.processors." + scriptName);
            description.dynamic_properties_ = proc->getPythonSupportDynamicProperties();
            auto properties = proc->getPythonProperties();

            minifi::AgentDocs::putDescription(scriptName, proc->getDescription());
            for (const auto &prop : properties) {
              description.class_properties_.insert(std::make_pair(prop.getName(), prop));
            }

            for (const auto &rel : proc->getSupportedRelationships()) {
              description.class_relationships_.push_back(rel);
            }
            minifi::ExternalBuildDescription::addExternalComponent(details, description);
          } catch (const std::exception &e) {
            logger_->log_warn("Cannot load %s because of %s", scriptName, e.what());
          }

        }

      }

    }

  }

 private:

  std::string getFileName(const std::string &pythonscript) {
    std::string path = pythonscript.substr(pythonscript.find_last_of("/\\") + 1);
    return path;
  }

  std::string getScriptName(const std::string &pythonscript) {
    std::string path = pythonscript.substr(pythonscript.find_last_of("/\\") + 1);
    size_t dot_i = path.find_last_of('.');
    return path.substr(0, dot_i);
  }

  std::vector<std::string> classpaths_;

  std::shared_ptr<logging::Logger> logger_;
}
;

} /* namespace python */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* EXTENSIONS_SCRIPT_PYTHONCREATOR_H_ */
