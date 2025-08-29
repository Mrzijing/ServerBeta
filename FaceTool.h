#pragma once
#include <string>
#include <vector>
#include <cctype>
#include <stdexcept>

namespace base64 {
    inline std::string strip_data_url_prefix(const std::string &s) {
        const std::string key = "base64,";
        auto pos = s.find(key);
        if (pos == std::string::npos) return s;
        return s.substr(pos + key.size());
    }

    namespace _impl {
        inline std::string remove_spaces(const std::string &in) {
            std::string out; out.reserve(in.size());
            for (unsigned char c : in) { if (!std::isspace(c)) out.push_back((char)c); }
            return out;
        }
        inline void urlsafe_to_std(std::string &s) {
            for (auto &c : s) { if (c == '-') c = '+'; else if (c == '_') c = '/'; }
        }
        inline void add_padding(std::string &s) {
            size_t m = s.size() % 4;
            if (m) s.append(4 - m, '=');
        }
        inline const signed char* decode_table() {
            static signed char T[256];
            static bool inited = false;
            if (!inited) {
                std::fill(std::begin(T), std::end(T), (signed char)-1);
                const std::string alphabet =
                    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
                for (int i = 0; i < (int)alphabet.size(); ++i)
                    T[(unsigned char)alphabet[i]] = (signed char)i;
                T[(unsigned char)'='] = 0;
                inited = true;
            }
            return T;
        }
        inline std::vector<unsigned char> decode(const std::string &input_raw) {
            // 1) 预处理：去前缀、去空白、URL-safe 转换、补齐 padding
            std::string s = remove_spaces(input_raw);
            urlsafe_to_std(s);
            add_padding(s);
            if (s.empty()) return {};

            const signed char *T = decode_table();
            size_t len = s.size();
            if (len % 4 != 0) throw std::runtime_error("invalid base64 length");

            size_t out_len = (len / 4) * 3;
            if (len >= 4) {
                if (s[len-1] == '=') --out_len;
                if (s[len-2] == '=') --out_len;
            }
            std::vector<unsigned char> out; out.resize(out_len);

            size_t oi = 0;
            for (size_t i = 0; i < len; i += 4) {
                int c0 = T[(unsigned char)s[i]];
                int c1 = T[(unsigned char)s[i+1]];
                int c2 = T[(unsigned char)s[i+2]];
                int c3 = T[(unsigned char)s[i+3]];
                if (c0 < 0 || c1 < 0 || c2 < -1 || c3 < -1)
                    throw std::runtime_error("invalid base64 char");

                unsigned int trip =
                    ((unsigned int)c0 << 18) |
                    ((unsigned int)c1 << 12) |
                    (((unsigned int)(s[i+2] == '=' ? 0 : c2)) << 6) |
                    ((unsigned int)(s[i+3] == '=' ? 0 : c3));

                if (oi < out_len) out[oi++] = (unsigned char)((trip >> 16) & 0xFF);
                if (s[i+2] != '=' && oi < out_len) out[oi++] = (unsigned char)((trip >> 8) & 0xFF);
                if (s[i+3] != '=' && oi < out_len) out[oi++] = (unsigned char)(trip & 0xFF);
            }
            return out;
        }
    } // namespace _impl

    // 可选的对外包装，FaceManager 也可直接用 _impl::decode
    inline std::vector<unsigned char> decode(const std::string &s) {
        return _impl::decode(s);
    }
}