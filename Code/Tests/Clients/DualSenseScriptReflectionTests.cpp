#include <AzCore/UnitTest/TestTypes.h>
#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Component/ComponentApplicationBus.h>
#include <AzCore/Interface/Interface.h>
#include <AzCore/Script/ScriptContext.h>
#include <AzFramework/Input/Devices/InputDeviceId.h>
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

} // namespace DualSenseTests
