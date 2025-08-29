#pragma once
#include <mosquitto.h>
#include <jsoncpp/json/json.h>
#include <iostream>
#include <string>
#include <thread>
#include <memory>
#include <functional>
#include "ProtocolHandler.h"
#include "ThreadPool.h"

//全局线程池，负责处理MQTT请求的并发
extern ThreadPool pool;

class MqttServer {
public:
    // 构造函数，初始化MQTT客户端
    explicit MqttServer(const std::string& host, int port)
        : mosq(nullptr), host(host), port(port), running(false) {
        // 初始化mosquitto库
        mosquitto_lib_init();
        // 创建mosquitto客户端实例
        mosq = mosquitto_new(nullptr, true, this);
        // 检查mosq是否创建成功
        if (!mosq) {
            std::cerr << "[MQTT] mosquitto_new 失败" << std::endl;
            return;
        }
        // 设置回调函数，负责连接成功后的处理
        mosquitto_connect_callback_set(mosq, on_connect);
        // 设置消息接收回调函数，负责处理接收到的消息
        mosquitto_message_callback_set(mosq, on_message);
    }

    //析构函数，停止MQTT服务并清理资源
    ~MqttServer() {
        stop();
        //如果mosq不为空，销毁mosq对象
        if (mosq) {
            mosquitto_destroy(mosq);
            mosq = nullptr;
        }
        // 清理libmosquitto库
        mosquitto_lib_cleanup();
    }

    // 注入协议解析器，负责解析和构建协议
    void setProtocolHandler(std::shared_ptr<ProtocolHandler> ph) { protocolHandler = std::move(ph); }

    // 业务路由：解析成功后回调 (类型, 主题, JSON)
    void setRouter(std::function<void(MessageType, const std::string&, const Json::Value&)> cb) {
        router = std::move(cb);
    }

    //模板成员函数，用于启动基于MQTT协议的服务
    template <typename Handler>
    void startService(std::shared_ptr<ProtocolHandler> ph, Handler* handler) {
        setProtocolHandler(std::move(ph));
        setRouter([this, handler](MessageType t, const std::string& topic, const Json::Value& root) {
            auto respond = [this](const Json::Value& resp) {
                Json::FastWriter writer;
                this->publish("response", writer.write(resp));
            };
            switch (t) {
                // 用户管理
                case MSG_REGISTER:        handler->handleRegisterMqtt(topic, root); break;
                case MSG_LOGIN:           handler->handleLoginMqtt(topic, root); break;
                case MSG_CHANGE_PASSWORD: handler->handleChangePasswordMqtt(topic, root); break;

                // 房间管理 
                case MSG_ADD_ROOM:        handler->handleAddRoomMqtt(topic, root); break;
                case MSG_DELETE_ROOM:     handler->handleDeleteRoomMqtt(topic, root); break;
                case MSG_LIST_ROOMS:      handler->handleListRoomsMqtt(topic, root); break;
                case MSG_UPDATE_ROOM:     handler->handleUpdateRoomMqtt(topic, root); break;

                // 设备管理
                case MSG_ADD_DEVICE:      handler->handleAddDeviceMqtt(topic, root); break;
                case MSG_DELETE_DEVICE:   handler->handleDeleteDeviceMqtt(topic, root); break;
                case MSG_LIST_DEVICES:    handler->handleListDevicesMqtt(topic, root); break;
                case MSG_UPDATE_DEVICE:   handler->handleUpdateDeviceMqtt(topic, root); break;
                case MSG_CONTROL_DEVICE:  handler->handleControlDeviceMqtt(topic, root); break;

                // 人脸管理
                case MSG_FACE_UPSERT:     handler->handleFaceUpsertMqtt(topic, root); break;
                case MSG_FACE_DELETE:     handler->handleFaceDeleteMqtt(topic, root); break;
                case MSG_FACE_QUERY:      handler->handleFaceQueryMqtt(topic, root); break;
                case MSG_FACE_RECOGNIZE:  handler->handleFaceRecognizeMqtt(topic, root); break;
                default: break;
            }
        });
        start();
    }

    // 启动MQTT服务
    void start() {
        // 检查mosq是否创建成功
        if (!mosq) return;
        // 连接MQTT代理
        const int rc = mosquitto_connect_async(mosq, host.c_str(), port, 60);
        if (rc != MOSQ_ERR_SUCCESS) {
            std::cerr << "[MQTT] 连接失败: " << mosquitto_strerror(rc) << std::endl;
            return;
        }
        running = true;
        loopThread = std::thread([this]() { mosquitto_loop_forever(mosq, -1, 1); });
    }

    // 停止MQTT服务
    void stop() {
        if (running) {
            running = false;
            if (mosq) mosquitto_disconnect(mosq);
            if (loopThread.joinable()) loopThread.join();
        }
    }

    // 发布消息，负责将消息发送到指定主题
    void publish(const std::string& topic, const std::string& message) {
        if (!mosq) return;
        mosquitto_publish(mosq, nullptr, topic.c_str(),
                          static_cast<int>(message.size()),
                          message.c_str(), 2, false);
    }

    // 订阅消息，负责订阅指定主题
    bool subscribe(const std::string& topic, int qos = 2) {
        if (!mosq) return false;
        const int rc = mosquitto_subscribe(mosq, nullptr, topic.c_str(), qos);
        if (rc != MOSQ_ERR_SUCCESS) {
            std::cerr << "[MQTT] 订阅失败: " << topic << " rc=" << rc << std::endl;
            return false;
        }
        return true;
    }

private:
    // MQTT连接成功回调函数，用于处理连接成功事件
    static void on_connect(struct mosquitto* mosq, void* /*userdata*/, int rc) {
        std::cout << "[MQTT] 已连接至服务端, rc=" << rc << std::endl;
        // 该MQTT服务下的订阅主题为smarthome，用于接收智能家居设备的状态更新
        mosquitto_subscribe(mosq, nullptr, "smarthome", 2);
    }

    // 消息接收回调函数,负责进行消息的解析和路由
    static void on_message(struct mosquitto*, void* userdata, const struct mosquitto_message* msg) {
        auto* server = static_cast<MqttServer*>(userdata);
        if (!server) return;

        const std::string topic = (msg && msg->topic) ? msg->topic : "";
        const std::string payload = (msg && msg->payload && msg->payloadlen > 0)
            ? std::string(static_cast<char*>(msg->payload), msg->payloadlen)
            : std::string();

        // 忽略响应主题，防止自反馈，从而陷入死循环
        if (topic == "response") {
            return;
        }

        if (!server->protocolHandler) {
            std::cout << "[MQTT] 未配置 ProtocolHandler，跳过解析。topic=" << topic << std::endl;
            return;
        }

        Json::Value root;
        MessageType msgType;

        // 解析MQTT消息
        if (!server->protocolHandler->parseMqttMessage(topic, payload, msgType, root)) {
            std::cerr << "[MQTT] 协议解析或字段校验失败" << std::endl;
            Json::Value resp;
            resp["status"] = "fail";
            resp["message"] = "协议解析或字段校验失败";
            Json::FastWriter writer;
            server->publish("response", writer.write(resp));
            return;
        }
        //将解析后的消息分发到相应的处理函数
        if (server->router) {
            server->router(msgType, topic, root);
        } else {
            Json::Value resp;
            resp["status"] = "fail";
            resp["message"] = "未设置业务路由";
            Json::FastWriter writer;
            server->publish("response", writer.write(resp));
        }
    }

    // MQTT相关参数，负责与MQTT代理进行连接和通信 
    struct mosquitto* mosq{nullptr};
    // 保存主机地址和端口号
    std::string host;
    int port{0};
    // MQTT事件循环线程，用于处理MQTT事件，防止线程阻塞
    std::thread loopThread;
    bool running{false};
    // MQTT协议处理器，用于解析和处理MQTT消息
    std::shared_ptr<ProtocolHandler> protocolHandler;
    // MQTT业务回调函数，负责将解析后的消息分发到相应的处理函数
    std::function<void(MessageType, const std::string&, const Json::Value&)> router; 
};