#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
#

# Always start by checking if the target already exists.
# This prevents repeated calls but also allows the user to substitute their own 3rd party library
# if they wish to do so.
if (TARGET 3rdParty::SDL3)
    return()
endif()

block()
    # Variables inside a block are scoped to the block body.
    # Putting all of this inside a block lets us basically ensure that any variables set by us before invoking the
    # external 3rdParty CMake file do not have any effect on the outside world, and allows us not to have to save and
    # restore anything except for cache changes.

    # Part 1: Where do you get the library from? Make sure to inform the user of the source of the library and any
    # patches applied.
    # release-3.4.12 is the latest stable SDL3 tag as of this writing (published 2026-07-01). Pinned to a tagged
    # release (not a branch) so the fetch is reproducible; hash was computed locally by downloading the tarball with
    # curl and running `shasum -a 256` on it before writing this file.
    o3de_fetch_content(SDL3
        VERSION "release-3.4.12"
        LICENSE "zlib"
        URL "https://github.com/libsdl-org/SDL/archive/refs/tags/release-3.4.12.tar.gz"
        URL_HASH "b68381f06a7580e63400b3b6eb547ec57d8c3ebde70f9f40e0aba530ba05da27"
        GIT "https://github.com/libsdl-org/SDL.git"
        GIT_HASH "release-3.4.12"
    )

    # Part 2: Set the build settings and trigger the actual execution of the downloaded CMakeLists.txt file.
    # Note that CMAKE_ARGS does NOT WORK for FetchContent_*, only ExternalProject.
    # Thus, you must set any configuration settings here, in the scope in which you call FetchContent_MakeAvailable.
    # These settings will be applied only to the current CMake scope - so it is only worth saving and restoring
    # values from settings from the cache since this is in its own scope inside this block.
    set(CMAKE_MESSAGE_LOG_LEVEL ${O3DE_FETCHCONTENT_MESSAGE_LEVEL})
    set(CMAKE_WARN_DEPRECATED OFF CACHE BOOL "" FORCE)

    # Work around an upstream SDL3 CMake bug on macOS: cmake/sdlplatform.cmake's
    # SDL_DetectCMakePlatform() only ever sets `MACOS` (never `DARWIN`) for a macOS configure, but
    # cmake/sdlchecks.cmake's CheckPTHREAD() branches on `elseif(DARWIN)` -- which can therefore never
    # match on macOS (grepped the fetched SDL-release-3.4.12 tree: `DARWIN` is referenced nowhere else
    # in its cmake/ scripts, so this is dead code, not something specific to our configuration). Without
    # the DARWIN branch, CheckPTHREAD falls through to its generic Unix `else()` case, which appends
    # `-lpthread` to CMAKE_REQUIRED_FLAGS for the HAVE_PTHREADS check_c_source_compiles probe. That flag
    # lands on the compile-only (`-c`) step of the probe, which clang treats as an "unused linker input"
    # warning -- normally harmless, but O3DE's global CMAKE_C_FLAGS already carries `-Wall -Werror`,
    # turning it into a hard error, so HAVE_PTHREADS (and therefore HAVE_SDL_THREADS) silently comes back
    # FALSE and SDL3's own `message(FATAL_ERROR "Threads are needed by many SDL subsystems and may not be
    # disabled")` check trips. This is purely a configure-time probe problem: it happens before
    # o3de_fixup_fetchcontent_targets (below) gets a chance to apply
    # O3DE_COMPILE_OPTION_DISABLE_WARNINGS to the real SDL3 target, so that fixup can't reach it. Suppress
    # warnings for the SDL3 configure/build the same way (`-w`, i.e. O3DE_COMPILE_OPTION_DISABLE_WARNINGS'
    # own flag) so every internal check SDL3 runs -- this one and any other latent ones -- gets the same
    # "3rd party code doesn't have to pass our warnings policy" treatment the built targets already get.
    # Scoped to this block(): CMAKE_C_FLAGS here is a plain (non-CACHE) variable, so it automatically
    # reverts to the real CACHE value once this block() exits; nothing to restore.
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -w")

    # DualSense only needs SDL3's joystick (and, for phase 5, sensor) subsystems -- everything else SDL3 can build
    # (audio/video/render/gpu/camera/haptic/power/etc.) is dead weight for this gem and would drag in system
    # frameworks (CoreAudio, Metal, ...) this gem has no business linking.
    #
    # Option names below were verified against the fetched source itself
    # (SDL-release-3.4.12/CMakeLists.txt, `define_sdl_subsystem` macro invocations around line 250): the actual
    # per-subsystem toggle names are SDL_AUDIO, SDL_VIDEO, SDL_GPU, SDL_RENDER, SDL_CAMERA, SDL_JOYSTICK,
    # SDL_HAPTIC, SDL_HIDAPI, SDL_POWER, SDL_SENSOR, SDL_DIALOG, SDL_TRAY -- these match what the task brief
    # assumed 1:1, no corrections needed. SDL_GPU/SDL_RENDER/SDL_CAMERA are declared with
    # `cmake_dependent_option(... DEPS SDL_VIDEO)`, so they would already be forced OFF once SDL_VIDEO=OFF; they
    # are set explicitly anyway so this file stays self-documenting if SDL3 ever changes that dependency.
    set(SDL_SHARED OFF)
    set(SDL_STATIC ON)
    set(SDL_AUDIO OFF)
    set(SDL_VIDEO OFF)
    set(SDL_RENDER OFF)
    set(SDL_GPU OFF)
    set(SDL_CAMERA OFF)
    set(SDL_JOYSTICK ON)
    set(SDL_HAPTIC OFF)
    set(SDL_POWER OFF)
    set(SDL_SENSOR ON) # kept ON per the phase 5 plan (motion/gyro input), even though nothing uses it yet
    set(SDL_TEST_LIBRARY OFF)
    set(SDL_TESTS OFF)
    set(SDL_EXAMPLES OFF)
    set(SDL_INSTALL OFF)

    # the below line is what actually runs its CMakeList.txt file, recurses into its subfolder, defines targets and
    # so on:
    FetchContent_MakeAvailable(SDL3)

    set(CMAKE_WARN_DEPRECATED ON CACHE BOOL "" FORCE)
endblock()

get_property(this_gem_root GLOBAL PROPERTY "@GEMROOT:${gem_name}@")
ly_get_engine_relative_source_dir(${this_gem_root} relative_this_gem_root)

# With SDL_STATIC=ON and SDL_SHARED=OFF, SDL3's own CMakeLists.txt (see `add_library(SDL3-static STATIC)` and
# `add_library(SDL3::SDL3-static ALIAS SDL3-static)`) creates a REAL target literally named `SDL3-static`, not
# `SDL3`. `SDL3::SDL3` and `SDL3::SDL3-static` both only exist as ALIASes onto it. o3de_fixup_fetchcontent_targets
# needs the real (non-alias) target name, so it is given `SDL3-static` here.
set(SDL3_TARGETS SDL3-static)

o3de_fixup_fetchcontent_targets(
    IDE_FOLDER
        "${relative_this_gem_root}/External"
    TARGETS
        ${SDL3_TARGETS})

# o3de_fixup_fetchcontent_targets aliases the raw target name it was given, so the line above produces
# `3rdParty::SDL3-static`, not `3rdParty::SDL3`. This gem's find_package(SDL3) callers (ly_add_target's automatic
# `3rdParty::PackageName` resolution derives the package name "SDL3" from this Find module's own filename) expect
# `3rdParty::SDL3` to exist, so alias it onto the same real target here, the same way this file is looked up by
# name ("SDL3") rather than by the 3rd party library's own internal target-naming choice ("SDL3-static").
#
# Target-name collision note (see MiniAudio's Findminiaudio.cmake for the general pattern this mirrors): if any
# other gem or engine module ever fetches something of its own and calls
# o3de_fixup_fetchcontent_targets(TARGETS SDL3) (i.e. picks the literal name "SDL3" for an unrelated real target),
# that call and this ALIAS creation would collide, because a given target name may only be defined once, real or
# alias, for the entire configure. Nothing else in this engine tree does that as of this writing (grepped for
# `3rdParty::SDL3` and `TARGET SDL3` before adding this file).
if (NOT TARGET 3rdParty::SDL3)
    add_library(3rdParty::SDL3 ALIAS SDL3-static)
endif()

# Copy headers and license files, as well as a custom "find" file that declares the targets as IMPORTED, into the
# install layout.
FetchContent_GetProperties(SDL3 SOURCE_DIR sdl3_source_dir)
ly_install(FILES ${CMAKE_CURRENT_LIST_DIR}/Installer/FindSDL3.cmake DESTINATION cmake/3rdParty)
ly_install(DIRECTORY ${sdl3_source_dir}/include/SDL3 DESTINATION include COMPONENT CORE)
ly_install(FILES ${sdl3_source_dir}/LICENSE.txt DESTINATION include/SDL3 COMPONENT CORE)

# signal that find_package(SDL3) has succeeded.
# we have to set it on the PARENT_SCOPE since we're in a function
set(SDL3_FOUND TRUE PARENT_SCOPE)

# for extra safety, we'll remove the function from the global scope, so that it can't be called again.
