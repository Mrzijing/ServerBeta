#ifndef PROTOCOL_HANDLER_H
#define PROTOCOL_HANDLER_H

#include <iostream>
#include <string>
#include <memory>
#include <jsoncpp/json/json.h>
#include <cstring>
#include <arpa/inet.h>
#include <iomanip>
#include <sstream>
#include <ctime>  // 新增：std::time

// 消息类型枚举，负责定义不同的消息类型，这些类型用于在协议处理器中解析和构建消息
// 同时与ProtocolHandler中的消息类型保持一致，以便在协议处理器中进行统一的消息解析和构建
// 这样可以确保客户端和服务器之间的通信协议一致，便于解析和处理
enum MessageType {

    // 用户注册/登录
    MSG_RESPONSE = 5,
    MSG_ERROR    = 6,
    MSG_REGISTER = 100,
    MSG_LOGIN    = 101,
    MSG_CHANGE_PASSWORD = 102,

    // 房间管理
    MSG_ADD_ROOM    = 200,
    MSG_DELETE_ROOM = 201,
    MSG_LIST_ROOMS  = 202,
    MSG_UPDATE_ROOM = 203,

    // 设备管理
    MSG_ADD_DEVICE     = 103,
    MSG_DELETE_DEVICE  = 104,
    MSG_LIST_DEVICES   = 105,
    MSG_CONTROL_DEVICE = 106,
    MSG_UPDATE_DEVICE  = 107,

    // 人脸识别
    MSG_FACE_UPSERT    = 300,
    MSG_FACE_DELETE    = 301,
    MSG_FACE_QUERY     = 302,
    MSG_FACE_RECOGNIZE = 303,

    // 客户端发送的十六进制字符串消息
    MSG_CLIENT_HEX = 400 // 修改：避免与 200 段房间管理冲突
};

// TCP客户端协议头结构（AA + 4字节长度）
#pragma pack(push, 1)
struct ClientProtocolHeader {
    uint8_t  magic;         // 0xAA
    uint32_t bodyLength;    // 数据长度
};
#pragma pack(pop)

// 协议处理器基类,负责协议的解析和构建；只负责解析和构建消息，不涉及具体业务逻辑，具体业务逻辑由MessageHandler类处理
// 将协议解析和业务逻辑分离，便于维护和扩展
class ProtocolHandler {
public:
    static const size_t CLIENT_HEADER_SIZE = sizeof(ClientProtocolHeader);// 客户端协议头大小

    // 析构函数,确保子类的析构函数能够被正确调用,避免内存泄漏和资源释放不当的问题
    virtual ~ProtocolHandler() {}
    
    // 解析TCP消息时，首先验证协议头，然后解析JSON数据，最后根据action字段设置消息类型
    virtual bool parseMessage(const char* data, size_t length, 
                              MessageType& msgType, Json::Value& jsonData) = 0;
    
    // 解析HTTP消息，纯虚函数，子类必须实现
    virtual bool parseHttpJson(const char* data, size_t length, 
                              MessageType& msgType, Json::Value& jsonData) = 0;

    // 解析MQTT消息，纯虚函数，子类必须实现
    virtual bool parseMqttMessage(const std::string& topic, const std::string& payload, 
                                  MessageType& msgType, Json::Value& jsonData) = 0;

    // 构建消息，纯虚函数，子类必须实现
    virtual std::string buildMessage(MessageType msgType, const std::string& body) = 0;

    // 十六进制工具函数,负责将十六进制字符串转换为普通字符串（TCP协议）
    static std::string hexToString(const std::string& hex) {
        std::string result;
        result.reserve(hex.length() / 2);
        for (size_t i = 0; i < hex.length(); i += 2) {
            if (i + 1 >= hex.length()) break;
            std::string byteString = hex.substr(i, 2);
            char byte = static_cast<char>(strtol(byteString.c_str(), nullptr, 16));
            result.push_back(byte);
        }
        return result;
    }

    // 发布MQTT响应消息，用于向客户端发送响应
    void publishMqttResponse(const std::string& topic, const Json::Value& resp) {
        Json::FastWriter writer;
        std::string payload = writer.write(resp);
    }

};

// 客户端协议处理器 ，承担所有解析和验证责任 ，对外提供简化的接口
class ClientProtocolHandler : public ProtocolHandler {
public:
    bool parseMessage(const char* data, size_t length,
                      MessageType& msgType, Json::Value& jsonData) override {
        
        // 1. 协议头验证
        if (length < CLIENT_HEADER_SIZE || data[0] != static_cast<char>(0xAA)) {
            std::cout << "协议头验证失败" << std::endl;
            return false;
        }
        
        // 2. 解析协议头
        ClientProtocolHeader header;
        memcpy(&header, data, CLIENT_HEADER_SIZE);

        uint32_t dataLength = ntohl(header.bodyLength);
        
        // 3. 验证数据长度
        if (length < CLIENT_HEADER_SIZE + dataLength) {
            std::cout << "数据长度验证失败" << std::endl;
            return false;
        }
        
        // 4. 提取并转换HEX数据
        std::string jsonString(data + CLIENT_HEADER_SIZE, dataLength);
        std::cout << "JSON字符串: " << jsonString << std::endl;
    
        // 5. 解析JSON
        Json::Reader reader;
        if (!reader.parse(jsonString, jsonData)) {
            std::cout << "JSON解析失败" << std::endl;
            return false;
        }
        
        // 6. 验证JSON基本结构
        if (!jsonData.isObject()) {
            std::cout << "JSON不是对象类型" << std::endl;
            return false;
        }
        
        // 7. 验证action字段
        if (!jsonData.isMember("action") || !jsonData["action"].isString()) {
            std::cout << "缺少action字段或类型错误" << std::endl;
            return false;
        }
        
        /*8. 根据action设置消息类型并验证必需字段*/
        std::string action = jsonData["action"].asString();
        // 用户请求字段处理
        if (action == "signup") {
            if (!validateRegisterFields(jsonData)) return false;
            msgType = MSG_REGISTER;
        }
        else if (action == "signin") {
            if (!validateLoginFields(jsonData)) return false;
            msgType = MSG_LOGIN;
        }
        else if (action == "resetPassword") {
            if (!validateChangePasswordFields(jsonData)) return false;
            msgType = MSG_CHANGE_PASSWORD;
        }
        // 设备请求字段处理
        else if (action == "addDevice") {
            if (!jsonData.isMember("device_name") || !jsonData["device_name"].isString() ||
                !jsonData.isMember("device_type") || !jsonData["device_type"].isString() ||
                !jsonData.isMember("room_id")     || !jsonData["room_id"].isInt()        ||
                !jsonData.isMember("image")       || !jsonData["image"].isString()) {
                std::cout << "添加设备请求缺少必需字段或类型错误" << std::endl;
                return false;
            }
            msgType = MSG_ADD_DEVICE;
        }
        else if (action == "deleteDevice") {
            if (!jsonData.isMember("device_id") || !jsonData["device_id"].isInt()) {
                std::cout << "删除设备请求缺少device_id字段或类型错误" << std::endl;
                return false;
            }
            msgType = MSG_DELETE_DEVICE;
        }
        else if (action == "listDevices") {
            // 可选：支持按房间过滤
            if (jsonData.isMember("room_id") && !jsonData["room_id"].isInt()) {
                std::cout << "listDevices room_id 类型错误" << std::endl;
                return false;
            }
            msgType = MSG_LIST_DEVICES;
        }
        else if (action == "updateDevice") {
            if (!jsonData.isMember("device_id")  || !jsonData["device_id"].isInt()     ||
                !jsonData.isMember("device_name")|| !jsonData["device_name"].isString()||
                !jsonData.isMember("device_type")|| !jsonData["device_type"].isString()||
                !jsonData.isMember("room_id")    || !jsonData["room_id"].isInt()       ||
                !jsonData.isMember("image")      || !jsonData["image"].isString()) {
                std::cout << "更新设备请求缺少必需字段或类型错误" << std::endl;
                return false;
            }
            msgType = MSG_UPDATE_DEVICE;
        }
        else if (action == "controlDevice") {
            if (!validateControlDeviceFields(jsonData)) return false;
            msgType = MSG_CONTROL_DEVICE;
        }
        // 房间请求字段处理
        else if (action == "addRoom") {
            if (!jsonData.isMember("room_type") || !jsonData["room_type"].isString() ||
                !jsonData.isMember("room_name") || !jsonData["room_name"].isString()) {
                std::cout << "addRoom 缺少必需字段或类型错误" << std::endl;
                return false;
            }
            msgType = MSG_ADD_ROOM;
        }
        else if (action == "deleteRoom") {
            if (!jsonData.isMember("room_id") || !jsonData["room_id"].isInt()) {
                std::cout << "deleteRoom 缺少 room_id 或类型错误" << std::endl;
                return false;
            }
            msgType = MSG_DELETE_ROOM;
        }
        else if (action == "listRooms") {
            msgType = MSG_LIST_ROOMS;
        }
        else if (action == "updateRoom") {
            if (!jsonData.isMember("room_id")   || !jsonData["room_id"].isInt()    ||
                !jsonData.isMember("room_type") || !jsonData["room_type"].isString() ||
                !jsonData.isMember("room_name") || !jsonData["room_name"].isString()) {
                std::cout << "updateRoom 缺少必需字段或类型错误" << std::endl;
                return false;
            }
            msgType = MSG_UPDATE_ROOM;
        }
        // 人脸请求字段处理
        else if (action == "faceUpsert") {
            if (!validateFaceUpsertFields(jsonData)) return false;
            msgType = MSG_FACE_UPSERT;
        }
        else if (action == "faceDelete") {
            if (!validateFaceDeleteFields(jsonData)) return false;
            msgType = MSG_FACE_DELETE;
        }
        else if (action == "faceQuery") {
            if (!validateFaceQueryFields(jsonData)) return false;
            msgType = MSG_FACE_QUERY;
        }
        else if (action == "faceRecognize") {
            if (!validateFaceRecognizeFields(jsonData)) return false;
            msgType = MSG_FACE_RECOGNIZE;
        }
        else {
            std::cout << "不支持的action: " << action << std::endl;
            return false;
        }
        return true;
    }

    //HTTP协议解析
    bool parseHttpJson(const char* data, size_t length,
                       MessageType& msgType, Json::Value& jsonData) override {
        Json::Reader reader;
        if (!reader.parse(data, data + length, jsonData)) {
            std::cout << "HTTP JSON解析失败" << std::endl;
            return false;
        }

        if (!jsonData.isMember("action") || !jsonData["action"].isString()) {
            std::cout << "HTTP JSON缺少action字段或类型错误" << std::endl;
            return false;
        }

        // 根据 action 进行具体字段校验
        std::string action = jsonData["action"].asString();
        // 用户请求字段处理
        if (action == "signup") {
            if (!validateRegisterFields(jsonData)) return false;
            msgType = MSG_REGISTER;
        }
        else if (action == "signin") {
            if (!validateLoginFields(jsonData)) return false;
            msgType = MSG_LOGIN;
        }
        else if (action == "resetPassword") {
            if (!validateChangePasswordFields(jsonData)) return false;
            msgType = MSG_CHANGE_PASSWORD;
        }
        // 设备请求字段处理
        else if (action == "addDevice") {
            if (!jsonData.isMember("device_name") || !jsonData["device_name"].isString() ||
                !jsonData.isMember("device_type") || !jsonData["device_type"].isString() ||
                !jsonData.isMember("room_id")     || !jsonData["room_id"].isInt()        ||
                !jsonData.isMember("image")       || !jsonData["image"].isString()) {
                std::cout << "添加设备请求缺少必需字段或类型错误" << std::endl;
                return false;
            }
            msgType = MSG_ADD_DEVICE;
        }
        else if (action == "deleteDevice") {
            if (!jsonData.isMember("device_id") || !jsonData["device_id"].isInt()) {
                std::cout << "删除设备请求缺少device_id字段或类型错误" << std::endl;
                return false;
            }
            msgType = MSG_DELETE_DEVICE;
        }
        else if (action == "listDevices") {
            if (jsonData.isMember("room_id") && !jsonData["room_id"].isInt()) {
                std::cout << "listDevices room_id 类型错误" << std::endl;
                return false;
            }
            msgType = MSG_LIST_DEVICES;
        }
        else if (action == "updateDevice") {
            if (!jsonData.isMember("device_id")  || !jsonData["device_id"].isInt()     ||
                !jsonData.isMember("device_name")|| !jsonData["device_name"].isString()||
                !jsonData.isMember("device_type")|| !jsonData["device_type"].isString()||
                !jsonData.isMember("room_id")    || !jsonData["room_id"].isInt()       ||
                !jsonData.isMember("image")      || !jsonData["image"].isString()) {
                std::cout << "更新设备请求缺少必需字段或类型错误" << std::endl;
                return false;
            }
            msgType = MSG_UPDATE_DEVICE;
        }
        else if (action == "controlDevice") {
            if (!validateControlDeviceFields(jsonData)) return false;
            msgType = MSG_CONTROL_DEVICE;
        }
        // 房间请求字段处理
        else if (action == "addRoom") {
            if (!jsonData.isMember("room_type") || !jsonData["room_type"].isString() ||
                !jsonData.isMember("room_name") || !jsonData["room_name"].isString()) {
                std::cout << "addRoom 缺少必需字段或类型错误" << std::endl;
                return false;
            }
            msgType = MSG_ADD_ROOM;
        }
        else if (action == "deleteRoom") {
            if (!jsonData.isMember("room_id") || !jsonData["room_id"].isInt()) {
                std::cout << "deleteRoom 缺少 room_id 或类型错误" << std::endl;
                return false;
            }
            msgType = MSG_DELETE_ROOM;
        }
        else if (action == "listRooms") {
            msgType = MSG_LIST_ROOMS;
        }
        else if (action == "updateRoom") {
            if (!jsonData.isMember("room_id")   || !jsonData["room_id"].isInt()    ||
                !jsonData.isMember("room_type") || !jsonData["room_type"].isString() ||
                !jsonData.isMember("room_name") || !jsonData["room_name"].isString()) {
                std::cout << "updateRoom 缺少必需字段或类型错误" << std::endl;
                return false;
            }
            msgType = MSG_UPDATE_ROOM;
        }
        // 人脸请求字段处理
        else if (action == "faceUpsert") {
            if (!validateFaceUpsertFields(jsonData)) return false;
            msgType = MSG_FACE_UPSERT;
        }
        else if (action == "faceDelete") {
            if (!validateFaceDeleteFields(jsonData)) return false;
            msgType = MSG_FACE_DELETE;
        }
        else if (action == "faceQuery") {
            if (!validateFaceQueryFields(jsonData)) return false;
            msgType = MSG_FACE_QUERY;
        }
        else if (action == "faceRecognize") {
            if (!validateFaceRecognizeFields(jsonData)) return false;
            msgType = MSG_FACE_RECOGNIZE;
        }
        else {
            std::cout << "不支持的action: " << action << std::endl;
            return false;
        }
        return true;
    }

    // MQTT协议解析
    bool parseMqttMessage(const std::string& topic, const std::string& payload,
                          MessageType& msgType, Json::Value& jsonData) override {
    
    Json::Reader reader;
    if (!reader.parse(payload, jsonData)) {
        std::cout << "MQTT JSON解析失败" << std::endl;
        return false;
    }
    
    // 字段校验,检查必需字段是否存在（以action字段为主）
    if (!jsonData.isMember("action") || !jsonData["action"].isString()) {
        std::cout << "MQTT JSON缺少action字段或类型错误" << std::endl;
        return false;
    }
    if (jsonData["action"].asString().empty()) {
        std::cout << "MQTT JSON action字段不能为空" << std::endl;
        return false;
    }


    // 根据action设置消息类型并验证必需字段
    std::string action = jsonData["action"].asString();
    //用户请求字段处理
    if (action == "signup") {
        if (!validateRegisterFields(jsonData)) {
            return false;
        }
        msgType = MSG_REGISTER;
    }
    else if (action == "signin") {
        if (!validateLoginFields(jsonData)) {
            return false;
        }
        msgType = MSG_LOGIN;
    }
    else if (action == "resetPassword") {
        if (!validateChangePasswordFields(jsonData)) {
            return false;
        }
        msgType = MSG_CHANGE_PASSWORD;
    }
    // 设备请求字段处理
    else if (action == "addDevice") {
        if (!jsonData.isMember("device_name") || !jsonData["device_name"].isString() ||
            !jsonData.isMember("device_type") || !jsonData["device_type"].isString() ||
            !jsonData.isMember("room_id")     || !jsonData["room_id"].isInt()        ||
            !jsonData.isMember("image")       || !jsonData["image"].isString()) {
            std::cout << "添加设备请求缺少必需字段" << std::endl;
            return false;
        }
        msgType = MSG_ADD_DEVICE;
    }
    else if (action == "deleteDevice") {
        if (!jsonData.isMember("device_id") || !jsonData["device_id"].isInt()) {
            std::cout << "删除设备请求缺少必须字段" << std::endl;
            return false;
        }
        msgType = MSG_DELETE_DEVICE;
    }
    else if (action == "listDevices") {
        if (jsonData.isMember("room_id") && !jsonData["room_id"].isInt()) {
            std::cout << "listDevices room_id 类型错误" << std::endl;
            return false;
        }
        msgType = MSG_LIST_DEVICES;
    }
    else if (action == "updateDevice") {
        if (!jsonData.isMember("device_id")  || !jsonData["device_id"].isInt()     ||
            !jsonData.isMember("device_name")|| !jsonData["device_name"].isString()||
            !jsonData.isMember("device_type")|| !jsonData["device_type"].isString()||
            !jsonData.isMember("room_id")    || !jsonData["room_id"].isInt()       ||
            !jsonData.isMember("image")      || !jsonData["image"].isString()) {
            std::cout << "更新设备请求缺少必需字段" << std::endl;
            return false;
        }
        msgType = MSG_UPDATE_DEVICE;
    }
    else if (action == "controlDevice") {
        if (!validateControlDeviceFields(jsonData)) return false;
        msgType = MSG_CONTROL_DEVICE;
    }
    // 房间请求字段处理
    else if (action == "addRoom") {
        if (!jsonData.isMember("room_type") || !jsonData["room_type"].isString() ||
            !jsonData.isMember("room_name") || !jsonData["room_name"].isString()) {
            std::cout << "addRoom 缺少必需字段或类型错误" << std::endl;
            return false;
        }
        msgType = MSG_ADD_ROOM;
    }
    else if (action == "deleteRoom") {
        if (!jsonData.isMember("room_id") || !jsonData["room_id"].isInt()) {
            std::cout << "deleteRoom 缺少 room_id 或类型错误" << std::endl;
            return false;
        }
        msgType = MSG_DELETE_ROOM;
    }
    else if (action == "listRooms") {
        msgType = MSG_LIST_ROOMS;
    }
    else if (action == "updateRoom") {
        if (!jsonData.isMember("room_id")   || !jsonData["room_id"].isInt()    ||
            !jsonData.isMember("room_type") || !jsonData["room_type"].isString() ||
            !jsonData.isMember("room_name") || !jsonData["room_name"].isString()) {
            std::cout << "updateRoom 缺少必需字段或类型错误" << std::endl;
            return false;
        }
        msgType = MSG_UPDATE_ROOM;
    }
    // 人脸请求字段处理
    else if (action == "faceUpsert") {
        if (!validateFaceUpsertFields(jsonData)) return false;
        msgType = MSG_FACE_UPSERT;
    }
    else if (action == "faceDelete") {
        if (!validateFaceDeleteFields(jsonData)) return false;
        msgType = MSG_FACE_DELETE;
    }
    else if (action == "faceQuery") {
        if (!validateFaceQueryFields(jsonData)) return false;
        msgType = MSG_FACE_QUERY;
    }
    else if (action == "faceRecognize") {
        if (!validateFaceRecognizeFields(jsonData)) return false;
        msgType = MSG_FACE_RECOGNIZE;
    }
    else {
        std::cout << "不支持的action: " << action << std::endl;
        return false;
    }
    
    return true;
}

// 构建消息, 主要用于向客户端发送数据,提示客户端使用十六进制字符串
// 这里的body是十六进制字符串形式
// 客户端需要将字符串转换为十六进制格式后再发送
std::string buildMessage(MessageType msgType, const std::string& body) override {
        
        ClientProtocolHeader header;
        header.magic = 0xAA;
        header.bodyLength = htonl(static_cast<uint32_t>(body.length())); 
        std::string message(reinterpret_cast<char*>(&header), CLIENT_HEADER_SIZE);
        message.append(body);
        return message;
    }
    
    
    std::string buildJsonResponse(MessageType msgType, const Json::Value& responseData) {
        Json::FastWriter writer;
        std::string jsonStr = writer.write(responseData);
        return buildMessage(msgType, jsonStr);
    }
    
    std::string buildErrorResponse(const std::string& errorMessage) {
        Json::Value errorData;
        errorData["status"] = "error";
        errorData["message"] = errorMessage;
        errorData["timestamp"] = static_cast<int64_t>(std::time(nullptr));
        return buildJsonResponse(MSG_ERROR, errorData);
    }
    
    std::string buildAuthResponse(bool success, const std::string& message, const std::string& action) {
        Json::Value response;
        response["action"] = action + "_result";
        response["success"] = success;
        response["message"] = message;
        response["timestamp"] = static_cast<int64_t>(std::time(nullptr));
        return buildJsonResponse(MSG_RESPONSE, response);
    }

private:
    // 验证注册请求字段
    bool validateRegisterFields(const Json::Value& data) {
        if (!data.isMember("username") || !data["username"].isString() ||
            !data.isMember("password") || !data["password"].isString()) {
            std::cout << "注册请求缺少必需字段" << std::endl;
            return false;
        }
        
        if (data["username"].asString().empty() || 
            data["password"].asString().empty()) {
            std::cout << "用户名或密码为空" << std::endl;
            return false;
        }
        
        return true;
    }
    
    // 验证登录请求字段
    bool validateLoginFields(const Json::Value& data) {
        if (!data.isMember("username") || !data["username"].isString() ||
            !data.isMember("password") || !data["password"].isString()) {
            std::cout << "登录请求缺少必需字段" << std::endl;
            return false;
        }
        
        if (data["username"].asString().empty() || 
            data["password"].asString().empty()) {
            std::cout << "用户名或密码为空" << std::endl;
            return false;
        }
        
        return true;
    }
    
    // 验证修改密码请求字段
    bool validateChangePasswordFields(const Json::Value& data) {
        if (!data.isMember("username") || !data["username"].isString() ||
            !data.isMember("password") || !data["password"].isString()) {
            std::cout << "修改密码请求缺少必需字段" << std::endl;
            return false;
        }
        
        if (data["username"].asString().empty() || 
            data["password"].asString().empty()) {
            std::cout << "用户名或密码为空" << std::endl;
            return false;
        }
        
        return true;
    }

    /*其余设备请求的验证在主逻辑中已经进行实现*/

    // 验证控制设备请求字段
    bool validateControlDeviceFields(const Json::Value& data) {
        if (!data.isMember("device_id") || !data["device_id"].isInt()) {
            std::cout << "控制设备请求缺少 device_id 或类型错误(需int)" << std::endl;
            return false;
        }
        if (!data.isMember("command") || !data["command"].isString() ||
            data["command"].asString().empty()) {
            std::cout << "控制设备请求缺少 command 或类型错误" << std::endl;
            return false;
        }
        return true;
    }

    /*人脸相关校验提取到协议层 */
    bool validateFaceUpsertFields(const Json::Value& data) {
        if (!data.isMember("username") || !data["username"].isString()) {
            std::cout << "faceUpsert 缺少 username 或类型错误" << std::endl;
            return false;
        }
        if (data.isMember("face_base64") && !data["face_base64"].isString()) {
            std::cout << "faceUpsert face_base64 类型错误" << std::endl;
            return false;
        }
        return true;
    }
    bool validateFaceDeleteFields(const Json::Value& data) {
        if (!data.isMember("username") || !data["username"].isString()) {
            std::cout << "faceDelete 缺少 username 或类型错误" << std::endl;
            return false;
        }
        return true;
    }
    bool validateFaceQueryFields(const Json::Value& data) {
        if (!data.isMember("username") || !data["username"].isString()) {
            std::cout << "faceQuery 缺少 username 或类型错误" << std::endl;
            return false;
        }
        return true;
    }
    bool validateFaceRecognizeFields(const Json::Value& data) {
        if (!data.isMember("face_base64") || !data["face_base64"].isString()) {
            std::cout << "faceRecognize 缺少 face_base64 或类型错误" << std::endl;
            return false;
        }
        return true;
    }

};

// 协议处理工厂类,负责进行协议处理器的创建，用于进行协议的解析和处理
class ProtocolHandlerFactory {
public:
    static std::shared_ptr<ProtocolHandler> createClientHandler() {
        return std::make_shared<ClientProtocolHandler>();
    }
    
    static std::shared_ptr<ProtocolHandler> createHandler(const std::string& type = "client") {
        if (type == "client" || type == "hex") {
            return std::make_shared<ClientProtocolHandler>();
        }
        return nullptr;
    }
};

#endif // PROTOCOL_HANDLER_H
