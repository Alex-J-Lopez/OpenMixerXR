cmake_minimum_required(VERSION 3.20)
project(OpenMixerXR VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── OpenVR (static) ───────────────────────────────────────────────────────────
set(BUILD_SHARED OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/openvr EXCLUDE_FROM_ALL)
# On Win64 the library target name becomes openvr_api64 (see third_party/openvr/src/CMakeLists.txt)

# ── spdlog (header-only) ──────────────────────────────────────────────────────
set(SPDLOG_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_BENCH    OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/spdlog EXCLUDE_FROM_ALL)

# ── GLM (header-only math, fetched once and cached) ───────────────────────────
include(FetchContent)
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        0.9.9.8
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(glm)
# GLM 0.9.9.8 exposes an INTERFACE target called "glm"; create alias for clarity
if(TARGET glm AND NOT TARGET glm::glm)
    add_library(glm::glm ALIAS glm)
endif()

# ── ImGui (Dear ImGui, DX11 backend — no Win32 platform backend needed) ────────
# We compile the core + DX11 renderer directly; platform input comes from
# SteamVR laser pointer events mapped to ImGui IO in DashboardUI.
set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/third_party/imgui)
set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_dx11.cpp
)

# ── Application sources ───────────────────────────────────────────────────────
set(OPENMIXER_SOURCES
    src/main.cpp
    src/D3D11Backend.cpp
    src/PassthroughBox.cpp
    src/ChromaRenderer.cpp
    src/DeviceTracker.cpp
    src/GrabController.cpp
    src/OverlayManager.cpp
    src/DashboardUI.cpp
    ${IMGUI_SOURCES}
)
set(OPENMIXER_HEADERS
    src/Config.h
    src/Logger.h
    src/MathHelpers.h
    src/D3D11Backend.h
    src/PassthroughBox.h
    src/ChromaRenderer.h
    src/DeviceTracker.h
    src/GrabController.h
    src/OverlayManager.h
    src/DashboardUI.h
)

add_executable(OpenMixerXR ${OPENMIXER_SOURCES} ${OPENMIXER_HEADERS})

target_include_directories(OpenMixerXR PRIVATE
    src
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)

target_link_libraries(OpenMixerXR PRIVATE
    openvr_api64                # OpenVR static (Win64 target name)
    d3d11                       # DirectX 11
    dxgi                        # DXGI (required by D3D11)
    spdlog::spdlog_header_only  # spdlog header-only
    glm::glm                    # GLM math
)

target_compile_definitions(OpenMixerXR PRIVATE
    OPENVR_BUILD_STATIC   # tell openvr.h we're linking the static lib, not the DLL
    $<$<EQUAL:${CMAKE_SIZEOF_VOID_P},8>:WIN64>
)

if(MSVC)
    target_compile_options(OpenMixerXR PRIVATE
        /W4         # high warning level
        /utf-8      # source files are UTF-8
        /MP         # parallel compilation
        /wd4201     # suppress: nameless struct/union (common in D3D/Windows headers)
    )
endif()

# ── Post-build: copy resources next to the executable ─────────────────────────
add_custom_command(TARGET OpenMixerXR POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/res/manifest.vrmanifest"
        "$<TARGET_FILE_DIR:OpenMixerXR>/manifest.vrmanifest"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/res/dashboard_icon.png"
        "$<TARGET_FILE_DIR:OpenMixerXR>/dashboard_icon.png"
    COMMENT "Copying OpenMixerXR resources to output directory"
)

# ── Unit tests ────────────────────────────────────────────────────────────────
# box_transform_test exercises pure math (no VR runtime, no D3D11 required).
add_executable(box_transform_test tests/box_transform_test.cpp)

target_include_directories(box_transform_test PRIVATE src)

target_link_libraries(box_transform_test PRIVATE
    openvr_api64    # needed for vr::HmdMatrix34_t type only; no runtime called
    glm::glm
)

target_compile_definitions(box_transform_test PRIVATE
    OPENVR_BUILD_STATIC
    $<$<EQUAL:${CMAKE_SIZEOF_VOID_P},8>:WIN64>
)

if(MSVC)
    target_compile_options(box_transform_test PRIVATE /W4 /utf-8 /wd4201)
endif()
