#pragma once
#include "hvi.hpp"
namespace hvi {
struct AdapterCapabilities {bool immediate=true,buffered=true,background=true,nativeOsInput=false;};
class DeviceAdapter:public InputAdapter {
public:
 virtual State snapshot()const=0;
 virtual std::vector<Transition> read(size_t count,bool peek=false)=0;
 virtual void disconnected()=0;
 virtual AdapterCapabilities capabilities()const=0;
};
// Reference cooperative adapter: no process injection or OS input synthesis.
class CooperativeAdapter final:public DeviceAdapter {
 InputDevice device;uint32_t session;
public:
 explicit CooperativeAdapter(uint32_t id):session(id){}
 bool deliver(const Event&e)override{return e.session==session&&device.apply(e);}
 State snapshot()const override{return device.snapshot();}
 std::vector<Transition> read(size_t n,bool peek=false)override{return device.read(n,peek);}
 void disconnected()override{device.disconnect();}
 AdapterCapabilities capabilities()const override{return {};}
};
}