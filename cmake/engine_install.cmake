# ── Install rules ────────────────────────────────────────────────
# Extracted from root CMakeLists.txt for maintainability.
# Requires: DSE_BUILD_SHARED, CMAKE_SOURCE_DIR, CMAKE_BINARY_DIR,
#           CMAKE_INSTALL_*, cmake/DSEngineConfig.cmake.in

# 1. 引擎目标（DLL/LIB）
if(DSE_BUILD_SHARED)
install(TARGETS dse_engine
    EXPORT DSEngineTargets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}  COMPONENT sdk
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}  COMPONENT sdk
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}  COMPONENT sdk
)

# 2. 引擎公共头文件
install(DIRECTORY ${CMAKE_SOURCE_DIR}/engine/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/DSEngine/engine
    COMPONENT sdk
    FILES_MATCHING PATTERN "*.h"
    PATTERN "*.h.in" EXCLUDE
    REGEX "render/rhi/(dx11|opengl|vulkan)/" EXCLUDE
)
# 2b. CMake 生成的版本头文件
install(FILES ${CMAKE_BINARY_DIR}/generated/engine/dse_version.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/DSEngine/engine
    COMPONENT sdk
)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/modules/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/DSEngine/modules
    COMPONENT sdk
    FILES_MATCHING PATTERN "*.h"
)

# 3. 第三方依赖公共头文件
install(DIRECTORY ${CMAKE_SOURCE_DIR}/depends/glm/glm/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/DSEngine/third_party/glm/glm
    COMPONENT sdk
)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/depends/glm_ext/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/DSEngine/third_party/glm_ext
    COMPONENT sdk
    FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp" PATTERN "*.inl"
)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/depends/entt-3.13.0/src/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/DSEngine/third_party/entt/src
    COMPONENT sdk
    FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h"
)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/depends/box2d-2.4.1/include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/DSEngine/third_party/box2d/include
    COMPONENT sdk
    FILES_MATCHING PATTERN "*.h"
)

# 4. Lua 脚本
install(DIRECTORY ${CMAKE_SOURCE_DIR}/script/
    DESTINATION ${CMAKE_INSTALL_DATADIR}/DSEngine/script
    COMPONENT sdk
    FILES_MATCHING PATTERN "*.lua"
)

# 5. 导出目标文件
install(EXPORT DSEngineTargets
    FILE DSEngineTargets.cmake
    NAMESPACE DSEngine::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/DSEngine
    COMPONENT sdk
)
endif() # DSE_BUILD_SHARED

# 6. 生成并安装 Package Config 文件
configure_package_config_file(
    ${CMAKE_SOURCE_DIR}/cmake/DSEngineConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/DSEngineConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/DSEngine
)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/DSEngineConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/DSEngineConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/DSEngineConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/DSEngine
    COMPONENT sdk
)
