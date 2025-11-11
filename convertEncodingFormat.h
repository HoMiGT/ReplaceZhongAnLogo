//
// Created by houmin on 25-11-11.
//

#ifndef CONVERTENCODINGFORMAT_H
#define CONVERTENCODINGFORMAT_H

#include <string>

std::string ToUtf8_FromUnknown(const std::string &bytes,bool &convertState, bool fallbackToSystemACP = true);

std::string WideToUtf8(const std::wstring &w, bool &convertState);

std::string WideToLocalACP(const std::wstring &w, bool &convertState);

bool IsUtf8();

#endif //CONVERTENCODINGFORMAT_H
