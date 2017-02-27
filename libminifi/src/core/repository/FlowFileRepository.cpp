#include "core/repository/FlowFileRepository.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {
namespace repository {

void FlowFileRepository::run() {
  // threshold for purge
  uint64_t purgeThreshold = max_partition_bytes_ * 3 / 4;
  while (running_) {
    std::this_thread::sleep_for(std::chrono::milliseconds(purge_period_));
    uint64_t curTime = getTimeMillis();
    uint64_t size = repoSize();
    if (size >= purgeThreshold) {
      std::vector<std::string> purgeList;
      leveldb::Iterator* it = db_->NewIterator(leveldb::ReadOptions());

      for (it->SeekToFirst(); it->Valid(); it->Next()) {
        FlowFileEventRecord eventRead;
        std::string key = it->key().ToString();
        if (eventRead.DeSerialize((uint8_t *) it->value().data(),
                                  (int) it->value().size())) {
          if ((curTime - eventRead.getEventTime()) > max_partition_millis_)
            purgeList.push_back(key);
        } else {
          logger_->log_debug("NiFi %s retrieve event %s fail", name_,
                             key.c_str());
          purgeList.push_back(key);
        }
      }
      delete it;
      for (auto eventId : purgeList) {
        logger_->log_info("Repository Repo %s Purge %s", name_,
                          eventId.c_str());
        Delete(eventId);
      }
    }
    if (size > max_partition_bytes_)
      repo_full_ = true;
    else
      repo_full_ = false;
  }
  return;
}

void FlowFileRepository::loadFlowFileToConnections(std::map<std::string, Connection *> &connectionMap)
 {

  std::vector<std::string> purgeList;
  leveldb::Iterator* it = db_->NewIterator(
            leveldb::ReadOptions());

  for (it->SeekToFirst(); it->Valid(); it->Next())
  {
    FlowFileEventRecord eventRead;
    std::string key = it->key().ToString();
    if (eventRead.DeSerialize((uint8_t *) it->value().data(),
        (int) it->value().size()))
    {
      auto search = connectionMap->find(eventRead.getConnectionUuid());
      if (search != connectionMap->end())
      {
        // we find the connection for the persistent flowfile, create the flowfile and enqueue that
        FlowFileRecord *record = new FlowFileRecord(&eventRead);
        // set store to repo to true so that we do need to persistent again in enqueue
        record->setStoredToRepository(true);
        search->second->put(record);
      }
      else
      {
        if (eventRead.getContentFullPath().length() > 0)
        {
          std::remove(eventRead.getContentFullPath().c_str());
        }
        purgeList.push_back(key);
      }
    }
    else
    {
      purgeList.push_back(key);
    }
  }

  delete it;
  std::vector<std::string>::iterator itPurge;
  for (itPurge = purgeList.begin(); itPurge != purgeList.end();
            itPurge++)
  {
    std::string eventId = *itPurge;
    logger_->log_info("Repository Repo %s Purge %s",
                    RepositoryTypeStr[_type],
                    eventId.c_str());
    Delete(eventId);
  }
 +
  return;
 +}

} /* namespace repository */
} /* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
