#ifndef MESSAGE_HANDLER_H
#define MESSAGE_HANDLER_H

#include <mosquitto.h>
#include <jsoncpp/json/json.h>
#include <event2/event.h>
#include <event2/http.h>
#include <event2/buffer.h>
#include <sys/socket.h>
#include <iostream>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
#include "DatabaseManager.h"
#include "ProtocolHandler.h"
#include "MqttServer.h"
#include "FaceManager.h"  // 新增：为使用 FaceManager::返回结构体，需要完整定义


class FaceManager; // 前向声明,目的是为了在MessageHandler中使用FaceManager

// 消息处理器，只负责业务逻辑，用于处理客户端发送的消息
// 解析客户端发送的JSON数据，验证字段，调用DatabaseManager进行数据库操作
// 构建响应消息并发送给客户端
// 这里的ProtocolHandler只负责协议的解析和构建，不涉及具体业务逻辑，便于维护和扩展
// 具体业务逻辑由MessageHandler类处理，MessageHandler类负责解析和处理客户端发送的数据
// MessageHandler类使用ProtocolHandler进行协议的解析和构建，确保协议解析和业务逻辑分离
class MessageHandler {
public:
    MessageHandler(DatabaseManager* dbManager, std::shared_ptr<ProtocolHandler> protocolHandler,MqttServer* mqttServer);
    void handleData(int clientfd, const char* buf, int len, const std::string& client_ip);

    // Tcp处理请求
    void handleRegister(int clientfd, const Json::Value& data);
    void handleLogin(int clientfd, const Json::Value& data);
    void handleChangePassword(int clientfd, const Json::Value& data);
    void handleAddRoom(int clientfd, const Json::Value& data);
    void handleDeleteRoom(int clientfd, const Json::Value& data);
    void handleListRooms(int clientfd, const Json::Value& data);
    void handleUpdateRoom(int clientfd, const Json::Value& data);
    void handleAddDevice(int clientfd, const Json::Value& data);
    void handleDeleteDevice(int clientfd, const Json::Value& data);
    void handleListDevices(int clientfd, const Json::Value& data);
    void handleUpdateDevice(int clientfd, const Json::Value& data);
    void sendError(int clientfd, const std::string& errorMsg);

    // HTTP处理请求
    void handleRegisterHttp(struct evhttp_request* req, void* arg);
    void handleLoginHttp(struct evhttp_request* req, void* arg);
    void handleChangePasswordHttp(struct evhttp_request* req, void* arg);
    void handleAddRoomHttp(struct evhttp_request* req, void* arg);
    void handleDeleteRoomHttp(struct evhttp_request* req, void* arg);
    void handleListRoomsHttp(struct evhttp_request* req, void* arg);
    void handleUpdateRoomHttp(struct evhttp_request* req, void* arg);
    void handleAddDeviceHttp(struct evhttp_request* req, void* arg);
    void handleDeleteDeviceHttp(struct evhttp_request* req, void* arg);
    void handleListDevicesHttp(struct evhttp_request* req, void* arg);
    void handleUpdateDeviceHttp(struct evhttp_request* req, void* arg);
    void sendErrorHttp(struct evhttp_request* req, const std::string& errorMsg);
    // HTTP 人脸识别处理请求
    void handleFaceUpsertHttp(struct evhttp_request* req);
    void handleFaceDeleteHttp(struct evhttp_request* req);
    void handleFaceQueryHttp(struct evhttp_request* req);
    void handleFaceRecognizeHttp(struct evhttp_request* req);
    
    // MQTT处理请求
    MqttServer* mqttServer; // 添加对MqttServer的引用
    void handleRegisterMqtt(const std::string& topic, const Json::Value& data);
    void handleLoginMqtt(const std::string& topic, const Json::Value& data);
    void handleChangePasswordMqtt(const std::string& topic, const Json::Value& data);
    void handleAddRoomMqtt(const std::string& topic, const Json::Value& data);
    void handleDeleteRoomMqtt(const std::string& topic, const Json::Value& data);
    void handleListRoomsMqtt(const std::string& topic, const Json::Value& data);
    void handleUpdateRoomMqtt(const std::string& topic, const Json::Value& data);
    void handleAddDeviceMqtt(const std::string& topic, const Json::Value& data);
    void handleDeleteDeviceMqtt(const std::string& topic, const Json::Value& data);
    void handleListDevicesMqtt(const std::string& topic, const Json::Value& data);
    void handleUpdateDeviceMqtt(const std::string& topic, const Json::Value& data);
    void publishMqttResponse(const std::string& topic, const Json::Value& resp);
    void handleControlDeviceMqtt(const std::string& topic, const Json::Value& data);
    // MQTT 人脸识别处理请求
    void handleFaceUpsertMqtt(const std::string& topic, const Json::Value& data);
    void handleFaceDeleteMqtt(const std::string& topic, const Json::Value& data);
    void handleFaceQueryMqtt(const std::string& topic, const Json::Value& data);
    void handleFaceRecognizeMqtt(const std::string& topic, const Json::Value& data);
    
    // 数据库管理器，负责与数据库的交互
    DatabaseManager* dbManager;
    
    // 协议处理器，负责解析和构建协议
    // 使用智能指针管理ProtocolHandler的生命周期
    std::shared_ptr<ProtocolHandler> protocolHandler;

    // 注入 FaceManager 进行人脸识别相关操作
    void setFaceManager(FaceManager* mgr) { faceMgr_ = mgr; }

private:
    FaceManager* faceMgr_ = nullptr;
};

// 消息处理器构造函数
MessageHandler::MessageHandler(DatabaseManager* dbManager, std::shared_ptr<ProtocolHandler> protocolHandler, MqttServer* mqttServer)
    : dbManager(dbManager), protocolHandler(protocolHandler), mqttServer(mqttServer) {}

// 消息处理函数，负责解析和处理客户端发送的数据
void MessageHandler::handleData(int clientfd, const char* buf, int length, const std::string& client_ip) {
    // 基本长度检查
    if (length < ProtocolHandler::CLIENT_HEADER_SIZE) {
        sendError(clientfd, "数据长度不足，无法解析协议头！");
        return;
    }
    
    // 一次性完成所有解析和验证
    MessageType msgType;
    Json::Value jsonData;
    
    if (!protocolHandler->parseMessage(buf, length, msgType, jsonData)) {
        sendError(clientfd, "错误的json格式！");
        return;
    }
    
    // 根据消息类型直接分发处理
    switch (msgType) {
        // 用户管理
        case MSG_REGISTER:
            handleRegister(clientfd, jsonData);
            break;
        case MSG_LOGIN:
            handleLogin(clientfd, jsonData);
            break;
        case MSG_CHANGE_PASSWORD:
            handleChangePassword(clientfd, jsonData);
            break;
        // 房间管理 
        case MSG_ADD_ROOM:
            handleAddRoom(clientfd, jsonData);
            break;
        case MSG_DELETE_ROOM:
            handleDeleteRoom(clientfd, jsonData);
            break;
        case MSG_LIST_ROOMS:
            handleListRooms(clientfd, jsonData);
            break;
        case MSG_UPDATE_ROOM:
            handleUpdateRoom(clientfd, jsonData);
            break;
        //设备管理
        case MSG_ADD_DEVICE:
            handleAddDevice(clientfd, jsonData);
            break;
        case MSG_DELETE_DEVICE:
            handleDeleteDevice(clientfd, jsonData);
            break;
        case MSG_LIST_DEVICES:
            handleListDevices(clientfd, jsonData);
            break;
        case MSG_UPDATE_DEVICE:
            handleUpdateDevice(clientfd, jsonData);
            break;
        default:
            sendError(clientfd, "不支持的消息类型!");
            break;
    }
}


//Tcp处理注册请求
void MessageHandler::handleRegister(int clientfd, const Json::Value& data) {
    // 直接使用解析出来的字段
    std::string username = data["username"].asString();
    std::string password = data["password"].asString();

    // 验证用户名和密码
    std::string responseMsg;
    bool success = dbManager->registerUser(username, password, responseMsg);

    //如果成功返回json字符与客户端进行响应
    std::string result;
    if(success) {
        std::cout << "用户" << username << "注册成功！" << std::endl;
        Json::Value resp;
        resp["action"] = "signup";
        resp["status"] = "success";
        Json::FastWriter writer;
        result = writer.write(resp);
    }else{
        std::cout << "用户" << username << "注册失败！" << std::endl;
        std::cout << "失败原因：" << responseMsg << std::endl;
        result = "注册失败:"+responseMsg;
    }

    send(clientfd, result.c_str(), result.length(), 0);
    
}


//Tcp处理登录请求
void MessageHandler::handleLogin(int clientfd, const Json::Value& data) {
    std::string username = data["username"].asString();
    std::string password = data["password"].asString();

    // 验证用户
    std::string responseMsg;
    bool success = dbManager->verifyUser(username, password, responseMsg);
    
    // 如果成功返回json字符与客户端进行响应
    std::string result;
    if(success) {
        std::cout << "用户" << username << "登陆成功！" << std::endl;
        Json::Value resp;
        resp["action"] = "signin";
        resp["status"] = "success";
        Json::FastWriter writer;
        result = writer.write(resp);
    }else{
        std::cout << "用户" << username << "登陆失败！" << std::endl;
        std::cout << "失败原因：" << responseMsg << std::endl;
        result = "登陆失败:"+responseMsg;
    }
    send(clientfd, result.c_str(), result.length(), 0);
}


//Tcp处理修改密码请求
void MessageHandler::handleChangePassword(int clientfd, const Json::Value& data) {
    // 数据经过ProtocolHandler验证过，直接使用解析出来的字段
    std::string username = data["username"].asString();
    std::string newPassword = data["password"].asString();
        
    // 更新密码
    std::string responseMsg;
    bool success = dbManager->updatePassword(username, newPassword, responseMsg);

    // 如果成功返回json字符与客户端进行响应
    std::string result;
    if (success) {
        std::cerr << "密码修改成功，用户名: " << username << std::endl;
        Json::Value resp;
        resp["action"] = "resetPassword";
        resp["status"] = "success";
        Json::FastWriter writer;
        result = writer.write(resp);
    }
    else {   
        std::cout << "密码修改失败，用户名: " << username << std::endl;
        std::cerr << "失败原因: " << responseMsg << std::endl;
        result = "密码修改失败：" + responseMsg;
    }

    send(clientfd, result.c_str(), result.length(), 0);
    
}

// TCP处理添加房间请求
void MessageHandler::handleAddRoom(int clientfd, const Json::Value& data) {
    std::string roomType = data["room_type"].asString();
    std::string roomName = data["room_name"].asString();
    std::string responseMsg;
    bool success = dbManager->addRoom(roomType, roomName, responseMsg);

    Json::Value resp;
    resp["action"] = "addRoom";
    resp["status"] = success ? "success" : "fail";
    resp["message"] = responseMsg;
    Json::FastWriter writer;
    std::string result = writer.write(resp);
    send(clientfd, result.c_str(), result.length(), 0);
}

// TCP处理删除房间请求
void MessageHandler::handleDeleteRoom(int clientfd, const Json::Value& data) {
    int roomId = data["room_id"].asInt();
    std::string responseMsg;
    bool success = dbManager->deleteRoom(roomId, responseMsg);

    Json::Value resp;
    resp["action"] = "deleteRoom";
    resp["status"] = success ? "success" : "fail";
    resp["message"] = responseMsg;
    Json::FastWriter writer;
    std::string result = writer.write(resp);
    send(clientfd, result.c_str(), result.length(), 0);
}

// TCP处理查询房间请求
void MessageHandler::handleListRooms(int clientfd, const Json::Value& data) {
    Json::Value rooms;
    std::string responseMsg;
    bool success = dbManager->listRooms(rooms, responseMsg);

    Json::Value resp;
    resp["action"] = "listRooms";
    resp["status"] = success ? "success" : "fail";
    resp["rooms"] = rooms;
    resp["message"] = responseMsg;
    Json::FastWriter writer;
    std::string result = writer.write(resp);
    send(clientfd, result.c_str(), result.length(), 0);
}

// TCP处理更新房间请求
void MessageHandler::handleUpdateRoom(int clientfd, const Json::Value& data) {
    int roomId = data["room_id"].asInt();
    std::string roomType = data["room_type"].asString();
    std::string roomName = data["room_name"].asString();
    std::string responseMsg;
    bool success = dbManager->updateRoom(roomId, roomType, roomName, responseMsg);

    Json::Value resp;
    resp["action"] = "updateRoom";
    resp["status"] = success ? "success" : "fail";
    resp["message"] = responseMsg;
    Json::FastWriter writer;
    std::string result = writer.write(resp);
    send(clientfd, result.c_str(), result.length(), 0);
}

//TCP处理添加设备请求
void MessageHandler::handleAddDevice(int clientfd, const Json::Value& data) {
    //直接读取字段
    std::string deviceName = data["device_name"].asString();    
    std::string deviceType = data["device_type"].asString();
    int roomId = data["room_id"].asInt();
    std::string image = data["image"].asString(); 

    // 添加设备
    std::string responseMsg;
    bool success = dbManager->addDevice(deviceName, deviceType, roomId, image, responseMsg);

    // 如果成功返回json字符与客户端进行响应
    std::string result;
    if (success) {
        std::cout << "设备" << deviceName << "添加成功！" << std::endl;
        Json::Value resp;
        resp["action"] = "addDevice";
        resp["status"] = "success";
        Json::FastWriter writer;
        result = writer.write(resp);
    } else {
        std::cout << "设备" << deviceName << "添加失败！" << std::endl;
        std::cout << "失败原因：" << responseMsg << std::endl;
        result = "添加设备失败：" + responseMsg;
    }

    send(clientfd, result.c_str(), result.length(), 0);
}

// TCP处理删除设备请求
void MessageHandler::handleDeleteDevice(int clientfd, const Json::Value& data) {
    // 解析设备ID,检查id字段
    std::string deviceIdStr = data["device_id"].asString();  
    int deviceId = std::stoi(deviceIdStr); 

    // 删除设备
    std::string responseMsg;
    bool success = dbManager->deleteDevice(deviceId, responseMsg);

    // 如果成功返回json字符与客户端进行响应
    std::string result;
    if (success) {
        std::cout << "设备ID " << deviceId << " 删除成功！" << std::endl;
        Json::Value resp;
        resp["action"] = "deleteDevice";
        resp["status"] = "success";
        Json::FastWriter writer;
        result = writer.write(resp);
    } else {
        std::cout << "设备ID " << deviceId << " 删除失败！" << std::endl;
        std::cout << "失败原因：" << responseMsg << std::endl;
        result = "删除设备失败：" + responseMsg;
    }

    send(clientfd, result.c_str(), result.length(), 0);
}

// TCP处理查询设备请求
void MessageHandler::handleListDevices(int clientfd, const Json::Value& data) {
    Json::Value devices;
    std::string responseMsg;
    bool success = dbManager->listDevices(devices, responseMsg);

    // 如果成功返回json字符与客户端进行响应
    std::string result;
    if (success) {
        std::cout << "设备列表查询成功！" << std::endl;
        Json::Value resp;
        resp["action"] = "listDevices";
        resp["status"] = "success";
        resp["devices"] = devices;  // 将设备列表添加到响应中
        Json::FastWriter writer;
        result = writer.write(resp);
    } else {
        std::cout << "设备列表查询失败！" << std::endl;
        std::cout << "失败原因：" << responseMsg << std::endl;
        result = "查询设备列表失败：" + responseMsg;
    }

    send(clientfd, result.c_str(), result.length(), 0);
}

// TCP处理修改设备请求
void MessageHandler::handleUpdateDevice(int clientfd, const Json::Value& data) {
    int deviceId = data["device_id"].asInt();
    std::string deviceName = data["device_name"].asString();
    std::string deviceType = data["device_type"].asString();
    int roomId = data["room_id"].asInt();
    std::string image = data["image"].asString();

    // 更新设备
    std::string responseMsg;
    bool success = dbManager->updateDevice(deviceId, deviceName, deviceType, roomId, image, responseMsg);

    // 如果成功返回json字符与客户端进行响应
    std::string result;
    if (success) {
        std::cout << "设备ID " << deviceId << " 更新成功！" << std::endl;
        Json::Value resp;
        resp["action"] = "updateDevice";
        resp["status"] = "success";
        Json::FastWriter writer;
        result = writer.write(resp);
    } else {
        std::cout << "设备ID " << deviceId << " 更新失败！" << std::endl;
        std::cout << "失败原因：" << responseMsg << std::endl;
        result = "更新设备失败：" + responseMsg;
    }

    send(clientfd, result.c_str(), result.length(), 0);
}


// TCP发送错误消息给客户端
void MessageHandler::sendError(int clientfd, const std::string& errorMsg) {
    send(clientfd, errorMsg.c_str(), errorMsg.length(), 0);
}


//HTTP处理注册请求
void MessageHandler::handleRegisterHttp(struct evhttp_request* req, void* arg) {
    // 1. 读取HTTP请求体
    struct evbuffer* buf = evhttp_request_get_input_buffer(req);
    size_t len = evbuffer_get_length(buf);
    std::vector<char> data(len);
    evbuffer_copyout(buf, data.data(), len);

    // 2. 协议解析和字段校验全部交给ProtocolHandler
    Json::Value jsonData;
    MessageType msgType;
    if (!protocolHandler->parseHttpJson(data.data(), len, msgType, jsonData)) {
        evhttp_send_reply(req, 400, "错误的请求", nullptr);
        return;
    }

    // 3. 业务处理
    std::string username = jsonData["username"].asString();
    std::string password = jsonData["password"].asString();
    std::string msg;
    bool success = dbManager->registerUser(username, password, msg);

    // 4. 构建HTTP响应
    struct evbuffer* out = evbuffer_new();
    // 创建输出缓冲区失败
    if (!out) {
        evhttp_send_reply(req, 500, "网络服务错误：创建输出缓冲区失败", nullptr);
        return;
    }
    // 根据注册结果构建响应
    if (success) {
        std::cout << "用户" << username << "注册成功！" << std::endl;
        // 设置响应头为JSON格式
        evhttp_add_header(evhttp_request_get_output_headers(req), "Content-Type", "application/json");// 函数作用：设置HTTP响应头
        Json::Value resp;
        resp["action"] = "signup";
        resp["status"] = "success";
        Json::FastWriter writer;
        std::string jsonStr = writer.write(resp);
        evbuffer_add(out, jsonStr.c_str(), jsonStr.length());
    } else {
        std::cout << "用户" << username << "注册失败！" << std::endl;
        std::cout << "失败原因：" << msg << std::endl;
        
        // 设置响应头为纯文本格式
        evhttp_add_header(evhttp_request_get_output_headers(req), "Content-Type", "text/plain");
        
        // 将错误消息添加到输出缓冲区，如果msg为空，则返回"fail"，否则返回具体的错误消息
        // 这里的msg是数据库操作返回的错误信息
        evbuffer_add_printf(out, "%s", msg.empty() ? "fail" : msg.c_str());
    }
    evhttp_send_reply(req, 200, "OK", out);
    evbuffer_free(out);
}

//HTTP处理登录请求
void MessageHandler::handleLoginHttp(struct evhttp_request* req, void* arg) {
    // 1. 读取HTTP请求体
    struct evbuffer* buf = evhttp_request_get_input_buffer(req);
    size_t len = evbuffer_get_length(buf);
    std::vector<char> data(len);
    evbuffer_copyout(buf, data.data(), len);

    // 2. 协议解析和字段校验全部交给ProtocolHandler
    Json::Value jsonData;
    MessageType msgType;
    if (!protocolHandler->parseHttpJson(data.data(), len, msgType, jsonData)) {
        evhttp_send_reply(req, 400, "错误的请求", nullptr);
        return;
    }

    // 3. 业务处理
    std::string username = jsonData["username"].asString();
    std::string password = jsonData["password"].asString();
    std::string msg;
    bool success = dbManager->verifyUser(username, password, msg);

    // 4. 构建HTTP响应
    struct evbuffer* out = evbuffer_new();
    if (!out) {
        std::cerr << "创建输出缓冲区失败" << std::endl;
        evhttp_send_reply(req, 500, "网络服务错误：创建输出缓冲区失败", nullptr);
        return;
    }
    if (success) {
        std::cout << "用户" << username << "登录成功！" << std::endl;
        // 设置响应头为JSON格式
        evhttp_add_header(evhttp_request_get_output_headers(req), "Content-Type", "application/json");
        Json::Value resp;
        resp["action"] = "signin";
        resp["status"] = "success";
        Json::FastWriter writer;
        std::string jsonStr = writer.write(resp);
        evbuffer_add(out, jsonStr.c_str(), jsonStr.length());
    } else {
        std::cout << "用户" << username << "登录失败！" << std::endl;
        std::cout << "失败原因：" << msg << std::endl;
        
        // 设置响应头为纯文本格式
        evhttp_add_header(evhttp_request_get_output_headers(req), "Content-Type", "text/plain");
        
        // 将错误消息添加到输出缓冲区，如果msg为空，则返回"fail"，否则返回具体的错误消息
        // 这里的msg是数据库操作返回的错误信息
        evbuffer_add_printf(out, "%s", msg.empty() ? "fail" : msg.c_str());
    }
    evhttp_send_reply(req, 200, "OK", out);// 函数作用：发送HTTP响应
    evbuffer_free(out);
}


// HTTP处理修改密码请求
void MessageHandler::handleChangePasswordHttp(struct evhttp_request* req, void* arg) {
    // 1. 读取HTTP请求体
    struct evbuffer* buf = evhttp_request_get_input_buffer(req);
    size_t len = evbuffer_get_length(buf);
    std::vector<char> data(len);
    evbuffer_copyout(buf, data.data(), len);

    // 2. 协议解析和字段校验全部交给ProtocolHandler
    Json::Value jsonData;
    MessageType msgType;
    if (!protocolHandler->parseHttpJson(data.data(), len, msgType,  jsonData)) {
        evhttp_send_reply(req, 400, "错误的请求", nullptr);
        return;
    }

    // 3. 业务处理
    std::string username = jsonData["username"].asString();
    std::string newPassword = jsonData["password"].asString();
    std::string msg;
    bool success = dbManager->updatePassword(username, newPassword, msg);

    // 4. 构建HTTP响应
    struct evbuffer* out = evbuffer_new();
    if (!out) {
        // 创建输出缓冲区失败
        std::cerr << "创建输出缓冲区失败" << std::endl;
        evhttp_send_reply(req, 500, "网络服务错误：创建输出缓冲区失败", nullptr);
        return;
    }
    if (success) {
        std::cout << "用户" << username << "密码修改成功！" << std::endl;
        // 设置响应头为JSON格式
        evhttp_add_header(evhttp_request_get_output_headers(req), "Content-Type", "application/json");
        Json::Value resp;
        resp["action"] = "resetPassword";
        resp["status"] = "success";
        Json::FastWriter writer;
        std::string jsonStr = writer.write(resp);
        evbuffer_add(out, jsonStr.c_str(), jsonStr.length());
    } else {
        std::cout << "用户" << username << "密码修改失败！" << std::endl;
        std::cout << "失败原因：" << msg << std::endl;
        
        // 设置响应头为纯文本格式
        evhttp_add_header(evhttp_request_get_output_headers(req), "Content-Type", "text/plain");
        
        // 将错误消息添加到输出缓冲区，如果msg为空，则返回"fail"，否则返回具体的错误消息
        // 这里的msg是数据库操作返回的错误信息
        evbuffer_add_printf(out, "%s", msg.empty() ? "fail" : msg.c_str());
    }
    evhttp_send_reply(req, 200, "OK", out);
    evbuffer_free(out);
}

// HTTP处理添加房间请求
void MessageHandler::handleAddRoomHttp(struct evhttp_request* req, void* arg) {
    struct evbuffer* buf = evhttp_request_get_input_buffer(req);
    size_t len = evbuffer_get_length(buf);
    std::vector<char> data(len);
    evbuffer_copyout(buf, data.data(), len);

    Json::Value jsonData;
    MessageType msgType;
    if (!protocolHandler->parseHttpJson(data.data(), len, msgType, jsonData)) {
        sendErrorHttp(req, "请求格式错误");
        return;
    }

    std::string roomType = jsonData["room_type"].asString();
    std::string roomName = jsonData["room_name"].asString();
    std::string msg;
    bool success = dbManager->addRoom(roomType, roomName, msg);

    struct evbuffer* out = evbuffer_new();
    if (!out) {
        evhttp_send_reply(req, 500, "网络服务错误：创建输出缓冲区失败", nullptr);
        return;
    }
    Json::Value resp;
    resp["action"] = "addRoom";
    resp["status"] = success ? "success" : "fail";
    resp["message"] = msg;
    Json::FastWriter writer;
    std::string jsonStr = writer.write(resp);
    evbuffer_add(out, jsonStr.c_str(), jsonStr.length());
    evhttp_send_reply(req, 200, "OK", out);
    evbuffer_free(out);
}

// HTTP处理删除房间请求
void MessageHandler::handleDeleteRoomHttp(struct evhttp_request* req, void* arg) {
    struct evbuffer* buf = evhttp_request_get_input_buffer(req);
    size_t len = evbuffer_get_length(buf);
    std::vector<char> data(len);
    evbuffer_copyout(buf, data.data(), len);

    Json::Value jsonData;
    MessageType msgType;
    if (!protocolHandler->parseHttpJson(data.data(), len, msgType, jsonData)) {
        sendErrorHttp(req, "请求格式错误");
        return;
    }

    int room_id = jsonData["room_id"].asInt();
    std::string msg;
    bool success = dbManager->deleteRoom(room_id, msg);

    struct evbuffer* out = evbuffer_new();
    if (!out) {
        evhttp_send_reply(req, 500, "网络服务错误：创建输出缓冲区失败", nullptr);
        return;
    }
    Json::Value resp;
    resp["action"] = "deleteRoom";
    resp["status"] = success ? "success" : "fail";
    resp["message"] = msg;
    Json::FastWriter writer;
    std::string jsonStr = writer.write(resp);
    evbuffer_add(out, jsonStr.c_str(), jsonStr.length());
    evhttp_send_reply(req, 200, "OK", out);
    evbuffer_free(out);
}

// HTTP处理查询房间请求
void MessageHandler::handleListRoomsHttp(struct evhttp_request* req, void* arg) {
    Json::Value rooms;             
    std::string msg;
    bool success = dbManager->listRooms(rooms, msg);

    struct evbuffer* out = evbuffer_new();
    if (!out) {
        evhttp_send_reply(req, 500, "网络服务错误：创建输出缓冲区失败", nullptr);
        return;
    }
    Json::Value resp;
    resp["action"]  = "listRooms";
    resp["status"]  = success ? "success" : "fail";
    resp["message"] = msg;
    if (success) {
        resp["rooms"] = rooms;  
    }

    Json::FastWriter writer;
    std::string jsonStr = writer.write(resp);
    evbuffer_add(out, jsonStr.c_str(), jsonStr.length());
    evhttp_send_reply(req, 200, "OK", out);
    evbuffer_free(out);
}

// TCP处理更新房间请求
void MessageHandler::handleUpdateRoomHttp(struct evhttp_request* req, void* arg) {
    struct evbuffer* buf = evhttp_request_get_input_buffer(req);
    size_t len = evbuffer_get_length(buf);
    std::vector<char> data(len);
    evbuffer_copyout(buf, data.data(), len);

    Json::Value jsonData;
    MessageType msgType;
    if (!protocolHandler->parseHttpJson(data.data(), len, msgType, jsonData)) {
        sendErrorHttp(req, "请求格式错误");
        return;
    }

    int room_id = jsonData["room_id"].asInt();
    std::string roomType = jsonData["room_type"].asString();
    std::string roomName = jsonData["room_name"].asString();
    std::string msg;
    bool success = dbManager->updateRoom(room_id, roomType, roomName, msg);

    struct evbuffer* out = evbuffer_new();
    if (!out) {
        evhttp_send_reply(req, 500, "网络服务错误：创建输出缓冲区失败", nullptr);
        return;
    }
    Json::Value resp;
    resp["action"] = "updateRoom";
    resp["status"] = success ? "success" : "fail";
    resp["message"] = msg;
    Json::FastWriter writer;
    std::string jsonStr = writer.write(resp);
    evbuffer_add(out, jsonStr.c_str(), jsonStr.length());
    evhttp_send_reply(req, 200, "OK", out);
    evbuffer_free(out);
}

// HTTP处理添加设备请求
void MessageHandler::handleAddDeviceHttp(struct evhttp_request* req, void* arg) {
    // 1. 读取HTTP请求体
    struct evbuffer* buf = evhttp_request_get_input_buffer(req);
    size_t len = evbuffer_get_length(buf);
    std::vector<char> data(len);
    evbuffer_copyout(buf, data.data(), len);

    // 2. 协议解析和字段校验
    Json::Value jsonData;
    MessageType msgType;
    if (!protocolHandler->parseHttpJson(data.data(), len, msgType, jsonData)) {
        sendErrorHttp(req, "请求格式错误");
        return;
    }

    // 3. 业务处理
    std::string device_name = jsonData["device_name"].asString();
    std::string device_type = jsonData["device_type"].asString();
    int room_id = jsonData["room_id"].asInt();
    std::string image = jsonData["image"].asString();
    std::string msg;
    bool success = dbManager->addDevice(device_name, device_type, room_id, image, msg);

    // 4. 构建HTTP响应
    struct evbuffer* out = evbuffer_new();
    if (!out) {
        evhttp_send_reply(req, 500, "网络服务错误：创建输出缓冲区失败", nullptr);
        return;
    }
    if (success) {
        std::cout << "设备" << device_name << "添加成功！" << std::endl;
        evhttp_add_header(evhttp_request_get_output_headers(req), "Content-Type", "application/json");
        Json::Value resp;
        resp["action"] = "addDevice";
        resp["status"] = "success";
        resp["message"] = msg;
        Json::FastWriter writer;
        std::string jsonStr = writer.write(resp);
        evbuffer_add(out, jsonStr.c_str(), jsonStr.length());
    } else {
        std::cout << "设备" << device_name << "添加失败！" << std::endl;
        std::cout << "失败原因：" << msg << std::endl;
        evhttp_add_header(evhttp_request_get_output_headers(req), "Content-Type", "text/plain");
        evbuffer_add_printf(out, "%s", msg.empty() ? "fail" : msg.c_str());
    }
    evhttp_send_reply(req, 200, "OK", out);
    evbuffer_free(out);
}

// HTTP处理删除设备请求
void MessageHandler::handleDeleteDeviceHttp(struct evhttp_request* req, void* arg) {
    // 1. 读取HTTP请求体
    struct evbuffer* buf = evhttp_request_get_input_buffer(req);
    size_t len = evbuffer_get_length(buf);
    std::vector<char> data(len);
    evbuffer_copyout(buf, data.data(), len);

    // 2. 协议解析和字段校验
    Json::Value jsonData;
    MessageType msgType;
    if (!protocolHandler->parseHttpJson(data.data(), len, msgType, jsonData)) {
        sendErrorHttp(req, "请求格式错误");
        return;
    }

    // 3. 业务处理
    int device_id = jsonData["device_id"].asInt();
    std::string msg;
    bool success = dbManager->deleteDevice(device_id, msg);

    // 4. 构建HTTP响应
    struct evbuffer* out = evbuffer_new();
    if (!out) {
        evhttp_send_reply(req, 500, "网络服务错误：创建输出缓冲区失败", nullptr);
        return;
    }
    if (success) {
        std::cout << "设备ID " << device_id << " 删除成功！" << std::endl;
        evhttp_add_header(evhttp_request_get_output_headers(req), "Content-Type", "application/json");
        Json::Value resp;
        resp["action"] = "deleteDevice";
        resp["status"] = "success";
        resp["message"] = msg;
        Json::FastWriter writer;
        std::string jsonStr = writer.write(resp);
        evbuffer_add(out, jsonStr.c_str(), jsonStr.length());
    } else {
        std::cout << "设备ID " << device_id << " 删除失败！" << std::endl;
        std::cout << "失败原因：" << msg << std::endl;
        evhttp_add_header(evhttp_request_get_output_headers(req), "Content-Type", "text/plain");
        evbuffer_add_printf(out, "%s", msg.empty() ? "fail" : msg.c_str());
    }
    evhttp_send_reply(req, 200, "OK", out);
    evbuffer_free(out);
}

// HTTP处理查询设备请求
void MessageHandler::handleListDevicesHttp(struct evhttp_request* req, void* arg) {
    // 1. 读取HTTP请求体（可选，若无参数可省略）
    // 2. 协议解析和字段校验（可选，若无参数可省略）

    // 3. 业务处理
    Json::Value devices;
    std::string msg;
    bool success = dbManager->listDevices(devices, msg);

    // 4. 构建HTTP响应
    struct evbuffer* out = evbuffer_new();
    if (!out) {
        evhttp_send_reply(req, 500, "网络服务错误：创建输出缓冲区失败", nullptr);
        return;
    }
    if (success) {
        std::cout << "设备列表查询成功！" << std::endl;
        evhttp_add_header(evhttp_request_get_output_headers(req), "Content-Type", "application/json");
        Json::Value resp;
        resp["action"] = "listDevices";
        resp["status"] = "success";
        resp["devices"] = devices;
        resp["message"] = msg;
        Json::FastWriter writer;
        std::string jsonStr = writer.write(resp);
        evbuffer_add(out, jsonStr.c_str(), jsonStr.length());
    } else {
        std::cout << "设备列表查询失败！" << std::endl;
        std::cout << "失败原因：" << msg << std::endl;
        evhttp_add_header(evhttp_request_get_output_headers(req), "Content-Type", "text/plain");
        evbuffer_add_printf(out, "%s", msg.empty() ? "fail" : msg.c_str());
    }
    evhttp_send_reply(req, 200, "OK", out);
    evbuffer_free(out);
}

// HTTP处理修改设备请求
void MessageHandler::handleUpdateDeviceHttp(struct evhttp_request* req, void* arg) {
    // 1. 读取HTTP请求体
    struct evbuffer* buf = evhttp_request_get_input_buffer(req);
    size_t len = evbuffer_get_length(buf);
    std::vector<char> data(len);
    evbuffer_copyout(buf, data.data(), len);

    // 2. 协议解析和字段校验
    Json::Value jsonData;
    MessageType msgType;
    if (!protocolHandler->parseHttpJson(data.data(), len, msgType, jsonData)) {
        sendErrorHttp(req, "请求格式错误");
        return;
    }

    // 3. 业务处理
    int device_id = jsonData["device_id"].asInt();
    std::string device_name = jsonData["device_name"].asString();
    std::string device_type = jsonData["device_type"].asString();
    int room_id = jsonData["room_id"].asInt();
    std::string msg;
    std::string image = jsonData["image"].asString();
    bool success = dbManager->updateDevice(device_id, device_name, device_type, room_id, image, msg);

    // 4. 构建HTTP响应
    struct evbuffer* out = evbuffer_new();
    if (!out) {
        evhttp_send_reply(req, 500, "网络服务错误：创建输出缓冲区失败", nullptr);
        return;
    }
    if (success) {
        std::cout << "设备ID " << device_id << " 更新成功！" << std::endl;
        evhttp_add_header(evhttp_request_get_output_headers(req), "Content-Type", "application/json");
        Json::Value resp;
        resp["action"] = "updateDevice";
        resp["status"] = "success";
        resp["message"] = msg;
        Json::FastWriter writer;
        std::string jsonStr = writer.write(resp);
        evbuffer_add(out, jsonStr.c_str(), jsonStr.length());
    } else {
        std::cout << "设备ID " << device_id << " 更新失败！" << std::endl;
        std::cout << "失败原因：" << msg << std::endl;
        evhttp_add_header(evhttp_request_get_output_headers(req), "Content-Type", "text/plain");
        evbuffer_add_printf(out, "%s", msg.empty() ? "fail" : msg.c_str());
    }
    evhttp_send_reply(req, 200, "OK", out);
    evbuffer_free(out);
}

// HTTP处理人脸上传请求
void MessageHandler::handleFaceUpsertHttp(struct evhttp_request* req) {
    //
    struct evbuffer* buf = evhttp_request_get_input_buffer(req);
    size_t len = evbuffer_get_length(buf);
    std::vector<char> data(len);
    evbuffer_copyout(buf, data.data(), len);

    // 检查人脸管理器是否可用
    if (!faceMgr_) {
        std::cout << "人脸管理器未设置" << std::endl; 
        sendErrorHttp(req, "人脸管理器未设置"); 
        return; 
    }

    // 解析JSON，提取必要字段
    MessageType mt; Json::Value jsonData;
    if (!protocolHandler->parseHttpJson(data.data(), len, mt, jsonData) || mt != MSG_FACE_UPSERT) {
        std::cout << "解析人脸上传请求失败或消息类型不匹配" << std::endl;
        sendErrorHttp(req, "无效的JSON请求，期待有效的action指令");
        return;
    }

    // 提取用户信息
    std::string username = jsonData["username"].asString();
    const std::string* face_b64_ptr = nullptr; 
    std::string face_b64;
    if (jsonData.isMember("face_base64")) {
        face_b64 = jsonData["face_base64"].asString();
        std::cout << "收到的 face_base64 长度: " << face_b64.length() << std::endl;
        face_b64_ptr = &face_b64;
    }

    auto res = faceMgr_->upsertUserFace(username, face_b64_ptr);

    // 终端输出成功/失败信息
    if (res.ok) {
        std::cout << "[HTTP] 人脸插入成功: username=" << username
                  << ", face_saved=" << (res.face_saved ? "true" : "false")
                  << ", face_len=" << (face_b64_ptr ? face_b64_ptr->size() : 0)
                  << std::endl;
    } else {
        std::cerr << "[HTTP] 人脸插入失败: username=" << username
                  << ", error=" << (res.error.empty() ? "unknown" : res.error)
                  << std::endl;
    }

    // 构建HTTP响应
    struct evbuffer* out = evbuffer_new();
    evhttp_add_header(evhttp_request_get_output_headers(req), "Content-Type", "application/json; charset=utf-8");

    // 构建响应JSON
    Json::Value resp;
    resp["status"] = res.ok ? "success" : "fail";
    resp["face_saved"] = res.face_saved;
    if(!res.error.empty()) resp["error"] = res.error;

    // 序列化JSON
    Json::FastWriter w;
    std::string s = w.write(resp);
    evbuffer_add(out, s.c_str(), s.size());
    
    // 根据错误类型选择状态码：用户不存在 -> 400，其它 -> 500
    int status = 200; const char* reason = "OK";
    if (!res.ok) {
        bool user_missing = (res.error.find("user not exists") != std::string::npos) ||
                            (res.error.find("user not exists?") != std::string::npos) ||
                            (res.error.find("用户未注册") != std::string::npos);
        status = user_missing ? 400 : 500;
        reason = user_missing ? "错误的请求" : "服务器错误";
    }
    
    evhttp_send_reply(req, status, reason, out);
    evbuffer_free(out);
}

// HTTP处理人脸删除请求
inline void MessageHandler::handleFaceDeleteHttp(struct evhttp_request* req) {
    struct evbuffer* buf = evhttp_request_get_input_buffer(req);
    size_t len = evbuffer_get_length(buf);
    std::vector<char> data(len);
    evbuffer_copyout(buf, data.data(), len);

    if (!faceMgr_) { sendErrorHttp(req, "人脸管理器未设置"); return; }

    MessageType mt; Json::Value jsonData;
    if (!protocolHandler->parseHttpJson(data.data(), len, mt, jsonData) || mt != MSG_FACE_DELETE) {
        sendErrorHttp(req, "无效的JSON请求，期待有效的action指令");
        return;
    }

    std::string err; bool ok = faceMgr_->deleteUserFace(jsonData["username"].asString(), &err);

    struct evbuffer* out = evbuffer_new();
    evhttp_add_header(evhttp_request_get_output_headers(req), "Content-Type", "application/json");
    Json::Value resp; 
    resp["status"] = ok ? "success" : "fail";  
    if(!err.empty()) resp["error"]=err;
    
    Json::FastWriter w; 
    std::string s = w.write(resp);

    evbuffer_add(out, s.c_str(), s.size());
    evhttp_send_reply(req, ok ? 200 : 500, "OK", out);
    evbuffer_free(out);
}

// HTTP处理人脸查询请求
inline void MessageHandler::handleFaceQueryHttp(struct evhttp_request* req) {
    struct evbuffer* buf = evhttp_request_get_input_buffer(req);
    size_t len = evbuffer_get_length(buf);
    std::vector<char> data(len);
    evbuffer_copyout(buf, data.data(), len);

    if (!faceMgr_) { sendErrorHttp(req, "人脸管理器未设置"); return; }

    MessageType mt; Json::Value jsonData;
    if (!protocolHandler->parseHttpJson(data.data(), len, mt, jsonData) || mt != MSG_FACE_QUERY) {
        sendErrorHttp(req, "无效的JSON请求，期待有效的action指令");
        return;
    }

    auto q = faceMgr_->queryUserFace(jsonData["username"].asString());

    struct evbuffer* out = evbuffer_new();
    evhttp_add_header(evhttp_request_get_output_headers(req), "Content-Type", "application/json");
    
    Json::Value resp;
    resp["status"] = q.exists ? "success" : "fail";
    resp["exists"] = q.exists;
    if (!q.error.empty()) resp["error"] = q.error;

    Json::FastWriter w;
    std::string s = w.write(resp);
    
    evbuffer_add(out, s.c_str(), s.size());
    evhttp_send_reply(req, 200, "OK", out);
    evbuffer_free(out);
}

// HTTP处理人脸识别请求
inline void MessageHandler::handleFaceRecognizeHttp(struct evhttp_request* req) {
    struct evbuffer* buf = evhttp_request_get_input_buffer(req);
    size_t len = evbuffer_get_length(buf);
    std::vector<char> data(len);
    evbuffer_copyout(buf, data.data(), len);

    // 检查人脸管理器是否可用
    if (!faceMgr_) { 
        sendErrorHttp(req, "人脸管理器未设置"); 
        return; 
    }

    MessageType mt; Json::Value jsonData;

    // 解析JSON
    if (!protocolHandler->parseHttpJson(data.data(), len, mt, jsonData) || mt != MSG_FACE_RECOGNIZE) {
        sendErrorHttp(req, "无效的JSON请求，期待有效的action指令");
        return;
    }

    auto rc = faceMgr_->recognize(jsonData["face_base64"].asString());

    struct evbuffer* out = evbuffer_new();
    evhttp_add_header(evhttp_request_get_output_headers(req), "Content-Type", "application/json");

    Json::Value resp;
    resp["status"] = rc.ok ? "success" : "fail";
    resp["username"] = rc.username;
    if (!rc.error.empty()) resp["error"] = rc.error;

    Json::FastWriter w; std::string s = w.write(resp);
    evbuffer_add(out, s.c_str(), s.size());
    evhttp_send_reply(req, 200, "OK", out);
    evbuffer_free(out);
}


// HTTP发送错误消息
void MessageHandler::sendErrorHttp(struct evhttp_request* req, const std::string& errorMsg) {
    struct evbuffer* out = evbuffer_new();
    if (!out) {
        evhttp_send_reply(req, 500, "网络服务器错误", nullptr);
        return;
    }

    // 统一用 JSON + UTF-8，避免乱码问题的出现
    evhttp_add_header(evhttp_request_get_output_headers(req), "Content-Type", "application/json; charset=utf-8");
    Json::Value resp;
    resp["status"] = "fail";
    resp["message"] = errorMsg;
    Json::FastWriter writer;
    std::string s = writer.write(resp);
    evbuffer_add(out, s.c_str(), s.size());

    // 发送响应
    evhttp_send_reply(req, 400, "错误的请求", out);
    evbuffer_free(out);
}

// MQTT处理注册请求
void MessageHandler::handleRegisterMqtt(const std::string& topic, const Json::Value& data) {
    // 直接使用解析出来的字段
    std::string username = data["username"].asString();
    std::string password = data["password"].asString();

    // 存储返回消息
    std::string msg;

    // 调用数据库管理器进行用户注册
    bool success = dbManager->registerUser(username, password, msg);

    // 处理注册结果
    if (success) {
        std::cout << "用户 " << username << " 注册成功！" << std::endl;
        Json::Value resp;
        resp["action"] = "signup";
        resp["status"] = "success";
        resp["message"] = msg;
        publishMqttResponse(topic, resp);
    } else {
        std::cerr << "[MQTT] 用户 " << username << " 注册失败！" << std::endl;
        Json::Value resp;
        resp["action"] = "signup";
        resp["status"] = "fail";
        resp["message"] = msg;
        publishMqttResponse(topic, resp);
    }
}

// MQTT处理登录请求
void MessageHandler::handleLoginMqtt(const std::string& topic, const Json::Value& data) {
    //直接使用解析出来的字段
    std::string username = data["username"].asString();
    std::string password = data["password"].asString();

    // 存储返回消息
    std::string msg;

    // 调用数据库管理器进行用户验证
    bool success = dbManager->verifyUser(username, password, msg);

    // 处理登录结果
    if (success) {
        std::cerr << "[MQTT] 用户 " << username << " 登录成功！" << std::endl;
        Json::Value resp;
        resp["action"] = "signin";
        resp["status"] = success ? "success" : "fail";
        resp["message"] = msg;
        publishMqttResponse(topic, resp);
    }else{
        std::cerr << "[MQTT] 用户 " << username << " 登录失败！" << std::endl;
        Json::Value resp;
        resp["action"] = "signin";
        resp["status"] = "fail";
        resp["message"] = msg;
        publishMqttResponse(topic, resp);
    }
}

// MQTT处理修改密码请求
void MessageHandler::handleChangePasswordMqtt(const std::string& topic, const Json::Value& data) {
    //直接使用解析出来的字段
    std::string username = data["username"].asString();
    std::string newPassword = data["password"].asString();

    // 存储返回消息
    std::string msg;

    // 调用数据库管理器进行密码更新
    bool success = dbManager->updatePassword(username, newPassword, msg);

    // 处理更新结果
    if(success) {
        Json::Value resp;
        std::cout << "[MQTT] 用户 " << username << " 密码修改成功！" << std::endl;
        resp["action"] = "resetPassword";
        resp["status"] = success ? "success" : "fail";
        resp["message"] = msg;
        publishMqttResponse(topic, resp);
    }else{
        Json::Value resp;
        std::cerr << "[MQTT] 用户 " << username << " 密码修改失败！" << std::endl;
        resp["action"] = "resetPassword";
        resp["status"] = "fail";
        resp["message"] = msg;
        publishMqttResponse(topic, resp);
    }
}

// MQTT处理添加房间请求
void MessageHandler::handleAddRoomMqtt(const std::string& topic, const Json::Value& data) {
    std::string roomType = data["room_type"].asString();
    std::string roomName = data["room_name"].asString();
    std::string responseMsg;
    bool success = dbManager->addRoom(roomType, roomName, responseMsg);

    Json::Value resp;
    resp["action"] = "addRoom";
    resp["status"] = success ? "success" : "fail";
    resp["message"] = responseMsg;
    publishMqttResponse(topic, resp);
}

// MQTT处理删除房间请求
void MessageHandler::handleDeleteRoomMqtt(const std::string& topic, const Json::Value& data) {
    int roomId = data["room_id"].asInt();
    std::string responseMsg;
    bool success = dbManager->deleteRoom(roomId, responseMsg);

    Json::Value resp;
    resp["action"] = "deleteRoom";
    resp["status"] = success ? "success" : "fail";
    resp["message"] = responseMsg;
    publishMqttResponse(topic, resp);
}

// MQTT处理查询房间请求
void MessageHandler::handleListRoomsMqtt(const std::string& topic, const Json::Value& data) {
    Json::Value rooms;
    std::string msg;
    bool success = dbManager->listRooms(rooms, msg);

    Json::Value resp;
    resp["action"] = "listRooms";
    resp["status"] = success ? "success" : "fail";
    resp["rooms"] = rooms;
    resp["message"] = msg;
    publishMqttResponse(topic, resp);
}

// MQTT处理更新房间请求
void MessageHandler::handleUpdateRoomMqtt(const std::string& topic, const Json::Value& data) {
    int roomId = data["room_id"].asInt();
    std::string roomName = data["room_name"].asString();
    std::string roomType = data["room_type"].asString();
    std::string responseMsg;
    // 修改：按 DatabaseManager::updateRoom 的参数顺序 (room_id, room_type, room_name)
    bool success = dbManager->updateRoom(roomId, roomType, roomName, responseMsg);

    Json::Value resp;
    resp["action"]  = "updateRoom";
    resp["status"]  = success ? "success" : "fail";
    resp["message"] = responseMsg;
    publishMqttResponse(topic, resp);
}

// MQTT处理添加设备请求
void MessageHandler::handleAddDeviceMqtt(const std::string& topic, const Json::Value& data) {
    //直接使用字段
    std::string deviceName = data["device_name"].asString();
    std::string deviceType = data["device_type"].asString();
    int roomId = data["room_id"].asInt();
    std::string image = data["image"].asString();

    std::string responseMsg;
    bool success = dbManager->addDevice(deviceName, deviceType, roomId, image, responseMsg);
    
    // 构造响应
    Json::Value response;
    response["action"] = "addDevice";
    response["status"] = success ? "success" : "fail";
    response["message"] = success ? "设备添加成功" : responseMsg;
    
    // 发送响应
    publishMqttResponse(topic, response);
    
    // 输出日志
    if (success) {
        std::cout << "[MQTT] 设备 " << deviceName << " 添加成功！" << std::endl;
    } else {
        std::cout << "[MQTT] 设备 " << deviceName << " 添加失败！" << std::endl;
    }
}

// MQTT处理删除设备请求
void MessageHandler::handleDeleteDeviceMqtt(const std::string& topic, const Json::Value& data) {
    // 直接使用解析出来的字段
    std::string deviceIdStr = data["device_id"].asString();
    int deviceId = std::stoi(deviceIdStr);

    // 存储返回消息
    std::string msg;

    // 调用数据库管理器进行设备删除
    bool success = dbManager->deleteDevice(deviceId, msg);

    // 处理删除结果
    if(success) {
        Json::Value resp;
        std::cout << "[MQTT] 设备ID " << deviceId << " 删除成功！" << std::endl;
        resp["action"] = "deleteDevice";
        resp["status"] = "success";
        resp["message"] = msg;
        publishMqttResponse(topic, resp);
    }else {
        Json::Value resp;
        std::cerr << "[MQTT] 设备ID " << deviceId << " 删除失败！" << std::endl;
        resp["action"] = "deleteDevice";
        resp["status"] = "fail";
        resp["message"] = msg;
        publishMqttResponse(topic, resp);
    }
}

// MQTT处理设备列表请求
void MessageHandler::handleListDevicesMqtt(const std::string& topic, const Json::Value& data) {
    // 直接使用解析出来的字段（如果有必要）
    Json::Value devices;
    std::string msg;
    bool success = dbManager->listDevices(devices, msg);

    Json::Value resp;
    resp["action"] = "listDevices";
    resp["status"] = success ? "success" : "fail";
    resp["devices"] = devices;
    resp["message"] = msg;
    publishMqttResponse(topic, resp);
}

// MQTT处理修改设备请求
void MessageHandler::handleUpdateDeviceMqtt(const std::string& topic, const Json::Value& data) {
    // 直接使用解析出来的字段
    int deviceId = data["device_id"].asInt();
    std::string deviceName = data["device_name"].asString();
    std::string deviceType = data["device_type"].asString();
    int roomId = data["room_id"].asInt();
    std::string image = data["image"].asString();

    // 存储返回消息
    std::string msg;

    // 调用数据库管理器进行设备更新
    bool success = dbManager->updateDevice(deviceId, deviceName, deviceType, roomId, image, msg);
    // 处理更新结果
    if(success) {
        Json::Value resp;
        std::cout << "[MQTT] 设备ID " << deviceId << " 更新成功！" << std::endl;
        resp["action"] = "updateDevice";
        resp["status"] = "success";
        resp["message"] = msg;
        publishMqttResponse(topic, resp);
    }else {
        Json::Value resp;
        std::cerr << "[MQTT] 设备ID " << deviceId << " 更新失败！" << std::endl;
        resp["action"] = "updateDevice";
        resp["status"] = "fail";
        resp["message"] = msg;
        publishMqttResponse(topic, resp);
    }
}

// MQTT处理设备控制请求
void MessageHandler::handleControlDeviceMqtt(const std::string& topic, const Json::Value& data) {
    // 直接使用解析出来的字段
    int deviceId = data["device_id"].asInt();
    std::string command = data["command"].asString();

    // 存储返回消息
    std::string msg;

    // 调用数据库管理器进行设备控制
    bool success = dbManager->controlDevice(deviceId, command, msg);

    if(success) {
        Json::Value resp;
        resp["action"] = "controlDevice";
        resp["status"] = "success";
        resp["message"] = msg;
        publishMqttResponse(topic, resp);
    }else {
        Json::Value resp;
        resp["action"] = "controlDevice";
        resp["status"] = "fail";
        resp["message"] = msg;
        publishMqttResponse(topic, resp);
    }
}

// MQTT人脸注册请求
inline void MessageHandler::handleFaceUpsertMqtt(const std::string& topic, const Json::Value& data) {
    Json::Value resp; 
    resp["action"] = "faceUpsert";

    // 1) 检查人脸管理器
    if (!faceMgr_) {
        resp["status"] = "fail";
        resp["face_saved"] = false; 
        resp["error"] = "人脸管理器未设置";
        return publishMqttResponse(topic, resp);
    }

    // 2) 参数校验
    if (!data.isMember("username") || !data["username"].isString()) { 
        resp["status"] = "fail";
        resp["face_saved"] = false; 
        resp["error"] = "未识别到用户名字段";
        return publishMqttResponse(topic, resp);
    }
    if (!data.isMember("face_base64") || !data["face_base64"].isString()) {
        resp["status"] = "fail";
        resp["face_saved"] = false; 
        resp["error"] = "未识别到人脸字段";
        return publishMqttResponse(topic, resp);
    }

    const std::string username = data["username"].asString();

    // 3) 用户必须已注册在数据库中
    if (!dbManager || !dbManager->userExists(username)) { 
        resp["status"] = "fail";
        resp["face_saved"] = false; 
        resp["error"] = "用户未注册";
        return publishMqttResponse(topic, resp);
    }

    // 4) 执行人脸入库
    std::string face_b64 = data["face_base64"].asString();
    const std::string* face_b64_ptr = &face_b64;

    auto r = faceMgr_->upsertUserFace(username, face_b64_ptr);

    // 5) 响应（与 HTTP 字段对齐，保留 status 兼容）
    resp["status"] = r.ok ? "success" : "fail";
    resp["face_saved"] = r.face_saved;
    if (!r.error.empty()) resp["error"] = r.error;


    publishMqttResponse(topic, resp);
}

//MQTT人脸删除请求
inline void MessageHandler::handleFaceDeleteMqtt(const std::string& topic, const Json::Value& data) {
    Json::Value resp; resp["action"]="faceDelete";
    
    // 检查人脸管理器是否可用
    if (!faceMgr_) {
        resp["status"]="fail"; 
        resp["message"]="人脸管理器未设置";
        return publishMqttResponse(topic, resp);
    }

    // 提取用户名和人脸图像 
    std::string err; bool ok = faceMgr_->deleteUserFace(data["username"].asString(), &err);
    resp["status"] = ok ? "success" : "fail"; 
    if(!err.empty()) resp["message"]=err;
    publishMqttResponse(topic, resp);
}

// MQTT人脸查询请求
inline void MessageHandler::handleFaceQueryMqtt(const std::string& topic, const Json::Value& data) {
    Json::Value resp; resp["action"]="faceQuery";

    // 检查人脸管理器是否可用
    if (!faceMgr_) {
        resp["status"]="fail";
        resp["message"]="人脸管理器未设置";
        return publishMqttResponse(topic, resp);
    }

    // 查询用户人脸特征
    auto q = faceMgr_->queryUserFace(data["username"].asString());
    resp["status"]="success"; 
    resp["exists"]=q.exists; 
    if(!q.error.empty()) resp["message"]=q.error;
    publishMqttResponse(topic, resp);
}

//MQTT人脸请求
inline void MessageHandler::handleFaceRecognizeMqtt(const std::string& topic, const Json::Value& data) {
    Json::Value resp; resp["action"]="faceRecognize";

    // 检查人脸管理器是否可用
    if (!faceMgr_) {
        resp["status"]="fail";
        resp["message"]="人脸管理器未设置";
        return publishMqttResponse(topic, resp);
    }

    // 调用人脸管理器进行人脸识别
    auto rc = faceMgr_->recognize(data["face_base64"].asString());
    resp["status"] = rc.error.empty() ? "success" : "fail";
    resp["username"]=rc.username; 
    if(!rc.error.empty()) resp["message"]=rc.error;
    publishMqttResponse(topic, resp);
}

//MQTT返回响应主题
void MessageHandler::publishMqttResponse(const std::string& topic, const Json::Value& resp) {
    Json::FastWriter writer;
    std::string payload = writer.write(resp);
    //发送至独立的响应主题，避免自反馈
    if (mqttServer) {
        // 统一发到 response，进行区分
        mqttServer->publish("response", payload);
    } else {
        std::cerr << "MQTT响应服务未初始化" << std::endl;
    }
}

#endif // MESSAGE_HANDLER_H