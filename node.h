// Copyright 2016 Dino Wernli. All Rights Reserved. See LICENSE for licensing terms.

#ifndef NODE_H_
#define NODE_H_

#include <future>
#include <iostream>
#include <set>
#include <string>

#include "error.h"
#include "input.h"
#include "output.h"

namespace ccproducers {

// The various states a node can be in during execution of a graph.
enum class NodeState { BLOCKED, RUNNING, FINISHED };

// Common base type for all node handles. Carry things around like an id used
// for retrieval of the real node within a producer graph.
class NodeHandleBase{
 public:
  NodeHandleBase(int node_id) : node_id_(node_id) {}
  virtual ~NodeHandleBase() {}

  // The id of the node this handle points to.
  int NodeId() const {
    return node_id_;
  }

 private:
  int node_id_;
};

// A handle to a node with a specific output type.
template<class T>
class NodeHandle : public NodeHandleBase {
 public:
  NodeHandle(int node_id) : NodeHandleBase(node_id) {}
  virtual ~NodeHandle() {}
};

// Base type for all nodes in the graph. Exists mainly because "Node" has a
// template parameter and we need a way to store pointers to nodes regardless
// of their exact type parameters.
class NodeBase {
 public:
  NodeBase(int id, std::string name, std::set<NodeBase*> deps);

  const std::string& name() const { return name_; }

  void Run();
  bool IsDone() const;
  void SetFinished();
  void AddReverseDep(NodeBase* rdep);

  // Attempts to set the node's state to running. Returns true if the node
  // has transitioned to RUNNING as a result of this call.
  bool TrySetRunning();

  // Starts the execution of this node asynchronously. Does not block.
  // Eventually, this node's result promise will be fulfilled.
  void Start();

  // Returns the transitive set of nodes which need to run in order for this
  // node to have produced a result. In particular, the returned set contains
  // this node.
  std::set<NodeBase*> TransitiveDeps();

  // Informs this node that another node has finished execution. The supplied
  // node must be a dependency of this node.
  void ReportFinished(NodeBase* node);

  void DumpState() const {
    std::cout << DebugPrefix() << std::endl;
  }

 protected:
  virtual void RunProducer() = 0;
  void TransitiveDepsInternal(std::set<NodeBase*>* result);

  std::string DebugPrefix() const;
  std::string DebugState() const;

 private:
  bool CanRun() const;

  // The id of this node. Unique withing a producer graph.
  int id_;
  std::string name_;

  // Holds the future used to track the async producer run.
  std::future<void> async_future_;

  // Deps (need to run before) and rdeps (can only run after) of this node.
  // Don't need to be lock guarded because configuration happens before the
  // execution of the graph.
  std::set<NodeBase*> deps_;
  std::set<NodeBase*> rdeps_;

  // Holds the state of the node.
  NodeState state_;
  mutable std::recursive_mutex state_lock_;

  // Holds the list of rdeps which have completed.
  std::set<NodeBase*> finished_deps_;
  mutable std::recursive_mutex finished_deps_lock_;
};

// Represents a node in the graph with an output of a specific type.
template<class T>
class Node : public NodeBase {
 public:
  Node(
    int id,
    std::string name,
    std::function<Output<T>()> producer,
    std::set<NodeBase*> deps)
      : NodeBase(id, name, deps), producer_(producer) { }

  // Returns a future which gets resolved once the producer for this node has
  // been executed.
  std::future<const T&> ResultFuture() {
    return result_promise_.get_future();
  }

  // Returns nullptr until the producer of this node hass been executed.
  const Output<T>* GetOutput() {
    return result_.get();
  }

 protected:
  void RunProducer() {
    std::cout << DebugPrefix() << "Running producer" << std::endl;

    // Try running the producer, making sure we recover from any exceptions.
    try {
      result_ = std::make_unique<Output<T>>(std::move(producer_()));
    } catch (std::exception&) {
      result_ = std::make_unique<Output<T>>(
          Error("Exception while running producer"));
    }

    // Resolve the promise for the produced result.
    try {
      if (result_->IsError()) {
        throw std::runtime_error("Producer ran and produced an error");
      } else {
        result_promise_.set_value(result_->get());
      }
    } catch (std::exception&) {
      std::cout << DebugPrefix()
                << "Recovering from exception, setting failed" << std::endl;
      result_promise_.set_exception(std::current_exception());
    }

    std::cout << DebugPrefix() << "Running producer finished" << std::endl;
  }

 private:
  // This points to nullptr until RunProducer() is called.
  std::unique_ptr<Output<T>> result_;

  // A producer with all inputs bound to the results of other producers. This
  // must only be executed if all dependency producers have already been run.
  std::function<Output<T>()> producer_;

  // A promise for the result. This is resolved once the result_ field above
  // gets populated with a value or an error.
  std::promise<const T&> result_promise_;
};

}  // namespace ccproducers

#endif  // NODE_H

