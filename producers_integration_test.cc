#include <chrono>
#include <cstdio>
#include <functional>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "gtest/gtest.h"

#include "error.h"
#include "producer_graph.h"

using ccproducers::Error;
using ccproducers::Input;
using ccproducers::Output;

namespace {

struct Foo {
  Foo(int x_) : x(x_) {}
  int x;
};

Output<Foo> ProduceFoo() {
  return Foo(100);
}

Output<int> ErrorProducer() {
  return Error();
}

Output<int> ThrowingProducer() {
  throw std::runtime_error("ThrowingProducer says hi");
}

Output<std::vector<int>> ProduceNumbers(Input<Foo> foo, Input<std::string> str) {
  std::vector<int> result;
  result.push_back(foo.get().x);
  return Output<std::vector<int>>(std::move(result));
}

Output<std::string> ProduceString() {
  return std::string("Hello");
}

Output<int> ExpensiveProduceNumber() {
  std::this_thread::sleep_for (std::chrono::seconds(2));
  return 7;
}

Output<int> ProduceOtherNumber() {
  return 10;
}

Output<int> ExpensiveAdd(Input<int> left, Input<int> right) {
  std::this_thread::sleep_for (std::chrono::seconds(1));
  return left.get() + right.get();
}

Output<std::string> MessageForNumber(Input<int> number) {
  std::stringstream stream;
  stream << "Hello world, number: " << number.get();
  return std::string(stream.str());
}

Output<float> ProduceFloat() {
  return 1.1;
}

Output<int> ProduceInt(
    Input<float> f0, Input<float> f1, Input<float> f2, Input<float> f3) {
  return static_cast<int>(f0.get() + f1.get() + f2.get() + f3.get());
}

}  // anonymous namespace


TEST(ProducerGraphTest, BasicGraph) {
  ccproducers::ProducerGraph graph;
  auto left = graph.AddProducer(&ExpensiveProduceNumber);
  auto right = graph.AddProducer(&ProduceOtherNumber);
  auto sum = graph.AddProducer(&ExpensiveAdd, left, right);
  auto message = graph.AddProducer(&MessageForNumber, sum);

  auto result_future = graph.Execute(message);
  result_future.wait();
  
  EXPECT_EQ("Hello world, number: 17", result_future.get());
}

TEST(ProducerGraphTest, GraphWithMixedTypes) {
  ccproducers::ProducerGraph graph;
  auto g = graph.AddProducer(&ProduceString);
  auto f = graph.AddProducer(&ProduceFoo);
  auto h = graph.AddProducer(&ProduceNumbers, f, g);

  auto result_future = graph.Execute(h);
  result_future.wait();
  
  EXPECT_EQ(100, result_future.get()[0]);
}

TEST(ProducerGraphTest, ErrorGraph) {
  ccproducers::ProducerGraph graph;
  auto f = graph.AddProducer(&ErrorProducer);
  auto g = graph.AddProducer(&MessageForNumber, f);

  auto result_future = graph.Execute(g);
  result_future.wait();

  try {
    result_future.get();
    ADD_FAILURE() << "Should have thrown";
  } catch (std::exception&) {
    // Success case.
  }
}

TEST(ProducerGraphTest, ThrowingGraph) {
  ccproducers::ProducerGraph graph;
  auto f = graph.AddProducer(&ThrowingProducer);
  auto g = graph.AddProducer(&MessageForNumber, f);

  auto result_future = graph.Execute(g);
  result_future.wait();

  try {
    result_future.get();
    ADD_FAILURE() << "Should have thrown";
  } catch (std::exception&) {
    // Success case.
  }
}

TEST(ProducerGraphTest, NodeWithManyInputs) {
  ccproducers::ProducerGraph graph;
  auto f1 = graph.AddProducer(&ProduceFloat);
  auto f2 = graph.AddProducer(&ProduceFloat);
  auto f3 = graph.AddProducer(&ProduceFloat);
  auto f4 = graph.AddProducer(&ProduceFloat);
  auto dHandle = graph.AddProducer(&ProduceInt, f1, f2, f3, f4);

  auto result_future = graph.Execute(dHandle);
  result_future.wait();
  EXPECT_EQ(4, result_future.get());
}
