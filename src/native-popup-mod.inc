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
