#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <string>
#include <cstdint>

struct VConsoleConfig {
    uint16_t port = 29000;
    std::string bind = "127.0.0.1";
    int max_connections = 1;  // 0 = unlimited
    bool logging = true;
};

bool loadConfig(const std::string& path, VConsoleConfig& config);
std::string getPluginDirectory();

#endif // CONFIG_HPP
