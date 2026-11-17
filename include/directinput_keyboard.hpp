#pragma once
#include "directinput_mouse.hpp"
namespace hvi {
// Protocol uses VKs. Convert using a fixed US physical layout; no VK byte offsets
// are exposed as DIK offsets. E0 scan codes occupy DirectInput's upper half.
inline DWORD vkToDik(int key){
 if(key<0||key>255||key==VK_SHIFT||key==VK_CONTROL||key==VK_MENU)return 0;
 if(key==VK_PAUSE)return DIK_PAUSE;
 if(key==VK_SNAPSHOT)return DIK_SYSRQ;
 static HKL layout=LoadKeyboardLayoutW(L"00000409",KLF_NOTELLSHELL);
 UINT scan=MapVirtualKeyExW(key,MAPVK_VK_TO_VSC_EX,layout);
 return (scan&0xff)|((scan&0xff00)==0xe000?0x80:0);
}
template<bool Wide> class DirectInputKeyboard final:public std::conditional_t<Wide,IDirectInputDevice8W,IDirectInputDevice8A>{
 using EnumCallback=std::conditional_t<Wide,LPDIENUMDEVICEOBJECTSCALLBACKW,LPDIENUMDEVICEOBJECTSCALLBACKA>;
 using ObjectInfo=std::conditional_t<Wide,LPDIDEVICEOBJECTINSTANCEW,LPDIDEVICEOBJECTINSTANCEA>;
 using DeviceInfo=std::conditional_t<Wide,LPDIDEVICEINSTANCEW,LPDIDEVICEINSTANCEA>;
 using EffectCallback=std::conditional_t<Wide,LPDIENUMEFFECTSCALLBACKW,LPDIENUMEFFECTSCALLBACKA>;
 using EffectInfo=std::conditional_t<Wide,LPDIEFFECTINFOW,LPDIEFFECTINFOA>;
 using Text=std::conditional_t<Wide,LPCWSTR,LPCSTR>;
 using ActionFormat=std::conditional_t<Wide,LPDIACTIONFORMATW,LPDIACTIONFORMATA>;
 using ImageInfo=std::conditional_t<Wide,LPDIDEVICEIMAGEINFOHEADERW,LPDIDEVICEIMAGEINFOHEADERA>;
 std::atomic<ULONG> refs{1};std::shared_ptr<MouseFeed> feed;std::mutex mutex;
 bool acquired=false,format=false,overflow=false;DWORD capacity=0;uint64_t cursor=0,generation=0;
 std::deque<DIDEVICEOBJECTDATA> queue;
 void sync(){
  auto& s=feed->state;
  if(generation!=feed->generation){generation=feed->generation;cursor=0;queue.clear();overflow=false;}
  if(cursor<s.keyboardEvents.lastDroppedOrder())overflow=true;
  for(auto& t:s.keyboardEvents.read(256,true))if(t.order>cursor){
   DWORD offset=vkToDik(t.control);
   if(offset&&offset<256&&capacity){
    if(queue.size()==capacity){queue.pop_front();overflow=true;}
    DIDEVICEOBJECTDATA d{};d.dwOfs=offset;d.dwData=static_cast<DWORD>(t.value);d.dwTimeStamp=static_cast<DWORD>(t.timestamp);d.dwSequence=static_cast<DWORD>(t.order);queue.push_back(d);
   }
   cursor=t.order;
  }
 }
public:
 explicit DirectInputKeyboard(std::shared_ptr<MouseFeed> f):feed(std::move(f)){++feed->liveObjects;}
 ~DirectInputKeyboard(){--feed->liveObjects;}
 HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,void** out)override{if(!out)return E_POINTER;*out=nullptr;if(iid!=IID_IUnknown&&iid!=(Wide?IID_IDirectInputDevice8W:IID_IDirectInputDevice8A))return E_NOINTERFACE;*out=this;AddRef();return S_OK;}
 ULONG STDMETHODCALLTYPE AddRef()override{return ++refs;}
 ULONG STDMETHODCALLTYPE Release()override{ULONG n=--refs;if(!n)delete this;return n;}
 HRESULT STDMETHODCALLTYPE GetCapabilities(LPDIDEVCAPS c)override{if(!c||c->dwSize!=sizeof(*c))return DIERR_INVALIDPARAM;*c={};c->dwSize=sizeof(*c);c->dwFlags=DIDC_ATTACHED;c->dwDevType=DI8DEVTYPE_KEYBOARD;c->dwButtons=256;return DI_OK;}
 HRESULT STDMETHODCALLTYPE SetCooperativeLevel(HWND hwnd,DWORD flags)override{std::lock_guard<std::mutex> l(mutex);if(acquired)return DIERR_ACQUIRED;if(!IsWindow(hwnd))return E_HANDLE;if(flags!=(DISCL_BACKGROUND|DISCL_NONEXCLUSIVE))return DIERR_UNSUPPORTED;return DI_OK;}
 HRESULT STDMETHODCALLTYPE SetDataFormat(LPCDIDATAFORMAT f)override{
  std::lock_guard<std::mutex> l(mutex);if(acquired)return DIERR_ACQUIRED;
  if(!f||f->dwSize!=sizeof(*f)||f->dwObjSize!=sizeof(DIOBJECTDATAFORMAT)||!f->rgodf)return DIERR_INVALIDPARAM;
  auto& expected=c_dfDIKeyboard;
  if(f->dwDataSize!=expected.dwDataSize||f->dwFlags!=expected.dwFlags||f->dwNumObjs!=expected.dwNumObjs)return DIERR_UNSUPPORTED;
  for(DWORD i=0;i<f->dwNumObjs;++i){auto&a=f->rgodf[i];auto&b=expected.rgodf[i];if(a.dwOfs!=b.dwOfs||a.dwType!=b.dwType||a.dwFlags!=b.dwFlags||bool(a.pguid)!=bool(b.pguid)||(a.pguid&&*a.pguid!=*b.pguid))return DIERR_UNSUPPORTED;}
  format=true;return DI_OK;
 }
 HRESULT STDMETHODCALLTYPE SetProperty(REFGUID prop,LPCDIPROPHEADER h)override{
  std::lock_guard<std::mutex> l(mutex);if(acquired)return DIERR_ACQUIRED;
  if(!h||h->dwSize!=sizeof(DIPROPDWORD)||h->dwHeaderSize!=sizeof(DIPROPHEADER)||h->dwHow!=DIPH_DEVICE||h->dwObj)return DIERR_INVALIDPARAM;
  if(&prop!=&DIPROP_BUFFERSIZE)return DIERR_UNSUPPORTED;
  DWORD n=reinterpret_cast<const DIPROPDWORD*>(h)->dwData;if(n>256)return DIERR_INVALIDPARAM;capacity=n;queue.clear();overflow=false;return DI_OK;
 }
 HRESULT STDMETHODCALLTYPE GetProperty(REFGUID prop,LPDIPROPHEADER h)override{
  std::lock_guard<std::mutex> l(mutex);if(!h||h->dwSize!=sizeof(DIPROPDWORD)||h->dwHeaderSize!=sizeof(DIPROPHEADER)||h->dwHow!=DIPH_DEVICE||h->dwObj)return DIERR_INVALIDPARAM;
  if(&prop!=&DIPROP_BUFFERSIZE)return DIERR_UNSUPPORTED;reinterpret_cast<DIPROPDWORD*>(h)->dwData=capacity;return DI_OK;
 }
 HRESULT STDMETHODCALLTYPE Acquire()override{std::lock_guard<std::mutex> l(mutex);if(!format)return DIERR_NOTINITIALIZED;if(acquired)return S_FALSE;std::lock_guard<std::mutex> f(feed->mutex);cursor=feed->state.order;generation=feed->generation;queue.clear();overflow=false;acquired=true;return DI_OK;}
 HRESULT STDMETHODCALLTYPE Unacquire()override{std::lock_guard<std::mutex> l(mutex);acquired=false;queue.clear();overflow=false;return DI_OK;}
 HRESULT STDMETHODCALLTYPE Poll()override{std::lock_guard<std::mutex> l(mutex);return acquired?DI_OK:DIERR_NOTACQUIRED;}
 HRESULT STDMETHODCALLTYPE GetDeviceState(DWORD size,LPVOID out)override{
  std::lock_guard<std::mutex> l(mutex);if(!acquired)return DIERR_NOTACQUIRED;if(!out||size!=256)return DIERR_INVALIDPARAM;
  std::lock_guard<std::mutex> f(feed->mutex);sync();std::memset(out,0,256);
  for(int k=0;k<256;++k)if(feed->state.keyboard.keys[k]){DWORD scan=vkToDik(k);if(scan&&scan<256)static_cast<BYTE*>(out)[scan]=0x80;}return DI_OK;
 }
 HRESULT STDMETHODCALLTYPE GetDeviceData(DWORD size,LPDIDEVICEOBJECTDATA out,LPDWORD count,DWORD flags)override{
  std::lock_guard<std::mutex> l(mutex);if(!acquired)return DIERR_NOTACQUIRED;if(!capacity)return DIERR_NOTBUFFERED;if(size!=sizeof(DIDEVICEOBJECTDATA)||!count||(flags&~DIGDD_PEEK))return DIERR_INVALIDPARAM;
  std::lock_guard<std::mutex> f(feed->mutex);sync();DWORD n=static_cast<DWORD>(std::min<size_t>(*count,queue.size()));for(DWORD i=0;i<n;++i)if(out)out[i]=queue[i];*count=n;bool lost=overflow;
  if(!(flags&DIGDD_PEEK)){queue.erase(queue.begin(),queue.begin()+n);overflow=false;}return lost?DI_BUFFEROVERFLOW:DI_OK;
 }
 #include "mouse_unsupported.inc"
};
}