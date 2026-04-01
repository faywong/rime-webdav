# SPDX-FileCopyrightText: 2024 Rime community
#
# SPDX-License-Identifier: GPL-3.0-or-later

cmake_minimum_required(VERSION 3.22)
project(rime-webdav LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# =============================================================================
# Find Fcitx5 — reuse parent context, or discover if standalone build
# =============================================================================
if(NOT TARGET Fcitx5::Core)
    set(CMAKE_PREFIX_PATH "/usr/lib/cmake" ${CMAKE_PREFIX_PATH})
    find_package(Fcitx5Core CONFIG REQUIRED)
    find_package(Fcitx5Config CONFIG REQUIRED)
    find_package(Fcitx5Utils CONFIG REQUIRED)
endif()

# =============================================================================
# System dependencies (pkg-config)
# =============================================================================
if(NOT DEFINED CURL_LIBRARIES)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(CURL REQUIRED libcurl)
    pkg_check_modules(OPENSSL REQUIRED openssl)
    pkg_check_modules(PUGIXML REQUIRED pugixml)
    pkg_check_modules(RIME REQUIRED rime)
endif()

# =============================================================================
# Plugin source files
# =============================================================================
set(WEBDAV_ADDON_SOURCES
    rime_webdav_addon.cc
)

add_library(fcitx-rime-webdav SHARED ${WEBDAV_ADDON_SOURCES})

# Shared abstraction layer and librime headers
target_include_directories(fcitx-rime-webdav PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../shared/
    /usr/include
    /usr/include/Fcitx5/Core
    /usr/include/Fcitx5/Utils
)

target_compile_features(fcitx-rime-webdav PUBLIC cxx_std_20)

# Link Fcitx5, shared sync library, and system deps
target_link_libraries(fcitx-rime-webdav PRIVATE
    Fcitx5::Core
    Fcitx5::Config
    Fcitx5::Utils
    rime_webdav_sync
    ${CURL_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    ${PUGIXML_LIBRARIES}
    ${RIME_LIBRARIES}
)

# Install
install(TARGETS fcitx-rime-webdav LIBRARY
    DESTINATION "${FCITX5_ADDON_INSTALL_DIR}")
install(FILES rime-webdav.conf
    DESTINATION "${FCITX5_ADDON_CONFIG_INSTALL_DIR}")
