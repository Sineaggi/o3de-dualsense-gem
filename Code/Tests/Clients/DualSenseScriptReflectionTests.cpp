#include <AzCore/UnitTest/TestTypes.h>
#include <AzCore/RTTI/BehaviorContext.h>
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

} // namespace DualSenseTests
