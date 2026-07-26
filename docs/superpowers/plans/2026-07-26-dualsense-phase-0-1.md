# DualSense Gem — Phases 0–1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Prove the gamepad implementation-swap architecture (Phase 0), then ship a working Mac backend: DualSense input on all 32 standard channels, rumble via CoreHaptics, and lightbar (Phase 1).

**Architecture:** The gem swaps `AzFramework::InputDeviceGamepad`'s pimpl per-slot at runtime via `InputDeviceImplementationRequest<InputDeviceGamepad>::SetCustomImplementation` (BarrierInput pattern), so a DualSense masquerades as the standard `gamepad` device. Phase 0 proves swap/restore with a debug implementation and unit tests; Phase 1 adds a Mac device monitor (GameController.framework) that performs the swap automatically for real hardware. Spec: `docs/superpowers/specs/2026-07-26-dualsense-gem-design.md`.

**Tech Stack:** C++17 (O3DE dialect: AZStd, EBus, AZ::Interface), Objective-C++ (`.mm`) for Mac, GameController.framework + CoreHaptics, googletest via AzTest/AzTestRunner, CMake (ly_add_target / PAL).

## Global Constraints

- **Zero engine modifications.** All work happens in `~/Source/o3de-dualsense-gem`. The engine at `~/Source/o3de` is only configured/built, never edited.
- **Engine build (Mac, from `~/Source/o3de`):** configure `cmake --preset mac-ninja -DLY_DISABLE_TEST_MODULES=FALSE`, build `cmake --build build/mac_ninja --config profile --target <Target> -j 10`. Known gotcha: with Ninja Multi-Config, `fixup_bundle` can race app linking ("not a valid bundle" with empty Contents/MacOS) — **re-running the same build converges**; always retry once before diagnosing.
- **Run unit tests:** `./build/mac_ninja/bin/profile/AzTestRunner $PWD/build/mac_ninja/bin/profile/libDualSense.Tests.dylib AzRunUnitTests` (append `--gtest_filter=<Pattern>` to narrow).
- **macOS floor: 11.3** for DualSense features. Guard GameController/CoreHaptics calls with `if (@available(macOS 11.3, *))`; below the floor the gem must leave stock engine behavior untouched.
- **Never "clear" an implementation by passing nullptr** — the engine handler ignores null factories. Restore the platform default by re-sending `AZ::Interface<AzFramework::InputDeviceGamepad::ImplementationFactory>::Get()`.
- **Channel-update contract:** every `TickInputDevice()` must call `ProcessRawGamepadState()` exactly once so each channel updates once per frame.
- **Client-only gem:** no `.Servers` aliases (removed in Task 1).
- **Commits** are made in `~/Source/o3de-dualsense-gem`, message style `<type>: <summary>` body optional, ending with:
  `Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>`
- New source files match the scaffold style: no copyright headers, 4-space indent, `DualSense` namespace.

---

### Task 1: Build integration — gem compiles in the engine build, tests enabled

**Files:**
- Modify: `Code/Platform/Mac/PAL_mac.cmake`
- Modify: `Code/Platform/Android/PAL_android.cmake`, `Code/Platform/iOS/PAL_ios.cmake`
- Modify: `Code/CMakeLists.txt` (lines 90–97: alias block)

**Interfaces:**
- Consumes: the scaffold committed at repo HEAD.
- Produces: a building `Gem::DualSense.Tests` target; every later task assumes `AzTestRunner` runs against `libDualSense.Tests.dylib`.

- [ ] **Step 1: Enable Mac unit tests, disable non-desktop platforms**

In `Code/Platform/Mac/PAL_mac.cmake` change the test trait:

```cmake
set(PAL_TRAIT_DUALSENSE_SUPPORTED TRUE)
set(PAL_TRAIT_DUALSENSE_TEST_SUPPORTED TRUE)
set(PAL_TRAIT_DUALSENSE_EDITOR_TEST_SUPPORTED FALSE)
```

In `Code/Platform/Android/PAL_android.cmake` and `Code/Platform/iOS/PAL_ios.cmake` set the first trait to `FALSE` (leave the other two FALSE):

```cmake
set(PAL_TRAIT_DUALSENSE_SUPPORTED FALSE)
```

- [ ] **Step 2: Remove the Servers aliases (client-only gem, per spec)**

In `Code/CMakeLists.txt` delete exactly these two lines:

```cmake
ly_create_alias(NAME ${gem_name}.Servers NAMESPACE Gem TARGETS Gem::${gem_name})
```
```cmake
ly_create_alias(NAME ${gem_name}.Servers.API NAMESPACE Gem TARGETS Gem::${gem_name}.API)
```

Leave `o3de_add_variant_dependencies_for_gem_dependencies(... VARIANTS Clients Servers Unified)` as-is (no-op with an empty dependency list).

- [ ] **Step 3: Configure the engine with tests enabled**

```bash
cd ~/Source/o3de
cmake --preset mac-ninja -DLY_DISABLE_TEST_MODULES=FALSE
```

Expected: configure succeeds (it will take longer than usual — test targets engine-wide are being generated; that's fine, we only *build* ours).

- [ ] **Step 4: Verify the gem's targets exist; add LY_EXTERNAL_SUBDIRS only if they don't**

```bash
cmake --build build/mac_ninja --config profile --target DualSense.Tests -j 10
```

If the target is unknown (manifest `external_subdirectories` not picked up by the engine build), reconfigure with the gem passed explicitly, then rebuild:

```bash
cmake --preset mac-ninja -DLY_DISABLE_TEST_MODULES=FALSE -DLY_EXTERNAL_SUBDIRS="$HOME/Source/o3de-dualsense-gem"
cmake --build build/mac_ninja --config profile --target DualSense.Tests -j 10
```

Expected: `libDualSense.Tests.dylib` links (the fixup_bundle retry rule applies).

- [ ] **Step 5: Run the (empty) test module**

```bash
./build/mac_ninja/bin/profile/AzTestRunner $PWD/build/mac_ninja/bin/profile/libDualSense.Tests.dylib AzRunUnitTests
```

Expected: runs 0 tests, exit code 0 (the scaffold's `DualSenseTest.cpp` is only `AZ_UNIT_TEST_HOOK`).

- [ ] **Step 6: Commit**

```bash
cd ~/Source/o3de-dualsense-gem
git add Code/Platform Code/CMakeLists.txt
git commit -m "build: enable Mac unit tests, drop Servers variant, disable mobile platforms"
```

---

### Task 2: Debug gamepad implementation + factory (the swap payload)

**Files:**
- Create: `Code/Source/Clients/DualSenseGamepadButtonMap.h`
- Create: `Code/Source/Clients/DualSenseDebugGamepadImpl.h`
- Create: `Code/Source/Clients/DualSenseDebugGamepadImpl.cpp`
- Modify: `Code/dualsense_private_files.cmake`
- Create: `Code/Tests/Clients/DualSenseDebugImplTests.cpp`
- Modify: `Code/dualsense_tests_files.cmake`

**Interfaces:**
- Consumes: `AzFramework::InputDeviceGamepad::Implementation` (engine header `AzFramework/Input/Devices/Gamepad/InputDeviceGamepad.h`): protected ctor `Implementation(InputDeviceGamepad&)`, pure virtuals `IsConnected()`, `SetVibration(float, float)`, `TickInputDevice()`, protected `ProcessRawGamepadState(const RawGamepadState&)`, public `using DigitalButtonIdByBitMaskMap = AZStd::unordered_map<AZ::u32, const InputChannelId*>`.
- Produces (used by Tasks 3, 4, and the Mac impl in Task 8):
  - `DualSense::ButtonBits` — `AZ::u32` constants `DPadUp, DPadDown, DPadLeft, DPadRight, Start, Select, L3, R3, L1, R1, A, B, X, Y`.
  - `const AzFramework::InputDeviceGamepad::Implementation::DigitalButtonIdByBitMaskMap& DualSense::GetDualSenseDigitalButtonMap();`
  - `class DualSense::DualSenseDebugGamepadImpl` with public `float m_lastVibrationLeft`, `m_lastVibrationRight`.
  - `struct DualSense::DualSenseDebugGamepadImplFactory : AzFramework::InputDeviceGamepad::ImplementationFactory` with public `DualSenseDebugGamepadImpl* m_lastCreated` (non-owning, test observation only) and `AZ::u32 GetMaxSupportedGamepads() const override` returning 4.

- [ ] **Step 1: Write the failing tests**

`Code/Tests/Clients/DualSenseDebugImplTests.cpp`:

```cpp
#include <AzCore/UnitTest/TestTypes.h>
#include <AzFramework/Input/Devices/Gamepad/InputDeviceGamepad.h>
#include <AzFramework/Input/Buses/Requests/InputHapticFeedbackRequestBus.h>
#include <Clients/DualSenseDebugGamepadImpl.h>
#include <Clients/DualSenseGamepadButtonMap.h>

namespace DualSenseTests
{
    using DualSenseDebugFixture = UnitTest::LeakDetectionFixture;

    TEST_F(DualSenseDebugFixture, ButtonMap_Has14UniqueSingleBitEntries)
    {
        const auto& map = DualSense::GetDualSenseDigitalButtonMap();
        EXPECT_EQ(map.size(), 14);
        AZ::u32 combined = 0;
        for (const auto& [bit, channelId] : map)
        {
            EXPECT_NE(channelId, nullptr);
            EXPECT_EQ(bit & (bit - 1), 0u) << "mask must be a single bit";
            EXPECT_EQ(combined & bit, 0u) << "bit used twice";
            combined |= bit;
        }
    }

    TEST_F(DualSenseDebugFixture, DebugFactory_CreatesImplementation_DeviceSupportedAndConnected)
    {
        DualSense::DualSenseDebugGamepadImplFactory factory;
        AzFramework::InputDeviceGamepad gamepad(AzFramework::InputDeviceGamepad::IdForIndex0, &factory);
        EXPECT_TRUE(gamepad.IsSupported());
        EXPECT_TRUE(gamepad.IsConnected());
        EXPECT_NE(factory.m_lastCreated, nullptr);
    }

    TEST_F(DualSenseDebugFixture, HapticBusSetVibration_ReachesDebugImplementation)
    {
        DualSense::DualSenseDebugGamepadImplFactory factory;
        AzFramework::InputDeviceGamepad gamepad(AzFramework::InputDeviceGamepad::IdForIndex0, &factory);
        AzFramework::InputHapticFeedbackRequestBus::Event(
            gamepad.GetInputDeviceId(),
            &AzFramework::InputHapticFeedbackRequests::SetVibration, 0.5f, 0.25f);
        ASSERT_NE(factory.m_lastCreated, nullptr);
        EXPECT_FLOAT_EQ(factory.m_lastCreated->m_lastVibrationLeft, 0.5f);
        EXPECT_FLOAT_EQ(factory.m_lastCreated->m_lastVibrationRight, 0.25f);
    }

    TEST_F(DualSenseDebugFixture, TickInputDevice_DoesNotCrashWithNoActivity)
    {
        DualSense::DualSenseDebugGamepadImplFactory factory;
        AzFramework::InputDeviceGamepad gamepad(AzFramework::InputDeviceGamepad::IdForIndex0, &factory);
        gamepad.TickInputDevice();
        gamepad.TickInputDevice();
    }
} // namespace DualSenseTests
```

Add to `Code/dualsense_tests_files.cmake`:

```cmake
set(FILES
    Tests/Clients/DualSenseTest.cpp
    Tests/Clients/DualSenseDebugImplTests.cpp
)
```

- [ ] **Step 2: Build to verify the tests fail to compile**

```bash
cd ~/Source/o3de && cmake --build build/mac_ninja --config profile --target DualSense.Tests -j 10
```

Expected: FAILS — `Clients/DualSenseDebugGamepadImpl.h` not found.

- [ ] **Step 3: Implement the button map header**

`Code/Source/Clients/DualSenseGamepadButtonMap.h`:

```cpp
#pragma once

#include <AzFramework/Input/Devices/Gamepad/InputDeviceGamepad.h>

namespace DualSense
{
    //! Bit assignments for the gem's platform-agnostic digital button state.
    //! Shared by the debug implementation (Phase 0), the Mac backend (Phase 1),
    //! and the raw-HID backends (Phase 3+).
    namespace ButtonBits
    {
        inline constexpr AZ::u32 DPadUp    = 1u << 0;
        inline constexpr AZ::u32 DPadDown  = 1u << 1;
        inline constexpr AZ::u32 DPadLeft  = 1u << 2;
        inline constexpr AZ::u32 DPadRight = 1u << 3;
        inline constexpr AZ::u32 Start     = 1u << 4;  // menu / options-cluster right
        inline constexpr AZ::u32 Select    = 1u << 5;  // create / options-cluster left
        inline constexpr AZ::u32 L3        = 1u << 6;
        inline constexpr AZ::u32 R3        = 1u << 7;
        inline constexpr AZ::u32 L1        = 1u << 8;
        inline constexpr AZ::u32 R1        = 1u << 9;
        inline constexpr AZ::u32 A         = 1u << 12; // cross
        inline constexpr AZ::u32 B         = 1u << 13; // circle
        inline constexpr AZ::u32 X         = 1u << 14; // square
        inline constexpr AZ::u32 Y         = 1u << 15; // triangle
    } // namespace ButtonBits

    //! Maps ButtonBits to the 14 standard gamepad digital button channels.
    const AzFramework::InputDeviceGamepad::Implementation::DigitalButtonIdByBitMaskMap&
        GetDualSenseDigitalButtonMap();
} // namespace DualSense
```

- [ ] **Step 4: Implement the debug implementation + factory**

`Code/Source/Clients/DualSenseDebugGamepadImpl.h`:

```cpp
#pragma once

#include <AzFramework/Input/Devices/Gamepad/InputDeviceGamepad.h>

namespace DualSense
{
    //! Phase-0 stand-in backend: proves the SetCustomImplementation swap works.
    //! Reports connected, records the last vibration request, ticks an all-zero
    //! raw state so every channel honors the once-per-frame update contract.
    class DualSenseDebugGamepadImpl
        : public AzFramework::InputDeviceGamepad::Implementation
    {
    public:
        explicit DualSenseDebugGamepadImpl(AzFramework::InputDeviceGamepad& inputDevice);

        bool IsConnected() const override;
        void SetVibration(float leftMotorSpeedNormalized, float rightMotorSpeedNormalized) override;
        void TickInputDevice() override;

        float m_lastVibrationLeft = -1.0f;
        float m_lastVibrationRight = -1.0f;

    private:
        RawGamepadState m_rawGamepadState;
    };

    struct DualSenseDebugGamepadImplFactory
        : public AzFramework::InputDeviceGamepad::ImplementationFactory
    {
        AZStd::unique_ptr<AzFramework::InputDeviceGamepad::Implementation> Create(
            AzFramework::InputDeviceGamepad& inputDevice) override;
        AZ::u32 GetMaxSupportedGamepads() const override;

        DualSenseDebugGamepadImpl* m_lastCreated = nullptr; // non-owning, observation only
    };
} // namespace DualSense
```

`Code/Source/Clients/DualSenseDebugGamepadImpl.cpp`:

```cpp
#include <Clients/DualSenseDebugGamepadImpl.h>
#include <Clients/DualSenseGamepadButtonMap.h>

#include <AzCore/Console/ILogger.h>

namespace DualSense
{
    const AzFramework::InputDeviceGamepad::Implementation::DigitalButtonIdByBitMaskMap&
        GetDualSenseDigitalButtonMap()
    {
        using Button = AzFramework::InputDeviceGamepad::Button;
        static const AzFramework::InputDeviceGamepad::Implementation::DigitalButtonIdByBitMaskMap map = {
            { ButtonBits::DPadUp,    &Button::DU },
            { ButtonBits::DPadDown,  &Button::DD },
            { ButtonBits::DPadLeft,  &Button::DL },
            { ButtonBits::DPadRight, &Button::DR },
            { ButtonBits::Start,     &Button::Start },
            { ButtonBits::Select,    &Button::Select },
            { ButtonBits::L3,        &Button::L3 },
            { ButtonBits::R3,        &Button::R3 },
            { ButtonBits::L1,        &Button::L1 },
            { ButtonBits::R1,        &Button::R1 },
            { ButtonBits::A,         &Button::A },
            { ButtonBits::B,         &Button::B },
            { ButtonBits::X,         &Button::X },
            { ButtonBits::Y,         &Button::Y },
        };
        return map;
    }

    DualSenseDebugGamepadImpl::DualSenseDebugGamepadImpl(AzFramework::InputDeviceGamepad& inputDevice)
        : AzFramework::InputDeviceGamepad::Implementation(inputDevice)
        , m_rawGamepadState(GetDualSenseDigitalButtonMap())
    {
        m_rawGamepadState.m_triggerMaximumValue = 1.0f;
        m_rawGamepadState.m_thumbStickMaximumValue = 1.0f;
        AZLOG_INFO("DualSense: debug gamepad implementation installed (device index %u)",
                   GetInputDeviceIndex());
        BroadcastInputDeviceConnectedEvent();
    }

    bool DualSenseDebugGamepadImpl::IsConnected() const
    {
        return true;
    }

    void DualSenseDebugGamepadImpl::SetVibration(float leftMotorSpeedNormalized, float rightMotorSpeedNormalized)
    {
        m_lastVibrationLeft = leftMotorSpeedNormalized;
        m_lastVibrationRight = rightMotorSpeedNormalized;
        AZLOG_INFO("DualSense: debug SetVibration(%.2f, %.2f)", leftMotorSpeedNormalized, rightMotorSpeedNormalized);
    }

    void DualSenseDebugGamepadImpl::TickInputDevice()
    {
        ProcessRawGamepadState(m_rawGamepadState);
    }

    AZStd::unique_ptr<AzFramework::InputDeviceGamepad::Implementation> DualSenseDebugGamepadImplFactory::Create(
        AzFramework::InputDeviceGamepad& inputDevice)
    {
        auto impl = AZStd::make_unique<DualSenseDebugGamepadImpl>(inputDevice);
        m_lastCreated = impl.get();
        return impl;
    }

    AZ::u32 DualSenseDebugGamepadImplFactory::GetMaxSupportedGamepads() const
    {
        return 4;
    }
} // namespace DualSense
```

Note: the `Button::DU/DD/DL/DR` short names must match the engine header — open
`~/Source/o3de/Code/Framework/AzFramework/AzFramework/Input/Devices/Gamepad/InputDeviceGamepad.h`,
find `struct Button`, and use the exact member names it declares for d-pad up/down/left/right
(they are the members whose channel names are `gamepad_button_d_up` etc.). Fix the four map
entries if the members are named differently (e.g. `DPadUp`).

Add both source files to `Code/dualsense_private_files.cmake`:

```cmake
set(FILES
    Source/DualSenseModuleInterface.cpp
    Source/DualSenseModuleInterface.h
    Source/Clients/DualSenseSystemComponent.cpp
    Source/Clients/DualSenseSystemComponent.h
    Source/Clients/DualSenseGamepadButtonMap.h
    Source/Clients/DualSenseDebugGamepadImpl.cpp
    Source/Clients/DualSenseDebugGamepadImpl.h
)
```

- [ ] **Step 5: Build and run tests to verify they pass**

```bash
cd ~/Source/o3de
cmake --build build/mac_ninja --config profile --target DualSense.Tests -j 10
./build/mac_ninja/bin/profile/AzTestRunner $PWD/build/mac_ninja/bin/profile/libDualSense.Tests.dylib AzRunUnitTests
```

Expected: 4 tests PASS, no leaks reported.

- [ ] **Step 6: Commit**

```bash
cd ~/Source/o3de-dualsense-gem
git add Code
git commit -m "feat: debug gamepad implementation, factory, and shared button map"
```

---

### Task 3: Swap/restore semantics — pinned by tests

**Files:**
- Create: `Code/Tests/Clients/DualSenseSwapTests.cpp`
- Modify: `Code/dualsense_tests_files.cmake`

**Interfaces:**
- Consumes: `DualSenseDebugGamepadImplFactory` (Task 2); engine bus `AzFramework::InputDeviceImplementationRequest<InputDeviceGamepad>` (`AzFramework/Input/Buses/Requests/InputDeviceRequestBus.h`).
- Produces: executable documentation of swap behavior that Task 7's monitor relies on (swap works addressed by device id; null factory is a no-op; restore = re-send another factory).

- [ ] **Step 1: Write the failing tests**

`Code/Tests/Clients/DualSenseSwapTests.cpp`:

```cpp
#include <AzCore/UnitTest/TestTypes.h>
#include <AzFramework/Input/Devices/Gamepad/InputDeviceGamepad.h>
#include <AzFramework/Input/Buses/Requests/InputDeviceRequestBus.h>
#include <Clients/DualSenseDebugGamepadImpl.h>

namespace DualSenseTests
{
    using SwapBus = AzFramework::InputDeviceImplementationRequest<AzFramework::InputDeviceGamepad>;
    using DualSenseSwapFixture = UnitTest::LeakDetectionFixture;

    TEST_F(DualSenseSwapFixture, SetCustomImplementation_OnDeviceWithNoImpl_InstallsOurs)
    {
        AzFramework::InputDeviceGamepad gamepad(AzFramework::InputDeviceGamepad::IdForIndex0, nullptr);
        EXPECT_FALSE(gamepad.IsSupported());

        DualSense::DualSenseDebugGamepadImplFactory factory;
        SwapBus::Bus::Event(gamepad.GetInputDeviceId(), &SwapBus::SetCustomImplementation, &factory);

        EXPECT_TRUE(gamepad.IsSupported());
        EXPECT_EQ(factory.m_lastCreated != nullptr, true);
    }

    TEST_F(DualSenseSwapFixture, SetCustomImplementation_SecondFactory_ReplacesFirst)
    {
        DualSense::DualSenseDebugGamepadImplFactory factoryA;
        AzFramework::InputDeviceGamepad gamepad(AzFramework::InputDeviceGamepad::IdForIndex0, &factoryA);

        DualSense::DualSenseDebugGamepadImplFactory factoryB;
        SwapBus::Bus::Event(gamepad.GetInputDeviceId(), &SwapBus::SetCustomImplementation, &factoryB);
        ASSERT_NE(factoryB.m_lastCreated, nullptr);

        // Vibration must now land on B's implementation, not A's.
        AzFramework::InputHapticFeedbackRequestBus::Event(
            gamepad.GetInputDeviceId(),
            &AzFramework::InputHapticFeedbackRequests::SetVibration, 1.0f, 1.0f);
        EXPECT_FLOAT_EQ(factoryB.m_lastCreated->m_lastVibrationLeft, 1.0f);
        EXPECT_FLOAT_EQ(factoryA.m_lastCreated->m_lastVibrationLeft, -1.0f);
    }

    TEST_F(DualSenseSwapFixture, SetCustomImplementation_NullFactory_IsIgnoredByEngine)
    {
        // Pins the engine quirk the spec documents: null does NOT clear the impl.
        // If this test ever fails after an engine upgrade, the restore path in
        // DualSenseSystemComponent must be re-reviewed.
        DualSense::DualSenseDebugGamepadImplFactory factory;
        AzFramework::InputDeviceGamepad gamepad(AzFramework::InputDeviceGamepad::IdForIndex0, &factory);

        SwapBus::Bus::Event(gamepad.GetInputDeviceId(), &SwapBus::SetCustomImplementation, nullptr);
        EXPECT_TRUE(gamepad.IsSupported());
    }

    TEST_F(DualSenseSwapFixture, SwapAddressedToIndex1_DoesNotTouchIndex0)
    {
        DualSense::DualSenseDebugGamepadImplFactory factory0;
        AzFramework::InputDeviceGamepad gamepad0(AzFramework::InputDeviceGamepad::IdForIndex0, &factory0);
        AzFramework::InputDeviceGamepad gamepad1(AzFramework::InputDeviceGamepad::IdForIndexN(1), nullptr);

        DualSense::DualSenseDebugGamepadImplFactory factory1;
        SwapBus::Bus::Event(gamepad1.GetInputDeviceId(), &SwapBus::SetCustomImplementation, &factory1);

        EXPECT_TRUE(gamepad1.IsSupported());
        EXPECT_EQ(factory0.m_lastCreated->m_lastVibrationLeft, -1.0f);
        EXPECT_NE(factory1.m_lastCreated, nullptr);
    }
} // namespace DualSenseTests
```

Also add `#include <AzFramework/Input/Buses/Requests/InputHapticFeedbackRequestBus.h>` at the top (used by the second test).

Add `Tests/Clients/DualSenseSwapTests.cpp` to `Code/dualsense_tests_files.cmake`.

- [ ] **Step 2: Build & run — expect these specific results**

```bash
cd ~/Source/o3de
cmake --build build/mac_ninja --config profile --target DualSense.Tests -j 10
./build/mac_ninja/bin/profile/AzTestRunner $PWD/build/mac_ninja/bin/profile/libDualSense.Tests.dylib AzRunUnitTests --gtest_filter='*Swap*'
```

Expected: all 4 PASS on first run (these tests pin existing engine behavior rather than drive new gem code — the "failing test" phase here is the compile failure before the include paths are right). If `NullFactory_IsIgnoredByEngine` fails, STOP: the engine's swap semantics changed; update the spec's quirk note and the restore logic design before continuing.

- [ ] **Step 3: Commit**

```bash
cd ~/Source/o3de-dualsense-gem
git add Code
git commit -m "test: pin engine implementation-swap semantics (install, replace, null no-op, per-slot addressing)"
```

---

### Task 4: System component — services, swap bookkeeping, debug console commands

**Files:**
- Modify: `Code/Source/Clients/DualSenseSystemComponent.h`
- Modify: `Code/Source/Clients/DualSenseSystemComponent.cpp`
- Create: `Code/Tests/Clients/DualSenseSystemComponentTests.cpp`
- Modify: `Code/dualsense_tests_files.cmake`

**Interfaces:**
- Consumes: `DualSenseDebugGamepadImplFactory` (Task 2), swap bus (Task 3).
- Produces (used by Tasks 5, 7):
  - `void DualSenseSystemComponent::SwapSlotToFactory(AZ::u32 slotIndex, AzFramework::InputDeviceGamepad::ImplementationFactory* factory);`
  - `void DualSenseSystemComponent::RestoreSlotToPlatformDefault(AZ::u32 slotIndex);`
  - Console commands `dualsense_debug_swap [slot]` and `dualsense_debug_restore [slot]` (default slot 0).

- [ ] **Step 1: Write the failing test**

`Code/Tests/Clients/DualSenseSystemComponentTests.cpp`:

```cpp
#include <AzCore/UnitTest/TestTypes.h>
#include <AzFramework/Input/Devices/Gamepad/InputDeviceGamepad.h>
#include <Clients/DualSenseSystemComponent.h>
#include <Clients/DualSenseDebugGamepadImpl.h>

namespace DualSenseTests
{
    using DualSenseComponentFixture = UnitTest::LeakDetectionFixture;

    TEST_F(DualSenseComponentFixture, SwapSlotToFactory_InstallsOnMatchingSlotOnly)
    {
        AzFramework::InputDeviceGamepad gamepad0(AzFramework::InputDeviceGamepad::IdForIndex0, nullptr);
        AzFramework::InputDeviceGamepad gamepad1(AzFramework::InputDeviceGamepad::IdForIndexN(1), nullptr);

        DualSense::DualSenseDebugGamepadImplFactory factory;
        DualSense::DualSenseSystemComponent::SwapSlotToFactory(1, &factory);

        EXPECT_FALSE(gamepad0.IsSupported());
        EXPECT_TRUE(gamepad1.IsSupported());
    }

    TEST_F(DualSenseComponentFixture, RestoreSlotToPlatformDefault_WithNoPlatformFactory_LeavesImplInPlace)
    {
        // In unit tests no NativeUISystemComponent has registered a platform factory,
        // so AZ::Interface<ImplementationFactory>::Get() is null and restore must be
        // a safe no-op (engine ignores null factories - pinned in DualSenseSwapTests).
        DualSense::DualSenseDebugGamepadImplFactory factory;
        AzFramework::InputDeviceGamepad gamepad0(AzFramework::InputDeviceGamepad::IdForIndex0, &factory);

        DualSense::DualSenseSystemComponent::RestoreSlotToPlatformDefault(0);
        EXPECT_TRUE(gamepad0.IsSupported());
    }
} // namespace DualSenseTests
```

Add `Tests/Clients/DualSenseSystemComponentTests.cpp` to `Code/dualsense_tests_files.cmake`.

- [ ] **Step 2: Build to verify it fails**

```bash
cd ~/Source/o3de && cmake --build build/mac_ninja --config profile --target DualSense.Tests -j 10
```

Expected: FAILS — `SwapSlotToFactory` is not a member of `DualSenseSystemComponent`.

- [ ] **Step 3: Implement**

In `Code/Source/Clients/DualSenseSystemComponent.h`, inside the class's `public:` section (after the destructor), add:

```cpp
        //! Swap the standard gamepad slot's backend to the given factory
        //! (addressed per-slot; other slots untouched).
        static void SwapSlotToFactory(
            AZ::u32 slotIndex, AzFramework::InputDeviceGamepad::ImplementationFactory* factory);

        //! Restore the slot to the platform-default backend. NOTE: the engine
        //! ignores null factories, so if no platform factory is registered this
        //! is intentionally a no-op (never pass nullptr expecting a clear).
        static void RestoreSlotToPlatformDefault(AZ::u32 slotIndex);
```

and add these includes at the top of the header:

```cpp
#include <AzFramework/Input/Devices/Gamepad/InputDeviceGamepad.h>
```

In `Code/Source/Clients/DualSenseSystemComponent.cpp`:

Add includes:

```cpp
#include "DualSenseDebugGamepadImpl.h"

#include <AzCore/Console/IConsole.h>
#include <AzCore/Interface/Interface.h>
#include <AzFramework/Input/Buses/Requests/InputDeviceRequestBus.h>
```

Add the implementations and console commands (file scope, inside `namespace DualSense`):

```cpp
    void DualSenseSystemComponent::SwapSlotToFactory(
        AZ::u32 slotIndex, AzFramework::InputDeviceGamepad::ImplementationFactory* factory)
    {
        using SwapBus = AzFramework::InputDeviceImplementationRequest<AzFramework::InputDeviceGamepad>;
        SwapBus::Bus::Event(
            AzFramework::InputDeviceGamepad::IdForIndexN(slotIndex),
            &SwapBus::SetCustomImplementation,
            factory);
    }

    void DualSenseSystemComponent::RestoreSlotToPlatformDefault(AZ::u32 slotIndex)
    {
        auto* platformFactory =
            AZ::Interface<AzFramework::InputDeviceGamepad::ImplementationFactory>::Get();
        if (platformFactory == nullptr)
        {
            AZLOG_WARN("DualSense: no platform gamepad factory registered; restore for slot %u skipped", slotIndex);
            return;
        }
        SwapSlotToFactory(slotIndex, platformFactory);
    }

    namespace DebugCommands
    {
        static DualSenseDebugGamepadImplFactory s_debugFactory;

        static AZ::u32 SlotFromArgs(const AZ::ConsoleCommandContainer& arguments)
        {
            if (!arguments.empty())
            {
                return static_cast<AZ::u32>(strtoul(AZStd::string(arguments.front()).c_str(), nullptr, 10));
            }
            return 0;
        }

        static void dualsense_debug_swap(const AZ::ConsoleCommandContainer& arguments)
        {
            const AZ::u32 slot = SlotFromArgs(arguments);
            AZLOG_INFO("DualSense: swapping gamepad slot %u to debug implementation", slot);
            DualSenseSystemComponent::SwapSlotToFactory(slot, &s_debugFactory);
        }
        AZ_CONSOLEFREEFUNC(dualsense_debug_swap, AZ::ConsoleFunctorFlags::DontReplicate,
            "Swap a gamepad slot (arg, default 0) to the DualSense debug implementation");

        static void dualsense_debug_restore(const AZ::ConsoleCommandContainer& arguments)
        {
            const AZ::u32 slot = SlotFromArgs(arguments);
            AZLOG_INFO("DualSense: restoring gamepad slot %u to platform default", slot);
            DualSenseSystemComponent::RestoreSlotToPlatformDefault(slot);
        }
        AZ_CONSOLEFREEFUNC(dualsense_debug_restore, AZ::ConsoleFunctorFlags::DontReplicate,
            "Restore a gamepad slot (arg, default 0) to the platform-default implementation");
    } // namespace DebugCommands
```

Also add `#include <AzCore/Console/ILogger.h>` for `AZLOG_*`, and update the service functions:

```cpp
    void DualSenseSystemComponent::GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType& dependent)
    {
        // Soft dependencies: order after the input system when present, but do not
        // hard-require it (this component also activates in AssetProcessor/AssetBuilder
        // via the Builders variant, where no input system exists).
        dependent.push_back(AZ_CRC_CE("InputSystemService"));
        dependent.push_back(AZ_CRC_CE("NativeUIInputSystemService"));
    }
```

(Remove the `[[maybe_unused]]` attribute from that parameter.)

- [ ] **Step 4: Build and run all tests**

```bash
cd ~/Source/o3de
cmake --build build/mac_ninja --config profile --target DualSense.Tests -j 10
./build/mac_ninja/bin/profile/AzTestRunner $PWD/build/mac_ninja/bin/profile/libDualSense.Tests.dylib AzRunUnitTests
```

Expected: all tests PASS (Tasks 2+3+4 suites).

- [ ] **Step 5: Commit**

```bash
cd ~/Source/o3de-dualsense-gem
git add Code
git commit -m "feat: slot swap/restore helpers, soft input-system service deps, debug console commands"
```

---

### Task 5: Testbed project + Phase-0 manual smoke + README (Phase 0 exit)

**Files:**
- Create: `~/Source/dualsense-testbed/` (o3de project, its own directory — NOT part of the gem repo)
- Create: `README.md` (gem repo)
- Create: `docs/hardware-smoke.md` (gem repo)

**Interfaces:**
- Consumes: console commands from Task 4.
- Produces: a runnable Editor+launcher environment every later hardware task uses; `docs/hardware-smoke.md` is the running checklist Tasks 8–10 append to.

- [ ] **Step 1: Create and wire the testbed project**

```bash
cd ~/Source/o3de
scripts/o3de.sh create-project -pp ~/Source/dualsense-testbed -pn DualSenseTestbed
scripts/o3de.sh enable-gem -gn DualSense -pp ~/Source/dualsense-testbed
```

Expected: both commands print `Success!`. (`enable-gem` adds `DualSense` to the project's `project.json` gem list.)

- [ ] **Step 2: Configure + build the Editor for the project**

```bash
cmake --preset mac-ninja -DLY_DISABLE_TEST_MODULES=FALSE -DLY_PROJECTS="$HOME/Source/dualsense-testbed"
cmake --build build/mac_ninja --config profile --target Editor -j 10
```

(Keep `LY_EXTERNAL_SUBDIRS` on the command too if Task 1 needed it.) Expected: builds; retry once on the fixup_bundle race. First Editor launch will run the AssetProcessor over the new project — allow several minutes.

- [ ] **Step 3: Manual smoke — swap and restore in the Editor**

1. `./build/mac_ninja/bin/profile/Editor.app/Contents/MacOS/Editor --project-path ~/Source/dualsense-testbed`
2. Open/create the default level. In the Editor console (bottom panel text field), run `dualsense_debug_swap`.
3. Expected in the console log: `DualSense: swapping gamepad slot 0 to debug implementation` followed by `DualSense: debug gamepad implementation installed (device index 0)`.
4. Run `dualsense_debug_restore`. Expected: the restore log line, and — because the Mac platform factory exists in the Editor — no "skipped" warning.
5. If any real controller is paired, verify it still works after restore (any input-consuming behavior, or just confirm no errors/spam in the console).

Record pass/fail for each step in `docs/hardware-smoke.md` (created next step).

- [ ] **Step 4: Write README and the smoke checklist**

`README.md`:

```markdown
# DualSense Gem for O3DE

Sony DualSense (PS5) controller support for O3DE: adaptive trigger force-feedback
effects, haptic feedback, light bar, player LEDs, and (planned) extended inputs.

Status: Phase 0 (swap architecture proven). See `docs/superpowers/specs/` for the
design and `docs/superpowers/plans/` for the implementation plan.

## Setup

1. Register the gem: `scripts/o3de.sh register -gp <path-to-this-repo>` (from your engine root)
2. Enable it for a project: `scripts/o3de.sh enable-gem -gn DualSense -pp <project-path>`
3. Configure + build your project as usual.

## Build & test (Mac)

    cd ~/Source/o3de
    cmake --preset mac-ninja -DLY_DISABLE_TEST_MODULES=FALSE
    cmake --build build/mac_ninja --config profile --target DualSense.Tests -j 10
    ./build/mac_ninja/bin/profile/AzTestRunner $PWD/build/mac_ninja/bin/profile/libDualSense.Tests.dylib AzRunUnitTests

## Debug console commands

- `dualsense_debug_swap [slot]` — swap a gamepad slot to the debug implementation
- `dualsense_debug_restore [slot]` — restore the platform-default implementation
```

`docs/hardware-smoke.md`:

```markdown
# Hardware smoke checklist

Run after each phase lands. Record date + result per line.

## Phase 0 — swap proof (Editor, no hardware needed)
- [ ] `dualsense_debug_swap` logs install line, no errors
- [ ] `dualsense_debug_restore` restores; no "skipped" warning in Editor
- [ ] Regular paired controller (if any) still works after restore
```

- [ ] **Step 5: Commit (gem repo only — the testbed project is not committed here)**

```bash
cd ~/Source/o3de-dualsense-gem
git add README.md docs/hardware-smoke.md
git commit -m "docs: README, hardware smoke checklist; phase 0 complete"
git tag phase-0
```

---

### Task 6: Mac platform scaffolding — frameworks, PLATFORM_INCLUDE_FILES, system pimpl

**Files:**
- Create: `Code/Platform/Mac/platform_mac.cmake`
- Create: `Code/Platform/Windows/platform_windows.cmake`, `Code/Platform/Linux/platform_linux.cmake` (empty stubs)
- Modify: `Code/CMakeLists.txt` (Private.Object target)
- Create: `Code/Source/Clients/DualSenseSystemImpl.h`
- Create: `Code/Platform/Common/Unimplemented/DualSenseSystemImpl_Unimplemented.cpp`
- Modify: `Code/Platform/Windows/dualsense_private_files.cmake`, `Code/Platform/Linux/dualsense_private_files.cmake`
- Modify: `Code/Source/Clients/DualSenseSystemComponent.h` / `.cpp` (own the pimpl)

**Interfaces:**
- Consumes: nothing new.
- Produces (Task 7 implements the Mac side):
  - `class DualSense::DualSenseSystemImpl` with `static AZStd::unique_ptr<DualSenseSystemImpl> Create(DualSenseSystemComponent& owner);`, virtual dtor, and virtual `void Tick();` — exactly one `Create` definition is linked per platform (SaveData pattern).
  - CMake: `platform_<platform>.cmake` consumed via `PLATFORM_INCLUDE_FILES` on `Private.Object`; Mac links `GameController`, `CoreHaptics`, `Foundation`.

- [ ] **Step 1: Platform cmake files**

`Code/Platform/Mac/platform_mac.cmake`:

```cmake
find_library(GAME_CONTROLLER_FRAMEWORK GameController)
find_library(CORE_HAPTICS_FRAMEWORK CoreHaptics)
find_library(FOUNDATION_FRAMEWORK Foundation)

set(LY_BUILD_DEPENDENCIES
    PRIVATE
        ${GAME_CONTROLLER_FRAMEWORK}
        ${CORE_HAPTICS_FRAMEWORK}
        ${FOUNDATION_FRAMEWORK}
)
```

`Code/Platform/Windows/platform_windows.cmake` and `Code/Platform/Linux/platform_linux.cmake` (identical stub content):

```cmake
# No platform-specific build settings yet (raw-HID backend arrives in phase 3/4).
```

- [ ] **Step 2: Wire PLATFORM_INCLUDE_FILES into Private.Object**

In `Code/CMakeLists.txt`, in the `ly_add_target(NAME ${gem_name}.Private.Object ...)` block, add directly under `NAMESPACE Gem`:

```cmake
    PLATFORM_INCLUDE_FILES
        ${pal_dir}/platform_${PAL_PLATFORM_NAME_LOWERCASE}.cmake
```

- [ ] **Step 3: The system pimpl header + unimplemented stub**

`Code/Source/Clients/DualSenseSystemImpl.h`:

```cpp
#pragma once

#include <AzStd/memory/unique_ptr.h>

namespace DualSense
{
    class DualSenseSystemComponent;

    //! Per-platform system backend: watches for DualSense hardware and performs
    //! gamepad-slot swaps. Exactly one Create() definition links per platform.
    class DualSenseSystemImpl
    {
    public:
        static AZStd::unique_ptr<DualSenseSystemImpl> Create(DualSenseSystemComponent& owner);
        virtual ~DualSenseSystemImpl() = default;

        //! Called from DualSenseSystemComponent::OnTick on the main thread.
        virtual void Tick() {}

    protected:
        explicit DualSenseSystemImpl(DualSenseSystemComponent& owner) : m_owner(owner) {}
        DualSenseSystemComponent& m_owner;
    };
} // namespace DualSense
```

(If `#include <AzStd/memory/unique_ptr.h>` fails to resolve, the correct engine include is `#include <AzCore/std/smart_ptr/unique_ptr.h>` — use that.)

`Code/Platform/Common/Unimplemented/DualSenseSystemImpl_Unimplemented.cpp`:

```cpp
#include <Clients/DualSenseSystemImpl.h>

namespace DualSense
{
    AZStd::unique_ptr<DualSenseSystemImpl> DualSenseSystemImpl::Create(DualSenseSystemComponent&)
    {
        return nullptr; // Platform has no DualSense backend; gem stays passive.
    }
} // namespace DualSense
```

Add to BOTH `Code/Platform/Windows/dualsense_private_files.cmake` and `Code/Platform/Linux/dualsense_private_files.cmake`:

```cmake
set(FILES
    ../Common/Unimplemented/DualSenseSystemImpl_Unimplemented.cpp
)
```

Add `Source/Clients/DualSenseSystemImpl.h` to `Code/dualsense_private_files.cmake`. Do NOT add the Unimplemented cpp to the Mac list — Task 7 gives Mac its own definition; until Task 7 lands, temporarily add it to `Code/Platform/Mac/dualsense_private_files.cmake` too so this task links, and note it will be replaced.

- [ ] **Step 4: System component owns the pimpl**

`DualSenseSystemComponent.h`: add member + include:

```cpp
#include <Clients/DualSenseSystemImpl.h>
```
```cpp
    private:
        AZStd::unique_ptr<DualSenseSystemImpl> m_impl;
```

`DualSenseSystemComponent.cpp`: in `Activate()` after the existing BusConnects:

```cpp
        m_impl = DualSenseSystemImpl::Create(*this);
```

in `Deactivate()` before the BusDisconnects:

```cpp
        m_impl.reset();
```

in `OnTick(...)`:

```cpp
        if (m_impl)
        {
            m_impl->Tick();
        }
```

- [ ] **Step 5: Build everything (tests + module), run tests**

```bash
cd ~/Source/o3de
cmake --build build/mac_ninja --config profile --target DualSense.Tests DualSense -j 10
./build/mac_ninja/bin/profile/AzTestRunner $PWD/build/mac_ninja/bin/profile/libDualSense.Tests.dylib AzRunUnitTests
```

Expected: builds link (frameworks found), all tests still PASS.

- [ ] **Step 6: Commit**

```bash
cd ~/Source/o3de-dualsense-gem
git add Code
git commit -m "build: per-platform pimpl scaffolding; Mac links GameController/CoreHaptics"
```

---

### Task 7: Mac device monitor — detect DualSense, auto-swap, hotplug

**Files:**
- Create: `Code/Source/Clients/DualSenseSlotTracker.h`, `Code/Source/Clients/DualSenseSlotTracker.cpp`
- Create: `Code/Platform/Mac/DualSenseSystemImpl_Mac.mm`
- Create: `Code/Platform/Mac/DualSenseMacGamepadImplFactory.h` (declares the factory Task 8 fills in)
- Modify: `Code/Platform/Mac/dualsense_private_files.cmake` (remove the temporary Unimplemented entry, add the new files)
- Modify: `Code/dualsense_private_files.cmake` (slot tracker)
- Create: `Code/Tests/Clients/DualSenseSlotTrackerTests.cpp`; modify `Code/dualsense_tests_files.cmake`

**Interfaces:**
- Consumes: `SwapSlotToFactory` / `RestoreSlotToPlatformDefault` (Task 4), `DualSenseSystemImpl` (Task 6).
- Produces:
  - `class DualSense::DualSenseSlotTracker` (pure C++, reused by Windows later):
    `static constexpr AZ::u32 InvalidSlot = 0xFFFFFFFF;`
    `AZ::u32 Assign(const void* deviceKey, AZ::u32 preferredSlot);` — returns the slot chosen (preferred if free and < 4, else lowest free, else InvalidSlot; same key re-assigns to its existing slot).
    `AZ::u32 Release(const void* deviceKey);` — returns the freed slot or InvalidSlot.
    `AZ::u32 SlotOf(const void* deviceKey) const;`
  - Mac: `struct DualSense::DualSenseMacGamepadImplFactory : AzFramework::InputDeviceGamepad::ImplementationFactory` with `void* m_pendingController = nullptr;` (a `GCController*`, typed `void*` to keep ObjC out of the header). Task 8 implements its `Create`; in THIS task it is declared only, and the monitor compiles against it.

- [ ] **Step 1: Write failing slot-tracker tests**

`Code/Tests/Clients/DualSenseSlotTrackerTests.cpp`:

```cpp
#include <AzCore/UnitTest/TestTypes.h>
#include <Clients/DualSenseSlotTracker.h>

namespace DualSenseTests
{
    using TrackerFixture = UnitTest::LeakDetectionFixture;
    static const void* Key(intptr_t v) { return reinterpret_cast<const void*>(v); }

    TEST_F(TrackerFixture, Assign_PreferredSlotFree_UsesIt)
    {
        DualSense::DualSenseSlotTracker t;
        EXPECT_EQ(t.Assign(Key(1), 2), 2u);
        EXPECT_EQ(t.SlotOf(Key(1)), 2u);
    }

    TEST_F(TrackerFixture, Assign_PreferredTaken_UsesLowestFree)
    {
        DualSense::DualSenseSlotTracker t;
        t.Assign(Key(1), 0);
        EXPECT_EQ(t.Assign(Key(2), 0), 1u);
    }

    TEST_F(TrackerFixture, Assign_SameKeyTwice_ReturnsExistingSlot)
    {
        DualSense::DualSenseSlotTracker t;
        t.Assign(Key(1), 3);
        EXPECT_EQ(t.Assign(Key(1), 0), 3u);
    }

    TEST_F(TrackerFixture, Assign_AllFourTaken_ReturnsInvalid)
    {
        DualSense::DualSenseSlotTracker t;
        for (intptr_t i = 1; i <= 4; ++i) { t.Assign(Key(i), 0); }
        EXPECT_EQ(t.Assign(Key(5), 0), DualSense::DualSenseSlotTracker::InvalidSlot);
    }

    TEST_F(TrackerFixture, Release_FreesSlotForReuse)
    {
        DualSense::DualSenseSlotTracker t;
        t.Assign(Key(1), 0);
        EXPECT_EQ(t.Release(Key(1)), 0u);
        EXPECT_EQ(t.SlotOf(Key(1)), DualSense::DualSenseSlotTracker::InvalidSlot);
        EXPECT_EQ(t.Assign(Key(2), 0), 0u);
    }

    TEST_F(TrackerFixture, Release_UnknownKey_ReturnsInvalid)
    {
        DualSense::DualSenseSlotTracker t;
        EXPECT_EQ(t.Release(Key(9)), DualSense::DualSenseSlotTracker::InvalidSlot);
    }
} // namespace DualSenseTests
```

Add to `Code/dualsense_tests_files.cmake`. Build → expect compile failure (header missing).

- [ ] **Step 2: Implement the tracker**

`Code/Source/Clients/DualSenseSlotTracker.h`:

```cpp
#pragma once

#include <AzCore/base.h>
#include <AzCore/std/containers/array.h>

namespace DualSense
{
    //! Tracks which engine gamepad slot (0..3) each detected DualSense occupies.
    //! Pure C++ so it is unit-testable and reusable by non-Mac backends.
    class DualSenseSlotTracker
    {
    public:
        static constexpr AZ::u32 InvalidSlot = 0xFFFFFFFF;
        static constexpr AZ::u32 MaxSlots = 4;

        AZ::u32 Assign(const void* deviceKey, AZ::u32 preferredSlot);
        AZ::u32 Release(const void* deviceKey);
        AZ::u32 SlotOf(const void* deviceKey) const;

    private:
        AZStd::array<const void*, MaxSlots> m_slots{{ nullptr, nullptr, nullptr, nullptr }};
    };
} // namespace DualSense
```

`Code/Source/Clients/DualSenseSlotTracker.cpp`:

```cpp
#include <Clients/DualSenseSlotTracker.h>

namespace DualSense
{
    AZ::u32 DualSenseSlotTracker::Assign(const void* deviceKey, AZ::u32 preferredSlot)
    {
        if (const AZ::u32 existing = SlotOf(deviceKey); existing != InvalidSlot)
        {
            return existing;
        }
        if (preferredSlot < MaxSlots && m_slots[preferredSlot] == nullptr)
        {
            m_slots[preferredSlot] = deviceKey;
            return preferredSlot;
        }
        for (AZ::u32 i = 0; i < MaxSlots; ++i)
        {
            if (m_slots[i] == nullptr)
            {
                m_slots[i] = deviceKey;
                return i;
            }
        }
        return InvalidSlot;
    }

    AZ::u32 DualSenseSlotTracker::Release(const void* deviceKey)
    {
        for (AZ::u32 i = 0; i < MaxSlots; ++i)
        {
            if (m_slots[i] == deviceKey)
            {
                m_slots[i] = nullptr;
                return i;
            }
        }
        return InvalidSlot;
    }

    AZ::u32 DualSenseSlotTracker::SlotOf(const void* deviceKey) const
    {
        for (AZ::u32 i = 0; i < MaxSlots; ++i)
        {
            if (m_slots[i] == deviceKey)
            {
                return i;
            }
        }
        return InvalidSlot;
    }
} // namespace DualSense
```

Add both to `Code/dualsense_private_files.cmake`. Build + run tracker tests → PASS.

- [ ] **Step 3: The Mac factory declaration (implementation arrives in Task 8)**

`Code/Platform/Mac/DualSenseMacGamepadImplFactory.h`:

```cpp
#pragma once

#include <AzFramework/Input/Devices/Gamepad/InputDeviceGamepad.h>

namespace DualSense
{
    //! Creates the Mac (GameController.framework) gamepad implementation.
    //! m_pendingController must be set to the target GCController* immediately
    //! before the swap bus event fires (synchronous dispatch), and cleared after.
    struct DualSenseMacGamepadImplFactory
        : public AzFramework::InputDeviceGamepad::ImplementationFactory
    {
        AZStd::unique_ptr<AzFramework::InputDeviceGamepad::Implementation> Create(
            AzFramework::InputDeviceGamepad& inputDevice) override;
        AZ::u32 GetMaxSupportedGamepads() const override { return 4; }

        void* m_pendingController = nullptr; // GCController*
    };
} // namespace DualSense
```

- [ ] **Step 4: The Mac monitor**

`Code/Platform/Mac/DualSenseSystemImpl_Mac.mm`:

```objc
#include <Clients/DualSenseSystemImpl.h>
#include <Clients/DualSenseSystemComponent.h>
#include <Clients/DualSenseSlotTracker.h>
#include <DualSenseMacGamepadImplFactory.h>

#include <AzCore/Console/ILogger.h>

#import <GameController/GameController.h>

namespace DualSense
{
    class DualSenseSystemImplMac
        : public DualSenseSystemImpl
    {
    public:
        explicit DualSenseSystemImplMac(DualSenseSystemComponent& owner)
            : DualSenseSystemImpl(owner)
        {
            if (@available(macOS 11.3, *))
            {
                NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
                m_connectObserver = [center addObserverForName:GCControllerDidConnectNotification
                                                        object:nil
                                                         queue:[NSOperationQueue mainQueue]
                                                    usingBlock:^(NSNotification* note) {
                                                        this->OnControllerConnected((GCController*)note.object);
                                                    }];
                m_disconnectObserver = [center addObserverForName:GCControllerDidDisconnectNotification
                                                           object:nil
                                                            queue:[NSOperationQueue mainQueue]
                                                       usingBlock:^(NSNotification* note) {
                                                           this->OnControllerDisconnected((GCController*)note.object);
                                                       }];
                for (GCController* controller in GCController.controllers)
                {
                    OnControllerConnected(controller);
                }
            }
            else
            {
                AZLOG_INFO("DualSense: macOS < 11.3, DualSense support inactive (stock engine behavior)");
            }
        }

        ~DualSenseSystemImplMac() override
        {
            if (@available(macOS 11.3, *))
            {
                // Restore every slot we own before tearing down.
                for (GCController* controller in GCController.controllers)
                {
                    OnControllerDisconnected(controller);
                }
                NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
                if (m_connectObserver) { [center removeObserver:m_connectObserver]; }
                if (m_disconnectObserver) { [center removeObserver:m_disconnectObserver]; }
            }
        }

    private:
        void OnControllerConnected(GCController* controller) API_AVAILABLE(macos(11.3))
        {
            if (![controller.extendedGamepad isKindOfClass:[GCDualSenseGamepad class]])
            {
                return; // not a DualSense; leave it to the stock engine backend
            }
            const AZ::u32 preferred = (controller.playerIndex != GCControllerPlayerIndexUnset)
                ? static_cast<AZ::u32>(controller.playerIndex) : 0;
            const AZ::u32 slot = m_slotTracker.Assign((__bridge const void*)controller, preferred);
            if (slot == DualSenseSlotTracker::InvalidSlot)
            {
                AZLOG_WARN("DualSense: controller detected but all 4 gamepad slots occupied");
                return;
            }
            AZLOG_INFO("DualSense: controller detected, taking over gamepad slot %u", slot);
            m_factory.m_pendingController = (__bridge void*)controller;
            DualSenseSystemComponent::SwapSlotToFactory(slot, &m_factory);
            m_factory.m_pendingController = nullptr;
        }

        void OnControllerDisconnected(GCController* controller) API_AVAILABLE(macos(11.3))
        {
            const AZ::u32 slot = m_slotTracker.Release((__bridge const void*)controller);
            if (slot == DualSenseSlotTracker::InvalidSlot)
            {
                return; // wasn't ours
            }
            AZLOG_INFO("DualSense: controller left slot %u, restoring platform default", slot);
            DualSenseSystemComponent::RestoreSlotToPlatformDefault(slot);
        }

        DualSenseMacGamepadImplFactory m_factory;
        DualSenseSlotTracker m_slotTracker;
        id m_connectObserver = nil;
        id m_disconnectObserver = nil;
    };

    AZStd::unique_ptr<DualSenseSystemImpl> DualSenseSystemImpl::Create(DualSenseSystemComponent& owner)
    {
        return AZStd::make_unique<DualSenseSystemImplMac>(owner);
    }
} // namespace DualSense
```

Note: blocks capturing `this` from an ObjC++ class are valid; the observers are removed in the destructor before `this` dies. Notifications are delivered on the main queue, same thread as `OnTick`.

Update `Code/Platform/Mac/dualsense_private_files.cmake`:

```cmake
set(FILES
    DualSenseMacGamepadImplFactory.h
    DualSenseSystemImpl_Mac.mm
)
```

(This removes the temporary `../Common/Unimplemented/DualSenseSystemImpl_Unimplemented.cpp` entry from Task 6.)

Temporary link fix for THIS task only: `DualSenseMacGamepadImplFactory::Create` has no definition until Task 8. Add a placeholder definition at the bottom of `DualSenseSystemImpl_Mac.mm`, clearly marked, that Task 8 deletes:

```cpp
    // TEMPORARY until Task 8 (Mac input path) provides the real implementation:
    // fall back to the debug implementation so swap wiring can be exercised.
    AZStd::unique_ptr<AzFramework::InputDeviceGamepad::Implementation> DualSenseMacGamepadImplFactory::Create(
        AzFramework::InputDeviceGamepad& inputDevice)
    {
        return AZStd::make_unique<DualSenseDebugGamepadImpl>(inputDevice);
    }
```

with `#include <Clients/DualSenseDebugGamepadImpl.h>` added to the mm's includes.

- [ ] **Step 5: Build module + tests; run tests**

```bash
cd ~/Source/o3de
cmake --build build/mac_ninja --config profile --target DualSense DualSense.Tests -j 10
./build/mac_ninja/bin/profile/AzTestRunner $PWD/build/mac_ninja/bin/profile/libDualSense.Tests.dylib AzRunUnitTests
```

Expected: links clean (the Mac Tests target now pulls in the .mm via Private.Object — GameController/CoreHaptics frameworks come along via PLATFORM_INCLUDE_FILES), all tests PASS.

- [ ] **Step 6: Manual smoke (hardware): auto-swap on connect**

Launch the Editor with the testbed project, pair/plug a DualSense. Expected console log: `DualSense: controller detected, taking over gamepad slot 0` then the debug-impl install line. Unplug: `DualSense: controller left slot 0, restoring platform default`. A non-DualSense controller must produce neither line.

- [ ] **Step 7: Commit**

```bash
cd ~/Source/o3de-dualsense-gem
git add Code
git commit -m "feat(mac): DualSense detection, slot tracking, auto swap/restore on hotplug"
```

---

### Task 8: Mac input path — all 32 standard channels from GCDualSenseGamepad

**Files:**
- Create: `Code/Platform/Mac/InputDeviceGamepadDualSenseMac.h`, `Code/Platform/Mac/InputDeviceGamepadDualSenseMac.mm`
- Modify: `Code/Platform/Mac/DualSenseSystemImpl_Mac.mm` (delete the temporary factory definition)
- Modify: `Code/Platform/Mac/dualsense_private_files.cmake`
- Modify: `docs/hardware-smoke.md`

**Interfaces:**
- Consumes: `GetDualSenseDigitalButtonMap()` + `ButtonBits` (Task 2), `DualSenseMacGamepadImplFactory` (Task 7).
- Produces: `class DualSense::InputDeviceGamepadDualSenseMac : AzFramework::InputDeviceGamepad::Implementation` — ctor `(InputDeviceGamepad&, void* gcController)`. Tasks 9/10 add haptics/lightbar members to THIS class.

- [ ] **Step 1: Read the engine's Mac deadzone configuration**

```bash
grep -n "m_triggerMaximumValue\|m_triggerDeadZoneValue\|m_thumbStickMaximumValue\|DeadZone" \
  ~/Source/o3de/Code/Framework/AzFramework/Platform/Mac/AzFramework/Input/Devices/Gamepad/InputDeviceGamepad_Mac.mm
```

Use the exact values that file assigns to its `RawGamepadState` config fields in the implementation below (they are the engine's tuned Mac values; GameController reports normalized floats).

- [ ] **Step 2: Implement the real Mac gamepad implementation**

`Code/Platform/Mac/InputDeviceGamepadDualSenseMac.h`:

```cpp
#pragma once

#include <AzFramework/Input/Devices/Gamepad/InputDeviceGamepad.h>

namespace DualSense
{
    //! Standard-gamepad backend for a DualSense driven by GameController.framework.
    class InputDeviceGamepadDualSenseMac
        : public AzFramework::InputDeviceGamepad::Implementation
    {
    public:
        InputDeviceGamepadDualSenseMac(
            AzFramework::InputDeviceGamepad& inputDevice, void* gcController);
        ~InputDeviceGamepadDualSenseMac() override;

        bool IsConnected() const override;
        void SetVibration(float leftMotorSpeedNormalized, float rightMotorSpeedNormalized) override;
        void SetLightBarColor(const AZ::Color& color) override;
        void ResetLightBarColor() override;
        void TickInputDevice() override;

    private:
        RawGamepadState m_rawGamepadState;
        void* m_controller = nullptr; // GCController*, retained
        bool m_wasConnected = false;
    };
} // namespace DualSense
```

`Code/Platform/Mac/InputDeviceGamepadDualSenseMac.mm`:

```objc
#include <InputDeviceGamepadDualSenseMac.h>
#include <Clients/DualSenseGamepadButtonMap.h>
#include <DualSenseMacGamepadImplFactory.h>

#include <AzCore/Console/ILogger.h>

#import <GameController/GameController.h>

namespace DualSense
{
    InputDeviceGamepadDualSenseMac::InputDeviceGamepadDualSenseMac(
        AzFramework::InputDeviceGamepad& inputDevice, void* gcController)
        : AzFramework::InputDeviceGamepad::Implementation(inputDevice)
        , m_rawGamepadState(GetDualSenseDigitalButtonMap())
        , m_controller((__bridge_retained void*)(__bridge GCController*)gcController)
    {
        // Values from the engine's InputDeviceGamepad_Mac.mm (Step 1):
        m_rawGamepadState.m_triggerMaximumValue = /* engine value */ 1.0f;
        m_rawGamepadState.m_triggerDeadZoneValue = /* engine value */ 0.0f;
        m_rawGamepadState.m_thumbStickMaximumValue = /* engine value */ 1.0f;
        m_rawGamepadState.m_thumbStickLeftDeadZone = /* engine value */ 0.0f;
        m_rawGamepadState.m_thumbStickRightDeadZone = /* engine value */ 0.0f;
    }

    InputDeviceGamepadDualSenseMac::~InputDeviceGamepadDualSenseMac()
    {
        if (m_wasConnected)
        {
            BroadcastInputDeviceDisconnectedEvent();
        }
        if (m_controller)
        {
            CFRelease(m_controller); // balances __bridge_retained
        }
    }

    bool InputDeviceGamepadDualSenseMac::IsConnected() const
    {
        return m_controller != nullptr;
    }

    void InputDeviceGamepadDualSenseMac::SetVibration(float, float)
    {
        // Implemented in the haptics task (CoreHaptics handle engines).
    }

    void InputDeviceGamepadDualSenseMac::SetLightBarColor(const AZ::Color&)
    {
        // Implemented in the lightbar task (GCDeviceLight).
    }

    void InputDeviceGamepadDualSenseMac::ResetLightBarColor()
    {
        // Implemented in the lightbar task (GCDeviceLight).
    }

    void InputDeviceGamepadDualSenseMac::TickInputDevice()
    {
        GCController* controller = (__bridge GCController*)m_controller;
        if (@available(macOS 11.3, *))
        {
            GCDualSenseGamepad* pad = (GCDualSenseGamepad*)controller.extendedGamepad;
            if (pad)
            {
                if (!m_wasConnected)
                {
                    m_wasConnected = true;
                    BroadcastInputDeviceConnectedEvent();
                }

                AZ::u32 buttons = 0;
                if (pad.dpad.up.pressed)              { buttons |= ButtonBits::DPadUp; }
                if (pad.dpad.down.pressed)            { buttons |= ButtonBits::DPadDown; }
                if (pad.dpad.left.pressed)            { buttons |= ButtonBits::DPadLeft; }
                if (pad.dpad.right.pressed)           { buttons |= ButtonBits::DPadRight; }
                if (pad.buttonMenu.pressed)           { buttons |= ButtonBits::Start; }
                if (pad.buttonOptions.pressed)        { buttons |= ButtonBits::Select; }
                if (pad.leftThumbstickButton.pressed) { buttons |= ButtonBits::L3; }
                if (pad.rightThumbstickButton.pressed){ buttons |= ButtonBits::R3; }
                if (pad.leftShoulder.pressed)         { buttons |= ButtonBits::L1; }
                if (pad.rightShoulder.pressed)        { buttons |= ButtonBits::R1; }
                if (pad.buttonA.pressed)              { buttons |= ButtonBits::A; }  // cross
                if (pad.buttonB.pressed)              { buttons |= ButtonBits::B; }  // circle
                if (pad.buttonX.pressed)              { buttons |= ButtonBits::X; }  // square
                if (pad.buttonY.pressed)              { buttons |= ButtonBits::Y; }  // triangle

                m_rawGamepadState.m_digitalButtonStates = buttons;
                m_rawGamepadState.m_triggerButtonLState  = pad.leftTrigger.value;
                m_rawGamepadState.m_triggerButtonRState  = pad.rightTrigger.value;
                m_rawGamepadState.m_thumbStickLeftXState  = pad.leftThumbstick.xAxis.value;
                m_rawGamepadState.m_thumbStickLeftYState  = pad.leftThumbstick.yAxis.value;
                m_rawGamepadState.m_thumbStickRightXState = pad.rightThumbstick.xAxis.value;
                m_rawGamepadState.m_thumbStickRightYState = pad.rightThumbstick.yAxis.value;
            }
        }
        ProcessRawGamepadState(m_rawGamepadState);
    }

    AZStd::unique_ptr<AzFramework::InputDeviceGamepad::Implementation> DualSenseMacGamepadImplFactory::Create(
        AzFramework::InputDeviceGamepad& inputDevice)
    {
        if (m_pendingController == nullptr)
        {
            AZLOG_WARN("DualSense: Mac factory invoked with no pending controller");
            return nullptr;
        }
        return AZStd::make_unique<InputDeviceGamepadDualSenseMac>(inputDevice, m_pendingController);
    }
} // namespace DualSense
```

Delete the temporary `DualSenseMacGamepadImplFactory::Create` definition (and its `DualSenseDebugGamepadImpl.h` include) from `DualSenseSystemImpl_Mac.mm`. Add the two new files to `Code/Platform/Mac/dualsense_private_files.cmake`.

- [ ] **Step 3: Build module + tests, run tests**

```bash
cd ~/Source/o3de
cmake --build build/mac_ninja --config profile --target DualSense DualSense.Tests -j 10
./build/mac_ninja/bin/profile/AzTestRunner $PWD/build/mac_ninja/bin/profile/libDualSense.Tests.dylib AzRunUnitTests
```

Expected: builds, all tests PASS.

- [ ] **Step 4: Manual hardware verification (the real deliverable)**

In the testbed Editor with a DualSense connected (try BOTH USB and Bluetooth):
1. Console shows the takeover log from Task 7.
2. Create an entity with an Input component bound to a default `.inputbindings` asset (or use any input-driven sample); verify: all 4 face buttons, d-pad, shoulders, stick clicks, menu/options, both analog triggers (gradual values), both sticks (full range, no drift at rest).
3. Sticks at rest produce no held events (deadzones effective).

Append results to `docs/hardware-smoke.md` under a new `## Phase 1 — Mac input` section listing each of the above as a checkbox.

- [ ] **Step 5: Commit**

```bash
cd ~/Source/o3de-dualsense-gem
git add Code docs/hardware-smoke.md
git commit -m "feat(mac): full 32-channel DualSense input via GCDualSenseGamepad"
```

---

### Task 9: Rumble — SetVibration via CoreHaptics handle engines

**Files:**
- Create: `Code/Platform/Mac/DualSenseHapticsMac.h`, `Code/Platform/Mac/DualSenseHapticsMac.mm`
- Modify: `Code/Platform/Mac/InputDeviceGamepadDualSenseMac.h` / `.mm`
- Modify: `Code/Source/Clients/DualSenseSystemComponent.cpp` (test console command)
- Modify: `Code/Platform/Mac/dualsense_private_files.cmake`, `docs/hardware-smoke.md`

**Interfaces:**
- Consumes: `InputDeviceGamepadDualSenseMac` (Task 8).
- Produces: `class DualSense::DualSenseHapticsMac` — ctor `(void* gcController)`, `void SetVibration(float left, float right);`, `void Stop();`. Console command `dualsense_rumble <left> <right> [slot]`.

- [ ] **Step 1: Implement the haptics wrapper**

`Code/Platform/Mac/DualSenseHapticsMac.h`:

```cpp
#pragma once

namespace DualSense
{
    //! Drives the DualSense voice-coil actuators through CoreHaptics engines
    //! created per handle locality (GCDeviceHaptics). Emulates classic two-motor
    //! rumble: one continuous haptic player per side, intensity = motor speed.
    class DualSenseHapticsMac
    {
    public:
        explicit DualSenseHapticsMac(void* gcController); // GCController*, not retained
        ~DualSenseHapticsMac();

        void SetVibration(float leftMotorSpeedNormalized, float rightMotorSpeedNormalized);
        void Stop();

    private:
        void* m_leftEngine = nullptr;   // CHHapticEngine*, retained
        void* m_rightEngine = nullptr;  // CHHapticEngine*, retained
        void* m_leftPlayer = nullptr;   // id<CHHapticPatternPlayer>, retained
        void* m_rightPlayer = nullptr;  // id<CHHapticPatternPlayer>, retained
    };
} // namespace DualSense
```

`Code/Platform/Mac/DualSenseHapticsMac.mm`:

```objc
#include <DualSenseHapticsMac.h>

#include <AzCore/Console/ILogger.h>
#include <AzCore/Math/MathUtils.h>

#import <GameController/GameController.h>
#import <CoreHaptics/CoreHaptics.h>

namespace DualSense
{
    namespace
    {
        void* CreateStartedEngine(GCController* controller, GCHapticsLocality locality) API_AVAILABLE(macos(11.3))
        {
            CHHapticEngine* engine = [controller.haptics createEngineWithLocality:locality];
            if (!engine)
            {
                return nullptr;
            }
            NSError* error = nil;
            if (![engine startAndReturnError:&error])
            {
                AZLOG_WARN("DualSense: haptic engine start failed: %s",
                           error.localizedDescription.UTF8String);
                return nullptr;
            }
            engine.resetHandler = ^{ [engine startAndReturnError:nil]; };
            return (__bridge_retained void*)engine;
        }

        // Replaces *playerSlot with a new infinite continuous player at `intensity`,
        // or stops/clears it when intensity is ~0.
        void UpdateSide(void* engineOpaque, void** playerSlot, float intensity) API_AVAILABLE(macos(11.3))
        {
            if (!engineOpaque)
            {
                return;
            }
            CHHapticEngine* engine = (__bridge CHHapticEngine*)engineOpaque;

            if (*playerSlot)
            {
                id<CHHapticPatternPlayer> old = (__bridge_transfer id<CHHapticPatternPlayer>)*playerSlot;
                [old stopAtTime:0 error:nil];
                *playerSlot = nullptr;
            }
            if (intensity <= 0.001f)
            {
                return;
            }

            NSError* error = nil;
            CHHapticEventParameter* intensityParam =
                [[CHHapticEventParameter alloc] initWithParameterID:CHHapticEventParameterIDHapticIntensity
                                                              value:intensity];
            CHHapticEvent* event =
                [[CHHapticEvent alloc] initWithEventType:CHHapticEventTypeHapticContinuous
                                              parameters:@[ intensityParam ]
                                            relativeTime:0
                                                duration:GCHapticDurationInfinite];
            CHHapticPattern* pattern = [[CHHapticPattern alloc] initWithEvents:@[ event ]
                                                               parameterCurves:@[]
                                                                         error:&error];
            if (!pattern)
            {
                AZLOG_WARN("DualSense: haptic pattern creation failed: %s",
                           error.localizedDescription.UTF8String);
                return;
            }
            id<CHHapticPatternPlayer> player = [engine createPlayerWithPattern:pattern error:&error];
            if (!player || ![player startAtTime:0 error:&error])
            {
                AZLOG_WARN("DualSense: haptic player start failed: %s",
                           error ? error.localizedDescription.UTF8String : "unknown");
                return;
            }
            *playerSlot = (__bridge_retained void*)player;
        }
    } // namespace

    DualSenseHapticsMac::DualSenseHapticsMac(void* gcController)
    {
        if (@available(macOS 11.3, *))
        {
            GCController* controller = (__bridge GCController*)gcController;
            m_leftEngine = CreateStartedEngine(controller, GCHapticsLocalityLeftHandle);
            m_rightEngine = CreateStartedEngine(controller, GCHapticsLocalityRightHandle);
        }
    }

    DualSenseHapticsMac::~DualSenseHapticsMac()
    {
        Stop();
        if (@available(macOS 11.3, *))
        {
            for (void** engineSlot : { &m_leftEngine, &m_rightEngine })
            {
                if (*engineSlot)
                {
                    CHHapticEngine* engine = (__bridge_transfer CHHapticEngine*)*engineSlot;
                    [engine stopWithCompletionHandler:nil];
                    *engineSlot = nullptr;
                }
            }
        }
    }

    void DualSenseHapticsMac::SetVibration(float leftMotorSpeedNormalized, float rightMotorSpeedNormalized)
    {
        if (@available(macOS 11.3, *))
        {
            UpdateSide(m_leftEngine, &m_leftPlayer, AZ::GetClamp(leftMotorSpeedNormalized, 0.0f, 1.0f));
            UpdateSide(m_rightEngine, &m_rightPlayer, AZ::GetClamp(rightMotorSpeedNormalized, 0.0f, 1.0f));
        }
    }

    void DualSenseHapticsMac::Stop()
    {
        SetVibration(0.0f, 0.0f);
    }
} // namespace DualSense
```

- [ ] **Step 2: Wire into the gamepad implementation**

In `InputDeviceGamepadDualSenseMac.h`: add member `AZStd::unique_ptr<class DualSenseHapticsMac> m_haptics;` (include `<AzCore/std/smart_ptr/unique_ptr.h>` and forward-declare or include the header in the .mm). In the .mm ctor: `m_haptics = AZStd::make_unique<DualSenseHapticsMac>(gcController);`. Replace the empty `SetVibration` body:

```cpp
    void InputDeviceGamepadDualSenseMac::SetVibration(float leftMotorSpeedNormalized, float rightMotorSpeedNormalized)
    {
        if (m_haptics)
        {
            m_haptics->SetVibration(leftMotorSpeedNormalized, rightMotorSpeedNormalized);
        }
    }
```

In the destructor, `m_haptics.reset();` before releasing the controller. Add both new files to `Code/Platform/Mac/dualsense_private_files.cmake`.

- [ ] **Step 3: Test console command**

In `DualSenseSystemComponent.cpp`, `DebugCommands` namespace, add:

```cpp
        static void dualsense_rumble(const AZ::ConsoleCommandContainer& arguments)
        {
            float left = 0.5f;
            float right = 0.5f;
            AZ::u32 slot = 0;
            if (arguments.size() >= 2)
            {
                left = static_cast<float>(atof(AZStd::string(arguments[0]).c_str()));
                right = static_cast<float>(atof(AZStd::string(arguments[1]).c_str()));
            }
            if (arguments.size() >= 3)
            {
                slot = static_cast<AZ::u32>(strtoul(AZStd::string(arguments[2]).c_str(), nullptr, 10));
            }
            AzFramework::InputHapticFeedbackRequestBus::Event(
                AzFramework::InputDeviceGamepad::IdForIndexN(slot),
                &AzFramework::InputHapticFeedbackRequests::SetVibration, left, right);
        }
        AZ_CONSOLEFREEFUNC(dualsense_rumble, AZ::ConsoleFunctorFlags::DontReplicate,
            "Send SetVibration to a gamepad slot: dualsense_rumble <left 0-1> <right 0-1> [slot]");
```

with `#include <AzFramework/Input/Buses/Requests/InputHapticFeedbackRequestBus.h>` added.

- [ ] **Step 4: Build + tests + hardware verification**

Build module + tests, run unit tests (all PASS — no regressions). Hardware, in the testbed Editor, DualSense connected over Bluetooth AND over USB:
1. `dualsense_rumble 1 0` → strong vibration concentrated on the left grip; `dualsense_rumble 0 1` → right grip; `dualsense_rumble 0 0` → silence.
2. `dualsense_rumble 0.2 0.2` → clearly weaker than `1 1`.
3. Disconnect mid-rumble → no crash, clean restore logs.

Append a `## Phase 1 — rumble` section with these checkboxes to `docs/hardware-smoke.md`.

- [ ] **Step 5: Commit**

```bash
cd ~/Source/o3de-dualsense-gem
git add Code docs/hardware-smoke.md
git commit -m "feat(mac): rumble via CoreHaptics handle engines; dualsense_rumble command"
```

---

### Task 10: Lightbar + Phase 1 wrap-up

**Files:**
- Modify: `Code/Platform/Mac/InputDeviceGamepadDualSenseMac.mm`
- Modify: `Code/Source/Clients/DualSenseSystemComponent.cpp` (lightbar console command)
- Modify: `README.md`, `docs/hardware-smoke.md`

**Interfaces:**
- Consumes: `InputDeviceGamepadDualSenseMac` (Tasks 8–9); engine bus `InputLightBarRequestBus` (already routed to the implementation by `InputDeviceGamepad`).
- Produces: working `SetLightBarColor`/`ResetLightBarColor`; console command `dualsense_lightbar <r> <g> <b> [slot]`; the `phase-1` tag.

- [ ] **Step 1: Implement the lightbar methods**

In `InputDeviceGamepadDualSenseMac.mm` replace the empty bodies:

```objc
    void InputDeviceGamepadDualSenseMac::SetLightBarColor(const AZ::Color& color)
    {
        if (@available(macOS 11.0, *))
        {
            GCController* controller = (__bridge GCController*)m_controller;
            if (controller.light)
            {
                controller.light.color = [[GCColor alloc] initWithRed:color.GetR()
                                                                green:color.GetG()
                                                                 blue:color.GetB()];
            }
        }
    }

    void InputDeviceGamepadDualSenseMac::ResetLightBarColor()
    {
        // The DualSense default when attached to a Mac is a dim blue-ish white;
        // there is no OS "reset" API, so approximate the default.
        SetLightBarColor(AZ::Color(0.0f, 0.25f, 1.0f, 1.0f));
    }
```

(`#include <AzCore/Math/Color.h>` if not already pulled in transitively.)

- [ ] **Step 2: Console command**

In `DebugCommands` add:

```cpp
        static void dualsense_lightbar(const AZ::ConsoleCommandContainer& arguments)
        {
            if (arguments.size() < 3)
            {
                AZLOG_INFO("Usage: dualsense_lightbar <r 0-1> <g 0-1> <b 0-1> [slot]");
                return;
            }
            const float r = static_cast<float>(atof(AZStd::string(arguments[0]).c_str()));
            const float g = static_cast<float>(atof(AZStd::string(arguments[1]).c_str()));
            const float b = static_cast<float>(atof(AZStd::string(arguments[2]).c_str()));
            const AZ::u32 slot = arguments.size() >= 4
                ? static_cast<AZ::u32>(strtoul(AZStd::string(arguments[3]).c_str(), nullptr, 10)) : 0;
            AzFramework::InputLightBarRequestBus::Event(
                AzFramework::InputDeviceGamepad::IdForIndexN(slot),
                &AzFramework::InputLightBarRequests::SetLightBarColor,
                AZ::Color(r, g, b, 1.0f));
        }
        AZ_CONSOLEFREEFUNC(dualsense_lightbar, AZ::ConsoleFunctorFlags::DontReplicate,
            "Set a gamepad slot's light bar color: dualsense_lightbar <r> <g> <b> [slot]");
```

with `#include <AzFramework/Input/Buses/Requests/InputLightBarRequestBus.h>` and `#include <AzCore/Math/Color.h>`.

- [ ] **Step 3: Build + tests + hardware verification**

Build module + tests; unit tests all PASS. Hardware (USB and BT): `dualsense_lightbar 1 0 0` → red; `0 1 0` → green; disconnect/reconnect → lightbar behavior sane, takeover/restore logs clean. Append `## Phase 1 — lightbar` checkboxes to `docs/hardware-smoke.md`.

- [ ] **Step 4: Update README status + tag**

In `README.md` change the status line to:

```markdown
Status: Phase 1 complete — on macOS (11.3+) a DualSense works as the standard
gamepad device with rumble (CoreHaptics) and light bar. Next: trigger-effect API
(phase 2). Console commands: `dualsense_rumble`, `dualsense_lightbar`,
`dualsense_debug_swap`, `dualsense_debug_restore`.
```

- [ ] **Step 5: Commit and tag**

```bash
cd ~/Source/o3de-dualsense-gem
git add Code README.md docs/hardware-smoke.md
git commit -m "feat(mac): lightbar via GCDeviceLight; phase 1 complete"
git tag phase-1
```

---

## Self-review notes (resolved during planning)

- **Spec coverage:** Phases 0–1 of the spec map to Tasks 1–10. The spec's `DualSenseTriggerEffectRequestBus`, setreg settings, and BehaviorContext reflection are Phase 2 — deliberately out of this plan.
- **Engine-name risk:** two places intentionally tell the implementer to verify engine identifiers against headers rather than trust this plan: the `Button::DU/DD/DL/DR` d-pad member names (Task 2 Step 4 note) and the Mac deadzone constants (Task 8 Step 1). Both are read-and-copy steps with exact file paths.
- **Soft vs required services:** the spec says "requires InputSystemService"; this plan downgrades it to a *dependent* service because the Builders variant activates the same system component inside AssetProcessor, where no input system exists (Task 4 Step 3 comment). Spec updated understanding — carry this back into the spec during Phase 2 if it survives contact with reality.
- **Type consistency check:** `SwapSlotToFactory(AZ::u32, ImplementationFactory*)` (Task 4) is what Task 7's monitor calls; `DualSenseMacGamepadImplFactory::m_pendingController` (declared Task 7, consumed Task 8) is `void*` in both; `DualSenseHapticsMac` ctor takes the unretained `GCController*` that `InputDeviceGamepadDualSenseMac` retains for its own lifetime (haptics object destroyed first in the dtor — ordering noted in Task 9 Step 2).
