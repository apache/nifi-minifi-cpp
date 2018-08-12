/*
 * DateType.h
 *
 *  Created on: Nov 28, 2018
 *      Author: mparisi
 */

#ifndef LIBMINIFI_INCLUDE_CORE_TYPES_DATETYPE_H_
#define LIBMINIFI_INCLUDE_CORE_TYPES_DATETYPE_H_

#include "state/Value.h"
#include <typeindex>
namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {

class TransformableValue{
 public:
  TransformableValue(){
  }
};


class DataSizeValue : public TransformableValue,  public state::response::Int64Value {
 public:
  static const std::type_index type_id;

  explicit DataSizeValue(const std::string &sizeString)
      : state::response::Int64Value(0) {
    StringToInt<uint64_t>(sizeString, value);
    string_value = sizeString;
  }

  explicit DataSizeValue(uint64_t value)
      : state::response::Int64Value(value) {
  }

  // Convert String to Integer
  template<typename T>
  static bool StringToInt(const std::string &input, T &output) {
    if (input.size() == 0) {
      return false;
    }

    const char *cvalue = input.c_str();
    char *pEnd;
    auto ival = std::strtoll(cvalue, &pEnd, 0);

    if (pEnd[0] == '\0') {
      output = ival;
      return true;
    }

    while (*pEnd == ' ') {
      // Skip the space
      pEnd++;
    }

    char end0 = toupper(pEnd[0]);
    if (end0 == 'B') {
      output = ival;
      return true;
    } else if ((end0 == 'K') || (end0 == 'M') || (end0 == 'G') || (end0 == 'T') || (end0 == 'P')) {
      if (pEnd[1] == '\0') {
        unsigned long int multiplier = 1000;

        if ((end0 != 'K')) {
          multiplier *= 1000;
          if (end0 != 'M') {
            multiplier *= 1000;
            if (end0 != 'G') {
              multiplier *= 1000;
              if (end0 != 'T') {
                multiplier *= 1000;
              }
            }
          }
        }
        output = ival * multiplier;
        return true;

      } else if ((pEnd[1] == 'b' || pEnd[1] == 'B') && (pEnd[2] == '\0')) {

        unsigned long int multiplier = 1024;

        if ((end0 != 'K')) {
          multiplier *= 1024;
          if (end0 != 'M') {
            multiplier *= 1024;
            if (end0 != 'G') {
              multiplier *= 1024;
              if (end0 != 'T') {
                multiplier *= 1024;
              }
            }
          }
        }
        output = ival * multiplier;
        return true;
      }
    }

    return false;
  }
};

} /* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* LIBMINIFI_INCLUDE_CORE_TYPES_DATETYPE_H_ */
