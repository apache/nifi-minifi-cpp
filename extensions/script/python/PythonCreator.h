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
#include "utils/StringUtils.h"

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
        const auto &package = getPackage(pathListings, path);
        if (!package.empty()) {
          std::string script_with_package = "org.apache.nifi.minifi.processors." + package + "." + scriptname;
          PyProcCreator::getPythonCreator()->addClassName(script_with_package, path);
        } else {
          // maintain backwards compatibility with the standard package.
          PyProcCreator::getPythonCreator()->addClassName(scriptname, path);
        }
      }

      core::ClassLoader::getDefaultClassLoader().registerResource("", "createPyProcFactory");

      for (const auto &path : classpaths_) {
        const auto &scriptName = getScriptName(path);

        utils::Identifier uuid;

        std::string loadName = scriptName;
        const auto &package = getPackage(pathListings, path);

        if (!package.empty())
          loadName = "org.apache.nifi.minifi.processors." + package + "." + scriptName;

        auto processor = std::dynamic_pointer_cast<core::Processor>(core::ClassLoader::getDefaultClassLoader().instantiate(loadName, uuid));
        if (processor) {
          try {
            processor->initialize();
            auto proc = std::dynamic_pointer_cast<python::processors::ExecutePythonProcessor>(processor);
            minifi::BundleDetails details;
            const auto &package = getPackage(pathListings, path);
            std::string script_with_package = "org.apache.nifi.minifi.processors.";
            if (!package.empty()) {
              script_with_package += package + ".";
            }
            script_with_package += scriptName;
            details.artifact = getFileName(path);
            details.version = minifi::AgentBuild::VERSION;
            details.group = "python";

            minifi::ClassDescription description(script_with_package);
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
  std::string getPackage(const std::string &basePath, const std::string pythonscript) {
    const auto script_directory = getPath(pythonscript);
    const auto loc = script_directory.find_first_of(basePath);
    if (loc != 0 || script_directory.size() <= basePath.size()) {
      return "";
    }
    const auto python_dir = script_directory.substr(basePath.length() + 1);
    auto python_package = python_dir.substr(0, python_dir.find_last_of("/\\"));
    utils::StringUtils::replaceAll(python_package, "/", ".");
    utils::StringUtils::replaceAll(python_package, "\\", ".");
    if (python_package.length() > 1 && python_package.at(0) == '.') {
      python_package = python_package.substr(1);
    }
    std::transform(python_package.begin(), python_package.end(), python_package.begin(), ::tolower);
    return python_package;
  }

  std::string getPath(const std::string &pythonscript) {
    std::string path = pythonscript.substr(0, pythonscript.find_last_of("/\\"));
    return path;
  }

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
