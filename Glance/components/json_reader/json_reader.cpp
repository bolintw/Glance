#include "json_reader.hpp"
#include <cstdlib>

JsonReader::JsonReader() : root(nullptr), is_owner(true) {}

JsonReader::JsonReader(cJSON* node, bool owner) : root(node), is_owner(owner) {}

JsonReader::~JsonReader() {
    if (is_owner && root) {
        cJSON_Delete(root);
    }
}

JsonReader::JsonReader(JsonReader&& other) noexcept : root(other.root), is_owner(other.is_owner) {
    other.root = nullptr;
}

JsonReader& JsonReader::operator=(JsonReader&& other) noexcept {
    if (this != &other) {
        if (is_owner && root) cJSON_Delete(root);
        root = other.root;
        is_owner = other.is_owner;
        other.root = nullptr;
    }
    return *this;
}

bool JsonReader::parse(const std::string& json_str) {
    if (is_owner && root) cJSON_Delete(root);
    root = cJSON_Parse(json_str.c_str());
    is_owner = true;
    return root != nullptr;
}

std::string JsonReader::print() const {
    if (!root) return "";
    char* str = cJSON_Print(root);
    std::string result(str);
    free(str);
    return result;
}

JsonReader JsonReader::get_object(const std::string& key) const {
    if (!root) return JsonReader(nullptr);
    cJSON* obj = cJSON_GetObjectItem(root, key.c_str());
    return JsonReader(obj, false); // Child objects are not owners
}

std::string JsonReader::get_string(const std::string& key, const std::string& default_val) const {
    if (!root) return default_val;
    cJSON* item = cJSON_GetObjectItem(root, key.c_str());
    if (cJSON_IsString(item)) {
        return item->valuestring;
    }
    return default_val;
}

std::vector<std::string> JsonReader::get_string_array(const std::string& key) const {
    std::vector<std::string> results;
    if (!root) return results;
    cJSON* array = cJSON_GetObjectItem(root, key.c_str());
    if (cJSON_IsArray(array)) {
        cJSON* item;
        cJSON_ArrayForEach(item, array) {
            if (cJSON_IsString(item)) {
                results.push_back(item->valuestring);
            }
        }
    }
    return results;
}
