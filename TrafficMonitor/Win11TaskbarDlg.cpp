#include "stdafx.h"
#include "Win11TaskbarDlg.h"
#include "WindowsSettingHelper.h"

void CWin11TaskbarDlg::AdjustTaskbarWndPos(bool force_adjust)
{
    //主任务栏重建由主窗口处理，这里不单独替换父窗口句柄。
    CRect rcTaskbar;
    CPoint origin(0, 0);
    if (!::IsWindow(m_hTaskbar) || !::GetClientRect(m_hTaskbar, rcTaskbar) || rcTaskbar.IsRectEmpty()
        || !::ClientToScreen(m_hTaskbar, &origin))
        return;

    //通知区和开始按钮可能单独重建，重新查找，避免使用失效或被复用的句柄。
    InitTaskbarWnd();
    CRect rcNotify;
    bool has_notify = m_hNotify != nullptr && ::GetWindowRect(m_hNotify, rcNotify) && !rcNotify.IsRectEmpty();
    if (has_notify)
        rcNotify.OffsetRect(-origin.x, -origin.y);

    CRect rcStart;
    bool has_start = m_hStart != nullptr && ::GetWindowRect(m_hStart, rcStart) && !rcStart.IsRectEmpty();
    if (has_start)
        rcStart.OffsetRect(-origin.x, -origin.y);
    else
        rcStart = rcTaskbar;

    CRect rect(0, 0, m_window_width, m_window_height);
    bool center_align = CWindowsSettingHelper::IsTaskbarCenterAlign();
    bool on_right = !theApp.m_taskbar_data.tbar_wnd_on_left || !center_align;
    if (!on_right && theApp.m_taskbar_data.tbar_wnd_snap && !has_start)
        return;
    const int notify_gap = DPI(2);
    if (on_right)
    {
        int notify_x_pos;
        if (has_notify)
            notify_x_pos = rcNotify.left;
        else if (m_is_secondary_display)
            notify_x_pos = rcTaskbar.right - DPI(88);
        else
            notify_x_pos = rcTaskbar.right - DPI(theApp.m_taskbar_data.taskbar_right_space_win11);

        //留出间隙，不再将窗口右边缘伸入托盘箭头的点击区域。
        rect.MoveToX(notify_x_pos - rect.Width() - notify_gap);
        if (theApp.m_taskbar_data.avoid_overlap_with_widgets && !center_align && CWindowsSettingHelper::IsTaskbarWidgetsBtnShown())
            rect.OffsetRect(-DPI(theApp.m_taskbar_data.taskbar_left_space_win11), 0);
    }
    else if (theApp.m_taskbar_data.tbar_wnd_snap)
    {
        rect.MoveToX(rcStart.left - rect.Width() - 2);
    }
    else
    {
        rect.MoveToX(2);
        if (CWindowsSettingHelper::IsTaskbarWidgetsBtnShown())
            rect.OffsetRect(DPI(theApp.m_taskbar_data.taskbar_left_space_win11), 0);
    }

    rect.OffsetRect(DPI(theApp.m_taskbar_data.window_offset_left), 0);
    //保留水平偏移设置，但靠右显示时不允许正偏移将窗口推入通知区。
    if (on_right && has_notify && rect.right > rcNotify.left - notify_gap)
        rect.MoveToX(rcNotify.left - notify_gap - rect.Width());

    //触屏任务栏可能比开始按钮更高，保持原有的底部对齐方式。
    rect.MoveToY((rcStart.Height() - rect.Height()) / 2 + (rcTaskbar.Height() - rcStart.Height())
        + DPI(theApp.m_taskbar_data.window_offset_top));
    m_rect = rect;

    //嵌入失败时，基类会转换为屏幕坐标后移动，避免先按客户区坐标错误移动一次。
    if (m_connot_insert_to_task_bar)
        return;

    //比较实际位置，而非只比较托盘宽度；可修正等宽平移、小位移和外部移位。
    CRect rcActual;
    bool has_actual = ::GetWindowRect(GetSafeHwnd(), rcActual) != FALSE;
    if (has_actual)
        rcActual.OffsetRect(-origin.x, -origin.y);
    if (force_adjust || !has_actual || rcActual != rect)
        MoveWindow(rect);
}

void CWin11TaskbarDlg::InitTaskbarWnd()
{
    m_hNotify = nullptr;
    m_hStart = nullptr;
    if (!::IsWindow(m_hTaskbar))
        return;
    m_hNotify = ::FindWindowEx(m_hTaskbar, nullptr, L"TrayNotifyWnd", nullptr);
    m_hStart = ::FindWindowEx(m_hTaskbar, nullptr, L"Start", nullptr);
}

void CWin11TaskbarDlg::ResetTaskbarPos()
{
}

HWND CWin11TaskbarDlg::GetParentHwnd()
{
    return m_hTaskbar;
}

void CWin11TaskbarDlg::CheckTaskbarOnTopOrBottom()
{
    m_taskbar_on_top_or_bottom = true;
}
