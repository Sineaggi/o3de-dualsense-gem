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
# 3rdParty/FindSDL3.cmake) via `target_link_libraries(SDL3-static PRIVATE ${ARGS_LIBS})`. Because
# that is PRIVATE (not PUBLIC/INTERFACE) on a STATIC library target, CMake's usual
# CMP0022-NEW behavior does NOT forward it as a transitive link requirement to anything that
# merely links 3rdParty::SDL3 (this gem's DualSense.Private.Object does exactly that, PRIVATE, in
# Code/CMakeLists.txt) -- a consumer must supply those system libraries itself if the final
# link (Editor.exe, a Launcher, DualSense.Tests.exe) doesn't already pull them in some other way.
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
# If the first real Windows build comes back with unresolved externals from SDL3's Windows
# joystick/HIDAPI backend, start by widening this list back toward SDL3's full base-dependency
# set above (winmm, imm32, ole32, oleaut32, version, uuid, advapi32, setupapi, shell32, gdi32,
# user32, kernel32) before looking anywhere else.
set(LY_BUILD_DEPENDENCIES
    PRIVATE
        winmm
        imm32
        version
        setupapi
)
