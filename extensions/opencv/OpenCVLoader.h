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
#ifndef EXTENSIONS_OPENCVLOADER_H_
#define EXTENSIONS_OPENCVLOADER_H_

#include "CaptureRTSPFrame.h"
#include "core/ClassLoader.h"

class OpenCVObjectFactoryInitializer : public core::ObjectFactoryInitializer {
 public:
  virtual bool initialize() {
    // By default in OpenCV, ffmpeg capture is hardcoded to use TCP and this is a workaround
    // also if UDP timeout, ffmpeg will retry with TCP
    // Note:
    // 1. OpenCV community are trying to find a better approach than setenv.
    // 2. The command will not overwrite value if "OPENCV_FFMPEG_CAPTURE_OPTIONS" already exists.
    return setenv("OPENCV_FFMPEG_CAPTURE_OPTIONS", "rtsp_transport;udp", 0) == 0;
  }

  virtual void deinitialize() {
  }
};

class OpenCVObjectFactory : public core::ObjectFactory {
 public:
  OpenCVObjectFactory() {

  }

  /**
   * Gets the name of the object.
   * @return class name of processor
   */
  virtual std::string getName() override{
    return "OpenCVObjectFactory";
  }

  virtual std::string getClassName() override{
    return "OpenCVObjectFactory";
  }
  /**
   * Gets the class name for the object
   * @return class name for the processor.
   */
  virtual std::vector<std::string> getClassNames() override{
    std::vector<std::string> class_names;
    class_names.push_back("CaptureRTSPFrame");
    return class_names;
  }

  virtual std::unique_ptr<ObjectFactory> assign(const std::string &class_name) override{
    if (utils::StringUtils::equalsIgnoreCase(class_name, "CaptureRTSPFrame")) {
      return std::unique_ptr<ObjectFactory>(new core::DefautObjectFactory<minifi::processors::CaptureRTSPFrame>());
    } else {
      return nullptr;
    }
  }

  virtual std::unique_ptr<core::ObjectFactoryInitializer> getInitializer() {
    return std::unique_ptr<core::ObjectFactoryInitializer>(new OpenCVObjectFactoryInitializer());
  }

  static bool added;

};

extern "C" {
	DLL_EXPORT void *createOpenCVFactory(void);
}
#endif /* EXTENSIONS_OPENCVLOADER_H_ */
