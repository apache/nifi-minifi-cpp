//
// Created by szaszm on 10/20/20.
//

#include "ConsumeJournald.h"

namespace org { namespace apache { namespace nifi { namespace minifi { namespace extensions { namespace systemd {

constexpr const char* ConsumeJournald::CURSOR_KEY;
const core::Relationship ConsumeJournald::Success("success", "Successfully consumed journal messages.");

}}}}}}  // namespace org::apache::nifi::minifi::extensions::systemd
