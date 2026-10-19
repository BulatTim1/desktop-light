// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <windows.h>
#include <tlhelp32.h>
#include <filesystem>
#include <string>
#include <stdexcept>

namespace app::integration {
inline constexpr wchar_t kExternalChange[] = L"CursorBrightness.ExternalBrightnessChanged.v1";
class Runtime {
    HANDLE process_ = nullptr;
    std::filesystem::path executable_;
public:
    Runtime() {
        wchar_t path[32768]{}; GetModuleFileNameW(nullptr,path,32768);
        executable_=std::filesystem::path(path).parent_path()/L"integration"/L"windhawk"/L"windhawk.exe";
    }
    ~Runtime(){ if(process_) CloseHandle(process_); }
    bool Available() const { return std::filesystem::exists(executable_); }
    bool Running() const { DWORD code=0; return process_ && GetExitCodeProcess(process_,&code) && code==STILL_ACTIVE; }
    void Start() {
        if(Running()) return;
        if(!Available()) throw std::runtime_error("The Quick Settings runtime is missing. Install the complete package, or use the native tray controls.");
        // Windhawk uses one per-session daemon. Do not take over another install.
        HANDLE existing=OpenMutexW(SYNCHRONIZE,FALSE,L"WindhawkDaemon");
        if(existing) {
            CloseHandle(existing);
            // Recover our own helper after an app crash, while leaving other installations alone.
            HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);
            if(snapshot!=INVALID_HANDLE_VALUE) {
                PROCESSENTRY32W entry{}; entry.dwSize=sizeof(entry);
                if(Process32FirstW(snapshot,&entry)) do {
                    if(_wcsicmp(entry.szExeFile,L"windhawk.exe")) continue;
                    DWORD candidateSession=0, currentSession=0;
                    if(!ProcessIdToSessionId(entry.th32ProcessID,&candidateSession) || !ProcessIdToSessionId(GetCurrentProcessId(),&currentSession) || candidateSession!=currentSession) continue;
                    HANDLE candidate=OpenProcess(SYNCHRONIZE|PROCESS_QUERY_LIMITED_INFORMATION,FALSE,entry.th32ProcessID);
                    if(!candidate) continue;
                    wchar_t path[32768]{}; DWORD length=32768;
                    if(QueryFullProcessImageNameW(candidate,0,path,&length) && !_wcsicmp(path,executable_.c_str())) {
                        if(process_) CloseHandle(process_);
                        process_=candidate; CloseHandle(snapshot); return;
                    }
                    CloseHandle(candidate);
                } while(Process32NextW(snapshot,&entry));
                CloseHandle(snapshot);
            }
            throw std::runtime_error("Another Windhawk installation is running. Quit it before enabling the bundled Quick Settings integration, or use the add-on in your existing installation.");
        }
        if(process_) { CloseHandle(process_); process_=nullptr; }
        std::wstring command=L"\""+executable_.wstring()+L"\" -tray-only";
        STARTUPINFOW startup{}; startup.cb=sizeof(startup); startup.dwFlags=STARTF_USESHOWWINDOW; startup.wShowWindow=SW_HIDE;
        PROCESS_INFORMATION process{};
        if(!CreateProcessW(executable_.c_str(),command.data(),nullptr,nullptr,FALSE,0,nullptr,executable_.parent_path().c_str(),&startup,&process)) throw std::runtime_error("Cannot start the bundled Quick Settings runtime");
        CloseHandle(process.hThread); process_=process.hProcess;
    }
    void Stop() noexcept {
        if(!Running()) return;
        std::wstring command=L"\""+executable_.wstring()+L"\" -exit -wait -timeout 10000";
        STARTUPINFOW startup{}; startup.cb=sizeof(startup); startup.dwFlags=STARTF_USESHOWWINDOW; startup.wShowWindow=SW_HIDE;
        PROCESS_INFORMATION process{};
        if(CreateProcessW(executable_.c_str(),command.data(),nullptr,nullptr,FALSE,0,nullptr,executable_.parent_path().c_str(),&startup,&process)) {
            CloseHandle(process.hThread); WaitForSingleObject(process.hProcess,12000); CloseHandle(process.hProcess);
        }
        if(process_) { CloseHandle(process_); process_=nullptr; }
    }
};
inline void NotifyBrightnessChanged() {
    if(HWND sink=FindWindowW(L"WindhawkPerMonitorBrightnessSink",nullptr)) PostMessageW(sink,RegisterWindowMessageW(kExternalChange),0,0);
}
} // namespace app::integration
