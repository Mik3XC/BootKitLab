cmake_minimum_required(VERSION 3.20)
project(BootKitStudio LANGUAGES CXX ASM)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

enable_testing()

add_library(stage_simulators OBJECT
  bootkitstudio/asm/mbr_stage_marker.S
  bootkitstudio/asm/vbr_stage_marker.S
  bootkitstudio/asm/uefi_stage_marker.S
  bootkitstudio/asm/acpi_annotation_marker.S
)

add_library(bootkitstudio_core
  bootkitstudio/src/util.cpp
  bootkitstudio/src/yaml_like_parser.cpp
  bootkitstudio/src/hash_utils.cpp
  bootkitstudio/src/guardrails.cpp
  bootkitstudio/src/source_claims.cpp
  bootkitstudio/src/simulator_registry.cpp
  bootkitstudio/src/qemu.cpp
  bootkitstudio/src/runner.cpp
  $<TARGET_OBJECTS:stage_simulators>
)

target_include_directories(bootkitstudio_core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/bootkitstudio/include
)

add_executable(bootkitstudio
  bootkitstudio/src/main.cpp
)

target_link_libraries(bootkitstudio PRIVATE bootkitstudio_core)

add_executable(bootkitstudio_unit_tests
  tests/unit_tests.cpp
)

target_link_libraries(bootkitstudio_unit_tests PRIVATE bootkitstudio_core)
target_compile_definitions(bootkitstudio_unit_tests PRIVATE BKS_REPO_ROOT="${CMAKE_CURRENT_SOURCE_DIR}")

add_executable(bootkitstudio_integration_tests
  tests/integration_tests.cpp
)

target_link_libraries(bootkitstudio_integration_tests PRIVATE bootkitstudio_core)
target_compile_definitions(bootkitstudio_integration_tests PRIVATE BKS_REPO_ROOT="${CMAKE_CURRENT_SOURCE_DIR}")

add_executable(bootkitstudio_qemu_tests
  tests/qemu_tests.cpp
)

target_link_libraries(bootkitstudio_qemu_tests PRIVATE bootkitstudio_core)
target_compile_definitions(bootkitstudio_qemu_tests PRIVATE BKS_REPO_ROOT="${CMAKE_CURRENT_SOURCE_DIR}")

add_test(NAME bootkitstudio_unit_tests COMMAND bootkitstudio_unit_tests)
add_test(NAME bootkitstudio_integration_tests COMMAND bootkitstudio_integration_tests)
add_test(NAME bootkitstudio_qemu_tests COMMAND bootkitstudio_qemu_tests)
