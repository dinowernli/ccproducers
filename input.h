#ifndef INPUT_H_
#define INPUT_H_

#include "error.h"
#include "value.h"

namespace ccproducers {

// Represents a single (immutable) input to a producer. Contains either a value
// or an error.
template<class T>
class Input {
 public:
  Input(const Value<T>* value) : value_(value), error_(nullptr) {}
  Input(const Error* error) : value_(nullptr), error_(error) {}
  Input(const Input<T>& other) : value_(other.value_), error_(other.error_) {}
  ~Input() {}

  const T& get() const {
    if (IsError()) {
      throw std::runtime_error("hi");
    }
    return value_->get();
  }

  bool IsError() const {
    return error_ != nullptr;
  }

  bool IsValue() const {
    return value_ != nullptr;
  }

 private:
  // Exactly one of these fields are set (i.e., not nullptr).
  const Value<T>* value_;
  const Error* error_;
};

}  // namespace ccproducers

#endif  // INPUT_H
