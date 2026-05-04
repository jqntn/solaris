function(machina_download_nvidia_usd)
    if(EXISTS "${MACHINA_NVIDIA_USD_STAMP}")
        return()
    endif()

    get_filename_component(archive_directory "${MACHINA_NVIDIA_USD_ARCHIVE}" DIRECTORY)
    file(MAKE_DIRECTORY "${archive_directory}")

    if(NOT EXISTS "${MACHINA_NVIDIA_USD_ARCHIVE}")
        set(temporary_archive "${MACHINA_NVIDIA_USD_ARCHIVE}.tmp")
        file(REMOVE "${temporary_archive}")
        file(
            DOWNLOAD "${MACHINA_NVIDIA_USD_URL}" "${temporary_archive}"
            EXPECTED_HASH "SHA256=${MACHINA_NVIDIA_USD_SHA256}"
            SHOW_PROGRESS
            TLS_VERIFY ON)
        file(RENAME "${temporary_archive}" "${MACHINA_NVIDIA_USD_ARCHIVE}")
    endif()

    file(REMOVE_RECURSE "${MACHINA_NVIDIA_USD_ROOT}")
    file(MAKE_DIRECTORY "${MACHINA_NVIDIA_USD_ROOT}")
    file(ARCHIVE_EXTRACT INPUT "${MACHINA_NVIDIA_USD_ARCHIVE}" DESTINATION "${MACHINA_NVIDIA_USD_ROOT}")
    file(REMOVE "${MACHINA_NVIDIA_USD_ARCHIVE}")
    file(WRITE "${MACHINA_NVIDIA_USD_STAMP}" "${MACHINA_NVIDIA_USD_URL}\n")
endfunction()
