/**
 * @file PutS3Object.cpp
 * PutS3Object class implementation
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
#include "PutS3Object.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace aws {
namespace processors {

const core::Property PutS3Object::ObjectKey(
  core::PropertyBuilder::createProperty("Object Key")
    ->withDescription("The key of the S3 object")
    ->isRequired(true)
    ->supportsExpressionLanguage(true)
    ->withDefaultValue<std::string>("${filename}")
    ->build());
const core::Property PutS3Object::Bucket(
  core::PropertyBuilder::createProperty("Bucket")
    ->withDescription("The S3 bucket")
    ->isRequired(true)
    ->supportsExpressionLanguage(true)
    ->build());
const core::Property PutS3Object::ContentType(
  core::PropertyBuilder::createProperty("Content Type")
    ->withDescription("Sets the Content-Type HTTP header indicating the type of content stored in "
                      "the associated object. The value of this header is a standard MIME type."
                      " AWS S3 client will attempt to determine the correct content type if "
                      "one hasn't been set yet. Users are responsible for ensuring a suitable "
                      "content type is set when uploading streams. If no content type is provided"
                      " and cannot be determined by the filename, the default content type "
                      "\"application/octet-stream\" will be used.")
    ->supportsExpressionLanguage(true)
    ->build());
const core::Property PutS3Object::AccessKey(
  core::PropertyBuilder::createProperty("Access Key")
    ->withDescription("AWS credential access key")
    ->supportsExpressionLanguage(true)
    ->build());
const core::Property PutS3Object::SecretKey(
  core::PropertyBuilder::createProperty("Secret Key")
    ->withDescription("AWS credential secret key")
    ->supportsExpressionLanguage(true)
    ->build());
const core::Property PutS3Object::CredentialsFile(
  core::PropertyBuilder::createProperty("Credentials File")
    ->withDescription("Path to a file containing AWS access key and secret key in properties file format.")
    ->build());
const core::Property PutS3Object::StorageClass(
  core::PropertyBuilder::createProperty("Storage Class")
    ->isRequired(true)
    ->withDefaultValue<std::string>(storage_class::STANDARD)
    ->withAllowableValues<std::string>({storage_class::STANDARD, storage_class::REDUCED_REDUNDANCY})
    ->withDescription("")
    ->build());
const core::Property PutS3Object::Region(
  core::PropertyBuilder::createProperty("Region")
    ->isRequired(true)
    ->withDefaultValue<std::string>(region::US_WEST_2)
    ->withAllowableValues<std::string>({region::US_GOV_WEST_1, region::US_EAST_1, region::US_EAST_2, region::US_WEST_1,
      region::US_WEST_2, region::EU_WEST_1, region::EU_WEST_2, region::EU_CENTRAL_1, region::AP_SOUTH_1,
      region::AP_SOUTHEAST_1, region::AP_SOUTHEAST_2, region::AP_NORTHEAST_1, region::AP_NORTHEAST_2,
      region::SA_EAST_1, region::CN_NORTH_1, region::CA_CENTRAL_1})
    ->withDescription("")
    ->build());
const core::Property PutS3Object::CommunicationsTimeout(
  core::PropertyBuilder::createProperty("Communications Timeout")
    ->isRequired(true)
    ->withDefaultValue<core::TimePeriodValue>("30 sec")
    ->withDescription("")
    ->build());
const core::Property PutS3Object::EndpointOverrideURL(
  core::PropertyBuilder::createProperty("Endpoint Override URL")
    ->withDescription("Endpoint URL to use instead of the AWS default including scheme, host, "
                      "port, and path. The AWS libraries select an endpoint URL based on the AWS "
                      "region, but this property overrides the selected endpoint URL, allowing use "
                      "with other S3-compatible endpoints.")
    ->supportsExpressionLanguage(true)
    ->build());
const core::Property PutS3Object::ProxyHost(
  core::PropertyBuilder::createProperty("Proxy Host")
      ->withDescription("Proxy host name or IP")
      ->build());
const core::Property PutS3Object::ProxyHostPort(
    core::PropertyBuilder::createProperty("Proxy Host Port")
      ->withDescription("Proxy host port")
      ->build());

const core::Relationship PutS3Object::Success("success", "Any FlowFile that is successfully uploaded to AWS S3 will be routed to this Relationship");
const core::Relationship PutS3Object::Failure("failure", "Any FlowFile that cannot be uploaded to AWS S3 will be routed to this Relationship");

void PutS3Object::initialize() {
  // Set the supported properties
  std::set<core::Property> properties;
  properties.insert(ObjectKey);
  properties.insert(Bucket);
  properties.insert(ContentType);
  properties.insert(AccessKey);
  properties.insert(SecretKey);
  properties.insert(CredentialsFile);
  // properties.insert(AWSCredentialsProviderService);
  properties.insert(StorageClass);
  properties.insert(Region);
  properties.insert(CommunicationsTimeout);
  // properties.insert(ExpirationTimeRule);
  // properties.insert(SSLContextService);
  properties.insert(EndpointOverrideURL);
  properties.insert(ProxyHost);
  properties.insert(ProxyHostPort);
  setSupportedProperties(properties);
  // Set the supported relationships
  std::set<core::Relationship> relationships;
  relationships.insert(Failure);
  relationships.insert(Success);
  setSupportedRelationships(relationships);
}

void PutS3Object::onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) {

}

void PutS3Object::onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) {

}

void PutS3Object::notifyStop() {

}

}  // namespace processors
}  // namespace aws
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
