// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <string>
#include <memory>
#include <vector>

#include "core/logging/LoggerConfiguration.h"
#include "core/flow/Node.h"

namespace org::apache::nifi::minifi::core::flow {

bool isFieldPresent(const Node &node, std::string_view field_name);
std::string buildErrorMessage(const Node &node, const std::vector<std::string> &alternate_field_names);

/**
 * This is a helper function for verifying the existence of a required
 * field in a Node object. If the field is not present, an error
 * message will be logged and an std::invalid_argument exception will be
 * thrown indicating the absence of the required field in the node.
 *
 * @param node          the node to check
 * @param field_name    the required field key
 * @param section       [optional] the top level section of the config
 *                       for the node. This is used for generating a
 *                       useful error message for troubleshooting.
 * @param error_message [optional] the error message string to use if
 *                       the required field is missing. If not provided,
 *                       a default error message will be generated.
 *
 * @throws std::invalid_argument if the required field 'field_name' is
 *                               not present in 'node'
 */
void checkRequiredField(const Node &node, const std::vector<std::string>& field_name, std::string_view error_message = "");

std::string getRequiredField(const Node &node, const std::vector<std::string> &alternate_names, std::string_view error_message = {});

}  // namespace org::apache::nifi::minifi::core::flow
