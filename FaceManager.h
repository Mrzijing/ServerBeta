#pragma once

#include <string>
#include <vector>
#include <memory>
#include <utility>
#include <sstream>
#include <iostream>
#include <cmath>

#include "DatabaseManager.h"
#include "FaceTool.h"
#include "FaceEngine.h"

class FaceManager {
public:
    struct UpsertResult {
        bool ok = false;
        bool face_saved = false;
        std::string error;
    };
    struct QueryResult {
        bool exists = false;
        int  feature_dim = 0;
        std::string error;
    };
    struct RecognizeResult {
        bool ok = false;
        std::string username;
        float score = 0.0f;
        std::string error;
    };

    FaceManager(DatabaseManager* db, std::shared_ptr<FaceEngine> engine)
        : db_(db), engine_(std::move(engine)) {}

    void setThreshold(float t) { threshold_ = t; }
    float threshold() const { return threshold_; }
    void setEngine(std::shared_ptr<FaceEngine> e) { engine_ = std::move(e); }

    // 人脸入库（需已注册用户；提取失败不落库）
    inline UpsertResult upsertUserFace(const std::string& username, const std::string* face_base64) {
        UpsertResult r;
        if (!db_) { r.error = "数据库连接失败"; return r; }
        if (!engine_) { r.error = "人脸识别引擎未初始化"; return r; }
        if (!face_base64 || face_base64->empty()) { r.error = "缺少人脸数据"; return r; }

        // 1) base64 解码
        std::string clean = base64::strip_data_url_prefix(*face_base64);
        std::vector<unsigned char> imgBytes;
        try { imgBytes = base64::_impl::decode(clean); }
        catch (...) { r.error = "base64 解码失败"; return r; }
        if (imgBytes.empty()) { r.error = "空图像数据"; return r; }

        // 2) 提特征（失败直接返回）
        std::vector<float> feat;
        if (!engine_->extractFeature(imgBytes, feat) || feat.empty()) {
            r.error = "extract feature failed"; return r;
        }
        // 保险丝：Seeta 一般 >= 512 维。128 维视为未启用/失败，拒绝入库。防止128维图像被误判
        if (feat.size() < 200) {
            std::ostringstream os; os << "feature_dim=" << feat.size() << " suspicious (Seeta not active?)";
            r.error = os.str(); return r;
        }
        std::cerr << "[FaceUpsert] user=" << username << " feat_dim=" << feat.size() << std::endl;

        // 3) 仅允许已注册用户
        if (!db_->userExists(username)) {
            r.error = "数据库更新失败 (用户不存在)";
            return r;
        }

        // 4) 写库
        if (!db_->upsertUserFace(username, feat, &imgBytes)) {
            r.error = "数据库更新失败";
            return r;
        }

        r.ok = true;
        r.face_saved = true;
        return r;
    }

    // 删除人脸
    inline bool deleteUserFace(const std::string& username, std::string* err) {
        if (!db_) { if (err) *err = "数据库连接失败"; return false; }
        if (!db_->userExists(username)) { if (err) *err = "用户不存在"; return false; }
        bool ok = db_->deleteUserFace(username);
        if (!ok && err) *err = "数据库删除失败";
        return ok;
    }

    // 查询人脸
    inline QueryResult queryUserFace(const std::string& username) {
        QueryResult q;
        if (!db_) { q.error = "数据库连接失败"; return q; }
        if (!db_->userExists(username)) { q.exists = false; return q; }
        std::vector<float> feat;
        if (db_->getUserFaceFeature(username, feat)) {
            q.exists = true;
            q.feature_dim = static_cast<int>(feat.size());
        } else {
            q.exists = false; // 用户存在但未录入人脸
            q.feature_dim = 0;
        }
        return q;
    }

    // 识别：遍历候选，取最高分；维度不一致跳过
    inline RecognizeResult recognize(const std::string& face_base64) {
        RecognizeResult r;
        if (!db_) { r.error = "数据库连接失败"; return r; }
        if (!engine_) { r.error = "人脸识别引擎未初始化"; return r; }
        if (face_base64.empty()) { r.error = "缺少人脸数据"; return r; }

        // 解码 + 提特征
        std::string clean = base64::strip_data_url_prefix(face_base64);
        std::vector<unsigned char> imgBytes;
        try { imgBytes = base64::_impl::decode(clean); }
        catch (...) { r.error = "base64 解码失败"; return r; }
        if (imgBytes.empty()) { r.error = "空图像数据"; return r; }

        std::vector<float> qfeat;
        if (!engine_->extractFeature(imgBytes, qfeat) || qfeat.empty()) {
            r.error = "extract feature failed"; return r;
        }
        if (qfeat.size() < 200) {
            std::ostringstream os; os << "feature_dim=" << qfeat.size() << " suspicious (Seeta not active?)";
            r.error = os.str(); return r;
        }
        std::cerr << "[FaceRecognize] query_dim=" << qfeat.size() << std::endl;

        // 候选
        std::vector<std::pair<std::string, std::vector<float>>> items;
        if (!db_->getAllFaceFeatures(items) || items.empty()) {
            r.error = "数据库查询失败"; return r;
        }

        float best = -2.0f;
        std::string best_name;
        for (auto& kv : items) {
            if (kv.second.size() != qfeat.size()) continue;
            float s = FaceEngine::cosine(qfeat, kv.second);
            std::cerr << "[FaceRecognize] cand=" << kv.first
                      << " score=" << s << std::endl;
            if (s > best) { best = s; best_name = kv.first; }
        }
        if (best < -1.0f) { r.error = "没有具有相同特征维度的候选人"; return r; }

        r.ok = true;
        r.score = best;
        r.username = best_name;
        return r;
    }

private:
    DatabaseManager* db_ = nullptr;
    std::shared_ptr<FaceEngine> engine_;
    float threshold_ = 0.6f;
};