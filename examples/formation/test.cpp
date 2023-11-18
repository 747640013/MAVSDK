#include "Communicator.hpp"


using std::chrono::seconds;
using std::this_thread::sleep_for;

int main(int argc, char** argv){
    std::vector<std::string> remoteIps={"192.168.100.104"};
    std::vector<int> localPorts={10104};
    UdpCommunicator udp("192.168.100.102",localPorts,remoteIps,10102);

    udp.WaitforGpsOrigin();
    Message msg1;
    msg1.pos_ned_yaw={0,0,-3,0};
    auto publishFuture = std::async(std::launch::async, [&udp,&msg1](){udp.publish(msg1);});

    auto subscribeFuture = std::async(std::launch::async,[&udp](){udp.startDynamicSubscribing();});
    
    sleep_for(seconds(30));
    udp.stopDynamicSubscribing();
    udp.stopPublishing();

    publishFuture.wait();
    subscribeFuture.wait();

} 