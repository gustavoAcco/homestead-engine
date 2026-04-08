#pragma once

#include <homestead/core/registry.hpp>
#include <homestead/solver/config.hpp>
#include <homestead/solver/result.hpp>

#include <span>

namespace homestead {

/// Abstract solver interface. Implement this to provide alternative planning
/// algorithms; the default implementation is BackpropagationSolver.
class ISolverStrategy {
   public:
    ISolverStrategy() = default;
    ISolverStrategy(const ISolverStrategy&) = delete;
    ISolverStrategy& operator=(const ISolverStrategy&) = delete;
    ISolverStrategy(ISolverStrategy&&) = delete;
    ISolverStrategy& operator=(ISolverStrategy&&) = delete;
    virtual ~ISolverStrategy() = default;

    /// Produce a PlanResult from a list of goals, a registry, and solver config.
    /// Never throws. Always returns a valid PlanResult; use diagnostics to
    /// inspect warnings or constraint violations.
    virtual PlanResult solve(std::span<const ProductionGoal> goals, const Registry& registry,
                             const SolverConfig& config) = 0;
};

}  // namespace homestead
