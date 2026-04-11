#pragma once
#include <string>
#include <vector>
#include "cJSON.h"

class ConfigReader {
public:
    ConfigReader();
    ConfigReader(cJSON* node, bool owner = false);
    ~ConfigReader();

    // Disable copy, only allow move
    ConfigReader(const ConfigReader&) = delete;
    ConfigReader& operator=(const ConfigReader&) = delete;
    ConfigReader(ConfigReader&& other) noexcept;
    ConfigReader& operator=(ConfigReader&& other) noexcept;

    bool parse(const std::string& json_str);
    std::string print() const;

    ConfigReader get_object(const std::string& key) const;
    std::string get_string(const std::string& key, const std::string& default_val = "") const;
    std::vector<std::string> get_string_array(const std::string& key) const;

    bool is_valid() const { return root != nullptr; }

private:
    cJSON* root;
    bool is_owner; // Whether this instance owns and should free the root cJSON node
};
