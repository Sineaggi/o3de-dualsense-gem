#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
#

# This file is used in the INSTALLER version of O3DE.
# This file is included in cmake/3rdParty, which is already part of the search path for Findxxxxx.cmake files.
if (TARGET 3rdParty::SDL3)
    return()
endif()

# It is still worth notifying people that they are accepting a 3rd Party Library here, and what license it uses, and
# where to get it.
set(SDL3_GIT_REPO "https://github.com/libsdl-org/SDL.git")
set(SDL3_GIT_TAG "release-3.4.12")
message(STATUS "DualSense Gem uses ${SDL3_GIT_REPO} ${SDL3_GIT_TAG} (zlib license)")

set(BASE_LIBRARY_FOLDER "${LY_ROOT_FOLDER}/lib/${PAL_PLATFORM_NAME}")

# SDL3 itself names its static-library CMake target `SDL3-static` (see FindSDL3.cmake in the folder above, and
# SDL's own CMakeLists.txt `add_library(SDL3-static STATIC)`), not `SDL3`. We import it under that same real
# name here, then alias both `3rdParty::SDL3-static` (matching SDL3's own naming) and `3rdParty::SDL3` (matching
# this Find module's name, and what find_package(SDL3) callers expect) onto it.
add_library(SDL3-static STATIC IMPORTED GLOBAL)
set_target_properties(SDL3-static PROPERTIES
    IMPORTED_LOCATION         "${BASE_LIBRARY_FOLDER}/profile/${CMAKE_STATIC_LIBRARY_PREFIX}SDL3-static${CMAKE_STATIC_LIBRARY_SUFFIX}"
    IMPORTED_LOCATION_DEBUG   "${BASE_LIBRARY_FOLDER}/debug/${CMAKE_STATIC_LIBRARY_PREFIX}SDL3-static${CMAKE_STATIC_LIBRARY_SUFFIX}"
    IMPORTED_LOCATION_RELEASE "${BASE_LIBRARY_FOLDER}/release/${CMAKE_STATIC_LIBRARY_PREFIX}SDL3-static${CMAKE_STATIC_LIBRARY_SUFFIX}")
ly_target_include_system_directories(TARGET SDL3-static INTERFACE "${LY_ROOT_FOLDER}/include")
add_library(3rdParty::SDL3-static ALIAS SDL3-static)
add_library(3rdParty::SDL3 ALIAS SDL3-static)

# notify O3DE that we have satisfied the SDL3 find_package requirements.
set(SDL3_FOUND TRUE)
