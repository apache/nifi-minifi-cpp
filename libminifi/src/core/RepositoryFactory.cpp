#include "core/Repository.h"
#include "core/Repository.h"

#ifdef LEVELDB_SUPPORT
#include "core/repository/FlowFileRepository.h"
#include "provenance/ProvenanceRepository.h"
#endif

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
#ifndef LEVELDB_SUPPORT
  namespace provenance{
  class ProvenanceRepository;
  }
#endif
namespace core {

#ifndef LEVELDB_SUPPORT
  class FlowFileRepository;
#endif


 std::shared_ptr<core::Repository> createRepository(
      const std::string configuration_class_name, bool fail_safe = false) {

    std::string class_name_lc = configuration_class_name;
    std::transform(class_name_lc.begin(), class_name_lc.end(),
                   class_name_lc.begin(), ::tolower);
    try {
      std::shared_ptr<core::Repository> return_obj = nullptr;
      if (class_name_lc == "flowfilerepository") {

        return_obj = std::shared_ptr<core::Repository>((core::Repository*)instantiate<core::repository::FlowFileRepository>());
      } else if (class_name_lc == "provenancerepository") {


	return_obj = std::shared_ptr<core::Repository>((core::Repository*)instantiate<provenance::ProvenanceRepository>());

      }
      
      if (return_obj){
        return return_obj;
      }
      if (fail_safe) {
        return std::make_shared<core::Repository>("fail_safe", "fail_safe", 1,
                                                  1, 1);
      } else {
        throw std::runtime_error(
            "Support for the provided configuration class could not be found");
      }
    } catch (const std::runtime_error &r) {
      if (fail_safe) {
        return std::make_shared<core::Repository>("fail_safe", "fail_safe", 1,
                                                  1, 1);
      }
    }

    throw std::runtime_error(
        "Support for the provided configuration class could not be found");
  }

  
} /* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
