#include "Communicator.hpp"


using std::chrono::seconds;
using std::this_thread::sleep_for;

int main(int argc, char** argv){
    std::vector<std::string> remoteIps={"192.168.100.104","192.168.100.101"};
    // The port for receiving data on this computer, the first one must be the port for receiving leader data.
    std::vector<int> localPorts={10104,10101};
    UdpCommunicator udp("192.168.100.102",localPorts,remoteIps,10102);

    udp.WaitforOriginGps();
    Message msg1;
    msg1.pos_ned_yaw={1,0,-3,0};
    auto publishFuture = std::async(std::launch::async, [&udp,&msg1](){udp.Publish(msg1);});

    auto subscribeFuture = std::async(std::launch::async,[&udp](){udp.StartDynamicSubscribing();});
    
    sleep_for(seconds(30));
    udp.StopDynamicSubscribing();
    udp.StopPublishing();

    publishFuture.wait();
    subscribeFuture.wait();

} 