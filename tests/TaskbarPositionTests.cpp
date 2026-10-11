#include "stdafx.h"
#include "Win11TaskbarDlg.h"
#include "WindowsSettingHelper.h"
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

static void Require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

static HWND CreateTestWindow(const wchar_t* class_name, HWND parent, int x, int y, int width, int height)
{
    HWND window = ::CreateWindowExW(0, class_name, L"PositionRegressionTest",
        parent ? WS_CHILD : WS_POPUP, x, y, width, height, parent, nullptr, ::GetModuleHandleW(nullptr), nullptr);
    Require(window != nullptr, "创建测试窗口失败");
    return window;
}

struct Fixture
{
    CWin11TaskbarDlg dialog;
    CTaskBarDlg& base{dialog};
    HWND parent{};
    HWND tray{};
    HWND start{};

    Fixture(int x = 0, int y = 700)
    {
        theApp.m_taskbar_data = {};
        CWindowsSettingHelper::center_align = true;
        CWindowsSettingHelper::widgets_shown = false;
        parent = CreateTestWindow(L"TaskbarPositionTestParent", nullptr, x, y, 1600, 60);
        tray = CreateTestWindow(L"TrayNotifyWnd", parent, 1300, 0, 300, 60);
        start = CreateTestWindow(L"Start", parent, 400, 0, 60, 60);
        base.m_hTaskbar = parent;
        base.m_hWnd = CreateTestWindow(L"TaskbarPositionTestReadout", parent, 0, 0, 200, 40);
        base.InitTaskbarWnd();
        Tick(true);
    }

    ~Fixture()
    {
        if (::IsWindow(parent))
            ::DestroyWindow(parent);
    }

    void Tick(bool force = false)
    {
        ::GetWindowRect(parent, base.m_rcTaskbar);
        base.AdjustTaskbarWndPos(force);
    }

    CRect Actual() const
    {
        CRect rect;
        CPoint origin(0, 0);
        Require(::GetWindowRect(base.m_hWnd, rect) != FALSE, "读取测试窗口失败");
        Require(::ClientToScreen(parent, &origin) != FALSE, "读取父窗口原点失败");
        rect.OffsetRect(-origin.x, -origin.y);
        return rect;
    }
};

int main()
{
    ::SetProcessDPIAware();
    for (const wchar_t* name : {L"TaskbarPositionTestParent", L"TaskbarPositionTestReadout", L"TrayNotifyWnd", L"Start"})
    {
        WNDCLASSW wc{};
        wc.lpfnWndProc = ::DefWindowProcW;
        wc.hInstance = ::GetModuleHandleW(nullptr);
        wc.lpszClassName = name;
        Require(::RegisterClassW(&wc) != 0, "注册测试窗口类失败");
    }

    std::vector<std::pair<std::string, std::function<void()>>> cases{
        {"right edge leaves tray gap", [] {
            Fixture f;
            Require(f.Actual().right == 1298, "右边缘应位于通知区左侧2px");
        }},
        {"measured arrow boundary does not overlap", [] {
            Fixture f;
            ::MoveWindow(f.parent, 0, 1936, 3200, 64, FALSE);
            ::MoveWindow(f.tray, 2224, 0, 976, 64, FALSE);
            ::MoveWindow(f.start, 0, 0, 80, 64, FALSE);
            f.base.m_window_width = 682;
            f.base.m_window_height = 64;
            f.Tick();
            Require(f.Actual().right == 2222 && f.Actual().right <= 2223, "应避开实测箭头左边界2223");
        }},
        {"stable layout makes no redundant moves", [] {
            Fixture f;
            int count = f.base.move_count;
            for (int i = 0; i < 100; ++i) f.Tick();
            Require(f.base.move_count == count, "位置未变时不应重复移动");
        }},
        {"equal width translation is tracked", [] {
            Fixture f;
            ::MoveWindow(f.tray, 1260, 0, 300, 60, FALSE);
            f.Tick();
            Require(f.Actual().right == 1258, "等宽移动未被跟踪");
        }},
        {"one pixel displacement is corrected", [] {
            Fixture f;
            ::MoveWindow(f.tray, 1299, 0, 300, 60, FALSE);
            f.Tick();
            Require(f.Actual().right == 1297, "1px偏移不能永久忽略");
        }},
        {"continuous displacement follows every sample", [] {
            Fixture f;
            for (int x = 1290; x >= 1230; x -= 10)
            {
                ::MoveWindow(f.tray, x, 0, 300, 60, FALSE);
                f.Tick();
                Require(f.Actual().right == x - 2, "持续变化不能停留在旧位置");
            }
        }},
        {"tray width changes cannot cover arrow", [] {
            Fixture f;
            for (int x : {1299, 1300, 1298, 1300})
            {
                ::MoveWindow(f.tray, x, 0, 1600 - x, 60, FALSE);
                f.Tick();
                Require(f.Actual().right == x - 2, "宽度变化后应保持间隙");
            }
        }},
        {"externally displaced readout recovers", [] {
            Fixture f;
            ::MoveWindow(f.base.m_hWnd, 100, 0, 200, 40, FALSE);
            f.Tick();
            Require(f.Actual().left == 1098, "外部移位后未纠正");
        }},
        {"externally resized readout recovers", [] {
            Fixture f;
            ::MoveWindow(f.base.m_hWnd, 1098, 10, 250, 40, FALSE);
            f.Tick();
            Require(f.Actual().Width() == 200 && f.Actual().right == 1298, "外部改变大小后未纠正");
        }},
        {"content width changes remain right aligned", [] {
            Fixture f;
            f.base.m_window_width = 280;
            f.base.m_rect.right = f.base.m_rect.left + 280;
            f.Tick();
            Require(f.Actual().Width() == 280 && f.Actual().right == 1298, "内容宽度变化后位置不正确");
        }},
        {"positive screen origin uses client coordinates", [] {
            Fixture f(1600, 100);
            Require(f.Actual().right == 1298, "屏幕坐标未转为客户区坐标");
        }},
        {"negative screen origin uses client coordinates", [] {
            Fixture f(-1600, -100);
            Require(f.Actual().right == 1298, "负坐标显示器定位错误");
        }},
        {"screen x zero is a valid tray boundary", [] {
            Fixture f(-1300, 700);
            theApp.m_taskbar_data.taskbar_right_space_win11 = 700;
            f.Tick(true);
            Require(f.Actual().right == 1298, "屏幕x=0被误认为没有通知区");
        }},
        {"moving parent needs no duplicate child move", [] {
            Fixture f;
            int count = f.base.move_count;
            ::MoveWindow(f.parent, 1600, 200, 1600, 60, FALSE);
            f.Tick();
            Require(f.Actual().right == 1298 && f.base.move_count == count, "父窗口平移不应重复移动子窗口");
        }},
        {"recreated tray handle is rediscovered", [] {
            Fixture f;
            ::DestroyWindow(f.tray);
            f.tray = CreateTestWindow(L"TrayNotifyWnd", f.parent, 1200, 0, 400, 60);
            f.Tick();
            Require(f.Actual().right == 1198, "没有重新发现通知区窗口");
        }},
        {"missing primary tray uses configured fallback", [] {
            Fixture f;
            ::DestroyWindow(f.tray);
            theApp.m_taskbar_data.taskbar_right_space_win11 = 400;
            f.Tick();
            Require(f.Actual().right == 1198, "主屏通知区缺失时回退错误");
        }},
        {"missing secondary tray uses clock fallback", [] {
            Fixture f;
            ::DestroyWindow(f.tray);
            f.base.m_is_secondary_display = true;
            f.Tick();
            Require(f.Actual().right == 1510, "副屏时钟留白错误");
        }},
        {"empty tray rectangle is not reused", [] {
            Fixture f;
            ::MoveWindow(f.tray, 1300, 0, 0, 60, FALSE);
            theApp.m_taskbar_data.taskbar_right_space_win11 = 400;
            f.Tick();
            Require(f.Actual().right == 1198, "零宽通知区未回退");
        }},
        {"positive offset cannot invade tray", [] {
            Fixture f;
            theApp.m_taskbar_data.window_offset_left = 50;
            f.Tick();
            Require(f.Actual().right == 1298, "水平正偏移侵入通知区");
        }},
        {"negative offset remains supported", [] {
            Fixture f;
            theApp.m_taskbar_data.window_offset_left = -20;
            f.Tick();
            Require(f.Actual().right == 1278, "水平负偏移失效");
        }},
        {"center aligned left snap remains supported", [] {
            Fixture f;
            theApp.m_taskbar_data.tbar_wnd_on_left = true;
            theApp.m_taskbar_data.tbar_wnd_snap = true;
            f.Tick();
            Require(f.Actual().right == 398, "靠开始按钮位置错误");
        }},
        {"missing start preserves left snap position", [] {
            Fixture f;
            theApp.m_taskbar_data.tbar_wnd_on_left = true;
            theApp.m_taskbar_data.tbar_wnd_snap = true;
            f.Tick();
            CRect previous = f.Actual();
            ::DestroyWindow(f.start);
            f.Tick();
            Require(f.Actual() == previous, "开始按钮缺失不应把窗口移出屏幕");
        }},
        {"recreated start is rediscovered", [] {
            Fixture f;
            theApp.m_taskbar_data.tbar_wnd_on_left = true;
            theApp.m_taskbar_data.tbar_wnd_snap = true;
            ::DestroyWindow(f.start);
            f.start = CreateTestWindow(L"Start", f.parent, 500, 0, 60, 60);
            f.Tick();
            Require(f.Actual().right == 498, "未重新发现开始按钮");
        }},
        {"left edge and widgets remain supported", [] {
            Fixture f;
            theApp.m_taskbar_data.tbar_wnd_on_left = true;
            f.Tick();
            Require(f.Actual().left == 2, "靠左显示错误");
            CWindowsSettingHelper::widgets_shown = true;
            f.Tick();
            Require(f.Actual().left == 162, "小组件留白失效");
        }},
        {"left aligned system retains widgets clearance", [] {
            Fixture f;
            theApp.m_taskbar_data.tbar_wnd_on_left = true;
            CWindowsSettingHelper::center_align = false;
            CWindowsSettingHelper::widgets_shown = true;
            theApp.m_taskbar_data.avoid_overlap_with_widgets = true;
            f.Tick();
            Require(f.Actual().right == 1138, "左对齐任务栏的小组件留白错误");
        }},
        {"taskbar height changes update vertical position", [] {
            Fixture f;
            ::MoveWindow(f.parent, 0, 700, 1600, 80, FALSE);
            ::MoveWindow(f.start, 400, 40, 60, 40, FALSE);
            f.Tick();
            Require(f.Actual().top == 40, "触屏任务栏高度变化未跟踪");
        }},
        {"vertical offset remains supported", [] {
            Fixture f;
            theApp.m_taskbar_data.window_offset_top = -3;
            f.Tick();
            Require(f.Actual().top == 7, "垂直偏移失效");
        }},
        {"detached fallback skips intermediate move", [] {
            Fixture f;
            f.base.m_connot_insert_to_task_bar = true;
            int count = f.base.move_count;
            ::MoveWindow(f.tray, 1200, 0, 400, 60, FALSE);
            f.Tick();
            Require(f.base.move_count == count && f.base.m_rect.right == 1198, "嵌入失败分支进行了中间错误移动");
        }},
        {"destroyed parent does not trigger move", [] {
            Fixture f;
            int count = f.base.move_count;
            ::DestroyWindow(f.parent);
            f.Tick();
            Require(f.base.move_count == count, "父窗口失效时仍尝试移动");
        }},
        {"explicit force remains supported", [] {
            Fixture f;
            int count = f.base.move_count;
            f.Tick(true);
            Require(f.base.move_count == count + 1, "强制调整失效");
        }}
    };
    for (int dpi : {96, 120, 144, 192})
    {
        cases.emplace_back("DPI gap " + std::to_string(dpi), [dpi] {
            Fixture f;
            f.base.dpi = dpi;
            f.Tick();
            Require(f.Actual().right == 1300 - MulDiv(2, dpi, 96), "留白未随DPI缩放");
        });
    }
    int failed = 0;
    for (const auto& test : cases)
    {
        try { test.second(); std::cout << "PASS " << test.first << '\n'; }
        catch (const std::exception& e) { ++failed; std::cout << "FAIL " << test.first << ": " << e.what() << '\n'; }
    }
    std::cout << "RESULT " << cases.size() - failed << '/' << cases.size() << " passed\n";
    return failed == 0 ? 0 : 1;
}
