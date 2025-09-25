# SmartHomeBetaTest - Smart Home Server

[中文版](README.md) | English

A comprehensive smart home server application with user management, device control, face recognition, and multi-protocol communication capabilities.

## Overview

SmartHomeBetaTest is a high-performance smart home backend server developed in C++, supporting multiple communication protocols (TCP, HTTP, MQTT) with face recognition capabilities for managing users, rooms, and smart devices.

## Key Features

### 🔐 User Management System
- User registration and login
- Password modification and verification  
- Face feature storage and recognition
- SQLite database-based user data persistence

### 🏠 Room & Device Management
- Room creation, deletion, update, and query
- Smart device addition, removal, update, and listing
- Device status management and control

### 👤 Face Recognition System
- High-precision face recognition based on SeetaFace2 engine
- Face feature extraction and storage
- Real-time face recognition verification
- Base64 image data processing support

### 🌐 Multi-Protocol Communication
- **TCP Server**: Port 8888, custom protocol support
- **HTTP Server**: Port 8080, RESTful API interfaces
- **MQTT Server**: Port 1883, IoT device communication

### ⚡ High-Performance Architecture
- Multi-threaded concurrent processing
- Thread pool management
- Epoll event-driven I/O
- Asynchronous message processing

## Technical Stack

- **C++14**: Modern C++ standard
- **SQLite3**: Lightweight database
- **JsonCpp**: JSON data processing
- **SeetaFace2**: Face recognition engine
- **OpenCV**: Image processing (optional)
- **libevent**: HTTP server support
- **libmosquitto**: MQTT protocol support

## Quick Start

### Dependencies (Ubuntu/Debian)
```bash
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

### Build & Run
```bash
# Clone the project
git clone https://github.com/Mrzijing/ServerBeta.git
cd ServerBeta

# Create build directory
mkdir -p build && cd build

# Configure and compile
cmake ..
make -j$(nproc)

# Run server
./Server
```

## API Endpoints

### TCP Protocol (Port 8888)
- `register`, `login`, `change_password`
- `add_room`, `delete_room`, `list_rooms`, `update_room`
- `add_device`, `delete_device`, `list_devices`, `update_device`

### HTTP RESTful API (Port 8080)
- `POST /register`, `POST /login`, `POST /change_password`
- `POST /add_room`, `DELETE /delete_room`, `GET /list_rooms`, `POST /update_room`
- `POST /add_device`, `DELETE /delete_device`, `GET /list_devices`, `POST /update_device`

### MQTT Topics (Port 1883)
- Publish/Subscribe pattern support
- Automatic message routing
- Integrated business logic processing

## Configuration

- **Server IP**: Modify binding address in main.cpp (default: 192.168.101.25)
- **Ports**: TCP:8888, HTTP:8080, MQTT:1883
- **Database**: `./server.db` (auto-created)
- **Face Models**: Configure correct SeetaFace2 model path

## License

This project is open source. Please see the LICENSE file in the project root directory for license information.

---

*Note: This project is for learning and research purposes only. Please comply with relevant laws and regulations.*