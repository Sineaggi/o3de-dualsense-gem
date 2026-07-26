#include <AzCore/UnitTest/TestTypes.h>
#include <Clients/DualSenseTriggerFireDetector.h>

namespace DualSenseTests
{
    using TriggerFireDetectorFixture = UnitTest::LeakDetectionFixture;
    using DualSense::WeaponTriggerStatus;
    using DualSense::IsWeaponFireEdge;

    // Rule: (*, Fired) where previous != Fired -> true. Cover every non-Fired previous state,
    // including the Unknown first-frame case (a trigger's previous-state member starts out
    // default-constructed to Unknown, and this must not be mistaken for a fire edge).
    TEST_F(TriggerFireDetectorFixture, IsWeaponFireEdge_UnknownToFired_IsEdge)
    {
        EXPECT_TRUE(IsWeaponFireEdge(WeaponTriggerStatus::Unknown, WeaponTriggerStatus::Fired));
    }

    TEST_F(TriggerFireDetectorFixture, IsWeaponFireEdge_ReadyToFired_IsEdge)
    {
        EXPECT_TRUE(IsWeaponFireEdge(WeaponTriggerStatus::Ready, WeaponTriggerStatus::Fired));
    }

    TEST_F(TriggerFireDetectorFixture, IsWeaponFireEdge_FiringToFired_IsEdge)
    {
        EXPECT_TRUE(IsWeaponFireEdge(WeaponTriggerStatus::Firing, WeaponTriggerStatus::Fired));
    }

    // Rule: (Fired, Fired) -> false. The trigger is held fired across ticks; only the
    // transition into Fired should notify, not every tick it stays there.
    TEST_F(TriggerFireDetectorFixture, IsWeaponFireEdge_FiredToFired_IsNotEdge)
    {
        EXPECT_FALSE(IsWeaponFireEdge(WeaponTriggerStatus::Fired, WeaponTriggerStatus::Fired));
    }

    // Rule: anything -> non-Fired -> false. Cover every current state that isn't Fired,
    // from every previous state, including the first-frame Unknown->Unknown case.
    TEST_F(TriggerFireDetectorFixture, IsWeaponFireEdge_UnknownToUnknown_IsNotEdge)
    {
        EXPECT_FALSE(IsWeaponFireEdge(WeaponTriggerStatus::Unknown, WeaponTriggerStatus::Unknown));
    }

    TEST_F(TriggerFireDetectorFixture, IsWeaponFireEdge_UnknownToReady_IsNotEdge)
    {
        EXPECT_FALSE(IsWeaponFireEdge(WeaponTriggerStatus::Unknown, WeaponTriggerStatus::Ready));
    }

    TEST_F(TriggerFireDetectorFixture, IsWeaponFireEdge_ReadyToReady_IsNotEdge)
    {
        EXPECT_FALSE(IsWeaponFireEdge(WeaponTriggerStatus::Ready, WeaponTriggerStatus::Ready));
    }

    TEST_F(TriggerFireDetectorFixture, IsWeaponFireEdge_ReadyToFiring_IsNotEdge)
    {
        EXPECT_FALSE(IsWeaponFireEdge(WeaponTriggerStatus::Ready, WeaponTriggerStatus::Firing));
    }

    TEST_F(TriggerFireDetectorFixture, IsWeaponFireEdge_FiringToFiring_IsNotEdge)
    {
        EXPECT_FALSE(IsWeaponFireEdge(WeaponTriggerStatus::Firing, WeaponTriggerStatus::Firing));
    }

    TEST_F(TriggerFireDetectorFixture, IsWeaponFireEdge_FiringToReady_IsNotEdge)
    {
        EXPECT_FALSE(IsWeaponFireEdge(WeaponTriggerStatus::Firing, WeaponTriggerStatus::Ready));
    }

    TEST_F(TriggerFireDetectorFixture, IsWeaponFireEdge_FiredToReady_IsNotEdge)
    {
        // The trigger reset back to Ready after firing (weapon-mode re-arm) -- not itself an
        // edge; only the next transition back into Fired is.
        EXPECT_FALSE(IsWeaponFireEdge(WeaponTriggerStatus::Fired, WeaponTriggerStatus::Ready));
    }

    TEST_F(TriggerFireDetectorFixture, IsWeaponFireEdge_FiredToUnknown_IsNotEdge)
    {
        // E.g. a dead-controller @try/@catch on .status after having just fired.
        EXPECT_FALSE(IsWeaponFireEdge(WeaponTriggerStatus::Fired, WeaponTriggerStatus::Unknown));
    }

} // namespace DualSenseTests
