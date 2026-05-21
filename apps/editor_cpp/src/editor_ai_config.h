#pragma once

#include <string>
#include <vector>

namespace dse::editor {

struct AIProviderConfig {
    std::string name = "OpenAI";
    std::string api_key;
    std::string base_url = "https://api.openai.com/v1";
    std::string model = "gpt-4o";
    std::string image_model = "dall-e-3";
    std::string proxy_url;
    int timeout_ms = 30000;
    float temperature = 0.7f;
    int max_tokens = 4096;
};

struct AIConfig {
    std::vector<AIProviderConfig> providers;
    int current_provider_index = 0;
    bool enable_streaming = true;
    bool enable_images = true;
    std::string default_agent = "general";
    bool debug_mode = false;
    bool log_raw_protocol = false;
};

class AIConfigManager {
public:
    static AIConfigManager& Instance();
    
    void Load(const std::string& path);
    void Save(const std::string& path);
    AIConfig& GetConfig();
    
    void DrawConfigWindow();
    void ShowConfigWindow(bool show = true);
    
    // API Key encryption (Windows DPAPI)
    std::string EncryptAPIKey(const std::string& key);
    std::string DecryptAPIKey(const std::string& encrypted_key);

private:
    AIConfigManager() = default;
    AIConfig config_;
    bool show_config_ = false;
};

} // namespace dse::editor
