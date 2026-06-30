/**
 * @file world_state_persistence.cpp
 * @brief 持久化世界状态实现：二进制增量格式读写
 */

#include "engine/scene/world_state_persistence.h"
#include <fstream>
#include <cstring>
#include <chrono>
#include <filesystem>

namespace dse {

namespace {
constexpr char kMagic[] = "DCST"; // DSE Cell STate
constexpr uint32_t kFormatVersion = 1;
}

void WorldStatePersistence::Init(const std::string& save_directory) {
    save_directory_ = save_directory;
    initialized_ = true;
}

void WorldStatePersistence::RecordModification(int cell_x, int cell_y, const EntityModRecord& record) {
    if (!initialized_) return;

    CellKey key{cell_x, cell_y};
    auto& state = cell_states_[key];
    state.cell_x = cell_x;
    state.cell_y = cell_y;
    state.timestamp = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    state.modifications.push_back(record);
    dirty_cells_[key] = true;
}

void WorldStatePersistence::RecordSpawn(int cell_x, int cell_y, uint64_t entity_id,
                                         const std::vector<uint8_t>& serialized_data) {
    EntityModRecord rec;
    rec.entity_id = entity_id;
    rec.type = EntityModType::Spawned;
    rec.new_value = serialized_data;
    RecordModification(cell_x, cell_y, rec);
}

void WorldStatePersistence::RecordDestruction(int cell_x, int cell_y, uint64_t entity_id) {
    EntityModRecord rec;
    rec.entity_id = entity_id;
    rec.type = EntityModType::Destroyed;
    RecordModification(cell_x, cell_y, rec);
}

const CellStateData* WorldStatePersistence::GetCellState(int cell_x, int cell_y) const {
    CellKey key{cell_x, cell_y};
    auto it = cell_states_.find(key);
    if (it != cell_states_.end()) return &it->second;
    return nullptr;
}

bool WorldStatePersistence::SaveCell(int cell_x, int cell_y) {
    CellKey key{cell_x, cell_y};
    auto it = cell_states_.find(key);
    if (it == cell_states_.end()) return false;

    const auto& state = it->second;
    std::string path = BuildCellStatePath(cell_x, cell_y);

    // 确保目录存在
    std::filesystem::path dir = std::filesystem::path(path).parent_path();
    if (!dir.empty()) {
        std::filesystem::create_directories(dir);
    }

    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) return false;

    // Header: magic(4) + version(4) + cell_x(4) + cell_y(4) + timestamp(8) + mod_count(4)
    file.write(kMagic, 4);
    file.write(reinterpret_cast<const char*>(&kFormatVersion), 4);
    file.write(reinterpret_cast<const char*>(&state.cell_x), 4);
    file.write(reinterpret_cast<const char*>(&state.cell_y), 4);
    file.write(reinterpret_cast<const char*>(&state.timestamp), 8);

    uint32_t mod_count = static_cast<uint32_t>(state.modifications.size());
    file.write(reinterpret_cast<const char*>(&mod_count), 4);

    for (const auto& mod : state.modifications) {
        file.write(reinterpret_cast<const char*>(&mod.entity_id), 8);
        uint8_t type_byte = static_cast<uint8_t>(mod.type);
        file.write(reinterpret_cast<const char*>(&type_byte), 1);

        // component_name (length-prefixed)
        uint16_t comp_len = static_cast<uint16_t>(mod.component_name.size());
        file.write(reinterpret_cast<const char*>(&comp_len), 2);
        file.write(mod.component_name.data(), comp_len);

        // field_name (length-prefixed)
        uint16_t field_len = static_cast<uint16_t>(mod.field_name.size());
        file.write(reinterpret_cast<const char*>(&field_len), 2);
        file.write(mod.field_name.data(), field_len);

        // new_value (length-prefixed)
        uint32_t val_len = static_cast<uint32_t>(mod.new_value.size());
        file.write(reinterpret_cast<const char*>(&val_len), 4);
        if (val_len > 0) {
            file.write(reinterpret_cast<const char*>(mod.new_value.data()), val_len);
        }
    }

    dirty_cells_.erase(key);
    return true;
}

bool WorldStatePersistence::LoadCell(int cell_x, int cell_y) {
    std::string path = BuildCellStatePath(cell_x, cell_y);
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return false;

    char magic[4];
    file.read(magic, 4);
    if (std::memcmp(magic, kMagic, 4) != 0) return false;

    uint32_t version;
    file.read(reinterpret_cast<char*>(&version), 4);
    if (version > kFormatVersion) return false;

    CellStateData state;
    file.read(reinterpret_cast<char*>(&state.cell_x), 4);
    file.read(reinterpret_cast<char*>(&state.cell_y), 4);
    file.read(reinterpret_cast<char*>(&state.timestamp), 8);

    uint32_t mod_count;
    file.read(reinterpret_cast<char*>(&mod_count), 4);

    state.modifications.resize(mod_count);
    for (uint32_t i = 0; i < mod_count; ++i) {
        auto& mod = state.modifications[i];
        file.read(reinterpret_cast<char*>(&mod.entity_id), 8);
        uint8_t type_byte;
        file.read(reinterpret_cast<char*>(&type_byte), 1);
        mod.type = static_cast<EntityModType>(type_byte);

        uint16_t comp_len;
        file.read(reinterpret_cast<char*>(&comp_len), 2);
        mod.component_name.resize(comp_len);
        file.read(mod.component_name.data(), comp_len);

        uint16_t field_len;
        file.read(reinterpret_cast<char*>(&field_len), 2);
        mod.field_name.resize(field_len);
        file.read(mod.field_name.data(), field_len);

        uint32_t val_len;
        file.read(reinterpret_cast<char*>(&val_len), 4);
        mod.new_value.resize(val_len);
        if (val_len > 0) {
            file.read(reinterpret_cast<char*>(mod.new_value.data()), val_len);
        }
    }

    state.version = version;
    CellKey key{cell_x, cell_y};
    cell_states_[key] = std::move(state);
    return true;
}

void WorldStatePersistence::ResetCell(int cell_x, int cell_y) {
    CellKey key{cell_x, cell_y};
    cell_states_.erase(key);
    dirty_cells_.erase(key);

    std::string path = BuildCellStatePath(cell_x, cell_y);
    std::filesystem::remove(path);
}

void WorldStatePersistence::SaveAll() {
    // 复制 dirty keys 避免迭代中修改
    std::vector<CellKey> keys;
    for (const auto& [k, _] : dirty_cells_) {
        keys.push_back(k);
    }
    for (const auto& k : keys) {
        SaveCell(k.x, k.y);
    }
}

size_t WorldStatePersistence::TotalModificationCount() const {
    size_t total = 0;
    for (const auto& [_, state] : cell_states_) {
        total += state.modifications.size();
    }
    return total;
}

std::string WorldStatePersistence::BuildCellStatePath(int cell_x, int cell_y) const {
    return save_directory_ + "/cell_" + std::to_string(cell_x) + "_" + std::to_string(cell_y) + ".dcell_state";
}

void WorldStatePersistence::Shutdown() {
    SaveAll();
    cell_states_.clear();
    dirty_cells_.clear();
    initialized_ = false;
}

} // namespace dse
