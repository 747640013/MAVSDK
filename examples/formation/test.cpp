#include "Communicator.hpp"

using namespace mavsdk;
using std::chrono::seconds;
using std::this_thread::sleep_for;

int main(int argc, char** argv){
    std::vector<std::string> remoteIps={"192.168.100.101"};
    std::vector<int> localPorts={10101};
    UdpCommunicator udp("192.168.100.104",localPorts,remoteIps,10104);
    int count = 0;
    Message msg1;
    msg1.pos_ned_yaw={0,0,-3,0};
    auto publishFuture = std::async(std::launch::async, [&udp,&msg1]() {udp.publish(msg1);}); 
    auto subscribeFuture = std::async(std::launch::async,[&udp](){udp.startDynamicSubscribing();});
    
    sleep_for(seconds(60));
    udp.stopDynamicSubscribing();
    udp.stopPublishing();

    publishFuture.wait();
    subscribeFuture.wait();
    
} 