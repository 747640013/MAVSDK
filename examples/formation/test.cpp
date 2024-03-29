#include "Communicator.hpp"

using namespace mavsdk;
using std::chrono::seconds;
using std::this_thread::sleep_for;

void usage(const std::string& bin_name)
{
    std::cerr << "Usage : " << bin_name << " <connection_url>\n"
              << "Connection URL format should be :\n"
              << " For TCP : tcp://[server_host][:server_port]\n"
              << " For UDP : udp://[bind_host][:bind_port]\n"
              << " For Serial : serial:///path/to/serial/dev[:baudrate]\n"
              << "For example, to connect to the simulator use URL: udp://:14540\n";
}

int main(int argc, char** argv){
    if (argc != 2) {
        usage(argv[0]);
        return 1;
    }

    Mavsdk mavsdk;
    ConnectionResult connection_result = mavsdk.add_any_connection(argv[1]);

    if (connection_result != ConnectionResult::Success) {
        std::cerr << "--Connection failed: " << connection_result << '\n';
        return 1;
    }

    auto system = mavsdk.first_autopilot(3.0);
    if (!system) {
        std::cerr << "--Timed out waiting for system\n";
        return 1;
    }

    // Instantiate plugins.
    auto action = Action{system.value()};   //创建匿名对象，然后给对象action赋值
    auto offboard = Offboard{system.value()};
    auto telemetry = Telemetry{system.value()};

    std::vector<std::string> remoteIps={"192.168.100.104"};
    // 对于跟随者，第一个需是用于接收领导者的的端口
    std::vector<int> localPorts={10104};
    UdpCommunicator udp("192.168.100.102",localPorts,remoteIps,10102);

    while (!telemetry.health_all_ok()) {
        std::cout << "--Waiting for system to be ready\n";
        sleep_for(seconds(1));
    }
    std::cout << "--System is ready\n";
    
    std::cout << "--Reading home position in Global coordinates\n";

    const auto res_and_gps_origin = telemetry.get_gps_global_origin();
    if (res_and_gps_origin.first != Telemetry::Result::Success) {
        std::cerr << "--Telemetry failed: " << res_and_gps_origin.first << '\n';
    }
    Telemetry::GpsGlobalOrigin origin = res_and_gps_origin.second;
    std::cerr << "--Origin (lat, lon, alt amsl):\n " << origin << '\n';

    udp.WaitforOriginGps();
    udp.calculate_bais(origin);


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