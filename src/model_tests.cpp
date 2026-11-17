#include "hvi.hpp"
#include <thread>
#include "device_adapter.hpp"
int main(){int errors=0;auto check=[&](bool ok,const char* name){if(!ok){std::cerr<<name<<"\n";++errors;}};
 hvi::State s;uint32_t seq=0;auto send=[&](hvi::Kind k,int a=0,int b=0){hvi::Event e;e.kind=k;e.a=a;e.b=b;e.sequence=++seq;check(s.apply(e),"apply");};
 send(hvi::Kind::Key,'A',1);check(s.keyboard.pressed['A']&&s.keyboard.keys['A'],"down");
 size_t size=s.events.size();s.keyboard.nextFrame();send(hvi::Kind::Key,'A',1);check(!s.keyboard.pressed['A']&&s.events.size()==size&&s.keyboard.repeats['A']==1,"repeat no duplicate edge");
 check(s.keyboard.async('A')==0x8001&&s.keyboard.async('A')==0x8000,"async consumed edge");
 send(hvi::Kind::Key,'A',0);check(s.keyboard.released['A']&&!s.keyboard.keys['A'],"up");
 send(hvi::Kind::Key,VK_LSHIFT,1);send(hvi::Kind::Key,VK_RSHIFT,1);send(hvi::Kind::Key,VK_LSHIFT,0);check(s.keyboard.keys[VK_SHIFT],"modifier aggregation");send(hvi::Kind::Key,VK_RSHIFT,0);check(!s.keyboard.keys[VK_SHIFT],"modifier release");
 send(hvi::Kind::Key,VK_CAPITAL,1);send(hvi::Kind::Key,VK_CAPITAL,1);send(hvi::Kind::Key,VK_CAPITAL,0);check(s.keyboard.byte(VK_CAPITAL)==1,"toggle once per down");
 send(hvi::Kind::Absolute,100,-20);send(hvi::Kind::Relative,-5,8);send(hvi::Kind::Wheel,120,-120);send(hvi::Kind::Button,4,1);
 check(s.mouse.x==95&&s.mouse.y==-12&&s.mouse.dx==95&&s.mouse.wheel==120&&s.mouse.horizontalWheel==-120&&s.mouse.buttons[4],"mouse");
 auto peek=s.events.read(2,true);auto read=s.events.read(2);check(peek.size()==read.size()&&peek[0].order==read[0].order,"peek");
 for(int i=0;i<300;i++)send(hvi::Kind::Relative,1,0);check(s.events.size()==256&&s.events.overflowed(),"bounded overflow");
 auto all=s.events.read(256);bool ordered=true;for(size_t i=1;i<all.size();i++)ordered&=all[i].order>all[i-1].order;check(ordered&&!s.events.overflowed(),"ordered drain");
 send(hvi::Kind::Reset);check(!s.mouse.buttons[4]&&!s.keyboard.toggled[VK_CAPITAL]&&s.mouse.x==0,"reset");
 hvi::InputDevice a,b;auto worker=[](hvi::InputDevice&d,int key){for(int i=0;i<1000;i++){hvi::Event e;e.kind=hvi::Kind::Key;e.a=key;e.b=i%2;d.apply(e);(void)d.snapshot();}};
 std::thread t1(worker,std::ref(a),'A'),t2(worker,std::ref(b),'B');t1.join();t2.join();
 check(a.snapshot().keyboard.keys['A']&&!a.snapshot().keyboard.keys['B']&&b.snapshot().keyboard.keys['B'],"parallel isolation");a.disconnect();check(!a.snapshot().keyboard.keys['A'],"disconnect");
 hvi::CooperativeAdapter adapter(7);hvi::Event event;event.session=8;check(!adapter.deliver(event),"adapter target rejection");event.session=7;check(adapter.deliver(event),"adapter delivery");check(!adapter.capabilities().nativeOsInput,"capability honesty");
 std::cout<<"model errors="<<errors<<"\n";return errors?1:0;
}