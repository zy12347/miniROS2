#pragma once
#include <string>
#include <vector>
#include <map>
#include <cassert>
#include <stdexcept>
#include <sstream>
#include <memory>
#include <climits>

// JSON 支持的类型枚举（标签）
enum class JsonType {
    Null,
    Bool,
    Int,
    Double,
    String,
    Array,
    Object
};

// 前置声明
class JsonValue;

// 数组和对象的类型定义
using JsonArray = std::vector<JsonValue>;
using JsonObject = std::map<std::string, JsonValue>;

// JSON 值类：使用标签联合实现多类型存储
class JsonValue {
private:
    // 类型标签，标识当前存储的具体类型
    JsonType type_;
    
    // 联合：存储不同类型的值（共享同一块内存）
    union Data {
        bool bool_val;                // 布尔值
        int int_val;                  // 整数
        double double_val;            // 浮点数
        std::string* str_val;         // 字符串（用指针避免联合中存储非POD类型）
        JsonArray* arr_val;           // 数组（指针）
        JsonObject* obj_val;          // 对象（指针）
        
        // 联合的构造函数和析构函数需要显式定义
        Data() {}
        ~Data() {}
    } data_;

    // 辅助函数：销毁当前类型的数据
    void destroyData() {
        switch (type_) {
            case JsonType::String:
                delete data_.str_val;
                break;
            case JsonType::Array:
                delete data_.arr_val;
                break;
            case JsonType::Object:
                delete data_.obj_val;
                break;
            default:
                // 其他类型不需要动态内存管理
                break;
        }
    }

    // 辅助函数：复制数据（用于拷贝构造和赋值）
    void copyData(const JsonValue& other);

public:
    // 构造函数：默认构造为Null类型
    JsonValue() : type_(JsonType::Null) {}

    // 拷贝构造函数
    JsonValue(const JsonValue& other) : type_(other.type_) {
        copyData(other);
    }

    // 移动构造函数
    JsonValue(JsonValue&& other) noexcept : type_(other.type_) {
        // 转移指针所有权，避免内存泄漏
        switch (type_) {
            case JsonType::String:
                data_.str_val = other.data_.str_val;
                other.data_.str_val = nullptr;
                break;
            case JsonType::Array:
                data_.arr_val = other.data_.arr_val;
                other.data_.arr_val = nullptr;
                break;
            case JsonType::Object:
                data_.obj_val = other.data_.obj_val;
                other.data_.obj_val = nullptr;
                break;
            default:
                data_ = other.data_;
                break;
        }
        other.type_ = JsonType::Null;
    }

    // 类型特定的构造函数
    JsonValue(bool value) : type_(JsonType::Bool) {
        data_.bool_val = value;
    }

    JsonValue(int value) : type_(JsonType::Int) {
        data_.int_val = value;
    }

    JsonValue(double value) : type_(JsonType::Double) {
        data_.double_val = value;
    }

    JsonValue(const std::string& value) : type_(JsonType::String) {
        data_.str_val = new std::string(value);
    }

    JsonValue(const char* value) : type_(JsonType::String) {
        data_.str_val = new std::string(value);
    }

    JsonValue(const JsonArray& value) : type_(JsonType::Array) {
        data_.arr_val = new JsonArray(value);
    }

    JsonValue(const JsonObject& value) : type_(JsonType::Object) {
        data_.obj_val = new JsonObject(value);
    }

    // 析构函数：清理动态分配的内存
    ~JsonValue() {
        destroyData();
    }

    // 赋值运算符
    JsonValue& operator=(const JsonValue& other) {
        if (this != &other) {
            destroyData();      // 先销毁当前数据
            type_ = other.type_;
            copyData(other);    // 复制新数据
        }
        return *this;
    }

    // 移动赋值运算符
    JsonValue& operator=(JsonValue&& other) noexcept {
        if (this != &other) {
            destroyData();      // 先销毁当前数据
            type_ = other.type_;
            
            // 转移指针所有权
            switch (type_) {
                case JsonType::String:
                    data_.str_val = other.data_.str_val;
                    other.data_.str_val = nullptr;
                    break;
                case JsonType::Array:
                    data_.arr_val = other.data_.arr_val;
                    other.data_.arr_val = nullptr;
                    break;
                case JsonType::Object:
                    data_.obj_val = other.data_.obj_val;
                    other.data_.obj_val = nullptr;
                    break;
                default:
                    data_ = other.data_;
                    break;
            }
            
            other.type_ = JsonType::Null;
        }
        return *this;
    }

    JsonValue& operator=(const std::string& other) {
        destroyData();      // 先销毁当前数据
        type_ = JsonType::String;
        data_.str_val = new std::string(other);
        return *this;
    }

    JsonValue& operator=(const char* other) {
        destroyData();      // 先销毁当前数据
        type_ = JsonType::String;
        data_.str_val = new std::string(other);
        return *this;
    }

    JsonValue& operator=(const int other) {
        destroyData();      // 先销毁当前数据
        type_ = JsonType::Int;
        data_.int_val = other;
        return *this;
    }

    JsonValue& operator=(const double other) {
        destroyData();      // 先销毁当前数据
        type_ = JsonType::Double;
        data_.double_val = other;
        return *this;
    }

    JsonValue& operator=(const bool other) {
        destroyData();      // 先销毁当前数据
        type_ = JsonType::Bool;
        data_.bool_val = other;
        return *this;
    }

    JsonValue& operator=(const JsonArray& other) {
        destroyData();      // 先销毁当前数据
        type_ = JsonType::Array;
        data_.arr_val = new JsonArray(other);
        return *this;
    }

    JsonValue& operator=(const JsonObject& other) {
        destroyData();      // 先销毁当前数据
        type_ = JsonType::Object;
        data_.obj_val = new JsonObject(other);
        return *this;
    }

    // 类型判断方法
    JsonType type() const { return type_; }
    bool isNull() const { return type_ == JsonType::Null; }
    bool isBool() const { return type_ == JsonType::Bool; }
    bool isInt() const { return type_ == JsonType::Int; }
    bool isDouble() const { return type_ == JsonType::Double; }
    bool isString() const { return type_ == JsonType::String; }
    bool isArray() const { return type_ == JsonType::Array; }
    bool isObject() const { return type_ == JsonType::Object; }

    // 安全访问方法：带类型检查，不匹配则抛出异常
    bool asBool() const {
        if (!isBool()) {
            throw std::domain_error("JsonValue: not a boolean");
        }
        return data_.bool_val;
    }

    int asInt() const {
        if (!isInt()) {
            throw std::domain_error("JsonValue: not an integer");
        }
        return data_.int_val;
    }

    double asDouble() const {
        if (!isDouble()) {
            throw std::domain_error("JsonValue: not a double");
        }
        return data_.double_val;
    }

    const std::string& asString() const {
        if (!isString()) {
            throw std::domain_error("JsonValue: not a string");
        }
        return *data_.str_val;
    }

    const JsonArray& asArray() const {
        if (!isArray()) {
            throw std::domain_error("JsonValue: not an array");
        }
        return *data_.arr_val;
    }

    JsonArray& asArray() {
        if (!isArray()) {
            throw std::domain_error("JsonValue: not an array");
        }
        return *data_.arr_val;
    }

    const JsonObject& asObject() const {
        if (!isObject()) {
            throw std::domain_error("JsonValue: not an object");
        }
        return *data_.obj_val;
    }

    JsonObject& asObject() {
        if (!isObject()) {
            throw std::domain_error("JsonValue: not an object");
        }
        return *data_.obj_val;
    }

    // 数组和对象的操作方法
    void push_back(const JsonValue& value) {
        if (!isArray()) {
            throw std::domain_error("JsonValue: cannot push_back to non-array");
        }
        data_.arr_val->push_back(value);
    }

    size_t size() const {
        if (isArray()) {
            return data_.arr_val->size();
        } else if (isObject()) {
            return data_.obj_val->size();
        }
        throw std::domain_error("JsonValue: size() only valid for array/object");
    }

    // 索引访问（数组）
    const JsonValue& operator[](size_t index) const {
        if (!isArray()) {
            throw std::domain_error("JsonValue: [] index not valid for non-array");
        }
        if (index >= data_.arr_val->size()) {
            throw std::out_of_range("JsonValue: array index out of range");
        }
        return (*data_.arr_val)[index];
    }

    JsonValue& operator[](size_t index) {
        if (!isArray()) {
            throw std::domain_error("JsonValue: [] index not valid for non-array");
        }
        if (index >= data_.arr_val->size()) {
            throw std::out_of_range("JsonValue: array index out of range");
        }
        return (*data_.arr_val)[index];
    }

    // 键访问（对象）
    const JsonValue& operator[](const std::string& key) const {
        if (!isObject()) {
            throw std::domain_error("JsonValue: [] key not valid for non-object");
        }
        auto it = data_.obj_val->find(key);
        if (it == data_.obj_val->end()) {
            throw std::out_of_range("JsonValue: object key not found");
        }
        return it->second;
    }

    JsonValue& operator[](const std::string& key) {
        if(isNull()) {
            type_ = JsonType::Object;
            data_.obj_val = new JsonObject();
        }
        if (!isObject()) {
            throw std::domain_error("JsonValue: [] key not valid for non-object");
        }
        return (*data_.obj_val)[key]; // 自动插入新键值对（如果不存在）
    }

    bool isMember(const std::string& key) const;
    // 序列化：将JSON值转换为字符串
    std::string serialize() const;

    static JsonValue deserialize(const std::string& json);

private:
    // 辅助函数：转义字符串中的特殊字符
    std::string escapeString(const std::string& str) const;

    static void skipWhitespace(const std::string& json, size_t& index);
    // 辅助函数：解析双引号包裹的字符串（处理转义），返回解析后的字符串，更新索引
    static std::string parseString(const std::string& json, size_t& index);
    // 辅助函数：解析值（递归支持嵌套对象/数组），返回对应的JsonValue，更新索引
    static JsonValue parseValue(const std::string& json, size_t& index);
    // 辅助函数：解析对象（{}包裹的键值对），返回JsonValue（Object类型），更新索引
    static JsonValue parseObject(const std::string& json, size_t& index);
    // 辅助函数：解析数组（[]包裹的值列表），返回JsonValue（Array类型），更新索引（支持数组场景）
    static JsonValue parseArray(const std::string& json, size_t& index);
    // 辅助函数：解析数字（int或double），返回对应的JsonValue，更新索引
    static JsonValue parseNumber(const std::string& json, size_t& index);
};


/*
#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <sstream>
#include <cassert>

// 前向声明
class JsonValue;

// 类型定义
using JsonArray = std::vector<std::unique_ptr<JsonValue>>;
using JsonObject = std::map<std::string, std::unique_ptr<JsonValue>>;

// JSON值的类型枚举
enum class JsonType {
    Null,
    Bool,
    Int,
    Double,
    String,
    Array,
    Object
};

// 基类
class JsonValue {
public:
    virtual ~JsonValue() = default;
    
    // 获取类型
    virtual JsonType type() const = 0;
    
    // 序列化
    virtual std::string serialize() const = 0;
    
    // 反序列化（静态方法）
    // JsonValue deserialize(const std::string& json);
    
    // 类型检查
    bool isNull() const { return type() == JsonType::Null; }
    bool isBool() const { return type() == JsonType::Bool; }
    bool isInt() const { return type() == JsonType::Int; }
    bool isDouble() const { return type() == JsonType::Double; }
    bool isString() const { return type() == JsonType::String; }
    bool isArray() const { return type() == JsonType::Array; }
    bool isObject() const { return type() == JsonType::Object; }
    
    // 类型安全的访问（需要先通过isXXX()检查类型）
    virtual bool asBool() const;
    virtual int asInt() const;
    virtual double asDouble() const;
    virtual const std::string& asString() const;
    virtual const JsonArray& asArray() const;
    virtual const JsonObject& asObject() const;
    
    // 数组操作
    void push_back(std::unique_ptr<JsonValue> value);
    size_t size() const;


    
    // 对象操作
    // bool hasKey(const std::string& key) const;
    // void set(const std::string& key, JsonValue value);
    
    // 访问器
    std::unique_ptr<JsonValue> operator[](size_t index);
    const std::unique_ptr<JsonValue>& operator[](size_t index) const;
    std::unique_ptr<JsonValue> operator[](const std::string& key);
    const std::unique_ptr<JsonValue>& operator[](const std::string& key) const;
};

// 派生类实现
class JsonNull : public JsonValue {
public:
    JsonType type() const override { return JsonType::Null; }
    std::string serialize() const override { return "null"; }

    
};

class JsonBool : public JsonValue {
public:
    JsonBool(bool value) : value_(value) {}
    JsonType type() const override { return JsonType::Bool; }
    std::string serialize() const override { return value_ ? "true" : "false"; }
    bool value() const { return value_; }
private:
    bool value_;
};

class JsonInt : public JsonValue {
public:
    JsonInt(int value) : value_(value) {}
    JsonType type() const override { return JsonType::Int; }
    std::string serialize() const override { return std::to_string(value_); }
    int value() const { return value_; }

private:
    int value_;
};

class JsonDouble : public JsonValue {
public:
    JsonDouble(double value) : value_(value) {}
    JsonType type() const override { return JsonType::Double; }
    std::string serialize() const override { 
        std::stringstream ss;
        ss << value_;
        return ss.str();
    }
    double value() const { return value_; }
private:
    double value_;
};

class JsonString : public JsonValue {
public:
    JsonString(const std::string& value) : value_(value) {}
    JsonType type() const override { return JsonType::String; }
    std::string serialize() const override { 
        return "\"" + escapeString(value_) + "\""; 
    }
    const std::string& value() const { return value_; }
private:
    std::string value_;
    std::string escapeString(const std::string& str) const {
        std::string res;
        for (char c : str) {
            switch (c) {
                case '"': res += "\\\""; break;
                case '\\': res += "\\\\"; break;
                case '\b': res += "\\b"; break;
                case '\f': res += "\\f"; break;
                case '\n': res += "\\n"; break;
                case '\r': res += "\\r"; break;
                case '\t': res += "\\t"; break;
                default: res += c;
            }
        }
        return res;
    }
};

class JsonArrayImpl : public JsonValue {
public:
    JsonArrayImpl() = default;
    JsonArrayImpl(JsonArray arr) : value_(std::move(arr)) {}
    JsonType type() const override { return JsonType::Array; }
    std::string serialize() const override {
        std::string res = "[";
        for (size_t i = 0; i < value_.size(); ++i) {
            if (i > 0) res += ",";
            res += value_[i]->serialize();
        }
        res += "]";
        return res;
    }
    JsonArray& value() { return value_; }
    const JsonArray& value() const { return value_; }
private:
    JsonArray value_;
};

class JsonObjectImpl : public JsonValue {
public:
    JsonObjectImpl() = default;
    JsonObjectImpl(JsonObject obj) : value_(std::move(obj)) {}
    JsonType type() const override { return JsonType::Object; }
    std::string serialize() const override {
        std::string res = "{";
        size_t i = 0;
        for (const auto& pair : value_) {
            if (i > 0) res += ",";
            res += "\"" + pair.first + "\":" + pair.second->serialize();
            ++i;
        }
        res += "}";
        return res;
    }
    JsonObject& value() { return value_; }
    const JsonObject& value() const { return value_; }

private:
    JsonObject value_;
};

*/

/*
'''
C++17
#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <variant>
#include <sstream>
// 前向声明
class JsonValue;

// JSON值的类型定义
// using JsonValuePtr = std::shared_ptr<JsonValue>;
using JsonArray = std::vector<JsonValue>;
using JsonObject = std::map<std::string, JsonValue>;

// 使用variant存储不同类型的JSON值
using JsonVariant = std::variant<
    std::nullptr_t,    // Null
    bool,              // Bool
    int,               // Int
    double,            // Double
    std::string,       // String
    JsonArray,         // Array
    JsonObject         // Object
>;

class JsonValue {
public:
    // 构造函数
    JsonValue() : value_(JsonObject()) {}//默认初始化object
    JsonValue(std::nullptr_t) : value_(nullptr) {}
    JsonValue(bool val) : value_(val) {}
    JsonValue(int val) : value_(val) {}//整数
    JsonValue(double val) : value_(val) {}//浮点数
    JsonValue(const std::string& val) : value_(val) {}//字符串
    JsonValue(const char* val) : value_(std::string(val)) {}
    JsonValue(const JsonArray& val) : value_(val) {}
    JsonValue(const JsonObject& val) : value_(val) {}
    
    // 移动构造函数
    JsonValue(JsonArray&& val) : value_(std::move(val)) {}
    JsonValue(JsonObject&& val) : value_(std::move(val)) {}
    
    // 析构函数
    ~JsonValue() = default;
    
    // 序列化
    std::string serialize() const;
    
    // 反序列化（静态方法）
    static JsonValue deserialize(const std::string& json);
    
    // 类型安全的访问器
    template<typename T>
    T get() const {
        return std::get<T>(value_);
    }//get<int>()
    
    // 检查是否为特定类型
    template<typename T>
    bool is() const {
        return std::holds_alternative<T>(value_);
    }//is<int>()
    
    
    // 数组和对象访问
    JsonValue& operator[](size_t index);
    JsonValue& operator[](const std::string& key);
    const JsonValue& operator[](size_t index) const;
    const JsonValue& operator[](const std::string& key) const;
    
    JsonValue& operator=(int value) {
        value_ = value;
        return *this;
    }

    JsonValue& operator=(double value) {
        value_ = value;
        return *this;
    }

    JsonValue& operator=(bool value) {
        value_ = value;
        return *this;
    }

    JsonValue& operator=(const std::string& value) {
        value_ = value;
        return *this;
    }

    JsonValue& operator=(const char* value) {  // 支持字符串字面量
        value_ = std::string(value);
        return *this;
    }

    JsonValue& operator=(std::nullptr_t) {
        value_ = nullptr;
        return *this;
    }

    JsonValue& operator=(const JsonArray& value) {
        value_ = value;
        return *this;
    }

    JsonValue& operator=(const JsonObject& value) {
        value_ = value;
        return *this;
    }

    // 数组操作
    void push_back(JsonValue value);
    size_t size() const;
    
    // 对象操作
    bool hasKey(const std::string& key) const;
    void set(const std::string& key, JsonValue value);
    
private:
    JsonVariant value_;
    
    // 辅助方法
    std::string serializeValue() const;
    std::string serializeArray() const;
    std::string serializeObject() const;
    std::string escapeString(const std::string& str) const;
};
'''
*/