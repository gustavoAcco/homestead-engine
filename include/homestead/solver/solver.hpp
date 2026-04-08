#pragma once

#include <homestead/solver/strategy.hpp>

namespace homestead {

/// Default solver: demand-driven backward expansion with Gauss-Seidel
/// fixed-point convergence for circular dependencies.
class BackpropagationSolver : public ISolverStrategy {
   public:
    PlanResult solve(std::span<const ProductionGoal> goals, const Registry& registry,
                     const SolverConfig& config) override;
};

/// Convenience free function using the default BackpropagationSolver.
/// Equivalent to BackpropagationSolver{}.solve(goals, registry, config).
PlanResult solve(std::span<const ProductionGoal> goals, const Registry& registry,
                 const SolverConfig& config = {});

}  // namespace homestead
