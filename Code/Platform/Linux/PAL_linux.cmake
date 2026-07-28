
set(PAL_TRAIT_DUALSENSE_SUPPORTED TRUE)
set(PAL_TRAIT_DUALSENSE_TEST_SUPPORTED FALSE)
set(PAL_TRAIT_DUALSENSE_EDITOR_TEST_SUPPORTED FALSE)

# Phase 3a: SDL3 (joystick-only) fetch + link is available on this platform. This only controls
# whether 3rdParty::SDL3 gets linked and DUALSENSE_SDL_BACKEND_ENABLED gets defined -- it does NOT
# select the backend at runtime. That is the dualsense_backend cvar's job (default "native"; see
# DualSenseSystemComponent.cpp), which Task 3 will wire up to actually consume this.
set(PAL_TRAIT_DUALSENSE_SDL_BACKEND TRUE)
