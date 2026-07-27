#include <Clients/DualSenseBackendSelection.h>

#include <AzCore/Console/IConsole.h>
#include <AzCore/Interface/Interface.h>
#include <AzCore/std/string/string.h>
#include <AzCore/std/string/conversions.h>

namespace DualSense
{
    BackendSelection ParseBackendSelection(AZStd::string_view value)
    {
        AZStd::string lower(value);
        AZStd::to_lower(lower.begin(), lower.end());
        return (lower == "sdl") ? BackendSelection::Sdl : BackendSelection::Native;
    }

    BackendSelection GetDualSenseBackendSelection()
    {
        AZ::CVarFixedString value = "native";
        if (auto* console = AZ::Interface<AZ::IConsole>::Get())
        {
            console->GetCvarValue("dualsense_backend", value);
        }
        return ParseBackendSelection(AZStd::string_view(value.c_str(), value.size()));
    }
} // namespace DualSense
