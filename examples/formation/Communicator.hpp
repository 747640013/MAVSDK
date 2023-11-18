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
/**
 * @brief A class for multi-UAV communication
 * 
 * The leader's initial GPS information and each drone's own NED coordinates 
 * are sent to multiple on-board computers with known IP addresses
*/
class UdpCommunicator{
public:
   /**
    * @brief the constructor
    * 
    * @param param1 The IPv4 address of this on-board computer
    * @param param2 Multiple ports are used to receive data,the first of which is the port for receiving data from the leader
    * @param param3 IPv4 address of each on-board computer
    * @param param3 Port used to send ned coordinate point information
    * @param param4 Port used to receive initial gps information from the leader
   */
   UdpCommunicator(const std::string& , const std::vector<int>& ,const std::vector<std::string>& , const int&, const int&);
   ~UdpCommunicator();
   
   void Publish(const Message&);
   void StopPublishing();

   void SendOriginGps(const OriginMsg&);
   void WaitforAck();
   void WaitforAckLoop(size_t);
   void WaitforAllIps();
   
   void WaitforOriginGps();

   void SetSocketNodBlocking(int);
   void StartDynamicSubscribing();
   void DynamicSubscribingLoop(size_t);
   void StopDynamicSubscribing();

private:
   std::string _local_ip;
   int _targetPort, _GpsPort, _socketSend,_socketRecv;
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