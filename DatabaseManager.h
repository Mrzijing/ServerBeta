#ifndef DATABASE_MANAGER_H
#define DATABASE_MANAGER_H

#include <iostream>
#include <string>
#include <sqlite3.h>
#include <jsoncpp/json/json.h>
#include <iomanip>
#include <mutex>
#include <vector>
#include <utility>
#include <cstring>

class DatabaseManager {
public:
    explicit DatabaseManager(const std::string& path) : dbPath(path) {}

    // 初始化数据库
    inline bool initialize();

    // 用户管理
    inline bool registerUser(const std::string& username, const std::string& password, std::string& responseMsg);
    inline bool verifyUser(const std::string& username, const std::string& password, std::string& responseMsg);
    inline bool updatePassword(const std::string& username, const std::string& newPassword, std::string& responseMsg);
    inline void printAllUsers();

    // 房间管理
    inline bool addRoom(const std::string& room_type, const std::string& room_name, std::string& responseMsg);
    inline bool deleteRoom(int room_id, std::string& responseMsg);
    inline bool listRooms(Json::Value& rooms, std::string& responseMsg);
    inline bool updateRoom(int room_id, const std::string& room_type, const std::string& room_name, std::string& responseMsg);    
    inline bool roomExists(int room_id);
    inline bool printAllRooms();

    // 设备管理
    inline bool addDevice(const std::string& device_name, const std::string& device_type, int room_id, const std::string& image, std::string& responseMsg);
    inline bool deleteDevice(int device_id, std::string& responseMsg);
    inline bool listDevices(Json::Value& devices, std::string& responseMsg);
    inline bool listDevicesByRoom(int room_id, Json::Value& devices, std::string& responseMsg);
    inline bool updateDevice(int device_id, const std::string& device_name, const std::string& device_type, int room_id, const std::string& image, std::string& responseMsg);
    inline bool controlDevice(int device_id, const std::string& action, std::string& responseMsg);
    inline void printAllDevices();

    /* 人脸识别（users 单表：以 username 为主键，附加人脸列）*/
    inline bool upsertUserFace(const std::string& username, const std::vector<float>& feature,
                               const std::vector<unsigned char>* imageBytes = nullptr);
    inline bool deleteUserFace(const std::string& username);
    inline bool getUserFaceFeature(const std::string& username, std::vector<float>& feature);
    inline bool getAllFaceFeatures(std::vector<std::pair<std::string, std::vector<float>>>& items);
    // 用户是否存在（供 HTTP/MQTT 统一校验）
    inline bool userExists(const std::string &username);

private:
    std::string dbPath;
    std::mutex db_mutex;

    inline static bool isValidDeviceType(const std::string& type) {
        return type == "风扇" || type == "灯" || type == "空调";
    }
};

/* ======== 辅助工具（同头内定义） ======== */
inline bool _has_column(sqlite3* db, const char* table, const char* col) {
    sqlite3_stmt* stmt = nullptr;
    std::string sql = std::string("PRAGMA table_info(") + table + ");";
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return false;
    bool found = false;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char* name = sqlite3_column_text(stmt, 1);
        if (name && col && std::string(reinterpret_cast<const char*>(name)) == col) {
            found = true; break;
        }
    }
    sqlite3_finalize(stmt);
    return found;
}

inline bool _ensure_users_face_columns(sqlite3* db) {
    char* errMsg = nullptr;
    // face_feature BLOB
    if (!_has_column(db, "users", "face_feature")) {
        if (sqlite3_exec(db, "ALTER TABLE users ADD COLUMN face_feature BLOB;", nullptr, nullptr, &errMsg) != SQLITE_OK) {
            if (errMsg) { std::cerr << "添加列 face_feature 失败: " << errMsg << std::endl; sqlite3_free(errMsg); }
            return false;
        }
    }
    // face_image BLOB
    if (!_has_column(db, "users", "face_image")) {
        if (sqlite3_exec(db, "ALTER TABLE users ADD COLUMN face_image BLOB;", nullptr, nullptr, &errMsg) != SQLITE_OK) {
            if (errMsg) { std::cerr << "添加列 face_image 失败: " << errMsg << std::endl; sqlite3_free(errMsg); }
            return false;
        }
    }
    // face_updated_at TEXT
    if (!_has_column(db, "users", "face_updated_at")) {
        if (sqlite3_exec(db, "ALTER TABLE users ADD COLUMN face_updated_at TEXT;", nullptr, nullptr, &errMsg) != SQLITE_OK) {
            if (errMsg) { std::cerr << "添加列 face_updated_at 失败: " << errMsg << std::endl; sqlite3_free(errMsg); }
            return false;
        }
    }
    return true;
}

/* ======== 成员函数实现（全部 inline） ======== */
inline bool DatabaseManager::initialize() {
    sqlite3* db = nullptr;
    if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) {
        std::cerr << "打开数据库失败: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }

    char* errMsg = nullptr;
    // users
    const char* createUserTableSQL =
        "CREATE TABLE IF NOT EXISTS users ("
        "username TEXT PRIMARY KEY,"
        "password TEXT NOT NULL"
        ");";
    if (sqlite3_exec(db, createUserTableSQL, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        std::cerr << "创建用户表失败: " << errMsg << std::endl;
        sqlite3_free(errMsg);
        sqlite3_close(db);
        return false;
    }
    // devices
    const char* createDeviceTableSQL =
        "CREATE TABLE IF NOT EXISTS devices ("
        "device_id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "device_name TEXT NOT NULL,"
        "device_type TEXT NOT NULL,"
        "room_id INTEGER,"
        "status TEXT DEFAULT 'off',"
        "image TEXT"
        ");";
    if (sqlite3_exec(db, createDeviceTableSQL, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        std::cerr << "创建设备表失败: " << errMsg << std::endl;
        sqlite3_free(errMsg);
        sqlite3_close(db);
        return false;
    }
    // rooms
    const char* createRoomTableSQL =
        "CREATE TABLE IF NOT EXISTS rooms ("
        "room_id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "room_type TEXT NOT NULL,"
        "room_name TEXT NOT NULL"
        ");";
    if (sqlite3_exec(db, createRoomTableSQL, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        std::cerr << "创建房间表失败: " << errMsg << std::endl;
        sqlite3_free(errMsg);
        sqlite3_close(db);
        return false;
    }

    // 迁移：users 表增加人脸列
    if (!_ensure_users_face_columns(db)) {
        sqlite3_close(db);
        return false;
    }

    sqlite3_close(db);
    std::cout << "数据库初始化成功" << std::endl;
    return true;
}

inline bool DatabaseManager::registerUser(const std::string& username, const std::string& password, std::string& responseMsg) {
    if (username.empty() || password.empty()) {
        responseMsg = "用户名或密码不能为空";
        return false;
    }
    sqlite3* db = nullptr;
    if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) {
        responseMsg = "打开数据库失败: " + std::string(sqlite3_errmsg(db));
        return false;
    }
    std::lock_guard<std::mutex> lock(db_mutex);
    const char* insertSQL = "INSERT INTO users (username, password) VALUES (?, ?);";
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db, insertSQL, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        responseMsg = std::string("数据库准备失败: ") + sqlite3_errmsg(db);
        sqlite3_close(db);
        return false;
    }
    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, password.c_str(), -1, SQLITE_TRANSIENT);
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        if (rc == SQLITE_CONSTRAINT) {
            responseMsg = "用户名已存在";
        } else {
            responseMsg = std::string("数据库插入失败: ") + sqlite3_errmsg(db);
        }
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return false;
    }
    sqlite3_finalize(stmt);
    responseMsg = "注册成功";
    sqlite3_close(db);
    return true;
}

inline bool DatabaseManager::verifyUser(const std::string& username, const std::string& password, std::string& responseMsg) {
    if (username.empty() || password.empty()) {
        responseMsg = "用户名或密码为空";
        return false;
    }
    sqlite3* db = nullptr;
    if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) {
        responseMsg = "打开数据库失败: " + std::string(sqlite3_errmsg(db));
        return false;
    }
    std::lock_guard<std::mutex> lock(db_mutex);
    const char* selectSQL = "SELECT password FROM users WHERE username = ?;";
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db, selectSQL, -1, &stmt, nullptr);
    if (rc == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
        rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW) {
            const char* storedPassword = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            bool match = (storedPassword && password == storedPassword);
            sqlite3_finalize(stmt);
            sqlite3_close(db);
            if (match) {
                responseMsg = "登录成功";
                return true;
            } else {
                responseMsg = "密码错误";
                return false;
            }
        } else {
            sqlite3_finalize(stmt);
            sqlite3_close(db);
            responseMsg = "用户不存在";
            return false;
        }
    } else {
        responseMsg = "服务器内部错误";
        sqlite3_close(db);
        return false;
    }
}

inline bool DatabaseManager::updatePassword(const std::string& username, const std::string& newPassword, std::string& responseMsg) {
    if (username.empty() || newPassword.empty()) {
        responseMsg = "用户名或新密码为空";
        return false;
    }
    sqlite3* db = nullptr;
    if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) {
        responseMsg = "打开数据库失败: " + std::string(sqlite3_errmsg(db));
        return false;
    }
    std::lock_guard<std::mutex> lock(db_mutex);
    const char* updateSQL = "UPDATE users SET password = ? WHERE username = ?;";
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db, updateSQL, -1, &stmt, nullptr);
    if (rc == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, newPassword.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, username.c_str(), -1, SQLITE_TRANSIENT);
        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        if (rc == SQLITE_DONE) {
            responseMsg = "密码修改成功";
            return true;
        } else {
            responseMsg = "密码修改失败: " + std::string(sqlite3_errmsg(db));
            return false;
        }
    } else {
        responseMsg = "准备密码修改语句失败: " + std::string(sqlite3_errmsg(db));
        sqlite3_close(db);
        return false;
    }
}

/* 添加房间 */
inline bool DatabaseManager::addRoom(const std::string& room_type, const std::string& room_name, std::string& responseMsg) {
    if (room_type.empty() || room_name.empty()) {
        responseMsg = "房间类型或名称不能为空";
        return false;
    }
    sqlite3* db = nullptr;
    if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) {
        responseMsg = "打开数据库失败: " + std::string(sqlite3_errmsg(db));
        return false;
    }
    std::lock_guard<std::mutex> lock(db_mutex);
    const char* insertSQL = "INSERT INTO rooms (room_type, room_name) VALUES (?, ?);";
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db, insertSQL, -1, &stmt, nullptr);
    if (rc == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, room_type.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, room_name.c_str(), -1, SQLITE_TRANSIENT);
        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        if (rc == SQLITE_DONE) {
            responseMsg = "房间添加成功";
            return true;
        } else {
            responseMsg = "添加房间失败: " + std::string(sqlite3_errmsg(db));
            return false;
        }
    } else {
        responseMsg = "准备添加房间语句失败: " + std::string(sqlite3_errmsg(db));
        sqlite3_close(db);
        return false;
    }
}

inline bool DatabaseManager::deleteRoom(int room_id, std::string& responseMsg) {
    if (room_id <= 0) {
        responseMsg = "无效的房间ID";
        return false;
    }
    sqlite3* db = nullptr;
    if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) {
        responseMsg = "打开数据库失败: " + std::string(sqlite3_errmsg(db));
        return false;
    }
    std::lock_guard<std::mutex> lock(db_mutex);

    // 先删除设备
    const char* delDevSQL = "DELETE FROM devices WHERE room_id = ?;";
    sqlite3_stmt* stmt1;
    int rc = sqlite3_prepare_v2(db, delDevSQL, -1, &stmt1, nullptr);
    if (rc == SQLITE_OK) {
        sqlite3_bind_int(stmt1, 1, room_id);
        sqlite3_step(stmt1);
        sqlite3_finalize(stmt1);
    }

    // 再删除房间
    const char* delRoomSQL = "DELETE FROM rooms WHERE room_id = ?;";
    sqlite3_stmt* stmt2;
    rc = sqlite3_prepare_v2(db, delRoomSQL, -1, &stmt2, nullptr);
    if (rc == SQLITE_OK) {
        sqlite3_bind_int(stmt2, 1, room_id);
        rc = sqlite3_step(stmt2);
        sqlite3_finalize(stmt2);
        sqlite3_close(db);
        if (rc == SQLITE_DONE) {
            responseMsg = "房间及其设备删除成功";
            return true;
        } else {
            responseMsg = "删除房间失败: " + std::string(sqlite3_errmsg(db));
            return false;
        }
    } else {
        responseMsg = "准备删除房间语句失败: " + std::string(sqlite3_errmsg(db));
        sqlite3_close(db);
        return false;
    }
}

inline bool DatabaseManager::listRooms(Json::Value& rooms, std::string& responseMsg) {
    sqlite3* db = nullptr;
    if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) {
        responseMsg = "打开数据库失败: " + std::string(sqlite3_errmsg(db));
        return false;
    }
    std::lock_guard<std::mutex> lock(db_mutex);
    const char* selectSQL = "SELECT room_id, room_type, room_name FROM rooms;";
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db, selectSQL, -1, &stmt, nullptr);
    if (rc == SQLITE_OK) {
        Json::Value roomList(Json::arrayValue);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            Json::Value room(Json::objectValue);
            room["room_id"]   = sqlite3_column_int(stmt, 0);
            room["room_type"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            room["room_name"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            roomList.append(room);
        }
        rooms = roomList;
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return true;
    } else {
        responseMsg = "查询房间失败: " + std::string(sqlite3_errmsg(db));
        sqlite3_close(db);
        return false;
    }
}

inline bool DatabaseManager::updateRoom(int room_id, const std::string& room_type, const std::string& room_name, std::string& responseMsg) {
    if (room_id <= 0 || room_type.empty() || room_name.empty()) {
        responseMsg = "无效的房间ID、类型或名称";
        return false;
    }
    sqlite3* db = nullptr;
    if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) {
        responseMsg = "打开数据库失败: " + std::string(sqlite3_errmsg(db));
        return false;
    }
    std::lock_guard<std::mutex> lock(db_mutex);
    const char* updateSQL = "UPDATE rooms SET room_type = ?, room_name = ? WHERE room_id = ?;";
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db, updateSQL, -1, &stmt, nullptr);
    if (rc == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, room_type.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, room_name.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 3, room_id);
        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        if (rc == SQLITE_DONE) {
            responseMsg = "房间信息修改成功";
            return true;
        } else {
            responseMsg = "修改房间信息失败: " + std::string(sqlite3_errmsg(db));
            return false;
        }
    } else {
        responseMsg = "准备修改房间信息语句失败: " + std::string(sqlite3_errmsg(db));
        sqlite3_close(db);
        return false;
    }
}

inline bool DatabaseManager::roomExists(int room_id) {
    sqlite3* db = nullptr;
    if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) return false;
    std::lock_guard<std::mutex> lock(db_mutex);
    const char* sql = "SELECT 1 FROM rooms WHERE room_id=? LIMIT 1;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) { sqlite3_close(db); return false; }
    sqlite3_bind_int(stmt, 1, room_id);
    int rc = sqlite3_step(stmt);
    bool exists = (rc == SQLITE_ROW);
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return exists;
}

// 添加设备
inline bool DatabaseManager::addDevice(const std::string& device_name, const std::string& device_type, int room_id, const std::string& image, std::string& responseMsg) {
    if (device_name.empty() || device_type.empty() || room_id <= 0) {
        responseMsg = "设备名称、设备类型或房间ID无效";
        return false;
    }
    if (!isValidDeviceType(device_type)) {
        responseMsg = "设备类型不合法";
        return false;
    }
    sqlite3* db = nullptr;
    if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) {
        responseMsg = "打开数据库失败: " + std::string(sqlite3_errmsg(db));
        return false;
    }
    std::lock_guard<std::mutex> lock(db_mutex);
    const char* insertSQL = "INSERT INTO devices (device_name, device_type, room_id, image) VALUES (?, ?, ?, ?);";
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db, insertSQL, -1, &stmt, nullptr);
    if (rc == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, device_name.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, device_type.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 3, room_id);
        sqlite3_bind_text(stmt, 4, image.c_str(), -1, SQLITE_TRANSIENT);
        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        if (rc == SQLITE_DONE) {
            responseMsg = "设备添加成功";
            return true;
        } else {
            responseMsg = "添加设备失败: " + std::string(sqlite3_errmsg(db));
            return false;
        }
    } else {
        responseMsg = "准备添加设备语句失败: " + std::string(sqlite3_errmsg(db));
        sqlite3_close(db);
        return false;
    }
}

// 删除设备
inline bool DatabaseManager::deleteDevice(int device_id, std::string& responseMsg) {
    if (device_id <= 0) {
        responseMsg = "无效的设备ID";
        return false;
    }
    sqlite3* db = nullptr;
    if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) {
        responseMsg = "打开数据库失败: " + std::string(sqlite3_errmsg(db));
        return false;
    }
    std::lock_guard<std::mutex> lock(db_mutex);

    // 检查设备是否存在
    const char* checkSQL = "SELECT 1 FROM devices WHERE device_id = ?;";
    sqlite3_stmt* checkStmt;
    int rc = sqlite3_prepare_v2(db, checkSQL, -1, &checkStmt, nullptr);
    if (rc == SQLITE_OK) {
        sqlite3_bind_int(checkStmt, 1, device_id);
        rc = sqlite3_step(checkStmt);
        sqlite3_finalize(checkStmt);
        if (rc != SQLITE_ROW) {
            sqlite3_close(db);
            responseMsg = "设备ID不存在";
            return false;
        }
    } else {
        sqlite3_close(db);
        responseMsg = "设备校验失败: " + std::string(sqlite3_errmsg(db));
        return false;
    }

    // 删除设备
    const char* deleteSQL = "DELETE FROM devices WHERE device_id = ?;";
    sqlite3_stmt* stmt;
    rc = sqlite3_prepare_v2(db, deleteSQL, -1, &stmt, nullptr);
    if (rc == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, device_id);
        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        if (rc == SQLITE_DONE) {
            responseMsg = "设备删除成功";
            return true;
        } else {
            responseMsg = "删除设备失败: " + std::string(sqlite3_errmsg(db));
            return false;
        }
    } else {
        responseMsg = "准备删除设备语句失败: " + std::string(sqlite3_errmsg(db));
        sqlite3_close(db);
        return false;
    }
}

// 列出所有设备
inline bool DatabaseManager::listDevices(Json::Value& devices, std::string& responseMsg) {
    sqlite3* db = nullptr;
    if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) {
        responseMsg = "打开数据库失败: " + std::string(sqlite3_errmsg(db));
        return false;
    }
    std::lock_guard<std::mutex> lock(db_mutex);
    const char* selectSQL = "SELECT device_id, device_name, device_type, room_id, image FROM devices;";
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db, selectSQL, -1, &stmt, nullptr);
    if (rc == SQLITE_OK) {
        Json::Value deviceList(Json::arrayValue);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            Json::Value device(Json::objectValue);
            device["device_id"]  = sqlite3_column_int(stmt, 0);
            device["device_name"]= reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            device["device_type"]= reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            device["image"]      = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
            device["room_id"]    = sqlite3_column_int(stmt, 3);
            deviceList.append(device);
        }
        devices = deviceList;
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return true;
    } else {
        responseMsg = "查询设备失败: " + std::string(sqlite3_errmsg(db));
        sqlite3_close(db);
        return false;
    }
}

// 按房间ID列出设备
inline bool DatabaseManager::listDevicesByRoom(int room_id, Json::Value& devices, std::string& responseMsg) {
    sqlite3* db = nullptr;
    if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) {
        responseMsg = "打开数据库失败: " + std::string(sqlite3_errmsg(db));
        return false;
    }
    std::lock_guard<std::mutex> lock(db_mutex);
    const char* selectSQL = "SELECT device_id, device_name, device_type, room_id, image FROM devices WHERE room_id = ?;";
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db, selectSQL, -1, &stmt, nullptr);
    if (rc == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, room_id);
        Json::Value deviceList(Json::arrayValue);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            Json::Value device(Json::objectValue);
            device["device_id"]  = sqlite3_column_int(stmt, 0);
            device["device_name"]= reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            device["device_type"]= reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            device["image"]      = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
            device["room_id"]    = sqlite3_column_int(stmt, 3);
            deviceList.append(device);
        }
        devices = deviceList;
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return true;
    } else {
        responseMsg = "查询设备失败: " + std::string(sqlite3_errmsg(db));
        sqlite3_close(db);
        return false;
    }
}

// 更新设备信息
inline bool DatabaseManager::updateDevice(int device_id, const std::string& device_name, const std::string& device_type, int room_id, const std::string& image, std::string& responseMsg) {
    if (device_id <= 0 || device_name.empty() || device_type.empty() || room_id <= 0) {
        responseMsg = "无效的设备ID、设备名称、设备类型或房间ID";
        return false;
    }
    if (!isValidDeviceType(device_type)) {
        responseMsg = "设备类型不合法";
        return false;
    }
    sqlite3* db = nullptr;
    if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) {
        responseMsg = "打开数据库失败: " + std::string(sqlite3_errmsg(db));
        return false;
    }
    std::lock_guard<std::mutex> lock(db_mutex);
    const char* updateSQL = "UPDATE devices SET device_name = ?, device_type = ?, room_id = ?, image = ? WHERE device_id = ?;";
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db, updateSQL, -1, &stmt, nullptr);
    if (rc == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, device_name.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, device_type.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 3, room_id);
        sqlite3_bind_text(stmt, 4, image.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 5, device_id);
        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        if (rc == SQLITE_DONE) {
            responseMsg = "设备信息修改成功";
            return true;
        } else {
            responseMsg = "修改设备信息失败: " + std::string(sqlite3_errmsg(db));
            return false;
        }
    } else {
        responseMsg = "准备修改设备信息语句失败: " + std::string(sqlite3_errmsg(db));
        sqlite3_close(db);
        return false;
    }
}

// 控制设备，MQTT协议新增模块，只负责控制开关
inline bool DatabaseManager::controlDevice(int device_id, const std::string& action, std::string& responseMsg) {
    if (device_id <= 0 || action.empty()) {
        responseMsg = "无效的设备ID或操作";
        return false;
    }
    // 这里可以接入实际设备控制；当前仅返回成功
    std::lock_guard<std::mutex> lock(db_mutex);
    responseMsg = "设备控制成功: " + action + " 操作已执行";
    return true;
}

//打印所有用户，用于调试
inline void DatabaseManager::printAllUsers() {
    sqlite3* db = nullptr;
    if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) {
        std::cerr << "打开数据库失败: " << sqlite3_errmsg(db) << std::endl;
        return;
    }
    std::lock_guard<std::mutex> lock(db_mutex);
    const char* selectSQL = "SELECT username FROM users;";
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db, selectSQL, -1, &stmt, nullptr);
    if (rc == SQLITE_OK) {
        std::cout << "\n======= 数据库中的所有用户 ========" << std::endl;
        std::cout << std::setw(23) << "用户名" << std::endl;
        std::cout << std::string(35, '-') << std::endl;
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const char* username = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            std::cout << std::setw(20) << (username ? username : "NULL") << std::endl;
        }
        std::cout << std::string(35, '-') << std::endl;
        sqlite3_finalize(stmt);
    } else {
        std::cerr << "查询用户失败: " << sqlite3_errmsg(db) << std::endl;
    }
    sqlite3_close(db);
}

// 打印所有房间，用于调试
inline bool DatabaseManager::printAllRooms() {
    sqlite3* db = nullptr;
    if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) return false;
    std::lock_guard<std::mutex> lock(db_mutex);
    const char* sql = "SELECT room_id, room_type, room_name FROM rooms;";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        sqlite3_close(db);
        return false;
    }
    std::cout << "\n======= 数据库中的所有房间 ========" << std::endl;
    std::cout << std::setw(10) << "ID"
              << std::setw(15) << "房间类型"
              << std::setw(20) << "房间名称" << std::endl;
    std::cout << std::string(45, '-') << std::endl;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int room_id = sqlite3_column_int(stmt, 0);
        const char* room_type = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        const char* room_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        std::cout << std::setw(10) << room_id
                  << std::setw(15) << (room_type ? room_type : "NULL")
                  << std::setw(20) << (room_name ? room_name : "NULL") << std::endl;
    }
    std::cout << std::string(45, '-') << std::endl;
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return true;
}

/// 打印所有设备，用于调试
inline void DatabaseManager::printAllDevices() {
    sqlite3* db = nullptr;
    if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) {
        std::cerr << "打开数据库失败: " << sqlite3_errmsg(db) << std::endl;
        return;
    }
    std::lock_guard<std::mutex> lock(db_mutex);
    const char* selectSQL = "SELECT device_id, device_name, device_type, room_id FROM devices;";
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db, selectSQL, -1, &stmt, nullptr);
    if (rc == SQLITE_OK) {
        std::cout << "\n======= 数据库中的所有设备 ========" << std::endl;
        std::cout << std::setw(10) << "ID"
                  << std::setw(20) << "设备名称"
                  << std::setw(15) << "设备类型"
                  << std::setw(10) << "房间ID" << std::endl;
        std::cout << std::string(60, '-') << std::endl;
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            int device_id = sqlite3_column_int(stmt, 0);
            const char* device_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            const char* device_type = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            int room_id = sqlite3_column_int(stmt, 3);
            std::cout << std::setw(10) << device_id
                      << std::setw(20) << (device_name ? device_name : "NULL")
                      << std::setw(15) << (device_type ? device_type : "NULL")
                      << std::setw(10) << room_id << std::endl;
        }
        std::cout << std::string(60, '-') << std::endl;
        sqlite3_finalize(stmt);
    } else {
        std::cerr << "查询设备失败: " << sqlite3_errmsg(db) << std::endl;
    }
    sqlite3_close(db);
}

/* 人脸：写入或更新（users 单表；当用户不存在时返回 false） */
inline bool DatabaseManager::upsertUserFace(const std::string& username, const std::vector<float>& feature,
                                            const std::vector<unsigned char>* imageBytes) {
    sqlite3* db = nullptr;
    if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) return false;
    std::lock_guard<std::mutex> lock(db_mutex);
    if (!_ensure_users_face_columns(db)) { 
        sqlite3_close(db); 
        return false; 
    }

    const char* sql = "UPDATE users SET face_feature=?, face_image=?, face_updated_at=datetime('now') WHERE username=?;";
    sqlite3_stmt* stmt = nullptr;
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) { 
        sqlite3_close(db); 
        return false; 
    }

    sqlite3_bind_blob(stmt, 1, feature.data(), (int)(feature.size() * sizeof(float)), SQLITE_STATIC);

    if (imageBytes && !imageBytes->empty()) {
        sqlite3_bind_blob(stmt, 2, imageBytes->data(), (int)imageBytes->size(), SQLITE_STATIC);
    }
    else {
        sqlite3_bind_null(stmt, 2);
    }
    
    sqlite3_bind_text(stmt, 3, username.c_str(), -1, SQLITE_TRANSIENT);

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE) && (sqlite3_changes(db) > 0);
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return ok; // 若用户不存在将返回 false
}

/* 人脸：删除（置空列） */
inline bool DatabaseManager::deleteUserFace(const std::string& username) {
    sqlite3* db = nullptr;
    if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) return false;
    std::lock_guard<std::mutex> lock(db_mutex);
    if (!_ensure_users_face_columns(db)) { sqlite3_close(db); return false; }

    const char* sql = "UPDATE users SET face_feature=NULL, face_image=NULL, face_updated_at=datetime('now') WHERE username=?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) { sqlite3_close(db); return false; }
    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE) && (sqlite3_changes(db) > 0);
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return ok;
}

/* 人脸：查询单个用户特征 */
inline bool DatabaseManager::getUserFaceFeature(const std::string& username, std::vector<float>& feature) {
    feature.clear();
    sqlite3* db = nullptr;
    if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) return false;
    std::lock_guard<std::mutex> lock(db_mutex);
    if (!_ensure_users_face_columns(db)) { sqlite3_close(db); return false; }

    const char* sql = "SELECT face_feature FROM users WHERE username=?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) { sqlite3_close(db); return false; }
    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);

    bool ok = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const void* blob = sqlite3_column_blob(stmt, 0);
        int bytes = sqlite3_column_bytes(stmt, 0);
        if (blob && bytes > 0 && bytes % (int)sizeof(float) == 0) {
            int n = bytes / (int)sizeof(float);
            feature.resize(n);
            std::memcpy(feature.data(), blob, bytes);
            ok = true;
        }
    }
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return ok;
}

/* 人脸：遍历所有（仅含有特征的用户） */
inline bool DatabaseManager::getAllFaceFeatures(std::vector<std::pair<std::string, std::vector<float>>>& items) {
    items.clear();
    sqlite3* db = nullptr;
    if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) return false;
    std::lock_guard<std::mutex> lock(db_mutex);
    if (!_ensure_users_face_columns(db)) { sqlite3_close(db); return false; }

    const char* sql = "SELECT username, face_feature FROM users WHERE face_feature IS NOT NULL;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) { sqlite3_close(db); return false; }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* uname = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        const void* blob = sqlite3_column_blob(stmt, 1);
        int bytes = sqlite3_column_bytes(stmt, 1);
        if (uname && blob && bytes > 0 && bytes % (int)sizeof(float) == 0) {
            int n = bytes / (int)sizeof(float);
            std::vector<float> feat(n);
            std::memcpy(feat.data(), blob, bytes);
            items.emplace_back(std::string(uname), std::move(feat));
        }
    }
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return true;
}

/* 用户是否存在 */
inline bool DatabaseManager::userExists(const std::string &username) {
    sqlite3* db = nullptr;
    if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) return false;
    std::lock_guard<std::mutex> lock(db_mutex);
    static const char *sql = "SELECT 1 FROM users WHERE username=? LIMIT 1;";
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) { sqlite3_close(db); return false; }
    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(stmt);
    bool exists = (rc == SQLITE_ROW);
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return exists;
}

#endif // DATABASE_MANAGER_H

