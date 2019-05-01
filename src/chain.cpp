#include "chain.h"

std::shared_ptr<Milestone> Chain::GetChainHead() const {
    vmilestones_.front();
}
