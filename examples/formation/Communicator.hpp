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
   */
   UdpCommunicator(const std::string& , const std::vector<int>& ,const std::vector<std::string>& , const int&);
   ~UdpCommunicator();
   
   bool ConnectToFollowers();
   void SendtoAllFollowers(const mavsdk::Telemetry::GpsGlobalOrigin&);
   void WaitforAllIps();
   
   void WaitforOriginGps();
   
   void Publish(const mavsdk::Offboard::PositionNedYaw&);
   void StopPublishing();

   void SetSocketNodBlocking(int);
   void StartDynamicSubscribing();
   void DynamicSubscribingLoop(size_t); 
   void StopDynamicSubscribing();
   
   std::mutex mtx;
   std::vector<int> followerSockets;
   std::vector<std::future<void>> futures;
private:
   std::string m_local_ip;
   int m_targetPort, m_socketSend,m_socketRecv;
   struct sockaddr_in m_localAddress, m_toAddress;

   std::vector<int> m_localPorts,m_sockets;
   std::vector<std::string> m_remoteIps;
   std::vector<std::thread> m_dynamicSubscribingThreads;
   std::atomic_bool m_stopDynamicSubscribing{false};
   std::atomic_bool m_OriginFlag{false};
   std::atomic_bool m_stopPublishing{false};
   std::atomic_int m_receivedIpCount{0};  // Atomic counter for received IPs
  
   void initialize();
   void closeSocket();
   
};