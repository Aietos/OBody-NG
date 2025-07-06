#pragma once

#include "ActorTracker/ActorTracker.h"

ActorTracker::Registry ActorTracker::Registry::instance;

namespace ActorTracker {
    Registry& Registry::GetInstance() { return instance; }
}  // namespace ActorTracker
