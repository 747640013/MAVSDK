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

// This is a member function that is only valid for the leader 
bool UdpCommunicator::ConnectToFollowers(){

    int port = _targetPort + 10000;
    struct sockaddr_in _remoteAddress;

    for(const auto ip : _remoteIps){
        int socket_fd = socket(AF_INET, SOCK_STREAM, 0);  
        if(socket_fd < 0){
            perror("Socket creation failed");
            exit(EXIT_FAILURE);
        }
        
        // setup remote address   
        memset(&_remoteAddress, 0, sizeof(_remoteAddress));
        _remoteAddress.sin_family = AF_INET;
        _remoteAddress.sin_addr.s_addr = inet_addr(ip.c_str());
        _remoteAddress.sin_port = htons(port);
        int retryCount = 0;
        while(retryCount < 5){
            if (connect(socket_fd, (struct sockaddr*)&_remoteAddress, sizeof(_remoteAddress)) == 0) {
                break;
            }
            perror("--Error connecting to follower");
            std::this_thread::sleep_for(std::chrono::seconds(5));
            ++retryCount;
        }
        if(retryCount == 5) {
            std::cerr << "--Failed to connect to follower at " << ip << std::endl;
            close(socket_fd);
            return false;
        }
        
        followerSockets.push_back(socket_fd);
        std::cout << "--Connected to follower at:" << ip << std::endl;
    }
    return true;
}

// This is a member function that is only valid for the leader
void UdpCommunicator::SendtoAllFollowers(const OriginMsg& msg){
    if (followerSockets.empty()) {
            std::cerr << "--Not connected to any follower" << std::endl;
            return;
        }
    
    std::cout << "--Sent initial gps msg to all followers: " 
            << "--X=" << msg.origin_gps.latitude_deg << ", "
            << "--Y=" << msg.origin_gps.longitude_deg << ", "
            << "--Z=" << msg.origin_gps.altitude_m << std::endl;
    
    for (int Socket : followerSockets) {
        futures.push_back(std::async(std::launch::async, [this, Socket, &msg]() {
            if (write(Socket, &msg, sizeof(msg)) == -1) {
                perror("Error sending data");
                return;
            }
            OriginMsg recv_msg;
            int bytesReceived = read(Socket, &recv_msg, sizeof(recv_msg));
            if (bytesReceived == -1) {
                perror("Error receiving response");
                return;
            }
            ++_receivedIpCount;
            std::cout << "--Received response from follower:"
            << "--X=" << msg.origin_gps.latitude_deg << ", "
            << "--Y=" << msg.origin_gps.longitude_deg << ", "
            << "--Z=" << msg.origin_gps.altitude_m << std::endl;
        }));
    }
}

// This is a member function that is only valid for the leader
void UdpCommunicator::WaitforAllIps(){
    // Wait for all expected IPs to be received
    while (_receivedIpCount < _remoteIps.size()) {
        // Optionally sleep for a short duration to avoid high CPU usage
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    
    for(auto & future:futures){
        future.wait();
    }
    futures.clear();

    for (int Socket : followerSockets) {
            close(Socket);
        }

    followerSockets.clear();
    std::cout << "--All Tcp connections closed" << std::endl;
}

// This is a member function that is only valid for the follower
void UdpCommunicator::WaitforOriginGps(){
    OriginMsg recv_msg;

    struct sockaddr_in _remoteAddress;
    socklen_t _addrLength = sizeof(_remoteAddress);
    
    if((_socketRecv = socket(AF_INET, SOCK_STREAM,0))<0){
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    int Port = _localPorts[0]+10000;
    memset(&_localAddress, 0, sizeof(_localAddress));
        _localAddress.sin_family = AF_INET;
        _localAddress.sin_addr.s_addr = inet_addr(_local_ip.c_str());
        _localAddress.sin_port = htons(Port);
        
    // Bind the socket
    if (bind(_socketRecv, (const struct sockaddr*)&_localAddress, sizeof(_localAddress)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    if(listen(_socketRecv, 5) == -1) {
            perror("Error listening on server socket");
            close(_socketRecv);
            exit(EXIT_FAILURE);
        }

    std::cout << "--Wait for initial GPS msg on port:"<<Port<< std::endl;

    sockaddr_in clientAddr;
    socklen_t clientAddrLen = sizeof(clientAddr);
    int clientSocket = accept(_socketRecv, (struct sockaddr*)&clientAddr, &clientAddrLen);
    
    if (clientSocket == -1) {
        std::cerr << "Not connected to any client or server" << std::endl;
        exit(EXIT_FAILURE);
    }

    read(clientSocket,&recv_msg,sizeof(recv_msg));
    // Convert sender's IP address to string
    char remoteIp[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &clientAddr.sin_addr, remoteIp, sizeof(remoteIp));
    // Print the received message as needed
    std::cout << "--Received the initial GPS msg from " << remoteIp << ": "<< ntohs(clientAddr.sin_port)<< "\n"
            << "--X=" << recv_msg.origin_gps.latitude_deg << ", "
            << "--Y=" << recv_msg.origin_gps.longitude_deg << ", "
            << "--Z=" << recv_msg.origin_gps.altitude_m << std::endl;
    
    write(clientSocket, &recv_msg, sizeof(recv_msg));
    
    close(clientSocket);
    close(_socketRecv);

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

            std::lock_guard<std::mutex> lock(mtx);
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

        // Optionally sleep for a short duration to avoid high CPU usage 
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

void UdpCommunicator::initialize(){

     // Create multiple sockets for receiving NED coordinate information from multiple drones
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

    // Create a socket for transmitting its own NED  coordinate information to multiple drones
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