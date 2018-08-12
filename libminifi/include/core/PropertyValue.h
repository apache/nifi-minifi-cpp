/*
 * PropertyValue.h
 *
 *  Created on: Nov 28, 2018
 *      Author: mparisi
 */

#ifndef LIBMINIFI_INCLUDE_CORE_TYPES_PROPERTYVALUE_H_
#define LIBMINIFI_INCLUDE_CORE_TYPES_PROPERTYVALUE_H_



#include "state/Value.h"
#include "PropertyValidation.h"
#include <typeindex>
#include "TypedValues.h"
namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {


class PropertyValue : public state::response::ValueNode {
 public:
  PropertyValue() : type_id(typeid(std::string)){
    value_ = nullptr;
    validator_ = StandardValidators::VALID;
  }

  void setValidator(const std::shared_ptr<PropertyValidator> &val) {
    validator_ = val;
  }

  std::shared_ptr<PropertyValidator> getValidator() const {
    return validator_;
  }

  ValidationResult validate(const std::string &subject) const {
    return validator_->validate(subject, getValue());
  }

  operator int64_t() const {
    auto cast = std::dynamic_pointer_cast<state::response::Int64Value>(value_);
    if (cast) {
      return cast->getValue();
    } else if (std::dynamic_pointer_cast<state::response::IntValue>(value_)) {
      return std::dynamic_pointer_cast<state::response::IntValue>(value_)->getValue();
    }
    throw std::runtime_error("Invalid conversion int64_t");
  }

  operator int() const {
    auto cast = std::dynamic_pointer_cast<state::response::IntValue>(value_);
    if (cast) {
      return cast->getValue();
    }
    throw std::runtime_error("Invalid conversion int ");
  }

  operator bool() const {
    auto cast = std::dynamic_pointer_cast<state::response::BoolValue>(value_);
    if (cast) {
      return cast->getValue();
    }
    throw std::runtime_error("Invalid conversion bool");
  }

  operator std::string() const {
     return to_string();
   }



  std::type_index getTypeInfo() const{
    return type_id;
  }
  /**
   * Define the representations and eventual storage relationships through
   * createValue
   */
  template<typename T>
  auto operator=(const T ref) -> typename std::enable_if<std::is_same<T, int >::value ||
  std::is_same<T, uint32_t >::value ||
  std::is_same<T, uint64_t >::value ||
  std::is_same<T, int64_t >::value ||
  std::is_same<T, bool >::value ||
  std::is_same<T, char* >::value ||
  std::is_same<T, const char* >::value ||
  std::is_same<T, std::string>::value,ValueNode&>::type {
    type_id = typeid(T);
    value_ = minifi::state::response::createValue(ref);
    return *this;
  }

  template<typename T>
    auto operator=(const std::string &ref) -> typename std::enable_if<
    std::is_same<T, DataSizeValue >::value,ValueNode&>::type {
      type_id = T::type_id;
      value_ = std::make_shared<T>(ref);
      return *this;
    }

 protected:
  std::type_index type_id;
  std::shared_ptr<PropertyValidator> validator_;
};

inline char const* conditional_conversion(const PropertyValue &v) {
  return v.getValue()->getStringValue().c_str();
}

} /* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */


#endif /* LIBMINIFI_INCLUDE_CORE_TYPES_PROPERTYVALUE_H_ */
