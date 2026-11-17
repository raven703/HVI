#include "hvi.hpp"
struct Local: hvi::InputAdapter {hvi::State state;bool deliver(const hvi::Event&e)override{return state.apply(e);}};
int main(){int errors=0;auto check=[&](bool ok){if(!ok)++errors;};
 Local a,b;hvi::SessionManager sm;check(sm.add(1,a));check(sm.add(2,b));check(!sm.add(1,b));hvi::InputRouter r{sm};hvi::Event e;e.session=1;e.kind=hvi::Kind::Key;e.a=65;e.b=1;
 check(r.route(e));check(a.state.keyboard.keys[65]);check(!b.state.keyboard.keys[65]);e.b=0;check(r.route(e));check(!a.state.keyboard.keys[65]);
 e.kind=hvi::Kind::Relative;e.a=-12;e.b=9;check(r.route(e));check(a.state.mouse.x==-12&&a.state.mouse.y==9);check(b.state.mouse.x==0);
 e.kind=hvi::Kind::Key;e.a=256;check(!r.route(e));e.kind=hvi::Kind::Reset;check(r.route(e));check(a.state.mouse.x==0);
 sm.remove(1);check(!r.route(e));check(sm.add(1,a));e.session=99;check(!r.route(e));
 std::cout<<"errors="<<errors<<"\n";return errors?1:0;
}