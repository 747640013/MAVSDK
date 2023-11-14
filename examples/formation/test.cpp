#include "Communicator.hpp"


using std::chrono::seconds;
using std::this_thread::sleep_for;

int main(int argc, char** argv){
    std::vector<std::string> remoteIps={"192.168.100.105","192.168.100.103"};
    std::vector<int> localPorts={10103,10105};
    UdpCommunicator udp("192.168.100.11",localPorts,remoteIps,10011);

    // udp.startDynamicSubscribing();
    // std::this_thread::sleep_for(std::chrono::seconds(30));
    // udp.stopDynamicSubscribing();
    Message msg1;
    msg1.pos_ned_yaw={0,0,-3,0};
    while (1)
    {
        udp.publish(msg1);
        sleep_for(seconds(1));
        msg1.pos_ned_yaw.down_m +=1;
    }
    
} 