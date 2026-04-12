#pragma once
#include <string>
#include <vector>
#include "cJSON.h"

class JsonReader {
public:
    JsonReader();
    JsonReader(cJSON* node, bool owner = false);
    ~JsonReader();

    // Disable copy, only allow move
    JsonReader(const JsonReader&) = delete;
    JsonReader& operator=(const JsonReader&) = delete;
    JsonReader(JsonReader&& other) noexcept;
    JsonReader& operator=(JsonReader&& other) noexcept;

    bool parse(const std::string& json_str);
    std::string print() const;

    JsonReader get_object(const std::string& key) const;
    std::string get_string(const std::string& key, const std::string& default_val = "") const;
    std::vector<std::string> get_string_array(const std::string& key) const;

    bool is_valid() const { return root != nullptr; }

private:
    cJSON* root;
    bool is_owner; // Whether this instance owns and should free the root cJSON node
};
