#include "hvi.hpp"
int main(){try{
 hvi::NamedPipeTransport a(1),b(2);hvi::SessionManager sessions;sessions.add(1,a);sessions.add(2,b);hvi::InputRouter router{sessions};
 uint32_t seq=0;
 auto send=[&](uint32_t id,hvi::Kind k,int x=0,int y=0){hvi::Event e;e.session=id;e.sequence=++seq;e.kind=k;e.a=x;e.b=y;if(!router.route(e))throw std::runtime_error("delivery failed");};
 send(1,hvi::Kind::Key,65,1);send(2,hvi::Kind::Key,66,1);
 send(1,hvi::Kind::Relative,10,-3);send(2,hvi::Kind::Absolute,200,300);
 send(1,hvi::Kind::Snapshot);send(2,hvi::Kind::Snapshot);
 send(1,hvi::Kind::Key,65,0);send(2,hvi::Kind::Key,66,0);
 send(1,hvi::Kind::Button,0,1);send(2,hvi::Kind::Button,1,1);
 send(1,hvi::Kind::Reset);send(2,hvi::Kind::Reset);
 send(1,hvi::Kind::Stop);send(2,hvi::Kind::Stop);
 std::cout<<"ACK deliveries="<<router.stats.delivered<<"\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 1;}}