#include <string>
#include <map>
#include <memory>
#include <utility>
#include <exception>

#include "api/nanofi.h"
#include "core/Core.h"
#include "core/expect.h"
#include "cxx/Instance.h"
#include "cxx/Plan.h"
#include "ResourceClaim.h"
#include "processors/GetFile.h"
#include "core/logging/LoggerConfiguration.h"
#include "utils/StringUtils.h"

using string_map = std::map<std::string, std::string>;

class API_INITIALIZER {
 public:
  static int initialized;
};

int API_INITIALIZER::initialized = initialize_api();

int initialize_api() {
  logging::LoggerConfiguration::getConfiguration().disableLogging();
  return 1;
}

void enable_logging() {
  logging::LoggerConfiguration::getConfiguration().enableLogging();
}

void set_terminate_callback(void (*terminate_callback)()) {
  std::set_terminate(terminate_callback);
}

class DirectoryConfiguration {
 protected:
  DirectoryConfiguration() {
    minifi::setDefaultDirectory(DEFAULT_CONTENT_DIRECTORY);
  }
 public:
  static void initialize() {
    static DirectoryConfiguration configure;
  }
};


void *create_cxx_instance(const char *url, nifi_port *port){
  DirectoryConfiguration::initialize();
  return new minifi::Instance(url, port->port_id);
}

void initialize_cxx_instance(nifi_instance* instance){
  auto minifi_instance_ref = static_cast<minifi::Instance*>(instance->instance_ptr);
    minifi_instance_ref->setRemotePort(instance->port.port_id);
}


void enable_async_cxx_c2(nifi_instance *instance, C2_Server *server, c2_stop_callback *c1, c2_start_callback *c2, c2_update_callback *c3) {
  auto minifi_instance_ref = static_cast<minifi::Instance*>(instance->instance_ptr);
  minifi_instance_ref->enableAsyncC2(server, c1, c2, c3);
}
