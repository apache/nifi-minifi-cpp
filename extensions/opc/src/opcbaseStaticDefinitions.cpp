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

#include "fetchopc.h"
#include "opcbase.h"
#include "putopc.h"
#include "core/PropertyBuilder.h"
#include "core/Resource.h"

// FIXME(fgerlits): we need to put all these static definitions in a single file so that they are executed in this order at runtime
// when https://issues.apache.org/jira/browse/MINIFICPP-1825 is closed, these definitions should be moved back to the cpp file of the class to which they belong

namespace org::apache::nifi::minifi::processors {

// BaseOPCProcessor

const core::Property BaseOPCProcessor::OPCServerEndPoint(
    core::PropertyBuilder::createProperty("OPC server endpoint")
    ->withDescription("Specifies the address, port and relative path of an OPC endpoint")
    ->isRequired(true)->build());

const core::Property BaseOPCProcessor::ApplicationURI(
    core::PropertyBuilder::createProperty("Application URI")
        ->withDescription("Application URI of the client in the format 'urn:unconfigured:application'. "
                          "Mandatory, if using Secure Channel and must match the URI included in the certificate's Subject Alternative Names.")->build());

const core::Property BaseOPCProcessor::Username(
    core::PropertyBuilder::createProperty("Username")
        ->withDescription("Username to log in with.")->build());

const core::Property BaseOPCProcessor::Password(
    core::PropertyBuilder::createProperty("Password")
        ->withDescription("Password to log in with.")->build());

const core::Property BaseOPCProcessor::CertificatePath(
    core::PropertyBuilder::createProperty("Certificate path")
        ->withDescription("Path to the DER-encoded cert file")->build());

const core::Property BaseOPCProcessor::KeyPath(
    core::PropertyBuilder::createProperty("Key path")
        ->withDescription("Path to the DER-encoded key file")->build());

const core::Property BaseOPCProcessor::TrustedPath(
    core::PropertyBuilder::createProperty("Trusted server certificate path")
        ->withDescription("Path to the DER-encoded trusted server certificate")->build());


// FetchOPCProcessor

const core::Property FetchOPCProcessor::NodeID(
    core::PropertyBuilder::createProperty("Node ID")
    ->withDescription("Specifies the ID of the root node to traverse")
    ->isRequired(true)->build());


const core::Property FetchOPCProcessor::NodeIDType(
    core::PropertyBuilder::createProperty("Node ID type")
    ->withDescription("Specifies the type of the provided node ID")
    ->isRequired(true)
    ->withAllowableValues<std::string>({"Path", "Int", "String"})->build());

const core::Property FetchOPCProcessor::NameSpaceIndex(
    core::PropertyBuilder::createProperty("Namespace index")
    ->withDescription("The index of the namespace. Used only if node ID type is not path.")
    ->withDefaultValue<int32_t>(0)->build());

const core::Property FetchOPCProcessor::MaxDepth(
    core::PropertyBuilder::createProperty("Max depth")
    ->withDescription("Specifiec the max depth of browsing. 0 means unlimited.")
    ->withDefaultValue<uint64_t>(0)->build());

const core::Property FetchOPCProcessor::Lazy(
    core::PropertyBuilder::createProperty("Lazy mode")
    ->withDescription("Only creates flowfiles from nodes with new timestamp from the server.")
    ->withDefaultValue<std::string>("Off")
    ->isRequired(true)
    ->withAllowableValues<std::string>({"On", "Off"})
    ->build());

const core::Relationship FetchOPCProcessor::Success("success", "Successfully retrieved OPC-UA nodes");
const core::Relationship FetchOPCProcessor::Failure("failure", "Retrieved OPC-UA nodes where value cannot be extracted (only if enabled)");

REGISTER_RESOURCE(FetchOPCProcessor, Processor);


// PutOPCProcessor

const core::Property PutOPCProcessor::ParentNodeID(
    core::PropertyBuilder::createProperty("Parent node ID")
        ->withDescription("Specifies the ID of the root node to traverse")
        ->isRequired(true)->build());

const core::Property PutOPCProcessor::ParentNodeIDType(
    core::PropertyBuilder::createProperty("Parent node ID type")
        ->withDescription("Specifies the type of the provided node ID")
        ->isRequired(true)
        ->withAllowableValues<std::string>({"Path", "Int", "String"})->build());

const core::Property PutOPCProcessor::ParentNameSpaceIndex(
    core::PropertyBuilder::createProperty("Parent node namespace index")
        ->withDescription("The index of the namespace. Used only if node ID type is not path.")
        ->withDefaultValue<int32_t>(0)->build());

const core::Property PutOPCProcessor::ValueType(
    core::PropertyBuilder::createProperty("Value type")
        ->withDescription("Set the OPC value type of the created nodes")
        ->withAllowableValues(opc::stringToOPCDataTypeMapKeys())
        ->isRequired(true)->build());

const core::Property PutOPCProcessor::TargetNodeIDType(
    core::PropertyBuilder::createProperty("Target node ID type")
        ->withDescription("ID type of target node. Allowed values are: Int, String.")
        ->supportsExpressionLanguage(true)->build());

const core::Property PutOPCProcessor::TargetNodeID(
    core::PropertyBuilder::createProperty("Target node ID")
        ->withDescription("ID of target node.")
        ->supportsExpressionLanguage(true)->build());

const core::Property PutOPCProcessor::TargetNodeNameSpaceIndex(
    core::PropertyBuilder::createProperty("Target node namespace index")
        ->withDescription("The index of the namespace. Used only if node ID type is not path.")
        ->supportsExpressionLanguage(true)->build());

const core::Property PutOPCProcessor::TargetNodeBrowseName(
    core::PropertyBuilder::createProperty("Target node browse name")
        ->withDescription("Browse name of target node. Only used when new node is created.")
        ->supportsExpressionLanguage(true)->build());

const core::Relationship PutOPCProcessor::Success("success", "Successfully put OPC-UA node");
const core::Relationship PutOPCProcessor::Failure("failure", "Failed to put OPC-UA node");

REGISTER_RESOURCE(PutOPCProcessor, Processor);

}  // namespace org::apache::nifi::minifi::processors
