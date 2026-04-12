#include <homestead/serialization/schema_version.hpp>
#include <homestead/serialization/serialization.hpp>

#include <format>

using json = nlohmann::json;

namespace homestead {

// ── SchemaVersion helpers ──────────────────────────────────────────────────────

std::string SchemaVersion::to_string() const {
    return std::format("{}.{}.{}", major, minor, patch);
}

std::expected<SchemaVersion, std::string> SchemaVersion::parse(std::string_view sv) {
    SchemaVersion v;
    // Expect "N.N.N"
    auto s = std::string{sv};
    try {
        std::size_t pos1 = s.find('.');
        if (pos1 == std::string::npos) {
            return std::unexpected("Missing first '.' in version");
        }
        std::size_t pos2 = s.find('.', pos1 + 1);
        if (pos2 == std::string::npos) {
            return std::unexpected("Missing second '.' in version");
        }
        v.major = std::stoi(s.substr(0, pos1));
        v.minor = std::stoi(s.substr(pos1 + 1, pos2 - pos1 - 1));
        v.patch = std::stoi(s.substr(pos2 + 1));
    } catch (...) {
        return std::unexpected(std::format("Cannot parse version string '{}'", sv));
    }
    return v;
}

// ── Low-level serialization helpers ───────────────────────────────────────────

namespace {

json vq_to_json(const VariableQuantity& q) {
    return json::array({q.min, q.expected, q.max});
}

std::expected<VariableQuantity, std::string> vq_from_json(const json& j, std::string_view field) {
    if (!j.is_array() || j.size() != 3) {
        return std::unexpected(
            std::format("Field '{}': expected [min, expected, max] array", field));
    }
    if (!j[0].is_number() || !j[1].is_number() || !j[2].is_number()) {
        return std::unexpected(
            std::format("Field '{}': all three elements must be numbers", field));
    }
    auto result =
        VariableQuantity::make(j[0].get<double>(), j[1].get<double>(), j[2].get<double>());
    if (!result) {
        return std::unexpected(std::format("Field '{}': {}", field, result.error()));
    }
    return *result;
}

json resource_to_json(const Resource& r) {
    json j;
    j["slug"] = r.slug;
    j["name"] = r.name;
    j["category"] = static_cast<int>(r.category);
    j["composition"] = r.composition;
    j["physical"]["weight_kg_per_unit"] = r.physical.weight_kg_per_unit;
    j["physical"]["volume_liters_per_unit"] = r.physical.volume_liters_per_unit;
    j["physical"]["shelf_life_days"] = r.physical.shelf_life_days;
    if (r.nutrition) {
        const auto& n = *r.nutrition;
        j["nutrition"]["calories_kcal_per_kg"] = n.calories_kcal_per_kg;
        j["nutrition"]["protein_g_per_kg"] = n.protein_g_per_kg;
        j["nutrition"]["fat_g_per_kg"] = n.fat_g_per_kg;
        j["nutrition"]["carbs_g_per_kg"] = n.carbs_g_per_kg;
        j["nutrition"]["micronutrients"] = n.micronutrients;
    }
    return j;
}

std::expected<Resource, std::string> resource_from_json(const json& j) {
    Resource r;
    if (!j.contains("slug") || !j["slug"].is_string()) {
        return std::unexpected("Resource missing 'slug' field");
    }
    r.slug = j["slug"].get<std::string>();

    if (!j.contains("name") || !j["name"].is_string()) {
        return std::unexpected(std::format("Resource '{}' missing 'name'", r.slug));
    }
    r.name = j["name"].get<std::string>();

    if (!j.contains("category") || !j["category"].is_number_integer()) {
        return std::unexpected(std::format("Resource '{}' missing 'category'", r.slug));
    }
    r.category = static_cast<ResourceCategory>(j["category"].get<int>());

    if (j.contains("composition") && j["composition"].is_object()) {
        for (const auto& [k, v] : j["composition"].items()) {
            r.composition[k] = v.get<double>();
        }
    }

    if (j.contains("physical")) {
        const auto& p = j["physical"];
        r.physical.weight_kg_per_unit = p.value("weight_kg_per_unit", 1.0);
        r.physical.volume_liters_per_unit = p.value("volume_liters_per_unit", 0.0);
        r.physical.shelf_life_days = p.value("shelf_life_days", -1);
    }

    if (j.contains("nutrition") && j["nutrition"].is_object()) {
        NutritionalProfile n;
        const auto& np = j["nutrition"];
        n.calories_kcal_per_kg = np.value("calories_kcal_per_kg", 0.0);
        n.protein_g_per_kg = np.value("protein_g_per_kg", 0.0);
        n.fat_g_per_kg = np.value("fat_g_per_kg", 0.0);
        n.carbs_g_per_kg = np.value("carbs_g_per_kg", 0.0);
        if (np.contains("micronutrients") && np["micronutrients"].is_object()) {
            for (const auto& [k, v] : np["micronutrients"].items()) {
                n.micronutrients[k] = v.get<double>();
            }
        }
        r.nutrition = n;
    }
    return r;
}

json rfs_to_json(const ResourceFlowSpec& s) {
    return json{{"resource_slug", s.resource_slug},
                {"quantity_per_cycle", vq_to_json(s.quantity_per_cycle)},
                {"timing", s.timing_within_cycle}};
}

std::expected<ResourceFlowSpec, std::string> rfs_from_json(const json& j, std::string_view ctx) {
    if (!j.contains("resource_slug") || !j["resource_slug"].is_string()) {
        return std::unexpected(std::format("{}: missing 'resource_slug'", ctx));
    }
    ResourceFlowSpec s;
    s.resource_slug = j["resource_slug"].get<std::string>();
    auto qty =
        vq_from_json(j.value("quantity_per_cycle", json::array({0, 0, 0})), "quantity_per_cycle");
    if (!qty) {
        return std::unexpected(qty.error());
    }
    s.quantity_per_cycle = *qty;
    s.timing_within_cycle = j.value("timing", 0.0);
    return s;
}

json entity_to_json(const Entity& e) {
    json j;
    j["slug"] = e.slug;
    j["name"] = e.name;
    j["description"] = e.description;
    j["lifecycle"]["setup_days"] = e.lifecycle.setup_days;
    j["lifecycle"]["cycle_days"] = e.lifecycle.cycle_days;
    j["lifecycle"]["cycles_per_year"] = e.lifecycle.cycles_per_year;
    j["lifecycle"]["active_months"] = e.lifecycle.active_months;
    j["operating_labor"] = vq_to_json(e.operating_labor_per_cycle);
    j["stocking_density"] = vq_to_json(e.stocking_density);
    j["infrastructure"]["area_m2"] = vq_to_json(e.infrastructure.area_m2);
    j["infrastructure"]["initial_cost"] = vq_to_json(e.infrastructure.initial_cost);
    j["infrastructure"]["initial_labor"] = vq_to_json(e.infrastructure.initial_labor_hours);
    for (const auto& inp : e.inputs) {
        j["inputs"].push_back(rfs_to_json(inp));
    }
    for (const auto& out : e.outputs) {
        j["outputs"].push_back(rfs_to_json(out));
    }
    for (const auto& m : e.infrastructure.construction_materials) {
        j["infrastructure"]["materials"].push_back(rfs_to_json(m));
    }
    if (e.nutrient_demand) {
        const auto& nd = *e.nutrient_demand;
        j["nutrient_demand"]["n_g_per_m2_per_cycle"] = nd.n_g_per_m2_per_cycle;
        j["nutrient_demand"]["p_g_per_m2_per_cycle"] = nd.p_g_per_m2_per_cycle;
        j["nutrient_demand"]["k_g_per_m2_per_cycle"] = nd.k_g_per_m2_per_cycle;
    }
    return j;
}

std::expected<Entity, std::string> entity_from_json(const json& j) {
    Entity e;
    if (!j.contains("slug") || !j["slug"].is_string()) {
        return std::unexpected("Entity missing 'slug'");
    }
    e.slug = j["slug"].get<std::string>();

    if (!j.contains("name") || !j["name"].is_string()) {
        return std::unexpected(std::format("Entity '{}' missing 'name'", e.slug));
    }
    e.name = j["name"].get<std::string>();
    e.description = j.value("description", std::string{});

    if (j.contains("lifecycle")) {
        const auto& lc = j["lifecycle"];
        e.lifecycle.setup_days = lc.value("setup_days", 0);
        e.lifecycle.cycle_days = lc.value("cycle_days", 1);
        e.lifecycle.cycles_per_year = lc.value("cycles_per_year", 1.0);
        e.lifecycle.active_months = lc.value("active_months", static_cast<MonthMask>(ALL_MONTHS));
    }

    auto parse_vq = [&](const json& jj,
                        std::string_view fn) -> std::expected<VariableQuantity, std::string> {
        if (!jj.contains(fn)) {
            return VariableQuantity{0.0};
        }
        return vq_from_json(jj[fn], fn);
    };

    auto ol = parse_vq(j, "operating_labor");
    if (!ol) {
        return std::unexpected(ol.error());
    }
    e.operating_labor_per_cycle = *ol;

    auto sd = parse_vq(j, "stocking_density");
    if (!sd) {
        return std::unexpected(sd.error());
    }
    e.stocking_density = *sd;

    if (j.contains("infrastructure")) {
        const auto& inf = j["infrastructure"];
        auto area = parse_vq(inf, "area_m2");
        if (!area) {
            return std::unexpected(area.error());
        }
        e.infrastructure.area_m2 = *area;

        auto cost = parse_vq(inf, "initial_cost");
        if (!cost) {
            return std::unexpected(cost.error());
        }
        e.infrastructure.initial_cost = *cost;

        auto labor = parse_vq(inf, "initial_labor");
        if (!labor) {
            return std::unexpected(labor.error());
        }
        e.infrastructure.initial_labor_hours = *labor;

        if (inf.contains("materials") && inf["materials"].is_array()) {
            for (const auto& m : inf["materials"]) {
                auto s = rfs_from_json(m, "construction_material");
                if (!s) {
                    return std::unexpected(s.error());
                }
                e.infrastructure.construction_materials.push_back(*s);
            }
        }
    }

    auto parse_flows = [&](const json& arr, std::vector<ResourceFlowSpec>& out,
                           std::string_view tag) -> std::expected<void, std::string> {
        if (!arr.is_array()) {
            return {};
        }
        for (const auto& item : arr) {
            auto s = rfs_from_json(item, tag);
            if (!s) {
                return std::unexpected(s.error());
            }
            out.push_back(*s);
        }
        return {};
    };

    if (j.contains("inputs")) {
        auto r = parse_flows(j["inputs"], e.inputs, "input");
        if (!r) {
            return std::unexpected(r.error());
        }
    }
    if (j.contains("outputs")) {
        auto r = parse_flows(j["outputs"], e.outputs, "output");
        if (!r) {
            return std::unexpected(r.error());
        }
    }
    if (j.contains("nutrient_demand") && j["nutrient_demand"].is_object()) {
        NutrientDemand nd;
        const auto& ndj = j["nutrient_demand"];
        nd.n_g_per_m2_per_cycle = ndj.value("n_g_per_m2_per_cycle", 0.0);
        nd.p_g_per_m2_per_cycle = ndj.value("p_g_per_m2_per_cycle", 0.0);
        nd.k_g_per_m2_per_cycle = ndj.value("k_g_per_m2_per_cycle", 0.0);
        e.nutrient_demand = nd;
    }
    return e;
}

// ── Schema version check helper ──────────────────────────────────────────────

std::expected<void, std::string> check_version(const json& j, const SchemaVersion& supported,
                                               std::string_view doc_type) {
    if (!j.contains("version")) {
        return std::unexpected(std::format("{}: missing 'version' field", doc_type));
    }
    if (!j["version"].is_string()) {
        return std::unexpected(std::format("{}: 'version' must be a string", doc_type));
    }

    auto ver = SchemaVersion::parse(j["version"].get<std::string>());
    if (!ver) {
        return std::unexpected(std::format("{}: {}", doc_type, ver.error()));
    }

    if (!ver->compatible_with(supported)) {
        return std::unexpected(
            std::format("{}: incompatible schema version {} (supported major={})", doc_type,
                        ver->to_string(), supported.major));
    }
    return {};
}

}  // anonymous namespace

// ── Registry ──────────────────────────────────────────────────────────────────

json to_json(const Registry& registry) {
    json data;
    data["resources"] = json::array();
    data["entities"] = json::array();
    for (const auto& r : registry.resources()) {
        data["resources"].push_back(resource_to_json(r));
    }
    for (const auto& e : registry.entities()) {
        data["entities"].push_back(entity_to_json(e));
    }

    return json{
        {"version", REGISTRY_SCHEMA_VERSION.to_string()}, {"type", "registry"}, {"data", data}};
}

std::expected<Registry, std::string> registry_from_json(const json& j) {
    if (auto v = check_version(j, REGISTRY_SCHEMA_VERSION, "registry"); !v) {
        return std::unexpected(v.error());
    }

    if (!j.contains("data")) {
        return std::unexpected("registry: missing 'data' field");
    }
    const auto& data = j["data"];

    Registry reg;

    if (data.contains("resources") && data["resources"].is_array()) {
        for (const auto& rj : data["resources"]) {
            auto r = resource_from_json(rj);
            if (!r) {
                return std::unexpected(r.error());
            }
            auto result = reg.register_resource(*r);
            if (!result) {
                return std::unexpected(result.error().message);
            }
        }
    }

    if (data.contains("entities") && data["entities"].is_array()) {
        for (const auto& ej : data["entities"]) {
            auto e = entity_from_json(ej);
            if (!e) {
                return std::unexpected(e.error());
            }
            auto result = reg.register_entity(*e);
            if (!result) {
                return std::unexpected(result.error().message);
            }
        }
    }

    return reg;
}

// ── Graph ─────────────────────────────────────────────────────────────────────

json to_json(const Graph& graph) {
    json nodes_arr = json::array();
    json edges_arr = json::array();

    for (NodeId nid = 0;; ++nid) {
        auto nv = graph.get_node(nid);
        if (!nv) {
            if (nid >= graph.node_count() + 100) {
                break;
            }
            continue;
        }
        json nj;
        std::visit(
            [&nj](const auto& n) {
                using T = std::decay_t<decltype(n)>;
                nj["id"] = n.id;
                if constexpr (std::is_same_v<T, EntityInstanceNode>) {
                    nj["kind"] = "entity_instance";
                    nj["instance_name"] = n.instance_name;
                    nj["entity_slug"] = n.entity_slug;
                    nj["quantity"] =
                        json::array({n.quantity.min, n.quantity.expected, n.quantity.max});
                    nj["schedule"] = n.schedule;
                } else if constexpr (std::is_same_v<T, ExternalSourceNode>) {
                    nj["kind"] = "external_source";
                    nj["resource_slug"] = n.resource_slug;
                    nj["cost_per_unit"] = n.cost_per_unit;
                } else {
                    nj["kind"] = "goal_sink";
                    nj["resource_slug"] = n.resource_slug;
                    nj["quantity_per_month"] =
                        json::array({n.quantity_per_month.min, n.quantity_per_month.expected,
                                     n.quantity_per_month.max});
                }
            },
            *nv);
        nodes_arr.push_back(nj);

        for (NodeId succ : graph.successors(nid)) {
            json ej;
            ej["from"] = nid;
            ej["to"] = succ;
            // resource_slug and quantity are stored in edges; we emit a minimal edge.
            edges_arr.push_back(ej);
        }
    }

    return json{{"version", GRAPH_SCHEMA_VERSION.to_string()},
                {"type", "graph"},
                {"data", {{"nodes", nodes_arr}, {"edges", edges_arr}}}};
}

std::expected<Graph, std::string> graph_from_json(const json& j) {
    if (auto v = check_version(j, GRAPH_SCHEMA_VERSION, "graph"); !v) {
        return std::unexpected(v.error());
    }

    if (!j.contains("data")) {
        return std::unexpected("graph: missing 'data' field");
    }

    const auto& data = j["data"];
    Graph g;

    if (data.contains("nodes") && data["nodes"].is_array()) {
        for (const auto& nj : data["nodes"]) {
            if (!nj.contains("kind") || !nj["kind"].is_string()) {
                return std::unexpected("graph node missing 'kind'");
            }
            std::string kind = nj["kind"].get<std::string>();

            if (kind == "entity_instance") {
                EntityInstanceNode n;
                n.instance_name = nj.value("instance_name", std::string{});
                n.entity_slug = nj.value("entity_slug", std::string{});
                n.quantity = VariableQuantity{1.0};
                n.schedule = nj.value("schedule", static_cast<MonthMask>(ALL_MONTHS));
                g.add_entity_instance(n);
            } else if (kind == "external_source") {
                ExternalSourceNode n;
                n.resource_slug = nj.value("resource_slug", std::string{});
                n.cost_per_unit = nj.value("cost_per_unit", 0.0);
                g.add_external_source(n);
            } else if (kind == "goal_sink") {
                GoalSinkNode n;
                n.resource_slug = nj.value("resource_slug", std::string{});
                n.quantity_per_month = VariableQuantity{0.0};
                g.add_goal_sink(n);
            }
        }
    }
    // Edges are topology only at this stage.
    return g;
}

// ── PlanResult ────────────────────────────────────────────────────────────────

static json monthly_to_json(const MonthlyValues& mv) {
    json arr = json::array();
    for (double v : mv) {
        arr.push_back(v);
    }
    return arr;
}

static MonthlyValues monthly_from_json(const json& j) {
    MonthlyValues mv{};
    if (j.is_array()) {
        for (std::size_t i = 0; i < 12 && i < j.size(); ++i) {
            mv[i] = j[i].get<double>();
        }
    }
    return mv;
}

json to_json(const PlanResult& plan) {
    json data;
    data["graph"] = to_json(plan.graph);
    data["labor_schedule"] = monthly_to_json(plan.labor_schedule);
    data["loop_closure_score"] = plan.loop_closure_score;

    data["balance_sheet"] = json::array();
    for (const auto& b : plan.balance_sheet) {
        json bj;
        bj["resource_slug"] = b.resource_slug;
        bj["internal_production"] = monthly_to_json(b.internal_production);
        bj["consumption"] = monthly_to_json(b.consumption);
        bj["external_purchase"] = monthly_to_json(b.external_purchase);
        bj["annual_internal_production"] = b.annual_internal_production;
        bj["annual_consumption"] = b.annual_consumption;
        bj["annual_external_purchase"] = b.annual_external_purchase;
        data["balance_sheet"].push_back(bj);
    }

    data["bom"]["total_area_m2"] = plan.bom.total_area_m2;
    data["bom"]["estimated_initial_cost"] = plan.bom.estimated_initial_cost;
    data["bom"]["initial_labor_hours"] = plan.bom.initial_labor_hours;
    data["bom"]["materials"] = json::array();
    for (const auto& [slug, qty] : plan.bom.materials) {
        data["bom"]["materials"].push_back({{"slug", slug}, {"qty", qty}});
    }

    data["diagnostics"] = json::array();
    for (const auto& d : plan.diagnostics) {
        json dj;
        dj["kind"] = static_cast<int>(d.kind);
        dj["message"] = d.message;
        if (d.resource_slug) {
            dj["resource_slug"] = *d.resource_slug;
        }
        if (d.entity_slug) {
            dj["entity_slug"] = *d.entity_slug;
        }
        data["diagnostics"].push_back(dj);
    }

    if (plan.nutrient_balance) {
        const auto& nb = *plan.nutrient_balance;
        data["nutrient_balance"]["available_n"] = monthly_to_json(nb.available_n);
        data["nutrient_balance"]["available_p"] = monthly_to_json(nb.available_p);
        data["nutrient_balance"]["available_k"] = monthly_to_json(nb.available_k);
        data["nutrient_balance"]["demanded_n"] = monthly_to_json(nb.demanded_n);
        data["nutrient_balance"]["demanded_p"] = monthly_to_json(nb.demanded_p);
        data["nutrient_balance"]["demanded_k"] = monthly_to_json(nb.demanded_k);
    }
    // If nullopt, field is simply omitted from JSON.

    return json{
        {"version", PLAN_SCHEMA_VERSION.to_string()}, {"type", "plan_result"}, {"data", data}};
}

std::expected<PlanResult, std::string> plan_from_json(const json& j) {
    if (auto v = check_version(j, PLAN_SCHEMA_VERSION, "plan_result"); !v) {
        return std::unexpected(v.error());
    }
    if (!j.contains("data")) {
        return std::unexpected("plan_result: missing 'data' field");
    }

    // Parse schema version for backward-compat logic (Q2).
    SchemaVersion parsed_version{1, 0, 0};
    if (j.contains("version") && j["version"].is_string()) {
        auto ver = SchemaVersion::parse(j["version"].get<std::string>());
        if (ver) {
            parsed_version = *ver;
        }
    }

    const auto& data = j["data"];
    PlanResult plan;

    if (data.contains("graph")) {
        auto g = graph_from_json(data["graph"]);
        if (!g) {
            return std::unexpected(g.error());
        }
        plan.graph = std::move(*g);
    }

    if (data.contains("labor_schedule")) {
        plan.labor_schedule = monthly_from_json(data["labor_schedule"]);
    }

    plan.loop_closure_score = data.value("loop_closure_score", 0.0);

    if (data.contains("balance_sheet") && data["balance_sheet"].is_array()) {
        for (const auto& bj : data["balance_sheet"]) {
            ResourceBalance b;
            b.resource_slug = bj.value("resource_slug", std::string{});
            b.internal_production =
                monthly_from_json(bj.value("internal_production", json::array()));
            b.consumption = monthly_from_json(bj.value("consumption", json::array()));
            b.external_purchase = monthly_from_json(bj.value("external_purchase", json::array()));
            b.annual_internal_production = bj.value("annual_internal_production", 0.0);
            b.annual_consumption = bj.value("annual_consumption", 0.0);
            b.annual_external_purchase = bj.value("annual_external_purchase", 0.0);
            plan.balance_sheet.push_back(b);
        }
    }

    if (data.contains("bom")) {
        const auto& bj = data["bom"];
        plan.bom.total_area_m2 = bj.value("total_area_m2", 0.0);
        plan.bom.estimated_initial_cost = bj.value("estimated_initial_cost", 0.0);
        plan.bom.initial_labor_hours = bj.value("initial_labor_hours", 0.0);
        if (bj.contains("materials") && bj["materials"].is_array()) {
            for (const auto& m : bj["materials"]) {
                plan.bom.materials.emplace_back(m.value("slug", std::string{}),
                                                m.value("qty", 0.0));
            }
        }
    }

    if (data.contains("diagnostics") && data["diagnostics"].is_array()) {
        for (const auto& dj : data["diagnostics"]) {
            Diagnostic d;
            d.kind = static_cast<DiagnosticKind>(dj.value("kind", 0));
            d.message = dj.value("message", std::string{});
            if (dj.contains("resource_slug") && dj["resource_slug"].is_string()) {
                d.resource_slug = dj["resource_slug"].get<std::string>();
            }
            if (dj.contains("entity_slug") && dj["entity_slug"].is_string()) {
                d.entity_slug = dj["entity_slug"].get<std::string>();
            }
            plan.diagnostics.push_back(d);
        }
    }

    // Deserialize NutrientBalance with Q2 backward-compat rule:
    //   - Field present → parse it.
    //   - Field absent AND version < 1.1.0 → zero-initialize (NOT nullopt).
    //   - Field absent AND version >= 1.1.0 → remains nullopt (no-crop plan).
    if (data.contains("nutrient_balance") && data["nutrient_balance"].is_object()) {
        NutrientBalance nb;
        const auto& nbj = data["nutrient_balance"];
        nb.available_n = monthly_from_json(nbj.value("available_n", json::array()));
        nb.available_p = monthly_from_json(nbj.value("available_p", json::array()));
        nb.available_k = monthly_from_json(nbj.value("available_k", json::array()));
        nb.demanded_n = monthly_from_json(nbj.value("demanded_n", json::array()));
        nb.demanded_p = monthly_from_json(nbj.value("demanded_p", json::array()));
        nb.demanded_k = monthly_from_json(nbj.value("demanded_k", json::array()));
        plan.nutrient_balance = nb;
    } else if (parsed_version < SchemaVersion{1, 1, 0}) {
        // Old plan file: zero-initialize all arrays (Q2 clarification — NOT nullopt).
        plan.nutrient_balance = NutrientBalance{};
    }
    // If field absent in a 1.1.0+ file: remains nullopt (no-crop plan).

    return plan;
}

}  // namespace homestead
