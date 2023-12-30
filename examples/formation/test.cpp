#include "Communicator.hpp"

using namespace mavsdk;
using std::chrono::seconds;
using std::this_thread::sleep_for;

int main(int argc, char** argv){
    std::vector<std::string> remoteIps={"192.168.100.104"};
    // 对于跟随者，第一个需是用于接收领导者的的端口
    std::vector<int> localPorts={10104};
    UdpCommunicator udp("192.168.100.102",localPorts,remoteIps,10102);

    udp.WaitforOriginGps();
    Offboard::PositionNedYaw msg1;
    msg1 = {1,0,-3,0};
    auto publishFuture = std::async(std::launch::async, [&udp,&msg1](){udp.Publish(msg1);});

    auto subscribeFuture = std::async(std::launch::async,[&udp](){udp.StartDynamicSubscribing();});
    
    sleep_for(seconds(30));
    udp.StopDynamicSubscribing();
    udp.StopPublishing();

    publishFuture.wait();
    subscribeFuture.wait();

} 