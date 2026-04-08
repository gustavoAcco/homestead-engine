// Minimal CLI harness for homestead-engine.
// Usage: homestead_cli --scenario <registry.json> --output <plan.json>
//
// Exit 0 on success, 1 on any error (message written to stderr).

#include <homestead/core/registry.hpp>
#include <homestead/serialization/serialization.hpp>
#include <homestead/solver/solver.hpp>

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <string>

static void usage() {
    std::cerr << "Usage:\n"
              << "  homestead_cli --scenario <registry.json> --output <plan.json>\n"
              << "  homestead_cli --defaults  --goals <goals.json> --output <plan.json>\n"
              << "  homestead_cli --dump-defaults --output <registry.json>\n";
}

int main(int argc, char* argv[]) {
    std::string scenario_path;
    std::string goals_path;
    std::string output_path;
    bool use_defaults = false;
    bool dump_defaults = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg{argv[i]};
        if (arg == "--scenario" && i + 1 < argc) {
            scenario_path = argv[++i];
        } else if (arg == "--goals" && i + 1 < argc) {
            goals_path = argv[++i];
        } else if (arg == "--output" && i + 1 < argc) {
            output_path = argv[++i];
        } else if (arg == "--defaults") {
            use_defaults = true;
        } else if (arg == "--dump-defaults") {
            dump_defaults = true;
        } else {
            usage();
            return 1;
        }
    }

    if (output_path.empty()) {
        usage();
        return 1;
    }

    // ── Dump default registry to JSON and exit ────────────────────────────────
    if (dump_defaults) {
        std::ofstream out{output_path};
        if (!out) {
            std::cerr << "Error: cannot open output file '" << output_path << "'\n";
            return 1;
        }
        out << homestead::to_json(homestead::Registry::load_defaults()).dump(2) << '\n';
        std::cout << "Default registry written to " << output_path << '\n';
        return 0;
    }

    // ── Load registry ──────────────────────────────────────────────────────────
    homestead::Registry registry = [&]() -> homestead::Registry {
        if (use_defaults) {
            return homestead::Registry::load_defaults();
        }
        return homestead::Registry{};
    }();

    nlohmann::json scenario_json = nlohmann::json::object();

    if (!use_defaults) {
        if (scenario_path.empty()) {
            usage();
            std::exit(1);
        }
        std::ifstream scenario_file{scenario_path};
        if (!scenario_file) {
            std::cerr << "Error: cannot open scenario file '" << scenario_path << "'\n";
            std::exit(1);
        }
        try {
            scenario_file >> scenario_json;
        } catch (const std::exception& ex) {
            std::cerr << "Error: failed to parse JSON: " << ex.what() << '\n';
            std::exit(1);
        }
        auto reg_result = homestead::registry_from_json(scenario_json);
        if (!reg_result) {
            std::cerr << "Error: " << reg_result.error() << '\n';
            std::exit(1);
        }
        registry = std::move(*reg_result);
    }

    // ── Load goals (from --goals file, scenario JSON, or inline) ─────────────
    std::vector<homestead::ProductionGoal> goals{};

    auto parse_goals = [&](const nlohmann::json& j) {
        if (j.contains("goals") && j["goals"].is_array()) {
            for (const auto& g : j["goals"]) {
                homestead::ProductionGoal goal;
                goal.resource_slug = g.value("resource_slug", std::string{});
                double qty = g.value("quantity_per_month", 0.0);
                goal.quantity_per_month = homestead::VariableQuantity{qty};
                goals.push_back(goal);
            }
        }
    };

    if (!goals_path.empty()) {
        std::ifstream gf{goals_path};
        if (!gf) {
            std::cerr << "Error: cannot open goals file '" << goals_path << "'\n";
            return 1;
        }
        nlohmann::json gj;
        try {
            gf >> gj;
        } catch (const std::exception& ex) {
            std::cerr << "Error parsing goals: " << ex.what() << '\n';
            return 1;
        }
        parse_goals(gj);
    } else {
        parse_goals(scenario_json);
    }

    // ── Solve ──────────────────────────────────────────────────────────────────
    homestead::SolverConfig config;
    auto plan = homestead::solve(goals, registry, config);

    // Report diagnostics to stderr.
    for (const auto& d : plan.diagnostics) {
        std::cerr << "[" << homestead::to_string(d.kind) << "] " << d.message << '\n';
    }

    // ── Write output ───────────────────────────────────────────────────────────
    std::ofstream out_file{output_path};
    if (!out_file) {
        std::cerr << "Error: cannot open output file '" << output_path << "'\n";
        return 1;
    }
    out_file << homestead::to_json(plan).dump(2) << '\n';

    std::cout << "Plan written to " << output_path << '\n';
    std::cout << "Loop closure score: " << plan.loop_closure_score << '\n';
    return 0;
}
