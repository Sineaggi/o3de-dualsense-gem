# Phase 3c: DualSenseSystemImpl_Windows.cpp activates the SDL3 backend, so this platform now
# needs to satisfy SDL3-static's Windows link requirements.
#
# UNVERIFIED ON WINDOWS -- researched by reading the fetched SDL3 3.4.12 source tree
# (build/mac_ninja/_deps/sdl3-src on this (macOS) machine; a Windows configure would fetch the
# same pinned tag) rather than by actually configuring/linking on Windows. Specifically:
# CMakeLists.txt's WINDOWS branch calls
#   sdl_link_dependency(base LIBS kernel32 user32 gdi32 winmm imm32 ole32 oleaut32 version uuid advapi32 setupapi shell32)
# and cmake/sdlcommands.cmake's sdl_generic_link_dependency adds those to the STATIC_TARGETS
# (SDL3-static, which is what 3rdParty::SDL3 aliases when SDL_STATIC=ON/SDL_SHARED=OFF -- see
# 3rdParty/FindSDL3.cmake) via `target_link_libraries(SDL3-static PRIVATE ${ARGS_LIBS})`.
#
# CORRECTED 2026-07-27 (review finding, empirically tested with a 4-level static->static->shared
# ->exe CMake chain): on the FROM-SOURCE path these entries are REDUNDANT. PUBLIC/PRIVATE on a
# static library governs usage requirements (include dirs, compile definitions), NOT whether that
# library's own link libraries reach a consuming binary's link line -- cmake-buildsystem(7):
# "CMake records static libraries' link dependencies for transitive use when linking consuming
# binaries". So SDL3-static's full Windows base set already reaches Editor.exe/DualSense.dll on
# its own. They are kept anyway because they ARE load-bearing on the OTHER consumption path:
# 3rdParty/Installer/FindSDL3.cmake declares SDL3-static as a bare IMPORTED target with no
# recorded link libraries (SDL3's own CMakeLists never runs in an installed-SDK build), and
# duplicate system-import-lib entries are harmless (the linker de-dupes).
#
# DEBUGGING NOTE for the first real Windows link: if it fails with unresolved externals from
# SDL3's HIDAPI/joystick objects on a from-source configure, widening this list is the WRONG first
# move (propagation already happens). Look instead at whether SDL3-static's target shape differs
# from what FindSDL3.cmake assumes, whether an Installer/SDK path is in play, or at the linker
# invocation itself.
#
# kernel32/user32/gdi32/ole32/oleaut32/uuid/advapi32/shell32 are left OFF this list: O3DE's own
# Windows Editor/Launcher targets already link Qt and AzFramework, both of which are extremely
# likely to already pull these common Win32/COM libraries in transitively -- but this has not
# been confirmed against an actual Windows link (see the Phase 3c report). winmm (multimedia
# timer, used by SDL3's Windows joystick/haptic timing) and imm32/version/setupapi (IME, resource
# versioning, and PnP device-class enumeration -- all specific to SDL3's Windows HID/joystick
# backend) are far less likely to already be linked by anything else in this gem's dependency
# chain, so they are added explicitly here as the genuinely-at-risk set. cfgmgr32 -- also visible
# in SDL3's fetched src/hidapi/windows/ tree -- is deliberately NOT added: hid.c LoadLibraryW's
# cfgmgr32.dll and hid.dll at runtime (see the RESOLVE(...) macro calls in that file) rather than
# import-linking them, so no .lib is needed for either.
#
set(LY_BUILD_DEPENDENCIES
    PRIVATE
        winmm
        imm32
        version
        setupapi
)
