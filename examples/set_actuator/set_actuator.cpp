//
// Simple example to demonstrate how to control actuators (i.e. servos) using MAVSDK.
//

#include <mavsdk/mavsdk.h>
#include <mavsdk/plugins/action/action.h>
#include <mavsdk/plugins/offboard/offboard.h>
#include "mavsdk/core/log.h"
#include <chrono>
#include <cstdint>
#include <iostream>
#include <future>

using namespace mavsdk;

void usage(const std::string& bin_name)
{
    std::cerr << "Usage :" << bin_name << " <connection_url> <actuator_index> <actuator_value>\n"
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

    const std::string connection_url = argv[1];
    // const int index = std::stod(argv[2]);
    // const float value = std::stof(argv[3]);

    Mavsdk mavsdk{Mavsdk::Configuration{Mavsdk::ComponentType::GroundStation}};
    const ConnectionResult connection_result = mavsdk.add_any_connection(connection_url);

    if (connection_result != ConnectionResult::Success) {
        std::cerr << "Connection failed: " << connection_result << '\n';
        return 1;
    }

    LogInfo() << "Waiting to discover system...";
    auto prom = std::promise<std::shared_ptr<System>>{};
    auto fut = prom.get_future();

    // We wait for new systems to be discovered, once we find one that has an
    // autopilot, we decide to use it.
    Mavsdk::NewSystemHandle handle = mavsdk.subscribe_on_new_system([&mavsdk, &prom, &handle]() {
        auto system = mavsdk.systems().back();

        if (system->has_autopilot()) {
            LogInfo() << "Discovered autopilot";

            // Unsubscribe again as we only want to find one system.
            mavsdk.unsubscribe_on_new_system(handle);
            prom.set_value(system);
        }
    });

    // We usually receive heartbeats at 1Hz, therefore we should find a
    // system after around 3 seconds max, surely.
    if (fut.wait_for(std::chrono::seconds(3)) == std::future_status::timeout) {
        LogErr() << "No autopilot found, exiting.";
        return 1;
    }

    // Get discovered system now.
    auto system = fut.get();

    // Instantiate plugins.
    auto action = Action{system};
    auto offboard = Offboard{system}; 

    const auto arm_result = action.arm();
    if (arm_result != Action::Result::Success) {
        LogErr() << "Arming failed: " << arm_result;
        return 1;
    }
    LogWarn() << "Armed";

    Offboard::ActuatorControl control_group{}; 
    Offboard::ActuatorControlGroup flight_control{};

    flight_control.controls.push_back(.1f);  // 
    flight_control.controls.push_back(.0f);  // 
    flight_control.controls.push_back(.0f);  // 
    flight_control.controls.push_back(.0f);  // 

    control_group.groups.push_back(flight_control);

    /*先有数据才能进入offb*/
    offboard.set_actuator_control(control_group);

    /*需要先进入offb模式，进入offb必须有定位数据*/
    Offboard::Result offboard_result = offboard.start();
    if (offboard_result != Offboard::Result::Success) {
        LogErr() << "Offboard start failed: " << offboard_result;
        return false;
    }
    std::cout << "Offboard started\n";
    

    LogInfo() << "Setting actuator...";
    const Offboard::Result set_actuator_result = offboard.set_actuator_control(control_group);

    if (set_actuator_result != Offboard::Result::Success) {
        LogErr() << "Setting actuator failed:" << set_actuator_result;
        return 1;
    }

    return 0;
}
