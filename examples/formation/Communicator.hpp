# pragma once

#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <mavsdk/plugins/offboard/offboard.h>

struct Message {
   mavsdk::Offboard::PositionNedYaw pos_ned_yaw;
};

class UdpCommunicator{

public:
   UdpCommunicator(const std::string& local_ip,const std::string& remote_ip, int port);
   ~UdpCommunicator();
   void publish(const Message& mesg);
   void subscribe(Message& mesg);

private:
   std::string _local_ip,_remote_ip;
   int _port, _socket;
   struct sockaddr_in _localAddress, _remoteAddress;
   void initialize();
   void closeSocket();
};