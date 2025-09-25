# API 接口文档

## 概述

SmartHomeBetaTest 提供三种通信协议的 API 接口：TCP、HTTP 和 MQTT。所有接口都支持 JSON 格式的数据交换。

## 通用响应格式

所有 API 接口的响应都遵循以下格式：

```json
{
    "success": true/false,
    "message": "响应消息",
    "data": {
        // 具体数据内容（可选）
    }
}
```

## TCP 协议接口 (端口: 8888)

TCP 协议使用自定义二进制协议头 + JSON 数据体的格式。

### 协议头结构
```c
struct ClientProtocolHeader {
    char magic[4];        // 魔数: "SHBT" 
    uint32_t version;     // 协议版本
    uint32_t data_length; // JSON 数据长度
    char reserved[4];     // 保留字段
};
```

### 支持的操作

#### 1. 用户注册
```json
{
    "action": "register",
    "username": "用户名",
    "password": "密码",
    "face_data": "base64编码的人脸图像数据（可选）"
}
```

#### 2. 用户登录
```json
{
    "action": "login",
    "username": "用户名",
    "password": "密码"
}
```

#### 3. 修改密码
```json
{
    "action": "change_password",
    "username": "用户名",
    "old_password": "旧密码",
    "new_password": "新密码"
}
```

#### 4. 添加房间
```json
{
    "action": "add_room",
    "room_type": "房间类型",
    "room_name": "房间名称"
}
```

#### 5. 删除房间
```json
{
    "action": "delete_room",
    "room_id": 房间ID
}
```

#### 6. 房间列表
```json
{
    "action": "list_rooms"
}
```

#### 7. 更新房间
```json
{
    "action": "update_room",
    "room_id": 房间ID,
    "room_type": "新房间类型",
    "room_name": "新房间名称"
}
```

#### 8. 添加设备
```json
{
    "action": "add_device",
    "device_name": "设备名称",
    "device_type": "设备类型",
    "room_id": 房间ID,
    "device_status": "设备状态"
}
```

#### 9. 删除设备
```json
{
    "action": "delete_device",
    "device_id": 设备ID
}
```

#### 10. 设备列表
```json
{
    "action": "list_devices"
}
```

#### 11. 更新设备
```json
{
    "action": "update_device",
    "device_id": 设备ID,
    "device_name": "新设备名称",
    "device_type": "新设备类型",
    "room_id": 新房间ID,
    "device_status": "新设备状态"
}
```

## HTTP RESTful API (端口: 8080)

### 用户管理

#### POST /register - 用户注册
```json
{
    "username": "用户名",
    "password": "密码",
    "face_data": "base64编码的人脸图像数据（可选）"
}
```

#### POST /login - 用户登录
```json
{
    "username": "用户名",
    "password": "密码"
}
```

#### POST /change_password - 修改密码
```json
{
    "username": "用户名",
    "old_password": "旧密码",
    "new_password": "新密码"
}
```

### 房间管理

#### POST /add_room - 添加房间
```json
{
    "room_type": "房间类型",
    "room_name": "房间名称"
}
```

#### DELETE /delete_room - 删除房间
```json
{
    "room_id": 房间ID
}
```

#### GET /list_rooms - 房间列表
无需请求体

#### POST /update_room - 更新房间
```json
{
    "room_id": 房间ID,
    "room_type": "新房间类型",
    "room_name": "新房间名称"
}
```

### 设备管理

#### POST /add_device - 添加设备
```json
{
    "device_name": "设备名称",
    "device_type": "设备类型",
    "room_id": 房间ID,
    "device_status": "设备状态"
}
```

#### DELETE /delete_device - 删除设备
```json
{
    "device_id": 设备ID
}
```

#### GET /list_devices - 设备列表
无需请求体

#### POST /update_device - 更新设备
```json
{
    "device_id": 设备ID,
    "device_name": "新设备名称",
    "device_type": "新设备类型",
    "room_id": 新房间ID,
    "device_status": "新设备状态"
}
```

## MQTT 协议接口 (端口: 1883)

### 主题格式
- 发布主题: `smarthome/request/{action}`
- 响应主题: `smarthome/response/{client_id}`

### 消息格式
MQTT 消息体使用 JSON 格式，结构与 HTTP API 相同。

### 示例

#### 发布消息（用户注册）
主题: `smarthome/request/register`
消息体:
```json
{
    "username": "test_user",
    "password": "test_password",
    "client_id": "client_001"
}
```

#### 响应消息
主题: `smarthome/response/client_001`
消息体:
```json
{
    "success": true,
    "message": "用户注册成功"
}
```

## 人脸识别相关

### 人脸数据格式
人脸图像数据需要 Base64 编码，支持以下格式：
- 标准 Base64: `data:image/jpeg;base64,/9j/4AAQSkZJRgABAQAAAQABAAD...`
- 纯 Base64: `/9j/4AAQSkZJRgABAQAAAQABAAD...`

### 人脸识别流程
1. 用户注册时提供人脸数据（可选）
2. 系统提取人脸特征并存储到数据库
3. 后续可通过人脸数据进行身份验证

### 人脸识别阈值
- 默认阈值: 0.80
- 可通过 `FaceManager::setThreshold()` 修改
- 余弦相似度计算，值越大匹配度越高

## 错误码说明

| 错误码 | 说明 |
|--------|------|
| 1000 | 参数错误 |
| 1001 | 用户不存在 |
| 1002 | 密码错误 |
| 1003 | 用户已存在 |
| 1004 | 数据库错误 |
| 1005 | 人脸识别失败 |
| 1006 | 房间不存在 |
| 1007 | 设备不存在 |

## 客户端示例

### Python TCP 客户端示例
```python
import socket
import json
import struct

def send_tcp_request(action_data):
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect(('192.168.101.25', 8888))
    
    # 构建协议头
    json_data = json.dumps(action_data).encode('utf-8')
    header = struct.pack('4sII4s', b'SHBT', 1, len(json_data), b'\x00' * 4)
    
    # 发送数据
    sock.send(header + json_data)
    
    # 接收响应
    response = sock.recv(4096)
    sock.close()
    
    return response

# 使用示例
result = send_tcp_request({
    "action": "register",
    "username": "test_user",
    "password": "test_password"
})
```

### cURL HTTP 请求示例
```bash
# 用户注册
curl -X POST http://192.168.101.25:8080/register \
  -H "Content-Type: application/json" \
  -d '{"username": "test_user", "password": "test_password"}'

# 房间列表
curl -X GET http://192.168.101.25:8080/list_rooms
```

## 注意事项

1. **字符编码**: 所有文本数据使用 UTF-8 编码
2. **线程安全**: 所有 API 接口都是线程安全的
3. **连接管理**: TCP 连接支持长连接和短连接
4. **数据验证**: 所有输入数据都会进行格式和长度验证
5. **人脸识别**: 需要正确配置 SeetaFace2 模型文件路径

## 测试工具推荐

- **TCP 测试**: telnet, netcat, 自定义客户端
- **HTTP 测试**: curl, Postman, HTTPie
- **MQTT 测试**: mosquitto_pub, mosquitto_sub, MQTT Explorer
