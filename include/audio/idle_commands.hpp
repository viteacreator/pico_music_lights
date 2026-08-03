#pragma once

#include <cstddef>
#include <cstdint>

enum class IdleCommandType : uint8_t {
    invalid,
    enable,
    disable,
    status,
    test,
};

inline IdleCommandType parse_idle_command(const char* text) {
    if (text == nullptr) {
        return IdleCommandType::invalid;
    }

    struct CommandEntry {
        const char* text;
        IdleCommandType type;
    };
    constexpr CommandEntry kCommands[] = {
        {"idle_enable", IdleCommandType::enable},
        {"idle_disable", IdleCommandType::disable},
        {"idle_status", IdleCommandType::status},
        {"idle_test", IdleCommandType::test},
    };
    for (const CommandEntry& command : kCommands) {
        std::size_t index = 0u;
        while (text[index] != '\0' && command.text[index] != '\0' &&
               text[index] == command.text[index]) {
            ++index;
        }
        if (text[index] == '\0' && command.text[index] == '\0') {
            return command.type;
        }
    }
    return IdleCommandType::invalid;
}
