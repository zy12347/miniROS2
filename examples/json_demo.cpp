#include "json.h"
#include <iostream>
#include <memory>

int main() {
    std::cout << "=== MiniROS2 JSON Demo ===" << std::endl;
    JsonValue json;
    json["name"] = "John";
    json["age"] = 30;
    json["city"] = "New York";
    std::cout << json.serialize() << std::endl;
    return 0;
}
