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

#include "DeleteS3Object.h"
#include "FetchS3Object.h"
#include "ListS3.h"
#include "PutS3Object.h"
#include "S3Processor.h"
#include "core/PropertyBuilder.h"
#include "core/Resource.h"
#include "utils/MapUtils.h"

// FIXME(fgerlits): we need to put all these static definitions in a single file so that they are executed in this order at runtime
// when https://issues.apache.org/jira/browse/MINIFICPP-1825 is closed, these definitions should be moved back to the cpp file of the class to which they belong

namespace org::apache::nifi::minifi::aws::processors {

// S3Processor

const std::set<std::string> S3Processor::REGIONS({region::AF_SOUTH_1, region::AP_EAST_1, region::AP_NORTHEAST_1,
  region::AP_NORTHEAST_2, region::AP_NORTHEAST_3, region::AP_SOUTH_1, region::AP_SOUTHEAST_1, region::AP_SOUTHEAST_2,
  region::AP_SOUTHEAST_3, region::CA_CENTRAL_1, region::CN_NORTH_1, region::CN_NORTHWEST_1, region::EU_CENTRAL_1,
  region::EU_NORTH_1, region::EU_SOUTH_1, region::EU_WEST_1, region::EU_WEST_2, region::EU_WEST_3, region::ME_CENTRAL_1,
  region::ME_SOUTH_1, region::SA_EAST_1, region::US_EAST_1, region::US_EAST_2, region::US_GOV_EAST_1, region::US_GOV_WEST_1,
  region::US_ISO_EAST_1, region::US_ISOB_EAST_1, region::US_ISO_WEST_1, region::US_WEST_1, region::US_WEST_2});

const core::Property S3Processor::Bucket(
  core::PropertyBuilder::createProperty("Bucket")
    ->withDescription("The S3 bucket")
    ->isRequired(true)
    ->supportsExpressionLanguage(true)
    ->build());
const core::Property S3Processor::AccessKey(
  core::PropertyBuilder::createProperty("Access Key")
    ->withDescription("AWS account access key")
    ->supportsExpressionLanguage(true)
    ->build());
const core::Property S3Processor::SecretKey(
  core::PropertyBuilder::createProperty("Secret Key")
    ->withDescription("AWS account secret key")
    ->supportsExpressionLanguage(true)
    ->build());
const core::Property S3Processor::CredentialsFile(
  core::PropertyBuilder::createProperty("Credentials File")
    ->withDescription("Path to a file containing AWS access key and secret key in properties file format. Properties used: accessKey and secretKey")
    ->build());
const core::Property S3Processor::AWSCredentialsProviderService(
  core::PropertyBuilder::createProperty("AWS Credentials Provider service")
    ->withDescription("The name of the AWS Credentials Provider controller service that is used to obtain AWS credentials.")
    ->build());
const core::Property S3Processor::Region(
  core::PropertyBuilder::createProperty("Region")
    ->isRequired(true)
    ->withDefaultValue<std::string>(region::US_WEST_2)
    ->withAllowableValues<std::string>(S3Processor::REGIONS)
    ->withDescription("AWS Region")
    ->build());
const core::Property S3Processor::CommunicationsTimeout(
  core::PropertyBuilder::createProperty("Communications Timeout")
    ->isRequired(true)
    ->withDefaultValue<core::TimePeriodValue>("30 sec")
    ->withDescription("Sets the timeout of the communication between the AWS server and the client")
    ->build());
const core::Property S3Processor::EndpointOverrideURL(
  core::PropertyBuilder::createProperty("Endpoint Override URL")
    ->withDescription("Endpoint URL to use instead of the AWS default including scheme, host, "
                      "port, and path. The AWS libraries select an endpoint URL based on the AWS "
                      "region, but this property overrides the selected endpoint URL, allowing use "
                      "with other S3-compatible endpoints.")
    ->supportsExpressionLanguage(true)
    ->build());
const core::Property S3Processor::ProxyHost(
  core::PropertyBuilder::createProperty("Proxy Host")
    ->withDescription("Proxy host name or IP")
    ->supportsExpressionLanguage(true)
    ->build());
const core::Property S3Processor::ProxyPort(
  core::PropertyBuilder::createProperty("Proxy Port")
    ->withDescription("The port number of the proxy host")
    ->supportsExpressionLanguage(true)
    ->build());
const core::Property S3Processor::ProxyUsername(
    core::PropertyBuilder::createProperty("Proxy Username")
    ->withDescription("Username to set when authenticating against proxy")
    ->supportsExpressionLanguage(true)
    ->build());
const core::Property S3Processor::ProxyPassword(
  core::PropertyBuilder::createProperty("Proxy Password")
    ->withDescription("Password to set when authenticating against proxy")
    ->supportsExpressionLanguage(true)
    ->build());
const core::Property S3Processor::UseDefaultCredentials(
    core::PropertyBuilder::createProperty("Use Default Credentials")
    ->withDescription("If true, uses the Default Credential chain, including EC2 instance profiles or roles, environment variables, default user credentials, etc.")
    ->withDefaultValue<bool>(false)
    ->isRequired(true)
    ->build());


// DeleteS3Object

const core::Property DeleteS3Object::ObjectKey(
  core::PropertyBuilder::createProperty("Object Key")
    ->withDescription("The key of the S3 object. If none is given the filename attribute will be used by default.")
    ->supportsExpressionLanguage(true)
    ->build());
const core::Property DeleteS3Object::Version(
  core::PropertyBuilder::createProperty("Version")
    ->withDescription("The Version of the Object to delete")
    ->supportsExpressionLanguage(true)
    ->build());

const core::Relationship DeleteS3Object::Success("success", "FlowFiles are routed to success relationship");
const core::Relationship DeleteS3Object::Failure("failure", "FlowFiles are routed to failure relationship");

REGISTER_RESOURCE(DeleteS3Object, Processor);


// FetchS3Object

const core::Property FetchS3Object::ObjectKey(
  core::PropertyBuilder::createProperty("Object Key")
    ->withDescription("The key of the S3 object. If none is given the filename attribute will be used by default.")
    ->supportsExpressionLanguage(true)
    ->build());
const core::Property FetchS3Object::Version(
  core::PropertyBuilder::createProperty("Version")
    ->withDescription("The Version of the Object to download")
    ->supportsExpressionLanguage(true)
    ->build());
const core::Property FetchS3Object::RequesterPays(
  core::PropertyBuilder::createProperty("Requester Pays")
    ->isRequired(true)
    ->withDefaultValue<bool>(false)
    ->withDescription("If true, indicates that the requester consents to pay any charges associated with retrieving "
                      "objects from the S3 bucket. This sets the 'x-amz-request-payer' header to 'requester'.")
    ->build());

const core::Relationship FetchS3Object::Success("success", "FlowFiles are routed to success relationship");
const core::Relationship FetchS3Object::Failure("failure", "FlowFiles are routed to failure relationship");

REGISTER_RESOURCE(FetchS3Object, Processor);


// ListS3

const core::Property ListS3::Delimiter(
  core::PropertyBuilder::createProperty("Delimiter")
    ->withDescription("The string used to delimit directories within the bucket. Please consult the AWS documentation for the correct use of this field.")
    ->build());
const core::Property ListS3::Prefix(
  core::PropertyBuilder::createProperty("Prefix")
    ->withDescription("The prefix used to filter the object list. In most cases, it should end with a forward slash ('/').")
    ->build());
const core::Property ListS3::UseVersions(
  core::PropertyBuilder::createProperty("Use Versions")
    ->isRequired(true)
    ->withDefaultValue<bool>(false)
    ->withDescription("Specifies whether to use S3 versions, if applicable. If false, only the latest version of each object will be returned.")
    ->build());
const core::Property ListS3::MinimumObjectAge(
  core::PropertyBuilder::createProperty("Minimum Object Age")
    ->isRequired(true)
    ->withDefaultValue<core::TimePeriodValue>("0 sec")
    ->withDescription("The minimum age that an S3 object must be in order to be considered; any object younger than this amount of time (according to last modification date) will be ignored.")
    ->build());
const core::Property ListS3::WriteObjectTags(
  core::PropertyBuilder::createProperty("Write Object Tags")
    ->isRequired(true)
    ->withDefaultValue<bool>(false)
    ->withDescription("If set to 'true', the tags associated with the S3 object will be written as FlowFile attributes.")
    ->build());
const core::Property ListS3::WriteUserMetadata(
  core::PropertyBuilder::createProperty("Write User Metadata")
    ->isRequired(true)
    ->withDefaultValue<bool>(false)
    ->withDescription("If set to 'true', the user defined metadata associated with the S3 object will be added to FlowFile attributes/records.")
    ->build());
const core::Property ListS3::RequesterPays(
  core::PropertyBuilder::createProperty("Requester Pays")
    ->isRequired(true)
    ->withDefaultValue<bool>(false)
    ->withDescription("If true, indicates that the requester consents to pay any charges associated with listing the S3 bucket. This sets the 'x-amz-request-payer' header to 'requester'. "
                      "Note that this setting is only used if Write User Metadata is true.")
    ->build());

const core::Relationship ListS3::Success("success", "FlowFiles are routed to success relationship");

REGISTER_RESOURCE(ListS3, Processor);


// PutS3Object

const std::set<std::string> PutS3Object::CANNED_ACLS(minifi::utils::MapUtils::getKeys(minifi::aws::s3::CANNED_ACL_MAP));
const std::set<std::string> PutS3Object::STORAGE_CLASSES(minifi::utils::MapUtils::getKeys(minifi::aws::s3::STORAGE_CLASS_MAP));
const std::set<std::string> PutS3Object::SERVER_SIDE_ENCRYPTIONS(minifi::utils::MapUtils::getKeys(minifi::aws::s3::SERVER_SIDE_ENCRYPTION_MAP));

const core::Property PutS3Object::ObjectKey(
  core::PropertyBuilder::createProperty("Object Key")
    ->withDescription("The key of the S3 object. If none is given the filename attribute will be used by default.")
    ->supportsExpressionLanguage(true)
    ->build());
const core::Property PutS3Object::ContentType(
  core::PropertyBuilder::createProperty("Content Type")
    ->withDescription("Sets the Content-Type HTTP header indicating the type of content stored in "
                      "the associated object. The value of this header is a standard MIME type. "
                      "If no content type is provided the default content type "
                      "\"application/octet-stream\" will be used.")
    ->supportsExpressionLanguage(true)
    ->withDefaultValue<std::string>("application/octet-stream")
    ->build());
const core::Property PutS3Object::StorageClass(
  core::PropertyBuilder::createProperty("Storage Class")
    ->isRequired(true)
    ->withDefaultValue<std::string>("Standard")
    ->withAllowableValues<std::string>(PutS3Object::STORAGE_CLASSES)
    ->withDescription("AWS S3 Storage Class")
    ->build());
const core::Property PutS3Object::FullControlUserList(
  core::PropertyBuilder::createProperty("FullControl User List")
    ->withDescription("A comma-separated list of Amazon User ID's or E-mail addresses that specifies who should have Full Control for an object.")
    ->supportsExpressionLanguage(true)
    ->build());
const core::Property PutS3Object::ReadPermissionUserList(
  core::PropertyBuilder::createProperty("Read Permission User List")
    ->withDescription("A comma-separated list of Amazon User ID's or E-mail addresses that specifies who should have Read Access for an object.")
    ->supportsExpressionLanguage(true)
    ->build());
const core::Property PutS3Object::ReadACLUserList(
  core::PropertyBuilder::createProperty("Read ACL User List")
    ->withDescription("A comma-separated list of Amazon User ID's or E-mail addresses that specifies who should have permissions to read "
                      "the Access Control List for an object.")
    ->supportsExpressionLanguage(true)
    ->build());
const core::Property PutS3Object::WriteACLUserList(
  core::PropertyBuilder::createProperty("Write ACL User List")
    ->withDescription("A comma-separated list of Amazon User ID's or E-mail addresses that specifies who should have permissions to change "
                      "the Access Control List for an object.")
    ->supportsExpressionLanguage(true)
    ->build());
const core::Property PutS3Object::CannedACL(
  core::PropertyBuilder::createProperty("Canned ACL")
    ->withDescription("Amazon Canned ACL for an object. Allowed values: BucketOwnerFullControl, BucketOwnerRead, AuthenticatedRead, "
                      "PublicReadWrite, PublicRead, Private, AwsExecRead; will be ignored if any other ACL/permission property is specified.")
    ->supportsExpressionLanguage(true)
    ->build());
const core::Property PutS3Object::ServerSideEncryption(
  core::PropertyBuilder::createProperty("Server Side Encryption")
    ->isRequired(true)
    ->withDefaultValue<std::string>("None")
    ->withAllowableValues<std::string>(PutS3Object::SERVER_SIDE_ENCRYPTIONS)
    ->withDescription("Specifies the algorithm used for server side encryption.")
    ->build());
const core::Property PutS3Object::UsePathStyleAccess(
    core::PropertyBuilder::createProperty("Use Path Style Access")
    ->withDescription("Path-style access can be enforced by setting this property to true. Set it to true if your endpoint does not support "
                      "virtual-hosted-style requests, only path-style requests.")
    ->withDefaultValue<bool>(false)
    ->isRequired(true)
    ->build());


const core::Relationship PutS3Object::Success("success", "FlowFiles are routed to success relationship");
const core::Relationship PutS3Object::Failure("failure", "FlowFiles are routed to failure relationship");

REGISTER_RESOURCE(PutS3Object, Processor);

}  // namespace org::apache::nifi::minifi::aws::processors
