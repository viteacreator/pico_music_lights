#pragma once

#include <cstddef>
#include <cstdint>

enum class VuCalibrationCommandType : uint8_t {
    invalid,
    start,
    cancel,
    report_floors,
};

struct VuCalibrationCommand {
    VuCalibrationCommandType type = VuCalibrationCommandType::invalid;
    uint32_t duration_ms = 0u;
};

constexpr uint32_t kDefaultVuCalibrationDurationMs = 2000u;
constexpr uint32_t kMinimumVuCalibrationDurationMs = 250u;
constexpr uint32_t kMaximumVuCalibrationDurationMs = 10000u;
constexpr uint16_t kVuCalibrationRawMaximum = 2047u;
constexpr uint16_t kVuCalibrationSafetyMargin = 8u;

constexpr uint16_t vu_calibrated_floor(uint16_t measured_peak) {
    const uint32_t floor = static_cast<uint32_t>(measured_peak) +
                           kVuCalibrationSafetyMargin;
    return static_cast<uint16_t>(
        floor > kVuCalibrationRawMaximum ? kVuCalibrationRawMaximum : floor);
}

inline bool vu_command_equals(const char* text, const char* expected) {
    if (text == nullptr || expected == nullptr) {
        return false;
    }

    while (*text != '\0' && *expected != '\0' && *text == *expected) {
        ++text;
        ++expected;
    }
    return *text == '\0' && *expected == '\0';
}

inline VuCalibrationCommand parse_vu_calibration_command(const char* text) {
    constexpr const char kStart[] = "vu_noise_calibrate";
    constexpr const char kCancel[] = "vu_noise_calibrate cancel";
    constexpr const char kFloors[] = "vu_noise_floors";

    if (vu_command_equals(text, kFloors)) {
        return {VuCalibrationCommandType::report_floors, 0u};
    }
    if (vu_command_equals(text, kCancel)) {
        return {VuCalibrationCommandType::cancel, 0u};
    }
    if (text == nullptr) {
        return {};
    }

    std::size_t index = 0u;
    while (kStart[index] != '\0' && text[index] == kStart[index]) {
        ++index;
    }
    if (kStart[index] != '\0') {
        return {};
    }
    if (text[index] == '\0') {
        return {VuCalibrationCommandType::start,
                kDefaultVuCalibrationDurationMs};
    }
    if (text[index] != ' ') {
        return {};
    }

    ++index;
    uint32_t duration = 0u;
    bool has_digit = false;
    for (; text[index] != '\0'; ++index) {
        const char character = text[index];
        if (character < '0' || character > '9') {
            return {};
        }
        has_digit = true;
        const uint32_t digit = static_cast<uint32_t>(character - '0');
        if (duration > (kMaximumVuCalibrationDurationMs - digit) / 10u) {
            return {};
        }
        duration = duration * 10u + digit;
    }

    if (!has_digit || duration < kMinimumVuCalibrationDurationMs ||
        duration > kMaximumVuCalibrationDurationMs) {
        return {};
    }
    return {VuCalibrationCommandType::start, duration};
}
