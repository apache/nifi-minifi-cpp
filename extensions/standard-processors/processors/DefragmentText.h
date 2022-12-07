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
#include "core/logging/LoggerConfiguration.h"
#include "utils/Enum.h"
#include "serialization/PayloadSerializer.h"
#include "utils/RegexUtils.h"

namespace org::apache::nifi::minifi::processors {

class DefragmentText : public core::Processor {
 public:
  explicit DefragmentText(std::string name,  const utils::Identifier& uuid = {})
      : Processor(std::move(name), uuid) {
  }

  EXTENSIONAPI static constexpr const char* Description = "DefragmentText splits and merges incoming flowfiles so cohesive messages are not split between them. "
      "It can handle multiple inputs differentiated by the absolute.path flow file attribute.";

  EXTENSIONAPI static const core::Property Pattern;
  EXTENSIONAPI static const core::Property PatternLoc;
  EXTENSIONAPI static const core::Property MaxBufferAge;
  EXTENSIONAPI static const core::Property MaxBufferSize;
  static auto properties() {
    return std::array{
      Pattern,
      PatternLoc,
      MaxBufferAge,
      MaxBufferSize
    };
  }

  EXTENSIONAPI static const core::Relationship Self;
  EXTENSIONAPI static const core::Relationship Success;
  EXTENSIONAPI static const core::Relationship Failure;
  static auto relationships() {
    return std::array{
      Success,
      Failure
    };
  }

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_ALLOWED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = true;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void initialize() override;
  void onSchedule(core::ProcessContext* context, core::ProcessSessionFactory* sessionFactory) override;
  void onTrigger(core::ProcessContext* context, core::ProcessSession* session) override;
  void restore(const std::shared_ptr<core::FlowFile>& flowFile) override;
  std::set<core::Connectable*> getOutGoingConnections(const std::string &relationship) override;

  SMART_ENUM(PatternLocation,
             (END_OF_MESSAGE, "End of Message"),
             (START_OF_MESSAGE, "Start of Message")
  )

 protected:
  class Buffer {
   public:
    void append(core::ProcessSession* session, const gsl::not_null<std::shared_ptr<core::FlowFile>>& flow_file_to_append);
    bool maxSizeReached(const std::optional<size_t> max_size) const;
    bool maxAgeReached(const std::optional<std::chrono::milliseconds> max_age) const;
    void flushAndReplace(core::ProcessSession* session, const core::Relationship& relationship,
                         const std::shared_ptr<core::FlowFile>& new_buffered_flow_file);

    bool empty() const { return buffered_flow_file_ == nullptr; }
    std::optional<size_t> getNextFragmentOffset() const;

   private:
    void store(core::ProcessSession* session, const std::shared_ptr<core::FlowFile>& new_buffered_flow_file);

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
  PatternLocation pattern_location_;
  std::optional<std::chrono::milliseconds> max_age_;
  std::optional<size_t> max_size_;

  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<DefragmentText>::getLogger(uuid_);
  core::FlowFileStore flow_file_store_;
  std::unordered_map<FragmentSource::Id, FragmentSource, FragmentSource::Id::hash> fragment_sources_;

  void processNextFragment(core::ProcessSession *session, const gsl::not_null<std::shared_ptr<core::FlowFile>>& next_fragment);

  bool splitFlowFileAtLastPattern(core::ProcessSession *session,
                                  const gsl::not_null<std::shared_ptr<core::FlowFile>> &original_flow_file,
                                  std::shared_ptr<core::FlowFile> &split_before_last_pattern,
                                  std::shared_ptr<core::FlowFile> &split_after_last_pattern) const;

  static void updateAttributesForSplitFiles(const core::FlowFile &original_flow_file,
                                            const std::shared_ptr<core::FlowFile> &split_before_last_pattern,
                                            const std::shared_ptr<core::FlowFile> &split_after_last_pattern,
                                            const size_t split_position);
};


}  // namespace org::apache::nifi::minifi::processors
