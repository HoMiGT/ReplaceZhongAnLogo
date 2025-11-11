//
// Created by houmin on 25-11-11.
//

#include "convertEncodingFormat.h"
#include <windows.h>
#include <vector>
#include <iostream>


// 判断字符串是否为严格的 UTF-8 编码
static bool IsUtf8Strict(const std::string &s)
{
    if (s.empty()) return true;
    const auto req = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), static_cast<int>(s.size()), nullptr, 0);
    return req != 0;
}

// 把任意字节(优先判断 UTF-8)编码的字符串转换为宽字符串
// fallbackToSystemACP: 如果 true 在 GB18030 失败时再用GetACP()
std::string ToUtf8_FromUnknown(const std::string &bytes, bool &convertState, bool fallbackToSystemACP)
{
    if (bytes.empty()) return {};

    // 1. Bom检查
    if (bytes.size() >=3 &&
        static_cast<unsigned char>(bytes[0]) == 0xEF &&
        static_cast<unsigned char>(bytes[1]) == 0xBB &&
        static_cast<unsigned char>(bytes[2]) == 0xBF)
    {
        convertState = true;
        return bytes.substr(3);
    }

    // 2. 严格 UTF-8 检查
    if (IsUtf8Strict(bytes))
    {
        convertState = true;
        return bytes;
    }

    // 3. 按照 GB18030 (windows code page 54936) 解为 utf-16
    constexpr UINT cp_gb18030 = 54936;
    auto w_len = MultiByteToWideChar(cp_gb18030, 0, bytes.data(), static_cast<int>(bytes.size()), nullptr, 0);
    if (w_len == 0)
    {
        if (fallbackToSystemACP)
        {
            const UINT acp = GetACP();
            w_len = MultiByteToWideChar(acp, 0 , bytes.data(), static_cast<int>(bytes.size()),nullptr,0);
            if (w_len == 0)
            {
                // 解码失败，返回原始bytes 已防止丢失
                convertState = false;
                return bytes;
            }
            std::wstring w;
            w.resize(w_len);
            MultiByteToWideChar(acp, 0, bytes.data(),static_cast<int>(bytes.size()),&w[0],w_len);
            // 再转为 utf-8
            const auto u8_len = WideCharToMultiByte(CP_UTF8,0,w.data(),w_len,nullptr,0,nullptr,nullptr);
            std::string out(u8_len,0);
            WideCharToMultiByte(CP_UTF8, 0, w.data(),w_len,&out[0],u8_len,nullptr,nullptr);
            convertState = true;
            return out;
        }
        convertState = false;
        return bytes;
    }
    std::wstring w;
    w.resize(w_len);
    MultiByteToWideChar(cp_gb18030, 0, bytes.data(), static_cast<int>(bytes.size()), &w[0],w_len);

    // 4. 将 utf-16 转为 utf-8
    const auto u8_len = WideCharToMultiByte(CP_UTF8, 0, w.data(), w_len, nullptr, 0, nullptr, nullptr);
    std::string out(u8_len,0);
    WideCharToMultiByte(CP_UTF8, 0, w.data(), w_len,&out[0],u8_len,nullptr,nullptr);
    convertState = true;
    return out;
}

// 将 std::wstring (UTF-16) 转为 UTF-8 std::string
std::string WideToUtf8(const std::wstring &w, bool &convertState) {
    if (w.empty())
    {
        convertState = false;
        return {};
    }
    int size_needed = WideCharToMultiByte(
        CP_UTF8,            // 目标编码 UTF-8
        0,                  // flags
        w.data(), (int)w.size(),
        NULL, 0, NULL, NULL
    );
    if (size_needed <= 0) {
        convertState=false;
        return {};
    }
    std::string s(size_needed, 0);
    const int converted = WideCharToMultiByte(
        CP_UTF8, 0,
        w.data(), (int)w.size(),
        &s[0], size_needed,
        NULL, NULL
    );
    if (converted == 0) {
        convertState=false;
        return {};
    }
    convertState=true;
    return s;
}

std::string WideToLocalACP(const std::wstring &w, bool &convertState) {
    if (w.empty())
    {
        convertState=false;
        return {};
    }
    const UINT cp = GetACP(); // 或直接用 936
    const int n = WideCharToMultiByte(cp, 0, w.data(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
    std::string out(n, '\0');
    WideCharToMultiByte(cp, 0, w.data(), static_cast<int>(w.size()), &out[0], n, nullptr, nullptr);
    convertState=true;
    return out;
}

bool IsUtf8()
{
    if (GetACP() == 936)
    {
        return false;
    }
    return true;
}