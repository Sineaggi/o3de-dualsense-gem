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
