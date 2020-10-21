#include "WorkerThread.h"

namespace org { namespace apache { namespace nifi { namespace minifi { namespace extensions { namespace systemd {

namespace detail {
WorkerThread::WorkerThread()
    : thread_{&WorkerThread::run, this} {}

WorkerThread::~WorkerThread() {
  work_.stop();
  thread_.join();
}

void WorkerThread::run() noexcept {
  while (work_.isRunning()) {
    work_.consumeWait([](std::packaged_task<void()>&& f) { f(); });
  }
}
}  // namespace detail

}}}}}}  // namespace org::apache::nifi::minifi::extensions::systemd
