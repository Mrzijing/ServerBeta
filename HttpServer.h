#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <event2/event.h>
#include <event2/http.h>
#include <event2/buffer.h>
#include "MessageHandler.h"
#include "DatabaseManager.h"
#include "ThreadPool.h"

// 全局线程池,负责处理HTTP请求的并发
extern ThreadPool pool;


// HttpServer类
class HttpServer {
public:
    // 构造函数，传入IP地址、端口号和消息处理器
    HttpServer(const std::string& ip, int port, MessageHandler* handler);
    ~HttpServer();
    void start();

private:
    std::string ip_;// 服务器IP地址
    int port_;// 服务器端口
    struct event_base* base_;// libevent事件基础
    struct evhttp* http_;// libevent HTTP服务器实例
    MessageHandler* messageHandler_;// 消息处理器实例，负责处理HTTP请求

    // 静态回调适配器，负责将HTTP请求分发到MessageHandler的成员函数
    // 这些静态函数是为了适配evhttp的回调机制，实际处理逻辑在MessageHandler中
    // 这里的arg参数是HttpServer实例指针，用于访问成员函数
    static void onRegister(struct evhttp_request* req, void* arg);
    static void onLogin(struct evhttp_request* req, void* arg);            
    static void onChangePassword(struct evhttp_request* req, void* arg);
    static void onAddDevice(struct evhttp_request* req, void* arg);
    static void onDeleteDevice(struct evhttp_request* req, void* arg);
    static void onListDevices(struct evhttp_request* req, void* arg);
    static void onUpdateDevice(struct evhttp_request* req, void* arg);
    // 新增：人脸相关静态回调
    static void onFaceUpsert(struct evhttp_request* req, void* arg);
    static void onFaceDelete(struct evhttp_request* req, void* arg);
    static void onFaceQuery(struct evhttp_request* req, void* arg);
    static void onFaceRecognize(struct evhttp_request* req, void* arg);
    // 新增：房间管理静态回调
    static void onAddRoom(struct evhttp_request* req, void* arg);
    static void onDeleteRoom(struct evhttp_request* req, void* arg);
    static void onListRooms(struct evhttp_request* req, void* arg);
    static void onUpdateRoom(struct evhttp_request* req, void* arg);

    // 工具：分发到MessageHandler
    void handleRequest(struct evhttp_request* req, void (MessageHandler::*func)(struct evhttp_request*, void*));
};

// HTTP服务器构造函数,负责初始化成员变量
HttpServer::HttpServer(const std::string& ip, int port, MessageHandler* handler)
    : ip_(ip), port_(port), base_(nullptr), http_(nullptr), messageHandler_(handler) {}

HttpServer::~HttpServer() {
    if (http_) evhttp_free(http_);
    if (base_) event_base_free(base_);
}


// 启动HTTP服务器
// 1. 创建事件基础
// 2. 创建HTTP服务器实例
// 3. 绑定端口
// 4. 注册路由回调
// 5. 启动事件循环
// 6. 清理资源
void HttpServer::start() {
    base_ = event_base_new();
    http_ = evhttp_new(base_);
    if(evhttp_bind_socket(http_, ip_.c_str(), port_) != 0) {
        std::cerr << "HTTP服务器绑定失败: " << ip_ << ":" << port_ << std::endl;
        return;
    }

    // 注册路由，回调静态成员
    evhttp_set_cb(http_, "/signup", &HttpServer::onRegister, this);
    evhttp_set_cb(http_, "/signin", &HttpServer::onLogin, this);
    evhttp_set_cb(http_, "/resetPassword", &HttpServer::onChangePassword, this);
    evhttp_set_cb(http_, "/addDevice", &HttpServer::onAddDevice, this);
    evhttp_set_cb(http_, "/deleteDevice", &HttpServer::onDeleteDevice, this);
    evhttp_set_cb(http_, "/listDevices", &HttpServer::onListDevices, this);
    evhttp_set_cb(http_, "/updateDevice", &HttpServer::onUpdateDevice, this);

    // 人脸相关 HTTP 路由（action 在 body 中：faceUpsert/faceDelete/faceQuery/faceRecognize）
    evhttp_set_cb(http_, "/faceUpsert", &HttpServer::onFaceUpsert, this);
    evhttp_set_cb(http_, "/faceDelete", &HttpServer::onFaceDelete, this);
    evhttp_set_cb(http_, "/faceQuery", &HttpServer::onFaceQuery, this);
    evhttp_set_cb(http_, "/faceRecognize", &HttpServer::onFaceRecognize, this);

    // 房间管理路由
    evhttp_set_cb(http_, "/addRoom",    &HttpServer::onAddRoom, this);
    evhttp_set_cb(http_, "/deleteRoom", &HttpServer::onDeleteRoom, this);
    evhttp_set_cb(http_, "/listRooms",  &HttpServer::onListRooms, this);
    evhttp_set_cb(http_, "/updateRoom", &HttpServer::onUpdateRoom, this);

    // 启动事件循环,负责处理HTTP请求
    if (!base_) {
        std::cerr << "创建事件基础失败" << std::endl;
        return;
    }
    
    // 启动HTTP服务器
    if (!http_) {
        std::cerr << "创建HTTP服务器失败" << std::endl;
        event_base_free(base_);
        return;
    }

    // 启动事件循环，处理HTTP请求,这里会阻塞，直到服务器停止或发生错误
    if (event_base_dispatch(base_) < 0) {
        std::cerr << "启动HTTP服务器失败" << std::endl;
    }
    
    // 释放资源
    evhttp_free(http_);
    event_base_free(base_);

}

// 静态适配器，分发到成员函数,负责将请求分发到MessageHandler，处理具体业务逻辑
void HttpServer::handleRequest(struct evhttp_request* req, void (MessageHandler::*func)(struct evhttp_request*, void*)) {
    // 使用线程池处理请求，避免阻塞主线程
    pool.enqueue([this, req, func]() {
        (messageHandler_->*func)(req, nullptr);
    });
}

// 处理注册请求,通过线程池异步处理，避免阻塞主线程
// 这里的req是evhttp_request指针，arg是HttpServer实例指针
void HttpServer::onRegister(struct evhttp_request* req, void* arg) {
    // 直接调用MessageHandler的处理函数
    // arg是HttpServer实例指针，static_cast转换为HttpServer类型
    // 这里的arg可以传递给MessageHandler的处理函数
    // 这样可以避免在每个回调函数中重复获取HttpServer实例
    HttpServer* server = static_cast<HttpServer*>(arg);

    // 用线程池处理，避免阻塞主线程
    // 通过线程池异步处理注册请求
    // 这里的req是evhttp_request指针，arg是HttpServer实例指针
    // 通过lambda表达式捕获server和req，调用MessageHandler的处理函数
    // 这样可以将请求处理逻辑放在MessageHandler中，保持HttpServer的简洁性
    pool.enqueue([server, req]() {
        server->messageHandler_->handleRegisterHttp(req, nullptr);
    });
}

// 处理登录请求
void HttpServer::onLogin(struct evhttp_request* req, void* arg) {
    // 直接调用MessageHandler的处理函数
    HttpServer* server = static_cast<HttpServer*>(arg);
    // 用线程池处理，避免阻塞主线程
    pool.enqueue([server, req]() {
        server->messageHandler_->handleLoginHttp(req, nullptr);
    });
}

// 处理修改密码请求
void HttpServer::onChangePassword(struct evhttp_request* req, void* arg) {
   // 直接调用MessageHandler的处理函数
    HttpServer* server = static_cast<HttpServer*>(arg);
    // 用线程池处理，避免阻塞主线程
    pool.enqueue([server, req]() {
        server->messageHandler_->handleChangePasswordHttp(req, nullptr);
    });
}

// 房间回调实现（用线程池分发到 MessageHandler）
void HttpServer::onAddRoom(struct evhttp_request* req, void* arg) {
    HttpServer* server = static_cast<HttpServer*>(arg);
    pool.enqueue([server, req]() {
        server->messageHandler_->handleAddRoomHttp(req, nullptr);
    });
}

void HttpServer::onDeleteRoom(struct evhttp_request* req, void* arg) {
    HttpServer* server = static_cast<HttpServer*>(arg);
    pool.enqueue([server, req]() {
        server->messageHandler_->handleDeleteRoomHttp(req, nullptr);
    });
}

void HttpServer::onListRooms(struct evhttp_request* req, void* arg) {
    HttpServer* server = static_cast<HttpServer*>(arg);
    pool.enqueue([server, req]() {
        server->messageHandler_->handleListRoomsHttp(req, nullptr);
    });
}

void HttpServer::onUpdateRoom(struct evhttp_request* req, void* arg) {
    HttpServer* server = static_cast<HttpServer*>(arg);
    pool.enqueue([server, req]() {
        server->messageHandler_->handleUpdateRoomHttp(req, nullptr);
    });
}

// 处理添加设备请求
void HttpServer::onAddDevice(struct evhttp_request* req, void* arg) {
    // 直接调用MessageHandler的处理函数
    HttpServer* server = static_cast<HttpServer*>(arg);
    // 用线程池处理，避免阻塞主线程
    pool.enqueue([server, req]() {
        server->messageHandler_->handleAddDeviceHttp(req, nullptr);
    });
}

// 处理删除设备请求
void HttpServer::onDeleteDevice(struct evhttp_request* req, void* arg) {
    // 直接调用MessageHandler的处理函数
    HttpServer* server = static_cast<HttpServer*>(arg);
    // 用线程池处理，避免阻塞主线程
    pool.enqueue([server, req]() {
        server->messageHandler_->handleDeleteDeviceHttp(req, nullptr);
    });
}

// 处理设备列表请求
void HttpServer::onListDevices(struct evhttp_request* req, void* arg) {
    // 直接调用MessageHandler的处理函数
    HttpServer* server = static_cast<HttpServer*>(arg);
    // 用线程池处理，避免阻塞主线程
    pool.enqueue([server, req]() {
        server->messageHandler_->handleListDevicesHttp(req, nullptr);
    });
}


// 处理更新设备请求
void HttpServer::onUpdateDevice(struct evhttp_request* req, void* arg) {
    // 直接调用MessageHandler的处理函数
    HttpServer* server = static_cast<HttpServer*>(arg);
    // 用线程池处理，避免阻塞主线程
    pool.enqueue([server, req]() {
        server->messageHandler_->handleUpdateDeviceHttp(req, nullptr);
    });
}   


// 人脸回调静态适配器（用线程池分发到 MessageHandler）
void HttpServer::onFaceUpsert(struct evhttp_request* req, void* arg) {
    HttpServer* server = static_cast<HttpServer*>(arg);
    pool.enqueue([server, req]() {
        // 注意：人脸方法签名只有一个参数
        server->messageHandler_->handleFaceUpsertHttp(req);
    });
}
void HttpServer::onFaceDelete(struct evhttp_request* req, void* arg) {
    HttpServer* server = static_cast<HttpServer*>(arg);
    pool.enqueue([server, req]() {
        server->messageHandler_->handleFaceDeleteHttp(req);
    });
}
void HttpServer::onFaceQuery(struct evhttp_request* req, void* arg) {
    HttpServer* server = static_cast<HttpServer*>(arg);
    pool.enqueue([server, req]() {
        server->messageHandler_->handleFaceQueryHttp(req);
    });
}
void HttpServer::onFaceRecognize(struct evhttp_request* req, void* arg) {
    HttpServer* server = static_cast<HttpServer*>(arg);
    pool.enqueue([server, req]() {
        server->messageHandler_->handleFaceRecognizeHttp(req);
    });
}



// ...可扩展其它业务...


#endif // HTTP_SERVER_H