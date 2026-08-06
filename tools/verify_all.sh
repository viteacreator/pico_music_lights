#!/usr/bin/env bash
set -Eeuo pipefail
IFS=$'\n\t'
export PYTHONDONTWRITEBYTECODE=1

readonly SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
readonly REPO_ROOT="$(cd -- "${SCRIPT_DIR}/.." && pwd -P)"
readonly EXPECTED_TESTS=(led_logic_tests audio_processing_tests spectrum_analysis_tests diagnostic_renderer_tests effect_engine_tests device_configuration_tests)
readonly REQUIRED_COMMANDS=(cmake ninja gcc g++ clang clang++ python3 git rg picotool arm-none-eabi-gcc arm-none-eabi-g++ arm-none-eabi-readelf arm-none-eabi-objcopy arm-none-eabi-size arm-none-eabi-nm arm-none-eabi-objdump)

for command_name in "${REQUIRED_COMMANDS[@]}"; do
    command -v "${command_name}" >/dev/null || { echo "missing executable: ${command_name}" >&2; exit 2; }
done
[[ -z "$(git -C "${REPO_ROOT}" status --porcelain=v1 --untracked-files=all)" ]] || {
    echo "verification requires a clean repository working tree" >&2
    exit 2
}

if [[ -n "${VERIFY_OUTPUT_DIR:-}" ]]; then
    [[ ! -e "${VERIFY_OUTPUT_DIR}" && ! -L "${VERIFY_OUTPUT_DIR}" ]] || { echo "VERIFY_OUTPUT_DIR must not already exist" >&2; exit 2; }
    OUTPUT_PARENT="$(realpath -e "$(dirname -- "${VERIFY_OUTPUT_DIR}")")"
    OUTPUT_DIR="${OUTPUT_PARENT}/$(basename -- "${VERIFY_OUTPUT_DIR}")"
    case "${OUTPUT_DIR}/" in "${REPO_ROOT}/"*) echo "VERIFY_OUTPUT_DIR must be outside the repository" >&2; exit 2;; esac
    mkdir -- "${OUTPUT_DIR}"
else
    OUTPUT_DIR="$(mktemp -d "${TMPDIR:-/tmp}/pico-music-lights-verify.XXXXXXXX")"
fi
readonly OUTPUT_DIR
readonly SUMMARY="${OUTPUT_DIR}/verification-summary.txt"
: > "${SUMMARY}"

fail() { printf 'FAIL: %s\n' "$*" | tee -a "${SUMMARY}" >&2; exit 1; }
stage() { printf '\n== %s ==\n' "$*" | tee -a "${SUMMARY}"; }
pass() { printf 'PASS: %s\n' "$*" | tee -a "${SUMMARY}"; }
trap 'rc=$?; if ((rc != 0)); then printf "verification=FAIL exit_code=%d output_dir=%s\n" "$rc" "${OUTPUT_DIR}" | tee -a "${SUMMARY}" >&2; fi' EXIT

stage "Prerequisites and pinned versions"
: "${PICO_SDK_PATH:?PICO_SDK_PATH must name Pico SDK 2.3.0}"
[[ -f "${PICO_SDK_PATH}/pico_sdk_version.cmake" ]] || fail "invalid PICO_SDK_PATH: ${PICO_SDK_PATH}"
SDK_VERSION="$(awk '/^    set\(PICO_SDK_VERSION_(MAJOR|MINOR|REVISION) [0-9]+\)/ { value=$2; gsub(/\)/, "", value); versions[++count]=value } END { if (count == 3) print versions[1] "." versions[2] "." versions[3] }' "${PICO_SDK_PATH}/pico_sdk_version.cmake")"
[[ "${SDK_VERSION}" == "2.3.0" ]] || fail "Pico SDK must be 2.3.0, found ${SDK_VERSION:-unknown}"
ARM_VERSION="$(arm-none-eabi-gcc -dumpfullversion)"
[[ "${ARM_VERSION}" == "15.2.1" ]] || fail "Arm GCC must be 15.2.1, found ${ARM_VERSION}"
ARM_BANNER="$(arm-none-eabi-gcc --version | head -1)"
[[ "${ARM_BANNER}" == *"Arm GNU Toolchain 15.2.Rel1"* ]] || fail "Arm toolchain must be Arm GNU Toolchain 15.2.Rel1, found ${ARM_BANNER}"
ARM_GCC_REAL="$(realpath "$(command -v arm-none-eabi-gcc)")"
ARM_BIN_DIR="$(dirname "${ARM_GCC_REAL}")"
for arm_tool in arm-none-eabi-g++ arm-none-eabi-readelf arm-none-eabi-objcopy arm-none-eabi-size arm-none-eabi-nm arm-none-eabi-objdump; do
    [[ "$(dirname "$(realpath "$(command -v "${arm_tool}")")")" == "${ARM_BIN_DIR}" ]] || fail "mixed Arm toolchain executable: ${arm_tool}"
done
PICOTOOL_VERSION="$(picotool version 2>&1)"
[[ "${PICOTOOL_VERSION}" == picotool\ v2.3.0* ]] || fail "picotool must be 2.3.0, found ${PICOTOOL_VERSION}"
for item in "gcc=$(gcc --version | head -1)" "clang=$(clang --version | head -1)" "arm_gcc=$(arm-none-eabi-gcc --version | head -1)" "pico_sdk=${SDK_VERSION} (${PICO_SDK_PATH})" "picotool=${PICOTOOL_VERSION}"; do printf '%s\n' "$item" | tee -a "${SUMMARY}"; done
PICOTOOL_REAL="$(realpath "$(command -v picotool)")"
PICOTOOL_CMAKE_DIR=""
for candidate in "$(dirname "${PICOTOOL_REAL}")" "$(dirname "${ARM_BIN_DIR}")/lib/cmake/picotool" /usr/local/lib/cmake/picotool; do
    if [[ -f "${candidate}/picotoolConfig.cmake" ]]; then PICOTOOL_CMAKE_DIR="$(realpath -e "${candidate}")"; break; fi
done
[[ -n "${PICOTOOL_CMAKE_DIR}" ]] || fail "no installed picotool 2.3.0 CMake package found"
pass "prerequisites"

run_host() {
    local name="$1" compiler="$2" sanitizer="$3"
    local build_dir="${OUTPUT_DIR}/host-${name}" log="${OUTPUT_DIR}/host-${name}.log"
    stage "Host ${name}"
    cmake -S "${REPO_ROOT}/tests/host" -B "${build_dir}" -G Ninja \
        -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_CXX_COMPILER="${compiler}" \
        -DPICO_MUSIC_LIGHTS_HOST_SANITIZER="${sanitizer}" 2>&1 | tee "${log}"
    cmake --build "${build_dir}" --verbose 2>&1 | tee -a "${log}"
    PYTHONDONTWRITEBYTECODE=1 python3 - "${build_dir}" "${EXPECTED_TESTS[@]}" <<'PY'
import json, subprocess, sys
build, expected = sys.argv[1], sys.argv[2:]
data = json.loads(subprocess.check_output(["ctest", "--test-dir", build, "--show-only=json-v1"], text=True))
actual = [test["name"] for test in data["tests"]]
if sorted(actual) != sorted(expected) or len(actual) != len(expected):
    raise SystemExit(f"required test mismatch: expected={expected}, actual={actual}")
PY
    local -a env_args=()
    if [[ "${sanitizer}" == address ]]; then env_args=(ASAN_OPTIONS=halt_on_error=1:detect_leaks=1:abort_on_error=1); fi
    if [[ "${sanitizer}" == undefined ]]; then env_args=(UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1); fi
    env "${env_args[@]}" ctest --test-dir "${build_dir}" --output-on-failure --no-tests=error 2>&1 | tee -a "${log}"
    if rg -i '(^|[^[:alpha:]])warning:' "${log}" >/dev/null; then fail "compiler warning detected in ${name}"; fi
    if rg -i '(AddressSanitizer|UndefinedBehaviorSanitizer|runtime error:)' "${log}" >/dev/null; then fail "sanitizer diagnostic detected in ${name}"; fi
    pass "host_${name} suites=6"
}
run_host gcc g++ none
run_host clang clang++ none
run_host asan g++ address
run_host ubsan g++ undefined

stage "Pico W Release firmware"
for forbidden in PICO_NO_PICOTOOL PICO_NO_UF2; do [[ -z "${!forbidden:-}" ]] || fail "${forbidden} must not be set"; done
FIRMWARE_DIR="${OUTPUT_DIR}/firmware"
FIRMWARE_LOG="${OUTPUT_DIR}/firmware.log"
ARM_TOOLCHAIN_ROOT="$(dirname "$(dirname "$(realpath "$(command -v arm-none-eabi-gcc)")")")"
cmake -S "${REPO_ROOT}" -B "${FIRMWARE_DIR}" -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DPICO_BOARD=pico_w -DPICO_SDK_PATH="${PICO_SDK_PATH}" \
    -DPICO_TOOLCHAIN_PATH="${ARM_TOOLCHAIN_ROOT}" \
    -Dpicotool_DIR="${PICOTOOL_CMAKE_DIR}" \
    2>&1 | tee "${FIRMWARE_LOG}"
rg '^PICO_BOARD:STRING=pico_w$' "${FIRMWARE_DIR}/CMakeCache.txt" >/dev/null || fail "configured board is not pico_w"
rg "^PICO_SDK_PATH:PATH=${PICO_SDK_PATH//\//\\/}$" "${FIRMWARE_DIR}/CMakeCache.txt" >/dev/null || fail "configured SDK path differs"
rg 'CMAKE_C_COMPILER:(FILEPATH|STRING)=.*/arm-none-eabi-gcc$' "${FIRMWARE_DIR}/CMakeCache.txt" >/dev/null || fail "unexpected Arm compiler"
PYTHONDONTWRITEBYTECODE=1 python3 "${SCRIPT_DIR}/check_toolchain_path.py" \
    --cache "${FIRMWARE_DIR}/CMakeCache.txt" \
    --expected-root "${ARM_TOOLCHAIN_ROOT}" \
    --expected-bin "${ARM_BIN_DIR}" \
    --expected-gcc "${ARM_GCC_REAL}" | tee -a "${SUMMARY}" || fail "unexpected Arm toolchain path"
rg "^picotool_DIR:[A-Z]+=${PICOTOOL_CMAKE_DIR}$" "${FIRMWARE_DIR}/CMakeCache.txt" >/dev/null || fail "unexpected picotool configuration"
rg -F "Using picotool from ${PICOTOOL_REAL}" "${FIRMWARE_LOG}" >/dev/null || fail "firmware did not select the validated picotool"
if rg -i 'Downloading Picotool|No installed picotool' "${FIRMWARE_LOG}" >/dev/null; then fail "picotool fallback detected"; fi
for language in C CXX ASM; do
    configured="$(sed -n "s/^CMAKE_${language}_COMPILER:[A-Z]*=//p" "${FIRMWARE_DIR}/CMakeCache.txt")"
    [[ "$(realpath "${configured}")" == "${ARM_BIN_DIR}/arm-none-eabi-$([[ ${language} == CXX ]] && echo g++ || echo gcc)" ]] || fail "unexpected ${language} compiler: ${configured}"
done
cmake --build "${FIRMWARE_DIR}" --verbose 2>&1 | tee -a "${FIRMWARE_LOG}"
if rg -i '(^|[^[:alpha:]])warning:' "${FIRMWARE_LOG}" >/dev/null; then fail "firmware compiler warning detected"; fi
PREFIX="${FIRMWARE_DIR}/pico_music_lights"
arm-none-eabi-objdump -d -S "${PREFIX}.elf" > "${PREFIX}.dis"
PYTHONDONTWRITEBYTECODE=1 python3 "${SCRIPT_DIR}/validate_firmware.py" --prefix "${PREFIX}" \
    --tool-prefix arm-none-eabi- --picotool "$(command -v picotool)" \
    --report "${OUTPUT_DIR}/firmware-size-report.txt" | tee -a "${SUMMARY}"
pass "pico_w_release artifacts_validated"

stage "Repository integrity"
[[ -z "$(git -C "${REPO_ROOT}" status --porcelain=v1 --untracked-files=all)" ]] || { git -C "${REPO_ROOT}" status --short >&2; fail "verification modified repository content"; }
pass "repository_unchanged"
printf 'verification=PASS configurations=5 visible_stages=7 host_suites_each=6 output_dir=%s\n' "${OUTPUT_DIR}" | tee -a "${SUMMARY}"
trap - EXIT
