#include "numerick/builtin_tools.hpp"

#include <cassert>
#include <cctype>
#include <iostream>
#include <string>

using numerick::Json;
using numerick::ToolRegistry;

int main() {
    ToolRegistry registry;

    numerick::registerBuiltinTools(registry);

    // The built-in tool must be registered.
    assert(registry.contains("get_local_time"));

    // It must appear in the published tool definitions.
    const auto definitions = registry.definitions();

    assert(definitions.size() == 1);
    assert(definitions[0].name == "get_local_time");
    assert(definitions[0].parametersSchema["type"] == "object");
    assert(definitions[0].parametersSchema["additionalProperties"]
           == false);

    // The tool must return a non-empty, correctly shaped timestamp.
    const auto result = registry.execute(
        "get_local_time",
        Json::object()
    );

    assert(result.success);
    assert(result.output.size() == 19);
    assert(result.output[4] == '-');
    assert(result.output[7] == '-');
    assert(result.output[10] == ' ');
    assert(result.output[13] == ':');
    assert(result.output[16] == ':');

    for (std::size_t i = 0; i < result.output.size(); ++i) {
        if (i == 4 || i == 7 || i == 10 ||
            i == 13 || i == 16) {
            continue;
        }

        assert(std::isdigit(
            static_cast<unsigned char>(result.output[i])
        ));
    }

    // Extra arguments must be rejected.
    const auto invalid = registry.execute(
        "get_local_time",
        {{"unexpected", "value"}}
    );

    assert(!invalid.success);

    std::cout << "All built-in tool tests passed.\n";

    return 0;
}
