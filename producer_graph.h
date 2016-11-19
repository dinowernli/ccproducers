// Copyright 2016 Dino Wernli. All Rights Reserved. See LICENSE for licensing terms.

#ifndef PRODUCER_GRAPH_H_
#define PRODUCER_GRAPH_H_

#include <cassert>
#include <functional>
#include <future>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <vector>

#include "error.h"
#include "input.h"
#include "node.h"
#include "output.h"

namespace {

const char* kDefaultNodeNamePrefix = "unnamed";

std::string CreateNodeName(int id) {
  std::stringstream stream;
  stream << kDefaultNodeNamePrefix << "-" << id;
  return stream.str();
}

}  // namespace

namespace ccproducers {

// Contains a bunch of registered producers with their respective inputs and
// outputs wired up to each other.
class ProducerGraph {
 public:
  ProducerGraph() : next_id_(0) {}
  ~ProducerGraph() {}

  // Runs all the registered producers required to produce the supplied output.
  template<typename T>
  std::future<const T&> Execute(NodeHandle<T>* node_handle) {
    Node<T>* node = static_cast<Node<T>*>(nodes_by_id_[node_handle->NodeId()]);
    std::set<NodeBase*> allNodes = node->TransitiveDeps();
    for (NodeBase* node : allNodes) {
      node->Start();
    }
    return node->ResultFuture();
  }

  // Adds a producer to the graph with no arguments.
  template<typename ReturnType>
  NodeHandle<ReturnType>* AddProducer(std::function<Output<ReturnType>()> f) {
    return AddProducer("" /* name */, f);
  }

  template<typename ReturnType>
  NodeHandle<ReturnType>* AddProducer(
      std::string name, std::function<Output<ReturnType>()> f) {
    int id = next_id_++;
    if (name.empty()) {
      name = CreateNodeName(id);
    }

    auto result_handle = std::make_unique<NodeHandle<ReturnType>>(id);
    auto result = std::make_unique<Node<ReturnType>>(
        id,
        name,
        f,
        std::set<NodeBase*>{});
    node_handles_.push_back(std::move(result_handle));
    nodes_.push_back(std::move(result));

    auto node = nodes_.back().get();
    auto handle = node_handles_.back().get();

    nodes_by_id_[id] = node;
    assert(!node->IsDone());
    return static_cast<NodeHandle<ReturnType>*>(handle);
  }

  // Adds a producer with arguments to the graph.
  template<typename ReturnType, typename... Params>
  NodeHandle<ReturnType>* AddProducer(
      std::function<Output<ReturnType>(Input<Params>...)> f,
      NodeHandle<Params>*... node_handles) {
    return AddProducer("" /* name */, f, node_handles...);
  }

  template<typename ReturnType, typename... Params>
  NodeHandle<ReturnType>* AddProducer(
      std::string name,
      std::function<Output<ReturnType>(Input<Params>...)> f,
      NodeHandle<Params>*... node_handles) {

    auto other = BindRecursive(f, node_handles...);

    std::set<NodeBase*> nodes;
    std::vector<NodeHandleBase*> node_handle_bases = {node_handles...};
    for (const auto& handle : node_handle_bases) {
      int handle_node_id = handle->NodeId();
      auto handle_node = nodes_by_id_[handle_node_id];
      nodes.insert(handle_node);
    }

    int id = next_id_++;
    auto result_handle = std::make_unique<NodeHandle<ReturnType>>(id);
    auto result = std::make_unique<Node<ReturnType>>(
        id,
        name,
        other,
        nodes);
    node_handles_.push_back(std::move(result_handle));
    nodes_.push_back(std::move(result));

    auto node = nodes_.back().get();
    auto handle = node_handles_.back().get();
    nodes_by_id_[id] = node;
    assert(!node->IsDone());
    return static_cast<NodeHandle<ReturnType>*>(handle);
  }

  // Overload allowing us to accept function pointers. Just forwards to the
  // std::function-based implementation above.
  template<typename ReturnType, typename... Params>
  NodeHandle<ReturnType>* AddProducer(
      std::string name,
      Output<ReturnType> (*f)(Input<Params>...),
      NodeHandle<Params>*... nodes) {
    std::function<Output<ReturnType>(Input<Params>...)> function(f);
    return AddProducer<ReturnType, Params...>(name, function, nodes...);
  }

  template<typename ReturnType, typename... Params>
  NodeHandle<ReturnType>* AddProducer(
      Output<ReturnType> (*f)(Input<Params>...),
      NodeHandle<Params>*... nodes) {
    std::function<Output<ReturnType>(Input<Params>...)> function(f);
    return AddProducer<ReturnType, Params...>(function, nodes...);
  }

  // Overload allowing us to accept function pointers. Just forwards to the
  // std::function-based implementation above.
  template<typename ReturnType>
  NodeHandle<ReturnType>* AddProducer(std::string name, Output<ReturnType> (*f)()) {
    std::function<Output<ReturnType>()> function(f);
    return AddProducer<ReturnType>(name, function);
  }

  template<typename ReturnType>
  NodeHandle<ReturnType>* AddProducer(Output<ReturnType> (*f)()) {
    std::function<Output<ReturnType>()> function(f);
    return AddProducer<ReturnType>(function);
  }

 private:
  // Partial template speciliazation for the base case, i.e., the case where
  // there is only one node.
  template<typename ReturnType, typename P>
  std::function<Output<ReturnType>()> BindRecursive(
      std::function<Output<ReturnType>(Input<P>)> f,
      NodeHandle<P>* node_handle) {
    // Pass this pointer into the lambda below rather than a snapshot.
    std::map<int, NodeBase*>* nodes_by_id = &nodes_by_id_;
    int node_id = node_handle->NodeId();

    return [f, node_id, nodes_by_id]() {
      Node<P>* node = static_cast<Node<P>*>(nodes_by_id->at(node_id));
      return f(node->GetOutput()->AsInput());
    };
  }

  // General recursive case.
  template<typename ReturnType, typename P, typename... Tail>
  std::function<Output<ReturnType>()> BindRecursive(
      std::function<Output<ReturnType>(Input<P>, Input<Tail>...)> f,
      NodeHandle<P>* node_handle,
      NodeHandle<Tail>*... tail) {
    // Pass this pointer into the lambda below rather than a snapshot.
    std::map<int, NodeBase*>* nodes_by_id = &nodes_by_id_;
    int node_id = node_handle->NodeId();

    // Bind the result of the first output handle and call recursively.
    std::function<Output<ReturnType>(Input<Tail>...)> h =
        [f, node_id, nodes_by_id](Input<Tail>... tail) {
          Node<P>* node = static_cast<Node<P>*>(nodes_by_id->at(node_id));
          return f(node->GetOutput()->AsInput(), tail...);
        };
    return BindRecursive<ReturnType, Tail...>(h, tail...);
  }

  int next_id_;
  std::map<int, NodeBase*> nodes_by_id_;
  std::vector<std::unique_ptr<NodeBase>> nodes_;
  std::vector<std::unique_ptr<NodeHandleBase>> node_handles_;
};

}  // namespace ccproducers


#endif  // PRODUCER_GRAPH_H
