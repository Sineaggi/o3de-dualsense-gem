
o3de_gem_setup("DualSense")

# Adds the 3rdParty folder into the CMAKE_MODULE_PATH. This causes any call to find_package(XXXX)
# to look in this folder for `FindXXXX.cmake` before it looks elsewhere, and run that file if it can
# find it. (SDL3, fetched behind PAL_TRAIT_DUALSENSE_SDL_BACKEND -- see Code/CMakeLists.txt.)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/3rdParty)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH})

add_subdirectory(Code)
