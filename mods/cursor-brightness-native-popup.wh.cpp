// ==WindhawkMod==
// @id              cursor-brightness-native-popup
// @name            Cursor brightness in the native Windows popup
// @description     Synchronize the existing Windows brightness indicator with the cursor monitor
// @version         1.2.1
// @author          Cursor Brightness contributors
// @include         explorer.exe
// @architecture    x86-64
// @compilerOptions -lruntimeobject -luuid
// ==/WindhawkMod==
// SPDX-License-Identifier: GPL-3.0-only
// SPDX-License-Identifier: GPL-3.0-only

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace cursor_brightness {
struct Bounds { long left, top, right, bottom; };
inline bool Contains(Bounds r, long x, long y) {
    return x >= r.left && x < r.right && y >= r.top && y < r.bottom;
}
inline int Adjust(int current, int direction, int step) {
    return std::clamp(current + direction * std::clamp(step, 1, 100), 0, 100);
}
inline unsigned PercentToRaw(int percent, unsigned maximum) {
    return unsigned((std::uint64_t(std::clamp(percent, 0, 100)) * maximum + 50) / 100);
}
inline int RawToPercent(unsigned value, unsigned maximum) {
    return maximum ? int(std::min<std::uint64_t>(100, (std::uint64_t(value) * 100 + maximum / 2) / maximum)) : -1;
}
// Match Windows' SDR-content slider: 0..100 maps to 80..480 reference nits.
inline unsigned PercentToWhiteLevel(int percent) { return 1000 + 50 * std::clamp(percent, 0, 100); }
inline int WhiteLevelToPercent(unsigned value) { return int(std::clamp<long long>((static_cast<long long>(value) - 1000 + 25) / 50, 0, 100)); }
struct Candidate { std::wstring id; Bounds bounds; };
inline bool FreshNativePopup(int percent, bool enabled, std::uint64_t tick, std::uint64_t now) {
    return enabled && percent >= 0 && percent <= 100 && tick <= now && now - tick <= 2000;
}
struct PanelSelection {
    bool open = false, resolved = false;
    long x = 0, y = 0;
    std::wstring id;
    bool Open(long cursorX, long cursorY) {
        if (open) return false;
        open = true; resolved = false; id.clear();
        x = cursorX; y = cursorY;
        return true;
    }
    void Close() { open = false; }
    std::wstring Resolve(const std::vector<Candidate>& displays) {
        if (!resolved && !displays.empty()) {
            for (const auto& display : displays) {
                if (Contains(display.bounds, x, y)) { id = display.id; break; }
            }
            resolved = true;
        }
        return id;
    }
};
// Repeated identical HID reports do not reset the repeat deadline or trigger
// another immediate step. Release, direction changes and removal reset it.
struct RepeatState {
    int held = 0;
    std::uint64_t next = 0;
    bool Update(int direction, std::uint64_t now) {
        if (direction == held) return false;
        held = direction;
        next = direction ? now + 400 : 0;
        return direction != 0;
    }
    bool Tick(std::uint64_t now) {
        if (!held || now < next) return false;
        // Never replay missed repeats after a blocked message loop or resume.
        next = now + 100;
        return true;
    }
    void Reset() { held = 0; next = 0; }
};
} // namespace cursor_brightness

// SPDX-License-Identifier: GPL-3.0-only

#include <windows.h>


namespace cursor_brightness::native_popup {
inline constexpr wchar_t kMapping[] = L"Local\\CursorBrightness.NativePopup.v1";
struct Shared {
    volatile LONG sequence;
    DWORD magic;
    LONG percent;
    LONG enabled;
    ULONGLONG heartbeat;
};
inline constexpr DWORD kMagic = 0x43424e31;
inline bool Read(int& percent,const wchar_t* name=kMapping) {
    HANDLE mapping=OpenFileMappingW(FILE_MAP_READ,FALSE,name);
    if(!mapping) return false;
    const auto* shared=static_cast<const Shared*>(MapViewOfFile(mapping,FILE_MAP_READ,0,0,sizeof(Shared)));
    bool valid=false;
    if(shared) {
        for(int attempt=0;attempt<3;++attempt) {
            const LONG first=shared->sequence;
            if(first&1) continue;
            MemoryBarrier();
            const DWORD magic=shared->magic;
            const int value=shared->percent;
            const bool enabled=shared->enabled!=0;
            const auto tick=shared->heartbeat;
            MemoryBarrier();
            if(first!=shared->sequence) continue;
            valid=magic==kMagic && FreshNativePopup(value,enabled,tick,GetTickCount64());
            if(valid) percent=value;
            break;
        }
        UnmapViewOfFile(shared);
    }
    CloseHandle(mapping); return valid;
}
class Publisher {
    HANDLE mapping_=nullptr;
    Shared* shared_=nullptr;
public:
    explicit Publisher(const wchar_t* name=kMapping) {
        mapping_=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,sizeof(Shared),name);
        if(mapping_) shared_=static_cast<Shared*>(MapViewOfFile(mapping_,FILE_MAP_WRITE,0,0,sizeof(Shared)));
    }
    ~Publisher() {
        Publish(-1,false);
        if(shared_) UnmapViewOfFile(shared_);
        if(mapping_) CloseHandle(mapping_);
    }
    void Publish(int percent,bool enabled) {
        if(!shared_) return;
        InterlockedIncrement(&shared_->sequence);
        shared_->magic=kMagic; shared_->percent=percent;
        shared_->enabled=enabled; shared_->heartbeat=GetTickCount64();
        InterlockedIncrement(&shared_->sequence);
    }
};
} // namespace cursor_brightness::native_popup

// SPDX-License-Identifier: GPL-3.0-only
// Native popup view-model only: no display, power, audio or keyboard APIs.
#include <windows.h>
#include <unknwn.h>
#include <inspectable.h>
#include <winstring.h>
#include <cwchar>
#include <atomic>
#include <thread>
#include <windhawk_utils.h>

namespace {
using Getter=HRESULT (WINAPI*)(void*,UINT*);
using Setter=HRESULT (WINAPI*)(void*,UINT);
Getter getOriginal=nullptr;
Setter setOriginal=nullptr;
std::atomic<bool> stopping{false};
std::atomic<HWND> uiWindow{nullptr};
DWORD uiThread=0;
IUnknown* model=nullptr;
UINT nativeValue=100;
bool overridden=false;
HANDLE quitEvent=nullptr;
[[clang::no_destroy]] std::thread watcher;
thread_local bool syntheticUpdate=false;
SRWLOCK captureLock=SRWLOCK_INIT;
constexpr UINT kStop=WM_APP+71;

bool IsScreenModel(void* self) {
    // Windows folds the screen and keyboard-backlight getters to the same
    // address on 26100. Verify the runtime class before overriding anything.
    IInspectable* inspectable=nullptr;
    if(FAILED(static_cast<IUnknown*>(self)->QueryInterface(__uuidof(IInspectable),reinterpret_cast<void**>(&inspectable)))) return false;
    HSTRING name=nullptr;
    const HRESULT result=inspectable->GetRuntimeClassName(&name);
    inspectable->Release();
    UINT32 length=0;
    const wchar_t* text=SUCCEEDED(result) ? WindowsGetStringRawBuffer(name,&length) : nullptr;
    constexpr wchar_t expected[]=L"HWConfirmatorUI.BrightnessConfirmatorViewModel";
    const bool screen=text && length==ARRAYSIZE(expected)-1 && !wmemcmp(text,expected,length);
    WindowsDeleteString(name); return screen;
}

void UpdateModel(UINT value) {
    syntheticUpdate=true; setOriginal(model,value); syntheticUpdate=false;
}

void ReleaseModel() {
    if(model) {
        if(overridden && setOriginal) UpdateModel(nativeValue);
        model->Release(); model=nullptr;
    }
    overridden=false;
}
LRESULT CALLBACK PopupWindowProc(HWND window,UINT message,WPARAM wp,LPARAM lp) {
    if(message==WM_TIMER && !stopping && model) {
        int percent=0;
        if(cursor_brightness::native_popup::Read(percent)) {
            UINT previous=0;
            if(SUCCEEDED(getOriginal(model,&previous)) && previous!=static_cast<UINT>(percent)) {
                overridden=true;
                UpdateModel(percent);
            }
        } else if(overridden) {
            overridden=false; UpdateModel(nativeValue);
        }
        return 0;
    }
    if(message==kStop) {
        KillTimer(window,1); ReleaseModel();
        uiWindow.store(nullptr); DestroyWindow(window); return 0;
    }
    return DefWindowProcW(window,message,wp,lp);
}
void Capture(void* self,UINT original) {
    if(stopping) return;
    AcquireSRWLockExclusive(&captureLock);
    if(stopping) { ReleaseSRWLockExclusive(&captureLock); return; }
    if(!uiWindow.load()) {
        WNDCLASSW klass{}; klass.lpfnWndProc=PopupWindowProc;
        klass.hInstance=GetModuleHandleW(nullptr); klass.lpszClassName=L"CursorBrightness.NativePopup.Sync";
        RegisterClassW(&klass);
        uiThread=GetCurrentThreadId();
        HWND window=CreateWindowExW(0,klass.lpszClassName,L"",0,0,0,0,0,HWND_MESSAGE,nullptr,klass.hInstance,nullptr);
        if(!window) { ReleaseSRWLockExclusive(&captureLock); return; }
        uiWindow.store(window); SetTimer(window,1,40,nullptr);
    }
    if(GetCurrentThreadId()!=uiThread) { ReleaseSRWLockExclusive(&captureLock); return; }
    auto* current=static_cast<IUnknown*>(self);
    if(model!=current) {
        current->AddRef();
        if(model) model->Release();
        model=current; nativeValue=original; overridden=false;
    }
    ReleaseSRWLockExclusive(&captureLock);
}
HRESULT WINAPI GetHook(void* self,UINT* value) {
    const HRESULT result=getOriginal(self,value);
    if(SUCCEEDED(result) && value && !stopping && !syntheticUpdate && IsScreenModel(self)) {
        Capture(self,*value);
        int percent=0;
        if(cursor_brightness::native_popup::Read(percent)) {
            Wh_Log(L"Native brightness getter: Windows=%u, monitor=%d",*value,percent);
            *value=percent;
        }
    }
    return result;
}
HRESULT WINAPI SetHook(void* self,UINT value) {
    if(!stopping && IsScreenModel(self)) {
        Capture(self,value);
        if(GetCurrentThreadId()==uiThread) nativeValue=value;
        int percent=0;
        if(cursor_brightness::native_popup::Read(percent)) {
            Wh_Log(L"Native brightness setter: Windows=%u, monitor=%d",value,percent);
            value=percent;
            if(GetCurrentThreadId()==uiThread) overridden=true;
        }
    }
    return setOriginal(self,value);
}
void Watch() noexcept try {
    while(WaitForSingleObject(quitEvent,250)==WAIT_TIMEOUT) {
        HMODULE module=GetModuleHandleW(L"Windows.Internal.HardwareConfirmator.dll");
        if(!module) continue;
        WindhawkUtils::SYMBOL_HOOK hooks[]={
            {{L"public: virtual int __cdecl winrt::impl::produce<struct winrt::HWConfirmatorUI::implementation::BrightnessConfirmatorViewModel,struct winrt::HWConfirmatorUI::IBrightnessConfirmatorViewModel>::get_Brightness(unsigned int *)"},&getOriginal},
            {{L"public: virtual int __cdecl winrt::impl::produce<struct winrt::HWConfirmatorUI::implementation::BrightnessConfirmatorViewModel,struct winrt::HWConfirmatorUI::IBrightnessConfirmatorViewModel>::put_Brightness(unsigned int)"},&setOriginal},
        };
        if(WindhawkUtils::HookSymbols(module,hooks,ARRAYSIZE(hooks))) {
            const auto getTarget=getOriginal;
            const auto setTarget=setOriginal;
            WindhawkUtils::SetFunctionHook(getTarget,GetHook,&getOriginal);
            WindhawkUtils::SetFunctionHook(setTarget,SetHook,&setOriginal);
            Wh_ApplyHookOperations();
            Wh_Log(L"Native Windows brightness popup hooks installed");
        } else Wh_Log(L"Native popup hooks unavailable; Windows popup remains unchanged");
        break;
    }
} catch(...) { Wh_Log(L"Native popup hook initialization failed; Windows popup remains available"); }
}
BOOL Wh_ModInit() {
    quitEvent=CreateEventW(nullptr,TRUE,FALSE,nullptr);
    if(!quitEvent) return FALSE;
    try { watcher=std::thread(Watch); }
    catch(...) { CloseHandle(quitEvent); quitEvent=nullptr; return FALSE; }
    return TRUE;
}
void Wh_ModBeforeUninit() {
    stopping=true;
    SetEvent(quitEvent);
    if(watcher.joinable()) watcher.join();
    AcquireSRWLockExclusive(&captureLock);
    ReleaseSRWLockExclusive(&captureLock);
    if(HWND window=uiWindow.load()) SendMessageW(window,kStop,0,0);
}
void Wh_ModUninit() {
    UnregisterClassW(L"CursorBrightness.NativePopup.Sync",GetModuleHandleW(nullptr));
    if(quitEvent) CloseHandle(quitEvent);
}
