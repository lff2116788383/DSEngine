if(NOT NPM_EXECUTABLE)
    message(FATAL_ERROR "DSE_BUILD_LAUNCHER requires npm")
endif()

add_custom_target(dse_launcher ALL
    COMMAND ${CMAKE_COMMAND} -E env NPM_CONFIG_CACHE=${DSE_NPM_CACHE_DIR} ${NPM_EXECUTABLE} install
    COMMAND ${CMAKE_COMMAND} -E env NPM_CONFIG_CACHE=${DSE_NPM_CACHE_DIR} ${NPM_EXECUTABLE} run build
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/bin/launcher
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/launcher/main.js
        ${CMAKE_SOURCE_DIR}/launcher/preload.js
        ${CMAKE_SOURCE_DIR}/launcher/index.html
        ${CMAKE_SOURCE_DIR}/launcher/package.json
        ${CMAKE_SOURCE_DIR}/bin/launcher
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/launcher/dist
        ${CMAKE_SOURCE_DIR}/bin/launcher/dist
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/launcher
)

add_custom_target(dse_launcher_exe
    COMMAND ${CMAKE_COMMAND} -E env NPM_CONFIG_CACHE=${DSE_NPM_CACHE_DIR} ${NPM_EXECUTABLE} run package:win
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/launcher
)

add_dependencies(dse_launcher_exe dse_launcher)
