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

bool JsonValue::isMember(const std::string& key) const {
    if (!isObject()) {
        return false;
    }
    return data_.obj_val->find(key) != data_.obj_val->end();
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


void JsonValue::skipWhitespace(const std::string& json, size_t& index) {
    while (index < json.size() && (json[index] == ' ' || json[index] == '\t' || json[index] == '\n' || json[index] == '\r')) {
        index++;
    }
}

// 解析字符串（处理转义字符）
std::string JsonValue::parseString(const std::string& json, size_t& index) {
    // 检查是否以双引号开头
    if (index >= json.size() || json[index] != '"') {
        throw std::invalid_argument("Json deserialize: expected '\"' at index " + std::to_string(index));
    }
    index++; // 跳过开头的"

    std::string result;
    while (index < json.size() && json[index] != '"') {
        if (json[index] == '\\') {   // 处理转义字符
            index++;                 // 跳过
            switch (json[index]) {
            case '"':
                result += '"';
                break;   // 还原"
            case '\\':
                result += '\\';
                break;   // 还原\
            case 'b':  result += '\b'; break; // 还原退格
            case 'f':
                result += '\f';
                break;   // 还原换页
            case 'n':
                result += '\n';
                break;   // 还原换行
            case 'r':
                result += '\r';
                break;   // 还原回车
            case 't':
                result += '\t';
                break;   // 还原制表符
            default:
                throw std::invalid_argument("Json deserialize: invalid escape character '" +
                                            std::string(1, json[index]) + "' at index " +
                                            std::to_string(index));   // 普通字符直接添加
            }
        }else{
            result += json[index];
        }
        index++;
    }

    // 检查是否以双引号结尾
    if (index >= json.size() || json[index] != '"') {
        throw std::invalid_argument("Json deserialize: expected '\"' at index " + std::to_string(index));
    }
    index++; // 跳过结尾的"
    return result;
}
// 解析数字（区分int和double）
JsonValue JsonValue::parseNumber(const std::string& json, size_t& index) {
    size_t start = index;
    // 处理负号（如-123）
    if (json[index] == '-') {
        index++;
        if (index >= json.size() || !isdigit(json[index])) {
            throw std::invalid_argument("Json deserialize: invalid number at index " + std::to_string(index));
        }
    }
    // 处理整数部分（如123）
    while (index < json.size() && isdigit(json[index])) {
        index++;
    }
    // 处理小数部分（如有.，则为double，如1.8）
    bool is_double = false;
    if (index < json.size() && json[index] == '.') {
        is_double = true;
        index++;
        if (index >= json.size() || !isdigit(json[index])) {
            throw std::invalid_argument("Json deserialize: invalid decimal at index " + std::to_string(index));
        }
        while (index < json.size() && isdigit(json[index])) {
            index++;
        }
    }
    // 处理指数部分（如有e/E，如123e4，也为double）
    if (index < json.size() && (json[index] == 'e' || json[index] == 'E')) {
        is_double = true;
        index++;
        // 指数部分可带正负号（如123e-4）
        if (index < json.size() && (json[index] == '+' || json[index] == '-')) {
            index++;
        }
        if (index >= json.size() || !isdigit(json[index])) {
            throw std::invalid_argument("Json deserialize: invalid exponent at index " + std::to_string(index));
        }
        while (index < json.size() && isdigit(json[index])) {
            index++;
        }
    }

    // 截取数字字符串，转换为int或double
    std::string num_str = json.substr(start, index - start);
    if (is_double) {
        return JsonValue(std::stod(num_str)); // 转为double
    } else {
        // 检查是否超出int范围（可选，根据需求调整）
        long long num = std::stoll(num_str);
        if (num < INT_MIN || num > INT_MAX) {
            throw std::invalid_argument("Json deserialize: number " + num_str + " out of int range");
        }
        return JsonValue(static_cast<int>(num)); // 转为int
    }
}

// 解析数组（[]包裹）
JsonValue JsonValue::parseArray(const std::string& json, size_t& index) {
    // 检查是否以[开头
    if (index >= json.size() || json[index] != '[') {
        throw std::invalid_argument("Json deserialize: expected '[' at index " + std::to_string(index));
    }
    index++; // 跳过[
    skipWhitespace(json, index);

    JsonArray arr;
    // 解析数组内的值（可能是嵌套类型）
    while (index < json.size() && json[index] != ']') {
        // 解析一个值，添加到数组
        arr.push_back(parseValue(json, index));
        skipWhitespace(json, index);
        // 处理逗号分隔（如[1,2,3]）
        if (index < json.size() && json[index] == ',') {
            index++;
            skipWhitespace(json, index);
            // 逗号后不能直接是]（如[1,]是非法的）
            if (index >= json.size() || json[index] == ']') {
                throw std::invalid_argument("Json deserialize: unexpected ',' at end of array at index " + std::to_string(index));
            }
        } else if (json[index] != ']') { // 既不是,也不是]，格式错误
            throw std::invalid_argument("Json deserialize: expected ',' or ']' at index " + std::to_string(index));
        }
    }

    // 检查是否以]结尾
    if (index >= json.size() || json[index] != ']') {
        throw std::invalid_argument("Json deserialize: expected ']' at index " + std::to_string(index));
    }
    index++; // 跳过]
    return JsonValue(arr); // 返回Array类型的JsonValue
}

// 解析对象（{}包裹）
JsonValue JsonValue::parseObject(const std::string& json, size_t& index) {
    // 检查是否以{开头
    if (index >= json.size() || json[index] != '{') {
        throw std::invalid_argument("Json deserialize: expected '{' at index " + std::to_string(index));
    }
    index++; // 跳过{
    skipWhitespace(json, index);

    JsonObject obj;
    // 解析键值对（如"age":30）
    while (index < json.size() && json[index] != '}') {
        // 1. 解析键（必须是字符串）
        std::string key = parseString(json, index);
        skipWhitespace(json, index);

        // 2. 解析冒号:（键和值之间必须有:）
        if (index >= json.size() || json[index] != ':') {
            throw std::invalid_argument("Json deserialize: expected ':' after key '" + key + "' at index " + std::to_string(index));
        }
        index++; // 跳过:
        skipWhitespace(json, index);

        // 3. 解析值（支持任意JSON类型）
        JsonValue value = parseValue(json, index);
        // 将键值对加入对象（map自动处理键唯一性）
        obj[key] = std::move(value);
        skipWhitespace(json, index);

        // 4. 处理逗号分隔（如"age":30,"city":"NY"）
        if (index < json.size() && json[index] == ',') {
            index++;
            skipWhitespace(json, index);
            // 逗号后不能直接是}（如{"age":30,}是非法的）
            if (index >= json.size() || json[index] == '}') {
                throw std::invalid_argument("Json deserialize: unexpected ',' at end of object at index " + std::to_string(index));
            }
        } else if (json[index] != '}') { // 既不是,也不是}，格式错误
            throw std::invalid_argument("Json deserialize: expected ',' or '}' at index " + std::to_string(index));
        }
    }

    // 检查是否以}结尾
    if (index >= json.size() || json[index] != '}') {
        throw std::invalid_argument("Json deserialize: expected '}' at index " + std::to_string(index));
    }
    index++; // 跳过}
    return JsonValue(obj); // 返回Object类型的JsonValue
}

// 解析值（根据首字符判断类型，递归调用）
JsonValue JsonValue::parseValue(const std::string& json, size_t& index) {
    skipWhitespace(json, index);
    if (index >= json.size()) {
        throw std::invalid_argument("Json deserialize: unexpected end of input");
    }

    char first_char = json[index];
    switch (first_char) {
        case '{': // 对象类型
            return parseObject(json, index);
        case '[': // 数组类型
            return parseArray(json, index);
        case '"': // 字符串类型
            return JsonValue(parseString(json, index));
        case 't': // 布尔值true（首字符t）
            if (index + 3 >= json.size() || json.substr(index, 4) != "true") {
                throw std::invalid_argument("Json deserialize: invalid boolean at index " + std::to_string(index));
            }
            index += 4; // 跳过true
            return JsonValue(true);
        case 'f': // 布尔值false（首字符f）
            if (index + 4 >= json.size() || json.substr(index, 5) != "false") {
                throw std::invalid_argument("Json deserialize: invalid boolean at index " + std::to_string(index));
            }
            index += 5; // 跳过false
            return JsonValue(false);
        case 'n': // Null类型（首字符n）
            if (index + 3 >= json.size() || json.substr(index, 4) != "null") {
                throw std::invalid_argument("Json deserialize: invalid null at index " + std::to_string(index));
            }
            index += 4; // 跳过null
            return JsonValue(); // 默认构造为Null
        case '-': // 负数（首字符-）
        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9': // 正数
            return parseNumber(json, index);
        default: // 未知类型
            throw std::invalid_argument("Json deserialize: unexpected character '" + std::string(1, first_char) + "' at index " + std::to_string(index));
    }
}

// ------------------------------ 核心反序列化函数实现 ------------------------------
JsonValue JsonValue::deserialize(const std::string& json) {
    size_t index = 0;
    // 解析根值（你的例子中根是对象，也支持根为数组、字符串等）
    JsonValue root = parseValue(json, index);
    // 解析完成后，剩余字符必须全是空白（否则有多余字符，格式非法）
    skipWhitespace(json, index);
    if (index != json.size()) {
        throw std::invalid_argument("Json deserialize: unexpected characters after root value at index " + std::to_string(index));
    }
    return root;
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