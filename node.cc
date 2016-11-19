#include "node.h"

#include <assert.h>
#include <set>
#include <sstream>
#include <string>

namespace ccproducers {

NodeBase::NodeBase(int id, std::string name, std::set<NodeBase*> deps) :
    id_(id), name_(name), deps_(deps), rdeps_({}), state_(NodeState::BLOCKED), finished_deps_({}) {
  for (const auto& dep : deps) {
    dep->AddReverseDep(this);
  }
}

void NodeBase::Run() {
  assert(!IsDone());
  assert(CanRun());
  RunProducer();
  SetFinished();

  for (const auto& rdep : rdeps_) {
    rdep->ReportFinished(this);
  }
}

bool NodeBase::IsDone() const {
  std::lock_guard<std::recursive_mutex> lock(state_lock_);
  return (state_ == NodeState::FINISHED);
}

bool NodeBase::CanRun() const {
  std::lock_guard<std::recursive_mutex> lock(finished_deps_lock_);
  return (deps_.size() == finished_deps_.size());
}

bool NodeBase::TrySetRunning() {
  std::lock_guard<std::recursive_mutex> lock(state_lock_);
  if (state_ == NodeState::BLOCKED) {
    state_ = NodeState::RUNNING;
    return true;
  } else {
    return false;
  }
}

void NodeBase::Start() {
  if (!CanRun()) {
    std::cout << DebugPrefix() << "Can't run yet, ignoring" << std::endl;
    return;
  }

  bool running = TrySetRunning();
  if (!running) {
    std::cout << DebugPrefix() << "Called start but could not set to running" << std::endl;
    return;
  }

  std::cout << DebugPrefix() << "Kicking off async producer run" << std::endl;
  async_future_ = std::async(std::launch::async, &NodeBase::Run, this);
  std::cout << DebugPrefix() << "Starting node finished" << std::endl;
}

void NodeBase::SetFinished() {
  std::lock_guard<std::recursive_mutex> lock(state_lock_);
  assert(state_ == NodeState::RUNNING);
  state_ = NodeState::FINISHED;
  std::cout << DebugPrefix() << "Finished" << std::endl;
}

void NodeBase::AddReverseDep(NodeBase* rdep) {
  rdeps_.insert(rdep);
}

std::set<NodeBase*> NodeBase::TransitiveDeps() {
  std::set<NodeBase*> result;
  TransitiveDepsInternal(&result);
  return std::move(result);
}

void NodeBase::ReportFinished(NodeBase* node) {
  assert(deps_.find(node) != deps_.end());

  std::cout << DebugPrefix() << "Got report of finished node: " << node->name() << std::endl;

  std::lock_guard<std::recursive_mutex> lock(finished_deps_lock_);
  finished_deps_.insert(node);

  // This node could have become ready as a result of this call.
  if (CanRun()) {
    Start();
  }
}

std::string NodeBase::DebugPrefix() const {
  std::stringstream stream;
  stream << "["
    << "node=" << name() << ", "
    << "state=" << DebugState() << ", "
    << "deps=" << deps_.size() << ", "
    << "finished_deps=" << finished_deps_.size()
    << "] ";
  return stream.str();
}

std::string NodeBase::DebugState() const {
  std::lock_guard<std::recursive_mutex> lock(state_lock_);
  if (state_ == NodeState::BLOCKED) {
    return "blocked";
  } else if (state_ == NodeState::FINISHED) {
    return "finished";
  } else {
    return "running";
  }
}

void NodeBase::TransitiveDepsInternal(std::set<NodeBase*>* result) {
  if (result->find(this) != result->end()) {
    // Already visited. Abort.
    return;
  }
  result->insert(this);
  for (NodeBase* dep : deps_) {
    dep->TransitiveDepsInternal(result);
  }
}

}  // namespace ccproducers
