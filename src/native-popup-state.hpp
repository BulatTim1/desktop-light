// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <windows.h>
#include "brightness-core.hpp"

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
