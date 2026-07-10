#include "engine/scripting/blueprint/blueprint_compiler.h"
#include "engine/scripting/blueprint/blueprint_vm.h"

#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {

bool Near(float lhs, float rhs) {
    return std::fabs(lhs - rhs) < 0.0001f;
}

int Fail(const std::string& message) {
    std::cerr << "[blueprint_smoke] " << message << '\n';
    return 1;
}

}  // namespace

int main() {
    const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() /
        ("dse-blueprint-smoke-" + std::to_string(suffix) + ".dbp");

    const char* json = R"json({
  "name": "RuntimeConvergence",
  "version": 2,
  "description": "Editor-format Blueprint runtime smoke",
  "variables": [
    {
      "name": "counter",
      "type": "Float",
      "default_bool": false,
      "default_int": 0,
      "default_float": 1.0,
      "default_string": "",
      "default_vec": [0.0, 0.0, 0.0, 0.0],
      "is_exposed": true
    },
    {
      "name": "dt_value",
      "type": "Float",
      "default_bool": false,
      "default_int": 0,
      "default_float": 0.0,
      "default_string": "",
      "default_vec": [0.0, 0.0, 0.0, 0.0],
      "is_exposed": false
    }
  ],
  "graphs": [
    {
      "name": "EventGraph",
      "next_id": 300,
      "is_pure": false,
      "input_params": [],
      "output_params": [],
      "nodes": [
        {
          "id": 1,
          "name": "On Init",
          "category": "Event",
          "comment": "",
          "inputs": [],
          "outputs": [
            {"id": 101, "name": "Flow", "type": "Flow"}
          ]
        },
        {
          "id": 2,
          "name": "Set Variable",
          "category": "Variables",
          "comment": "counter",
          "inputs": [
            {"id": 102, "name": "Flow", "type": "Flow"},
            {"id": 103, "name": "Value", "type": "Float", "default_float": 5.0}
          ],
          "outputs": []
        },
        {
          "id": 3,
          "name": "On Update",
          "category": "Event",
          "comment": "",
          "inputs": [],
          "outputs": [
            {"id": 201, "name": "Flow", "type": "Flow"},
            {"id": 202, "name": "dt", "type": "Float"}
          ]
        },
        {
          "id": 4,
          "name": "Set Variable",
          "category": "Variables",
          "comment": "counter",
          "inputs": [
            {"id": 210, "name": "Flow", "type": "Flow"},
            {"id": 211, "name": "Value", "type": "Float", "default_float": 0.0}
          ],
          "outputs": [
            {"id": 212, "name": "Flow", "type": "Flow"}
          ]
        },
        {
          "id": 5,
          "name": "Add",
          "category": "Math",
          "comment": "",
          "inputs": [
            {"id": 221, "name": "A", "type": "Float", "default_float": 0.0},
            {"id": 222, "name": "B", "type": "Float", "default_float": 1.0}
          ],
          "outputs": [
            {"id": 220, "name": "Result", "type": "Float"}
          ]
        },
        {
          "id": 6,
          "name": "Get Variable",
          "category": "Variables",
          "comment": "counter",
          "inputs": [],
          "outputs": [
            {"id": 230, "name": "Value", "type": "Float"}
          ]
        },
        {
          "id": 7,
          "name": "Set Variable",
          "category": "Variables",
          "comment": "dt_value",
          "inputs": [
            {"id": 240, "name": "Flow", "type": "Flow"},
            {"id": 241, "name": "Value", "type": "Float", "default_float": 0.0}
          ],
          "outputs": []
        }
      ],
      "links": [
        {"id": 1, "from_pin": 101, "to_pin": 102},
        {"id": 2, "from_pin": 201, "to_pin": 210},
        {"id": 3, "from_pin": 230, "to_pin": 221},
        {"id": 4, "from_pin": 220, "to_pin": 211},
        {"id": 5, "from_pin": 212, "to_pin": 240},
        {"id": 6, "from_pin": 202, "to_pin": 241}
      ]
    }
  ],
  "interfaces": []
})json";

    {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        if (!output) return Fail("could not create temporary .dbp");
        output << json;
    }

    dse::bp::BlueprintAsset asset;
    asset.name = "stale";
    asset.graphs.push_back({});
    if (!dse::bp::LoadBlueprintAsset(asset, path.string())) {
        std::filesystem::remove(path);
        return Fail("runtime loader rejected editor-format .dbp");
    }
    std::filesystem::remove(path);

    if (asset.name != "RuntimeConvergence" || asset.graphs.size() != 1 ||
        asset.variables.size() != 2) {
        return Fail("runtime loader did not replace and populate the asset");
    }
    if (asset.graphs[0].nodes[0].outputs[0].type != dse::bp::BpPinType::Flow) {
        return Fail("serialized pin types were not preserved");
    }

    dse::bp::CompiledBlueprint compiled = dse::bp::CompileToByteCode(asset);
    if (compiled.functions.size() != 2 ||
        compiled.functions[0].name != "on_init" ||
        compiled.functions[1].name != "on_update") {
        return Fail("EventGraph did not compile to on_init/on_update entry points");
    }

    dse::bp::BlueprintInstance instance;
    instance.blueprint = &compiled;
    dse::bp::BlueprintVM::Get().RunInit(instance, 42, nullptr);
    if (!instance.initialized || instance.variables.size() != 2 ||
        !Near(instance.variables[0].AsFloat(), 5.0f)) {
        return Fail("on_init did not execute the editor-authored graph");
    }

    dse::bp::BlueprintVM::Get().RunUpdate(instance, 42, 0.25f, nullptr);
    if (!Near(instance.variables[0].AsFloat(), 6.0f) ||
        !Near(instance.variables[1].AsFloat(), 0.25f)) {
        return Fail("on_update did not execute graph state and Delta Time consistently");
    }

    std::cout << "[blueprint_smoke] PASS\n";
    return 0;
}
