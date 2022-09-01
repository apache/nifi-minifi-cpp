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
#ifndef CONTROLLER_CONTROLLER_H_
#define CONTROLLER_CONTROLLER_H_

#include <memory>

#include "core/RepositoryFactory.h"
#include "core/ConfigurationFactory.h"
#include "core/extension/ExtensionManager.h"
#include "io/ClientSocket.h"
#include "c2/ControllerSocketProtocol.h"
#include "utils/gsl.h"
#include "Exception.h"
#include "FlowController.h"

/**
 * Sends a single argument comment
 * @param socket socket unique ptr.
 * @param op operation to perform
 * @param value value to send
 */
bool sendSingleCommand(std::unique_ptr<org::apache::nifi::minifi::io::Socket> socket, uint8_t op, const std::string& value) {
  socket->initialize();
  std::vector<uint8_t> data;
  org::apache::nifi::minifi::io::BufferStream stream;
  stream.write(&op, 1);
  stream.write(value);
  return socket->write(stream.getBuffer()) == stream.size();
}

/**
 * Stops a stopped component
 * @param socket socket unique ptr.
 * @param op operation to perform
 */
bool stopComponent(std::unique_ptr<org::apache::nifi::minifi::io::Socket> socket, const std::string& component) {
  return sendSingleCommand(std::move(socket), org::apache::nifi::minifi::c2::Operation::STOP, component);
}

/**
 * Starts a previously stopped component.
 * @param socket socket unique ptr.
 * @param op operation to perform
 */
bool startComponent(std::unique_ptr<org::apache::nifi::minifi::io::Socket> socket, const std::string& component) {
  return sendSingleCommand(std::move(socket), org::apache::nifi::minifi::c2::Operation::START, component);
}

/**
 * Clears a connection queue.
 * @param socket socket unique ptr.
 * @param op operation to perform
 */
bool clearConnection(std::unique_ptr<org::apache::nifi::minifi::io::Socket> socket, const std::string& connection) {
  return sendSingleCommand(std::move(socket), org::apache::nifi::minifi::c2::Operation::CLEAR, connection);
}

/**
 * Updates the flow to the provided file
 */
int updateFlow(std::unique_ptr<org::apache::nifi::minifi::io::Socket> socket, std::ostream &out, const std::string& file) {
  socket->initialize();
  std::vector<uint8_t> data;
  uint8_t op = org::apache::nifi::minifi::c2::Operation::UPDATE;
  org::apache::nifi::minifi::io::BufferStream stream;
  stream.write(&op, 1);
  stream.write("flow");
  stream.write(file);
  if (org::apache::nifi::minifi::io::isError(socket->write(stream.getBuffer()))) {
    return -1;
  }
  // read the response
  uint8_t resp = 0;
  socket->read(resp);
  if (resp == org::apache::nifi::minifi::c2::Operation::DESCRIBE) {
    uint16_t connections = 0;
    socket->read(connections);
    out << connections << " are full" << std::endl;
    for (int i = 0; i < connections; i++) {
      std::string fullcomponent;
      socket->read(fullcomponent);
      out << fullcomponent << " is full" << std::endl;
    }
  }
  return 0;
}

/**
 * Lists connections which are full
 * @param socket socket ptr
 */
int getFullConnections(std::unique_ptr<org::apache::nifi::minifi::io::Socket> socket, std::ostream &out) {
  socket->initialize();
  std::vector<uint8_t> data;
  uint8_t op = org::apache::nifi::minifi::c2::Operation::DESCRIBE;
  org::apache::nifi::minifi::io::BufferStream stream;
  stream.write(&op, 1);
  stream.write("getfull");
  if (org::apache::nifi::minifi::io::isError(socket->write(stream.getBuffer()))) {
    return -1;
  }
  // read the response
  uint8_t resp = 0;
  socket->read(resp);
  if (resp == org::apache::nifi::minifi::c2::Operation::DESCRIBE) {
    uint16_t connections = 0;
    socket->read(connections);
    out << connections << " are full" << std::endl;
    for (int i = 0; i < connections; i++) {
      std::string fullcomponent;
      socket->read(fullcomponent);
      out << fullcomponent << " is full" << std::endl;
    }
  }
  return 0;
}

int getJstacks(std::unique_ptr<org::apache::nifi::minifi::io::Socket> socket, std::ostream &out) {
  socket->initialize();
  std::vector<uint8_t> data;
  uint8_t op = org::apache::nifi::minifi::c2::Operation::DESCRIBE;
  org::apache::nifi::minifi::io::BufferStream stream;
  stream.write(&op, 1);
  stream.write("jstack");
  if (org::apache::nifi::minifi::io::isError(socket->write(stream.getBuffer()))) {
    return -1;
  }
  // read the response
  uint8_t resp = 0;
  socket->read(resp);
  if (resp == org::apache::nifi::minifi::c2::Operation::DESCRIBE) {
    uint64_t size = 0;
    socket->read(size);

    for (uint64_t i = 0; i < size; i++) {
      std::string name;
      uint64_t lines = 0;
      socket->read(name);
      socket->read(lines);
      for (uint64_t j = 0; j < lines; j++) {
        std::string line;
        socket->read(line);
        out << name << " -- " << line << std::endl;
      }
    }
  }
  return 0;
}

/**
 * Prints the connection size for the provided connection.
 * @param socket socket ptr
 * @param connection connection whose size will be returned.
 */
int getConnectionSize(std::unique_ptr<org::apache::nifi::minifi::io::Socket> socket, std::ostream &out, const std::string& connection) {
  socket->initialize();
  std::vector<uint8_t> data;
  uint8_t op = org::apache::nifi::minifi::c2::Operation::DESCRIBE;
  org::apache::nifi::minifi::io::BufferStream stream;
  stream.write(&op, 1);
  stream.write("queue");
  stream.write(connection);
  if (org::apache::nifi::minifi::io::isError(socket->write(stream.getBuffer()))) {
    return -1;
  }
  // read the response
  uint8_t resp = 0;
  socket->read(resp);
  if (resp == org::apache::nifi::minifi::c2::Operation::DESCRIBE) {
    std::string size;
    socket->read(size);
    out << "Size/Max of " << connection << " " << size << std::endl;
  }
  return 0;
}

int listComponents(std::unique_ptr<org::apache::nifi::minifi::io::Socket> socket, std::ostream &out, bool show_header = true) {
  socket->initialize();
  org::apache::nifi::minifi::io::BufferStream stream;
  uint8_t op = org::apache::nifi::minifi::c2::Operation::DESCRIBE;
  stream.write(&op, 1);
  stream.write("components");
  if (org::apache::nifi::minifi::io::isError(socket->write(stream.getBuffer()))) {
    return -1;
  }
  uint16_t responses = 0;
  socket->read(op);
  socket->read(responses);
  if (show_header)
    out << "Components:" << std::endl;

  for (int i = 0; i < responses; i++) {
    std::string name, status;
    socket->read(name, false);
    socket->read(status, false);
    out << name << ", running: " << status << std::endl;
  }
  return 0;
}

int listConnections(std::unique_ptr<org::apache::nifi::minifi::io::Socket> socket, std::ostream &out, bool show_header = true) {
  socket->initialize();
  org::apache::nifi::minifi::io::BufferStream stream;
  uint8_t op = org::apache::nifi::minifi::c2::Operation::DESCRIBE;
  stream.write(&op, 1);
  stream.write("connections");
  if (org::apache::nifi::minifi::io::isError(socket->write(stream.getBuffer()))) {
    return -1;
  }
  uint16_t responses = 0;
  socket->read(op);
  socket->read(responses);
  if (show_header)
    out << "Connection Names:" << std::endl;

  for (int i = 0; i < responses; i++) {
    std::string name;
    socket->read(name, false);
    out << name << std::endl;
  }
  return 0;
}

std::shared_ptr<org::apache::nifi::minifi::core::controller::ControllerService> getControllerService(const std::shared_ptr<org::apache::nifi::minifi::Configure> &configuration, const std::string &service_name) {
  std::string prov_repo_class = "provenancerepository";
  std::string flow_repo_class = "flowfilerepository";
  std::string nifi_configuration_class_name = "yamlconfiguration";
  std::string content_repo_class = "filesystemrepository";

  org::apache::nifi::minifi::core::extension::ExtensionManager::get().initialize(configuration);

  configuration->get(org::apache::nifi::minifi::Configure::nifi_provenance_repository_class_name, prov_repo_class);
  // Create repos for flow record and provenance
  const std::shared_ptr prov_repo = org::apache::nifi::minifi::core::createRepository(prov_repo_class, "provenance");
  if (!prov_repo) {
    throw org::apache::nifi::minifi::Exception(org::apache::nifi::minifi::REPOSITORY_EXCEPTION, "Could not create provenance repository");
  }
  prov_repo->initialize(configuration);

  configuration->get(org::apache::nifi::minifi::Configure::nifi_flow_repository_class_name, flow_repo_class);

  const std::shared_ptr flow_repo = org::apache::nifi::minifi::core::createRepository(flow_repo_class, "flowfile");
  if (!flow_repo) {
    throw org::apache::nifi::minifi::Exception(org::apache::nifi::minifi::REPOSITORY_EXCEPTION, "Could not create flowfile repository");
  }

  flow_repo->initialize(configuration);


  configuration->get(org::apache::nifi::minifi::Configure::nifi_content_repository_class_name, content_repo_class);

  const std::shared_ptr content_repo = org::apache::nifi::minifi::core::createContentRepository(content_repo_class, true, "content");

  content_repo->initialize(configuration);

  std::string content_repo_path;
  if (configuration->get(org::apache::nifi::minifi::Configure::nifi_dbcontent_repository_directory_default, content_repo_path)) {
    std::cout << "setting default dir to " << content_repo_path << std::endl;
    org::apache::nifi::minifi::setDefaultDirectory(content_repo_path);
  }

  configuration->get(org::apache::nifi::minifi::Configure::nifi_configuration_class_name, nifi_configuration_class_name);

  const auto stream_factory = org::apache::nifi::minifi::io::StreamFactory::getInstance(configuration);

  auto flow_configuration = org::apache::nifi::minifi::core::createFlowConfiguration(prov_repo, flow_repo, content_repo, configuration, stream_factory, nifi_configuration_class_name);

  const auto controller = std::make_unique<org::apache::nifi::minifi::FlowController>(prov_repo, flow_repo, configuration, std::move(flow_configuration), content_repo);
  controller->load();
  auto service = controller->getControllerService(service_name);
  if (service)
    service->onEnable();
  return service;
}

void printManifest(const std::shared_ptr<org::apache::nifi::minifi::Configure> &configuration) {
  std::string prov_repo_class = "volatileprovenancerepository";
  std::string flow_repo_class = "volatileflowfilerepository";
  std::string nifi_configuration_class_name = "yamlconfiguration";
  std::string content_repo_class = "volatilecontentrepository";

  const auto log_properties = std::make_shared<org::apache::nifi::minifi::core::logging::LoggerProperties>();
  log_properties->setHome("./");
  log_properties->set("appender.stdout", "stdout");
  log_properties->set("logger.org::apache::nifi::minifi", "OFF,stdout");
  org::apache::nifi::minifi::core::logging::LoggerConfiguration::getConfiguration().initialize(log_properties);

  configuration->get(org::apache::nifi::minifi::Configure::nifi_provenance_repository_class_name, prov_repo_class);
  // Create repos for flow record and provenance
  const std::shared_ptr prov_repo = org::apache::nifi::minifi::core::createRepository(prov_repo_class, "provenance");
  if (!prov_repo) {
    throw org::apache::nifi::minifi::Exception(org::apache::nifi::minifi::REPOSITORY_EXCEPTION, "Could not create provenance repository");
  }
  prov_repo->initialize(configuration);

  configuration->get(org::apache::nifi::minifi::Configure::nifi_flow_repository_class_name, flow_repo_class);

  const std::shared_ptr flow_repo = org::apache::nifi::minifi::core::createRepository(flow_repo_class, "flowfile");
  if (!flow_repo) {
    throw org::apache::nifi::minifi::Exception(org::apache::nifi::minifi::REPOSITORY_EXCEPTION, "Could not create flowfile repository");
  }

  flow_repo->initialize(configuration);

  configuration->get(org::apache::nifi::minifi::Configure::nifi_content_repository_class_name, content_repo_class);

  const std::shared_ptr content_repo = org::apache::nifi::minifi::core::createContentRepository(content_repo_class, true, "content");

  content_repo->initialize(configuration);

  std::string content_repo_path;
  if (configuration->get(org::apache::nifi::minifi::Configure::nifi_dbcontent_repository_directory_default, content_repo_path)) {
    org::apache::nifi::minifi::setDefaultDirectory(content_repo_path);
  }

  configuration->set(org::apache::nifi::minifi::Configure::nifi_c2_agent_heartbeat_period, "25");
  configuration->set(org::apache::nifi::minifi::Configure::nifi_c2_root_classes, "AgentInformation");
  configuration->set(org::apache::nifi::minifi::Configure::nifi_c2_enable, "true");
  configuration->set(org::apache::nifi::minifi::Configure::nifi_c2_agent_class, "test");
  configuration->set(org::apache::nifi::minifi::Configure::nifi_c2_agent_heartbeat_reporter_classes, "AgentPrinter");

  configuration->get(org::apache::nifi::minifi::Configure::nifi_configuration_class_name, nifi_configuration_class_name);

  const auto stream_factory = org::apache::nifi::minifi::io::StreamFactory::getInstance(configuration);

  auto flow_configuration = org::apache::nifi::minifi::core::createFlowConfiguration(
      prov_repo, flow_repo, content_repo, configuration, stream_factory, nifi_configuration_class_name);

  const auto controller = std::make_unique<org::apache::nifi::minifi::FlowController>(prov_repo, flow_repo, configuration, std::move(flow_configuration), content_repo, "manifest");
  controller->load();
  controller->start();
  std::this_thread::sleep_for(std::chrono::milliseconds(10000));
  controller->stop();
}

#endif /* CONTROLLER_CONTROLLER_H_ */
