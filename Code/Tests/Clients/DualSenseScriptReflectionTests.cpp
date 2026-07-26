#include <AzCore/UnitTest/TestTypes.h>
#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Component/ComponentApplicationBus.h>
#include <AzCore/Interface/Interface.h>
#include <AzCore/Math/Color.h>
#include <AzCore/Script/ScriptContext.h>
#include <AzFramework/Input/Buses/Requests/InputHapticFeedbackRequestBus.h>
#include <AzFramework/Input/Buses/Requests/InputLightBarRequestBus.h>
#include <AzFramework/Input/Devices/InputDeviceId.h>
#include <AzFramework/Input/Devices/Gamepad/InputDeviceGamepad.h>
#include <DualSense/DualSenseTriggerEffects.h>
#include <DualSense/DualSenseHaptics.h>

namespace DualSenseTests
{
    using ScriptReflectionFixture = UnitTest::LeakDetectionFixture;

    TEST_F(ScriptReflectionFixture, BehaviorContext_RegistersTriggerEffectClassAndBus)
    {
        AZ::BehaviorContext bc;
        DualSense::TriggerEffect::Reflect(&bc);

        EXPECT_NE(bc.m_classes.find("DualSenseTriggerEffect"), bc.m_classes.end());
        EXPECT_NE(bc.m_ebuses.find("DualSenseTriggerEffectRequestBus"), bc.m_ebuses.end());
    }

    TEST_F(ScriptReflectionFixture, BehaviorContext_RegistersHapticPulseBus)
    {
        AZ::BehaviorContext bc;
        DualSense::ReflectDualSenseHapticPulseBus(&bc);

        EXPECT_NE(bc.m_ebuses.find("DualSenseHapticPulseRequestBus"), bc.m_ebuses.end());
    }

    // Phase 2.6, Task 1: PlayHapticBuzz/StopHaptics (frozen additions to
    // DualSenseHapticPulseRequests, Code/Include/DualSense/DualSenseHaptics.h). Asserts both new
    // events are actually reflected onto the bus's BehaviorEBus, not just that the bus itself
    // exists (which BehaviorContext_RegistersHapticPulseBus above already covers).
    TEST_F(ScriptReflectionFixture, BehaviorContext_RegistersHapticBuzzAndStopEvents)
    {
        AZ::BehaviorContext bc;
        DualSense::ReflectDualSenseHapticPulseBus(&bc);

        auto ebusIt = bc.m_ebuses.find("DualSenseHapticPulseRequestBus");
        ASSERT_NE(ebusIt, bc.m_ebuses.end());
        EXPECT_NE(ebusIt->second->m_events.find("PlayHapticBuzz"), ebusIt->second->m_events.end());
        EXPECT_NE(ebusIt->second->m_events.find("StopHaptics"), ebusIt->second->m_events.end());
    }

    // Phase 2.5, Task 2: DualSenseTriggerNotificationBus (weapon-fire notifications) is
    // reflected via its own free function, mirroring ReflectDualSenseHapticPulseBus above.
    TEST_F(ScriptReflectionFixture, BehaviorContext_RegistersTriggerNotificationBus)
    {
        AZ::BehaviorContext bc;
        DualSense::ReflectDualSenseTriggerNotificationBus(&bc);

        EXPECT_NE(bc.m_ebuses.find("DualSenseTriggerNotificationBus"), bc.m_ebuses.end());
    }

    namespace
    {
        // Filled in by the bound "capture" global method below, so the C++ test body
        // can inspect what a Lua script actually did to a DualSenseTriggerEffect instance.
        DualSense::TriggerEffect g_capturedEffect;
        int g_captureCount = 0;

        void CaptureTriggerEffect(const DualSense::TriggerEffect& effect)
        {
            g_capturedEffect = effect;
            ++g_captureCount;
        }
    } // namespace

    // AZ::ScriptContext::BindTo()/Execute() look up AZ::Interface<AZ::ComponentApplicationRequests>
    // for BehaviorContext access, exactly like AzCore's own (test-target-private, not exported to
    // gems) Code/Framework/AzCore/Tests/BehaviorContextFixture.h. Reproduced here so this gem can
    // drive a real Lua round-trip through AZ::ScriptContext.
    class ScriptRoundTripFixture
        : public UnitTest::LeakDetectionFixture
        , public AZ::ComponentApplicationBus::Handler
    {
    public:
        void SetUp() override
        {
            LeakDetectionFixture::SetUp();
            m_behaviorContext = aznew AZ::BehaviorContext();
            AZ::ComponentApplicationBus::Handler::BusConnect();
            AZ::Interface<AZ::ComponentApplicationRequests>::Register(this);
        }

        void TearDown() override
        {
            AZ::Interface<AZ::ComponentApplicationRequests>::Unregister(this);
            AZ::ComponentApplicationBus::Handler::BusDisconnect();
            delete m_behaviorContext;
            m_behaviorContext = nullptr;
            LeakDetectionFixture::TearDown();
        }

        // AZ::ComponentApplicationRequests — only GetBehaviorContext() matters for this fixture.
        AZ::ComponentApplication* GetApplication() override { return nullptr; }
        void RegisterComponentDescriptor(const AZ::ComponentDescriptor*) override {}
        void UnregisterComponentDescriptor(const AZ::ComponentDescriptor*) override {}
        void RegisterEntityAddedEventHandler(AZ::EntityAddedEvent::Handler&) override {}
        void RegisterEntityRemovedEventHandler(AZ::EntityRemovedEvent::Handler&) override {}
        void RegisterEntityActivatedEventHandler(AZ::EntityActivatedEvent::Handler&) override {}
        void RegisterEntityDeactivatedEventHandler(AZ::EntityDeactivatedEvent::Handler&) override {}
        void SignalEntityActivated(AZ::Entity*) override {}
        void SignalEntityDeactivated(AZ::Entity*) override {}
        bool AddEntity(AZ::Entity*) override { return true; }
        bool RemoveEntity(AZ::Entity*) override { return true; }
        bool DeleteEntity(const AZ::EntityId&) override { return true; }
        AZ::Entity* FindEntity(const AZ::EntityId&) override { return nullptr; }
        AZ::SerializeContext* GetSerializeContext() override { return nullptr; }
        AZ::BehaviorContext* GetBehaviorContext() override { return m_behaviorContext; }
        AZ::JsonRegistrationContext* GetJsonRegistrationContext() override { return nullptr; }
        const char* GetEngineRoot() const override { return nullptr; }
        const char* GetExecutableFolder() const override { return nullptr; }
        void EnumerateEntities(const EntityCallback&) override {}
        void QueryApplicationType(AZ::ApplicationTypeQuery&) const override {}

    protected:
        AZ::BehaviorContext* m_behaviorContext = nullptr;
    };

    TEST_F(ScriptRoundTripFixture, Lua_ScalarPropertiesRoundTripThroughBehaviorContext)
    {
        // In a real running app (Editor/Launcher), AzFramework::InputSystemComponent::Reflect()
        // has already reflected InputDeviceId (the EBus's BusIdType) to this BehaviorContext. A
        // bare unit-test context doesn't get that for free, so reflect it explicitly to faithfully
        // reproduce the runtime environment DualSenseTriggerEffectRequestBus is actually used in.
        AzFramework::InputDeviceId::Reflect(m_behaviorContext);
        DualSense::TriggerEffect::Reflect(m_behaviorContext);
        m_behaviorContext->Method("DualSenseTests_CaptureTriggerEffect", &CaptureTriggerEffect);

        g_capturedEffect = DualSense::TriggerEffect{};
        g_captureCount = 0;

        AZ::ScriptContext sc;
        sc.BindTo(m_behaviorContext);

        EXPECT_TRUE(sc.Execute("e = DualSenseTriggerEffect()"));
        EXPECT_TRUE(sc.Execute("e.strength = 0.5"));
        EXPECT_TRUE(sc.Execute("e.startPosition = 0.25"));
        EXPECT_TRUE(sc.Execute("DualSenseTests_CaptureTriggerEffect(e)"));

        ASSERT_EQ(g_captureCount, 1);
        EXPECT_FLOAT_EQ(g_capturedEffect.m_strength, 0.5f);
        EXPECT_FLOAT_EQ(g_capturedEffect.m_startPosition, 0.25f);
    }

    // Evidence test for the coordinator's positionalValues (AZStd::array<float,10>) review flag.
    // AZStd::array has an OnDemandReflection specialization
    // (Code/Framework/AzCore/AzCore/RTTI/AzStdOnDemandReflection.inl, "OnDemand reflection for
    // AZStd::array") that BehaviorContext::Property/Method queue automatically for any parameter
    // or return type that needs it (see BehaviorContext.h SetParameters/OnDemandReflectFunctions).
    // BehaviorValueProperty's getter returns T& (a live reference to the member, not a copy — see
    // Internal::BehaviorValuePropertyHelper<T C::*>::Get), so a script-side mutation of
    // "e.positionalValues" should mutate the real TriggerEffect it came from. This test proves that
    // end-to-end via Lua rather than asserting it.
    TEST_F(ScriptRoundTripFixture, Lua_PositionalValuesArrayFillRoundTripsThroughBehaviorContext)
    {
        // In a real running app (Editor/Launcher), AzFramework::InputSystemComponent::Reflect()
        // has already reflected InputDeviceId (the EBus's BusIdType) to this BehaviorContext. A
        // bare unit-test context doesn't get that for free, so reflect it explicitly to faithfully
        // reproduce the runtime environment DualSenseTriggerEffectRequestBus is actually used in.
        AzFramework::InputDeviceId::Reflect(m_behaviorContext);
        DualSense::TriggerEffect::Reflect(m_behaviorContext);
        m_behaviorContext->Method("DualSenseTests_CaptureTriggerEffect", &CaptureTriggerEffect);

        g_capturedEffect = DualSense::TriggerEffect{};
        g_captureCount = 0;

        AZ::ScriptContext sc;
        sc.BindTo(m_behaviorContext);

        EXPECT_TRUE(sc.Execute("e = DualSenseTriggerEffect()"));
        EXPECT_TRUE(sc.Execute("e.positionalValues:Fill(0.75)"));
        EXPECT_TRUE(sc.Execute("DualSenseTests_CaptureTriggerEffect(e)"));

        ASSERT_EQ(g_captureCount, 1);
        for (float value : g_capturedEffect.m_positionalValues)
        {
            EXPECT_FLOAT_EQ(value, 0.75f);
        }
    }

    namespace
    {
        // Records whatever DualSenseTriggerEffectRequestBus::SetTriggerEffect is called with, so
        // the test below can drive a dispatch entirely from Lua (the README example shape) and
        // assert on the C++ side what actually arrived.
        class TestTriggerEffectHandler : public DualSense::DualSenseTriggerEffectRequestBus::Handler
        {
        public:
            explicit TestTriggerEffectHandler(const AzFramework::InputDeviceId& id)
            {
                DualSense::DualSenseTriggerEffectRequestBus::Handler::BusConnect(id);
            }

            ~TestTriggerEffectHandler() override
            {
                DualSense::DualSenseTriggerEffectRequestBus::Handler::BusDisconnect();
            }

            void SetTriggerEffect(DualSense::Trigger trigger, const DualSense::TriggerEffect& effect) override
            {
                m_lastTrigger = trigger;
                m_lastEffect = effect;
                ++m_callCount;
            }

            void ClearTriggerEffects() override
            {
            }

            DualSense::Trigger m_lastTrigger = DualSense::Trigger::L2;
            DualSense::TriggerEffect m_lastEffect;
            int m_callCount = 0;
        };

        // Filled in by the bound "capture" global method below, used only by the engine-gap
        // regression assertion (see Lua_ReadmeExampleShapeDispatchesThroughRequestBusWithEnumModeAssignment).
        AzFramework::InputDeviceId g_capturedDeviceId;
        int g_deviceIdCaptureCount = 0;

        void CaptureDeviceId(const AzFramework::InputDeviceId& id)
        {
            g_capturedDeviceId = id;
            ++g_deviceIdCaptureCount;
        }
    } // namespace

    // README.md's "Lua example" is the documented usage shape for this whole feature; this test
    // executes it verbatim (module-qualified dot-call, and the DualSense_GetGamepadDeviceId helper
    // -- see Fix 3 follow-up below) end-to-end through a real Lua script, including the
    // previously-untested case of assigning an enum-typed BehaviorValueProperty
    // ("e.mode = DualSenseTriggerEffectMode_Weapon") from script.
    //
    // Fix 3 follow-up: the original attempt at this test used
    // `InputDeviceId(InputDeviceGamepad.name, 0)` (matching the README verbatim at the time) to
    // build the deviceId in Lua, and that is a genuine engine bug -- see the regression assertion
    // at the bottom of this test, which documents it directly rather than relying on prose. The
    // gem-side fix is DualSense_GetGamepadDeviceId(slotIndex), a BehaviorContext global method
    // (DualSenseTriggerEffects.h) that builds the InputDeviceId in C++ and hands Lua an
    // already-correct object, sidestepping the broken Lua-side constructor entirely. README.md was
    // updated to use this helper instead of raw InputDeviceId construction.
    TEST_F(ScriptRoundTripFixture, Lua_ReadmeExampleShapeDispatchesThroughRequestBusWithEnumModeAssignment)
    {
        AzFramework::InputDeviceId::Reflect(m_behaviorContext);
        DualSense::TriggerEffect::Reflect(m_behaviorContext); // also reflects DualSense_GetGamepadDeviceId
        m_behaviorContext->Method("DualSenseTests_CaptureDeviceId", &CaptureDeviceId);

        TestTriggerEffectHandler handler(AzFramework::InputDeviceGamepad::IdForIndex0);

        AZ::ScriptContext sc;
        sc.BindTo(m_behaviorContext);

        EXPECT_TRUE(sc.Execute("e = DualSenseTriggerEffect()"));
        EXPECT_TRUE(sc.Execute("e.mode = DualSenseTriggerEffectMode_Weapon"));
        EXPECT_TRUE(sc.Execute("e.startPosition = 0.2"));
        EXPECT_TRUE(sc.Execute("e.endPosition = 0.8"));
        EXPECT_TRUE(sc.Execute("e.strength = 0.9"));
        EXPECT_TRUE(sc.Execute("deviceId = DualSense_GetGamepadDeviceId(0)"));
        EXPECT_TRUE(sc.Execute("DualSenseTriggerEffectRequestBus.Event.SetTriggerEffect(deviceId, DualSenseTrigger_R2, e)"));

        ASSERT_EQ(handler.m_callCount, 1);
        EXPECT_EQ(handler.m_lastTrigger, DualSense::Trigger::R2);
        EXPECT_EQ(handler.m_lastEffect.m_mode, DualSense::TriggerEffectMode::Weapon);
        EXPECT_FLOAT_EQ(handler.m_lastEffect.m_startPosition, 0.2f);
        EXPECT_FLOAT_EQ(handler.m_lastEffect.m_endPosition, 0.8f);
        EXPECT_FLOAT_EQ(handler.m_lastEffect.m_strength, 0.9f);

        // Regression assertion documenting the engine gap this helper works around: constructing
        // InputDeviceId directly in Lua silently invokes the default constructor and discards both
        // arguments (AzFramework::InputDeviceId::Reflect() reflects two Constructor<>() overloads
        // but the class is also default-constructible, and AZ::ScriptContext's Lua binding only
        // supports overload-free constructor dispatch -- see
        // Code/Framework/AzCore/AzCore/Script/ScriptContext.cpp:5096-5137 in the engine repo -- so
        // it falls back to the default ctor whenever one is registered). If the engine ever adds a
        // Script::Attributes::ConstructorOverride to fix this, badId below will start comparing
        // equal to deviceId, this assertion will fail, and that is the signal to simplify
        // README.md/this test back to raw InputDeviceId construction and drop the helper.
        g_deviceIdCaptureCount = 0;
        EXPECT_TRUE(sc.Execute("badId = InputDeviceId('gamepad', 0)"));
        EXPECT_TRUE(sc.Execute("DualSenseTests_CaptureDeviceId(badId)"));
        ASSERT_EQ(g_deviceIdCaptureCount, 1);
        EXPECT_NE(g_capturedDeviceId, AzFramework::InputDeviceGamepad::IdForIndex0)
            << "Engine gap appears fixed: InputDeviceId('gamepad', 0) now matches IdForIndex0. "
               "Simplify README.md/this test to construct InputDeviceId directly and drop "
               "DualSense_GetGamepadDeviceId.";
    }

    namespace
    {
        // Records whatever DualSenseHapticPulseRequestBus is called with, so the test below can
        // drive a dispatch entirely from Lua and assert on the C++ side what actually arrived.
        // Mirrors TestTriggerEffectHandler above.
        class TestHapticPulseHandler : public DualSense::DualSenseHapticPulseRequestBus::Handler
        {
        public:
            explicit TestHapticPulseHandler(const AzFramework::InputDeviceId& id)
            {
                DualSense::DualSenseHapticPulseRequestBus::Handler::BusConnect(id);
            }

            ~TestHapticPulseHandler() override
            {
                DualSense::DualSenseHapticPulseRequestBus::Handler::BusDisconnect();
            }

            void PlayHapticPulse(float leftIntensity, float rightIntensity, float sharpness) override
            {
                m_lastLeftIntensity = leftIntensity;
                m_lastRightIntensity = rightIntensity;
                m_lastSharpness = sharpness;
                ++m_pulseCallCount;
            }

            void SetAutoRecoil(DualSense::Trigger trigger, bool enabled, float intensity, float sharpness) override
            {
                m_lastRecoilTrigger = trigger;
                m_lastRecoilEnabled = enabled;
                m_lastRecoilIntensity = intensity;
                m_lastRecoilSharpness = sharpness;
                ++m_recoilCallCount;
            }

            // Phase 2.6, Task 1 additions.
            void PlayHapticBuzz(float leftIntensity, float rightIntensity, float sharpness, float durationSeconds) override
            {
                m_lastBuzzLeftIntensity = leftIntensity;
                m_lastBuzzRightIntensity = rightIntensity;
                m_lastBuzzSharpness = sharpness;
                m_lastBuzzDurationSeconds = durationSeconds;
                ++m_buzzCallCount;
            }

            void StopHaptics() override
            {
                ++m_stopCallCount;
            }

            float m_lastLeftIntensity = 0.0f;
            float m_lastRightIntensity = 0.0f;
            float m_lastSharpness = 0.0f;
            int m_pulseCallCount = 0;

            DualSense::Trigger m_lastRecoilTrigger = DualSense::Trigger::L2;
            bool m_lastRecoilEnabled = false;
            float m_lastRecoilIntensity = 0.0f;
            float m_lastRecoilSharpness = 0.0f;
            int m_recoilCallCount = 0;

            float m_lastBuzzLeftIntensity = 0.0f;
            float m_lastBuzzRightIntensity = 0.0f;
            float m_lastBuzzSharpness = 0.0f;
            float m_lastBuzzDurationSeconds = 0.0f;
            int m_buzzCallCount = 0;
            int m_stopCallCount = 0;
        };
    } // namespace

    // Same dispatch shape as Lua_ReadmeExampleShapeDispatchesThroughRequestBusWithEnumModeAssignment
    // above, but for the new DualSenseHapticPulseRequestBus (Phase 2.5, Task 1): drives
    // PlayHapticPulse entirely from Lua via the same DualSense_GetGamepadDeviceId helper, and
    // asserts the fixture handler received exactly the values passed from script.
    TEST_F(ScriptRoundTripFixture, Lua_HapticPulseDispatchesThroughRequestBus)
    {
        AzFramework::InputDeviceId::Reflect(m_behaviorContext);
        DualSense::TriggerEffect::Reflect(m_behaviorContext); // also reflects DualSense_GetGamepadDeviceId
        DualSense::ReflectDualSenseHapticPulseBus(m_behaviorContext);

        TestHapticPulseHandler handler(AzFramework::InputDeviceGamepad::IdForIndex0);

        AZ::ScriptContext sc;
        sc.BindTo(m_behaviorContext);

        EXPECT_TRUE(sc.Execute("deviceId = DualSense_GetGamepadDeviceId(0)"));
        EXPECT_TRUE(sc.Execute("DualSenseHapticPulseRequestBus.Event.PlayHapticPulse(deviceId, 0.9, 0.2, 0.7)"));

        ASSERT_EQ(handler.m_pulseCallCount, 1);
        EXPECT_FLOAT_EQ(handler.m_lastLeftIntensity, 0.9f);
        EXPECT_FLOAT_EQ(handler.m_lastRightIntensity, 0.2f);
        EXPECT_FLOAT_EQ(handler.m_lastSharpness, 0.7f);
    }

    // Carried-forward test debt from the Task 1 review (SetAutoRecoil had zero coverage): same
    // dispatch shape as Lua_HapticPulseDispatchesThroughRequestBus above, but drives
    // SetAutoRecoil from Lua instead of PlayHapticPulse, and asserts the fixture handler
    // (TestHapticPulseHandler, already wired to record SetAutoRecoil calls) received exactly
    // what script passed.
    TEST_F(ScriptRoundTripFixture, Lua_SetAutoRecoilDispatchesThroughRequestBus)
    {
        AzFramework::InputDeviceId::Reflect(m_behaviorContext);
        DualSense::TriggerEffect::Reflect(m_behaviorContext); // also reflects DualSense_GetGamepadDeviceId + Trigger enums
        DualSense::ReflectDualSenseHapticPulseBus(m_behaviorContext);

        TestHapticPulseHandler handler(AzFramework::InputDeviceGamepad::IdForIndex0);

        AZ::ScriptContext sc;
        sc.BindTo(m_behaviorContext);

        EXPECT_TRUE(sc.Execute("deviceId = DualSense_GetGamepadDeviceId(0)"));
        EXPECT_TRUE(sc.Execute(
            "DualSenseHapticPulseRequestBus.Event.SetAutoRecoil(deviceId, DualSenseTrigger_R2, true, 0.6, 0.3)"));

        ASSERT_EQ(handler.m_recoilCallCount, 1);
        EXPECT_EQ(handler.m_lastRecoilTrigger, DualSense::Trigger::R2);
        EXPECT_TRUE(handler.m_lastRecoilEnabled);
        EXPECT_FLOAT_EQ(handler.m_lastRecoilIntensity, 0.6f);
        EXPECT_FLOAT_EQ(handler.m_lastRecoilSharpness, 0.3f);
    }

    // Phase 2.6, Task 1: same dispatch shape as Lua_HapticPulseDispatchesThroughRequestBus above,
    // but for the new PlayHapticBuzz event (sustained buzz, shares the continuous actuator slot
    // with SetVibration on Mac -- see DualSenseHapticsMac.mm). Drives it entirely from Lua and
    // asserts the fixture handler received exactly the values script passed, including the new
    // durationSeconds parameter.
    TEST_F(ScriptRoundTripFixture, Lua_HapticBuzzDispatchesThroughRequestBus)
    {
        AzFramework::InputDeviceId::Reflect(m_behaviorContext);
        DualSense::TriggerEffect::Reflect(m_behaviorContext); // also reflects DualSense_GetGamepadDeviceId
        DualSense::ReflectDualSenseHapticPulseBus(m_behaviorContext);

        TestHapticPulseHandler handler(AzFramework::InputDeviceGamepad::IdForIndex0);

        AZ::ScriptContext sc;
        sc.BindTo(m_behaviorContext);

        EXPECT_TRUE(sc.Execute("deviceId = DualSense_GetGamepadDeviceId(0)"));
        EXPECT_TRUE(sc.Execute("DualSenseHapticPulseRequestBus.Event.PlayHapticBuzz(deviceId, 0.6, 0.3, 0.3, 1.5)"));

        ASSERT_EQ(handler.m_buzzCallCount, 1);
        EXPECT_FLOAT_EQ(handler.m_lastBuzzLeftIntensity, 0.6f);
        EXPECT_FLOAT_EQ(handler.m_lastBuzzRightIntensity, 0.3f);
        EXPECT_FLOAT_EQ(handler.m_lastBuzzSharpness, 0.3f);
        EXPECT_FLOAT_EQ(handler.m_lastBuzzDurationSeconds, 1.5f);
    }

    // Phase 2.6, Task 1: same dispatch shape, but for the new StopHaptics event (clears
    // gem-issued transient + continuous/buzz haptics on both sides; never touches trigger
    // effects). StopHaptics takes no scalar arguments beyond the implicit deviceId.
    TEST_F(ScriptRoundTripFixture, Lua_StopHapticsDispatchesThroughRequestBus)
    {
        AzFramework::InputDeviceId::Reflect(m_behaviorContext);
        DualSense::TriggerEffect::Reflect(m_behaviorContext); // also reflects DualSense_GetGamepadDeviceId
        DualSense::ReflectDualSenseHapticPulseBus(m_behaviorContext);

        TestHapticPulseHandler handler(AzFramework::InputDeviceGamepad::IdForIndex0);

        AZ::ScriptContext sc;
        sc.BindTo(m_behaviorContext);

        EXPECT_TRUE(sc.Execute("deviceId = DualSense_GetGamepadDeviceId(0)"));
        EXPECT_TRUE(sc.Execute("DualSenseHapticPulseRequestBus.Event.StopHaptics(deviceId)"));

        ASSERT_EQ(handler.m_stopCallCount, 1);
    }

    namespace
    {
        // Records whatever AzFramework::InputHapticFeedbackRequestBus (the ENGINE's own bus, not
        // a DualSense-owned one) is called with, so the DualSense_SetRumble bridge helper test
        // below can drive a dispatch entirely from Lua and assert on the C++ side what actually
        // arrived. Phase 2.6, Task 2: this engine bus is not behavior-reflected (spec §2) -- see
        // DualSense_SetRumble's comment in DualSenseTriggerEffects.h.
        class TestEngineHapticHandler : public AzFramework::InputHapticFeedbackRequestBus::Handler
        {
        public:
            explicit TestEngineHapticHandler(const AzFramework::InputDeviceId& id)
            {
                AzFramework::InputHapticFeedbackRequestBus::Handler::BusConnect(id);
            }

            ~TestEngineHapticHandler() override
            {
                AzFramework::InputHapticFeedbackRequestBus::Handler::BusDisconnect();
            }

            void SetVibration(float leftMotorSpeedNormalized, float rightMotorSpeedNormalized) override
            {
                m_lastLeft = leftMotorSpeedNormalized;
                m_lastRight = rightMotorSpeedNormalized;
                ++m_callCount;
            }

            float m_lastLeft = 0.0f;
            float m_lastRight = 0.0f;
            int m_callCount = 0;
        };

        // Mirrors TestEngineHapticHandler, but for AzFramework::InputLightBarRequestBus (backing
        // DualSense_SetLightBar).
        class TestEngineLightBarHandler : public AzFramework::InputLightBarRequestBus::Handler
        {
        public:
            explicit TestEngineLightBarHandler(const AzFramework::InputDeviceId& id)
            {
                AzFramework::InputLightBarRequestBus::Handler::BusConnect(id);
            }

            ~TestEngineLightBarHandler() override
            {
                AzFramework::InputLightBarRequestBus::Handler::BusDisconnect();
            }

            void SetLightBarColor(const AZ::Color& color) override
            {
                m_lastColor = color;
                ++m_callCount;
            }

            void ResetLightBarColor() override
            {
                ++m_resetCallCount;
            }

            AZ::Color m_lastColor = AZ::Color::CreateZero();
            int m_callCount = 0;
            int m_resetCallCount = 0;
        };
    } // namespace

    // Phase 2.6, Task 2: DualSense_SetRumble bridges AzFramework::InputHapticFeedbackRequestBus
    // (an engine bus that is not behavior-reflected -- spec §2) for scripts and the phase 2.6
    // test scene's coexistence checks. Same dispatch shape as the DualSense-owned-bus tests
    // above, but the fixture handler connects to the engine's own bus this time.
    TEST_F(ScriptRoundTripFixture, Lua_SetRumbleDispatchesThroughEngineHapticBus)
    {
        AzFramework::InputDeviceId::Reflect(m_behaviorContext);
        DualSense::TriggerEffect::Reflect(m_behaviorContext); // also reflects DualSense_SetRumble/DualSense_SetLightBar

        TestEngineHapticHandler handler(AzFramework::InputDeviceGamepad::IdForIndex0);

        AZ::ScriptContext sc;
        sc.BindTo(m_behaviorContext);

        EXPECT_TRUE(sc.Execute("DualSense_SetRumble(0, 0.7, 0.4)"));

        ASSERT_EQ(handler.m_callCount, 1);
        EXPECT_FLOAT_EQ(handler.m_lastLeft, 0.7f);
        EXPECT_FLOAT_EQ(handler.m_lastRight, 0.4f);
    }

    // Same dispatch shape, for DualSense_SetLightBar / AzFramework::InputLightBarRequestBus.
    TEST_F(ScriptRoundTripFixture, Lua_SetLightBarDispatchesThroughEngineLightBarBus)
    {
        AzFramework::InputDeviceId::Reflect(m_behaviorContext);
        DualSense::TriggerEffect::Reflect(m_behaviorContext); // also reflects DualSense_SetRumble/DualSense_SetLightBar

        TestEngineLightBarHandler handler(AzFramework::InputDeviceGamepad::IdForIndex0);

        AZ::ScriptContext sc;
        sc.BindTo(m_behaviorContext);

        EXPECT_TRUE(sc.Execute("DualSense_SetLightBar(0, 0.1, 0.2, 0.3)"));

        ASSERT_EQ(handler.m_callCount, 1);
        EXPECT_FLOAT_EQ(handler.m_lastColor.GetR(), 0.1f);
        EXPECT_FLOAT_EQ(handler.m_lastColor.GetG(), 0.2f);
        EXPECT_FLOAT_EQ(handler.m_lastColor.GetB(), 0.3f);
        EXPECT_FLOAT_EQ(handler.m_lastColor.GetA(), 1.0f);
    }

} // namespace DualSenseTests
