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

#include <regex>
#include <memory>
#include <string>
#include <set>

#include "core/Processor.h"
#include "utils/FlowFileStore.h"
#include "utils/Enum.h"
#include "serialization/PayloadSerializer.h"


namespace org::apache::nifi::minifi::processors {

class DefragTextFlowFiles : public core::Processor {
 public:
  explicit DefragTextFlowFiles(const std::string& name,  const utils::Identifier& uuid = {})
      : Processor(name, uuid) {
    logger_ = logging::LoggerFactory<DefragTextFlowFiles>::getLogger();
  }
  EXTENSIONAPI static core::Relationship Self;
  EXTENSIONAPI static core::Relationship Success;
  EXTENSIONAPI static core::Relationship Failure;

  EXTENSIONAPI static core::Property Pattern;
  EXTENSIONAPI static core::Property PatternLoc;
  EXTENSIONAPI static core::Property MaxBufferAge;
  EXTENSIONAPI static core::Property MaxBufferSize;

  void initialize() override;
  void onSchedule(core::ProcessContext* context, core::ProcessSessionFactory* sessionFactory) override;
  void onTrigger(core::ProcessContext* context, core::ProcessSession* session) override;
  void restore(const std::shared_ptr<core::FlowFile>& flowFile) override;
  std::set<std::shared_ptr<core::Connectable>> getOutGoingConnections(const std::string &relationship) const override;

  SMART_ENUM(PatternLocation,
             (END_OF_MESSAGE, "End of Message"),
             (START_OF_MESSAGE, "Start of Message")
  )

  class LastPatternFinder : public InputStreamCallback {
   public:
    LastPatternFinder(const std::regex& pattern, PatternLocation pattern_location) : pattern_(pattern), pattern_location_(pattern_location) {}
    ~LastPatternFinder() override = default;
    int64_t process(const std::shared_ptr<io::BaseStream>& stream) override;

    bool foundPattern() const { return last_pattern_location.has_value(); }
    const std::optional<size_t>& getLastPatternPosition() const { return last_pattern_location; }

   protected:
    void searchContent(const std::string& content);
    const std::regex& pattern_;
    PatternLocation pattern_location_;
    std::optional<size_t> last_pattern_location;
  };

 protected:
  class Buffer {
   public:
    void append(core::ProcessSession* session, const std::shared_ptr<core::FlowFile>& flow_file_to_append);
    bool maxSizeReached() const;
    bool maxAgeReached() const;
    void setMaxAge(uint64_t max_age);
    void setMaxSize(size_t max_size);
    void flushAndReplace(core::ProcessSession* session, const core::Relationship& relationship,
                         const std::shared_ptr<core::FlowFile>& new_buffered_flow_file);

    bool canBeAppended(const std::shared_ptr<core::FlowFile>& flow_file_to_append) const;
    bool empty() const { return buffered_flow_file_ == nullptr; }

    void store(core::ProcessSession* session);

   private:
    std::shared_ptr<core::FlowFile> buffered_flow_file_;
    std::chrono::time_point<std::chrono::system_clock> creation_time_;
    std::optional<std::chrono::milliseconds> max_age_;
    std::optional<size_t> max_size_;
  };

  std::mutex defrag_mutex_;
  std::regex pattern_;
  PatternLocation pattern_location_;

  std::shared_ptr<logging::Logger> logger_;
  utils::FlowFileStore flow_file_store_;
  Buffer buffer_;

  void processNextFragment(core::ProcessSession *session, const std::shared_ptr<core::FlowFile> &original_flow_file);

  bool splitFlowFileAtLastPattern(core::ProcessSession *session,
                                  const std::shared_ptr<core::FlowFile> &original_flow_file,
                                  std::shared_ptr<core::FlowFile> &split_before_last_pattern,
                                  std::shared_ptr<core::FlowFile> &split_after_last_pattern) const;

  void updateAttributesForSplittedFiles(const std::shared_ptr<const core::FlowFile> &original_flow_file,
                                        const std::shared_ptr<core::FlowFile> &split_before_last_pattern,
                                        const std::shared_ptr<core::FlowFile> &split_after_last_pattern,
                                        const size_t split_position) const;
};


}  // namespace org::apache::nifi::minifi::processors
