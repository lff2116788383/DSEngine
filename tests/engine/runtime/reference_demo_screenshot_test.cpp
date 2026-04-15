#include "catch/catch.hpp"

#include <stb_image.h>


#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <numeric>
#include <string>
#include <vector>


namespace {

std::filesystem::path FindWorkspaceRoot() {
    auto current = std::filesystem::current_path();
    while (!current.empty()) {
        if (std::filesystem::exists(current / "CMakeLists.txt") && std::filesystem::exists(current / "bin")) {
            return current;
        }
        const auto parent = current.parent_path();
        if (parent == current) {
            break;
        }
        current = parent;
    }
    return std::filesystem::current_path();
}

std::string QuoteArg(const std::string& value) {
#ifdef _WIN32
    return "\"" + value + "\"";
#else
    return "'" + value + "'";
#endif
}

struct CommandResult {
    int exit_code = -1;
    std::string output;
};

CommandResult RunCommandCapture(const std::string& command) {
#ifdef _WIN32
    FILE* pipe = _popen(command.c_str(), "r");
#else
    FILE* pipe = popen(command.c_str(), "r");
#endif
    REQUIRE(pipe != nullptr);

    std::array<char, 512> buffer{};
    std::string output;
    while (std::fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        output += buffer.data();
    }

#ifdef _WIN32
    const int exit_code = _pclose(pipe);
#else
    const int exit_code = pclose(pipe);
#endif
    return {exit_code, output};
}

std::vector<unsigned char> ReadBinaryFile(const std::filesystem::path& path) {
    std::FILE* file = std::fopen(path.string().c_str(), "rb");
    REQUIRE(file != nullptr);
    std::fseek(file, 0, SEEK_END);
    const long size = std::ftell(file);
    REQUIRE(size > 0);
    std::rewind(file);
    std::vector<unsigned char> bytes(static_cast<std::size_t>(size));
    const auto read = std::fread(bytes.data(), 1, bytes.size(), file);
    std::fclose(file);
    REQUIRE(read == bytes.size());
    return bytes;
}

std::vector<unsigned char> ExtractIdatPayloads(const std::vector<unsigned char>& png) {
    std::vector<unsigned char> payload;
    std::size_t offset = 8; // PNG signature
    while (offset + 8 <= png.size()) {
        const std::uint32_t length = (static_cast<std::uint32_t>(png[offset]) << 24) |
                                     (static_cast<std::uint32_t>(png[offset + 1]) << 16) |
                                     (static_cast<std::uint32_t>(png[offset + 2]) << 8) |
                                     static_cast<std::uint32_t>(png[offset + 3]);
        offset += 4;
        if (offset + 4 > png.size()) {
            break;
        }
        const std::string type(reinterpret_cast<const char*>(png.data() + offset), 4);
        offset += 4;
        if (offset + length + 4 > png.size()) {
            break;
        }
        if (type == "IDAT") {
            payload.insert(payload.end(), png.begin() + static_cast<std::ptrdiff_t>(offset), png.begin() + static_cast<std::ptrdiff_t>(offset + length));
        }
        offset += length + 4; // data + crc
        if (type == "IEND") {
            break;
        }
    }
    return payload;
}

unsigned long RollingHash(const std::vector<unsigned char>& bytes) {
    unsigned long hash = 2166136261u;
    for (unsigned char byte : bytes) {
        hash ^= static_cast<unsigned long>(byte);
        hash *= 16777619u;
    }
    return hash;
}

struct DecodedImage {
    int width = 0;
    int height = 0;
    int channels = 0;
    std::vector<unsigned char> pixels;
};

DecodedImage DecodePngRgba8(const std::filesystem::path& path) {
    int width = 0;
    int height = 0;
    int channels = 0;
    stbi_uc* data = stbi_load(path.string().c_str(), &width, &height, &channels, 4);
    REQUIRE(data != nullptr);

    DecodedImage image;
    image.width = width;
    image.height = height;
    image.channels = 4;
    image.pixels.assign(data, data + static_cast<std::ptrdiff_t>(width) * static_cast<std::ptrdiff_t>(height) * 4);
    stbi_image_free(data);
    return image;
}

struct RegionStats {
    double avg_luma = 0.0;
    double avg_red = 0.0;
    double avg_green = 0.0;
    double avg_blue = 0.0;
};

RegionStats ComputeRegionStats(const DecodedImage& image, int x0, int y0, int x1, int y1) {
    REQUIRE(image.width > 0);
    REQUIRE(image.height > 0);
    x0 = std::max(0, std::min(x0, image.width));
    y0 = std::max(0, std::min(y0, image.height));
    x1 = std::max(x0 + 1, std::min(x1, image.width));
    y1 = std::max(y0 + 1, std::min(y1, image.height));

    double luma_sum = 0.0;
    double red_sum = 0.0;
    double green_sum = 0.0;
    double blue_sum = 0.0;
    std::size_t sample_count = 0;
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            const std::size_t index = (static_cast<std::size_t>(y) * static_cast<std::size_t>(image.width) + static_cast<std::size_t>(x)) * 4u;
            const double red = static_cast<double>(image.pixels[index + 0]);
            const double green = static_cast<double>(image.pixels[index + 1]);
            const double blue = static_cast<double>(image.pixels[index + 2]);
            red_sum += red;
            green_sum += green;
            blue_sum += blue;
            luma_sum += 0.2126 * red + 0.7152 * green + 0.0722 * blue;
            ++sample_count;
        }
    }

    REQUIRE(sample_count > 0u);
    RegionStats stats;
    stats.avg_luma = luma_sum / static_cast<double>(sample_count);
    stats.avg_red = red_sum / static_cast<double>(sample_count);
    stats.avg_green = green_sum / static_cast<double>(sample_count);
    stats.avg_blue = blue_sum / static_cast<double>(sample_count);
    return stats;
}


} // namespace

TEST_CASE("Given_ReferenceDemo159Runtime_When_ScreenshotCaptureIsEnabled_Then_RuntimeWritesNonEmptyPngWithStableWeakSignal", "[engine][smoke][snapshot][runtime_host][reference_demo][screenshot]") {
    const auto root = FindWorkspaceRoot();
    const auto cpp_host = root / "bin" / "DSEngine_c++_debug.exe";
    REQUIRE(std::filesystem::exists(cpp_host));

    const auto screenshot_dir = std::filesystem::temp_directory_path() / "dse_reference_demo_screenshots";
    std::filesystem::create_directories(screenshot_dir);
    const auto screenshot_path = screenshot_dir / "reference_demo_15_9_capture.png";
    std::error_code ec;
    std::filesystem::remove(screenshot_path, ec);

    std::string command = "$env:DSE_MAX_FRAMES='20'; ";
    command += "$env:DSE_NO_TEST_PAUSE='1'; ";
    command += "$env:DSE_STARTUP_SCENE='assets/scenes/reference_demo_15_9.scene.json'; ";
    command += "$env:DSE_SCREENSHOT_PATH='" + screenshot_path.generic_string() + "'; ";
    command += "$env:DSE_SCREENSHOT_TARGET='scene'; ";






    command += "$p = Start-Process -FilePath '" + cpp_host.string() + "' ";
    command += "-WorkingDirectory '" + root.string() + "' ";
    command += "-PassThru -RedirectStandardOutput '" + (screenshot_dir / "reference_demo_15_9_capture.log").string() + "'; ";
    command += "Wait-Process -Id $p.Id; ";
    command += "Get-Content '" + (screenshot_dir / "reference_demo_15_9_capture.log").string() + "' -Raw";

    const std::string shell_command = "powershell -NoProfile -Command \"" + command + "\"";
    const auto result = RunCommandCapture(shell_command);
    INFO(result.output);
    REQUIRE(result.exit_code == 0);
    REQUIRE(result.output.find("startup_scene_loaded path=assets/scenes/reference_demo_15_9.scene.json") != std::string::npos);
    REQUIRE(result.output.find("DSE_SCREENSHOT_WRITTEN path=") != std::string::npos);
    REQUIRE(std::filesystem::exists(screenshot_path));





    const auto png_bytes = ReadBinaryFile(screenshot_path);
    REQUIRE(png_bytes.size() > 1024u);
    REQUIRE(png_bytes[0] == 0x89u);
    REQUIRE(png_bytes[1] == 'P');
    REQUIRE(png_bytes[2] == 'N');
    REQUIRE(png_bytes[3] == 'G');

    const auto idat_payload = ExtractIdatPayloads(png_bytes);
    REQUIRE(idat_payload.size() > 256u);
    const auto weak_hash = RollingHash(idat_payload);
    INFO("reference_demo_15_9 screenshot weak hash=" << weak_hash);
    REQUIRE(weak_hash != 0u);

    const auto image = DecodePngRgba8(screenshot_path);
    REQUIRE(image.width >= 320);
    REQUIRE(image.height >= 180);

    const RegionStats center_stats = ComputeRegionStats(image,
        image.width / 3,
        image.height / 4,
        image.width * 2 / 3,
        image.height * 3 / 4);
    const RegionStats sky_stats = ComputeRegionStats(image,
        image.width / 4,
        0,
        image.width * 3 / 4,
        image.height / 5);

    INFO("center avg luma=" << center_stats.avg_luma << " rgb=" << center_stats.avg_red << "," << center_stats.avg_green << "," << center_stats.avg_blue);
    INFO("sky avg luma=" << sky_stats.avg_luma << " rgb=" << sky_stats.avg_red << "," << sky_stats.avg_green << "," << sky_stats.avg_blue);

    REQUIRE(center_stats.avg_luma >= 4.5);
    REQUIRE(sky_stats.avg_luma >= 4.5);
    REQUIRE(sky_stats.avg_blue >= sky_stats.avg_red);
}


