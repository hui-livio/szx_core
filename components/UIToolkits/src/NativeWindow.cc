/**
 * @file NativeWindow.cc
 * @brief Windows 原生窗口辅助实现 — 处理 NCCALCSIZE 和 DWM 窗口属性。
 *
 * @author livliu(yuanfeng1897@outlook.com)
 * @date 2026-07-01
 */

#include "NativeWindow.h"
#include <QAbstractNativeEventFilter>
#include <QDebug>
#include <QGuiApplication>
#include <QSize>
#include <QString>
#include <QTimer>
#include <cstdlib>
#include <unordered_map>

#ifdef Q_OS_WIN
    #include <dwmapi.h>
    #include <windows.h>
    #include <windowsx.h>

    #pragma comment(lib, "dwmapi.lib")
#endif

#ifdef Q_OS_WIN
namespace
{

struct NativeWindowState
{
    QSize minimumSize;
    HMONITOR lastMonitor = nullptr;
};

std::unordered_map<HWND, NativeWindowState> g_nativeWindows;

QString nativeRectText(const RECT &rect)
{
    return QStringLiteral("%1,%2 %3x%4").arg(rect.left).arg(rect.top).arg(rect.right - rect.left).arg(rect.bottom - rect.top);
}

QString nativeHandleText(const void *handle)
{
    return QStringLiteral("0x%1").arg(reinterpret_cast<quintptr>(handle), 0, 16);
}

void logNativeWindowDiagnostic(const char *event, HWND hwnd, HMONITOR monitor, const RECT *suggestedRect = nullptr)
{
    RECT windowRect          = {};
    const bool hasWindowRect = GetWindowRect(hwnd, &windowRect) != FALSE;

    MONITORINFO monitorInfo   = {};
    monitorInfo.cbSize        = sizeof(monitorInfo);
    const bool hasMonitorInfo = monitor && GetMonitorInfo(monitor, &monitorInfo) != FALSE;

    auto debug                       = qInfo().noquote().nospace();
    const DPI_AWARENESS dpiAwareness = GetAwarenessFromDpiAwarenessContext(GetThreadDpiAwarenessContext());
    debug << "WINDOW_DIAGNOSTIC event=" << event << " hwnd=" << nativeHandleText(hwnd) << " monitor=" << nativeHandleText(monitor)
          << " dpi=" << GetDpiForWindow(hwnd) << " dpi_awareness=" << static_cast<int>(dpiAwareness)
          << " zoomed=" << (IsZoomed(hwnd) != FALSE) << " iconic=" << (IsIconic(hwnd) != FALSE)
          << " window_rect_ok=" << hasWindowRect << " monitor_info_ok=" << hasMonitorInfo;
    if (hasWindowRect)
        debug << " window_rect=" << nativeRectText(windowRect);
    if (hasMonitorInfo)
        debug << " monitor_rect=" << nativeRectText(monitorInfo.rcMonitor) << " work_rect=" << nativeRectText(monitorInfo.rcWork);
    if (suggestedRect)
        debug << " suggested_rect=" << nativeRectText(*suggestedRect);
}

HMONITOR currentMonitor(HWND hwnd, NativeWindowState &state)
{
    // A minimized HWND can temporarily live at the legacy (-32000, -32000)
    // coordinates. MonitorFromWindow() would then select the primary monitor
    // instead of the monitor from which the window was minimized.
    if (IsIconic(hwnd) && state.lastMonitor)
        return state.lastMonitor;

    HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONULL);
    if (monitor)
    {
        state.lastMonitor = monitor;
        return monitor;
    }

    if (state.lastMonitor)
        return state.lastMonitor;

    return MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
}

LONG scaleForWindow(HWND hwnd, int logicalPixels)
{
    const UINT dpi = GetDpiForWindow(hwnd);
    return MulDiv(logicalPixels, dpi > 0 ? static_cast<int>(dpi) : USER_DEFAULT_SCREEN_DPI, USER_DEFAULT_SCREEN_DPI);
}

bool isWindows11OrGreater()
{
    static const bool cached = []
    {
        OSVERSIONINFOEXW osvi    = {};
        osvi.dwOSVersionInfoSize = sizeof(osvi);
        osvi.dwBuildNumber       = 22000;
        DWORDLONG conditionMask  = VerSetConditionMask(0, VER_BUILDNUMBER, VER_GREATER_EQUAL);
        return VerifyVersionInfoW(&osvi, VER_BUILDNUMBER, conditionMask) != FALSE;
    }();
    return cached;
}

constexpr LONG kMinValidCoordinate = -30000;
constexpr LONG kMinWindowSizePx    = 100;

bool isValidMaximizeRect(const RECT &rect)
{
    return rect.left >= kMinValidCoordinate && rect.top >= kMinValidCoordinate && rect.right > rect.left &&
        rect.bottom > rect.top && (rect.right - rect.left) >= kMinWindowSizePx && (rect.bottom - rect.top) >= kMinWindowSizePx;
}

bool clientAreaFillsMonitorWorkArea(HWND hwnd)
{
    RECT clientRect{};
    if (!GetClientRect(hwnd, &clientRect))
        return false;

    POINT topLeft{clientRect.left, clientRect.top};
    POINT bottomRight{clientRect.right, clientRect.bottom};
    if (!ClientToScreen(hwnd, &topLeft) || !ClientToScreen(hwnd, &bottomRight))
        return false;

    clientRect       = RECT{topLeft.x, topLeft.y, bottomRight.x, bottomRight.y};
    HMONITOR monitor = MonitorFromRect(&clientRect, MONITOR_DEFAULTTONEAREST);
    MONITORINFO monitorInfo{};
    monitorInfo.cbSize = sizeof(monitorInfo);
    if (!monitor || !GetMonitorInfo(monitor, &monitorInfo))
        return false;

    const LONG tolerance = scaleForWindow(hwnd, 2);
    const RECT &workArea = monitorInfo.rcWork;
    return std::abs(clientRect.left - workArea.left) <= tolerance && std::abs(clientRect.top - workArea.top) <= tolerance &&
        std::abs(clientRect.right - workArea.right) <= tolerance && std::abs(clientRect.bottom - workArea.bottom) <= tolerance;
}

} // namespace
#endif

// ── NCCALCSIZE filter: remove standard frame → DWM still renders shadow ──
class FramelessFilter : public QAbstractNativeEventFilter
{
public:
    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override
    {
#ifdef Q_OS_WIN
        if (eventType != "windows_generic_MSG")
            return false;
        MSG *msg = static_cast<MSG *>(message);

        auto stateIt = g_nativeWindows.find(msg->hwnd);
        if (stateIt == g_nativeWindows.end())
            return false;

        NativeWindowState &state = stateIt->second;

        if (msg->message == WM_NCDESTROY)
        {
            g_nativeWindows.erase(stateIt);
            return false;
        }

        if ((msg->message == WM_MOVE || msg->message == WM_WINDOWPOSCHANGED || msg->message == WM_DPICHANGED) &&
            !IsIconic(msg->hwnd))
        {
            HMONITOR previousMonitor = state.lastMonitor;
            currentMonitor(msg->hwnd, state);
            if (msg->message == WM_DPICHANGED)
            {
                const auto *suggestedRect = reinterpret_cast<const RECT *>(msg->lParam);
                HMONITOR suggestedMonitor = MonitorFromRect(suggestedRect, MONITOR_DEFAULTTONEAREST);
                logNativeWindowDiagnostic("dpi_changed", msg->hwnd, suggestedMonitor, suggestedRect);
            }
            else if (state.lastMonitor != previousMonitor)
            {
                logNativeWindowDiagnostic("monitor_changed", msg->hwnd, state.lastMonitor);
            }
        }

        // Remove standard frame — entire window is client area
        // DWM still renders shadow + rounded corners around it
        if (msg->message == WM_NCCALCSIZE)
        {
            if (msg->wParam && IsZoomed(msg->hwnd) && !IsIconic(msg->hwnd))
            {
                auto *params            = reinterpret_cast<NCCALCSIZE_PARAMS *>(msg->lParam);
                const RECT proposedRect = params->rgrc[0];
                HMONITOR monitor        = MonitorFromRect(&params->rgrc[0], MONITOR_DEFAULTTONULL);
                if (!monitor)
                    monitor = state.lastMonitor;

                const bool hasInvalidCoordinate = !isValidMaximizeRect(proposedRect);
                const bool needWin11Fix         = isWindows11OrGreater() && hasInvalidCoordinate;

                MONITORINFO monitorInfo{};
                monitorInfo.cbSize = sizeof(monitorInfo);
                if (monitor && GetMonitorInfo(monitor, &monitorInfo))
                {
                    params->rgrc[0]   = monitorInfo.rcWork;
                    state.lastMonitor = monitor;
    #ifdef QT_DEBUG
                    if (needWin11Fix)
                    {
                        qInfo().noquote().nospace()
                            << "WINDOW_DIAGNOSTIC event=win11_snap_fix hwnd=" << nativeHandleText(msg->hwnd)
                            << " proposed_rect=" << nativeRectText(proposedRect)
                            << " corrected_rect=" << nativeRectText(monitorInfo.rcWork);
                    }
    #endif
                }
                logNativeWindowDiagnostic("maximize_client_area", msg->hwnd, monitor, &proposedRect);
            }
            *result = 0;
            return true;
        }

        // Fix maximized window + respect QML minimum size
        if (msg->message == WM_GETMINMAXINFO)
        {
            MINMAXINFO *mmi = reinterpret_cast<MINMAXINFO *>(msg->lParam);
            HMONITOR mon    = currentMonitor(msg->hwnd, state);
            MONITORINFO mi{};
            mi.cbSize = sizeof(mi);
            if (mon && GetMonitorInfo(mon, &mi))
            {
                mmi->ptMaxPosition.x = mi.rcWork.left - mi.rcMonitor.left;
                mmi->ptMaxPosition.y = mi.rcWork.top - mi.rcMonitor.top;
                mmi->ptMaxSize.x     = mi.rcWork.right - mi.rcWork.left;
                mmi->ptMaxSize.y     = mi.rcWork.bottom - mi.rcWork.top;
            }
            mmi->ptMinTrackSize.x = scaleForWindow(msg->hwnd, state.minimumSize.width());
            mmi->ptMinTrackSize.y = scaleForWindow(msg->hwnd, state.minimumSize.height());
            if (IsZoomed(msg->hwnd) || IsIconic(msg->hwnd))
                logNativeWindowDiagnostic("minmax_constraints", msg->hwnd, mon);
            *result = 0;
            return true;
        }

        // Qt can report a frameless window on a secondary monitor as windowed.
        // Block QML x/y changes at the native boundary while the client area
        // still fills the monitor work area. WINDOWPOS may contain size fields
        // even for a position-only QWindow update, so only SWP_NOMOVE is added.
        if (msg->message == WM_WINDOWPOSCHANGING && (GetKeyState(VK_LBUTTON) & 0x8000) != 0 && !IsIconic(msg->hwnd) &&
            clientAreaFillsMonitorWorkArea(msg->hwnd))
        {
            auto *windowPos   = reinterpret_cast<WINDOWPOS *>(msg->lParam);
            windowPos->flags |= SWP_NOMOVE;
        }

        // Native resize via border hit-test
        if (msg->message == WM_NCHITTEST)
        {
            if (IsZoomed(msg->hwnd) || IsIconic(msg->hwnd) || clientAreaFillsMonitorWorkArea(msg->hwnd))
                if (IsZoomed(msg->hwnd) || IsIconic(msg->hwnd) || clientAreaFillsMonitorWorkArea(msg->hwnd))
                {
                    *result = HTCLIENT;
                    return true;
                }

            const LONG border = scaleForWindow(msg->hwnd, 8);
            POINT pt{GET_X_LPARAM(msg->lParam), GET_Y_LPARAM(msg->lParam)};
            RECT rc;
            GetWindowRect(msg->hwnd, &rc);

            bool left   = pt.x < rc.left + border;
            bool right  = pt.x >= rc.right - border;
            bool top    = pt.y < rc.top + border;
            bool bottom = pt.y >= rc.bottom - border;

            if (top && left)
                *result = HTTOPLEFT;
            else if (top && right)
                *result = HTTOPRIGHT;
            else if (bottom && left)
                *result = HTBOTTOMLEFT;
            else if (bottom && right)
                *result = HTBOTTOMRIGHT;
            else if (left)
                *result = HTLEFT;
            else if (right)
                *result = HTRIGHT;
            else if (top)
                *result = HTTOP;
            else if (bottom)
                *result = HTBOTTOM;
            else
                *result = HTCLIENT;
            return true;
        }
#endif
        return false;
    }
};

void installNativeWindowFilter()
{
    // 静态局部变量：生命周期等于程序运行期，避免内存泄漏
    static FramelessFilter s_filter;
    if (auto *application = QGuiApplication::instance())
        application->installNativeEventFilter(&s_filter);
}

void setupNativeWindow(QWindow *win)
{
#ifdef Q_OS_WIN
    if (!win)
        return;
    HWND hwnd = reinterpret_cast<HWND>(win->winId());

    // Hide an already-created native window before changing its frame style.
    // This prevents DWM from presenting the standard caption for one frame.
    ShowWindow(hwnd, SW_HIDE);

    // Keep DWM from presenting the standard frame while the existing HWND is
    // converted to the custom frame. The QML window is still hidden here.
    BOOL enabled = TRUE;
    DwmSetWindowAttribute(hwnd, DWMWA_CLOAK, &enabled, sizeof(enabled));
    DwmSetWindowAttribute(hwnd, DWMWA_TRANSITIONS_FORCEDISABLED, &enabled, sizeof(enabled));

    // Register only windows explicitly configured through this component.
    // Other Qt top-level windows and popups must retain their native handling.
    g_nativeWindows[hwnd] = NativeWindowState{win->minimumSize(), MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST)};
    logNativeWindowDiagnostic("native_window_setup", hwnd, g_nativeWindows[hwnd].lastMonitor);

    // Keep the resize frame for native resizing and DWM shadows, but remove
    // WS_CAPTION. If the caption style remains, DWM can recreate a thin native
    // title bar after a frame recalculation on a secondary monitor.
    LONG_PTR style  = GetWindowLongPtr(hwnd, GWL_STYLE);
    style          &= ~static_cast<LONG_PTR>(WS_CAPTION);
    style          |= WS_THICKFRAME;
    SetWindowLongPtr(hwnd, GWL_STYLE, style);

    // Extend DWM frame into client area → native shadow + rounded corners
    MARGINS margins{1, 1, 1, 1};
    DwmExtendFrameIntoClientArea(hwnd, &margins);

    // Windows 11 dark immersive title bar
    BOOL dark = TRUE;
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));

    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);

    // Keep the cloak until the caller has completed QWindow::show().
    // revealNativeWindow() removes it on the next event-loop turn, preventing
    // DWM from painting a transient native caption during frame setup.
    DwmFlush();
#endif
}

void revealNativeWindow(QWindow *win)
{
#ifdef Q_OS_WIN
    if (!win)
        return;
    const HWND hwnd = reinterpret_cast<HWND>(win->winId());
    // The caller owns the Qt window lifecycle (show/raise/activate).  This
    // helper only removes the temporary DWM cloak after native frame setup;
    // calling QWindow::show() here races the caller and can leave the window
    // minimized or invisible while its taskbar thumbnail remains alive.
    QTimer::singleShot(0, win,
                       [hwnd]()
                       {
                           if (!IsWindow(hwnd))
                               return;
                           BOOL disabled = FALSE;
                           DwmSetWindowAttribute(hwnd, DWMWA_CLOAK, &disabled, sizeof(disabled));
                           DwmSetWindowAttribute(hwnd, DWMWA_TRANSITIONS_FORCEDISABLED, &disabled, sizeof(disabled));
                           DwmFlush();
                       });
#else
    Q_UNUSED(win)
#endif
}
