#pragma once
#include "stdafx.h"

//测试替身只提供基类接口，不复制被测定位算法。
class CTaskBarDlg
{
public:
    virtual ~CTaskBarDlg() = default;
    virtual void InitTaskbarWnd() = 0;
    virtual void AdjustTaskbarWndPos(bool force_adjust) = 0;
    virtual void ResetTaskbarPos() = 0;
    virtual HWND GetParentHwnd() = 0;
    virtual void CheckTaskbarOnTopOrBottom() = 0;

    HWND m_hTaskbar{};
    HWND m_hWnd{};
    CRect m_rcTaskbar;
    CRect m_rect;
    int m_window_width{200};
    int m_window_height{40};
    bool m_is_secondary_display{};
    bool m_connot_insert_to_task_bar{};
    bool m_taskbar_on_top_or_bottom{};
    int dpi{96};
    int move_count{};

    HWND GetSafeHwnd() const { return m_hWnd; }
    int DPI(int value) const { return MulDiv(value, dpi, 96); }
    void MoveWindow(CRect rect)
    {
        ++move_count;
        ::MoveWindow(m_hWnd, rect.left, rect.top, rect.Width(), rect.Height(), FALSE);
    }
};
