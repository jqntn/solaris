include("${CMAKE_CURRENT_LIST_DIR}/download_nvidia_usd.cmake")

set(
    MACHINA_NVIDIA_USD_URL
    "https://developer.nvidia.com/downloads/usd/usd_binaries/25.08/usd.py312.windows-x86_64.usdview.release-v25.08.71e038c1.zip")
set(MACHINA_NVIDIA_USD_SHA256 "61bae28d18c873871047e7a8b3fe1ffe2188fb88fdde113be429812d27f0c8b4")
set(MACHINA_NVIDIA_USD_ARCHIVE_NAME "usd.py312.windows-x86_64.usdview.release-v25.08.71e038c1.zip")
set(MACHINA_NVIDIA_USD_VENDOR_DIR "${CMAKE_CURRENT_SOURCE_DIR}/out/vendor")
set(MACHINA_NVIDIA_USD_ROOT "${MACHINA_NVIDIA_USD_VENDOR_DIR}/nvidia-usd-25.08")
set(MACHINA_NVIDIA_USD_ARCHIVE "${MACHINA_NVIDIA_USD_VENDOR_DIR}/${MACHINA_NVIDIA_USD_ARCHIVE_NAME}")
set(MACHINA_NVIDIA_USD_STAMP "${MACHINA_NVIDIA_USD_ROOT}/.machina_nvidia_usd_25.08.stamp")

set(MACHINA_NVIDIA_USD_LIBRARY_NAMES
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

set(MACHINA_NVIDIA_USD_RUNTIME_DLL_PATHS
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

set(MACHINA_NVIDIA_USD_RUNTIME_RESOURCE_DIRS ar sdf sdr usd usdGeom usdMtlx usdShade)

function(machina_nvidia_usd_lib output_variable library_name)
    set("${output_variable}" "${MACHINA_NVIDIA_USD_ROOT}/lib/${library_name}.lib" PARENT_SCOPE)
endfunction()

function(machina_ensure_nvidia_usd_sdk)
    message(STATUS "Ensuring NVIDIA OpenUSD 25.08 SDK")
    machina_download_nvidia_usd()
endfunction()

function(machina_nvidia_usd_runtime_dlls output_variable)
    set(runtime_dlls)
    foreach(dll_path IN LISTS MACHINA_NVIDIA_USD_RUNTIME_DLL_PATHS)
        list(APPEND runtime_dlls "${MACHINA_NVIDIA_USD_ROOT}/${dll_path}")
    endforeach()
    set("${output_variable}" ${runtime_dlls} PARENT_SCOPE)
endfunction()

function(machina_add_nvidia_usd_runtime_copy target runtime_directory)
    machina_nvidia_usd_runtime_dlls(runtime_dlls)

    add_custom_command(
        TARGET "${target}"
        POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different ${runtime_dlls} "${runtime_directory}"
        COMMAND "${CMAKE_COMMAND}" -E remove_directory "${runtime_directory}/usd"
        COMMAND "${CMAKE_COMMAND}" -E remove_directory "${runtime_directory}/usd_plugins"
        COMMAND "${CMAKE_COMMAND}" -E remove_directory
                "${runtime_directory}/${MACHINA_MATERIALX_LIBRARY_RUNTIME_ROOT}/libraries"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${runtime_directory}/usd"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${MACHINA_NVIDIA_USD_ROOT}/lib/usd/plugInfo.json"
                "${runtime_directory}/usd"
        COMMAND "${CMAKE_COMMAND}" -E copy_directory
                "${MACHINA_NVIDIA_USD_ROOT}/libraries"
                "${runtime_directory}/${MACHINA_MATERIALX_LIBRARY_RUNTIME_ROOT}/libraries")

    foreach(resource_dir IN LISTS MACHINA_NVIDIA_USD_RUNTIME_RESOURCE_DIRS)
        add_custom_command(
            TARGET "${target}"
            POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_directory
                    "${MACHINA_NVIDIA_USD_ROOT}/lib/usd/${resource_dir}"
                    "${runtime_directory}/usd/${resource_dir}")
    endforeach()
endfunction()

function(machina_add_nvidia_usd_sdk)
    machina_ensure_nvidia_usd_sdk()

    set(nvidia_usd_link_libraries)
    foreach(library_name IN LISTS MACHINA_NVIDIA_USD_LIBRARY_NAMES)
        machina_nvidia_usd_lib(library_path "${library_name}")
        list(APPEND nvidia_usd_link_libraries "${library_path}")
    endforeach()
    list(APPEND nvidia_usd_link_libraries "${MACHINA_NVIDIA_USD_ROOT}/python/libs/python312.lib")

    add_library(machina_nvidia_usd INTERFACE)

    target_compile_definitions(machina_nvidia_usd INTERFACE NOMINMAX)
    target_include_directories(
        machina_nvidia_usd
        INTERFACE "${MACHINA_NVIDIA_USD_ROOT}/include"
                  "${MACHINA_NVIDIA_USD_ROOT}/python/include")
    target_link_libraries(
        machina_nvidia_usd
        INTERFACE ${nvidia_usd_link_libraries})

    set(MACHINA_MATERIALX_LIBRARY_RUNTIME_ROOT "materialx" PARENT_SCOPE)
endfunction()

function(machina_copy_nvidia_usd_runtime target)
    machina_add_nvidia_usd_runtime_copy("${target}" "$<TARGET_FILE_DIR:${target}>")
endfunction()
