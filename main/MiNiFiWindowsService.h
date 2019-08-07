#pragma once

#ifdef WIN32

#include <memory>
#include "core/Core.h"

void CheckRunAsService();
bool CreateServiceTerminationThread(std::shared_ptr<logging::Logger> logger);

#endif
