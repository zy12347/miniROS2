#include "Json.h"


void JsonValue::copyData(const JsonValue& other) {
    switch (other.type_) {
        case JsonType::Null:
            // 无需操作
            break;
        case JsonType::Bool:
            data_.bool_val = other.data_.bool_val;
            break;
        case JsonType::Int:
            data_.int_val = other.data_.int_val;
            break;
        case JsonType::Double:
            data_.double_val = other.data_.double_val;
            break;
        case JsonType::String:
            data_.str_val = new std::string(*other.data_.str_val);
            break;
        case JsonType::Array:
            data_.arr_val = new JsonArray(*other.data_.arr_val);
            break;
        case JsonType::Object:
            data_.obj_val = new JsonObject(*other.data_.obj_val);
            break;
    }
}

std::string JsonValue::serialize() const {
    std::stringstream ss;
    switch (type_) {
        case JsonType::Null:
            ss << "null";
            break;
        case JsonType::Bool:
            ss << (data_.bool_val ? "true" : "false");
            break;
        case JsonType::Int:
            ss << data_.int_val;
            break;
        case JsonType::Double:
            ss << data_.double_val;
            break;
        case JsonType::String:
            ss << "\"" << escapeString(*data_.str_val) << "\"";
            break;
        case JsonType::Array:
            ss << "[";
            for (size_t i = 0; i < data_.arr_val->size(); ++i) {
                if (i > 0) ss << ",";
                ss << (*data_.arr_val)[i].serialize();
            }
            ss << "]";
            break;
        case JsonType::Object:
            ss << "{";
            size_t i = 0;
            for (const auto& pair : *data_.obj_val) {
                if (i > 0) ss << ",";
                ss << "\"" << escapeString(pair.first) << "\":" << pair.second.serialize();
                ++i;
            }
            ss << "}";
            break;
    }
    return ss.str();
}

std::string JsonValue::escapeString(const std::string& str) const {
    std::string res;
    for (char c : str) {
        switch (c) {
            case '"':  res += "\\\""; break;
            case '\\': res += "\\\\"; break;
            case '\b': res += "\\b";  break;
            case '\f': res += "\\f";  break;
            case '\n': res += "\\n";  break;
            case '\r': res += "\\r";  break;
            case '\t': res += "\\t";  break;
            default:   res += c;
        }
    }
    return res;
}

/*
// 类型转换实现
bool JsonValue::asBool() const {
    assert(isBool());
    return value();
}

int JsonValue::asInt() const {
    assert(isInt());
    return value();
}

double JsonValue::asDouble() const {
    assert(isDouble());
    return value();
}

const std::string& JsonValue::asString() const {
    assert(isString());
    return value();
}

const JsonArray& JsonValue::asArray() const {
    assert(isArray());
    return value();
}

const JsonObject& JsonValue::asObject() const {
    assert(isObject());
    return value();
}

JsonArray& JsonValue::asArray() {
    assert(isArray());
    return value();
}

JsonObject& JsonValue::asObject() {
    assert(isObject());
    return value();
}

// 数组操作
void JsonValue::push_back(JsonValue& value) {
    assert(isArray());
    asArray().push_back(value);
}

size_t JsonValue::size() const {
    if (isArray()) {
        return asArray().size();
    } else if (isObject()) {
        return asObject().size();
    }
    return 0;
}

// 对象操作
bool JsonValue::hasKey(const std::string& key) const {
    assert(isObject());
    return asObject().find(key) != asObject().end();
}

void JsonValue::set(const std::string& key, JsonValue& value) {
    assert(isObject());
    asObject()[key] = value;
}

// 访问器实现
std::unique_ptr<JsonValue> JsonValue::operator[](size_t index) {
    assert(isArray());
    if (index < asArray().size()) {
        return asArray()[index];
    }
    return JsonValue();
}

const std::unique_ptr<JsonValue> JsonValue::operator[](size_t index) const {
    assert(isArray());
    if (index < asArray().size()) {
        return asArray()[index];
    }
    return JsonValue();
}

std::unique_ptr<JsonValue> JsonValue::operator[](const std::string& key) {
    assert(isObject());
    auto it = asObject().find(key);
    if (it != asObject().end()) {
        return it->second;
    }
    asObject()[key] = JsonValue();
    return asObject()[key];
}

const std::unique_ptr<JsonValue> JsonValue::operator[](const std::string& key) const {
    assert(isObject());
    auto it = asObject().find(key);
    if (it != asObject().end()) {
        return it->second;
    }
    asObject()[key] = JsonValue();
    return asObject()[key];
}
*/
// 反序列化实现（简化版）
// JsonValue JsonValue::deserialize(const std::string& json) {
//     // 实际实现需要复杂的解析逻辑
//     // 这里仅作为示例返回一个空对象
//     return JsonValue();
// }

/*
C++17
#include "json.h"
#include <regex>
#include <stdexcept>



std::string JsonValue::serialize() const {
    return serializeValue();
}

std::string JsonValue::serializeValue() const {
    return std::visit([this](const auto& value) -> std::string {
        using T = std::decay_t<decltype(value)>;
        
        if constexpr (std::is_same_v<T, std::nullptr_t>) {
            return "null";
        } else if constexpr (std::is_same_v<T, bool>) {
            return value ? "true" : "false";
        } else if constexpr (std::is_same_v<T, int>) {
            return std::to_string(value);
        } else if constexpr (std::is_same_v<T, double>) {
            return std::to_string(value);
        } else if constexpr (std::is_same_v<T, std::string>) {
            return "\"" + escapeString(value) + "\"";
        } else if constexpr (std::is_same_v<T, JsonArray>) {
            return serializeArray();
        } else if constexpr (std::is_same_v<T, JsonObject>) {
            return serializeObject();
        }
    }, value_);
}

std::string JsonValue::serializeArray() const {
    if (!is<JsonArray>()) {
        throw std::runtime_error("Not an array");
    }
    
    const auto& arr = get<JsonArray>();
    std::ostringstream oss;
    oss << "[";
    
    for (size_t i = 0; i < arr.size(); ++i) {
        if (i > 0) oss << ",";
        if (!arr[i].is<std::nullptr_t>()) {
            oss << arr[i].serialize();
        } else {
            oss << "null";
        }
    }
    
    oss << "]";
    return oss.str();
}

std::string JsonValue::serializeObject() const {
    if (!is<JsonObject>()) {
        throw std::runtime_error("Not an object");
    }
    
    const auto& obj = get<JsonObject>();
    std::ostringstream oss;
    oss << "{";
    
    bool first = true;
    for (const auto& [key, value] : obj) {
        if (!first) oss << ",";
        oss << "\"" << escapeString(key) << "\":";
        if (!value.is<std::nullptr_t>()) {
            oss << value.serialize();
        } else {
            oss << "null";
        }
        first = false;
    }
    
    oss << "}";
    return oss.str();
}

std::string JsonValue::escapeString(const std::string& str) const {
    std::string result;
    result.reserve(str.length() + 10); // 预留一些空间
    
    for (char c : str) {
        switch (c) {
            case '"':  result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\b': result += "\\b"; break;
            case '\f': result += "\\f"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default:
                if (c < 0x20) {
                    // 控制字符
                    result += "\\u";
                    result += "0000";
                    result[result.length() - 2] = "0123456789abcdef"[(c >> 4) & 0xF];
                    result[result.length() - 1] = "0123456789abcdef"[c & 0xF];
                } else {
                    result += c;
                }
                break;
        }
    }
    
    return result;
}

JsonValue& JsonValue::operator[](size_t index) {
    if (!is<JsonArray>()) {
        throw std::runtime_error("Not an array");
    }
    
    auto& arr = get<JsonArray>();
    if (index >= arr.size()) {
        arr.resize(index + 1);
    }
    
    if (!arr[index].is<std::nullptr_t>()) {
        arr[index] = JsonValue();
    }
    
    return arr[index];
}

JsonValue& JsonValue::operator[](const std::string& key) {
    if (!is<JsonObject>()) {
        throw std::runtime_error("Not an object");
    }
    
    auto& obj = get<JsonObject>();
    if (obj.find(key) == obj.end()) {
        obj[key] = JsonValue();
    }
    
    return obj[key];
}

const JsonValue& JsonValue::operator[](size_t index) const {
    if (!is<JsonArray>()) {
        throw std::runtime_error("Not an array");
    }
    
    const auto& arr = get<JsonArray>();
    if (index >= arr.size()) {
        JsonValue nullValue = JsonValue();
        return nullValue;
    }
    
    return arr[index];
}

const JsonValue& JsonValue::operator[](const std::string& key) const {
    if (!is<JsonObject>()) {
        throw std::runtime_error("Not an object");
    }
    
    const auto& obj = get<JsonObject>();
    auto it = obj.find(key);
    if (it == obj.end()) {
        JsonValue nullValue = JsonValue();
        return nullValue;
    }
    
    return it->second;
}

void JsonValue::push_back(JsonValue value) {
    if (!is<JsonArray>()) {
        throw std::runtime_error("Not an array");
    }
    
    get<JsonArray>().push_back(value);
}

size_t JsonValue::size() const {
    if (is<JsonArray>()) {
        return get<JsonArray>().size();
    } else if (is<JsonObject>()) {
        return get<JsonObject>().size();
    } else {
        return 0;
    }
}

bool JsonValue::hasKey(const std::string& key) const {
    if (!is<JsonObject>()) {
        return false;
    }
    
    const auto& obj = get<JsonObject>();
    return obj.find(key) != obj.end();
}

void JsonValue::set(const std::string& key, JsonValue value) {
    if (!is<JsonObject>()) {
        throw std::runtime_error("Not an object");
    }
    
    get<JsonObject>()[key] = value;
}

// 简单的JSON解析器（基础实现）
JsonValue JsonValue::deserialize(const std::string& json) {
    // 这里是一个简化的JSON解析器实现
    // 在实际项目中，建议使用成熟的JSON库如nlohmann/json
    
    std::string trimmed = json;
    // 去除前后空白
    trimmed.erase(0, trimmed.find_first_not_of(" \t\n\r"));
    trimmed.erase(trimmed.find_last_not_of(" \t\n\r") + 1);
    
    if (trimmed.empty()) {
        return JsonValue();
    }
    
    // 解析null
    if (trimmed == "null") {
        return JsonValue(nullptr);
    }
    
    // 解析boolean
    if (trimmed == "true") {
        return JsonValue(true);
    }
    if (trimmed == "false") {
        return JsonValue(false);
    }
    
    // 解析字符串
    if (trimmed.front() == '"' && trimmed.back() == '"') {
        std::string str = trimmed.substr(1, trimmed.length() - 2);
        // 这里应该处理转义字符，简化实现
        return JsonValue(str);
    }
    
    // 解析数字（简化实现）
    try {
        if (trimmed.find('.') != std::string::npos) {
            double val = std::stod(trimmed);
            return JsonValue(val);
        } else {
            int val = std::stoi(trimmed);
            return JsonValue(val);
        }
    } catch (const std::exception&) {
        // 解析失败
    }
    
    // 解析数组（简化实现）
    if (trimmed.front() == '[' && trimmed.back() == ']') {
        JsonArray arr;
        // 这里需要更复杂的解析逻辑
        // 简化实现，返回空数组
        return JsonValue(arr);
    }
    
    // 解析对象（简化实现）
    if (trimmed.front() == '{' && trimmed.back() == '}') {
        JsonObject obj;
        // 这里需要更复杂的解析逻辑
        // 简化实现，返回空对象
        return JsonValue(obj);
    }
    
    throw std::runtime_error("Invalid JSON format");
}
*/