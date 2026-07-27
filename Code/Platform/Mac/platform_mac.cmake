find_library(GAME_CONTROLLER_FRAMEWORK GameController)
find_library(CORE_HAPTICS_FRAMEWORK CoreHaptics)
find_library(FOUNDATION_FRAMEWORK Foundation)
# BT-readiness addendum ("3b"): DualSenseSdlRuntime.cpp calls IOHIDCheckAccess
# (IOKit/hid/IOHIDLib.h) under __APPLE__ to detect macOS's Input Monitoring (TCC) gate on SDL's
# HID access path.
find_library(IOKIT_FRAMEWORK IOKit)

set(LY_BUILD_DEPENDENCIES
    PRIVATE
        ${GAME_CONTROLLER_FRAMEWORK}
        ${CORE_HAPTICS_FRAMEWORK}
        ${FOUNDATION_FRAMEWORK}
        ${IOKIT_FRAMEWORK}
)
