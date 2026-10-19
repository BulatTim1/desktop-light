// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <windows.h>
#include <taskschd.h>
#include <sddl.h>
#include <comdef.h>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

namespace app::startup {
inline constexpr wchar_t kDescription[] = L"Cursor Brightness standalone application (GPL-3.0). Per-user interactive logon startup.";
template<class T> struct Com {
    T* value = nullptr;
    ~Com(){ if(value) value->Release(); }
    Com() = default;
    Com(const Com&) = delete;
    Com& operator=(const Com&) = delete;
    T* operator->() const { return value; }
    T** Put() { if(value) value->Release(); value = nullptr; return &value; }
};
inline void Check(HRESULT result) { if(FAILED(result)) throw std::runtime_error("Task Scheduler operation failed (" + std::to_string(static_cast<unsigned long>(result)) + ")"); }
inline std::wstring Sid() {
    HANDLE token = nullptr;
    if(!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) throw std::runtime_error("Cannot query current user");
    DWORD bytes = 0;
    GetTokenInformation(token, TokenUser, nullptr, 0, &bytes);
    std::vector<BYTE> buffer(bytes);
    bool ok = GetTokenInformation(token, TokenUser, buffer.data(), bytes, &bytes) != FALSE;
    CloseHandle(token);
    if(!ok) throw std::runtime_error("Cannot query current user SID");
    LPWSTR sid = nullptr;
    if(!ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER*>(buffer.data())->User.Sid, &sid)) throw std::runtime_error("Cannot format user SID");
    std::wstring result(sid); LocalFree(sid); return result;
}
inline std::wstring TaskName() { return L"CursorBrightnessApp-" + Sid(); }
struct Scheduler {
    Com<ITaskService> service;
    Com<ITaskFolder> folder;
    Scheduler() {
        Check(CoCreateInstance(CLSID_TaskScheduler, nullptr, CLSCTX_INPROC_SERVER, IID_ITaskService, reinterpret_cast<void**>(service.Put())));
        Check(service->Connect(_variant_t(), _variant_t(), _variant_t(), _variant_t()));
        Check(service->GetFolder(_bstr_t(L"\\"), folder.Put()));
    }
    bool OwnTask(Com<IRegisteredTask>& task) {
        HRESULT result = folder->GetTask(_bstr_t(TaskName().c_str()), task.Put());
        if(result == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)) return false;
        Check(result);
        Com<ITaskDefinition> definition; Check(task->get_Definition(definition.Put()));
        Com<IRegistrationInfo> info; Check(definition->get_RegistrationInfo(info.Put()));
        BSTR text = nullptr; Check(info->get_Description(&text));
        const bool ours = text && std::wstring(text) == kDescription;
        SysFreeString(text);
        if(!ours) throw std::runtime_error("An unrelated startup task uses this name; it was not changed");
        return true;
    }
};
inline bool Enabled() {
    Scheduler scheduler;
    Com<IRegisteredTask> task;
    if(!scheduler.OwnTask(task)) return false;
    VARIANT_BOOL enabled = VARIANT_FALSE; Check(task->get_Enabled(&enabled));
    return enabled != VARIANT_FALSE;
}
inline void Set(bool enabled) {
    Scheduler scheduler;
    Com<IRegisteredTask> previous;
    const bool existed = scheduler.OwnTask(previous);
    if(!enabled) { if(existed) Check(scheduler.folder->DeleteTask(_bstr_t(TaskName().c_str()), 0)); return; }
    wchar_t filename[32768]{};
    if(!GetModuleFileNameW(nullptr, filename, 32768)) throw std::runtime_error("Cannot locate application executable");
    const auto directory = std::filesystem::path(filename).parent_path();
    const auto sid = Sid();
    Com<ITaskDefinition> definition; Check(scheduler.service->NewTask(0, definition.Put()));
    Com<IRegistrationInfo> info; Check(definition->get_RegistrationInfo(info.Put()));
    Check(info->put_Description(_bstr_t(kDescription)));
    Com<IPrincipal> principal; Check(definition->get_Principal(principal.Put()));
    Check(principal->put_UserId(_bstr_t(sid.c_str())));
    Check(principal->put_LogonType(TASK_LOGON_INTERACTIVE_TOKEN));
    Check(principal->put_RunLevel(TASK_RUNLEVEL_LUA));
    Com<ITaskSettings> settings; Check(definition->get_Settings(settings.Put()));
    Check(settings->put_DisallowStartIfOnBatteries(VARIANT_FALSE));
    Check(settings->put_StopIfGoingOnBatteries(VARIANT_FALSE));
    Check(settings->put_ExecutionTimeLimit(_bstr_t(L"PT0S")));
    Check(settings->put_StartWhenAvailable(VARIANT_TRUE));
    Check(settings->put_MultipleInstances(TASK_INSTANCES_IGNORE_NEW));
    Check(settings->put_RestartCount(3));
    Check(settings->put_RestartInterval(_bstr_t(L"PT1M")));
    Com<ITriggerCollection> triggers; Check(definition->get_Triggers(triggers.Put()));
    Com<ITrigger> trigger; Check(triggers->Create(TASK_TRIGGER_LOGON, trigger.Put()));
    Com<ILogonTrigger> logon; Check(trigger->QueryInterface(IID_ILogonTrigger, reinterpret_cast<void**>(logon.Put())));
    Check(logon->put_UserId(_bstr_t(sid.c_str())));
    Check(logon->put_Delay(_bstr_t(L"PT15S")));
    Com<IActionCollection> actions; Check(definition->get_Actions(actions.Put()));
    Com<IAction> action; Check(actions->Create(TASK_ACTION_EXEC, action.Put()));
    Com<IExecAction> exec; Check(action->QueryInterface(IID_IExecAction, reinterpret_cast<void**>(exec.Put())));
    Check(exec->put_Path(_bstr_t(filename)));
    Check(exec->put_Arguments(_bstr_t(L"--background")));
    Check(exec->put_WorkingDirectory(_bstr_t(directory.c_str())));
    Com<IRegisteredTask> registered;
    Check(scheduler.folder->RegisterTaskDefinition(_bstr_t(TaskName().c_str()), definition.value, TASK_CREATE_OR_UPDATE,
        _variant_t(sid.c_str()), _variant_t(), TASK_LOGON_INTERACTIVE_TOKEN, _variant_t(), registered.Put()));
}
} // namespace app::startup
