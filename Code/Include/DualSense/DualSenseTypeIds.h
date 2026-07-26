
#pragma once

namespace DualSense
{
    // System Component TypeIds
    inline constexpr const char* DualSenseSystemComponentTypeId = "{792FCEA6-2615-4800-B58D-79DED5ED0F6B}";
    inline constexpr const char* DualSenseEditorSystemComponentTypeId = "{C5F48623-3389-4A25-ABCF-42A150A82B3F}";

    // Module derived classes TypeIds
    inline constexpr const char* DualSenseModuleInterfaceTypeId = "{428C9809-C1EA-40D0-BD70-59368F5993F3}";
    inline constexpr const char* DualSenseModuleTypeId = "{1CA85F97-CFFE-4C26-AC7B-36825988A569}";
    // The Editor Module by default is mutually exclusive with the Client Module
    // so they use the Same TypeId
    inline constexpr const char* DualSenseEditorModuleTypeId = DualSenseModuleTypeId;

    // Interface TypeIds
    inline constexpr const char* DualSenseRequestsTypeId = "{62523F80-B843-4F76-AF4C-1BAE3E4BA2BB}";

    // Trigger Effects TypeIds
    inline constexpr const char* DualSenseTriggerEffectTypeId = "{173E97C6-1364-44E4-A642-498F9652AB62}";
} // namespace DualSense
