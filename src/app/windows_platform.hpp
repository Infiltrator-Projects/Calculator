/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#include <windows.h>
#include <string>
#include <string_view>
namespace calculator::app::win32 {
std::wstring utf8_to_wide(const std::string& text);
std::string wide_to_utf8(const std::wstring& text);
std::wstring win32_multiline_text(std::string_view text);
std::wstring window_text(HWND window);
int window_dpi(HWND window);
int sx(HWND window, int logical);
bool system_prefers_dark();
} // namespace calculator::app::win32
