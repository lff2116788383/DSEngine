# write_objects_txt.cmake
# Workaround: CMake 4.3 VS generator may not generate objects.txt for
# WINDOWS_EXPORT_ALL_SYMBOLS in certain configurations.
# This script scans the intermediate directory for .obj files and writes
# their paths to objects.txt, which cmake -E __create_def then consumes.

if(NOT OBJ_DIR)
    message(FATAL_ERROR "OBJ_DIR not set")
endif()

file(GLOB_RECURSE obj_files "${OBJ_DIR}/*.obj")
list(SORT obj_files)

set(content "")
foreach(f ${obj_files})
    string(REPLACE "\\" "/" f "${f}")
    string(APPEND content "${f}\n")
endforeach()

file(WRITE "${OBJ_DIR}/objects.txt" "${content}")
list(LENGTH obj_files count)
message(STATUS "write_objects_txt: ${count} objects -> ${OBJ_DIR}/objects.txt")
