#include <iostream>
#include <thread>
#include <memory>
#include "TcpServer.h"
#include "HttpServer.h"
#include "MqttServer.h"
#include "ThreadPool.h"
#include "DatabaseManager.h"
#include "ProtocolHandler.h"
#include "MessageHandler.h"
#include "FaceEngine.h"
#include "FaceManager.h"

int main() {
    // 初始化SeetaFace 引擎
    auto engine = std::make_shared<SeetaFaceEngine>();
    const std::string model_dir = "/home/crystalchen/SeetaFace2/Build/bin/model";
    if (!engine->init(model_dir)) {
        std::cerr << "SeetaFace 初始化失败" << std::endl;
        return 1;
    }
    std::cerr << "SeetaFace 初始化完成" << std::endl;

    // 数据库管理
    auto dbManager = new DatabaseManager("./server.db");
    if (!dbManager->initialize()) {
        std::cerr << "数据库初始化失败！" << std::endl;
        return -1;
    }

    // MQTT服务
    auto mqttServer = new MqttServer("192.168.101.25", 1883);

    // 协议/消息处理
    auto protocolHandler = ProtocolHandlerFactory::createClientHandler();
    auto messageHandler = new MessageHandler(dbManager, protocolHandler, mqttServer);

    // FaceManager 
    static std::unique_ptr<FaceManager> faceMgr;
    faceMgr = std::make_unique<FaceManager>(dbManager, engine);
    //faceMgr->setThreshold(0.80f);人脸识别阈值，默认0.80
    messageHandler->setFaceManager(faceMgr.get());

    // TCP服务器
    auto tcpServer = new TcpServer("192.168.101.25", "8888", dbManager, protocolHandler, messageHandler);
    std::cout << "TCP服务器已启动，等待客户端连接........" << std::endl;
    std::thread tcpThread([&]() { tcpServer->listenClientConnect(100); });

    // HTTP服务器
    std::string http_ip = "192.168.101.25";
    int http_port = 8080;
    std::cout << "HTTP服务器已启动，等待客户端连接......." << std::endl;
    HttpServer httpServer(http_ip, http_port, messageHandler);
    std::thread httpThread([&]() { httpServer.start(); });

    // 启动MQTT
    mqttServer->startService(protocolHandler, messageHandler);
    std::cout << "MQTT消息服务已启动，等待连接..........." << std::endl;
    mqttServer->publish("test/topic", "你好，MQTT!");

    // 周期打印,显示用户/房间/设备信息
    std::thread dbQueryThread([dbManager]() {
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(30));
            dbManager->printAllUsers();
        }
    });
    dbQueryThread.detach();

    std::thread dbRoomQueryThread([dbManager]() {
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(30));
            dbManager->printAllRooms();
        }
    });
    dbRoomQueryThread.detach();

    std::thread dbDeviceQueryThread([dbManager]() {
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(30));
            dbManager->printAllDevices();
        }
    });
    dbDeviceQueryThread.detach();


    // 等待线程结束
    tcpThread.join();
    httpThread.join();

    delete tcpServer;
    delete messageHandler;
    delete mqttServer;
    delete dbManager;
    return 0;
}

