#ifndef TCP_Server_H
#define TCP_Server_H

#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <thread>
#include <vector>
#include <queue>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <set>                  // 用于记录已断开的客户端套接字,防止客户端重复输出断开连接信息
#include <map>                  // 用于存储客户端接收缓冲区，用于处理粘包和半包问题
#include <jsoncpp/json/json.h>  // 使用JsonCpp库处理JSON数据
#include <iomanip> 
#include <unistd.h>
#include <string.h>
#include "ThreadPool.h"         // 包含线程池头文件
#include "DatabaseManager.h"    // 包含数据库管理器头文件
#include "ProtocolHandler.h"    // 包含协议处理器头文件 
#include "MessageHandler.h"


// 全局线程池是为了处理并发客户端请求，提升服务器性能
extern ThreadPool pool; 


// TCP服务器类封装
class TcpServer{
public:
    TcpServer(const char *ip, const char *port,DatabaseManager* dbManager,
              std::shared_ptr<ProtocolHandler> protocolHandler,MessageHandler* messageHandler);
   
    ~TcpServer();
    
    void listenClientConnect(int n);
    
private:
    int epfd;	
    int listenfd;
    void handleClientData(const struct epoll_event *event);
    void handleClientConnect();

    // 数据库管理器，负责与数据库的交互
    DatabaseManager* dbManager;
    
    // 协议处理器，负责进行协议的解析和构建
    std::shared_ptr<ProtocolHandler> protocolHandler;

    // 消息处理器，负责具体消息的处理逻辑
    MessageHandler* messageHandler;

    // 用于记录已断开的客户端套接字，防止重复输出断开连接信息
    std::set<int> closedFds; 

    // 用于存储客户端接收缓冲区，用于处理粘包和半包问题
    std::map<int, std::vector<char>> recvBuffers; 

    // 互斥锁，保护closedFds和recvBuffers的访问
    std::mutex closedFdsMutex;
};

// TCP服务器构造函数,负责初始化服务器以及相关业务接口
TcpServer::TcpServer(const char *ip, const char *port, 
                     DatabaseManager* dbManager,
                     std::shared_ptr<ProtocolHandler> protocolHandler,
                     MessageHandler* messageHandler) 
    : dbManager(dbManager), protocolHandler(protocolHandler), messageHandler(messageHandler)
{
    // 创建监听套接字
    struct sockaddr_in server_addr;
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if(listenfd < 0){
        perror("创建套接字失败");
        return;
    }
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(port));
    server_addr.sin_addr.s_addr = inet_addr(ip);
    int ret = bind(listenfd, (const struct sockaddr *)&server_addr, sizeof(server_addr));
    if(ret < 0) {
        perror("绑定套接字失败");
        return;
    }
    epfd = epoll_create(1);
    if(epfd < 0) {
        perror("epoll描述符创建失败");
        return;
    }
    
}

// TCP服务器析构函数，用于释放资源
TcpServer::~TcpServer() {
    if(listenfd > 0) {
        close(listenfd);
    }
    if(epfd > 0) {
        close(epfd);
    }
}

// 开始监听客户端连接
void TcpServer::listenClientConnect(int n) {
    int ret = listen(listenfd, n);
    
    if(ret < 0) {
        perror("监听失败");
        return;
    }
    
    struct epoll_event event;
    event.events = EPOLLIN;
    event.data.fd = listenfd;
    
    ret = epoll_ctl(epfd, EPOLL_CTL_ADD, listenfd, &event);
    
    if(ret < 0) {
        perror("添加监听套接字到epoll失败");
        return;
    }
    
    while(true) {
        struct epoll_event events[10];
        int n = epoll_wait(epfd, events, 10, -1);
        
        if(n < 0) {
            perror("epoll等待失败");
            return;
        }
        
        // 遍历所有就绪事件，负责处理客户端连接和数据
        // 使用线程池来处理每个客户端的请求
        // 这样可以提高服务器的并发处理能力
        for(int i = 0; i < n; i++) {
            if(events[i].data.fd == listenfd && events[i].events & EPOLLIN) {
                handleClientConnect();
            } else {
                struct epoll_event ev = events[i]; // 拷贝一份，为了防止生命周期问题
                pool.enqueue([this, ev]() {
                    handleClientData(&ev);
                });
            }
        }
    }
}

// 处理客户端连接
void TcpServer::handleClientConnect() {
    struct sockaddr_in client_addr;
    socklen_t addrlen = sizeof(client_addr);
    int clientfd = accept(listenfd, (struct sockaddr *)&client_addr, &addrlen);
    
    if(clientfd < 0) {
        perror("接受连接失败");
        return;
    }
    
    printf("客户端 IP地址: %s\n", inet_ntoa(client_addr.sin_addr));
    printf("客户端 接口: %d\n", ntohs(client_addr.sin_port));
    
    struct epoll_event event;
    event.events = EPOLLIN;
    event.data.fd = clientfd;
    
    int ret = epoll_ctl(epfd, EPOLL_CTL_ADD, clientfd, &event);
    if(ret < 0) {
        perror("添加客户端套接字到epoll失败");
    }

    // 清理closedFds，防止fd复用导致新连接被误判为已断开
    {
        std::lock_guard<std::mutex> lock(closedFdsMutex);
        closedFds.erase(clientfd);
    }
}

// 处理客户端数据
void TcpServer::handleClientData(const struct epoll_event *event) {
    int clientfd = event->data.fd;

    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    getpeername(clientfd, (struct sockaddr*)&client_addr, &client_addr_len);

    {
        std::lock_guard<std::mutex> lock(closedFdsMutex);
        if (closedFds.count(clientfd)) {
            return;
        }
    }
    
    if(event->events & EPOLLIN) {
        char buf[1024];// 缓冲区大小可以根据实际情况调整

        // 接收数据，clientfd是客户端套接字
        // 使用recv函数接收数据，返回值n是实际接收到的字节数
        // 如果n <= 0，表示客户端断开连接或发生错误
        // 如果n > 0，表示成功接收到数据
        int n = recv(clientfd, buf, sizeof(buf), 0);

        // 进行循环处理接收数据，负责处理粘包和半包问题以及客户端断开连接和数据包解析
        if(n <= 0) {

            // 布尔判断，负责判断是否需要打印客户端断开连接信息
            // 如果客户端连接关闭，插入到closedFds集合中，并判断是否需要打印断开连接信息
            // 使用互斥锁保护closedFds的访问，防止多个线程同时访问导致数据竞争
            // 如果插入成功，说明该客户端之前没有断开连接，设置needPrint为true，表示需要打印断开连接信息
            // 如果插入失败，说明该客户端之前已经断开连接，不需要重复打印，这样可以避免重复输出同一客户端断开连接的信息
            bool needPrint = false;
            {
                std::lock_guard<std::mutex> lock(closedFdsMutex);
                if (closedFds.insert(clientfd).second) {
                    needPrint = true;
                }
            }
           
            // 如果客户端连接关闭，打印客户端断开连接信息
            if (needPrint) {
                std::cout << "客户端--ip:" << inet_ntoa(client_addr.sin_addr) << "--已断开连接" << std::endl;
            }

            // 当客户端断开连接时，从epoll中删除客户端套接字，并关闭连接
            epoll_ctl(epfd, EPOLL_CTL_DEL, clientfd, nullptr);
            close(clientfd);
            recvBuffers.erase(clientfd);
            return;
        }

        // 追加数据到缓冲区，为了处理粘包和半包问题而导致的接收数据不完整
        auto& buffer = recvBuffers[clientfd];
        buffer.insert(buffer.end(), buf, buf + n);

        // 循环处理所有完整包
        while (buffer.size() >= ProtocolHandler::CLIENT_HEADER_SIZE) {
            uint8_t magic = buffer[0];
            uint32_t bodyLen = ntohl(*(uint32_t*)&buffer[1]);
            
            if (magic != 0xAA) {
                std::cout << "协议头错误，fd=" << clientfd << std::endl;
                buffer.clear();
                break;
            }

            size_t totalLen = ProtocolHandler::CLIENT_HEADER_SIZE + bodyLen;
            if (buffer.size() < totalLen) break;

            std::vector<char> onePacket(buffer.begin(), buffer.begin() + totalLen);
            
            // 处理完整包
            std::cout << "数据长度: " << totalLen << std::endl;
            std::cout << "HEX: ";
            for (size_t i = 0; i < totalLen; ++i) {
                printf("%02X ", (unsigned char)onePacket[i]);
            }
            printf("\n");
            
            
            if (messageHandler) {
                messageHandler->handleData(clientfd, onePacket.data(), totalLen, "");
            }
            
            // 从缓冲区中移除已处理的数据
            buffer.erase(buffer.begin(), buffer.begin() + totalLen);
            
        }
    }
}

#endif // TCP_Server_H

