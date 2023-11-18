# pragma once

#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <mavsdk/mavsdk.h>
#include <mavsdk/plugins/action/action.h>
#include <mavsdk/plugins/offboard/offboard.h>
#include <mavsdk/plugins/telemetry/telemetry.h>
#include <vector>
#include <fcntl.h>
#include <future>
#include <chrono>
#include <memory>
#include <cstdint>
#include <mutex>
#include <queue>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <cmath>


struct Message {
   mavsdk::Offboard::PositionNedYaw pos_ned_yaw;
};

struct OriginMsg{
   mavsdk::Telemetry::GpsGlobalOrigin origin_gps;
};

class UdpCommunicator{
public:
   // 构造函数参数：本机ip，本机接收端口(多个)，目标ip地址(多个)，目标端口
   UdpCommunicator(const std::string& , const std::vector<int>& ,const std::vector<std::string>& , const int&);
   ~UdpCommunicator();
   
   void publish(const Message& );
   void stopPublishing();

   void SendGpsOrigin(const OriginMsg& );
   void WaitforAck();
   void WaitforAckLoop(size_t);
   void WaitforAllIps();
   
   void WaitforGpsOrigin();

   void setSocketNodBlocking(int);
   void setSocketBlocking(int);
   void startDynamicSubscribing();
   void dynamicSubscribingLoop(size_t);
   void stopDynamicSubscribing();

private:
   std::string _local_ip;
   int _targetPort, _socketSend;
   struct sockaddr_in _localAddress, _toAddress;

   std::vector<int> _localPorts,_sockets;
   std::vector<std::string> _remoteIps;
   std::vector<std::thread> _dynamicSubscribingThreads;
   std::atomic_bool _stopDynamicSubscribing{false};
   std::atomic_bool _OriginFlag{false};
   std::atomic_bool _stopPublishing{false};
   std::atomic_int _receivedIpCount{0};  // Atomic counter for received IPs
  
   void initialize();
   void closeSocket();
   
};