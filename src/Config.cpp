#include "Config.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>

namespace SparkBuild {

const char* GeneratorToString(Generator gen) {
    switch (gen) {
        case Generator::VS2022:       return "Visual Studio 17 2022";
        case Generator::VS2026:       return "Visual Studio 18 2026";
        case Generator::Ninja:        return "Ninja";
        case Generator::UnixMakefiles:return "Unix Makefiles";
    }
    return "Visual Studio 17 2022";
}

const char* GeneratorDisplayName(Generator gen) {
    switch (gen) {
        case Generator::VS2022:       return "Visual Studio 2022";
        case Generator::VS2026:       return "Visual Studio 2026";
        case Generator::Ninja:        return "Ninja";
        case Generator::UnixMakefiles:return "Unix Makefiles";
    }
    return "Visual Studio 2022";
}

const char* BuildTypeToString(BuildType bt) {
    switch (bt) {
        case BuildType::Debug:         return "Debug";
        case BuildType::Release:       return "Release";
        case BuildType::RelWithDebInfo:return "RelWithDebInfo";
    }
    return "Release";
}

const char* CategoryDisplayName(OptionCategory cat) {
    switch (cat) {
        case OptionCategory::Core:         return "Core Systems";
        case OptionCategory::Graphics:     return "Graphics";
        case OptionCategory::EditorTools:  return "Editor & Tools";
        case OptionCategory::Scripting:    return "Scripting";
        case OptionCategory::Gameplay:     return "Gameplay Systems";
        case OptionCategory::Experimental: return "Experimental";
    }
    return "Other";
}

ConfigManager::ConfigManager() {
    InitDefaults();
}

void ConfigManager::InitDefaults() {
    config.options.clear();

    // Core Systems
    config.options.push_back({"ENABLE_GRAPHICS",       "Graphics Engine",          "DirectX 11 rendering system",                     true, true, OptionCategory::Core});
    config.options.push_back({"ENABLE_PHYSX",          "Physics (Bullet)",         "Bullet Physics 3D physics engine",                true, true, OptionCategory::Core});
    config.options.push_back({"ENABLE_AI",             "AI & Navigation",          "AI systems and NavMesh pathfinding",              true, true, OptionCategory::Core});
    config.options.push_back({"ENABLE_ANIMATION",      "Skeletal Animation",       "Skeletal animation system",                       true, true, OptionCategory::Core});
    config.options.push_back({"ENABLE_SAVE_SYSTEM",    "Save/Load System",         "Save and load game state",                        true, true, OptionCategory::Core});

    // Graphics
    config.options.push_back({"ENABLE_VULKAN",         "Vulkan Backend",           "Vulkan graphics backend (experimental)",          true, true, OptionCategory::Graphics});
    config.options.push_back({"ENABLE_OPENGL",         "OpenGL Backend",           "OpenGL 4.5 graphics backend (experimental)",      true, true, OptionCategory::Graphics});
    config.options.push_back({"ENABLE_DXR",            "DirectX Raytracing",       "DXR support (requires D3D12)",                    false, false, OptionCategory::Graphics});
    config.options.push_back({"ENABLE_POST_PROCESSING","Post-Processing",          "Bloom, tone mapping, FXAA effects",               true, true, OptionCategory::Graphics});
    config.options.push_back({"ENABLE_LIGHTING_SYSTEM","Advanced Lighting",        "Advanced lighting and IBL system",                true, true, OptionCategory::Graphics});
    config.options.push_back({"ENABLE_DECALS",         "Decal System",             "Projected decals for impacts and effects",        true, true, OptionCategory::Graphics});
    config.options.push_back({"ENABLE_MESH_LOD",       "Mesh LOD",                 "Mesh level-of-detail system",                     true, true, OptionCategory::Graphics});

    // Editor & Tools
    config.options.push_back({"ENABLE_EDITOR",         "Editor (Windows Only)",    "ImGui visual editor (Win32 + DX11 only)",         true, true, OptionCategory::EditorTools});
    config.options.push_back({"ENABLE_PROFILING",      "Profiling Tools",          "Performance profiling and monitoring",             true, true, OptionCategory::EditorTools});
    config.options.push_back({"BUILD_TESTS",           "Unit Tests",               "Build the test suite",                            true, true, OptionCategory::EditorTools});

    // Scripting
    config.options.push_back({"ENABLE_LUA",            "Lua Scripting",            "Lua scripting language support",                  true, true, OptionCategory::Scripting});
    config.options.push_back({"ENABLE_HOT_RELOAD",     "Hot Reload",               "Script hot-reload during development",            true, true, OptionCategory::Scripting});

    // Gameplay Systems
    config.options.push_back({"ENABLE_TERRAIN_SYSTEM", "Terrain System",           "Heightmap terrain with LOD",                      true, true, OptionCategory::Gameplay});
    config.options.push_back({"ENABLE_ADVANCED_INPUT", "Advanced Input",           "Extended input features (gamepad, etc.)",          true, true, OptionCategory::Gameplay});
    config.options.push_back({"ENABLE_ASSET_STREAMING","Asset Streaming",          "Runtime asset streaming",                         true, true, OptionCategory::Gameplay});
    config.options.push_back({"ENABLE_PROCEDURAL",     "Procedural Generation",    "Procedural content generation",                   true, true, OptionCategory::Gameplay});
    config.options.push_back({"ENABLE_CINEMATIC",      "Cinematic Sequencer",      "Cinematic sequence system",                       true, true, OptionCategory::Gameplay});

    // Experimental
    config.options.push_back({"ENABLE_NETWORKING",     "Networking",               "Networking features (disabled: CURL issues)",      false, false, OptionCategory::Experimental});
    config.options.push_back({"ENABLE_SDL2",           "SDL2 Input",               "SDL2 cross-platform input (requires SDL2)",       false, false, OptionCategory::Experimental});
    config.options.push_back({"ENABLE_COLLABORATIVE",  "Collaborative",            "Collaborative editing features",                  true, true, OptionCategory::Experimental});

    // Set default paths
    config.buildPath = "build";
    config.parallelJobs = 0;
    config.generator = Generator::VS2022;
    config.buildType = BuildType::Release;
}

void ConfigManager::ApplyPresetAllOn() {
    for (auto& opt : config.options)
        opt.currentValue = true;
}

void ConfigManager::ApplyPresetAllOff() {
    for (auto& opt : config.options)
        opt.currentValue = false;
}

void ConfigManager::ApplyPresetDefaults() {
    for (auto& opt : config.options)
        opt.currentValue = opt.defaultValue;
}

void ConfigManager::ApplyPresetMinimal() {
    for (auto& opt : config.options)
        opt.currentValue = false;
    // Enable only core essentials
    for (auto& opt : config.options) {
        if (opt.cmakeVar == "ENABLE_GRAPHICS" || opt.cmakeVar == "ENABLE_PHYSX")
            opt.currentValue = true;
    }
}

static std::string Trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

bool ConfigManager::Load(const std::string& iniPath) {
    std::ifstream file(iniPath);
    if (!file.is_open()) return false;

    std::string line, currentSection;
    while (std::getline(file, line)) {
        line = Trim(line);
        if (line.empty() || line[0] == ';' || line[0] == '#') continue;

        if (line[0] == '[' && line.back() == ']') {
            currentSection = line.substr(1, line.size() - 2);
            continue;
        }

        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = Trim(line.substr(0, eq));
        std::string val = Trim(line.substr(eq + 1));

        if (currentSection == "Paths") {
            if (key == "EnginePath")  config.enginePath = val;
            else if (key == "BuildPath")   config.buildPath = val;
            else if (key == "CMakePath")   config.cmakePath = val;
        } else if (currentSection == "Build") {
            if (key == "Generator") {
                if (val == "VS2022") config.generator = Generator::VS2022;
                else if (val == "VS2026") config.generator = Generator::VS2026;
                else if (val == "Ninja") config.generator = Generator::Ninja;
                else if (val == "UnixMakefiles") config.generator = Generator::UnixMakefiles;
            } else if (key == "BuildType") {
                if (val == "Debug") config.buildType = BuildType::Debug;
                else if (val == "Release") config.buildType = BuildType::Release;
                else if (val == "RelWithDebInfo") config.buildType = BuildType::RelWithDebInfo;
            } else if (key == "MSVCToolset") {
                config.msvcToolset = val;
            } else if (key == "ParallelJobs") {
                config.parallelJobs = std::atoi(val.c_str());
            }
        } else if (currentSection == "Options") {
            for (auto& opt : config.options) {
                if (opt.cmakeVar == key) {
                    opt.currentValue = (val == "1" || val == "ON" || val == "true");
                    break;
                }
            }
        }
    }
    return true;
}

bool ConfigManager::Save(const std::string& iniPath) const {
    std::ofstream file(iniPath);
    if (!file.is_open()) return false;

    file << "; SparkBuild Configuration\n";
    file << "; Auto-generated - edit with SparkBuild GUI\n\n";

    file << "[Paths]\n";
    file << "EnginePath=" << config.enginePath << "\n";
    file << "BuildPath=" << config.buildPath << "\n";
    file << "CMakePath=" << config.cmakePath << "\n\n";

    file << "[Build]\n";
    switch (config.generator) {
        case Generator::VS2022:        file << "Generator=VS2022\n"; break;
        case Generator::VS2026:        file << "Generator=VS2026\n"; break;
        case Generator::Ninja:         file << "Generator=Ninja\n"; break;
        case Generator::UnixMakefiles: file << "Generator=UnixMakefiles\n"; break;
    }
    file << "BuildType=" << BuildTypeToString(config.buildType) << "\n";
    file << "MSVCToolset=" << config.msvcToolset << "\n";
    file << "ParallelJobs=" << config.parallelJobs << "\n\n";

    file << "[Options]\n";
    for (const auto& opt : config.options) {
        file << opt.cmakeVar << "=" << (opt.currentValue ? "ON" : "OFF") << "\n";
    }

    return true;
}

std::string ConfigManager::BuildCMakeConfigureCommand() const {
    std::string cmd;
    std::string cmake = config.cmakePath.empty() ? "cmake" : ("\"" + config.cmakePath + "\"");
    std::string srcDir = config.enginePath.empty() ? "." : ("\"" + config.enginePath + "\"");
    std::string buildDir = config.buildPath.empty() ? "build" : config.buildPath;

    cmd = cmake + " -S " + srcDir + " -B \"" + buildDir + "\"";
    cmd += " -G \"" + std::string(GeneratorToString(config.generator)) + "\"";

    // Build type (for single-config generators like Ninja/Makefiles)
    if (config.generator == Generator::Ninja || config.generator == Generator::UnixMakefiles) {
        cmd += " -DCMAKE_BUILD_TYPE=" + std::string(BuildTypeToString(config.buildType));
    }

    // MSVC toolset override
    if (!config.msvcToolset.empty() &&
        (config.generator == Generator::VS2022 || config.generator == Generator::VS2026)) {
        cmd += " -DSPARK_MSVC_TOOLSET=" + config.msvcToolset;
    }

    // Platform for VS generators
    if (config.generator == Generator::VS2022 || config.generator == Generator::VS2026) {
        cmd += " -A x64";
    }

    // All build options
    for (const auto& opt : config.options) {
        cmd += " -D" + opt.cmakeVar + "=" + (opt.currentValue ? "ON" : "OFF");
    }

    return cmd;
}

std::string ConfigManager::BuildCMakeBuildCommand() const {
    std::string cmake = config.cmakePath.empty() ? "cmake" : ("\"" + config.cmakePath + "\"");
    std::string buildDir = config.buildPath.empty() ? "build" : config.buildPath;

    std::string cmd = cmake + " --build \"" + buildDir + "\"";
    cmd += " --config " + std::string(BuildTypeToString(config.buildType));

    if (config.parallelJobs > 0) {
        cmd += " --parallel " + std::to_string(config.parallelJobs);
    } else {
        cmd += " --parallel";
    }

    return cmd;
}

std::string ConfigManager::GetDefaultIniPath() {
    char exePath[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, exePath, MAX_PATH);
    std::string path(exePath);
    auto pos = path.find_last_of("\\/");
    if (pos != std::string::npos) {
        path = path.substr(0, pos + 1);
    }
    return path + "sparkbuild.ini";
}

} // namespace SparkBuild
