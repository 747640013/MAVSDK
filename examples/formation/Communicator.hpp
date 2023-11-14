# pragma once

#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <mavsdk/plugins/offboard/offboard.h>
#include <vector>
#include <fcntl.h>
#include <mutex>
#include <queue>
#include <thread>
#include <atomic>
#include <condition_variable>

struct Message {
   mavsdk::Offboard::PositionNedYaw pos_ned_yaw;
};


class UdpCommunicator{

public:
   UdpCommunicator(const std::string& , const std::vector<int>& ,const std::vector<std::string>& , const int& );
   ~UdpCommunicator();
   
   void publish(const Message& );

   void setSocketNodBlocking(int );
   void startDynamicSubscribing();
   void stopDynamicSubscribing();
   void dynamicSubscribingLoop(size_t);
private:
   std::string _local_ip;
   int _targetPort, _socketSend;
   struct sockaddr_in _localAddress, _remoteAddress,_toAddress;
   socklen_t _addrLength = sizeof(_remoteAddress);
   
   std::vector<int> _localPorts,_sockets;
   std::vector<std::string> _remoteIps;
   std::vector<std::thread> _dynamicSubscribingThreads;
   std::atomic_bool _stopDynamicSubscribing{false};
  
   void initialize();
   void closeSocket();
   
};