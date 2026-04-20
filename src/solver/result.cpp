#include <homestead/solver/result.hpp>

namespace homestead {

std::string to_string(DiagnosticKind kind) {
    switch (kind) {
        case DiagnosticKind::unsatisfied_goal:
            return "unsatisfied_goal";
        case DiagnosticKind::area_exceeded:
            return "area_exceeded";
        case DiagnosticKind::non_convergent_cycle:
            return "non_convergent_cycle";
        case DiagnosticKind::missing_producer:
            return "missing_producer";
        case DiagnosticKind::seasonality_gap:
            return "seasonality_gap";
        case DiagnosticKind::nutrient_deficit:
            return "nutrient_deficit";
        case DiagnosticKind::composition_gap:
            return "composition_gap";
    }
    return "unknown";
}

}  // namespace homestead
