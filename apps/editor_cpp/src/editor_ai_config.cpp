#include "editor_ai_config.h"
#include <imgui.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>

#ifdef _WIN32
#include <windows.h>
#include <dpapi.h>
#endif

namespace {

std::string ToHex(const std::string& bytes) {
    std::ostringstream oss;
    for (unsigned char c : bytes)
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)c;
    return oss.str();
}

std::string FromHex(const std::string& hex) {
    std::string out;
    out.reserve(hex.size() / 2);
    for (size_t i = 0; i + 1 < hex.size(); i += 2) {
        unsigned char c = (unsigned char)std::stoul(hex.substr(i, 2), nullptr, 16);
        out.push_back((char)c);
    }
    return out;
}

} // namespace

namespace dse::editor {

// ─── Singleton ─────────────────────────────────────────────────────────────

AIConfigManager& AIConfigManager::Instance() {
    static AIConfigManager instance;
    return instance;
}

// ─── Load / Save ───────────────────────────────────────────────────────────

void AIConfigManager::Load(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        // Create default config
        config_.providers.push_back({"OpenAI"});
        return;
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
    file.close();

    rapidjson::Document doc;
    doc.Parse(content.c_str());
    if (doc.HasParseError()) {
        config_ = AIConfig{};                        // 重置为默认值，避免半初始化
        config_.providers.push_back({"OpenAI"});
        return;
    }

    // Load providers
    if (doc.HasMember("providers") && doc["providers"].IsArray()) {
        config_.providers.clear();
        for (const auto& provider : doc["providers"].GetArray()) {
            AIProviderConfig p;
            if (provider.HasMember("name") && provider["name"].IsString())
                p.name = provider["name"].GetString();
            if (provider.HasMember("api_key") && provider["api_key"].IsString())
                p.api_key = DecryptAPIKey(provider["api_key"].GetString());
            if (provider.HasMember("base_url") && provider["base_url"].IsString())
                p.base_url = provider["base_url"].GetString();
            if (provider.HasMember("model") && provider["model"].IsString())
                p.model = provider["model"].GetString();
            if (provider.HasMember("image_model") && provider["image_model"].IsString())
                p.image_model = provider["image_model"].GetString();
            if (provider.HasMember("proxy_url") && provider["proxy_url"].IsString())
                p.proxy_url = provider["proxy_url"].GetString();
            if (provider.HasMember("timeout_ms") && provider["timeout_ms"].IsInt())
                p.timeout_ms = provider["timeout_ms"].GetInt();
            if (provider.HasMember("temperature") && provider["temperature"].IsFloat())
                p.temperature = provider["temperature"].GetFloat();
            if (provider.HasMember("max_tokens") && provider["max_tokens"].IsInt())
                p.max_tokens = provider["max_tokens"].GetInt();
            config_.providers.push_back(p);
        }
    }

    // Load global settings
    if (doc.HasMember("current_provider_index") && doc["current_provider_index"].IsInt())
        config_.current_provider_index = doc["current_provider_index"].GetInt();
    if (doc.HasMember("enable_streaming") && doc["enable_streaming"].IsBool())
        config_.enable_streaming = doc["enable_streaming"].GetBool();
    if (doc.HasMember("enable_images") && doc["enable_images"].IsBool())
        config_.enable_images = doc["enable_images"].GetBool();
    if (doc.HasMember("default_agent") && doc["default_agent"].IsString())
        config_.default_agent = doc["default_agent"].GetString();
    if (doc.HasMember("debug_mode") && doc["debug_mode"].IsBool())
        config_.debug_mode = doc["debug_mode"].GetBool();
    if (doc.HasMember("log_raw_protocol") && doc["log_raw_protocol"].IsBool())
        config_.log_raw_protocol = doc["log_raw_protocol"].GetBool();
}

void AIConfigManager::Save(const std::string& path) {
    rapidjson::Document doc(rapidjson::kObjectType);
    auto& alloc = doc.GetAllocator();

    // Save providers
    rapidjson::Value providers_array(rapidjson::kArrayType);
    for (const auto& p : config_.providers) {
        rapidjson::Value provider_obj(rapidjson::kObjectType);
        provider_obj.AddMember("name", rapidjson::Value(p.name.c_str(), alloc), alloc);
        provider_obj.AddMember("api_key", rapidjson::Value(EncryptAPIKey(p.api_key).c_str(), alloc), alloc);
        provider_obj.AddMember("base_url", rapidjson::Value(p.base_url.c_str(), alloc), alloc);
        provider_obj.AddMember("model", rapidjson::Value(p.model.c_str(), alloc), alloc);
        provider_obj.AddMember("image_model", rapidjson::Value(p.image_model.c_str(), alloc), alloc);
        provider_obj.AddMember("proxy_url", rapidjson::Value(p.proxy_url.c_str(), alloc), alloc);
        provider_obj.AddMember("timeout_ms", p.timeout_ms, alloc);
        provider_obj.AddMember("temperature", p.temperature, alloc);
        provider_obj.AddMember("max_tokens", p.max_tokens, alloc);
        providers_array.PushBack(provider_obj, alloc);
    }
    doc.AddMember("providers", providers_array, alloc);

    // Save global settings
    doc.AddMember("current_provider_index", config_.current_provider_index, alloc);
    doc.AddMember("enable_streaming", config_.enable_streaming, alloc);
    doc.AddMember("enable_images", config_.enable_images, alloc);
    doc.AddMember("default_agent", rapidjson::Value(config_.default_agent.c_str(), alloc), alloc);
    doc.AddMember("debug_mode", config_.debug_mode, alloc);
    doc.AddMember("log_raw_protocol", config_.log_raw_protocol, alloc);

    rapidjson::StringBuffer buf;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buf);
    doc.Accept(writer);

    std::ofstream file(path);
    file << buf.GetString();
    file.close();
}

AIConfig& AIConfigManager::GetConfig() {
    return config_;
}

// ─── API Key Encryption ─────────────────────────────────────────────────────

std::string AIConfigManager::EncryptAPIKey(const std::string& key) {
#ifdef _WIN32
    DATA_BLOB in, out;
    in.pbData = (BYTE*)key.data();
    in.cbData = (DWORD)key.size();
    if (CryptProtectData(&in, L"DSEngine AI Key", nullptr, nullptr, nullptr, 0, &out)) {
        std::string binary((char*)out.pbData, out.cbData);
        LocalFree(out.pbData);
        return "dpapi:" + ToHex(binary);
    }
#endif
    // Fallback: XOR + hex encode (symmetric, hex ensures JSON-safe output)
    const char key_xor[] = "DSEngineAIKeyXOR";
    std::string result = key;
    for (size_t i = 0; i < result.size(); ++i)
        result[i] ^= key_xor[i % (sizeof(key_xor) - 1)];
    return "xor:" + ToHex(result);
}

std::string AIConfigManager::DecryptAPIKey(const std::string& encrypted) {
    if (encrypted.empty()) return {};
#ifdef _WIN32
    if (encrypted.substr(0, 6) == "dpapi:") {
        std::string binary = FromHex(encrypted.substr(6));
        DATA_BLOB in, out;
        in.pbData = (BYTE*)binary.data();
        in.cbData = (DWORD)binary.size();
        if (CryptUnprotectData(&in, nullptr, nullptr, nullptr, nullptr, 0, &out)) {
            std::string result((char*)out.pbData, out.cbData);
            LocalFree(out.pbData);
            return result;
        }
        return {};
    }
#endif
    if (encrypted.substr(0, 4) == "xor:") {
        std::string bytes = FromHex(encrypted.substr(4));
        const char key_xor[] = "DSEngineAIKeyXOR";
        for (size_t i = 0; i < bytes.size(); ++i)
            bytes[i] ^= key_xor[i % (sizeof(key_xor) - 1)];
        return bytes;
    }
    return encrypted; // Legacy: unencrypted plaintext
}

// ─── Config Window UI ──────────────────────────────────────────────────────

void AIConfigManager::ShowConfigWindow(bool show) {
    show_config_ = show;
}

void AIConfigManager::DrawConfigWindow() {
    if (!show_config_) return;
    
    ImGui::SetNextWindowSize(ImVec2(600, 500), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("AI Configuration", &show_config_)) {
        // Provider selection
        if (config_.providers.empty()) {
            config_.providers.push_back({"OpenAI"});
        }
        
        if (config_.current_provider_index >= config_.providers.size()) {
            config_.current_provider_index = 0;
        }
        
        if (ImGui::BeginCombo("Provider", config_.providers[config_.current_provider_index].name.c_str())) {
            for (size_t i = 0; i < config_.providers.size(); ++i) {
                if (ImGui::Selectable(config_.providers[i].name.c_str(), 
                                     i == static_cast<size_t>(config_.current_provider_index))) {
                    config_.current_provider_index = static_cast<int>(i);
                }
            }
            ImGui::EndCombo();
        }
        
        auto& provider = config_.providers[config_.current_provider_index];
        
        ImGui::Separator();
        
#define SAFE_STRCPY(buf, src) do { strncpy((buf), (src).c_str(), sizeof(buf)-1); (buf)[sizeof(buf)-1]='\0'; } while(0)

        // Provider name
        char name_buf[64];
        SAFE_STRCPY(name_buf, provider.name);
        if (ImGui::InputText("Provider Name", name_buf, sizeof(name_buf))) {
            provider.name = name_buf;
        }
        
        // API Key (password masked)
        char key_buf[256];
        SAFE_STRCPY(key_buf, provider.api_key);
        if (ImGui::InputText("API Key", key_buf, sizeof(key_buf), ImGuiInputTextFlags_Password)) {
            provider.api_key = key_buf;
        }
        
        // Base URL
        char url_buf[256];
        SAFE_STRCPY(url_buf, provider.base_url);
        if (ImGui::InputText("Base URL", url_buf, sizeof(url_buf))) {
            provider.base_url = url_buf;
        }
        
        // Model
        char model_buf[64];
        SAFE_STRCPY(model_buf, provider.model);
        if (ImGui::InputText("Model", model_buf, sizeof(model_buf))) {
            provider.model = model_buf;
        }
        
        // Image Model
        char img_model_buf[64];
        SAFE_STRCPY(img_model_buf, provider.image_model);
        if (ImGui::InputText("Image Model", img_model_buf, sizeof(img_model_buf))) {
            provider.image_model = img_model_buf;
        }
        
        // Proxy
        char proxy_buf[256];
        SAFE_STRCPY(proxy_buf, provider.proxy_url);
        if (ImGui::InputText("Proxy URL", proxy_buf, sizeof(proxy_buf))) {
            provider.proxy_url = proxy_buf;
        }
        
        // Temperature
        ImGui::SliderFloat("Temperature", &provider.temperature, 0.0f, 2.0f);
        
        // Max Tokens
        ImGui::InputInt("Max Tokens", &provider.max_tokens);
        
        // Timeout
        ImGui::InputInt("Timeout (ms)", &provider.timeout_ms);
        
        ImGui::Separator();
        
        // Global settings
        ImGui::Checkbox("Enable Streaming", &config_.enable_streaming);
        ImGui::Checkbox("Enable Images", &config_.enable_images);
        ImGui::Checkbox("Debug Mode", &config_.debug_mode);
        ImGui::Checkbox("Log Raw Protocol", &config_.log_raw_protocol);
        
        ImGui::Separator();
        
        // Default Agent
        char agent_buf[64];
        SAFE_STRCPY(agent_buf, config_.default_agent);
        if (ImGui::InputText("Default Agent", agent_buf, sizeof(agent_buf))) {
            config_.default_agent = agent_buf;
        }

#undef SAFE_STRCPY
        
        ImGui::Separator();
        
        // Buttons
        if (ImGui::Button("Save")) {
            Save("bin/editor_ai_config.json");
        }
        ImGui::SameLine();
        if (ImGui::Button("Add Provider")) {
            config_.providers.push_back({"New Provider"});
        }
        ImGui::SameLine();
        if (ImGui::Button("Remove Provider") && config_.providers.size() > 1) {
            config_.providers.erase(config_.providers.begin() + config_.current_provider_index);
            config_.current_provider_index = 0;
        }
    }
    ImGui::End();
}

} // namespace dse::editor
