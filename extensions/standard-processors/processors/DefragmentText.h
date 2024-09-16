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

#pragma once

#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>

#include "core/Processor.h"
#include "core/FlowFileStore.h"
#include "core/logging/Logger.h"
#include "core/logging/LoggerFactory.h"
#include "core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "utils/Enum.h"
#include "serialization/PayloadSerializer.h"
#include "utils/RegexUtils.h"

namespace org::apache::nifi::minifi::processors::defragment_text {
enum class PatternLocation {
    END_OF_MESSAGE,
    START_OF_MESSAGE
};
}  // namespace org::apache::nifi::minifi::processors::defragment_text

namespace magic_enum::customize {
using PatternLocation = org::apache::nifi::minifi::processors::defragment_text::PatternLocation;

template <>
constexpr customize_t enum_name<PatternLocation>(PatternLocation value) noexcept {
  switch (value) {
    case PatternLocation::END_OF_MESSAGE:
      return "End of Message";
    case PatternLocation::START_OF_MESSAGE:
      return "Start of Message";
  }
  return invalid_tag;
}
}  // namespace magic_enum::customize

namespace org::apache::nifi::minifi::processors {

class DefragmentText : public core::ProcessorImpl {
 public:
  explicit DefragmentText(std::string_view name,  const utils::Identifier& uuid = {})
      : ProcessorImpl(name, uuid) {
  }

  EXTENSIONAPI static constexpr const char* Description = "DefragmentText splits and merges incoming flowfiles so cohesive messages are not split between them. "
      "It can handle multiple inputs differentiated by the absolute.path flow file attribute.";

  EXTENSIONAPI static constexpr auto Pattern = core::PropertyDefinitionBuilder<>::createProperty("Pattern")
      .withDescription("A regular expression to match at the start or end of messages.")
      .isRequired(true)
      .build();
  EXTENSIONAPI static constexpr auto PatternLoc = core::PropertyDefinitionBuilder<magic_enum::enum_count<defragment_text::PatternLocation>()>::createProperty("Pattern Location")
      .withDescription("Whether the pattern is located at the start or at the end of the messages.")
      .withDefaultValue(magic_enum::enum_name(defragment_text::PatternLocation::START_OF_MESSAGE))
      .withAllowedValues(magic_enum::enum_names<defragment_text::PatternLocation>())
      .isRequired(true)
      .build();
  EXTENSIONAPI static constexpr auto MaxBufferAge = core::PropertyDefinitionBuilder<>::createProperty("Max Buffer Age")
      .withDescription("The maximum age of the buffer after which it will be transferred to success when matching Start of Message patterns or to failure when matching End of Message patterns. "
          "Expected format is <duration> <time unit>")
      .withPropertyType(core::StandardPropertyTypes::TIME_PERIOD_TYPE)
      .withDefaultValue("10 min")
      .build();
  EXTENSIONAPI static constexpr auto MaxBufferSize = core::PropertyDefinitionBuilder<>::createProperty("Max Buffer Size")
      .withDescription("The maximum buffer size, if the buffer exceeds this, it will be transferred to failure. Expected format is <size> <data unit>")
      .withPropertyType(core::StandardPropertyTypes::DATA_SIZE_TYPE)
      .build();
  EXTENSIONAPI static constexpr auto Properties = std::to_array<core::PropertyReference>({
      Pattern,
      PatternLoc,
      MaxBufferAge,
      MaxBufferSize
  });


  EXTENSIONAPI static constexpr auto Success = core::RelationshipDefinition{"success", "Flowfiles that have been successfully defragmented"};
  EXTENSIONAPI static constexpr auto Failure = core::RelationshipDefinition{"failure", "Flowfiles that failed the defragmentation process"};
  EXTENSIONAPI static constexpr auto Relationships = std::array{
      Success,
      Failure
  };

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_ALLOWED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = true;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  EXTENSIONAPI static const core::Relationship Self;

  void initialize() override;
  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) override;
  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;
  void restore(const std::shared_ptr<core::FlowFile>& flowFile) override;
  std::set<core::Connectable*> getOutGoingConnections(const std::string &relationship) override;

 protected:
  class Buffer {
   public:
    void append(core::ProcessSession& session, const gsl::not_null<std::shared_ptr<core::FlowFile>>& flow_file_to_append);
    bool maxSizeReached(const std::optional<size_t> max_size) const;
    bool maxAgeReached(const std::optional<std::chrono::milliseconds> max_age) const;
    void flushAndReplace(core::ProcessSession& session, const core::Relationship& relationship,
                         const std::shared_ptr<core::FlowFile>& new_buffered_flow_file);

    bool empty() const { return buffered_flow_file_ == nullptr; }
    std::optional<size_t> getNextFragmentOffset() const;

   private:
    void store(core::ProcessSession& session, const std::shared_ptr<core::FlowFile>& new_buffered_flow_file);

    std::shared_ptr<core::FlowFile> buffered_flow_file_;
    std::chrono::steady_clock::time_point creation_time_;
  };

  struct FragmentSource {
    class Id {
     public:
      explicit Id(const core::FlowFile& flow_file);
      struct hash {
        size_t operator()(const Id& fragment_id) const;
      };
      bool operator==(const Id& rhs) const = default;
     protected:
      std::optional<std::string> absolute_path_;
    };

    Buffer buffer;
  };


  utils::Regex pattern_;
  defragment_text::PatternLocation pattern_location_;
  std::optional<std::chrono::milliseconds> max_age_;
  std::optional<size_t> max_size_;

  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<DefragmentText>::getLogger(uuid_);
  core::FlowFileStore flow_file_store_;
  std::unordered_map<FragmentSource::Id, FragmentSource, FragmentSource::Id::hash> fragment_sources_;

  void processNextFragment(core::ProcessSession& session, const gsl::not_null<std::shared_ptr<core::FlowFile>>& next_fragment);

  bool splitFlowFileAtLastPattern(core::ProcessSession& session,
                                  const gsl::not_null<std::shared_ptr<core::FlowFile>> &original_flow_file,
                                  std::shared_ptr<core::FlowFile> &split_before_last_pattern,
                                  std::shared_ptr<core::FlowFile> &split_after_last_pattern) const;

  static void updateAttributesForSplitFiles(const core::FlowFile &original_flow_file,
                                            const std::shared_ptr<core::FlowFile> &split_before_last_pattern,
                                            const std::shared_ptr<core::FlowFile> &split_after_last_pattern,
                                            const size_t split_position);
};


}  // namespace org::apache::nifi::minifi::processors
