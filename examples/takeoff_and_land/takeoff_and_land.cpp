//
// Simple example to demonstrate how takeoff and land using MAVSDK.
//

#include <chrono>
#include <cstdint>
#include <mavsdk/mavsdk.h>
#include <mavsdk/plugins/action/action.h>
#include <mavsdk/plugins/telemetry/telemetry.h>
#include <mavsdk/plugins/mavlink_passthrough/mavlink_passthrough.h>
#include <iostream>
#include <future>
#include <memory>
#include <thread>

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

int main(int argc, char** argv)
{
    if (argc != 2) {
        usage(argv[0]);
        return 1;
    }

    Mavsdk mavsdk;
    ConnectionResult connection_result = mavsdk.add_any_connection(argv[1]);

    if (connection_result != ConnectionResult::Success) {
        std::cerr << "Connection failed: " << connection_result << '\n';
        return 1;
    }

    auto system = mavsdk.first_autopilot(3.0);
    if (!system) {
        std::cerr << "Timed out waiting for system\n";
        return 1;
    }

    // Instantiate plugins.
    auto telemetry = Telemetry{system.value()};

    auto action = Action{system.value()};
    auto mavlink_passthrough = MavlinkPassthrough{system.value()};

    // We want to listen to the altitude of the drone at 1 Hz.
    const auto set_rate_result = telemetry.set_rate_position(1.0);
    if (set_rate_result != Telemetry::Result::Success) {
        std::cerr << "Setting rate failed: " << set_rate_result << '\n';
        return 1;
    }

    // Set up callback to monitor altitude while the vehicle is in flight
    telemetry.subscribe_position([](Telemetry::Position position) {
        std::cout << "Altitude: " << position.relative_altitude_m << " m\n";
    });

    // Check until vehicle is ready to arm
    while (telemetry.health_all_ok() != true) {
        std::cout << "Vehicle is getting ready to arm\n";
        sleep_for(seconds(1));
    }
    
    const auto res_and_gps_origin = telemetry.get_gps_global_origin();
    if (res_and_gps_origin.first != Telemetry::Result::Success) {
        std::cerr << "--Telemetry failed: " << res_and_gps_origin.first << '\n';
    }

    Telemetry::GpsGlobalOrigin origin = res_and_gps_origin.second;
    std::cerr << "--Origin (lat, lon, alt amsl):\n " << origin << '\n';


    auto now = std::chrono::system_clock::now();
    // 转换时间点为自纪元以来的微秒数
    auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();
    uint8_t tgt_system = mavlink_passthrough.get_target_sysid();
    const MavlinkPassthrough::Result pass_result = mavlink_passthrough.queue_message([&microseconds,&tgt_system](MavlinkAddress mavlink_address,uint8_t channel)->mavlink_message_t{
        mavlink_message_t msg;
        mavlink_address.system_id=1;
        mavlink_address.component_id=1;
        mavlink_msg_set_gps_global_origin_pack_chan(
            mavlink_address.system_id,
            mavlink_address.component_id,
            MAVLINK_COMM_0,
            &msg,
            tgt_system,
            41.15403*1E7,
            116.2593928*1E7,
            500.982,
            microseconds
        );
        return msg;
    });

    switch(pass_result){
            case MavlinkPassthrough::Result::Unknown: /**< @brief Unknown error. */
                std::cout << "passthrough.send_message()->Unknown" << std::endl;
                break;
            case MavlinkPassthrough::Result::Success: /**< @brief Success. */
                std::cout << "passthrough.send_message()->Success" << std::endl;
                break;
            case MavlinkPassthrough::Result::ConnectionError: /**< @brief Connection error. */
                std::cout << "passthrough.send_message()->ConnectionError" << std::endl;
                break;
            case MavlinkPassthrough::Result::CommandNoSystem: /**< @brief System not available. */
                std::cout << "passthrough.send_message()->CommandNoSystem" << std::endl;
                break;
            case MavlinkPassthrough::Result::CommandBusy: /**< @brief System is busy. */
                std::cout << "passthrough.send_message()->CommandBusy" << std::endl;
                break;
            case MavlinkPassthrough::Result::CommandDenied: /**< @brief Command has been denied. */
                std::cout << "passthrough.send_message()->CommandDenied" << std::endl;
                break;
            case MavlinkPassthrough::Result::CommandUnsupported: /**< @brief Command is not supported. */
                std::cout << "passthrough.send_message()->CommandUnsupported" << std::endl;
                break;
            case MavlinkPassthrough::Result::CommandTimeout: /**< @brief A timeout happened. */
                std::cout << "passthrough.send_message()->CommandTimeout" << std::endl;
                break;
	}

    std::cout<<"Wait for set ekf origin\n";
    sleep_for(seconds(20));
    
    // Arm vehicle
    std::cout << "Arming...\n";
    const Action::Result arm_result = action.arm();

    if (arm_result != Action::Result::Success) {
        std::cerr << "Arming failed: " << arm_result << '\n';
        return 1;
    }

    // Take off
    std::cout << "Taking off...\n";
    const Action::Result takeoff_result = action.takeoff();
    if (takeoff_result != Action::Result::Success) {
        std::cerr << "Takeoff failed: " << takeoff_result << '\n';
        return 1;
    }

    // Let it hover for a bit before landing again.
    sleep_for(seconds(10));

    std::cout << "Landing...\n";
    const Action::Result land_result = action.land();
    if (land_result != Action::Result::Success) {
        std::cerr << "Land failed: " << land_result << '\n';
        return 1;
    }

    // Check if vehicle is still in air
    while (telemetry.in_air()) {
        std::cout << "Vehicle is landing...\n";
        sleep_for(seconds(1));
    }
    std::cout << "Landed!\n";

    // We are relying on auto-disarming but let's keep watching the telemetry for a bit longer.
    sleep_for(seconds(3));
    std::cout << "Finished...\n";

    return 0;
}
