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

#include "FetchSFTP.h"
#include "ListSFTP.h"
#include "PutSFTP.h"
#include "SFTPProcessorBase.h"
#include "core/PropertyBuilder.h"
#include "core/Resource.h"

// FIXME(fgerlits): we need to put all these static definitions in a single file so that they are executed in this order at runtime
// when https://issues.apache.org/jira/browse/MINIFICPP-1825 is closed, these definitions should be moved back to the cpp file of the class to which they belong

namespace org::apache::nifi::minifi::processors {

// SFTPProcessorBase

const core::Property SFTPProcessorBase::Hostname(core::PropertyBuilder::createProperty("Hostname")
    ->withDescription("The fully qualified hostname or IP address of the remote system")
    ->isRequired(true)->supportsExpressionLanguage(true)->build());

const core::Property SFTPProcessorBase::Port(core::PropertyBuilder::createProperty("Port")
    ->withDescription("The port that the remote system is listening on for file transfers")
    ->isRequired(true)->supportsExpressionLanguage(true)->build());

const core::Property SFTPProcessorBase::Username(core::PropertyBuilder::createProperty("Username")
    ->withDescription("Username")
    ->isRequired(true)->supportsExpressionLanguage(true)->build());

const core::Property SFTPProcessorBase::Password(core::PropertyBuilder::createProperty("Password")
    ->withDescription("Password for the user account")
    ->isRequired(false)->supportsExpressionLanguage(true)->build());

const core::Property SFTPProcessorBase::PrivateKeyPath(core::PropertyBuilder::createProperty("Private Key Path")
    ->withDescription("The fully qualified path to the Private Key file")
    ->isRequired(false)->supportsExpressionLanguage(true)->build());

const core::Property SFTPProcessorBase::PrivateKeyPassphrase(core::PropertyBuilder::createProperty("Private Key Passphrase")
    ->withDescription("Password for the private key")
    ->isRequired(false)->supportsExpressionLanguage(true)->build());

const core::Property SFTPProcessorBase::StrictHostKeyChecking(core::PropertyBuilder::createProperty("Strict Host Key Checking")
    ->withDescription("Indicates whether or not strict enforcement of hosts keys should be applied")
    ->isRequired(true)->withDefaultValue<bool>(false)->build());

const core::Property SFTPProcessorBase::HostKeyFile(core::PropertyBuilder::createProperty("Host Key File")
    ->withDescription("If supplied, the given file will be used as the Host Key; otherwise, no use host key file will be used")
    ->isRequired(false)->build());

const core::Property SFTPProcessorBase::ConnectionTimeout(core::PropertyBuilder::createProperty("Connection Timeout")
    ->withDescription("Amount of time to wait before timing out while creating a connection")
    ->isRequired(true)->withDefaultValue<core::TimePeriodValue>("30 sec")->build());

const core::Property SFTPProcessorBase::DataTimeout(core::PropertyBuilder::createProperty("Data Timeout")
    ->withDescription("When transferring a file between the local and remote system, this value specifies how long is allowed to elapse without any data being transferred between systems")
    ->isRequired(true)->withDefaultValue<core::TimePeriodValue>("30 sec")->build());

const core::Property SFTPProcessorBase::SendKeepaliveOnTimeout(core::PropertyBuilder::createProperty("Send Keep Alive On Timeout")
    ->withDescription("Indicates whether or not to send a single Keep Alive message when SSH socket times out")
    ->isRequired(true)->withDefaultValue<bool>(true)->build());

const core::Property SFTPProcessorBase::ProxyType(core::PropertyBuilder::createProperty("Proxy Type")
    ->withDescription("Specifies the Proxy Configuration Controller Service to proxy network requests. If set, it supersedes proxy settings configured per component. "
                       "Supported proxies: HTTP + AuthN, SOCKS + AuthN")
    ->isRequired(false)
    ->withAllowableValues<std::string>({PROXY_TYPE_DIRECT,
                                        PROXY_TYPE_HTTP,
                                        PROXY_TYPE_SOCKS})
    ->withDefaultValue(PROXY_TYPE_DIRECT)->build());

const core::Property SFTPProcessorBase::ProxyHost(core::PropertyBuilder::createProperty("Proxy Host")
    ->withDescription("The fully qualified hostname or IP address of the proxy server")
    ->isRequired(false)->supportsExpressionLanguage(true)->build());

const core::Property SFTPProcessorBase::ProxyPort(core::PropertyBuilder::createProperty("Proxy Port")
    ->withDescription("The port of the proxy server")
    ->isRequired(false)->supportsExpressionLanguage(true)->build());

const core::Property SFTPProcessorBase::HttpProxyUsername(core::PropertyBuilder::createProperty("Http Proxy Username")
    ->withDescription("Http Proxy Username")
    ->isRequired(false)->supportsExpressionLanguage(true)->build());

const core::Property SFTPProcessorBase::HttpProxyPassword(core::PropertyBuilder::createProperty("Http Proxy Password")
    ->withDescription("Http Proxy Password")
    ->isRequired(false)->supportsExpressionLanguage(true)->build());


// FetchSFTP

const core::Property FetchSFTP::RemoteFile(core::PropertyBuilder::createProperty("Remote File")
    ->withDescription("The fully qualified filename on the remote system")
    ->isRequired(true)->supportsExpressionLanguage(true)->build());

const core::Property FetchSFTP::CompletionStrategy(
    core::PropertyBuilder::createProperty("Completion Strategy")
    ->withDescription("Specifies what to do with the original file on the server once it has been pulled into NiFi. "
                      "If the Completion Strategy fails, a warning will be logged but the data will still be transferred.")
    ->isRequired(true)
    ->withAllowableValues<std::string>({COMPLETION_STRATEGY_NONE, COMPLETION_STRATEGY_MOVE_FILE, COMPLETION_STRATEGY_DELETE_FILE})
    ->withDefaultValue(COMPLETION_STRATEGY_NONE)->build());

const core::Property FetchSFTP::MoveDestinationDirectory(core::PropertyBuilder::createProperty("Move Destination Directory")
    ->withDescription("The directory on the remote server to move the original file to once it has been ingested into NiFi. "
                      "This property is ignored unless the Completion Strategy is set to 'Move File'. "
                      "The specified directory must already exist on the remote system if 'Create Directory' is disabled, or the rename will fail.")
    ->isRequired(false)->supportsExpressionLanguage(true)->build());

const core::Property FetchSFTP::CreateDirectory(core::PropertyBuilder::createProperty("Create Directory")
    ->withDescription("Specifies whether or not the remote directory should be created if it does not exist.")
    ->isRequired(true)->withDefaultValue<bool>(false)->build());

const core::Property FetchSFTP::DisableDirectoryListing(core::PropertyBuilder::createProperty("Disable Directory Listing")
    ->withDescription("Control how 'Move Destination Directory' is created when 'Completion Strategy' is 'Move File' and 'Create Directory' is enabled. "
                      "If set to 'true', directory listing is not performed prior to create missing directories. "
                      "By default, this processor executes a directory listing command to see target directory existence before creating missing directories. "
                      "However, there are situations that you might need to disable the directory listing such as the following. "
                      "Directory listing might fail with some permission setups (e.g. chmod 100) on a directory. "
                      "Also, if any other SFTP client created the directory after this processor performed a listing and before a directory creation request by this processor is finished, "
                      "then an error is returned because the directory already exists.")
    ->isRequired(false)->withDefaultValue<bool>(false)->build());

const core::Property FetchSFTP::UseCompression(core::PropertyBuilder::createProperty("Use Compression")
    ->withDescription("Indicates whether or not ZLIB compression should be used when transferring files")
    ->isRequired(true)->withDefaultValue<bool>(false)->build());

const core::Relationship FetchSFTP::Success("success",
                                      "All FlowFiles that are received are routed to success");
const core::Relationship FetchSFTP::CommsFailure("comms.failure",
                                           "Any FlowFile that could not be fetched from the remote server due to a communications failure will be transferred to this Relationship.");
const core::Relationship FetchSFTP::NotFound("not.found",
                                       "Any FlowFile for which we receive a 'Not Found' message from the remote server will be transferred to this Relationship.");
const core::Relationship FetchSFTP::PermissionDenied("permission.denied",
                                               "Any FlowFile that could not be fetched from the remote server due to insufficient permissions will be transferred to this Relationship.");

REGISTER_RESOURCE(FetchSFTP, Processor);


// ListSFTP

const core::Property ListSFTP::ListingStrategy(core::PropertyBuilder::createProperty("Listing Strategy")
    ->withDescription("Specify how to determine new/updated entities. See each strategy descriptions for detail.")
    ->isRequired(true)
    ->withAllowableValues<std::string>({LISTING_STRATEGY_TRACKING_TIMESTAMPS, LISTING_STRATEGY_TRACKING_ENTITIES})
    ->withDefaultValue(LISTING_STRATEGY_TRACKING_TIMESTAMPS)->build());

const core::Property ListSFTP::RemotePath(core::PropertyBuilder::createProperty("Remote Path")
    ->withDescription("The fully qualified filename on the remote system")
    ->isRequired(false)->supportsExpressionLanguage(true)->build());

const core::Property ListSFTP::SearchRecursively(core::PropertyBuilder::createProperty("Search Recursively")
    ->withDescription("If true, will pull files from arbitrarily nested subdirectories; "
                      "otherwise, will not traverse subdirectories")
    ->isRequired(true)->withDefaultValue<bool>(false)->build());

const core::Property ListSFTP::FollowSymlink(core::PropertyBuilder::createProperty("Follow symlink")
    ->withDescription("If true, will pull even symbolic files and also nested symbolic subdirectories; "
                      "otherwise, will not read symbolic files and will not traverse symbolic link subdirectories")
    ->isRequired(true)->withDefaultValue<bool>(false)->build());

const core::Property ListSFTP::FileFilterRegex(core::PropertyBuilder::createProperty("File Filter Regex")
    ->withDescription("Provides a Java Regular Expression for filtering Filenames; "
                      "if a filter is supplied, only files whose names match that Regular Expression will be fetched")
    ->isRequired(false)->build());

const core::Property ListSFTP::PathFilterRegex(core::PropertyBuilder::createProperty("Path Filter Regex")
    ->withDescription("When Search Recursively is true, then only subdirectories whose path matches the given Regular Expression will be scanned")
    ->isRequired(false)->build());

const core::Property ListSFTP::IgnoreDottedFiles(core::PropertyBuilder::createProperty("Ignore Dotted Files")
    ->withDescription("If true, files whose names begin with a dot (\".\") will be ignored")
    ->isRequired(true)->withDefaultValue<bool>(true)->build());

const core::Property ListSFTP::TargetSystemTimestampPrecision(core::PropertyBuilder::createProperty("Target System Timestamp Precision")
    ->withDescription("Specify timestamp precision at the target system. "
                      "Since this processor uses timestamp of entities to decide which should be listed, "
                      "it is crucial to use the right timestamp precision.")
    ->isRequired(true)
    ->withAllowableValues<std::string>({TARGET_SYSTEM_TIMESTAMP_PRECISION_AUTO_DETECT,
                                            TARGET_SYSTEM_TIMESTAMP_PRECISION_MILLISECONDS,
                                            TARGET_SYSTEM_TIMESTAMP_PRECISION_SECONDS,
                                            TARGET_SYSTEM_TIMESTAMP_PRECISION_MINUTES})
    ->withDefaultValue(TARGET_SYSTEM_TIMESTAMP_PRECISION_AUTO_DETECT)->build());

const core::Property ListSFTP::EntityTrackingTimeWindow(core::PropertyBuilder::createProperty("Entity Tracking Time Window")
    ->withDescription("Specify how long this processor should track already-listed entities. "
                      "'Tracking Entities' strategy can pick any entity whose timestamp is inside the specified time window. "
                      "For example, if set to '30 minutes', any entity having timestamp in recent 30 minutes will be the listing target when this processor runs. "
                      "A listed entity is considered 'new/updated' and a FlowFile is emitted if one of following condition meets: "
                      "1. does not exist in the already-listed entities, "
                      "2. has newer timestamp than the cached entity, "
                      "3. has different size than the cached entity. "
                      "If a cached entity's timestamp becomes older than specified time window, that entity will be removed from the cached already-listed entities. "
                      "Used by 'Tracking Entities' strategy.")
    ->isRequired(false)->build());

const core::Property ListSFTP::EntityTrackingInitialListingTarget(core::PropertyBuilder::createProperty("Entity Tracking Initial Listing Target")
    ->withDescription("Specify how initial listing should be handled. Used by 'Tracking Entities' strategy.")
    ->withAllowableValues<std::string>({ENTITY_TRACKING_INITIAL_LISTING_TARGET_TRACKING_TIME_WINDOW,
                                            ENTITY_TRACKING_INITIAL_LISTING_TARGET_ALL_AVAILABLE})
    ->isRequired(false)->withDefaultValue(ENTITY_TRACKING_INITIAL_LISTING_TARGET_ALL_AVAILABLE)->build());

const core::Property ListSFTP::MinimumFileAge(core::PropertyBuilder::createProperty("Minimum File Age")
    ->withDescription("The minimum age that a file must be in order to be pulled; "
                      "any file younger than this amount of time (according to last modification date) will be ignored")
    ->isRequired(true)->withDefaultValue<core::TimePeriodValue>("0 sec")->build());

const core::Property ListSFTP::MaximumFileAge(core::PropertyBuilder::createProperty("Maximum File Age")
    ->withDescription("The maximum age that a file must be in order to be pulled; "
                      "any file older than this amount of time (according to last modification date) will be ignored")
    ->isRequired(false)->build());

const core::Property ListSFTP::MinimumFileSize(core::PropertyBuilder::createProperty("Minimum File Size")
    ->withDescription("The minimum size that a file must be in order to be pulled")
    ->isRequired(true)->withDefaultValue<core::DataSizeValue>("0 B")->build());

const core::Property ListSFTP::MaximumFileSize(core::PropertyBuilder::createProperty("Maximum File Size")
    ->withDescription("The maximum size that a file must be in order to be pulled")
    ->isRequired(false)->build());

const core::Relationship ListSFTP::Success("success", "All FlowFiles that are received are routed to success");

REGISTER_RESOURCE(ListSFTP, Processor);


// PutSFTP

const core::Property PutSFTP::RemotePath(core::PropertyBuilder::createProperty("Remote Path")
    ->withDescription("The path on the remote system from which to pull or push files")
    ->isRequired(false)->supportsExpressionLanguage(true)->build());

const core::Property PutSFTP::CreateDirectory(core::PropertyBuilder::createProperty("Create Directory")
    ->withDescription("Specifies whether or not the remote directory should be created if it does not exist.")
    ->isRequired(true)->withDefaultValue<bool>(false)->build());

const core::Property PutSFTP::DisableDirectoryListing(core::PropertyBuilder::createProperty("Disable Directory Listing")
    ->withDescription("If set to 'true', directory listing is not performed prior to create missing directories. "
                      "By default, this processor executes a directory listing command to see target directory existence before creating missing directories. "
                      "However, there are situations that you might need to disable the directory listing such as the following. "
                      "Directory listing might fail with some permission setups (e.g. chmod 100) on a directory. "
                      "Also, if any other SFTP client created the directory after this processor performed a listing and before a directory creation request by this processor is finished, "
                      "then an error is returned because the directory already exists.")
    ->isRequired(false)->withDefaultValue<bool>(false)->build());

const core::Property PutSFTP::BatchSize(core::PropertyBuilder::createProperty("Batch Size")
    ->withDescription("The maximum number of FlowFiles to send in a single connection")
    ->isRequired(true)->withDefaultValue<uint64_t>(500)->build());

const core::Property PutSFTP::ConflictResolution(core::PropertyBuilder::createProperty("Conflict Resolution")
    ->withDescription("Determines how to handle the problem of filename collisions")
    ->isRequired(true)
    ->withAllowableValues<std::string>({CONFLICT_RESOLUTION_REPLACE,
                                        CONFLICT_RESOLUTION_IGNORE,
                                        CONFLICT_RESOLUTION_RENAME,
                                        CONFLICT_RESOLUTION_REJECT,
                                        CONFLICT_RESOLUTION_FAIL,
                                        CONFLICT_RESOLUTION_NONE})
    ->withDefaultValue(CONFLICT_RESOLUTION_NONE)->build());

const core::Property PutSFTP::RejectZeroByte(core::PropertyBuilder::createProperty("Reject Zero-Byte Files")
    ->withDescription("Determines whether or not Zero-byte files should be rejected without attempting to transfer")
    ->isRequired(false)->withDefaultValue<bool>(true)->build());

const core::Property PutSFTP::DotRename(core::PropertyBuilder::createProperty("Dot Rename")
    ->withDescription("If true, then the filename of the sent file is prepended with a \".\" and then renamed back to the original once the file is completely sent. "
                      "Otherwise, there is no rename. This property is ignored if the Temporary Filename property is set.")
    ->isRequired(false)->withDefaultValue<bool>(true)->build());

const core::Property PutSFTP::TempFilename(core::PropertyBuilder::createProperty("Temporary Filename")
    ->withDescription("If set, the filename of the sent file will be equal to the value specified during the transfer and after successful completion will be renamed to the original filename. "
                      "If this value is set, the Dot Rename property is ignored.")
    ->isRequired(false)->supportsExpressionLanguage(true)->build());

const core::Property PutSFTP::LastModifiedTime(core::PropertyBuilder::createProperty("Last Modified Time")
    ->withDescription("The lastModifiedTime to assign to the file after transferring it. "
                      "If not set, the lastModifiedTime will not be changed. "
                      "Format must be yyyy-MM-dd'T'HH:mm:ssZ. "
                      "You may also use expression language such as ${file.lastModifiedTime}. "
                      "If the value is invalid, the processor will not be invalid but will fail to change lastModifiedTime of the file.")
    ->isRequired(false)->supportsExpressionLanguage(true)->build());

const core::Property PutSFTP::Permissions(core::PropertyBuilder::createProperty("Permissions")
    ->withDescription("The permissions to assign to the file after transferring it. "
                      "Format must be either UNIX rwxrwxrwx with a - in place of denied permissions (e.g. rw-r--r--) or an octal number (e.g. 644). "
                      "If not set, the permissions will not be changed. "
                      "You may also use expression language such as ${file.permissions}. "
                      "If the value is invalid, the processor will not be invalid but will fail to change permissions of the file.")
    ->isRequired(false)->supportsExpressionLanguage(true)->build());

const core::Property PutSFTP::RemoteOwner(core::PropertyBuilder::createProperty("Remote Owner")
    ->withDescription("Integer value representing the User ID to set on the file after transferring it. "
                      "If not set, the owner will not be set. You may also use expression language such as ${file.owner}. "
                      "If the value is invalid, the processor will not be invalid but will fail to change the owner of the file.")
    ->isRequired(false)->supportsExpressionLanguage(true)->build());

const core::Property PutSFTP::RemoteGroup(core::PropertyBuilder::createProperty("Remote Group")
    ->withDescription("Integer value representing the Group ID to set on the file after transferring it. "
                     "If not set, the group will not be set. You may also use expression language such as ${file.group}. "
                     "If the value is invalid, the processor will not be invalid but will fail to change the group of the file.")
    ->isRequired(false)->supportsExpressionLanguage(true)->build());

const core::Property PutSFTP::UseCompression(core::PropertyBuilder::createProperty("Use Compression")
    ->withDescription("Indicates whether or not ZLIB compression should be used when transferring files")
    ->isRequired(true)->withDefaultValue<bool>(false)->build());

const core::Relationship PutSFTP::Success("success", "FlowFiles that are successfully sent will be routed to success");
const core::Relationship PutSFTP::Reject("reject", "FlowFiles that were rejected by the destination system");
const core::Relationship PutSFTP::Failure("failure", "FlowFiles that failed to send to the remote system; failure is usually looped back to this processor");

REGISTER_RESOURCE(PutSFTP, Processor);

}  // namespace org::apache::nifi::minifi::processors
