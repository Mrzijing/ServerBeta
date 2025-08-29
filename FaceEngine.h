#pragma once
#include <string>
#include <vector>
#include <memory>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <unistd.h>
#include <exception>

class FaceEngine {
public:
    virtual ~FaceEngine() = default;
    virtual bool init(const std::string &model_dir) = 0;
    virtual bool extractFeature(const std::vector<unsigned char> &image_bytes,
                                std::vector<float> &feature) = 0;

    static float cosine(const std::vector<float>& a, const std::vector<float>& b) {
        if (a.size()!=b.size() || a.empty()) return -2.0f;
        double dot=0, na=0, nb=0;
        for (size_t i=0;i<a.size();++i){ double x=a[i], y=b[i]; dot+=x*y; na+=x*x; nb+=y*y; }
        if (na<=0 || nb<=0) return -2.0f;
        return (float)(dot/(std::sqrt(na)*std::sqrt(nb)));
    }
};

inline bool file_exists(const std::string &p) { return ::access(p.c_str(), R_OK) == 0; }

#ifdef SEETA_ENABLED
// ===== SeetaFace2 v2 + OpenCV =====
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/core.hpp>

#include <seeta/FaceDetector.h>
#include <seeta/FaceLandmarker.h>
#include <seeta/FaceRecognizer.h>
#include <seeta/Struct.h>   // 含 seeta::ModelSetting / SeetaImageData / SeetaFaceInfoArray

static inline SeetaImageData MatToSeeta(const cv::Mat &img) {
    SeetaImageData s{}; s.width = img.cols; s.height = img.rows; s.channels = img.channels(); s.data = img.data; return s;
}

class SeetaFaceEngine : public FaceEngine {
public:
    bool init(const std::string &model_dir) override {
        std::string fd  = model_dir + "/fd_2_00.dat";
        std::string fa5 = model_dir + "/pd_2_00_pts5.dat";
        std::string fa81= model_dir + "/pd_2_00_pts81.dat";
        std::string fr  = model_dir + "/fr_2_10.dat";
        std::string fa = file_exists(fa5) ? fa5 : fa81;

        std::fprintf(stderr,
            "[FaceEngine] model_dir=%s\n"
            "  fd=%s [%s]\n"
            "  fa5=%s [%s]\n"
            "  fa81=%s [%s]\n"
            "  fr=%s [%s]\n",
            model_dir.c_str(),
            fd.c_str(),  file_exists(fd)  ? "OK":"MISS",
            fa5.c_str(), file_exists(fa5) ? "OK":"MISS",
            fa81.c_str(),file_exists(fa81)? "OK":"MISS",
            fr.c_str(),  file_exists(fr)  ? "OK":"MISS");

        if (!file_exists(fd) || (!file_exists(fa5) && !file_exists(fa81)) || !file_exists(fr)) {
            std::fprintf(stderr, "[FaceEngine] 模型缺失，见上方 OK/MISS\n");
            return false;
        }

        try {
            seeta::ModelSetting ms_fd({fd}, seeta::ModelSetting::AUTO, 0);
            seeta::ModelSetting ms_fa({fa}, seeta::ModelSetting::AUTO, 0);
            seeta::ModelSetting ms_fr({fr}, seeta::ModelSetting::AUTO, 0);

            detector_   = std::make_unique<seeta::v2::FaceDetector>(ms_fd, 640, 480);
            landmarker_ = std::make_unique<seeta::v2::FaceLandmarker>(ms_fa);
            recognizer_ = std::make_unique<seeta::v2::FaceRecognizer>(ms_fr);
            std::fprintf(stderr, "[FaceEngine] SeetaFace 对象构造成功\n");
        } catch (const std::exception &e) {
            std::fprintf(stderr, "[FaceEngine] 创建 SeetaFace 对象失败: %s\n", e.what());
            return false;
        } catch (...) {
            std::fprintf(stderr, "[FaceEngine] 创建 SeetaFace 对象失败: 未知异常\n");
            return false;
        }

        int dim = 0; 
        try { dim = recognizer_->GetExtractFeatureSize(); } catch (...) {}
        std::fprintf(stderr, "[FaceEngine] Using SeetaFace2 v2, dim=%d\n", dim);
        return detector_ && landmarker_ && recognizer_;
    }

    bool extractFeature(const std::vector<unsigned char> &image_bytes,
                        std::vector<float> &feature) override {
        feature.clear();
        if (!detector_ || !landmarker_ || !recognizer_) return false;

        cv::Mat buf(1, (int)image_bytes.size(), CV_8UC1, (void*)image_bytes.data());
        cv::Mat img = cv::imdecode(buf, cv::IMREAD_COLOR);
        if (img.empty()) { std::fprintf(stderr, "[FaceEngine] imdecode 失败（请用 JPG/PNG）\n"); return false; }
        SeetaImageData sImg = MatToSeeta(img);

        // 检测
        SeetaFaceInfoArray arr{}; 
        try { arr = detector_->detect(sImg); } catch (...) { std::fprintf(stderr, "[FaceEngine] detect 异常\n"); return false; }
        if (arr.size <= 0 || arr.data == nullptr) { std::fprintf(stderr, "[FaceEngine] 未检测到人脸\n"); return false; }

        // 最大框
        int idx = 0;
        for (int i = 1; i < arr.size; ++i) {
            auto a = arr.data[i].pos.width * arr.data[i].pos.height;
            auto b = arr.data[idx].pos.width * arr.data[idx].pos.height;
            if (a > b) idx = i;
        }

        // 关键点
        std::vector<SeetaPointF> points;
        try { points = landmarker_->mark(sImg, arr.data[idx].pos); }
        catch (...) { std::fprintf(stderr, "[FaceEngine] 关键点检测失败\n"); return false; }
        if (points.empty()) { std::fprintf(stderr, "[FaceEngine] 关键点为空\n"); return false; }

        int dim = 0; try { dim = recognizer_->GetExtractFeatureSize(); } catch (...) {}
        if (dim <= 0) dim = 512;
        feature.resize(dim);

        bool ok = false;
        try { ok = recognizer_->Extract(sImg, points.data(), feature.data()); }
        catch (...) { ok = false; }
        if (!ok) { std::fprintf(stderr, "[FaceEngine] 提特征失败\n"); feature.clear(); return false; }
        return true;
    }

private:
    std::unique_ptr<seeta::v2::FaceDetector>   detector_;
    std::unique_ptr<seeta::v2::FaceLandmarker> landmarker_;
    std::unique_ptr<seeta::v2::FaceRecognizer> recognizer_;
};
#else
class SeetaFaceEngine : public FaceEngine {
public:
    bool init(const std::string&) override { return false; }
    bool extractFeature(const std::vector<unsigned char>&, std::vector<float>&) override { return false; }
};
#endif