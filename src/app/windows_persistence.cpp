/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "windows_persistence.hpp"
#include "windows_platform.hpp"
#include <vector>
namespace calculator::app {
namespace { constexpr wchar_t kRegistryPath[] = L"Software\\Infiltrator\\Calc"; }
ui::ThemeMode WindowsPersistence::load_theme_mode() const {
    DWORD value = 0U; DWORD size = sizeof(value);
    const LSTATUS status = RegGetValueW(HKEY_CURRENT_USER, kRegistryPath, L"ThemeMode", RRF_RT_REG_DWORD, nullptr, &value, &size);
    if (status != ERROR_SUCCESS) return ui::ThemeMode::System;
    if (value == 1U) return ui::ThemeMode::Day;
    if (value == 2U) return ui::ThemeMode::Night;
    return ui::ThemeMode::System;
}
void WindowsPersistence::save_theme_mode(ui::ThemeMode mode) const {
    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kRegistryPath, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr) != ERROR_SUCCESS) return;
    DWORD value = 0U; if (mode == ui::ThemeMode::Day) value = 1U; else if (mode == ui::ThemeMode::Night) value = 2U;
    (void)RegSetValueExW(key, L"ThemeMode", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&value), sizeof(value)); RegCloseKey(key);
}
void WindowsPersistence::save_controller_state() const {
    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kRegistryPath, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr) != ERROR_SUCCESS) return;
    const std::wstring state = win32::utf8_to_wide(controller_.persistent_state_text());
    const DWORD bytes = static_cast<DWORD>((state.size() + 1U) * sizeof(wchar_t));
    (void)RegSetValueExW(key, L"ControllerStateV1", 0, REG_SZ, reinterpret_cast<const BYTE*>(state.c_str()), bytes); RegCloseKey(key);
}
void WindowsPersistence::load_controller_state() {
    DWORD type = 0U; DWORD bytes = 0U;
    if (RegGetValueW(HKEY_CURRENT_USER, kRegistryPath, L"ControllerStateV1", RRF_RT_REG_SZ, &type, nullptr, &bytes) != ERROR_SUCCESS || type != REG_SZ || bytes < sizeof(wchar_t) || bytes > static_cast<DWORD>((ui::Controller::kMaxPersistentStateBytes + 1U) * sizeof(wchar_t))) return;
    std::vector<wchar_t> buffer(static_cast<std::size_t>(bytes / sizeof(wchar_t)), L'\0');
    if (RegGetValueW(HKEY_CURRENT_USER, kRegistryPath, L"ControllerStateV1", RRF_RT_REG_SZ, &type, buffer.data(), &bytes) != ERROR_SUCCESS) return;
    const std::wstring state(buffer.data()); (void)controller_.load_persistent_state_text(win32::wide_to_utf8(state));
}
} // namespace calculator::app
