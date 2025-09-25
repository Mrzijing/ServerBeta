# SmartHomeBetaTest - 智能家居服务器

一个功能完整的智能家居服务器应用，集成了用户管理、设备控制、人脸识别、多协议通信等功能。

## 项目概述

SmartHomeBetaTest 是一个用 C++ 开发的高性能智能家居后端服务器，支持多种通信协议（TCP、HTTP、MQTT），具备人脸识别能力，可以管理用户、房间和智能设备。

## 核心功能

### 🔐 用户管理系统
- 用户注册与登录
- 密码修改与验证
- 人脸特征存储与识别
- 基于 SQLite 数据库的用户数据持久化

### 🏠 房间与设备管理
- 房间创建、删除、更新和查询
- 智能设备添加、删除、更新和列表显示
- 设备状态管理和控制

### 👤 人脸识别系统
- 基于 SeetaFace2 引擎的高精度人脸识别
- 人脸特征提取与存储
- 实时人脸识别验证
- 支持 Base64 图像数据处理

### 🌐 多协议通信
- **TCP 服务器**: 端口 8888，支持自定义协议
- **HTTP 服务器**: 端口 8080，RESTful API 接口
- **MQTT 服务器**: 端口 1883，物联网设备通信

### ⚡ 高性能架构
- 多线程并发处理
- 线程池管理
- Epoll 事件驱动 I/O
- 异步消息处理

## 技术栈

### 核心依赖
- **C++14**: 现代 C++ 标准
- **SQLite3**: 轻量级数据库
- **JsonCpp**: JSON 数据处理
- **SeetaFace2**: 人脸识别引擎
- **OpenCV**: 图像处理（可选）
- **libevent**: HTTP 服务器支持
- **libmosquitto**: MQTT 协议支持

### 系统架构

```
┌─────────────────────────────────────────────────────────────────┐
│                        SmartHomeBetaTest                        │
├─────────────────────────────────────────────────────────────────┤
│  TCP服务器(8888)  │  HTTP服务器(8080)  │  MQTT服务器(1883)   │
├─────────────────────────────────────────────────────────────────┤
│                    MessageHandler                               │
│                  (业务逻辑处理层)                                │
├─────────────────────────────────────────────────────────────────┤
│ DatabaseManager │ FaceManager │ ProtocolHandler │ ThreadPool    │
├─────────────────────────────────────────────────────────────────┤
│   SQLite数据库   │ SeetaFace2  │   协议解析器    │   线程池      │
└─────────────────────────────────────────────────────────────────┘
```

## 项目结构

```
ServerBeta/
├── main.cpp                  # 主程序入口
├── CMakeLists.txt            # CMake 构建配置
├── server.db                 # SQLite 数据库文件
├── DatabaseManager.h         # 数据库管理器
├── MessageHandler.h          # 消息处理器
├── ProtocolHandler.h         # 协议处理器
├── TcpServer.h              # TCP 服务器
├── HttpServer.h             # HTTP 服务器
├── MqttServer.h             # MQTT 服务器
├── FaceEngine.h             # 人脸识别引擎
├── FaceManager.h            # 人脸管理器
├── FaceTool.h               # 人脸工具类 (Base64编解码)
├── ThreadPool.h             # 线程池实现
└── See_sqlitedb.sh          # 数据库查看脚本
```

## 核心组件说明

### 1. DatabaseManager (数据库管理器)
- 管理用户、房间、设备的 CRUD 操作
- 支持人脸特征数据存储
- 线程安全的数据库访问
- 自动数据库结构迁移

### 2. MessageHandler (消息处理器)
- 统一的业务逻辑处理中心
- 支持 TCP、HTTP、MQTT 多协议请求处理
- JSON 数据解析与响应构建
- 集成人脸识别功能

### 3. FaceEngine & FaceManager (人脸识别系统)
- `FaceEngine`: 封装 SeetaFace2 API，提供人脸特征提取
- `FaceManager`: 人脸识别业务逻辑，包括入库、查询、识别
- 支持余弦相似度计算
- 可配置识别阈值（默认 0.80）

### 4. 协议处理器
- `ProtocolHandler`: 协议解析基类
- `ClientProtocolHandler`: 客户端协议具体实现
- 支持自定义二进制协议头 + JSON 数据体

### 5. 服务器组件
- `TcpServer`: 基于 epoll 的高性能 TCP 服务器
- `HttpServer`: 基于 libevent 的 HTTP 服务器
- `MqttServer`: MQTT 消息代理客户端

## API 接口

### TCP 协议接口 (端口 8888)
支持的操作类型：
- `register`: 用户注册
- `login`: 用户登录  
- `change_password`: 修改密码
- `add_room`: 添加房间
- `delete_room`: 删除房间
- `list_rooms`: 房间列表
- `update_room`: 更新房间
- `add_device`: 添加设备
- `delete_device`: 删除设备
- `list_devices`: 设备列表
- `update_device`: 更新设备

### HTTP RESTful API (端口 8080)
- `POST /register`: 用户注册
- `POST /login`: 用户登录
- `POST /change_password`: 修改密码
- `POST /add_room`: 添加房间
- `DELETE /delete_room`: 删除房间
- `GET /list_rooms`: 房间列表
- `POST /update_room`: 更新房间
- `POST /add_device`: 添加设备
- `DELETE /delete_device`: 删除设备
- `GET /list_devices`: 设备列表
- `POST /update_device`: 更新设备

### MQTT 主题 (端口 1883)
- 支持发布/订阅模式
- 自动消息路由处理
- 集成业务逻辑处理

## 构建与部署

### 系统依赖
```bash
# Ubuntu/Debian 系统
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    libjsoncpp-dev \
    libsqlite3-dev \
    libevent-dev \
    libmosquitto-dev \
    pkg-config
```

### SeetaFace2 安装
1. 下载并编译 SeetaFace2
2. 修改 CMakeLists.txt 中的 `SEETA_ROOT` 路径
3. 确保模型文件路径正确（main.cpp 中的 `model_dir`）

### 编译步骤
```bash
# 克隆项目
git clone https://github.com/Mrzijing/ServerBeta.git
cd ServerBeta

# 创建构建目录
mkdir -p build && cd build

# 配置和编译
cmake ..
make -j$(nproc)

# 运行服务器
./Server
```

### 配置说明
- **服务器IP**: 在 main.cpp 中修改服务器绑定地址 (默认: 192.168.101.25)
- **端口配置**: 
  - TCP: 8888
  - HTTP: 8080  
  - MQTT: 1883
- **数据库文件**: `./server.db` (自动创建)
- **人脸模型路径**: 需要配置正确的 SeetaFace2 模型路径

## 运行效果

启动服务器后，会看到以下输出：
```
SeetaFace 初始化完成
数据库初始化成功
TCP服务器已启动，等待客户端连接........
HTTP服务器已启动，等待客户端连接.......
MQTT消息服务已启动，等待连接...........
```

服务器会每30秒自动打印用户、房间、设备信息统计。

## 特性亮点

1. **模块化设计**: 各个功能模块高度解耦，便于维护和扩展
2. **协议无关**: 业务逻辑与通信协议分离，支持多协议接入
3. **高并发**: 基于线程池和事件驱动，支持大量并发连接
4. **人脸识别**: 集成工业级人脸识别算法，支持实时识别
5. **数据持久化**: 基于 SQLite，支持数据持久化和迁移
6. **跨平台**: 基于标准 C++ 和开源库，支持 Linux 平台

## 开发计划

- [ ] 添加设备状态实时推送
- [ ] 支持更多人脸识别算法
- [ ] 添加 Web 管理界面
- [ ] 支持集群部署
- [ ] 添加日志系统
- [ ] 支持 HTTPS 和加密通信

## 许可证

本项目采用开源许可证，具体许可证信息请查看项目根目录下的 LICENSE 文件。

## 贡献指南

欢迎提交 Issue 和 Pull Request！在贡献代码前，请确保：
1. 代码风格符合项目规范
2. 添加必要的注释
3. 通过编译测试
4. 更新相关文档

---

*注：本项目仅供学习和研究使用，请遵守相关法律法规。*