#include "Communicator.hpp"

/*
构造函数
参数1 本机ipv4地址
参数2 本机多个接收端口
参数3 多个跟随者ipv4地址
参数4 本机发送端口
参数5,6 相对偏差
*/
UdpCommunicator::UdpCommunicator(const std::string& local_ip, const std::vector<int>& local_ports,
    const std::vector<std::string>& remote_ips, const int& target_port): m_local_ip(local_ip),
    m_localPorts(local_ports), m_remoteIps(remote_ips), m_targetPort(target_port),delta_x(0.0),delta_y(0.0)
{
    initialize();
}

UdpCommunicator::~UdpCommunicator(){
    closeSocket();
}

// This is a member function that is only valid for the leader 
/*
作为客户端向各个跟随者tcp连接请求，建立socket，（需要跟随者先开启服务端）
若跟随者服务端未开启，5秒后重试，共尝试5次
与所有跟随者成功建立通信返回true
若有一个失败返回false
*/
bool UdpCommunicator::ConnectToFollowers(){

    int port = m_targetPort + 10000;
    struct sockaddr_in _remoteAddress;

    for(const auto ip : m_remoteIps){
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
void UdpCommunicator::SendtoAllFollowers(const mavsdk::Telemetry::GpsGlobalOrigin& msg){
    if (followerSockets.empty()) {
            std::cerr << "--Not connected to any follower" << std::endl;
            return;
        }
    
    std::cout << "--Sent initial gps msg to all followers: " 
            << "--X=" << msg.latitude_deg << ", "
            << "--Y=" << msg.longitude_deg << ", "
            << "--Z=" << msg.altitude_m << std::endl;
    
    for (int Socket : followerSockets) {
        futures.push_back(std::async(std::launch::async, [this, Socket, &msg]() {
            if (write(Socket, &msg, sizeof(msg)) == -1) {
                perror("Error sending data");
                return;
            }
            mavsdk::Telemetry::GpsGlobalOrigin recv_msg;
            int bytesReceived = read(Socket, &recv_msg, sizeof(recv_msg));
            if (bytesReceived == -1) {
                perror("Error receiving response");
                return;
            }
            ++m_receivedIpCount;
            std::cout << "--Received response from follower:"
            << "--X=" << msg.latitude_deg << ", "
            << "--Y=" << msg.longitude_deg << ", "
            << "--Z=" << msg.altitude_m << std::endl;
        }));
    }
}

// This is a member function that is only valid for the leader
/*
所有跟随都收到领导者初始GPS信息后，清理socket，清理线程
*/
void UdpCommunicator::WaitforAllIps(){
    // Wait for all expected IPs to be received
    while (m_receivedIpCount < m_remoteIps.size()) {
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
/*
跟随者创建服务端tcp，接收初始gps信息并向领导者回复后，关闭并清理socket
*/
void UdpCommunicator::WaitforOriginGps()
{
    struct sockaddr_in _remoteAddress;
    socklen_t _addrLength = sizeof(_remoteAddress);
    
    if((m_socketRecv = socket(AF_INET, SOCK_STREAM,0))<0){
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    int Port = m_localPorts[0]+10000;
    memset(&m_localAddress, 0, sizeof(m_localAddress));
        m_localAddress.sin_family = AF_INET;
        m_localAddress.sin_addr.s_addr = inet_addr(m_local_ip.c_str());
        m_localAddress.sin_port = htons(Port);
        
    // Bind the socket
    if (bind(m_socketRecv, (const struct sockaddr*)&m_localAddress, sizeof(m_localAddress)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    if(listen(m_socketRecv, 5) == -1) {
            perror("Error listening on server socket");
            close(m_socketRecv);
            exit(EXIT_FAILURE);
        }

    std::cout << "--Wait for initial GPS msg on port:"<<Port<< std::endl;

    sockaddr_in clientAddr;
    socklen_t clientAddrLen = sizeof(clientAddr);
    int clientSocket = accept(m_socketRecv, (struct sockaddr*)&clientAddr, &clientAddrLen);
    
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
            << "--X=" << recv_msg.latitude_deg << ", "
            << "--Y=" << recv_msg.longitude_deg << ", "
            << "--Z=" << recv_msg.altitude_m << std::endl;
    
    write(clientSocket, &recv_msg, sizeof(recv_msg));
    
    close(clientSocket);
    close(m_socketRecv);

}

void UdpCommunicator::calculate_bais(mavsdk::Telemetry::GpsGlobalOrigin& msg)
{
	printf("--Leader's init gps info %3.10f,%3.10f,%3.10f\n",recv_msg.latitude_deg,recv_msg.longitude_deg,recv_msg.altitude_m);
	printf("--Local init gps %3.10f,%3.10f,%3.10f\n",msg.latitude_deg,msg.longitude_deg,msg.altitude_m);

    GeographicLib::LocalCartesian geo_converter;
    geo_converter.Reset(recv_msg.latitude_deg,recv_msg.longitude_deg,recv_msg.altitude_m);
	double x1, y1,z1;
	geo_converter.Forward(recv_msg.latitude_deg, recv_msg.longitude_deg, recv_msg.altitude_m, y1, x1, z1);  // ENU坐标

	double x2, y2,z2;
    geo_converter.Forward(msg.latitude_deg, msg.longitude_deg, msg.altitude_m, y2, x2, z2);
	delta_x = round((x1 - x2)*10)/10;
	delta_y = round((y1 - y2)*10)/10;
	printf("--dx=%f,dy=%f\n",delta_x,delta_y);

}


void UdpCommunicator::Publish(const mavsdk::Offboard::PositionNedYaw& msg){
    while(!m_stopPublishing){
         for (size_t i=0;i< m_remoteIps.size();++i){
            m_toAddress.sin_addr.s_addr = inet_addr(m_remoteIps[i].c_str());   
            sendto(m_socketSend, &msg, sizeof(msg),0,(struct sockaddr*)&m_toAddress, sizeof(m_toAddress));
        }
        //  Sleep for 0.5 seconds to give up CPU resources, i.e. 2hz
        std::this_thread::sleep_for(std::chrono::milliseconds(500));  
    }  
}

void UdpCommunicator::StopPublishing(){
    m_stopPublishing = true;
}

// Start dynamic subscribing to messages on different local ports
void UdpCommunicator::StartDynamicSubscribing(){
    m_stopDynamicSubscribing = false;

    // Create a thread for each local port
    for (size_t i = 0; i < m_localPorts.size(); ++i) {
        m_dynamicSubscribingThreads.emplace_back([this, i] { DynamicSubscribingLoop(i); });
    }
}

void UdpCommunicator::DynamicSubscribingLoop(size_t socketIndex) {
    while (!m_stopDynamicSubscribing) {
        mavsdk::Offboard::PositionNedYaw  receivedMessage;

        struct sockaddr_in _remoteAddress;
        socklen_t _addrLength = sizeof(_remoteAddress);
        // Non-blocking receive
        ssize_t bytesReceived = recvfrom(m_sockets[socketIndex], &receivedMessage, sizeof(receivedMessage),
                                            MSG_DONTWAIT, (struct sockaddr*)&_remoteAddress, &_addrLength);

        if (bytesReceived > 0) {

            // Convert sender's IP address to string
            char remoteIp[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &_remoteAddress.sin_addr, remoteIp, sizeof(remoteIp));
            // Process the received message as needed

            std::lock_guard<std::mutex> lock(mtx);
            std::cout << "Received message from " << remoteIp << ": "<< ntohs(_remoteAddress.sin_port)<< "\n"
                        << "X=" << receivedMessage.north_m << ", "
                        << "Y=" << receivedMessage.east_m << ", "
                        << "Z=" << receivedMessage.down_m << ", "
                        << "Yaw=" << receivedMessage.yaw_deg << std::endl;      
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
    m_stopDynamicSubscribing = true;

    // Join all dynamic subscribing threads
    for (std::thread& thread : m_dynamicSubscribingThreads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    m_dynamicSubscribingThreads.clear();
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
    for(int localPort : m_localPorts){
        int socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
        if(socket_fd<0){
            perror("Socket creation failed");
            exit(EXIT_FAILURE);
        }
        
        // set the socket to non-blocking mode
        SetSocketNodBlocking(socket_fd);

        // setup local address   
        memset(&m_localAddress, 0, sizeof(m_localAddress));
        m_localAddress.sin_family = AF_INET;
        m_localAddress.sin_addr.s_addr = inet_addr(m_local_ip.c_str());
        m_localAddress.sin_port = htons(localPort);
        
        // Bind the socket
        if (bind(socket_fd, (const struct sockaddr*)&m_localAddress, sizeof(m_localAddress)) < 0) {
            perror("Bind failed");
            exit(EXIT_FAILURE);
        }
        m_sockets.push_back(socket_fd);
    }

    // Create a socket for transmitting its own NED  coordinate information to multiple drones
    if((m_socketSend = socket(AF_INET, SOCK_DGRAM,0))<0){
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    } 

    // Set up  target address
    memset(&m_toAddress, 0, sizeof(m_toAddress));
    m_toAddress.sin_family = AF_INET;
    m_toAddress.sin_port = htons(m_targetPort);

}

void UdpCommunicator::closeSocket(){
    for(int socket_fd : m_sockets)
        close(socket_fd);
    close(m_socketSend);
}