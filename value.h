#ifndef VALUE_H_
#define VALUE_H_

namespace ccproducers {

// Holds an immutable value.
template<class T>
class Value {
 public:
  Value(T&& content) : content_(std::move(content)) {}
  ~Value() {}

  const T& get() const {
    return content_;
  }

 private:
  T content_;
};

}  // namespace ccproducers

#endif // VALUE_H
