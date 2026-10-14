/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "windows_platform.hpp"
#include <vector>
namespace calculator::app::win32 {
std::wstring utf8_to_wide(const std::string& text) {
    if (text.empty()) return {};
    const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (count <= 0) {
        const int fallback = MultiByteToWideChar(CP_ACP, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
        if (fallback <= 0) return {};
        std::wstring wide(static_cast<std::size_t>(fallback), L'\0');
        MultiByteToWideChar(CP_ACP, 0, text.data(), static_cast<int>(text.size()), wide.data(), fallback);
        return wide;
    }
    std::wstring wide(static_cast<std::size_t>(count), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), wide.data(), count);
    return wide;
}
std::string wide_to_utf8(const std::wstring& text) {
    if (text.empty()) return {};
    const int count = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    if (count <= 0) return {};
    std::string utf8(static_cast<std::size_t>(count), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), utf8.data(), count, nullptr, nullptr);
    return utf8;
}
std::wstring win32_multiline_text(std::string_view text) {
    std::string normalized; normalized.reserve(text.size() + text.size() / 8U); char previous = '\0';
    for (char ch : text) { if (ch == '\n' && previous != '\r') normalized.push_back('\r'); normalized.push_back(ch); previous = ch; }
    return utf8_to_wide(normalized);
}
std::wstring window_text(HWND window) {
    const int length = GetWindowTextLengthW(window); if (length <= 0) return {};
    std::vector<wchar_t> buffer(static_cast<std::size_t>(length) + 1U, L'\0');
    GetWindowTextW(window, buffer.data(), length + 1); return std::wstring(buffer.data(), static_cast<std::size_t>(length));
}
int window_dpi(HWND window) {
    if (window != nullptr) { const UINT dpi = GetDpiForWindow(window); if (dpi != 0U) return static_cast<int>(dpi); }
    HDC dc = GetDC(window); int dpi = 96; if (dc != nullptr) { dpi = GetDeviceCaps(dc, LOGPIXELSX); ReleaseDC(window, dc); }
    return dpi > 0 ? dpi : 96;
}
int sx(HWND window, int logical) { return MulDiv(logical, window_dpi(window), 96); }
bool system_prefers_dark() {
    DWORD value = 1U; DWORD size = sizeof(value);
    const LSTATUS status = RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", L"AppsUseLightTheme", RRF_RT_REG_DWORD, nullptr, &value, &size);
    return status == ERROR_SUCCESS ? value == 0U : true;
}
} // namespace calculator::app::win32
