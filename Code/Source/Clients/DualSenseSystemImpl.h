#pragma once

#include <AzCore/std/smart_ptr/unique_ptr.h>

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
