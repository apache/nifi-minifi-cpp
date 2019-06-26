#include <string>
#include <memory>
#include <vector>
#include <core/ConfigurableComponent.h>
#include <core/Processor.h>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

class ProcessorUtils {
 public:
  /**
   * Instantiates and configures a processor
   * @param class_short_name short name of the class
   * @param canonicalName full class name ( canonical name )
   * @param uuid uuid object for the processor
   * @param stream_factory stream factory to be set onto the processor
   */
  static inline std::shared_ptr<core::Processor> createProcessor(const std::string &class_short_name, const std::string &canonicalName, utils::Identifier &uuid,
                                                                 const std::shared_ptr<minifi::io::StreamFactory> &stream_factory) {
    auto ptr = core::ClassLoader::getDefaultClassLoader().instantiate(class_short_name, uuid);
    // fallback to the canonical name if the short name does not work, then attempt JNI bindings if they exist
    if (ptr == nullptr) {
      ptr = core::ClassLoader::getDefaultClassLoader().instantiate(canonicalName, uuid);
      if (ptr == nullptr) {
        ptr = core::ClassLoader::getDefaultClassLoader().instantiate("ExecuteJavaClass", uuid);
        if (ptr != nullptr) {
          std::shared_ptr<core::Processor> processor = std::dynamic_pointer_cast<core::Processor>(ptr);
          if (processor == nullptr) {
            throw std::runtime_error("Invalid return from the classloader");
          }
          processor->initialize();
          processor->setProperty("NiFi Processor", canonicalName);
          processor->setStreamFactory(stream_factory);
          return processor;
        }
      }
    }
    if (ptr == nullptr) {
      return nullptr;
    }
    auto returnPtr = std::dynamic_pointer_cast<core::Processor>(ptr);

    if (returnPtr == nullptr) {
      throw std::runtime_error("Invalid return from the classloader");
    }

    returnPtr->initialize();

    returnPtr->setStreamFactory(stream_factory);

    return returnPtr;

  }
 private:
  ProcessorUtils();
};

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
