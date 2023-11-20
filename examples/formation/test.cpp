#include "Communicator.hpp"

using namespace mavsdk;
using std::chrono::seconds;
using std::this_thread::sleep_for;

int main(int argc, char** argv){
    std::vector<std::string> remoteIps={"192.168.100.102","192.168.100.101"};
    std::vector<int> localPorts={10102,10101}; 
    UdpCommunicator udp("192.168.100.104",localPorts,remoteIps,10104);
    OriginMsg msg2;
    msg2.origin_gps = {1,2,3}; 
    if(udp.ConnectToFollowers()){
        udp.SendtoAllFollowers(msg2);
        udp.WaitforAllIps();
    }

    Message msg1;
    msg1.pos_ned_yaw={0,0,-3,0};
    auto publishFuture = std::async(std::launch::async, [&udp,&msg1]() {udp.Publish(msg1);}); 
    auto subscribeFuture = std::async(std::launch::async,[&udp](){udp.StartDynamicSubscribing();});
    
    sleep_for(seconds(30));
    udp.StopDynamicSubscribing();
    udp.StopPublishing();

    publishFuture.wait();
    subscribeFuture.wait();
    
} 