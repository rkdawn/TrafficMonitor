#pragma once
class CWindowsSettingHelper
{
public:
    inline static bool center_align{true};
    inline static bool widgets_shown{};
    static bool IsTaskbarCenterAlign() { return center_align; }
    static bool IsTaskbarWidgetsBtnShown() { return widgets_shown; }
};
