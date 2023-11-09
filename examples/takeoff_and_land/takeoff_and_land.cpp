//
// Simple example to demonstrate how takeoff and land using MAVSDK.
//

#include <chrono>
#include <cstdint>
#include <mavsdk/mavsdk.h>
#include <mavsdk/plugins/action/action.h>
#include <mavsdk/plugins/offboard/offboard.h>
#include <mavsdk/plugins/telemetry/telemetry.h>
#include <iostream>
#include <future>
#include <memory>
#include <thread>
#include <cmath>

using namespace mavsdk;
using std::chrono::seconds;
using std::this_thread::sleep_for;


// 定义一个函数，输入为Telemetry对象的引用，输出为PositionNedYaw对象
mavsdk::Offboard::PositionNedYaw get_current_position_ned_yaw(mavsdk::Telemetry& telemetry) 
{
    // 获取当前无人机的NED坐标和偏航角
    auto ned_origin = telemetry.position_velocity_ned();
    auto yaw_origin = telemetry.heading();
    float yaw = std::round(yaw_origin.heading_deg*100)/100;     // round对小数点后1位进行四舍五入

    // 输出无人机当前的NED坐标和偏航角
    std::cout << "--Now:"
              << " N:" << ned_origin.position.north_m<<"\t"
              << "E:" << ned_origin.position.east_m<<"\t"
              << "D:" << ned_origin.position.down_m<<"\t"
              << "Y:" << yaw << std::endl;

    // 创建并返回一个PositionNedYaw对象，代表无人机当前的NED坐标和偏航角
    return mavsdk::Offboard::PositionNedYaw{
        ned_origin.position.north_m,
        ned_origin.position.east_m,
        ned_origin.position.down_m,
        yaw
    };
}

//  在NED坐标系下使用offboard控制,程序运行无误返回值为1
bool offb_ctrl_ned(mavsdk::Offboard& offboard, mavsdk::Telemetry& telemetry)
{
    std::cout << "--Starting offboard velocity control in NED coordinates\n";   
    
    
    //  // 设置无人机位置更新频率为1hz
    // const auto set_rate_result = telemetry.set_rate_position(1.0);
    // if (set_rate_result != Telemetry::Result::Success) {
    //     std::cerr << "Setting rate failed: " << set_rate_result << '\n';
    //     return 1;
    // }

    // // Set up callback to monitor altitude while the vehicle is in flight
    // telemetry.subscribe_position_velocity_ned([](Telemetry::PositionVelocityNed position) {
    //     std::cout << "Height: " << position.position.down_m<< " m\n";
    // });

    // 需要先设置初值，否则无法切入offboard模式
    const Offboard::VelocityNedYaw stay{};
    offboard.set_velocity_ned(stay);

    Offboard::Result offboard_result = offboard.start();
    if (offboard_result != Offboard::Result::Success) {
        std::cerr << "--Offboard start failed: " << offboard_result << '\n';
        return false;
    }
    std::cout << "--offboard started\n";
    Offboard::PositionNedYaw stay0=get_current_position_ned_yaw(telemetry);
    Offboard::PositionNedYaw position_target[]{
        {0,0,-3,0},
        {3,0,0,0},
        {-3,0,0,0}
    };

    for(int i=0;i<3;i++){
      stay0.down_m += position_target[i].down_m;
      stay0.north_m += position_target[i].north_m;
      offboard.set_position_ned(stay0);
      sleep_for(seconds(10));
    }
    get_current_position_ned_yaw(telemetry);
    sleep_for(seconds(2));
    std::cout<<"--offboard ended\n";
    return true;
}

void usage(const std::string& bin_name)
{
    std::cerr << "Usage : " << bin_name << " <connection_url>\n"
              << "Connection URL format should be :\n"
              << " For TCP : tcp://[server_host][:server_port]\n"
              << " For UDP : udp://[bind_host][:bind_port]\n"
              << " For Serial : serial:///path/to/serial/dev[:baudrate]\n"
              << " For example, to connect to the simulator use URL: udp://:14540\n";
}

int main(int argc, char** argv)
{
    if (argc != 2) {
        usage(argv[0]);
        return 1;
    }

    Mavsdk mavsdk;  //创建mavsdk类对象
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
    auto telemetry = Telemetry{system.value()}; //使用构造函数创建对象，并结合列表初始化对象
    auto action = Action{system.value()};
    auto offboard= Offboard{system.value()};

    // Check until vehicle is ready to arm
    while (!telemetry.health_all_ok() ) {
        std::cout << "--Vehicle is getting ready to arm\n";
        sleep_for(seconds(1));
    }

    // Arm vehicle
    std::cout << "--Arming...\n";
    const Action::Result arm_result = action.arm();

    

    if (arm_result != Action::Result::Success) {
        std::cerr << "--Arming failed: " << arm_result << '\n';
        return 1;
    }
    
    // sleep_for(seconds(10)); //only for helicopter
    // Take off
    std::cout << "--Taking off...\n";
    const Action::Result takeoff_result = action.takeoff();
    if (takeoff_result != Action::Result::Success) {
        std::cerr << "--Takeoff failed: " << takeoff_result << '\n';
        return 1;
    }

    auto in_air_promise = std::promise<void>{};
    auto in_air_future = in_air_promise.get_future(); 
    Telemetry::LandedStateHandle handle = telemetry.subscribe_landed_state(
        [&telemetry, &in_air_promise, &handle](Telemetry::LandedState state) {
            // lambda表达式  
            if (state == Telemetry::LandedState::InAir) {
                std::cout << "--Taking off has finished.\n";
                telemetry.unsubscribe_landed_state(handle);
                in_air_promise.set_value();
            }
        });
    in_air_future.wait_for(seconds(10));
    if (in_air_future.wait_for(seconds(3)) == std::future_status::timeout) {
        std::cerr << "--Takeoff timed out.\n";
        return 1;
    }

    sleep_for(seconds(10));

    //  使用NED坐标系控制
    if (!offb_ctrl_ned(offboard,telemetry)){
        return 1;
    }

    sleep_for(seconds(3));
    // Let it hover for a bit before landing again.

    std::cout << "--Landing...\n";
    const Action::Result land_result = action.land();
    if (land_result != Action::Result::Success) {
        std::cerr << "--Land failed: " << land_result << '\n';
        return 1;
    }
    
    // Check if vehicle is still in air
    while (telemetry.in_air()) {
        std::cout << "--Vehicle is landing...\n";
        sleep_for(seconds(1));
    }
    std::cout << "--Landed!\n";

    // We are relying on auto-disarming but let's keep watching the telemetry for a bit longer.
    sleep_for(seconds(3));
    std::cout << "--Finished...\n";

    return 0;
}
