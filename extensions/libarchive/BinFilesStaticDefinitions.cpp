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

#include "BinFiles.h"
#include "MergeContent.h"
#include "core/PropertyBuilder.h"
#include "core/Resource.h"

// FIXME(fgerlits): we need to put all these static definitions in a single file so that they are executed in this order at runtime
// when https://issues.apache.org/jira/browse/MINIFICPP-1825 is closed, these definitions should be moved back to the cpp file of the class to which they belong

namespace org::apache::nifi::minifi::processors {

// BinFiles

const core::Property BinFiles::MinSize(
    core::PropertyBuilder::createProperty("Minimum Group Size")
    ->withDescription("The minimum size of for the bundle")
    ->withDefaultValue<uint64_t>(0)->build());
const core::Property BinFiles::MaxSize(
    core::PropertyBuilder::createProperty("Maximum Group Size")
    ->withDescription("The maximum size for the bundle. If not specified, there is no maximum.")
    ->withType(core::StandardValidators::UNSIGNED_LONG_VALIDATOR)->build());
const core::Property BinFiles::MinEntries(
    core::PropertyBuilder::createProperty("Minimum Number of Entries")
    ->withDescription("The minimum number of files to include in a bundle")
    ->withDefaultValue<uint32_t>(1)->build());
const core::Property BinFiles::MaxEntries(
    core::PropertyBuilder::createProperty("Maximum Number of Entries")
    ->withDescription("The maximum number of files to include in a bundle. If not specified, there is no maximum.")
    ->withType(core::StandardValidators::UNSIGNED_INT_VALIDATOR)->build());
const core::Property BinFiles::MaxBinAge(
    core::PropertyBuilder::createProperty("Max Bin Age")
    ->withDescription("The maximum age of a Bin that will trigger a Bin to be complete. Expected format is <duration> <time unit>")
    ->withType(core::StandardValidators::TIME_PERIOD_VALIDATOR)->build());
const core::Property BinFiles::MaxBinCount(
    core::PropertyBuilder::createProperty("Maximum number of Bins")
    ->withDescription("Specifies the maximum number of bins that can be held in memory at any one time")
    ->withDefaultValue<uint32_t>(100)->build());
const core::Property BinFiles::BatchSize(
    core::PropertyBuilder::createProperty("Batch Size")
    ->withDescription("Maximum number of FlowFiles processed in a single session")
    ->withDefaultValue<uint32_t>(1)->build());

const core::Relationship BinFiles::Original("original", "The FlowFiles that were used to create the bundle");
const core::Relationship BinFiles::Failure("failure", "If the bundle cannot be created, all FlowFiles that would have been used to create the bundle will be transferred to failure");

REGISTER_RESOURCE(BinFiles, Processor);


// MergeContent

const core::Property MergeContent::MergeStrategy(
  core::PropertyBuilder::createProperty("Merge Strategy")
  ->withDescription("Defragment or Bin-Packing Algorithm")
  ->withAllowableValues<std::string>({merge_content_options::MERGE_STRATEGY_DEFRAGMENT, merge_content_options::MERGE_STRATEGY_BIN_PACK})
  ->withDefaultValue(merge_content_options::MERGE_STRATEGY_BIN_PACK)->build());
const core::Property MergeContent::MergeFormat(
  core::PropertyBuilder::createProperty("Merge Format")
  ->withDescription("Merge Format")
  ->withAllowableValues<std::string>({
      merge_content_options::MERGE_FORMAT_CONCAT_VALUE,
      merge_content_options::MERGE_FORMAT_TAR_VALUE,
      merge_content_options::MERGE_FORMAT_ZIP_VALUE,
      merge_content_options::MERGE_FORMAT_FLOWFILE_STREAM_V3_VALUE})
  ->withDefaultValue(merge_content_options::MERGE_FORMAT_CONCAT_VALUE)->build());
const core::Property MergeContent::CorrelationAttributeName("Correlation Attribute Name", "Correlation Attribute Name", "");
const core::Property MergeContent::DelimiterStrategy(
  core::PropertyBuilder::createProperty("Delimiter Strategy")
  ->withDescription("Determines if Header, Footer, and Demarcator should point to files")
  ->withAllowableValues<std::string>({merge_content_options::DELIMITER_STRATEGY_FILENAME, merge_content_options::DELIMITER_STRATEGY_TEXT})
  ->withDefaultValue(merge_content_options::DELIMITER_STRATEGY_FILENAME)->build());
const core::Property MergeContent::Header("Header File", "Filename specifying the header to use", "");
const core::Property MergeContent::Footer("Footer File", "Filename specifying the footer to use", "");
const core::Property MergeContent::Demarcator("Demarcator File", "Filename specifying the demarcator to use", "");
const core::Property MergeContent::KeepPath(
  core::PropertyBuilder::createProperty("Keep Path")
  ->withDescription("If using the Zip or Tar Merge Format, specifies whether or not the FlowFiles' paths should be included in their entry")
  ->withDefaultValue(false)->build());
const core::Property MergeContent::AttributeStrategy(
  core::PropertyBuilder::createProperty("Attribute Strategy")
  ->withDescription("Determines which FlowFile attributes should be added to the bundle. If 'Keep All Unique Attributes' is selected, "
                    "any attribute on any FlowFile that gets bundled will be kept unless its value conflicts with the value from another FlowFile "
                    "(in which case neither, or none, of the conflicting attributes will be kept). If 'Keep Only Common Attributes' is selected, "
                    "only the attributes that exist on all FlowFiles in the bundle, with the same value, will be preserved.")
  ->withAllowableValues<std::string>({merge_content_options::ATTRIBUTE_STRATEGY_KEEP_COMMON, merge_content_options::ATTRIBUTE_STRATEGY_KEEP_ALL_UNIQUE})
  ->withDefaultValue(merge_content_options::ATTRIBUTE_STRATEGY_KEEP_COMMON)->build());

const core::Relationship MergeContent::Merge("merged", "The FlowFile containing the merged content");

REGISTER_RESOURCE(MergeContent, Processor);

}  // namespace org::apache::nifi::minifi::processors
