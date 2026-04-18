# SPDX-FileCopyrightText: 2024 Rime community
#
# SPDX-License-Identifier: GPL-3.0-or-later

cmake_minimum_required(VERSION 3.22)
project(rime-webdav LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# =============================================================================
# Find Qt6
# =============================================================================
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(NOT DEFINED Qt6_DIR)
    set(CMAKE_PREFIX_PATH "/usr/lib/cmake" ${CMAKE_PREFIX_PATH})
endif()
find_package(Qt6 REQUIRED COMPONENTS Widgets)

# =============================================================================
# Find system dependencies (pkg-config)
# =============================================================================
find_package(PkgConfig REQUIRED)
pkg_check_modules(CURL REQUIRED libcurl)
pkg_check_modules(OPENSSL REQUIRED openssl)
pkg_check_modules(PUGIXML REQUIRED pugixml)
pkg_check_modules(RIME REQUIRED rime)

# =============================================================================
# WebDAV client — direct source compilation
# Reuse parent targets when this directory is added from the top-level project.
# =============================================================================
if(NOT TARGET wdc)
    set(WDC_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/webdav-client-cpp/sources/callback.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/webdav-client-cpp/sources/client.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/webdav-client-cpp/sources/fsinfo.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/webdav-client-cpp/sources/header.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/webdav-client-cpp/sources/request.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/webdav-client-cpp/sources/urn.cpp"
    )

    add_library(wdc STATIC ${WDC_SOURCES})
    add_library(WDC::libwdc ALIAS wdc)

    target_include_directories(wdc PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/webdav-client-cpp/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/webdav-client-cpp/sources"
        ${CURL_INCLUDE_DIRS}
        ${OPENSSL_INCLUDE_DIRS}
        ${PUGIXML_INCLUDE_DIRS}
    )

    target_link_libraries(wdc PUBLIC
        ${CURL_LIBRARIES}
        ${OPENSSL_LIBRARIES}
        ${PUGIXML_LIBRARIES}
    )
endif()

# =============================================================================
# Shared abstraction sync layer
# =============================================================================
if(NOT TARGET rime_webdav_sync)
    set(PUGIXML_PC ON)
    add_subdirectory(../common "${CMAKE_BINARY_DIR}/shared")
endif()

# =============================================================================
# Qt Configuration UI
# =============================================================================
set(RIME_WEBDAV_QT_SOURCES
    main.cc
    mainwindow.cc
    qt_logger.cc
    file_logger.cc
)

add_executable(rime-webdav ${RIME_WEBDAV_QT_SOURCES})

target_include_directories(rime-webdav PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/
    ${Qt6Widgets_INCLUDE_DIRS}
)

target_link_libraries(rime-webdav PRIVATE
    Qt6::Widgets
    rime_webdav_sync
    WDC::libwdc
    ${CURL_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    ${PUGIXML_LIBRARIES}
    ${RIME_LIBRARIES}
)

# Install
install(TARGETS rime-webdav DESTINATION bin)
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/../rime-webdav.desktop"
        DESTINATION share/applications)
