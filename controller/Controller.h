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

#include "core/RepositoryFactory.h"
#include "core/ConfigurationFactory.h"
#include "io/ClientSocket.h"
#include "c2/ControllerSocketProtocol.h"
#include "utils/gsl.h"

/**
 * Sends a single argument comment
 * @param socket socket unique ptr.
 * @param op operation to perform
 * @param value value to send
 */
bool sendSingleCommand(std::unique_ptr<minifi::io::Socket> socket, uint8_t op, const std::string value) {
  socket->initialize();
  std::vector<uint8_t> data;
  minifi::io::BufferStream stream;
  stream.write(&op, 1);
  stream.write(value);
  return socket->write(stream.getBuffer(), stream.size()) == stream.size();
}

/**
 * Stops a stopped component
 * @param socket socket unique ptr.
 * @param op operation to perform
 */
bool stopComponent(std::unique_ptr<minifi::io::Socket> socket, std::string component) {
  return sendSingleCommand(std::move(socket), minifi::c2::Operation::STOP, component);
}

/**
 * Starts a previously stopped component.
 * @param socket socket unique ptr.
 * @param op operation to perform
 */
bool startComponent(std::unique_ptr<minifi::io::Socket> socket, std::string component) {
  return sendSingleCommand(std::move(socket), minifi::c2::Operation::START, component);
}

/**
 * Clears a connection queue.
 * @param socket socket unique ptr.
 * @param op operation to perform
 */
bool clearConnection(std::unique_ptr<minifi::io::Socket> socket, std::string connection) {
  return sendSingleCommand(std::move(socket), minifi::c2::Operation::CLEAR, connection);
}

/**
 * Updates the flow to the provided file
 */
int updateFlow(std::unique_ptr<minifi::io::Socket> socket, std::ostream &out, std::string file) {
  socket->initialize();
  std::vector<uint8_t> data;
  uint8_t op = minifi::c2::Operation::UPDATE;
  minifi::io::BufferStream stream;
  stream.write(&op, 1);
  stream.write("flow");
  stream.write(file);
  if (minifi::io::isError(socket->write(stream.getBuffer(), stream.size()))) {
    return -1;
  }
  // read the response
  uint8_t resp = 0;
  socket->read(&resp, 1);
  if (resp == minifi::c2::Operation::DESCRIBE) {
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
int getFullConnections(std::unique_ptr<minifi::io::Socket> socket, std::ostream &out) {
  socket->initialize();
  std::vector<uint8_t> data;
  uint8_t op = minifi::c2::Operation::DESCRIBE;
  minifi::io::BufferStream stream;
  stream.write(&op, 1);
  stream.write("getfull");
  if (minifi::io::isError(socket->write(stream.getBuffer(), stream.size()))) {
    return -1;
  }
  // read the response
  uint8_t resp = 0;
  socket->read(&resp, 1);
  if (resp == minifi::c2::Operation::DESCRIBE) {
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

int getJstacks(std::unique_ptr<minifi::io::Socket> socket, std::ostream &out) {
  socket->initialize();
  std::vector<uint8_t> data;
  uint8_t op = minifi::c2::Operation::DESCRIBE;
  minifi::io::BufferStream stream;
  stream.write(&op, 1);
  stream.write("jstack");
  if (minifi::io::isError(socket->write(stream.getBuffer(), stream.size()))) {
    return -1;
  }
  // read the response
  uint8_t resp = 0;
  socket->read(&resp, 1);
  if (resp == minifi::c2::Operation::DESCRIBE) {
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
int getConnectionSize(std::unique_ptr<minifi::io::Socket> socket, std::ostream &out, std::string connection) {
  socket->initialize();
  std::vector<uint8_t> data;
  uint8_t op = minifi::c2::Operation::DESCRIBE;
  minifi::io::BufferStream stream;
  stream.write(&op, 1);
  stream.write("queue");
  stream.write(connection);
  if (minifi::io::isError(socket->write(stream.getBuffer(), stream.size()))) {
    return -1;
  }
  // read the response
  uint8_t resp = 0;
  socket->read(&resp, 1);
  if (resp == minifi::c2::Operation::DESCRIBE) {
    std::string size;
    socket->read(size);
    out << "Size/Max of " << connection << " " << size << std::endl;
  }
  return 0;
}

int listComponents(std::unique_ptr<minifi::io::Socket> socket, std::ostream &out, bool show_header = true) {
  socket->initialize();
  minifi::io::BufferStream stream;
  uint8_t op = minifi::c2::Operation::DESCRIBE;
  stream.write(&op, 1);
  stream.write("components");
  if (minifi::io::isError(socket->write(stream.getBuffer(), stream.size()))) {
    return -1;
  }
  uint16_t responses = 0;
  socket->read(&op, 1);
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

int listConnections(std::unique_ptr<minifi::io::Socket> socket, std::ostream &out, bool show_header = true) {
  socket->initialize();
  minifi::io::BufferStream stream;
  uint8_t op = minifi::c2::Operation::DESCRIBE;
  stream.write(&op, 1);
  stream.write("connections");
  if (minifi::io::isError(socket->write(stream.getBuffer(), stream.size()))) {
    return -1;
  }
  uint16_t responses = 0;
  socket->read(&op, 1);
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

std::shared_ptr<core::controller::ControllerService> getControllerService(const std::shared_ptr<minifi::Configure> &configuration, const std::string &service_name) {
  std::string prov_repo_class = "provenancerepository";
  std::string flow_repo_class = "flowfilerepository";
  std::string nifi_configuration_class_name = "yamlconfiguration";
  std::string content_repo_class = "filesystemrepository";

  configuration->get(minifi::Configure::nifi_provenance_repository_class_name, prov_repo_class);
  // Create repos for flow record and provenance
  std::shared_ptr<core::Repository> prov_repo = core::createRepository(prov_repo_class, true, "provenance");
  prov_repo->initialize(configuration);

  configuration->get(minifi::Configure::nifi_flow_repository_class_name, flow_repo_class);

  std::shared_ptr<core::Repository> flow_repo = core::createRepository(flow_repo_class, true, "flowfile");

  flow_repo->initialize(configuration);

  configuration->get(minifi::Configure::nifi_content_repository_class_name, content_repo_class);

  std::shared_ptr<core::ContentRepository> content_repo = core::createContentRepository(content_repo_class, true, "content");

  content_repo->initialize(configuration);

  std::string content_repo_path;
  if (configuration->get(minifi::Configure::nifi_dbcontent_repository_directory_default, content_repo_path)) {
    std::cout << "setting default dir to " << content_repo_path << std::endl;
    minifi::setDefaultDirectory(content_repo_path);
  }

  configuration->get(minifi::Configure::nifi_configuration_class_name, nifi_configuration_class_name);

  std::shared_ptr<minifi::io::StreamFactory> stream_factory = minifi::io::StreamFactory::getInstance(configuration);

  std::unique_ptr<core::FlowConfiguration> flow_configuration = core::createFlowConfiguration(
      prov_repo, flow_repo, content_repo, configuration, stream_factory, nifi_configuration_class_name);

  std::shared_ptr<minifi::FlowController> controller = std::unique_ptr<minifi::FlowController>(
      new minifi::FlowController(prov_repo, flow_repo, configuration, std::move(flow_configuration), content_repo));
  controller->load();
  auto service = controller->getControllerService(service_name);
  if (service)
    service->onEnable();
  return service;
}

void printManifest(const std::shared_ptr<minifi::Configure> &configuration) {
  std::string prov_repo_class = "volatileprovenancerepository";
  std::string flow_repo_class = "volatileflowfilerepository";
  std::string nifi_configuration_class_name = "yamlconfiguration";
  std::string content_repo_class = "volatilecontentrepository";

  std::shared_ptr<logging::LoggerProperties> log_properties = std::make_shared<logging::LoggerProperties>();
  log_properties->setHome("./");
  log_properties->set("appender.stdout", "stdout");
  log_properties->set("logger.org::apache::nifi::minifi", "OFF,stdout");
  logging::LoggerConfiguration::getConfiguration().initialize(log_properties);

  configuration->get(minifi::Configure::nifi_provenance_repository_class_name, prov_repo_class);
  // Create repos for flow record and provenance
  std::shared_ptr<core::Repository> prov_repo = core::createRepository(prov_repo_class, true, "provenance");
  prov_repo->initialize(configuration);

  configuration->get(minifi::Configure::nifi_flow_repository_class_name, flow_repo_class);

  std::shared_ptr<core::Repository> flow_repo = core::createRepository(flow_repo_class, true, "flowfile");

  flow_repo->initialize(configuration);

  configuration->get(minifi::Configure::nifi_content_repository_class_name, content_repo_class);

  std::shared_ptr<core::ContentRepository> content_repo = core::createContentRepository(content_repo_class, true, "content");

  content_repo->initialize(configuration);

  std::string content_repo_path;
  if (configuration->get(minifi::Configure::nifi_dbcontent_repository_directory_default, content_repo_path)) {
    minifi::setDefaultDirectory(content_repo_path);
  }

  configuration->set("nifi.c2.agent.heartbeat.period", "25");
  configuration->set("nifi.c2.root.classes", "AgentInformation");
  configuration->set("nifi.c2.enable", "true");
  configuration->set("nifi.c2.agent.class", "test");
  configuration->set("c2.agent.listen", "true");
  configuration->set("nifi.c2.agent.heartbeat.reporter.classes", "AgentPrinter");

  configuration->get(minifi::Configure::nifi_configuration_class_name, nifi_configuration_class_name);

  std::shared_ptr<minifi::io::StreamFactory> stream_factory = minifi::io::StreamFactory::getInstance(configuration);

  std::unique_ptr<core::FlowConfiguration> flow_configuration = core::createFlowConfiguration(
      prov_repo, flow_repo, content_repo, configuration, stream_factory, nifi_configuration_class_name);

  std::shared_ptr<minifi::FlowController> controller = std::unique_ptr<minifi::FlowController>(
      new minifi::FlowController(prov_repo, flow_repo, configuration, std::move(flow_configuration), content_repo, "manifest"));
  controller->load();
  controller->start();
  std::this_thread::sleep_for(std::chrono::milliseconds(10000));
  controller->stop();
}

#endif /* CONTROLLER_CONTROLLER_H_ */
