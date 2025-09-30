#include "json.h"
#include <iostream>
#include <memory>
int main() {
    std::cout << "=== MiniROS2 JSON Demo ===" << std::endl;
    JsonValue json;
    json["name"] = "John";
    json["age"] = 30;
    json["city"] = "New York";
    json["is_student"] = true;
    json["height"] = 1.8;
    json["weight"] = 70;
    std::cout << json.serialize() << std::endl;
    std::string json_str = json.serialize();
    JsonValue json2 = JsonValue::deserialize(json_str);
    if(json2.isMember("name")) {
        std::cout << json2["name"].serialize() << std::endl;
    }
    if(json2.isMember("age")) {
        std::cout << json2["age"].serialize() << std::endl;
    }
    if(json2.isMember("city")) {
        std::cout << json2["city"].serialize() << std::endl;
    }
    if(json2.isMember("is_student")) {
        std::cout << json2["is_student"].serialize() << std::endl;
    }
    if(json2.isMember("height")) {
        std::cout << json2["height"].serialize() << std::endl;
    }
    if(json2.isMember("weight")) {
        std::cout << json2["weight"].serialize() << std::endl;
    }
    // std::cout << json2.serialize() << std::endl;
    // json["friends"] = JsonArray({JsonString("Alice"), JsonString("Bob"), JsonString("Charlie")});
    // json["address"] = JsonObject({{"street", JsonString("123 Main St")}, {"city", JsonString("Anytown")}, {"zip", JsonString("12345")}});
    // std::cout << json.serialize() << std::endl;
    return 0;
}
