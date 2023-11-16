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

// Define a generic message structure
struct Message {
   mavsdk::Offboard::PositionNedYaw pos_ned_yaw;
};


class UdpCommunicator{
public:
   UdpCommunicator(const std::string& , const std::vector<int>& ,const std::vector<std::string>& , const int&);
   ~UdpCommunicator();
   
   void publish(const Message& );
   void stopPublishing();

   void setSocketNodBlocking(int );
   void startDynamicSubscribing();
   void dynamicSubscribingLoop(size_t);
   void stopDynamicSubscribing();
private:
   std::string _local_ip;
   int _targetPort, _socketSend;
   struct sockaddr_in _localAddress, _remoteAddress, _toAddress;
   socklen_t _addrLength = sizeof(_remoteAddress);
   
   std::vector<int> _localPorts,_sockets;
   std::vector<std::string> _remoteIps;
   std::vector<std::thread> _dynamicSubscribingThreads;
   std::atomic_bool _stopDynamicSubscribing{false};
   std::atomic_bool _stopPublishing{false};
  
   void initialize();
   void closeSocket();
   
};