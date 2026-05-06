cmake_minimum_required(VERSION 3.28)

project(solaris LANGUAGES CXX)

# Toolchain
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>")

# Dependencies
set(FETCHCONTENT_BASE_DIR "${CMAKE_SOURCE_DIR}/out/vendor")
set(FETCHCONTENT_QUIET OFF)
include(FetchContent)

FetchContent_Declare(
    raylib
    URL "https://github.com/raysan5/raylib/archive/cf9f27db545dd686b03f9f773cfbdb8a8dd9f72f.tar.gz"
    URL_HASH SHA256=2ebe3e90cd66b16233a370406b7abc76392d45713a4302f14a68e87e129090a1
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    EXCLUDE_FROM_ALL)
FetchContent_MakeAvailable(raylib)

FetchContent_Declare(
    nvidia_usd
    URL "https://developer.nvidia.com/downloads/usd/usd_binaries/25.08/usd.py312.windows-x86_64.usdview.release-v25.08.71e038c1.zip"
    URL_HASH SHA256=61bae28d18c873871047e7a8b3fe1ffe2188fb88fdde113be429812d27f0c8b4
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    EXCLUDE_FROM_ALL)
FetchContent_MakeAvailable(nvidia_usd)

find_package(EnTT CONFIG REQUIRED)

# SDK layout
set(USD_ROOT "${nvidia_usd_SOURCE_DIR}")
set(ULTRALIGHT_ROOT "${CMAKE_SOURCE_DIR}/vendor/ultralight-free-sdk-1.4.0-win-x64")
set(ASSETS_RUNTIME_ROOT assets)
set(MATERIALX_RUNTIME_ROOT materialx)

set(USD_LIBS
    usd_usdShade
    usd_usdGeom
    usd_usd
    usd_sdf
    usd_vt
    usd_tf
    usd_python
    MaterialXGenGlsl
    MaterialXGenShader
    MaterialXFormat
    MaterialXCore
    tbb)

set(USD_DLLS
    bin/MaterialXCore.dll
    bin/MaterialXFormat.dll
    bin/MaterialXGenGlsl.dll
    bin/MaterialXGenShader.dll
    bin/tbb.dll
    lib/usd_ar.dll
    lib/usd_arch.dll
    lib/usd_gf.dll
    lib/usd_js.dll
    lib/usd_kind.dll
    lib/usd_pcp.dll
    lib/usd_plug.dll
    lib/usd_python.dll
    lib/usd_sdf.dll
    lib/usd_sdr.dll
    lib/usd_tf.dll
    lib/usd_trace.dll
    lib/usd_ts.dll
    lib/usd_usd.dll
    lib/usd_usdGeom.dll
    lib/usd_usdShade.dll
    lib/usd_vt.dll
    lib/usd_work.dll
    python/python312.dll)

set(USD_RESOURCE_DIRS ar sdf sdr usd usdGeom usdMtlx usdShade)

set(ULTRALIGHT_LIBS Ultralight UltralightCore WebCore AppCore)
set(ULTRALIGHT_DLLS bin/AppCore.dll bin/Ultralight.dll bin/UltralightCore.dll bin/WebCore.dll)

# SDK targets
add_library(nvidia_usd INTERFACE)
target_include_directories(nvidia_usd INTERFACE
    "${USD_ROOT}/include"
    "${USD_ROOT}/python/include")
target_compile_definitions(nvidia_usd INTERFACE NOMINMAX)
foreach(lib IN LISTS USD_LIBS)
    target_link_libraries(nvidia_usd INTERFACE "${USD_ROOT}/lib/${lib}.lib")
endforeach()
target_link_libraries(nvidia_usd INTERFACE "${USD_ROOT}/python/libs/python312.lib")

add_library(ultralight INTERFACE)
target_include_directories(ultralight INTERFACE "${ULTRALIGHT_ROOT}/include")
foreach(lib IN LISTS ULTRALIGHT_LIBS)
    target_link_libraries(ultralight INTERFACE "${ULTRALIGHT_ROOT}/lib/${lib}.lib")
endforeach()

# clang-tidy
find_program(CLANG_TIDY_EXE
    NAMES clang-tidy clang-tidy.exe
    HINTS "C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/Llvm/bin")

set(CMAKE_CXX_CLANG_TIDY
    "${CLANG_TIDY_EXE}"
    "--config-file=${CMAKE_SOURCE_DIR}/.clang-tidy"
    "--header-filter=^.*machina[\\\\/](include|src|tests)[\\\\/]"
    "--extra-arg=--target=x86_64-pc-windows-msvc"
    "--extra-arg=/EHsc")

# Engine and executable
add_library(machina_engine STATIC
    src/level_instantiator.cpp
    src/materialx_shader_generator.cpp
    src/renderer.cpp
    src/runtime_paths.cpp
    src/scene.cpp
    src/strategic_camera_controller.cpp
    src/usd_level_loader.cpp
    src/web_overlay.cpp)
target_include_directories(machina_engine PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(machina_engine PUBLIC cxx_std_23)
target_compile_definitions(machina_engine PUBLIC
    MACHINA_ASSETS_ROOT="${ASSETS_RUNTIME_ROOT}"
    MACHINA_MATERIALX_LIBRARY_ROOT="${MATERIALX_RUNTIME_ROOT}")
target_link_libraries(machina_engine PUBLIC raylib EnTT::EnTT nvidia_usd ultralight)

add_executable(solaris
    src/game_scene.cpp
    src/main.cpp
    src/main_menu_scene.cpp
    src/menu_commands.cpp)
target_link_libraries(solaris PRIVATE machina_engine)

# clang-format
find_program(CLANG_FORMAT_EXE
    NAMES clang-format clang-format.exe
    HINTS "C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/Llvm/bin")

file(GLOB_RECURSE FORMAT_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/include/*.hpp"
    "${CMAKE_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_SOURCE_DIR}/tests/*.cpp")

add_custom_target(format ALL
    COMMAND "${CLANG_FORMAT_EXE}" -i ${FORMAT_SOURCES}
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    VERBATIM)

add_dependencies(machina_engine format)
add_dependencies(solaris format)

# Build info
add_custom_target(build_info ALL
    BYPRODUCTS "${CMAKE_SOURCE_DIR}/assets/web/build-info.js"
    COMMAND powershell -NoProfile -Command [[Set-Content assets/web/build-info.js -Value ('window.solarisBuild = { revision: "' + (git describe --always --dirty=-dirty) + '" };')]]
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    VERBATIM)

add_dependencies(solaris build_info)

# Runtime symlinks
file(GLOB ASSETS_CHILDREN
    RELATIVE "${CMAKE_SOURCE_DIR}/assets"
    CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/assets/*")

foreach(cfg IN LISTS CMAKE_CONFIGURATION_TYPES)
    set(rt "${CMAKE_BINARY_DIR}/${cfg}")
    file(MAKE_DIRECTORY "${rt}/${ASSETS_RUNTIME_ROOT}" "${rt}/usd" "${rt}/${MATERIALX_RUNTIME_ROOT}")

    foreach(dll IN LISTS USD_DLLS)
        get_filename_component(name "${dll}" NAME)
        file(REMOVE "${rt}/${name}")
        file(CREATE_LINK "${USD_ROOT}/${dll}" "${rt}/${name}" SYMBOLIC)
    endforeach()

    foreach(dll IN LISTS ULTRALIGHT_DLLS)
        get_filename_component(name "${dll}" NAME)
        file(REMOVE "${rt}/${name}")
        file(CREATE_LINK "${ULTRALIGHT_ROOT}/${dll}" "${rt}/${name}" SYMBOLIC)
    endforeach()

    foreach(child IN LISTS ASSETS_CHILDREN)
        file(REMOVE "${rt}/${ASSETS_RUNTIME_ROOT}/${child}")
        file(CREATE_LINK "${CMAKE_SOURCE_DIR}/assets/${child}"
                         "${rt}/${ASSETS_RUNTIME_ROOT}/${child}" SYMBOLIC)
    endforeach()

    file(REMOVE "${rt}/${ASSETS_RUNTIME_ROOT}/resources")
    file(CREATE_LINK "${ULTRALIGHT_ROOT}/resources"
                     "${rt}/${ASSETS_RUNTIME_ROOT}/resources" SYMBOLIC)

    file(REMOVE "${rt}/usd/plugInfo.json")
    file(CREATE_LINK "${USD_ROOT}/lib/usd/plugInfo.json"
                     "${rt}/usd/plugInfo.json" SYMBOLIC)

    foreach(d IN LISTS USD_RESOURCE_DIRS)
        file(REMOVE "${rt}/usd/${d}")
        file(CREATE_LINK "${USD_ROOT}/lib/usd/${d}" "${rt}/usd/${d}" SYMBOLIC)
    endforeach()

    file(REMOVE "${rt}/${MATERIALX_RUNTIME_ROOT}/libraries")
    file(CREATE_LINK "${USD_ROOT}/libraries"
                     "${rt}/${MATERIALX_RUNTIME_ROOT}/libraries" SYMBOLIC)
endforeach()

# Tests
enable_testing()

function(solaris_test target test_name)
    add_executable(${target} ${ARGN})
    target_link_libraries(${target} PRIVATE machina_engine)
    add_test(NAME ${test_name}
             COMMAND ${target}
             WORKING_DIRECTORY "$<TARGET_FILE_DIR:${target}>")
endfunction()

solaris_test(machina_level_smoke level_smoke tests/level_smoke.cpp)
solaris_test(machina_strategic_camera strategic_camera_controller tests/strategic_camera_controller_test.cpp)
solaris_test(machina_scene_stack scene_stack tests/scene_stack.cpp)
solaris_test(solaris_menu_commands menu_commands tests/menu_commands.cpp src/menu_commands.cpp)

# Distribution
set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION ".")
include(InstallRequiredSystemLibraries)

install(TARGETS solaris RUNTIME DESTINATION .)
install(DIRECTORY assets/ DESTINATION "${ASSETS_RUNTIME_ROOT}")
install(DIRECTORY "${ULTRALIGHT_ROOT}/resources/" DESTINATION "${ASSETS_RUNTIME_ROOT}/resources")

foreach(dll IN LISTS USD_DLLS)
    install(FILES "${USD_ROOT}/${dll}" DESTINATION .)
endforeach()

foreach(dll IN LISTS ULTRALIGHT_DLLS)
    install(FILES "${ULTRALIGHT_ROOT}/${dll}" DESTINATION .)
endforeach()

install(FILES "${USD_ROOT}/lib/usd/plugInfo.json" DESTINATION usd)

foreach(d IN LISTS USD_RESOURCE_DIRS)
    install(DIRECTORY "${USD_ROOT}/lib/usd/${d}/" DESTINATION "usd/${d}")
endforeach()

install(DIRECTORY "${USD_ROOT}/libraries/" DESTINATION "${MATERIALX_RUNTIME_ROOT}/libraries")

add_custom_target(distrib
    COMMAND "${CMAKE_COMMAND}" --install "${CMAKE_BINARY_DIR}"
            --config Release
            --prefix "${CMAKE_SOURCE_DIR}/out/distrib")

add_dependencies(distrib solaris build_info)
