# SPDX-FileCopyrightText: 2024 Rime community
#
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Shared WebDAV sync library — used by both Android (JNI) and Linux (Fcitx5)
# builds. Must be added via add_subdirectory() from the parent CMakeLists.txt.
#
# Parent build must provide (before add_subdirectory):
#   - WDC::libwdc   (WebDAV client; from webdav-client-cpp build)
#   - PUGIXML_PC    (ON = use pkg-config for pugixml; OFF = use CMake target)

# Compute pugixml link interface
if(PUGIXML_PC)
    # Linux: pkg_check_modules creates PUGIXML_LIBRARIES as raw flags
    # Convert -l flags to imported targets
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(PUGIXML_PKGCONF REQUIRED pugixml)
    # Use the raw libraries from pkg-config; SSL is linked via WDC
    set(_shared_pugixml_lib "${PUGIXML_PKGCONF_LIBRARIES}")
else()
    # Android: pugixml is a CMake target from add_subdirectory()
    set(_shared_pugixml_lib pugixml)
endif()

add_library(rime_webdav_sync STATIC
    rime_webdav_sync.cc
)

target_include_directories(rime_webdav_sync PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/webdav-client-cpp/include
)

# C++20 required (Fcitx5 headers need C++20)
target_compile_features(rime_webdav_sync PUBLIC cxx_std_20)

# WDC::libwdc 已在父级配置好
target_link_libraries(rime_webdav_sync PUBLIC
    WDC::libwdc
    ${_shared_pugixml_lib}
)
