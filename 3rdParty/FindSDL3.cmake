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
    set(sdl3_version "release-3.4.12")
    set(sdl3_license "zlib")
    set(sdl3_url "https://github.com/libsdl-org/SDL/archive/refs/tags/release-3.4.12.tar.gz")
    set(sdl3_url_hash "b68381f06a7580e63400b3b6eb547ec57d8c3ebde70f9f40e0aba530ba05da27")
    set(sdl3_git_repo "https://github.com/libsdl-org/SDL.git")
    set(sdl3_git_tag "release-3.4.12")

    # o3de_fetch_content (cmake/3rdPartyPackages.cmake) is only defined when this file is being processed as
    # part of a SOURCE-built engine's own configure (it's pulled in transitively via cmake/LYPython.cmake, which
    # is only included from the engine root CMakeLists.txt itself). On an installed/downloaded O3DE SDK, that
    # root CMakeLists.txt is never processed -- the SDK ships a generated, much smaller cmake/ surface -- so
    # `o3de_fetch_content` simply does not exist there and a project that registers this gem against such an
    # engine fails to configure at all ("unknown command o3de_fetch_content"). Detect that case with a plain
    # `if(COMMAND ...)` and fall back to driving stock CMake FetchContent directly; this keeps the exact same
    # pinned version/hash and produces the same SDL3 target either way. O3DE_FORCE_STOCK_FETCHCONTENT is a
    # manual escape hatch (pass -DO3DE_FORCE_STOCK_FETCHCONTENT=TRUE) to exercise/verify the fallback path even
    # on a source-built engine where o3de_fetch_content is otherwise available.
    if(COMMAND o3de_fetch_content AND NOT O3DE_FORCE_STOCK_FETCHCONTENT)
        o3de_fetch_content(SDL3
            VERSION "${sdl3_version}"
            LICENSE "${sdl3_license}"
            URL "${sdl3_url}"
            URL_HASH "${sdl3_url_hash}"
            GIT "${sdl3_git_repo}"
            GIT_HASH "${sdl3_git_tag}"
        )
    else()
        # Stock-CMake fallback for installed/downloaded engines. Mirrors the parts of o3de_fetch_content that
        # matter for correctness here: DOWNLOAD_EXTRACT_TIMESTAMP on modern CMake (avoids the CMP0135 warning
        # and non-deterministic extracted timestamps), a URL-first attempt with a GIT_REPOSITORY/GIT_TAG
        # fallback if the URL can't be fetched, and the same license/version/source notification
        # o3de_fetch_content prints (that notification is a real feature -- users are meant to see what 3rd
        # party code and license they're pulling in -- so the fallback must not silently drop it).
        include(FetchContent)

        set(fc_args)
        if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.24")
            list(APPEND fc_args DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
        endif()

        # Probe the URL by downloading it into the build tree ourselves (re-using it by hash if already
        # present, same idea as o3de_fetch_content's download cache) so we know up front whether to hand
        # FetchContent_Declare a URL or fall back to GIT_REPOSITORY -- FetchContent_Declare itself has no
        # "try this, then that" behavior, so the decision has to be made before declaring.
        set(sdl3_probe_file "${CMAKE_BINARY_DIR}/downloads/SDL3/${sdl3_url_hash}")
        get_filename_component(sdl3_probe_dir "${sdl3_probe_file}" DIRECTORY)
        file(MAKE_DIRECTORY "${sdl3_probe_dir}")

        set(sdl3_url_ok FALSE)
        if(EXISTS "${sdl3_probe_file}")
            file(SHA256 "${sdl3_probe_file}" sdl3_existing_hash)
            if(sdl3_existing_hash STREQUAL sdl3_url_hash)
                set(sdl3_url_ok TRUE)
            else()
                file(REMOVE "${sdl3_probe_file}")
            endif()
        endif()

        if(NOT sdl3_url_ok)
            file(DOWNLOAD "${sdl3_url}" "${sdl3_probe_file}"
                EXPECTED_HASH "SHA256=${sdl3_url_hash}"
                STATUS sdl3_download_status
                TLS_VERIFY ON)
            list(GET sdl3_download_status 0 sdl3_download_code)
            if(sdl3_download_code EQUAL 0)
                set(sdl3_url_ok TRUE)
            else()
                list(GET sdl3_download_status 1 sdl3_download_error)
                message(WARNING
                    "DualSense gem: URL download of SDL3 failed (${sdl3_download_error}); falling back to "
                    "git clone of ${sdl3_git_repo}#${sdl3_git_tag}")
                file(REMOVE "${sdl3_probe_file}")
            endif()
        endif()

        if(sdl3_url_ok)
            list(APPEND fc_args
                URL "file://${sdl3_probe_file}"
                URL_HASH "SHA256=${sdl3_url_hash}")
        else()
            list(APPEND fc_args
                GIT_REPOSITORY "${sdl3_git_repo}"
                GIT_TAG "${sdl3_git_tag}")
        endif()

        message(STATUS
            "Using package SDL3 ${sdl3_version} (${sdl3_license} license), source: ${sdl3_url} "
            "[o3de_fetch_content unavailable on this engine flavor -- using stock CMake FetchContent]")

        FetchContent_Declare(SDL3 ${fc_args})
    endif()

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
    #
    # This is specifically a macOS/clang workaround (the dead DARWIN branch only matters to
    # CheckPTHREAD's Unix `-lpthread` probe, which is only reached on APPLE/Unix builds in the first
    # place). `-w` is a GCC/Clang flag; MSVC does not recognize it (its equivalent is `/w`), so gate this
    # to APPLE rather than applying it globally, which would either be a no-op or an actual bad-flag
    # error depending on the generator/toolchain on Windows.
    if(APPLE)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -w")
    endif()

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

    # BT-readiness addendum ("3b"): assert the fetched SDL3 actually compiled in the HIDAPI
    # joystick driver. Without HIDAPI, SDL3 has no PS5 driver at all: SDL_SendJoystickEffect
    # (this gem's adaptive-trigger path) and SDL_GetGamepadTypeForID's PS5 detection (this gem's
    # whole hotplug-monitor design, see DualSenseSdlMonitor::EnumeratePs5Joysticks) don't error out
    # without it -- they just silently return nothing/wrong-type, which would look like "no
    # DualSense connected" or "adaptive triggers do nothing" at runtime with zero diagnostic
    # signal pointing back at a build-configuration problem. Fail loudly here, at configure time,
    # instead of mysteriously at runtime.
    #
    # Locating the real file: SDL3's own CMakeLists.txt (searched the fetched
    # release-3.4.12 tree, around its `SDL_JOYSTICK_HIDAPI` cache-variable / build-config section)
    # writes this value in two steps, because config variables may contain generator expressions:
    #   1. `configure_file("${SDL3_SOURCE_DIR}/include/build_config/SDL_build_config.h.cmake"
    #      "${SDL3_BINARY_DIR}/CMakeFiles/SDL_build_config.h.intermediate")` -- runs eagerly, the
    #      moment SDL3's CMakeLists.txt is processed (i.e. already done by the
    #      FetchContent_MakeAvailable call directly above), with every value that has no generator
    #      expression already fully resolved. `SDL_JOYSTICK_HIDAPI` is a plain
    #      `#define SDL_JOYSTICK_HIDAPI 1`/absent line (verified: no `$<...>` in it), so this
    #      intermediate file already has the real answer.
    #   2. `file(GENERATE OUTPUT "${SDL3_BINARY_DIR}/include-config-$<LOWER_CASE:$<CONFIG>>/
    #      build_config/SDL_build_config.h" INPUT "...SDL_build_config.h.intermediate")` -- this is
    #      the header actually on the compiler's include path, but `file(GENERATE)` is deferred to
    #      CMake's generate phase, which runs after every CMakeLists.txt (including this one) has
    #      finished processing. It does not exist yet at this point in the configure, so it cannot
    #      be checked here (and with a multi-config generator -- this engine defaults to Ninja
    #      Multi-Config -- there is one per config: debug/profile/release, not one canonical path
    #      anyway).
    # So the step-1 intermediate is what this check reads. Confirmed against this repo's actual
    # build tree: `<build dir>/_deps/sdl3-build/CMakeFiles/SDL_build_config.h.intermediate` exists
    # right after this FetchContent_MakeAvailable call and contains `#define SDL_JOYSTICK_HIDAPI 1`
    # on a real hardware-fetched configure.
    FetchContent_GetProperties(SDL3 BINARY_DIR sdl3_binary_dir_for_hidapi_check)
    set(sdl3_build_config_intermediate "${sdl3_binary_dir_for_hidapi_check}/CMakeFiles/SDL_build_config.h.intermediate")

    # Portability hardening: the hardcoded path above was only ever confirmed against this repo's actual
    # macOS/Ninja Multi-Config build tree. Other generators/platforms may lay out SDL3's generated
    # build-config header differently (e.g. a single-config generator, or a future SDL3 CMakeLists.txt
    # restructuring). Rather than hard-fail configure over a path guess, search a couple of plausible
    # generated locations under the SDL3 binary dir before giving up. file(GLOB_RECURSE) with a literal
    # (non-wildcard) filename still recurses through every subdirectory of the base path and matches that
    # exact filename wherever it lands, which is exactly what's needed here.
    if(NOT EXISTS "${sdl3_build_config_intermediate}")
        foreach(candidate_name "SDL_build_config.h.intermediate" "SDL_build_config.h")
            file(GLOB_RECURSE sdl3_build_config_candidates "${sdl3_binary_dir_for_hidapi_check}/${candidate_name}")
            if(sdl3_build_config_candidates)
                list(GET sdl3_build_config_candidates 0 sdl3_build_config_intermediate)
                break()
            endif()
        endforeach()
    endif()

    if(NOT EXISTS "${sdl3_build_config_intermediate}")
        # Genuinely couldn't find the header anywhere under the SDL3 binary dir. This most likely means
        # SDL3's CMakeLists.txt internal layout changed, or this is an untested platform/generator
        # combination -- but a missing diagnostic header is not itself evidence that HIDAPI is actually
        # disabled. WARN instead of FATAL_ERROR so an otherwise-working build on an untested
        # platform/generator isn't blocked by this assertion; the tradeoff is that a real HIDAPI
        # misconfiguration on such a platform would only show up at runtime instead of at configure time.
        message(WARNING
            "DualSense gem: could not locate SDL3's generated build-config header under "
            "'${sdl3_binary_dir_for_hidapi_check}' (looked for SDL_build_config.h.intermediate and "
            "SDL_build_config.h). Skipping the HIDAPI build-time assertion -- SDL3's CMakeLists.txt "
            "internal layout may differ on this platform/generator from what was verified on macOS/Ninja "
            "Multi-Config. This does NOT mean HIDAPI is actually missing; if PS5 detection or adaptive "
            "triggers misbehave at runtime, check the SDL_HIDAPI/SDL_JOYSTICK options set above in this "
            "file manually.")
    else()
        file(STRINGS "${sdl3_build_config_intermediate}" sdl3_hidapi_define
            REGEX "^#define[ \t]+SDL_JOYSTICK_HIDAPI[ \t]+1[ \t]*$")
        if (NOT sdl3_hidapi_define)
            message(FATAL_ERROR
                "DualSense gem: the fetched SDL3 (release-3.4.12) was NOT built with the HIDAPI "
                "joystick driver (SDL_JOYSTICK_HIDAPI is not defined =1 in "
                "'${sdl3_build_config_intermediate}'). Without HIDAPI, SDL3 has no PS5 driver: "
                "SDL_SendJoystickEffect (adaptive triggers) and SDL_GetGamepadTypeForID's PS5 "
                "detection will silently degrade or misbehave at runtime instead of failing -- this "
                "gem depends on both. Check the SDL_HIDAPI/SDL_JOYSTICK options set above in this "
                "file, and this platform's HIDAPI prerequisites (see the fetched SDL3 tree's own "
                "docs/README-hidapi.md).")
        endif()
    endif()

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
