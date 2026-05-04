function(machina_msvc_redist_dlls output_variable)
  get_filename_component(cl_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
  get_filename_component(vc_dir "${cl_dir}/../../../../../.." ABSOLUTE)

  file(
    GLOB
    msvc_crt_dirs
    LIST_DIRECTORIES true
    "${vc_dir}/Redist/MSVC/*/x64/Microsoft.VC*.CRT")

  list(SORT msvc_crt_dirs COMPARE NATURAL)
  list(LENGTH msvc_crt_dirs msvc_crt_dir_count)
  math(EXPR msvc_crt_dir_last "${msvc_crt_dir_count} - 1")
  list(GET msvc_crt_dirs "${msvc_crt_dir_last}" msvc_crt_dir)

  file(GLOB msvc_redist_dlls "${msvc_crt_dir}/*.dll")
  set("${output_variable}" ${msvc_redist_dlls} PARENT_SCOPE)
endfunction()

function(machina_add_distribution_target)
  cmake_parse_arguments(
    MACHINA_DISTRIB
    ""
    "TARGET;OUTPUT_DIR"
    ""
    ${ARGN})

  machina_msvc_redist_dlls(msvc_redist_dlls)

  add_custom_target(
    distrib
    COMMAND "${CMAKE_COMMAND}" -E remove_directory "${MACHINA_DISTRIB_OUTPUT_DIR}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${MACHINA_DISTRIB_OUTPUT_DIR}"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "$<TARGET_FILE:${MACHINA_DISTRIB_TARGET}>"
            "${MACHINA_DISTRIB_OUTPUT_DIR}"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
            "$<TARGET_FILE_DIR:${MACHINA_DISTRIB_TARGET}>/assets"
            "${MACHINA_DISTRIB_OUTPUT_DIR}/assets"
    COMMENT "Assembling machina distribution")

  add_dependencies(distrib "${MACHINA_DISTRIB_TARGET}")
  machina_add_nvidia_usd_runtime_copy(distrib "${MACHINA_DISTRIB_OUTPUT_DIR}")

  add_custom_command(
    TARGET distrib
    POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different ${msvc_redist_dlls}
            "${MACHINA_DISTRIB_OUTPUT_DIR}")
endfunction()
