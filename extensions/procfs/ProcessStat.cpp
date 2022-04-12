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

#include "ProcessStat.h"
#include <sstream>

namespace org::apache::nifi::minifi::extensions::procfs {

std::optional<ProcessStatData> ProcessStatData::parseProcessStatFile(std::istream& stat_file) {
  std::stringstream stat_stream;
  copy(std::istreambuf_iterator<char>(stat_file),
       std::istreambuf_iterator<char>(),
       std::ostreambuf_iterator<char>(stat_stream));

  ProcessStatData process_stat_data;
  stat_stream >> process_stat_data.pid_;

  std::string stat_stream_string = stat_stream.str();
  size_t comm_start = stat_stream_string.find_first_of('(');
  size_t comm_end = stat_stream_string.find_last_of(')');
  if (comm_start == std::string::npos  || comm_end == std::string::npos)
    return std::nullopt;
  process_stat_data.comm_ = stat_stream_string.substr(comm_start+1, comm_end-comm_start-1);
  stat_stream.seekg(comm_end+2);  // seek to the start of the next field (after the space that follows (<comm>))

  stat_stream >> process_stat_data.state_ >> process_stat_data.ppid_ >> process_stat_data.pgrp_ >> process_stat_data.session_ >> process_stat_data.tty_nr_ >> process_stat_data.tpgid_
              >> process_stat_data.flags_ >> process_stat_data.minflt_ >> process_stat_data.cminflt_ >> process_stat_data.majflt_ >> process_stat_data.cmajflt_ >> process_stat_data.utime_
              >> process_stat_data.stime_ >> process_stat_data.cutime_ >> process_stat_data.cstime_ >> process_stat_data.priority_ >> process_stat_data.nice_ >> process_stat_data.num_threads_
              >> process_stat_data.itrealvalue_ >> process_stat_data.starttime_ >> process_stat_data.vsize_ >> process_stat_data.rss_ >> process_stat_data.rsslim_ >> process_stat_data.startcode_
              >> process_stat_data.endcode_ >> process_stat_data.startstack_ >> process_stat_data.kstkesp_ >> process_stat_data.kstkeip_ >> process_stat_data.signal_ >> process_stat_data.blocked_
              >> process_stat_data.sigignore_ >> process_stat_data.sigcatch_ >> process_stat_data.wchan_ >> process_stat_data.nswap_ >> process_stat_data.cnswap_ >> process_stat_data.exit_signal_
              >> process_stat_data.processor_ >> process_stat_data.rt_priority_ >> process_stat_data.policy_ >> process_stat_data.delayacct_blkio_ticks_ >> process_stat_data.guest_time_
              >> process_stat_data.cguest_time_ >> process_stat_data.start_data_ >> process_stat_data.end_data_ >> process_stat_data.start_brk_ >> process_stat_data.arg_start_
              >> process_stat_data.arg_end_ >> process_stat_data.env_start_ >> process_stat_data.env_end_ >> process_stat_data.exit_code_;
  if (stat_stream.fail())
    return std::nullopt;

  return process_stat_data;
}
}  // namespace org::apache::nifi::minifi::extensions::procfs
