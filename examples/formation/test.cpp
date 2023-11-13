#include "Communicator.hpp"
#include <chrono>
#include <thread>

using std::chrono::seconds;
using std::this_thread::sleep_for;

int main(int argc, char** argv){
    UdpCommunicator udp("192.168.100.14","192.168.100.105",12345);
    Message msg1;
    while(1){
        udp.subscribe(msg1);
        std::cout << "Received PositionNedYaw: "
              << "X=" << msg1.pos_ned_yaw.north_m << ", "
              << "Y=" << msg1.pos_ned_yaw.east_m << ", "
              << "Z=" << msg1.pos_ned_yaw.down_m << ", "
              << "Yaw=" << msg1.pos_ned_yaw.yaw_deg << std::endl;
    }
}