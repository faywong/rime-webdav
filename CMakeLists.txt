# SPDX-FileCopyrightText: 2024 Rime community
#
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Standalone CMakeLists.txt for fcitx5-rime-webdav.
#
# Build:
#   mkdir build && cd build
#   cmake .. -DCMAKE_BUILD_TYPE=Release
#   cmake --build . -j$(nproc)
#
# Install:
#   sudo cmake --install .

cmake_minimum_required(VERSION 3.22)
project(fcitx5-rime-webdav LANGUAGES CXX)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# =============================================================================
# Installation paths
# =============================================================================
if(NOT DEFINED FCITX5_ADDON_INSTALL_DIR)
    set(FCITX5_ADDON_INSTALL_DIR "/usr/lib/fcitx5/")
endif()
if(NOT DEFINED FCITX5_ADDON_CONFIG_INSTALL_DIR)
    set(FCITX5_ADDON_CONFIG_INSTALL_DIR "/usr/share/fcitx5/addon/")
endif()

# =============================================================================
# Find Fcitx5 — CMake config mode (Arch Linux /usr/lib/cmake/)
# =============================================================================
set(CMAKE_PREFIX_PATH "/usr/lib/cmake" ${CMAKE_PREFIX_PATH})
find_package(Fcitx5Core CONFIG REQUIRED)
find_package(Fcitx5Config CONFIG REQUIRED)
find_package(Fcitx5Utils CONFIG REQUIRED)

# =============================================================================
# Find system dependencies (pkg-config)
# =============================================================================
find_package(PkgConfig REQUIRED)
pkg_check_modules(CURL REQUIRED libcurl)
pkg_check_modules(OPENSSL REQUIRED openssl)
pkg_check_modules(PUGIXML REQUIRED pugixml)
pkg_check_modules(RIME REQUIRED rime)

find_library(RIME_LIBRARY NAMES rime fcitx5-rime)

# =============================================================================
# WebDAV client — direct source compilation
# =============================================================================
set(WDC_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/webdav-client-cpp/sources/callback.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/webdav-client-cpp/sources/client.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/webdav-client-cpp/sources/fsinfo.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/webdav-client-cpp/sources/header.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/webdav-client-cpp/sources/request.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/webdav-client-cpp/sources/urn.cpp"
)

add_library(wdc STATIC ${WDC_SOURCES})
add_library(WDC::libwdc ALIAS wdc)

target_include_directories(wdc PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/webdav-client-cpp/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/webdav-client-cpp/sources"
    ${CURL_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIRS}
    ${PUGIXML_INCLUDE_DIRS}
)

target_link_libraries(wdc PUBLIC
    ${CURL_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    ${PUGIXML_LIBRARIES}
)

# =============================================================================
# Shared abstraction sync layer
# =============================================================================
set(PUGIXML_PC ON)
add_subdirectory(common "${CMAKE_BINARY_DIR}/shared")

# =============================================================================
# Build rime-webdav UI plugin
# =============================================================================
add_subdirectory(ui)
