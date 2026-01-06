#include "config.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <linux/limits.h>
#include <dlfcn.h>
#endif

static std::string trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

std::string getPluginDirectory() {
#ifdef _WIN32
    char path[MAX_PATH];
    HMODULE hModule = NULL;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       (LPCSTR)&getPluginDirectory, &hModule);
    GetModuleFileNameA(hModule, path, MAX_PATH);
    std::string fullPath(path);
    size_t lastSlash = fullPath.find_last_of("\\/");
    if (lastSlash != std::string::npos) {
        fullPath = fullPath.substr(0, lastSlash);
        lastSlash = fullPath.find_last_of("\\/");
        if (lastSlash != std::string::npos) {
            return fullPath.substr(0, lastSlash + 1);
        }
    }
    return fullPath;
#else
    Dl_info info;
    if (dladdr((void*)&getPluginDirectory, &info) && info.dli_fname) {
        std::string fullPath(info.dli_fname);
        size_t lastSlash = fullPath.find_last_of('/');
        if (lastSlash != std::string::npos) {
            fullPath = fullPath.substr(0, lastSlash);
            lastSlash = fullPath.find_last_of('/');
            if (lastSlash != std::string::npos) {
                return fullPath.substr(0, lastSlash + 1);
            }
        }
        return fullPath;
    }
    return "./";
#endif
}

bool loadConfig(const std::string& path, VConsoleConfig& config) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return false;
    }

    std::string line;
    std::string currentSection;

    while (std::getline(file, line)) {
        line = trim(line);

        if (line.empty() || line[0] == '#' || line[0] == ';') {
            continue;
        }

        if (line[0] == '[' && line.back() == ']') {
            currentSection = line.substr(1, line.length() - 2);
            continue;
        }

        size_t eqPos = line.find('=');
        if (eqPos == std::string::npos) {
            continue;
        }

        std::string key = trim(line.substr(0, eqPos));
        std::string value = trim(line.substr(eqPos + 1));

        if (currentSection == "vconsole") {
            if (key == "port") {
                int port = std::stoi(value);
                if (port > 0 && port <= 65535) {
                    config.port = static_cast<uint16_t>(port);
                }
            } else if (key == "bind") {
                config.bind = value;
            } else if (key == "max_connections") {
                config.max_connections = std::stoi(value);
            } else if (key == "logging") {
                config.logging = (std::stoi(value) != 0);
            }
        }
    }

    return true;
}
