#include <AzCore/UnitTest/TestTypes.h>
#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Component/ComponentApplicationBus.h>
#include <AzCore/Interface/Interface.h>
#include <AzCore/Script/ScriptContext.h>
#include <AzFramework/Input/Devices/InputDeviceId.h>
#include <AzFramework/Input/Devices/Gamepad/InputDeviceGamepad.h>
#include <DualSense/DualSenseTriggerEffects.h>

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

} // namespace DualSenseTests
