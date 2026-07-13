#include "win32/Win32Error.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <string_view>

namespace ctt::win32 {

std::wstring FormatWindowsError(const unsigned long errorCode) {
    wchar_t* buffer = nullptr;
    const DWORD length = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        errorCode,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<wchar_t*>(&buffer),
        0,
        nullptr);

    std::wstring message;
    if (length != 0 && buffer != nullptr) {
        message.assign(buffer, buffer + length);
        while (!message.empty() && (message.back() == L'\r' || message.back() == L'\n' || message.back() == L' ')) {
            message.pop_back();
        }
    } else {
        message = L"Unknown error";
    }
    if (buffer != nullptr) {
        LocalFree(buffer);
    }
    return message;
}

std::wstring MakeWindowsError(const std::wstring_view operation, const unsigned long errorCode) {
    std::wstring result{operation};
    result.append(L": ");
    result.append(FormatWindowsError(errorCode));
    result.append(L" (");
    result.append(std::to_wstring(errorCode));
    result.push_back(L')');
    return result;
}

}  // namespace ctt::win32
