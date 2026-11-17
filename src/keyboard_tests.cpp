#include "directinput_keyboard.hpp"
#include <iostream>
template<bool W>int test(){
 int errors=0;auto check=[&](bool b,const char* s){if(!b){++errors;std::cerr<<s<<"\n";}};
 auto f=std::make_shared<hvi::MouseFeed>();auto*d=new hvi::DirectInputKeyboard<W>(f);auto*second=new hvi::DirectInputKeyboard<W>(f);
 void*p=nullptr;check(d->QueryInterface(W?IID_IDirectInputDevice8W:IID_IDirectInputDevice8A,&p)==S_OK,"QI");if(p)d->Release();
 check(d->QueryInterface(W?IID_IDirectInputDevice8A:IID_IDirectInputDevice8W,&p)==E_NOINTERFACE&&!p,"wrong QI");
 check(d->QueryInterface(IID_IUnknown,nullptr)==E_POINTER,"null QI");check(d->Acquire()==DIERR_NOTINITIALIZED,"no format");
 check(d->SetDataFormat(nullptr)==DIERR_INVALIDPARAM,"null format");check(d->SetDataFormat(&c_dfDIKeyboard)==DI_OK,"format");second->SetDataFormat(&c_dfDIKeyboard);
 DIPROPDWORD prop{};prop.diph={sizeof(prop),sizeof(DIPROPHEADER),0,DIPH_DEVICE};prop.dwData=256;
 check(d->SetProperty(DIPROP_BUFFERSIZE,&prop.diph)==DI_OK,"buffer");second->SetProperty(DIPROP_BUFFERSIZE,&prop.diph);
 check(d->SetCooperativeLevel(GetDesktopWindow(),DISCL_BACKGROUND|DISCL_NONEXCLUSIVE)==DI_OK,"coop");
 d->Acquire();second->Acquire();check(d->SetDataFormat(&c_dfDIKeyboard)==DIERR_ACQUIRED,"acquired format");
 hvi::Event e;auto key=[&](int k,int down){e.kind=hvi::Kind::Key;e.a=k;e.b=down;f->apply(e);};
 key('W',1);key('W',1);key(VK_LSHIFT,1);key(VK_RSHIFT,1);key(VK_LCONTROL,1);key(VK_RCONTROL,1);key(VK_LMENU,1);key(VK_RMENU,1);
 BYTE state[256]{};check(d->GetDeviceState(255,state)==DIERR_INVALIDPARAM,"size");check(d->GetDeviceState(256,state)==DI_OK&&state[DIK_W]==128&&state[DIK_LSHIFT]&&state[DIK_RSHIFT]&&state[DIK_LCONTROL]&&state[DIK_RCONTROL]&&state[DIK_LMENU]&&state[DIK_RMENU],"DIK modifiers");
 DIDEVICEOBJECTDATA data[256]{};DWORD n=256;check(d->GetDeviceData(sizeof(data[0]),data,&n,DIGDD_PEEK)==DI_OK&&n==7&&data[0].dwOfs==DIK_W,"peek no repeats");
 n=256;check(d->GetDeviceData(sizeof(data[0]),data,&n,0)==DI_OK&&n==7,"drain");n=256;check(second->GetDeviceData(sizeof(data[0]),data,&n,0)==DI_OK&&n==7,"object isolation");
 key(VK_LSHIFT,0);d->GetDeviceState(256,state);check(!state[DIK_LSHIFT]&&state[DIK_RSHIFT]&&f->state.keyboard.keys[VK_SHIFT],"left right release");
 e.kind=hvi::Kind::Reset;f->apply(e);d->GetDeviceState(256,state);check(!state[DIK_W]&&!state[DIK_RSHIFT],"reset");
 f->reset();n=256;check(d->GetDeviceData(sizeof(data[0]),data,&n,0)==DI_OK&&n==0,"generation");
 for(int i=0;i<300;++i)key('A',i%2==0);
 n=256;check(d->GetDeviceData(sizeof(data[0]),data,&n,DIGDD_PEEK)==DI_BUFFEROVERFLOW&&n==256,"feed overflow");
 n=256;check(d->GetDeviceData(sizeof(data[0]),data,&n,0)==DI_BUFFEROVERFLOW,"overflow drain");n=256;check(d->GetDeviceData(sizeof(data[0]),data,&n,0)==DI_OK&&n==0,"overflow cleared");
 d->Unacquire();check(d->GetDeviceState(256,state)==DIERR_NOTACQUIRED,"unacquired");
 check(d->Release()==0,"release");check(second->Release()==0&&f->liveObjects==0,"object count");return errors;
}
int main(){int errors=test<false>()+test<true>();std::cout<<"keyboard errors="<<errors<<"\n";return errors?1:0;}