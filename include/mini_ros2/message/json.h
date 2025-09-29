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
using JsonArray = std::vector<JsonValue>;
using JsonObject = std::map<std::string, JsonValue>;

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
    static JsonValue deserialize(const std::string& json);
    
    // 类型检查
    bool isNull() const { return type() == JsonType::Null; }
    bool isBool() const { return type() == JsonType::Bool; }
    bool isInt() const { return type() == JsonType::Int; }
    bool isDouble() const { return type() == JsonType::Double; }
    bool isString() const { return type() == JsonType::String; }
    bool isArray() const { return type() == JsonType::Array; }
    bool isObject() const { return type() == JsonType::Object; }
    
    // 类型安全的访问（需要先通过isXXX()检查类型）
    bool asBool() const;
    int asInt() const;
    double asDouble() const;
    const std::string& asString() const;
    const JsonArray& asArray() const;
    const JsonObject& asObject() const;
    
    // 数组和对象的修改接口
    JsonArray& asArray();
    JsonObject& asObject();
    
    // 数组操作
    void push_back(std::unique_ptr<JsonValue> value);
    size_t size() const;
    
    // 对象操作
    bool hasKey(const std::string& key) const;
    void set(const std::string& key, JsonValue value);
    
    // 访问器
    JsonValue operator[](size_t index);
    const JsonValue operator[](size_t index) const;
    JsonValue operator[](const std::string& key);
    const JsonValue operator[](const std::string& key) const;
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

    JsonValue& operator=(bool value){
        value_ = value;
        return *this;
    };
private:
    bool value_;
};

class JsonInt : public JsonValue {
public:
    JsonInt(int value) : value_(value) {}
    JsonType type() const override { return JsonType::Int; }
    std::string serialize() const override { return std::to_string(value_); }
    int value() const { return value_; }

    JsonValue& operator=(int value){
        value_ = value;
        return *this;
    };
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

    JsonValue& operator=(double value){
        value_ = value;
        return *this;
    };
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

    JsonValue& operator=(const std::string& value){
        value_ = value;
        return *this;
    };
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
            res += value_[i].serialize();
        }
        res += "]";
        return res;
    }
    JsonArray& value() { return value_; }
    const JsonArray& value() const { return value_; }

    JsonValue& operator=(const JsonArray& value){
        value_ = value;
        return *this;
    };
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
            res += "\"" + pair.first + "\":" + pair.second.serialize();
            ++i;
        }
        res += "}";
        return res;
    }
    JsonObject& value() { return value_; }
    const JsonObject& value() const { return value_; }

    JsonValue& operator=(const JsonObject& value){
        value_ = value;
        return *this;
    };
private:
    JsonObject value_;
};

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