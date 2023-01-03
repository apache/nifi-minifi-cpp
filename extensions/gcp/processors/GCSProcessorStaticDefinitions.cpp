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

#include "DeleteGCSObject.h"
#include "FetchGCSObject.h"
#include "../GCPAttributes.h"
#include "GCSProcessor.h"
#include "ListGCSBucket.h"
#include "PutGCSObject.h"
#include "../controllerservices/GCPCredentialsControllerService.h"
#include "core/PropertyBuilder.h"
#include "core/Resource.h"

// FIXME(fgerlits): we need to put all these static definitions in a single file so that they are executed in this order at runtime
// when https://issues.apache.org/jira/browse/MINIFICPP-1825 is closed, these definitions should be moved back to the cpp file of the class to which they belong

namespace org::apache::nifi::minifi::extensions::gcp {

// GCSProcessor

const core::Property GCSProcessor::GCPCredentials(
    core::PropertyBuilder::createProperty("GCP Credentials Provider Service")
        ->withDescription("The Controller Service used to obtain Google Cloud Platform credentials. Should be the name of a GCPCredentialsControllerService.")
        ->isRequired(true)
        ->asType<GCPCredentialsControllerService>()
        ->build());

const core::Property GCSProcessor::NumberOfRetries(
    core::PropertyBuilder::createProperty("Number of retries")
        ->withDescription("How many retry attempts should be made before routing to the failure relationship.")
        ->withDefaultValue<uint64_t>(6)
        ->isRequired(true)
        ->supportsExpressionLanguage(false)
        ->build());

const core::Property GCSProcessor::EndpointOverrideURL(
    core::PropertyBuilder::createProperty("Endpoint Override URL")
        ->withDescription("Overrides the default Google Cloud Storage endpoints")
        ->isRequired(false)
        ->supportsExpressionLanguage(true)
        ->build());


// DeleteGCSObject

const core::Property DeleteGCSObject::Bucket(
    core::PropertyBuilder::createProperty("Bucket")
        ->withDescription("Bucket of the object.")
        ->withDefaultValue("${gcs.bucket}")
        ->supportsExpressionLanguage(true)
        ->build());

const core::Property DeleteGCSObject::Key(
    core::PropertyBuilder::createProperty("Key")
        ->withDescription("Name of the object.")
        ->withDefaultValue("${filename}")
        ->supportsExpressionLanguage(true)
        ->build());

const core::Property DeleteGCSObject::ObjectGeneration(
    core::PropertyBuilder::createProperty("Object Generation")
        ->withDescription("The generation of the Object to download. If left empty, then it will download the latest generation.")
        ->supportsExpressionLanguage(true)
        ->build());

const core::Property DeleteGCSObject::EncryptionKey(
    core::PropertyBuilder::createProperty("Server Side Encryption Key")
        ->withDescription("The AES256 Encryption Key (encoded in base64) for server-side decryption of the object.")
        ->isRequired(false)
        ->supportsExpressionLanguage(true)
        ->build());

const core::Relationship DeleteGCSObject::Success("success", "FlowFiles are routed to this relationship after a successful Google Cloud Storage operation.");
const core::Relationship DeleteGCSObject::Failure("failure", "FlowFiles are routed to this relationship if the Google Cloud Storage operation fails.");

const core::OutputAttribute DeleteGCSObject::Message{GCS_STATUS_MESSAGE, { Failure }, "The status message received from google cloud."};
const core::OutputAttribute DeleteGCSObject::Reason{GCS_ERROR_REASON, { Failure }, "The description of the error occurred during operation."};
const core::OutputAttribute DeleteGCSObject::Domain{GCS_ERROR_DOMAIN, { Failure }, "The domain of the error occurred during operation."};

REGISTER_RESOURCE(DeleteGCSObject, Processor);


// FetchGCSObject

const core::Property FetchGCSObject::Bucket(
    core::PropertyBuilder::createProperty("Bucket")
        ->withDescription("Bucket of the object.")
        ->withDefaultValue("${gcs.bucket}")
        ->supportsExpressionLanguage(true)
        ->build());

const core::Property FetchGCSObject::Key(
    core::PropertyBuilder::createProperty("Key")
        ->withDescription("Name of the object.")
        ->withDefaultValue("${filename}")
        ->supportsExpressionLanguage(true)
        ->build());

const core::Property FetchGCSObject::ObjectGeneration(
    core::PropertyBuilder::createProperty("Object Generation")
        ->withDescription("The generation of the Object to download. If left empty, then it will download the latest generation.")
        ->supportsExpressionLanguage(true)
        ->build());

const core::Property FetchGCSObject::EncryptionKey(
    core::PropertyBuilder::createProperty("Server Side Encryption Key")
        ->withDescription("The AES256 Encryption Key (encoded in base64) for server-side decryption of the object.")
        ->isRequired(false)
        ->supportsExpressionLanguage(true)
        ->build());

const core::Relationship FetchGCSObject::Success("success", "FlowFiles are routed to this relationship after a successful Google Cloud Storage operation.");
const core::Relationship FetchGCSObject::Failure("failure", "FlowFiles are routed to this relationship if the Google Cloud Storage operation fails.");

const core::OutputAttribute FetchGCSObject::Message{GCS_STATUS_MESSAGE, { Failure }, "The status message received from google cloud."};
const core::OutputAttribute FetchGCSObject::Reason{GCS_ERROR_REASON, { Failure }, "The description of the error occurred during operation."};
const core::OutputAttribute FetchGCSObject::Domain{GCS_ERROR_DOMAIN, { Failure }, "The domain of the error occurred during operation."};

REGISTER_RESOURCE(FetchGCSObject, Processor);


// ListGCSBucket

const core::Property ListGCSBucket::Bucket(
    core::PropertyBuilder::createProperty("Bucket")
        ->withDescription("Bucket of the object.")
        ->isRequired(true)
        ->supportsExpressionLanguage(true)
        ->build());

const core::Property ListGCSBucket::ListAllVersions(
    core::PropertyBuilder::createProperty("List all versions")
        ->withDescription("Set this option to `true` to get all the previous versions separately.")
        ->withDefaultValue<bool>(false)
        ->build());

const core::Relationship ListGCSBucket::Success("success", "FlowFiles are routed to this relationship after a successful Google Cloud Storage operation.");

const core::OutputAttribute ListGCSBucket::BucketOutputAttribute{GCS_BUCKET_ATTR, { Success }, "Bucket of the object."};
const core::OutputAttribute ListGCSBucket::Key{GCS_OBJECT_NAME_ATTR, { Success }, "Name of the object."};
const core::OutputAttribute ListGCSBucket::Filename{"filename", { Success }, std::string{"Same as "} + GCS_OBJECT_NAME_ATTR};  // NOLINT
const core::OutputAttribute ListGCSBucket::Size{GCS_SIZE_ATTR, { Success }, "Size of the object."};
const core::OutputAttribute ListGCSBucket::Crc32c{GCS_CRC32C_ATTR, { Success }, "The CRC32C checksum of object's data, encoded in base64."};
const core::OutputAttribute ListGCSBucket::Md5{GCS_MD5_ATTR, { Success }, "The MD5 hash of the object's data, encoded in base64."};
const core::OutputAttribute ListGCSBucket::OwnerEntity{GCS_OWNER_ENTITY_ATTR, { Success }, "The owner entity, in the form \"user-emailAddress\"."};
const core::OutputAttribute ListGCSBucket::OwnerEntityId{GCS_OWNER_ENTITY_ID_ATTR, { Success }, "The ID for the entity."};
const core::OutputAttribute ListGCSBucket::ContentEncoding{GCS_CONTENT_ENCODING_ATTR, { Success }, "The content encoding of the object."};
const core::OutputAttribute ListGCSBucket::ContentLanguage{GCS_CONTENT_LANGUAGE_ATTR, { Success }, "The content language of the object."};
const core::OutputAttribute ListGCSBucket::ContentDisposition{GCS_CONTENT_DISPOSITION_ATTR, { Success }, "The data content disposition of the object."};
const core::OutputAttribute ListGCSBucket::MediaLink{GCS_MEDIA_LINK_ATTR, { Success }, "The media download link to the object."};
const core::OutputAttribute ListGCSBucket::SelfLink{GCS_SELF_LINK_ATTR, { Success }, "The link to this object."};
const core::OutputAttribute ListGCSBucket::Etag{GCS_ETAG_ATTR, { Success }, "The HTTP 1.1 Entity tag for the object."};
const core::OutputAttribute ListGCSBucket::GeneratedId{GCS_GENERATED_ID, { Success }, "The service-generated ID for the object."};
const core::OutputAttribute ListGCSBucket::Generation{GCS_GENERATION, { Success }, "The content generation of this object. Used for object versioning."};
const core::OutputAttribute ListGCSBucket::Metageneration{GCS_META_GENERATION, { Success }, "The metageneration of the object."};
const core::OutputAttribute ListGCSBucket::CreateTime{GCS_CREATE_TIME_ATTR, { Success }, "Unix timestamp of the object's creation in milliseconds."};
const core::OutputAttribute ListGCSBucket::UpdateTime{GCS_UPDATE_TIME_ATTR, { Success }, "Unix timestamp of the object's last modification in milliseconds."};
const core::OutputAttribute ListGCSBucket::DeleteTime{GCS_DELETE_TIME_ATTR, { Success }, "Unix timestamp of the object's deletion in milliseconds."};
const core::OutputAttribute ListGCSBucket::EncryptionAlgorithm{GCS_ENCRYPTION_ALGORITHM_ATTR, { Success }, "The algorithm used to encrypt the object."};
const core::OutputAttribute ListGCSBucket::EncryptionSha256{GCS_ENCRYPTION_SHA256_ATTR, { Success }, "The SHA256 hash of the key used to encrypt the object."};

REGISTER_RESOURCE(ListGCSBucket, Processor);


// PutGCSObject

const core::Property PutGCSObject::Bucket(
    core::PropertyBuilder::createProperty("Bucket")
        ->withDescription("Bucket of the object.")
        ->withDefaultValue("${gcs.bucket}")
        ->supportsExpressionLanguage(true)
        ->build());

const core::Property PutGCSObject::Key(
    core::PropertyBuilder::createProperty("Key")
        ->withDescription("Name of the object.")
        ->withDefaultValue("${filename}")
        ->supportsExpressionLanguage(true)
        ->build());

const core::Property PutGCSObject::ContentType(
    core::PropertyBuilder::createProperty("Content Type")
        ->withDescription("Content Type for the file, i.e. text/plain ")
        ->isRequired(false)
        ->withDefaultValue("${mime.type}")
        ->supportsExpressionLanguage(true)
        ->build());

const core::Property PutGCSObject::MD5Hash(
    core::PropertyBuilder::createProperty("MD5 Hash")
        ->withDescription("MD5 Hash (encoded in Base64) of the file for server-side validation.")
        ->isRequired(false)
        ->supportsExpressionLanguage(true)
        ->build());

const core::Property PutGCSObject::Crc32cChecksum(
    core::PropertyBuilder::createProperty("CRC32C Checksum")
        ->withDescription("CRC32C Checksum (encoded in Base64, big-Endian order) of the file for server-side validation.")
        ->isRequired(false)
        ->supportsExpressionLanguage(true)
        ->build());

const core::Property PutGCSObject::EncryptionKey(
    core::PropertyBuilder::createProperty("Server Side Encryption Key")
        ->withDescription("An AES256 Encryption Key (encoded in base64) for server-side encryption of the object.")
        ->isRequired(false)
        ->supportsExpressionLanguage(true)
        ->build());

const core::Property PutGCSObject::ObjectACL(
    core::PropertyBuilder::createProperty("Object ACL")
        ->withDescription("Access Control to be attached to the object uploaded. Not providing this will revert to bucket defaults.")
        ->isRequired(false)
        ->withAllowableValues(PredefinedAcl::values())
        ->build());

const core::Property PutGCSObject::OverwriteObject(
    core::PropertyBuilder::createProperty("Overwrite Object")
        ->withDescription("If false, the upload to GCS will succeed only if the object does not exist.")
        ->withDefaultValue<bool>(true)
        ->build());

const core::Relationship PutGCSObject::Success("success", "Files that have been successfully written to Google Cloud Storage are transferred to this relationship");
const core::Relationship PutGCSObject::Failure("failure", "Files that could not be written to Google Cloud Storage for some reason are transferred to this relationship");

const core::OutputAttribute PutGCSObject::Message{GCS_STATUS_MESSAGE, { Failure }, "The status message received from google cloud."};
const core::OutputAttribute PutGCSObject::Reason{GCS_ERROR_REASON, { Failure }, "The description of the error occurred during upload."};
const core::OutputAttribute PutGCSObject::Domain{GCS_ERROR_DOMAIN, { Failure }, "The domain of the error occurred during upload."};
const core::OutputAttribute PutGCSObject::BucketOutputAttribute{GCS_BUCKET_ATTR, { Success }, "Bucket of the object."};
const core::OutputAttribute PutGCSObject::KeyOutputAttribute{GCS_OBJECT_NAME_ATTR, { Success }, "Name of the object."};
const core::OutputAttribute PutGCSObject::Size{GCS_SIZE_ATTR, { Success }, "Size of the object."};
const core::OutputAttribute PutGCSObject::Crc32c{GCS_CRC32C_ATTR, { Success }, "The CRC32C checksum of object's data, encoded in base64."};
const core::OutputAttribute PutGCSObject::Md5{GCS_MD5_ATTR, { Success }, "The MD5 hash of the object's data, encoded in base64."};
const core::OutputAttribute PutGCSObject::OwnerEntity{GCS_OWNER_ENTITY_ATTR, { Success }, "The owner entity, in the form \"user-emailAddress\"."};
const core::OutputAttribute PutGCSObject::OwnerEntityId{GCS_OWNER_ENTITY_ID_ATTR, { Success }, "The ID for the entity."};
const core::OutputAttribute PutGCSObject::ContentEncoding{GCS_CONTENT_ENCODING_ATTR, { Success }, "The content encoding of the object."};
const core::OutputAttribute PutGCSObject::ContentLanguage{GCS_CONTENT_LANGUAGE_ATTR, { Success }, "The content language of the object."};
const core::OutputAttribute PutGCSObject::ContentDisposition{GCS_CONTENT_DISPOSITION_ATTR, { Success }, "The data content disposition of the object."};
const core::OutputAttribute PutGCSObject::MediaLink{GCS_MEDIA_LINK_ATTR, { Success }, "The media download link to the object."};
const core::OutputAttribute PutGCSObject::SelfLink{GCS_SELF_LINK_ATTR, { Success }, "The link to this object."};
const core::OutputAttribute PutGCSObject::Etag{GCS_ETAG_ATTR, { Success }, "The HTTP 1.1 Entity tag for the object."};
const core::OutputAttribute PutGCSObject::GeneratedId{GCS_GENERATED_ID, { Success }, "The service-generated ID for the object."};
const core::OutputAttribute PutGCSObject::Generation{GCS_GENERATION, { Success }, "The content generation of this object. Used for object versioning."};
const core::OutputAttribute PutGCSObject::Metageneration{GCS_META_GENERATION, { Success }, "The metageneration of the object."};
const core::OutputAttribute PutGCSObject::CreateTime{GCS_CREATE_TIME_ATTR, { Success }, "Unix timestamp of the object's creation in milliseconds."};
const core::OutputAttribute PutGCSObject::UpdateTime{GCS_UPDATE_TIME_ATTR, { Success }, "Unix timestamp of the object's last modification in milliseconds."};
const core::OutputAttribute PutGCSObject::DeleteTime{GCS_DELETE_TIME_ATTR, { Success }, "Unix timestamp of the object's deletion in milliseconds."};
const core::OutputAttribute PutGCSObject::EncryptionAlgorithm{GCS_ENCRYPTION_ALGORITHM_ATTR, { Success }, "The algorithm used to encrypt the object."};
const core::OutputAttribute PutGCSObject::EncryptionSha256{GCS_ENCRYPTION_SHA256_ATTR, { Success }, "The SHA256 hash of the key used to encrypt the object."};

REGISTER_RESOURCE(PutGCSObject, Processor);

}  // namespace org::apache::nifi::minifi::extensions::gcp
