#pragma once
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include "input_state.hpp"
#include <atomic>
#include <memory>
#include <cstring>
#include <climits>
#include <type_traits>
namespace hvi {
struct MouseFeed {
 std::mutex mutex;State state;uint64_t generation=0;
 bool apply(const Event&e){std::lock_guard<std::mutex> l(mutex);return state.apply(e);}
 void reset(){std::lock_guard<std::mutex> l(mutex);state={};++generation;}
};
template<bool Wide> class DirectInputMouse final:public std::conditional_t<Wide,IDirectInputDevice8W,IDirectInputDevice8A>{
 using EnumCallback=std::conditional_t<Wide,LPDIENUMDEVICEOBJECTSCALLBACKW,LPDIENUMDEVICEOBJECTSCALLBACKA>;
 using ObjectInfo=std::conditional_t<Wide,LPDIDEVICEOBJECTINSTANCEW,LPDIDEVICEOBJECTINSTANCEA>;
 using DeviceInfo=std::conditional_t<Wide,LPDIDEVICEINSTANCEW,LPDIDEVICEINSTANCEA>;
 using EffectCallback=std::conditional_t<Wide,LPDIENUMEFFECTSCALLBACKW,LPDIENUMEFFECTSCALLBACKA>;
 using EffectInfo=std::conditional_t<Wide,LPDIEFFECTINFOW,LPDIEFFECTINFOA>;
 using Text=std::conditional_t<Wide,LPCWSTR,LPCSTR>;
 using ActionFormat=std::conditional_t<Wide,LPDIACTIONFORMATW,LPDIACTIONFORMATA>;
 using ImageInfo=std::conditional_t<Wide,LPDIDEVICEIMAGEINFOHEADERW,LPDIDEVICEIMAGEINFOHEADERA>;
 std::atomic<ULONG> refs{1};std::shared_ptr<MouseFeed> feed;std::mutex mutex;
 bool acquired=false,format=false,extended=false,overflow=false;DWORD capacity=0;uint64_t cursor=0,generation=0;int64_t x=0,y=0,z=0;
 std::deque<DIDEVICEOBJECTDATA> queue;
 static LONG clamp(int64_t n){return static_cast<LONG>(std::clamp(n,int64_t(LONG_MIN),int64_t(LONG_MAX)));}
 void sync(){
  std::lock_guard<std::mutex> l(feed->mutex);auto&s=feed->state;
  if(generation!=feed->generation){generation=feed->generation;cursor=0;x=y=z=0;queue.clear();overflow=false;}
  auto events=s.mouseEvents.read(256,true);
  if(!events.empty()&&cursor&&events.front().order>cursor+1)overflow=true;
  for(auto&t:events)if(t.order>cursor){
   DWORD offset=0;bool supported=true;
   if(t.kind==Kind::Relative)offset=t.control?DIMOFS_Y:DIMOFS_X;
   else if(t.kind==Kind::Wheel&&t.control==0)offset=DIMOFS_Z;
   else if(t.kind==Kind::Button&&t.control<(extended?5:4))offset=DIMOFS_BUTTON0+t.control;
   else supported=false;
   if(supported&&capacity){if(queue.size()==capacity){queue.pop_front();overflow=true;}DIDEVICEOBJECTDATA d{};d.dwOfs=offset;d.dwData=static_cast<DWORD>(t.value);d.dwTimeStamp=static_cast<DWORD>(t.timestamp);d.dwSequence=static_cast<DWORD>(t.order);queue.push_back(d);}
   cursor=t.order;
  }
 }
public:
 explicit DirectInputMouse(std::shared_ptr<MouseFeed> f):feed(std::move(f)){}
 HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,void**out)override{if(!out)return E_POINTER;*out=nullptr;if(iid!=IID_IUnknown&&iid!=(Wide?IID_IDirectInputDevice8W:IID_IDirectInputDevice8A))return E_NOINTERFACE;*out=this;AddRef();return S_OK;}
 ULONG STDMETHODCALLTYPE AddRef()override{return ++refs;}
 ULONG STDMETHODCALLTYPE Release()override{ULONG n=--refs;if(!n)delete this;return n;}
 HRESULT STDMETHODCALLTYPE GetCapabilities(LPDIDEVCAPS c)override{if(!c||c->dwSize!=sizeof(*c))return DIERR_INVALIDPARAM;*c={};c->dwSize=sizeof(*c);c->dwFlags=DIDC_ATTACHED;c->dwDevType=DI8DEVTYPE_MOUSE;c->dwAxes=3;c->dwButtons=5;return DI_OK;}
 HRESULT STDMETHODCALLTYPE SetCooperativeLevel(HWND hwnd,DWORD flags)override{if(!IsWindow(hwnd))return E_HANDLE;if(flags!=(DISCL_NONEXCLUSIVE|DISCL_BACKGROUND))return DIERR_UNSUPPORTED;return DI_OK;}
 HRESULT STDMETHODCALLTYPE SetDataFormat(LPCDIDATAFORMAT f)override{
  std::lock_guard<std::mutex> l(mutex);if(acquired)return DIERR_ACQUIRED;
  if(!f||f->dwSize!=sizeof(*f)||f->dwObjSize!=sizeof(DIOBJECTDATAFORMAT)||!f->rgodf)return DIERR_INVALIDPARAM;
  const DIDATAFORMAT* expected=f->dwDataSize==sizeof(DIMOUSESTATE)?&c_dfDIMouse:f->dwDataSize==sizeof(DIMOUSESTATE2)?&c_dfDIMouse2:nullptr;
  if(!expected||f->dwFlags!=expected->dwFlags||f->dwNumObjs!=expected->dwNumObjs)return DIERR_UNSUPPORTED;
  for(DWORD i=0;i<f->dwNumObjs;i++){auto&a=f->rgodf[i];auto&b=expected->rgodf[i];if(a.dwOfs!=b.dwOfs||a.dwType!=b.dwType||a.dwFlags!=b.dwFlags||bool(a.pguid)!=bool(b.pguid)||(a.pguid&&*a.pguid!=*b.pguid))return DIERR_UNSUPPORTED;}
  extended=f->dwDataSize==sizeof(DIMOUSESTATE2);format=true;return DI_OK;
 }
 HRESULT STDMETHODCALLTYPE SetProperty(REFGUID prop,LPCDIPROPHEADER h)override{
  std::lock_guard<std::mutex> l(mutex);if(acquired)return DIERR_ACQUIRED;
  if(!h||h->dwSize!=sizeof(DIPROPDWORD)||h->dwHeaderSize!=sizeof(DIPROPHEADER)||h->dwHow!=DIPH_DEVICE||h->dwObj)return DIERR_INVALIDPARAM;
  auto p=reinterpret_cast<const DIPROPDWORD*>(h);
  if(&prop==&DIPROP_BUFFERSIZE){if(p->dwData>256)return DIERR_INVALIDPARAM;capacity=p->dwData;queue.clear();overflow=false;return DI_OK;}
  if(&prop==&DIPROP_AXISMODE)return p->dwData==DIPROPAXISMODE_REL?DI_OK:DIERR_UNSUPPORTED;
  return DIERR_UNSUPPORTED;
 }
 HRESULT STDMETHODCALLTYPE GetProperty(REFGUID prop,LPDIPROPHEADER h)override{
  std::lock_guard<std::mutex> l(mutex);if(!h||h->dwSize!=sizeof(DIPROPDWORD)||h->dwHeaderSize!=sizeof(DIPROPHEADER)||h->dwHow!=DIPH_DEVICE||h->dwObj)return DIERR_INVALIDPARAM;
  auto p=reinterpret_cast<DIPROPDWORD*>(h);if(&prop==&DIPROP_BUFFERSIZE)p->dwData=capacity;else if(&prop==&DIPROP_AXISMODE)p->dwData=DIPROPAXISMODE_REL;else return DIERR_UNSUPPORTED;return DI_OK;
 }
 HRESULT STDMETHODCALLTYPE Acquire()override{std::lock_guard<std::mutex> l(mutex);if(!format)return DIERR_NOTINITIALIZED;if(acquired)return S_FALSE;acquired=true;std::lock_guard<std::mutex> f(feed->mutex);x=feed->state.mouse.dx;y=feed->state.mouse.dy;z=feed->state.mouse.wheel;cursor=feed->state.order;generation=feed->generation;queue.clear();overflow=false;return DI_OK;}
 HRESULT STDMETHODCALLTYPE Unacquire()override{std::lock_guard<std::mutex> l(mutex);acquired=false;queue.clear();overflow=false;return DI_OK;}
 HRESULT STDMETHODCALLTYPE Poll()override{std::lock_guard<std::mutex> l(mutex);return acquired?DI_OK:DIERR_NOTACQUIRED;}
 HRESULT STDMETHODCALLTYPE GetDeviceState(DWORD size,LPVOID out)override{
  std::lock_guard<std::mutex> l(mutex);if(!acquired)return DIERR_NOTACQUIRED;if(!out||size!=(extended?sizeof(DIMOUSESTATE2):sizeof(DIMOUSESTATE)))return DIERR_INVALIDPARAM;sync();
  std::lock_guard<std::mutex> f(feed->mutex);auto&m=feed->state.mouse;DIMOUSESTATE2 s{};s.lX=clamp(m.dx-x);s.lY=clamp(m.dy-y);s.lZ=clamp(m.wheel-z);x=m.dx;y=m.dy;z=m.wheel;for(int i=0;i<(extended?5:4);i++)s.rgbButtons[i]=m.buttons[i]?0x80:0;std::memcpy(out,&s,size);return DI_OK;
 }
 HRESULT STDMETHODCALLTYPE GetDeviceData(DWORD size,LPDIDEVICEOBJECTDATA out,LPDWORD count,DWORD flags)override{
  std::lock_guard<std::mutex> l(mutex);if(!acquired)return DIERR_NOTACQUIRED;if(!capacity)return DIERR_NOTBUFFERED;if(size!=sizeof(DIDEVICEOBJECTDATA)||!count||(flags&~DIGDD_PEEK))return DIERR_INVALIDPARAM;sync();DWORD n=static_cast<DWORD>(std::min<size_t>(*count,queue.size()));for(DWORD i=0;i<n;i++)if(out)out[i]=queue[i];*count=n;bool lost=overflow;if(!(flags&DIGDD_PEEK)){queue.erase(queue.begin(),queue.begin()+n);overflow=false;}return lost?DI_BUFFEROVERFLOW:DI_OK;
 }
 #include "mouse_unsupported.inc"
};
}