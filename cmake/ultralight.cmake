set(MACHINA_ULTRALIGHT_SDK_ROOT
    "${CMAKE_CURRENT_SOURCE_DIR}/vendor/ultralight-free-sdk-1.4.0-win-x64"
    CACHE PATH "Path to the Ultralight SDK")

set(MACHINA_ULTRALIGHT_DOWNLOAD_URL "https://ultralig.ht/download")

set(MACHINA_ULTRALIGHT_LIBRARY_NAMES
    Ultralight
    UltralightCore
    WebCore
    AppCore)

set(MACHINA_ULTRALIGHT_RUNTIME_DLL_PATHS
    bin/AppCore.dll
    bin/Ultralight.dll
    bin/UltralightCore.dll
    bin/WebCore.dll)

function(machina_ensure_ultralight_sdk)
    if(NOT IS_DIRECTORY "${MACHINA_ULTRALIGHT_SDK_ROOT}")
        message(
            FATAL_ERROR
                "Ultralight SDK must be downloaded manually.\n"
                "Download: ${MACHINA_ULTRALIGHT_DOWNLOAD_URL}\n"
                "Extract to: ${MACHINA_ULTRALIGHT_SDK_ROOT}")
    endif()
endfunction()

function(machina_ultralight_lib output_variable library_name)
    set("${output_variable}" "${MACHINA_ULTRALIGHT_SDK_ROOT}/lib/${library_name}.lib" PARENT_SCOPE)
endfunction()

function(machina_add_ultralight_sdk)
    machina_ensure_ultralight_sdk()

    set(ultralight_link_libraries)
    foreach(library_name IN LISTS MACHINA_ULTRALIGHT_LIBRARY_NAMES)
        machina_ultralight_lib(library_path "${library_name}")
        list(APPEND ultralight_link_libraries "${library_path}")
    endforeach()

    add_library(machina_ultralight INTERFACE)

    target_include_directories(
        machina_ultralight
        INTERFACE "${MACHINA_ULTRALIGHT_SDK_ROOT}/include")
    target_link_libraries(
        machina_ultralight
        INTERFACE ${ultralight_link_libraries})
endfunction()

function(machina_ultralight_runtime_dlls output_variable)
    set(runtime_dlls)
    foreach(runtime_dll_path IN LISTS MACHINA_ULTRALIGHT_RUNTIME_DLL_PATHS)
        list(APPEND runtime_dlls
             "${MACHINA_ULTRALIGHT_SDK_ROOT}/${runtime_dll_path}")
    endforeach()
    set("${output_variable}" ${runtime_dlls} PARENT_SCOPE)
endfunction()

function(machina_add_ultralight_runtime_copy target runtime_directory)
    machina_ensure_ultralight_sdk()
    machina_ultralight_runtime_dlls(runtime_dlls)

    add_custom_command(
        TARGET "${target}"
        POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different ${runtime_dlls}
                "${runtime_directory}"
        COMMAND "${CMAKE_COMMAND}" -E remove_directory
                "${runtime_directory}/assets/resources"
        COMMAND "${CMAKE_COMMAND}" -E copy_directory
                "${MACHINA_ULTRALIGHT_SDK_ROOT}/resources"
                "${runtime_directory}/assets/resources")
endfunction()

function(machina_copy_ultralight_runtime target)
    machina_add_ultralight_runtime_copy("${target}" "$<TARGET_FILE_DIR:${target}>")
endfunction()
