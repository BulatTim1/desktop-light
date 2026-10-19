// SPDX-License-Identifier: GPL-3.0-only
#include "../src/native-popup-state.hpp"
#include <cassert>
#include <iostream>
int main() {
    using namespace cursor_brightness::native_popup;
    const auto name=L"Local\\CursorBrightness.NativePopup.Test."+std::to_wstring(GetCurrentProcessId());
    int value=-1;
    assert(!Read(value,name.c_str()));
    {
        Publisher publisher(name.c_str());
        publisher.Publish(37,true);
        assert(Read(value,name.c_str()) && value==37);
        publisher.Publish(100,true);
        assert(Read(value,name.c_str()) && value==100);
        publisher.Publish(0,true);
        assert(Read(value,name.c_str()) && value==0);
        publisher.Publish(50,false);
        assert(!Read(value,name.c_str()));
        publisher.Publish(-1,true);
        assert(!Read(value,name.c_str()));
    }
    assert(!Read(value,name.c_str()));
    std::cout << "Native popup mapping: publication, limits, disabled/unavailable data and teardown passed\n";
}
