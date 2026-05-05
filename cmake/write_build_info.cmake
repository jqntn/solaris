set(machina_revision "local build")

find_package(Git QUIET)

if(Git_FOUND)
  execute_process(
      COMMAND "${GIT_EXECUTABLE}" rev-parse --short HEAD
      WORKING_DIRECTORY "${MACHINA_SOURCE_DIR}"
      OUTPUT_VARIABLE machina_git_revision
      OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_QUIET
      RESULT_VARIABLE machina_git_revision_result)

  if(machina_git_revision_result EQUAL 0 AND NOT machina_git_revision STREQUAL "")
    set(machina_revision "${machina_git_revision}")

    execute_process(
        COMMAND "${GIT_EXECUTABLE}" status --porcelain --untracked-files=normal
        WORKING_DIRECTORY "${MACHINA_SOURCE_DIR}"
        OUTPUT_VARIABLE machina_git_status
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE machina_git_status_result)

    if(machina_git_status_result EQUAL 0 AND NOT machina_git_status STREQUAL "")
      string(APPEND machina_revision "-dirty")
    endif()
  endif()
endif()

set(machina_js_revision "${machina_revision}")
string(REPLACE "\\" "\\\\" machina_js_revision "${machina_js_revision}")
string(REPLACE "\"" "\\\"" machina_js_revision "${machina_js_revision}")
string(REPLACE "\n" "\\n" machina_js_revision "${machina_js_revision}")

get_filename_component(machina_build_info_dir "${MACHINA_BUILD_INFO_OUTPUT}" DIRECTORY)
file(MAKE_DIRECTORY "${machina_build_info_dir}")
file(WRITE
     "${MACHINA_BUILD_INFO_OUTPUT}"
     "window.solarisBuild = { revision: \"${machina_js_revision}\" };\n")
