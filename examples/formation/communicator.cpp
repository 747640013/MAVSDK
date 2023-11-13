#include "Communicator.hpp"


UdpCommunicator::UdpCommunicator(const std::string& local_ip, const std::string& remote_ip,int port): _local_ip(local_ip),_remote_ip(remote_ip), _port(port){
    initialize();
};

UdpCommunicator::~UdpCommunicator(){
    closeSocket();
}

void UdpCommunicator::publish(const Message& mesg){
    sendto(_socket, &mesg, sizeof(mesg),0,(struct sockaddr*)&_remoteAddress, sizeof(_remoteAddress));
}

void UdpCommunicator::subscribe(Message& mesg){
    socklen_t addrLength = sizeof(_remoteAddress);
        recvfrom(_socket, &mesg, sizeof(mesg), 0,
                 (struct sockaddr*)&_remoteAddress, &addrLength);
}

void UdpCommunicator::initialize(){
     // Create UDP socket
    if ((_socket = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Set up remote address
    memset(&_remoteAddress, 0, sizeof(_remoteAddress));
    _remoteAddress.sin_family = AF_INET;
    _remoteAddress.sin_addr.s_addr = inet_addr(_remote_ip.c_str());
    _remoteAddress.sin_port = htons(_port);

    // setup local address
    memset(&_localAddress, 0, sizeof(_localAddress));
    _localAddress.sin_family = AF_INET;
    _localAddress.sin_addr.s_addr = inet_addr(_local_ip.c_str());
    _localAddress.sin_port = htons(_port);

    // Bind the socket
    if (bind(_socket, (const struct sockaddr*)&_localAddress, sizeof(_localAddress)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
        }
}

void UdpCommunicator::closeSocket(){
    close(_socket);
}
