#ifndef ERROR_H_
#define ERROR_H_

#include <sstream>

namespace ccproducers {

class Error {
 public:
  Error() : cause_(nullptr), message_("") {}
  Error(Error&& other) : 
    cause_(std::move(other.cause_)), message_(std::move(other.message_)) {}
  Error(const Error* cause) : cause_(cause), message_("") {}
  Error(std::string message) : cause_(nullptr), message_(message) {}

  std::string ToString() const {
    std::stringstream stream;
    stream << "Producer error with message: " << std::endl 
           << message_ << std::endl;
    if (cause_ != nullptr) {
      stream << "Caused by: " << std::endl
             << cause_->ToString() << std::endl;
    }
    return stream.str();
  }

 private:
  const Error* cause_;
  std::string message_;
};

}  // namespace ccproducers

#endif  // ERROR_H
