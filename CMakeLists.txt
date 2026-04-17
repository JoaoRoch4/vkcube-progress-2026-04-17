cmake_minimum_required(VERSION 3.20)
project(vkcube CXX C)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Compiler / linker ─────────────────────────────────────────────────────────
if(NOT CMAKE_CXX_COMPILER)
    set(CMAKE_CXX_COMPILER /usr/bin/clang++)
endif()
if(NOT CMAKE_C_COMPILER)
    set(CMAKE_C_COMPILER /usr/bin/clang)
endif()
add_link_options(-fuse-ld=lld)

# ── Paths ─────────────────────────────────────────────────────────────────────
set(IMGUI_DIR "/home/joao/vscode/imgui" CACHE PATH "Dear ImGui source root")
set(APP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/app")
set(TE_DIR "/home/joao/vscode/imgui_test_engine/imgui_test_engine" CACHE PATH "Dear ImGui Test Engine source root")

# ── Dependencies ──────────────────────────────────────────────────────────────
find_package(PkgConfig REQUIRED)
find_package(Vulkan REQUIRED)
pkg_check_modules(SDL3      REQUIRED IMPORTED_TARGET sdl3)
pkg_check_modules(FREETYPE  REQUIRED IMPORTED_TARGET freetype2)
pkg_check_modules(PLUTOSVG  REQUIRED IMPORTED_TARGET plutosvg)
pkg_check_modules(LIBDRM    REQUIRED IMPORTED_TARGET libdrm)
pkg_check_modules(GBM       REQUIRED IMPORTED_TARGET gbm)
pkg_check_modules(LIBPNG    REQUIRED IMPORTED_TARGET libpng)

if(EXISTS "/home/joao/vcpkg/installed/x64-linux")
    list(APPEND CMAKE_PREFIX_PATH "/home/joao/vcpkg/installed/x64-linux")
endif()
find_package(reflectcpp CONFIG REQUIRED)

find_library(MATH_LIB m)

# ── SPIR-V shader compilation ─────────────────────────────────────────────────
find_program(GLSLANG glslangValidator REQUIRED)

set(SPIRV_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/spirv")
file(MAKE_DIRECTORY "${SPIRV_OUTPUT_DIR}")

add_custom_command(
    OUTPUT  "${SPIRV_OUTPUT_DIR}/vkcube.vert.spv.h"
    COMMAND ${GLSLANG} "${APP_DIR}/renderer/vulkan/shaders/vkcube.vert" -V -x -o "${SPIRV_OUTPUT_DIR}/vkcube.vert.spv.h"
    DEPENDS "${APP_DIR}/renderer/vulkan/shaders/vkcube.vert"
    COMMENT "Compiling vkcube.vert → SPIR-V header"
)
add_custom_command(
    OUTPUT  "${SPIRV_OUTPUT_DIR}/vkcube.frag.spv.h"
    COMMAND ${GLSLANG} "${APP_DIR}/renderer/vulkan/shaders/vkcube.frag" -V -x -o "${SPIRV_OUTPUT_DIR}/vkcube.frag.spv.h"
    DEPENDS "${APP_DIR}/renderer/vulkan/shaders/vkcube.frag"
    COMMENT "Compiling vkcube.frag → SPIR-V header"
)
add_custom_target(spirv_headers
    DEPENDS
        "${SPIRV_OUTPUT_DIR}/vkcube.vert.spv.h"
        "${SPIRV_OUTPUT_DIR}/vkcube.frag.spv.h"
)

# ── ImGui sources ─────────────────────────────────────────────────────────────
set(IMGUI_CORE_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/misc/freetype/imgui_freetype.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp
    ${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp
)

# ── ImGui Test Engine sources ────────────────────────────────────────────────
set(IMGUI_TE_SOURCES
    ${TE_DIR}/imgui_te_engine.cpp
    ${TE_DIR}/imgui_te_context.cpp
    ${TE_DIR}/imgui_te_coroutine.cpp
    ${TE_DIR}/imgui_te_exporters.cpp
    ${TE_DIR}/imgui_te_perftool.cpp
    ${TE_DIR}/imgui_te_ui.cpp
    ${TE_DIR}/imgui_te_utils.cpp
    ${TE_DIR}/imgui_capture_tool.cpp
)

# ── App class sources ─────────────────────────────────────────────────────────
set(APP_SOURCES
    ${APP_DIR}/platform/sdl_window.cpp
    ${APP_DIR}/platform/plutosvg_ft_override.cpp
    ${APP_DIR}/renderer/vulkan/vulkan_context.cpp
    ${APP_DIR}/renderer/vulkan/vulkan_emoji_atlas.cpp
    ${APP_DIR}/ui/emoji_atlas.cpp
    ${APP_DIR}/ui/imgui_layer.cpp
    ${APP_DIR}/ui/style_editor.cpp
    ${APP_DIR}/ui/ui_windows.cpp
    ${APP_DIR}/ui/imgui_console.cpp
    ${APP_DIR}/ui/main_menu_bar.cpp
    ${APP_DIR}/ui/test_engine_layer.cpp
    ${APP_DIR}/ui/window_state_toml.cpp
    ${APP_DIR}/hot_loader.cpp
    ${APP_DIR}/renderer/vulkan/vulkan_context_with_cube.cpp
    ${APP_DIR}/ui/app_windows.cpp
    ${APP_DIR}/app.cpp
)

# ── Executable ────────────────────────────────────────────────────────────────
add_executable(vkcube
    main.cpp
    ${APP_DIR}/renderer/vulkan/cube_renderer.cpp
    ${APP_DIR}/renderer/vulkan/esTransform.c
    ${APP_SOURCES}
    ${IMGUI_CORE_SOURCES}
    ${IMGUI_TE_SOURCES}
)

add_dependencies(vkcube spirv_headers)

target_include_directories(vkcube PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${SPIRV_OUTPUT_DIR}
    ${APP_DIR}
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${IMGUI_DIR}/misc/freetype
    ${TE_DIR}
    ${Vulkan_INCLUDE_DIRS}
)

target_compile_definitions(vkcube PRIVATE
    IMGUI_USER_CONFIG="imconfig.h"
    IMGUI_TEST_ENGINE_ENABLE_CAPTURE=1
    IMGUI_DISABLE_STB_IMAGE_WRITE_IMPLEMENTATION
)

target_link_libraries(vkcube PRIVATE
    PkgConfig::SDL3
    PkgConfig::FREETYPE
    PkgConfig::PLUTOSVG
    PkgConfig::LIBDRM
    PkgConfig::GBM
    PkgConfig::LIBPNG
    Vulkan::Vulkan
    reflectcpp::reflectcpp
    ${MATH_LIB}
    ${CMAKE_DL_LIBS}
    pthread
)

target_link_options(vkcube PRIVATE
    -Wl,--wrap=plutosvg_ft_svg_hooks
    -rdynamic
)

target_compile_options(vkcube PRIVATE
    -Wall
    -Wformat
    $<$<CONFIG:Debug>:-g>
    $<$<CONFIG:Release>:-O2>
)

# ── Hot-reload shared library ───────────────────────────────────────────────────────
add_library(hot SHARED
    ${APP_DIR}/hot_module.cpp
)

target_include_directories(hot PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${APP_DIR}
    ${IMGUI_DIR}
)

target_compile_definitions(hot PRIVATE
    IMGUI_USER_CONFIG="imconfig.h"
)

target_compile_options(hot PRIVATE
    -Wall
    $<$<CONFIG:Debug>:-g>
    $<$<CONFIG:Release>:-O2>
)

# Place libhot.so next to the vkcube binary so ./libhot.so resolves at runtime.
set_target_properties(hot PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)
