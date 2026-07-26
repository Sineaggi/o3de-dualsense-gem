
#pragma once

#include <AzCore/Component/Component.h>
#include <AzCore/Component/TickBus.h>
#include <AzFramework/Input/Devices/Gamepad/InputDeviceGamepad.h>
#include <DualSense/DualSenseBus.h>
#include <Clients/DualSenseSystemImpl.h>

namespace DualSense
{
    class DualSenseSystemComponent
        : public AZ::Component
        , protected DualSenseRequestBus::Handler
        , public AZ::TickBus::Handler
    {
    public:
        AZ_COMPONENT_DECL(DualSenseSystemComponent);

        static void Reflect(AZ::ReflectContext* context);

        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);
        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required);
        static void GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType& dependent);

        DualSenseSystemComponent();
        ~DualSenseSystemComponent();

        //! Swap the standard gamepad slot's backend to the given factory
        //! (addressed per-slot; other slots untouched).
        static void SwapSlotToFactory(
            AZ::u32 slotIndex, AzFramework::InputDeviceGamepad::ImplementationFactory* factory);

        //! Restore the slot to the platform-default backend. NOTE: the engine
        //! ignores null factories, so if no platform factory is registered this
        //! is intentionally a no-op (never pass nullptr expecting a clear).
        static void RestoreSlotToPlatformDefault(AZ::u32 slotIndex);

    protected:
        ////////////////////////////////////////////////////////////////////////
        // DualSenseRequestBus interface implementation

        ////////////////////////////////////////////////////////////////////////

        ////////////////////////////////////////////////////////////////////////
        // AZ::Component interface implementation
        void Init() override;
        void Activate() override;
        void Deactivate() override;
        ////////////////////////////////////////////////////////////////////////

        ////////////////////////////////////////////////////////////////////////
        // AZTickBus interface implementation
        void OnTick(float deltaTime, AZ::ScriptTimePoint time) override;
        ////////////////////////////////////////////////////////////////////////

    private:
        AZStd::unique_ptr<DualSenseSystemImpl> m_impl;
    };

} // namespace DualSense
