# Inspired by `CMake Cookbook – Radovan Bast`
set(CPACK_ARCHIVE_COMPONENT_INSTALL "ON")
set(CPACK_PACKAGE_VENDOR "Coveo")
set(CPACK_PACKAGE_DESCRIPTION_FILE "${PROJECT_SOURCE_DIR}/README.md")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Hareflow")
set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGING_INSTALL_PREFIX "")
set(CPACK_SOURCE_IGNORE_FILES "${PROJECT_BINARY_DIR};/.git/;.gitignore")
set(CPACK_SOURCE_GENERATOR "TGZ")
set(CPACK_GENERATOR "TGZ")

if(CMAKE_SYSTEM_NAME MATCHES "Linux") # Important: could be on macOS
    list(APPEND CPACK_GENERATOR "DEB")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "coveo")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS
        libboost-chrono1.71.0
        libboost-container1.71.0
        libboost-context1.71.0
        libboost-coroutine1.71.0
        libboost-date-time1.71.0
        libboost-exception1.71.0
        libboost-regex1.71.0
        libboost-system1.71.0
        libfmt-dev
        libqpid-proton11
    )
endif()
if(WIN32 OR MINGW)
    list(APPEND CPACK_GENERATOR "NSIS")
    set(CPACK_NSIS_PACKAGE_NAME "${PROJECT_NAME}")
    set(CPACK_NSIS_CONTACT "coveo")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
endif()
message(VERBOSE "CPack generators: ${CPACK_GENERATOR}")
include(CPack)
