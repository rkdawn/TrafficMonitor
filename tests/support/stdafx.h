#pragma once
#include <windows.h>
#include <atltypes.h>

//只替换应用环境；矩形运算及窗口 API 使用真实 Windows 实现。
struct TaskbarTestSettings
{
    bool tbar_wnd_on_left{};
    bool tbar_wnd_snap{};
    bool avoid_overlap_with_widgets{};
    int taskbar_left_space_win11{160};
    int taskbar_right_space_win11{300};
    int window_offset_left{};
    int window_offset_top{};
};
struct TaskbarTestApp
{
    TaskbarTestSettings m_taskbar_data;
};
inline TaskbarTestApp theApp;
