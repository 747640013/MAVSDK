#include "Communicator.hpp"


UdpCommunicator::UdpCommunicator(const std::string& local_ip, const std::vector<int>& local_ports,
    const std::vector<std::string>& remote_ips, const int& target_port): _local_ip(local_ip),
    _localPorts(local_ports), _remoteIps(remote_ips), _targetPort(target_port)
{
    initialize();
}

UdpCommunicator::~UdpCommunicator(){
    closeSocket();
}

void UdpCommunicator::Publish(const Message& msg){
    while(!_stopPublishing){
         for (size_t i=0;i< _remoteIps.size();++i){
            _toAddress.sin_addr.s_addr = inet_addr(_remoteIps[i].c_str());   
            sendto(_socketSend, &msg, sizeof(msg),0,(struct sockaddr*)&_toAddress, sizeof(_toAddress));
        }
        //  Sleep for 0.5 seconds to give up CPU resources, i.e. 2hz
        std::this_thread::sleep_for(std::chrono::milliseconds(500));  
    }  
}

void UdpCommunicator::StopPublishing(){
    _stopPublishing = true;
}

void UdpCommunicator::SendOriginGps(const OriginMsg& msg){

    std::cout<< "--Start sending  the initial gps msg" <<std::endl;
    while(!_OriginFlag){
        for (size_t i=0;i< _remoteIps.size();++i){
            _toAddress.sin_addr.s_addr = inet_addr(_remoteIps[i].c_str());   
            sendto(_socketSend, &msg, sizeof(msg),0,(struct sockaddr*)&_toAddress, sizeof(_toAddress));
        }
        //  Sleep for 0.5 seconds to give up CPU resources, i.e.2hz
        std::this_thread::sleep_for(std::chrono::milliseconds(500));    
    }
    std::cout<< "--Stop sending"<<std::endl;  
}

void UdpCommunicator::WaitforAck(){
    // Create a thread for each local port
    for (size_t i = 0; i < _localPorts.size(); ++i) {
        _dynamicSubscribingThreads.emplace_back([this, i] { WaitforAckLoop(i); });
    }
}

void UdpCommunicator::WaitforAckLoop(size_t socketIndex){
    while (!_OriginFlag) {
        OriginMsg recv_msg;
        
        struct sockaddr_in _remoteAddress;
        socklen_t _addrLength = sizeof(_remoteAddress);
        // Non-blocking receive
        ssize_t bytesReceived = recvfrom(_sockets[socketIndex], &recv_msg, sizeof(recv_msg),
                                            MSG_DONTWAIT, (struct sockaddr*)&_remoteAddress, &_addrLength);

        if (bytesReceived > 0) {

            // Convert sender's IP address to string
            char remoteIp[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &_remoteAddress.sin_addr, remoteIp, sizeof(remoteIp));
            // Process the received message as needed
            std::cout << "Received message from " << remoteIp << ": "<< ntohs(_remoteAddress.sin_port)<< "\n"
                        << "X=" << recv_msg.origin_gps.latitude_deg << ", "
                        << "Y=" << recv_msg.origin_gps.longitude_deg << ", "
                        << "Z=" << recv_msg.origin_gps.altitude_m << std::endl; 

            ++_receivedIpCount;  // Atomic counter for received IPs; 
            break;   
        }else if (bytesReceived < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            // Handle error (excluding EAGAIN or EWOULDBLOCK, which indicate no data available)
            perror("recvfrom failed");
            break;
        }

        // Optionally sleep for a short duration to avoid high CPU usage 
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

void UdpCommunicator::WaitforAllIps(){
    // Wait for all expected IPs to be received
    while (_receivedIpCount < _remoteIps.size()) {
        // Optionally sleep for a short duration to avoid high CPU usage
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    _OriginFlag = true;
     // Join all dynamic subscribing threads
    for (std::thread& thread : _dynamicSubscribingThreads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    _dynamicSubscribingThreads.clear();
}

void UdpCommunicator::WaitforOriginGps(){
    OriginMsg recv_msg;

    struct sockaddr_in _remoteAddress;
    socklen_t _addrLength = sizeof(_remoteAddress);
    
    /********         ！！！      ********
    Because the blocking model of sockets is used here, 
    the socket must be modified to blocking mode*/
    SetSocketBlocking(_sockets[0]);

    std::cout << "--Wait for Origin GPS msg"<< std::endl;

    // blocking receive
    recvfrom(_sockets[0], &recv_msg, sizeof(recv_msg),
                0, (struct sockaddr*)&_remoteAddress, &_addrLength);

    // Convert sender's IP address to string
    char remoteIp[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &_remoteAddress.sin_addr, remoteIp, sizeof(remoteIp));
    // Process the received message as needed
    std::cout << "--Received GPS msg from " << remoteIp << ": "<< ntohs(_remoteAddress.sin_port)<< "\n"
            << "X=" << recv_msg.origin_gps.latitude_deg << ", "
            << "Y=" << recv_msg.origin_gps.longitude_deg << ", "
            << "Z=" << recv_msg.origin_gps.altitude_m << std::endl;
    
    _remoteAddress.sin_port = htons(_targetPort);
    sendto(_socketSend, &recv_msg, sizeof(recv_msg),0,(struct sockaddr*)&_remoteAddress, sizeof(_remoteAddress));

    SetSocketNodBlocking(_sockets[0]);
}

// Start dynamic subscribing to messages on different local ports
void UdpCommunicator::StartDynamicSubscribing(){
    _stopDynamicSubscribing = false;

    // Create a thread for each local port
    for (size_t i = 0; i < _localPorts.size(); ++i) {
        _dynamicSubscribingThreads.emplace_back([this, i] { DynamicSubscribingLoop(i); });
    }
}

void UdpCommunicator::DynamicSubscribingLoop(size_t socketIndex) {
    while (!_stopDynamicSubscribing) {
        Message receivedMessage;

        struct sockaddr_in _remoteAddress;
        socklen_t _addrLength = sizeof(_remoteAddress);
        // Non-blocking receive
        ssize_t bytesReceived = recvfrom(_sockets[socketIndex], &receivedMessage, sizeof(receivedMessage),
                                            MSG_DONTWAIT, (struct sockaddr*)&_remoteAddress, &_addrLength);

        if (bytesReceived > 0) {

            // Convert sender's IP address to string
            char remoteIp[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &_remoteAddress.sin_addr, remoteIp, sizeof(remoteIp));
            // Process the received message as needed
            std::cout << "Received message from " << remoteIp << ": "<< ntohs(_remoteAddress.sin_port)<< "\n"
                        << "X=" << receivedMessage.pos_ned_yaw.north_m << ", "
                        << "Y=" << receivedMessage.pos_ned_yaw.east_m << ", "
                        << "Z=" << receivedMessage.pos_ned_yaw.down_m << ", "
                        << "Yaw=" << receivedMessage.pos_ned_yaw.yaw_deg << std::endl;      
        }else if (bytesReceived < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            // Handle error (excluding EAGAIN or EWOULDBLOCK, which indicate no data available)
            perror("recvfrom failed");
            break;
        }

        // Optionally sleep for a short duration to avoid high CPU usage in the absence of messages
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
}

void UdpCommunicator::StopDynamicSubscribing() {
    _stopDynamicSubscribing = true;

    // Join all dynamic subscribing threads
    for (std::thread& thread : _dynamicSubscribingThreads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    _dynamicSubscribingThreads.clear();
}

void UdpCommunicator::SetSocketNodBlocking(int socket_fd){
    int flags = fcntl(socket_fd, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl F_GETFL failed");
        exit(EXIT_FAILURE);
    }

    if (fcntl(socket_fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("fcntl F_SETFL failed");
        exit(EXIT_FAILURE);
    }
}

void UdpCommunicator::SetSocketBlocking(int socket_fd){
    int flags = fcntl(socket_fd, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl F_GETFL failed");
        exit(EXIT_FAILURE);
    }

    if (fcntl(socket_fd, F_SETFL, flags & ~O_NONBLOCK) == -1) {
        perror("fcntl F_SETFL failed");
        exit(EXIT_FAILURE);
    }
}

void UdpCommunicator::initialize(){

     // Create server sockets
    for(int localPort : _localPorts){
        int socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
        if(socket_fd<0){
            perror("Socket creation failed");
            exit(EXIT_FAILURE);
        }
        
        // set the socket to non-blocking mode
        SetSocketNodBlocking(socket_fd);

        // setup local address   
        memset(&_localAddress, 0, sizeof(_localAddress));
        _localAddress.sin_family = AF_INET;
        _localAddress.sin_addr.s_addr = inet_addr(_local_ip.c_str());
        _localAddress.sin_port = htons(localPort);
        
        // Bind the socket
        if (bind(socket_fd, (const struct sockaddr*)&_localAddress, sizeof(_localAddress)) < 0) {
            perror("Bind failed");
            exit(EXIT_FAILURE);
        }
        _sockets.push_back(socket_fd);
    }

    // Create a send socket
    if((_socketSend = socket(AF_INET, SOCK_DGRAM,0))<0){
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    } 

    // Set up  target address
    memset(&_toAddress, 0, sizeof(_toAddress));
    _toAddress.sin_family = AF_INET;
    _toAddress.sin_port = htons(_targetPort);

}

void UdpCommunicator::closeSocket(){
    for(int socket_fd : _sockets)
        close(socket_fd);
    close(_socketSend);
}