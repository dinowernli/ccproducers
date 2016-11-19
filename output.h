#ifndef OUTPUT_H_
#define OUTPUT_H_

#include <memory>

#include "error.h"
#include "value.h"

namespace ccproducers {

class OutputBase {
};

// Represents the (immutable) result of running a producer. Contains either a
// value or an error which occurred during execution.
template<class T>
class Output : public OutputBase {
 public:
  Output(T&& content)
      : value_(std::make_unique<Value<T>>(std::move(content))),
        error_(nullptr) {}
  Output(Error&& error)
      : value_(nullptr),
        error_(std::make_unique<Error>(std::move(error))) {}
  Output(Output<T>&& other)
      : value_(std::move(other.value_)),
        error_(std::move(other.error_)) {}
  ~Output() {}

  Output<T>& operator=(Output<T>&& other) {
    value_ = std::move(other.value_);
    error_ = std::move(other.error_);
    return *this;
  }

  bool IsError() const {
    return error_.get() != nullptr;
  }

  bool IsValue() const {
    return value_.get() != nullptr;
  }

  // This must only be called if IsValue() returns true.
  const T& get() const {
    assert(IsValue());
    return value_->get();
  }

  // Returns an Input instance which points to the result of this output.
  Input<T> AsInput() const {
    if (IsError()) {
      return Input<T>(error_.get());
    } else {
      return Input<T>(value_.get());
    }
  }

 private:
  // Exactly one of these two fields is set for any given instance.
  std::unique_ptr<Value<T>> value_;
  std::unique_ptr<Error> error_;
};

}  // namespace ccproducers

#endif  // OUTPUT_H
