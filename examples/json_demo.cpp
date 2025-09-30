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
    // json["friends"] = JsonArray({JsonString("Alice"), JsonString("Bob"), JsonString("Charlie")});
    // json["address"] = JsonObject({{"street", JsonString("123 Main St")}, {"city", JsonString("Anytown")}, {"zip", JsonString("12345")}});
    // std::cout << json.serialize() << std::endl;
    return 0;
}
