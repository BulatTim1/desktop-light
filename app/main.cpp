// SPDX-License-Identifier: GPL-3.0-only
#include "../src/brightness-engine.hpp"
#include "../src/consumer-input.hpp"
#include "preferences.hpp"
#include "startup.hpp"
#include "integration.hpp"
#include "../src/native-popup-state.hpp"
#include "../src/standalone-bridge.hpp"
#include <commctrl.h>
#include <shellapi.h>
#include <dwmapi.h>
#include <wtsapi32.h>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace {
constexpr wchar_t kClass[] = L"CursorBrightness.Standalone.Window";
constexpr UINT kChanged = WM_APP + 1, kTray = WM_APP + 2, kShow = WM_APP + 3, kQuit = WM_APP + 4;
constexpr UINT kKeysTimer = 1, kRefreshTimer = 2, kRescanTimer = 3, kIntegrationTimer = 5;
constexpr int kCombo = 100, kSlider = 101, kDecrease = 102, kIncrease = 103, kOptions = 104;
constexpr int kMenuShow = 200, kMenuRescan = 201, kMenuKeys = 202, kMenuStartup = 203, kMenuFolder = 204, kMenuAbout = 205, kMenuExit = 206, kStepBase = 300;
constexpr int kMenuIntegration = 207;
constexpr wchar_t kVersion[] = L"1.2.1";
brightness::Engine engine;
cursor_brightness::ConsumerInput input;
app::Preferences preferences;
std::unique_ptr<app::integration::Runtime> integration;
std::unique_ptr<cursor_brightness::native_popup::Publisher> popupPublisher;
std::wstring popupId;
ULONGLONG lastPopupKey=0;
HWND window = nullptr, combo = nullptr, slider = nullptr, title = nullptr, modeLabel = nullptr, valueLabel = nullptr, hint = nullptr;
HFONT normalFont = nullptr, titleFont = nullptr, valueFont = nullptr;
HICON icon = nullptr;
std::thread starter;
std::atomic<bool> ready{false};
std::vector<brightness::Display> displays;
std::wstring selectedId;
POINT panelPoint{};
bool resolved = false, updating = false, trayPresent = false;
std::atomic<bool> exiting{false};
UINT taskbarCreated = 0;
unsigned integrationRetries = 0;

void Error(const char* text) {
    const int length = MultiByteToWideChar(CP_UTF8, 0, text, -1, nullptr, 0);
    std::wstring wide(length > 0 ? length : 1, L'\0');
    if(length > 0) MultiByteToWideChar(CP_UTF8, 0, text, -1, wide.data(), length);
    MessageBoxW(window, wide.c_str(), L"Cursor Brightness", MB_OK | MB_ICONERROR);
}
HICON SunIcon() {
    BITMAPV5HEADER header{}; header.bV5Size = sizeof(header); header.bV5Width = 32; header.bV5Height = -32;
    header.bV5Planes = 1; header.bV5BitCount = 32; header.bV5Compression = BI_BITFIELDS;
    header.bV5RedMask = 0x00ff0000; header.bV5GreenMask = 0x0000ff00; header.bV5BlueMask = 0x000000ff; header.bV5AlphaMask = 0xff000000;
    void* bits = nullptr; HDC dc = GetDC(nullptr);
    HBITMAP bitmap = CreateDIBSection(dc, reinterpret_cast<BITMAPINFO*>(&header), DIB_RGB_COLORS, &bits, nullptr, 0);
    ReleaseDC(nullptr, dc);
    if(!bitmap) return CopyIcon(LoadIconW(nullptr, IDI_APPLICATION));
    auto* pixels = static_cast<DWORD*>(bits);
    for(int y=0; y<32; ++y) for(int x=0; x<32; ++x) {
        const double dx=x-15.5, dy=y-15.5, radius=std::sqrt(dx*dx+dy*dy);
        const double angle=std::atan2(dy,dx), ray=std::abs(std::sin(angle*4));
        pixels[y*32+x] = radius <= 6.5 || (radius>=10 && radius<=14 && ray<0.22) ? 0xffffb340 : 0;
    }
    HBITMAP mask = CreateBitmap(32,32,1,1,nullptr);
    ICONINFO information{TRUE,0,0,mask,bitmap};
    HICON result = CreateIconIndirect(&information);
    DeleteObject(mask); DeleteObject(bitmap); return result;
}
void Tray(bool remove = false) {
    NOTIFYICONDATAW data{}; data.cbSize = sizeof(data); data.hWnd = window; data.uID = 1;
    data.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP; data.uCallbackMessage = kTray; data.hIcon = icon;
    wcscpy_s(data.szTip, L"Cursor Brightness — hardware brightness keys follow your cursor");
    if(remove) { Shell_NotifyIconW(NIM_DELETE, &data); trayPresent = false; return; }
    if(Shell_NotifyIconW(trayPresent ? NIM_MODIFY : NIM_ADD, &data)) trayPresent = true;
    data.uVersion = NOTIFYICON_VERSION_4; Shell_NotifyIconW(NIM_SETVERSION, &data);
}
const brightness::Display* Selected() {
    for(auto const& display : displays) if(display.stableId == selectedId) return &display;
    return nullptr;
}
const brightness::Display* At(POINT point) {
    for(auto const& display : displays) if(cursor_brightness::Contains({display.rect.left,display.rect.top,display.rect.right,display.rect.bottom},point.x,point.y)) return &display;
    return nullptr;
}
std::wstring Mode(const brightness::Display& display) {
    switch(display.transport) {
    case brightness::Transport::HdrSdr: return L"SDR content brightness · HDR stays enabled";
    case brightness::Transport::DdcCi: return L"Hardware brightness · DDC/CI";
    case brightness::Transport::Wmi: return L"Built-in display brightness";
    default: return L"Brightness control unavailable";
    }
}
void RefreshUi() {
    displays = engine.GetDisplays();
    if(!resolved && !displays.empty()) { if(auto* display=At(panelPoint)) selectedId=display->stableId; resolved=true; }
    updating = true;
    SendMessageW(combo, CB_RESETCONTENT, 0,0);
    int selected=-1;
    for(size_t i=0; i<displays.size(); ++i) {
        SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(displays[i].name.c_str()));
        if(displays[i].stableId==selectedId) selected=static_cast<int>(i);
    }
    SendMessageW(combo, CB_SETCURSEL, selected, 0);
    auto* display=Selected();
    const bool available=display && display->transport != brightness::Transport::None && display->percent>=0;
    SetWindowTextW(title, display ? display->name.c_str() : displays.empty() ? L"Finding your displays…" : L"Display disconnected");
    SetWindowTextW(modeLabel, display ? Mode(*display).c_str() : L"Choose a connected display above.");
    SetWindowTextW(valueLabel, available ? (std::to_wstring(display->percent)+L"%").c_str() : L"—");
    EnableWindow(slider,available); EnableWindow(GetDlgItem(window,kDecrease),available); EnableWindow(GetDlgItem(window,kIncrease),available);
    if(available && !(GetCapture()==slider)) SendMessageW(slider,TBM_SETPOS,TRUE,display->percent);
    updating=false;
}
void ShowPanel() {
    GetCursorPos(&panelPoint); resolved=false; selectedId.clear();
    RefreshUi(); engine.RequestRefresh();
    MONITORINFO monitor{}; monitor.cbSize=sizeof(monitor);
    GetMonitorInfoW(MonitorFromPoint(panelPoint,MONITOR_DEFAULTTONEAREST), &monitor);
    RECT bounds{}; GetWindowRect(window,&bounds);
    const int width=bounds.right-bounds.left, height=bounds.bottom-bounds.top;
    const int x=std::clamp<int>(panelPoint.x-width/2, monitor.rcWork.left, std::max<int>(monitor.rcWork.left,monitor.rcWork.right-width));
    const int y=std::clamp<int>(panelPoint.y-height/2, monitor.rcWork.top, std::max<int>(monitor.rcWork.top,monitor.rcWork.bottom-height));
    SetWindowPos(window,nullptr,x,y,0,0,SWP_NOSIZE|SWP_NOZORDER);
    ShowWindow(window,SW_SHOWNORMAL); SetForegroundWindow(window);
}
void PublishPopup() {
    if(!popupPublisher) return;
    if(!ready.load() || !preferences.keys) { popupPublisher->Publish(-1,false); return; }
    const auto snapshot=engine.GetDisplays();
    const auto now=GetTickCount64();
    const brightness::Display* target=nullptr;
    if(lastPopupKey && now-lastPopupKey<1700) {
        for(const auto& display : snapshot) if(display.stableId==popupId) { target=&display; break; }
    } else {
        POINT point{};
        if(GetCursorPos(&point)) for(const auto& display : snapshot)
            if(cursor_brightness::Contains({display.rect.left,display.rect.top,display.rect.right,display.rect.bottom},point.x,point.y)) { target=&display; break; }
    }
    popupPublisher->Publish(target ? target->percent : -1,target && target->transport!=brightness::Transport::None && target->percent>=0);
}
void ConfigureKeys() {
    input.Stop();
    if(preferences.keys && !input.Register(window)) throw std::runtime_error("Cannot register hardware brightness controls. Another component may already own this input collection.");
}
void Menu(POINT point) {
    HMENU menu=CreatePopupMenu(), steps=CreatePopupMenu();
    AppendMenuW(menu,MF_STRING,kMenuShow,L"Open brightness");
    AppendMenuW(menu,MF_STRING,kMenuRescan,L"Refresh displays");
    AppendMenuW(menu,MF_SEPARATOR,0,nullptr);
    AppendMenuW(menu,MF_STRING|(preferences.keys?MF_CHECKED:0),kMenuKeys,L"Hardware brightness keys");
    bool starts=false;
    try { starts=app::startup::Enabled(); } catch(...) {}
    AppendMenuW(menu,MF_STRING|(starts?MF_CHECKED:0),kMenuStartup,L"Start with Windows");
    AppendMenuW(menu,MF_STRING|(integration && integration->Running()?MF_CHECKED:0)|(integration && integration->Available()?0:MF_GRAYED),kMenuIntegration,L"Windows Quick Settings (Win+A)");
    for(int step : {1,2,5,10,20}) AppendMenuW(steps,MF_STRING|(step==preferences.step?MF_CHECKED:0),kStepBase+step,(std::to_wstring(step)+L"%").c_str());
    AppendMenuW(menu,MF_POPUP,reinterpret_cast<UINT_PTR>(steps),L"Brightness step");
    AppendMenuW(menu,MF_SEPARATOR,0,nullptr);
    AppendMenuW(menu,MF_STRING,kMenuFolder,L"Open settings folder");
    AppendMenuW(menu,MF_STRING,kMenuAbout,L"About");
    AppendMenuW(menu,MF_STRING,kMenuExit,L"Quit");
    SetForegroundWindow(window);
    const UINT selected=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_RIGHTBUTTON,point.x,point.y,0,window,nullptr);
    DestroyMenu(menu); PostMessageW(window,WM_NULL,0,0);
    try {
        switch(selected) {
        case kMenuShow: ShowPanel(); break;
        case kMenuRescan: engine.RequestRescan(); break;
        case kMenuKeys: preferences.keys=!preferences.keys; ConfigureKeys(); preferences.Save(); PublishPopup(); break;
        case kMenuStartup: app::startup::Set(!starts); break;
        case kMenuIntegration:
            if(!integration) break;
            if(integration->Running()) { integration->Stop(); preferences.integration=false; }
            else { integration->Start(); preferences.integration=true; integrationRetries=0; }
            preferences.Save(); break;
        case kMenuFolder: ShellExecuteW(window,L"open",app::DataDirectory().c_str(),nullptr,nullptr,SW_SHOWNORMAL); break;
        case kMenuAbout: MessageBoxW(window,L"Cursor Brightness 1.2.1\n\nHardware brightness keys follow your cursor.\nSDR: DDC/CI · HDR: Windows SDR-content brightness\n\nGPL-3.0-only. Based on bardelyne's brightness engine.\nThe complete package bundles Windhawk for Win+A integration.\nThe native tray controls also work independently.",L"About Cursor Brightness",MB_OK|MB_ICONINFORMATION); break;
        case kMenuExit: PostMessageW(window,kQuit,0,0); break;
        default: if(selected>kStepBase && selected<=kStepBase+100) { preferences.step=selected-kStepBase; preferences.Save(); } break;
        }
    } catch(const std::exception& error) { Error(error.what()); }
}
void ChangeSelected(int direction) {
    displays=engine.GetDisplays();
    if(auto* display=Selected(); display && display->percent>=0 && display->transport!=brightness::Transport::None) engine.SetPercent(display->stableId,cursor_brightness::Adjust(display->percent,direction,preferences.step));
}
HWND Child(const wchar_t* klass,const wchar_t* text,DWORD style,int id) {
    HWND child=CreateWindowExW(0,klass,text,WS_CHILD|WS_VISIBLE|style,0,0,0,0,window,reinterpret_cast<HMENU>(INT_PTR(id)),GetModuleHandleW(nullptr),nullptr);
    SendMessageW(child,WM_SETFONT,reinterpret_cast<WPARAM>(normalFont),TRUE); return child;
}
void UpdateFonts(int dpi) {
    const auto previousNormal=normalFont, previousTitle=titleFont, previousValue=valueFont;
    auto make=[dpi](int size,int weight) { return CreateFontW(-MulDiv(size,dpi,72),0,0,0,weight,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI"); };
    normalFont=make(10,FW_NORMAL); titleFont=make(17,FW_SEMIBOLD); valueFont=make(28,FW_SEMIBOLD);
    for(HWND child : {combo,modeLabel,hint,GetDlgItem(window,kDecrease),GetDlgItem(window,kIncrease),GetDlgItem(window,kOptions)})
        if(child) SendMessageW(child,WM_SETFONT,reinterpret_cast<WPARAM>(normalFont),TRUE);
    if(title) SendMessageW(title,WM_SETFONT,reinterpret_cast<WPARAM>(titleFont),TRUE);
    if(valueLabel) SendMessageW(valueLabel,WM_SETFONT,reinterpret_cast<WPARAM>(valueFont),TRUE);
    for(auto font : {previousNormal,previousTitle,previousValue}) if(font) DeleteObject(font);
}
void Layout() {
    RECT rect{}; GetClientRect(window,&rect);
    const int dpi=GetDpiForWindow(window);
    const auto scale=[dpi](int value){return MulDiv(value,dpi,96);};
    const int w=rect.right, inset=scale(24);
    MoveWindow(combo,inset,scale(18),w-inset*2,scale(300),TRUE);
    MoveWindow(title,inset,scale(60),w-inset*2,scale(32),TRUE);
    MoveWindow(modeLabel,inset,scale(96),w-inset*2,scale(25),TRUE);
    MoveWindow(valueLabel,inset,scale(126),w-inset*2,scale(55),TRUE);
    MoveWindow(GetDlgItem(window,kDecrease),inset,scale(190),scale(42),scale(32),TRUE);
    MoveWindow(slider,inset+scale(48),scale(188),w-inset*2-scale(96),scale(38),TRUE);
    MoveWindow(GetDlgItem(window,kIncrease),w-inset-scale(42),scale(190),scale(42),scale(32),TRUE);
    MoveWindow(hint,inset,scale(236),w-inset*2,scale(43),TRUE);
    MoveWindow(GetDlgItem(window,kOptions),w-inset-scale(90),rect.bottom-scale(46),scale(90),scale(28),TRUE);
}
LRESULT HandleWindowMessage(HWND handle,UINT message,WPARAM wp,LPARAM lp) {
    if(message==taskbarCreated && taskbarCreated) { trayPresent=false; Tray(); return 0; }
    switch(message) {
    case WM_COPYDATA: {
        const auto* data = reinterpret_cast<const COPYDATASTRUCT*>(lp);
        if (!data || data->dwData != cursor_brightness::bridge::kSetPercent || !data->lpData ||
            data->cbData < sizeof(int) + sizeof(wchar_t) || data->cbData > sizeof(int) + 2049 * sizeof(wchar_t) ||
            (data->cbData - sizeof(int)) % sizeof(wchar_t) || !ready.load()) return FALSE;
        int percent = 0; std::memcpy(&percent, data->lpData, sizeof(percent));
        if (percent < 0 || percent > 100) return FALSE;
        const auto length = (data->cbData - sizeof(int)) / sizeof(wchar_t);
        std::wstring id(length, L'\0');
        std::memcpy(id.data(), static_cast<const BYTE*>(data->lpData) + sizeof(int), length * sizeof(wchar_t));
        if (id.back() != L'\0') return FALSE;
        id.pop_back();
        const auto snapshot = engine.GetDisplays();
        for (const auto& display : snapshot) if (display.stableId == id && display.percent >= 0 && display.transport != brightness::Transport::None) {
            engine.SetPercent(id, percent); return TRUE;
        }
        return FALSE;
    }
    case WM_CREATE: {
        window=handle;
        const int dpi=GetDpiForWindow(handle);
        UpdateFonts(dpi);
        combo=Child(WC_COMBOBOXW,L"",CBS_DROPDOWNLIST|WS_TABSTOP,kCombo);
        title=Child(L"STATIC",L"Finding your displays…",SS_LEFT,0); SendMessageW(title,WM_SETFONT,reinterpret_cast<WPARAM>(titleFont),TRUE);
        modeLabel=Child(L"STATIC",L"Reading current brightness",SS_LEFT,0);
        valueLabel=Child(L"STATIC",L"—",SS_CENTER,0); SendMessageW(valueLabel,WM_SETFONT,reinterpret_cast<WPARAM>(valueFont),TRUE);
        Child(L"BUTTON",L"−",BS_PUSHBUTTON|WS_TABSTOP,kDecrease);
        slider=Child(TRACKBAR_CLASSW,L"",TBS_HORZ|TBS_NOTICKS|WS_TABSTOP,kSlider); SendMessageW(slider,TBM_SETRANGE,TRUE,MAKELPARAM(0,100));
        Child(L"BUTTON",L"+",BS_PUSHBUTTON|WS_TABSTOP,kIncrease);
        hint=Child(L"STATIC",L"Move the cursor to a display, then use your keyboard’s brightness keys.",SS_LEFT,0);
        Child(L"BUTTON",L"Options",BS_PUSHBUTTON|WS_TABSTOP,kOptions);
        icon=SunIcon(); SendMessageW(handle,WM_SETICON,ICON_SMALL,reinterpret_cast<LPARAM>(icon)); SendMessageW(handle,WM_SETICON,ICON_BIG,reinterpret_cast<LPARAM>(icon));
        input.onStep=[](int direction) {
            if(!ready.load() || !preferences.keys) return;
            POINT point{}; if(!GetCursorPos(&point)) return;
            const bool changed=engine.AdjustAtPoint(point,direction,preferences.step);
            displays=engine.GetDisplays();
            if(changed) { if(auto* display=At(point)) { popupId=display->stableId; lastPopupKey=GetTickCount64(); } }
            PublishPopup();
        };
        ConfigureKeys();
        taskbarCreated=RegisterWindowMessageW(L"TaskbarCreated"); Tray();
        SetTimer(handle,kKeysTimer,25,nullptr); SetTimer(handle,kRefreshTimer,2000,nullptr);
        SetTimer(handle,kIntegrationTimer,10000,nullptr);
        WTSRegisterSessionNotification(handle,NOTIFY_FOR_THIS_SESSION);
        engine.SetOnChanged([](bool structural){ if(!exiting) { PostMessageW(window,kChanged,structural,0); app::integration::NotifyBrightnessChanged(); } });
        starter=std::thread([]{ engine.Start(); ready.store(true); PostMessageW(window,kChanged,TRUE,0); });
        Layout(); return 0;
    }
    case WM_SIZE: Layout(); return 0;
    case WM_CTLCOLORSTATIC:
        SetBkMode(reinterpret_cast<HDC>(wp),TRANSPARENT);
        return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_WINDOW));
    case WM_GETMINMAXINFO: { const int dpi=GetDpiForWindow(handle); reinterpret_cast<MINMAXINFO*>(lp)->ptMinTrackSize={MulDiv(440,dpi,96),MulDiv(380,dpi,96)}; return 0; }
    case WM_DPICHANGED: { UpdateFonts(HIWORD(wp)); const auto* r=reinterpret_cast<RECT*>(lp); SetWindowPos(handle,nullptr,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOZORDER|SWP_NOACTIVATE); return 0; }
    case kChanged:
        if(IsWindowVisible(handle)) RefreshUi();
        PublishPopup();
        return 0;
    case kShow: ShowPanel(); return 0;
    case WM_HSCROLL:
        if(reinterpret_cast<HWND>(lp)==slider && !updating) { if(auto* display=Selected()) { const int value=static_cast<int>(SendMessageW(slider,TBM_GETPOS,0,0)); SetWindowTextW(valueLabel,(std::to_wstring(value)+L"%").c_str()); engine.SetPercent(display->stableId,value); } }
        return 0;
    case WM_COMMAND:
        if(LOWORD(wp)==kCombo && HIWORD(wp)==CBN_SELCHANGE && !updating) { const auto index=SendMessageW(combo,CB_GETCURSEL,0,0); if(index>=0 && static_cast<size_t>(index)<displays.size()) { selectedId=displays[index].stableId; resolved=true; RefreshUi(); } }
        else if(LOWORD(wp)==kDecrease) ChangeSelected(-1);
        else if(LOWORD(wp)==kIncrease) ChangeSelected(1);
        else if(LOWORD(wp)==kOptions) { POINT p{}; GetCursorPos(&p); Menu(p); }
        return 0;
    case kTray:
        if(LOWORD(lp)==NIN_SELECT || LOWORD(lp)==NIN_KEYSELECT) ShowPanel();
        else if(LOWORD(lp)==WM_CONTEXTMENU) { POINT p{}; GetCursorPos(&p); Menu(p); }
        return 0;
    case WM_INPUT: input.Input(reinterpret_cast<HRAWINPUT>(lp),GetTickCount64()); break;
    case WM_INPUT_DEVICE_CHANGE: if(wp==GIDC_REMOVAL) input.Remove(reinterpret_cast<HANDLE>(lp)); break;
    case WM_TIMER:
        if(wp==kKeysTimer) { input.Tick(GetTickCount64()); PublishPopup(); }
        else if(wp==kRefreshTimer && IsWindowVisible(handle)) engine.RequestRefresh();
        else if(wp==kRescanTimer) { KillTimer(handle,kRescanTimer); engine.RequestRescan(); }
        else if(wp==kIntegrationTimer && preferences.integration && integration && integration->Available() && !integration->Running() && integrationRetries<3) {
            ++integrationRetries;
            try { integration->Start(); }
            catch(const std::exception& error) { std::ofstream(app::DataDirectory()/L"integration-error.txt") << error.what() << '\n'; }
        }
        return 0;
    case WM_DISPLAYCHANGE: case WM_SETTINGCHANGE: SetTimer(handle,kRescanTimer,500,nullptr); break;
    case WM_WTSSESSION_CHANGE: input.Reset(); if(wp==WTS_SESSION_UNLOCK) engine.RequestRescan(); break;
    case WM_POWERBROADCAST: input.Reset(); if(wp==PBT_APMRESUMEAUTOMATIC || wp==PBT_APMRESUMESUSPEND) engine.RequestRescan(); break;
    case WM_CLOSE: ShowWindow(handle,SW_HIDE); return 0;
    case WM_QUERYENDSESSION: return TRUE;
    case WM_ENDSESSION: if(wp) PostQuitMessage(0); return 0;
    case kQuit: input.Stop(); Tray(true); PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(handle,message,wp,lp);
}
LRESULT CALLBACK WindowProc(HWND handle,UINT message,WPARAM wp,LPARAM lp) noexcept {
    try { return HandleWindowMessage(handle,message,wp,lp); }
    catch(const std::exception& error) { Error(error.what()); return message==WM_CREATE ? -1 : 0; }
    catch(...) { Error("An unexpected Windows operation failed"); return message==WM_CREATE ? -1 : 0; }
}
std::string Utf8(std::wstring const& value) {
    int bytes=WideCharToMultiByte(CP_UTF8,0,value.c_str(),static_cast<int>(value.size()),nullptr,0,nullptr,nullptr);
    std::string result(bytes,'\0'); WideCharToMultiByte(CP_UTF8,0,value.c_str(),static_cast<int>(value.size()),result.data(),bytes,nullptr,nullptr); return result;
}
std::string Json(std::wstring const& value) {
    std::string result="\"";
    for(unsigned char c : Utf8(value)) { if(c=='"' || c=='\\') result+='\\'; if(c<32) result+=' '; else result+=static_cast<char>(c); }
    return result+'"';
}
int Diagnose(std::filesystem::path const& file) {
    engine.Start(); const auto snapshot=engine.GetDisplays();
    std::ofstream output(file); if(!output) { engine.Stop(); return 2; }
    output << "{\n  \"version\": \"1.2.1\",\n  \"standalone\": true,\n  \"displays\": [";
    for(size_t i=0;i<snapshot.size();++i) {
        const auto& d=snapshot[i];
        if(i) output<<',';
        output << "\n    {\"name\": " << Json(d.name) << ", \"mode\": " << Json(Mode(d)) << ", \"brightness\": " << d.percent << '}';
    }
    output << "\n  ]\n}\n"; engine.Stop(); return output.good()?0:3;
}
void Cleanup() noexcept {
    exiting.store(true);
    if(integration) integration->Stop();
    input.Stop(); engine.SetOnChanged(nullptr);
    popupPublisher.reset();
    if(starter.joinable()) starter.join();
    engine.Stop();
    if(window && IsWindow(window)) {
        WTSUnRegisterSessionNotification(window);
        if(trayPresent) Tray(true);
        DestroyWindow(window);
    }
    window=nullptr;
    if(icon) { DestroyIcon(icon); icon=nullptr; }
    for(auto font : {normalFont,titleFont,valueFont}) if(font) DeleteObject(font);
    normalFont=titleFont=valueFont=nullptr;
}
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    HRESULT com=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    int count=0; LPWSTR* arguments=CommandLineToArgvW(GetCommandLineW(),&count);
    bool background=false, quit=false; int startupAction=0; std::filesystem::path diagnostic;
    for(int i=1;i<count;++i) {
        if(std::wstring_view(arguments[i])==L"--background") background=true;
        else if(std::wstring_view(arguments[i])==L"--exit") quit=true;
        else if(std::wstring_view(arguments[i])==L"--enable-startup") startupAction=1;
        else if(std::wstring_view(arguments[i])==L"--disable-startup") startupAction=-1;
        else if(std::wstring_view(arguments[i])==L"--diagnose" && i+1<count) diagnostic=arguments[++i];
    }
    LocalFree(arguments);
    int result=0;
    try {
        if(startupAction) { app::startup::Set(startupAction>0); if(SUCCEEDED(com)) CoUninitialize(); return 0; }
        if(!diagnostic.empty()) { result=Diagnose(diagnostic); if(SUCCEEDED(com)) CoUninitialize(); return result; }
        if(quit) {
            if(HWND existing=FindWindowW(kClass,nullptr)) {
                DWORD processId=0; GetWindowThreadProcessId(existing,&processId);
                HANDLE process=OpenProcess(SYNCHRONIZE,FALSE,processId);
                PostMessageW(existing,kQuit,0,0);
                if(process) { result=WaitForSingleObject(process,20000)==WAIT_OBJECT_0 ? 0 : 1; CloseHandle(process); }
                else result=1;
            }
            if(SUCCEEDED(com)) CoUninitialize(); return result;
        }
        HANDLE mutex=CreateMutexW(nullptr,FALSE,L"Local\\CursorBrightness.Standalone.v1");
        if(!mutex) throw std::runtime_error("Cannot create application instance lock");
        if(GetLastError()==ERROR_ALREADY_EXISTS) { if(!background) { if(HWND existing=FindWindowW(kClass,nullptr)) PostMessageW(existing,kShow,0,0); } CloseHandle(mutex); if(SUCCEEDED(com)) CoUninitialize(); return 0; }
        preferences.Load();
        popupPublisher=std::make_unique<cursor_brightness::native_popup::Publisher>();
        integration=std::make_unique<app::integration::Runtime>();
        if(preferences.integration && integration->Available()) {
            try { integration->Start(); }
            catch(const std::exception& error) { std::ofstream(app::DataDirectory()/L"integration-error.txt") << error.what() << '\n'; }
        }
        INITCOMMONCONTROLSEX common{sizeof(common),ICC_BAR_CLASSES|ICC_STANDARD_CLASSES}; InitCommonControlsEx(&common);
        WNDCLASSW klass{}; klass.hInstance=instance; klass.lpfnWndProc=WindowProc; klass.lpszClassName=kClass; klass.hCursor=LoadCursorW(nullptr,IDC_ARROW); klass.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);
        if(!RegisterClassW(&klass)) throw std::runtime_error("Cannot register application window");
        window=CreateWindowExW(0,kClass,L"Cursor Brightness",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,460,390,nullptr,nullptr,instance,nullptr);
        if(!window) throw std::runtime_error("Cannot create application window");
        const int dpi=GetDpiForWindow(window);
        RECT initialSize{0,0,MulDiv(444,dpi,96),MulDiv(352,dpi,96)};
        AdjustWindowRectExForDpi(&initialSize,WS_OVERLAPPEDWINDOW,FALSE,0,dpi);
        SetWindowPos(window,nullptr,0,0,initialSize.right-initialSize.left,initialSize.bottom-initialSize.top,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
        // Consume STARTUPINFO's initial show override before later tray activation.
        ShowWindow(window,SW_HIDE);
        if(!background) ShowPanel();
        MSG message{};
        while(GetMessageW(&message,nullptr,0,0)>0) { if(!IsDialogMessageW(window,&message)) { TranslateMessage(&message); DispatchMessageW(&message); } }
        Cleanup();
        CloseHandle(mutex);
    } catch(const std::exception& error) {
        Cleanup();
        if(startupAction || background || !diagnostic.empty()) {
            try { std::ofstream(app::DataDirectory()/L"last-error.txt") << error.what() << '\n'; } catch(...) {}
        } else Error(error.what());
        result=1;
    }
    if(SUCCEEDED(com)) CoUninitialize(); return result;
}
