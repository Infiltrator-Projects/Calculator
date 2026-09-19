/* SPDX-License-Identifier: GPL-3.0-or-later */
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif

#include <windows.h>
#include <commctrl.h>

#include "../ui/calculator_ui_controller.hpp"
#include "../ui/calculator_theme.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

constexpr wchar_t kMainClass[] = L"CalculatorWindow";
constexpr wchar_t kHistoryClass[] = L"CalculatorHistoryWindow";

constexpr int kIdHistory = 1001;
constexpr int kIdTheme = 1002;
constexpr int kIdModeStandard = 1010;
constexpr int kIdModeScientific = 1011;
constexpr int kIdModeProgrammer = 1012;
constexpr int kIdKeyBase = 2000;
constexpr int kIdHistoryClear = 3001;

using calculator::ui::ThemeMode;
using calculator::ui::ThemePalette;

ThemeMode g_theme_mode = ThemeMode::System;
ThemePalette g_theme_palette{};
bool g_effective_dark_theme = true;

COLORREF to_colorref(std::uint32_t rgb) {
    return RGB(
        static_cast<BYTE>((rgb >> 16U) & 0xFFU),
        static_cast<BYTE>((rgb >> 8U) & 0xFFU),
        static_cast<BYTE>(rgb & 0xFFU));
}

#define kBackground to_colorref(g_theme_palette.background_rgb)
#define kPanel to_colorref(g_theme_palette.panel_rgb)
#define kCard to_colorref(g_theme_palette.card_rgb)
#define kSurface to_colorref(g_theme_palette.surface_rgb)
#define kInput to_colorref(g_theme_palette.input_rgb)
#define kBorder to_colorref(g_theme_palette.border_rgb)
#define kText to_colorref(g_theme_palette.text_rgb)
#define kTitle to_colorref(g_theme_palette.title_rgb)
#define kMuted to_colorref(g_theme_palette.muted_rgb)
#define kSubtle to_colorref(g_theme_palette.subtle_rgb)
#define kButtonBackground to_colorref(g_theme_palette.button_background_rgb)
#define kButtonForeground to_colorref(g_theme_palette.button_foreground_rgb)
#define kSelection to_colorref(g_theme_palette.selection_background_rgb)
#define kNeutralAccent to_colorref(g_theme_palette.neutral_accent_rgb)
#define kWarning to_colorref(g_theme_palette.warning_rgb)
#define kFault to_colorref(g_theme_palette.fault_rgb)
#define kOperation to_colorref(g_theme_palette.operation_rgb)
#define kCardHover to_colorref(g_theme_palette.card_hover_rgb)
#define kSurfaceHover to_colorref(g_theme_palette.surface_hover_rgb)
#define kOperationHover to_colorref(g_theme_palette.operation_hover_rgb)
#define kEqualsHover to_colorref(g_theme_palette.equals_hover_rgb)

using calculator::ui::ButtonRole;
using calculator::ui::ButtonSpec;
using calculator::ui::Command;
using calculator::ui::Controller;
using calculator::ui::Mode;

enum class ButtonKind { Number, Operation, Utility, Clear, Equals, Mode, Toolbar };

HINSTANCE g_instance = nullptr;
HWND g_main = nullptr;
HWND g_title = nullptr;
HWND g_subtitle = nullptr;
HWND g_history_button = nullptr;
HWND g_theme_button = nullptr;
HWND g_expression = nullptr;
HWND g_result = nullptr;
HWND g_status = nullptr;
HWND g_footer = nullptr;
HWND g_mode_buttons[3] = {nullptr, nullptr, nullptr};
HWND g_history_window = nullptr;
HWND g_history_edit = nullptr;
HWND g_history_dock = nullptr;
HWND g_hover_button = nullptr;

std::vector<HWND> g_standard_buttons;
std::vector<HWND> g_scientific_buttons;
std::vector<HWND> g_programmer_buttons;
std::unordered_map<int, const ButtonSpec*> g_key_specs;

HFONT g_ui_font = nullptr;
HFONT g_ui_bold_font = nullptr;
HFONT g_title_font = nullptr;
HFONT g_result_font = nullptr;
HFONT g_small_font = nullptr;
std::vector<HANDLE> g_private_font_handles;

HBRUSH g_background_brush = nullptr;
HBRUSH g_panel_brush = nullptr;
HBRUSH g_input_brush = nullptr;

RECT g_mode_rect{};
RECT g_display_rect{};

WNDPROC g_old_edit_proc = nullptr;

Controller g_controller;
bool g_status_fault = false;

std::wstring utf8_to_wide(const std::string& text) {
    if (text.empty()) return {};
    const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                          text.data(), static_cast<int>(text.size()),
                                          nullptr, 0);
    if (count <= 0) {
        const int fallback = MultiByteToWideChar(CP_ACP, 0, text.data(),
                                                 static_cast<int>(text.size()),
                                                 nullptr, 0);
        if (fallback <= 0) return {};
        std::wstring wide(static_cast<std::size_t>(fallback), L'\0');
        MultiByteToWideChar(CP_ACP, 0, text.data(), static_cast<int>(text.size()),
                            wide.data(), fallback);
        return wide;
    }

    std::wstring wide(static_cast<std::size_t>(count), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                        text.data(), static_cast<int>(text.size()),
                        wide.data(), count);
    return wide;
}

std::string wide_to_utf8(const std::wstring& text) {
    if (text.empty()) return {};
    const int count = WideCharToMultiByte(CP_UTF8, 0, text.data(),
                                          static_cast<int>(text.size()),
                                          nullptr, 0, nullptr, nullptr);
    if (count <= 0) return {};
    std::string utf8(static_cast<std::size_t>(count), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                        utf8.data(), count, nullptr, nullptr);
    return utf8;
}

std::wstring window_text(HWND window) {
    const int length = GetWindowTextLengthW(window);
    if (length <= 0) return {};
    std::vector<wchar_t> buffer(static_cast<std::size_t>(length) + 1U, L'\0');
    GetWindowTextW(window, buffer.data(), length + 1);
    return std::wstring(buffer.data(), static_cast<std::size_t>(length));
}

int window_dpi(HWND window) {
    HDC dc = GetDC(window);
    int dpi = 96;
    if (dc != nullptr) {
        dpi = GetDeviceCaps(dc, LOGPIXELSX);
        ReleaseDC(window, dc);
    }
    return dpi > 0 ? dpi : 96;
}

int sx(HWND window, int logical) {
    return MulDiv(logical, window_dpi(window), 96);
}

bool system_prefers_dark() {
    DWORD value = 1;
    DWORD size = sizeof(value);
    const LSTATUS status = RegGetValueW(
        HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        L"AppsUseLightTheme",
        RRF_RT_REG_DWORD,
        nullptr,
        &value,
        &size);
    return status == ERROR_SUCCESS ? value == 0 : true;
}

ThemeMode load_theme_mode() {
    DWORD value = 0;
    DWORD size = sizeof(value);
    const LSTATUS status = RegGetValueW(
        HKEY_CURRENT_USER,
        L"Software\\Infiltrator\\Calc",
        L"ThemeMode",
        RRF_RT_REG_DWORD,
        nullptr,
        &value,
        &size);
    if (status != ERROR_SUCCESS) return ThemeMode::System;
    if (value == 1) return ThemeMode::Day;
    if (value == 2) return ThemeMode::Night;
    return ThemeMode::System;
}

void save_theme_mode() {
    HKEY key = nullptr;
    if (RegCreateKeyExW(
            HKEY_CURRENT_USER,
            L"Software\\Infiltrator\\Calc",
            0, nullptr, 0, KEY_SET_VALUE,
            nullptr, &key, nullptr) != ERROR_SUCCESS) {
        return;
    }
    DWORD value = 0;
    if (g_theme_mode == ThemeMode::Day) value = 1;
    else if (g_theme_mode == ThemeMode::Night) value = 2;
    (void)RegSetValueExW(
        key, L"ThemeMode", 0, REG_DWORD,
        reinterpret_cast<const BYTE*>(&value), sizeof(value));
    RegCloseKey(key);
}

void resolve_theme() {
    const bool system_dark = system_prefers_dark();
    g_effective_dark_theme =
        g_theme_mode == ThemeMode::Night ||
        (g_theme_mode == ThemeMode::System && system_dark);
    g_theme_palette =
        calculator::ui::resolved_palette(g_theme_mode, system_dark);
}

void recreate_theme_brushes() {
    if (g_background_brush != nullptr) DeleteObject(g_background_brush);
    if (g_panel_brush != nullptr) DeleteObject(g_panel_brush);
    if (g_input_brush != nullptr) DeleteObject(g_input_brush);
    g_background_brush = CreateSolidBrush(kBackground);
    g_panel_brush = CreateSolidBrush(kPanel);
    g_input_brush = CreateSolidBrush(kInput);
}

const wchar_t* ui_font_family() {
    return L"MB Corpo S Title WEB";
}

const wchar_t* brand_font_family() {
    return L"MB Corpo A Title Cond WEB";
}

bool register_font_resource(int resource_id) {
    HRSRC resource = FindResourceW(
        g_instance, MAKEINTRESOURCEW(resource_id), MAKEINTRESOURCEW(10));
    if (resource == nullptr) return false;

    HGLOBAL loaded = LoadResource(g_instance, resource);
    if (loaded == nullptr) return false;

    const DWORD size = SizeofResource(g_instance, resource);
    const void* bytes = LockResource(loaded);
    if (bytes == nullptr || size == 0U) return false;

    DWORD registered_count = 0;
    HANDLE handle = AddFontMemResourceEx(
        const_cast<void*>(bytes), size, nullptr, &registered_count);
    if (handle == nullptr || registered_count == 0U) return false;

    g_private_font_handles.push_back(handle);
    return true;
}

bool register_bundled_fonts() {
    return
        register_font_resource(CALCULATOR_FONT_RESOURCE_REGULAR) &&
        register_font_resource(CALCULATOR_FONT_RESOURCE_BOLD) &&
        register_font_resource(CALCULATOR_FONT_RESOURCE_CONDENSED);
}

HFONT make_font(HWND window, int point_size, int weight, const wchar_t* family) {
    HDC dc = GetDC(window);
    const int dpi = dc != nullptr ? GetDeviceCaps(dc, LOGPIXELSY) : 96;
    if (dc != nullptr) ReleaseDC(window, dc);

    const int height = -MulDiv(point_size, dpi, 72);
    return CreateFontW(height, 0, 0, 0, weight, FALSE, FALSE, FALSE,
                       DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                       CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                       DEFAULT_PITCH | FF_DONTCARE, family);
}

void apply_font(HWND control, HFONT font) {
    if (control != nullptr && font != nullptr) {
        SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    }
}

void apply_nonclient_theme(HWND window) {
    using DwmSetWindowAttributeFn = HRESULT(WINAPI*)(HWND, DWORD, LPCVOID, DWORD);
    HMODULE module = LoadLibraryW(L"dwmapi.dll");
    if (module == nullptr) return;

    auto set_attribute = reinterpret_cast<DwmSetWindowAttributeFn>(
        GetProcAddress(module, "DwmSetWindowAttribute"));
    if (set_attribute != nullptr) {
        BOOL dark = g_effective_dark_theme ? TRUE : FALSE;
        COLORREF caption = kBackground;
        COLORREF text = kTitle;
        COLORREF border = kNeutralAccent;
        (void)set_attribute(window, 20U, &dark, sizeof(dark));
        (void)set_attribute(window, 35U, &caption, sizeof(caption));
        (void)set_attribute(window, 36U, &text, sizeof(text));
        (void)set_attribute(window, 34U, &border, sizeof(border));
    }
    FreeLibrary(module);
}

void apply_control_theme(HWND control) {
    using SetWindowThemeFn = HRESULT(WINAPI*)(HWND, LPCWSTR, LPCWSTR);
    HMODULE module = LoadLibraryW(L"uxtheme.dll");
    if (module == nullptr) return;
    auto set_theme = reinterpret_cast<SetWindowThemeFn>(
        GetProcAddress(module, "SetWindowTheme"));
    if (set_theme != nullptr) {
        (void)set_theme(
            control,
            g_effective_dark_theme ? L"DarkMode_Explorer" : L"Explorer",
            nullptr);
    }
    FreeLibrary(module);
}

std::wstring format_value(double value) {
    return utf8_to_wide(calculator::format_value(value));
}

std::size_t expression_cursor() {
    DWORD start = 0;
    DWORD end = 0;
    SendMessageW(
        g_expression, EM_GETSEL,
        reinterpret_cast<WPARAM>(&start),
        reinterpret_cast<LPARAM>(&end));
    return static_cast<std::size_t>(start);
}

void sync_controller_expression() {
    g_controller.set_expression(wide_to_utf8(window_text(g_expression)));
}

void show_grid(std::vector<HWND>& buttons, bool visible);
void refresh_history();
void redraw_button(HWND button);
void redraw_active_grid();
void redraw_mode_buttons();

void render_state(std::size_t cursor = Controller::kEnd) {
    const auto& state = g_controller.state();

    const std::wstring expression = utf8_to_wide(state.expression);
    if (window_text(g_expression) != expression) {
        SetWindowTextW(g_expression, expression.c_str());
    }

    const std::size_t effective_cursor =
        cursor == Controller::kEnd ? state.expression.size() : cursor;
    const LONG caret = static_cast<LONG>(
        std::min(effective_cursor, state.expression.size()));
    SendMessageW(g_expression, EM_SETSEL, caret, caret);

    SetWindowTextW(g_result, utf8_to_wide(state.result).c_str());
    SetWindowTextW(g_status, utf8_to_wide(state.status).c_str());
    g_status_fault = state.fault;

    show_grid(g_standard_buttons, state.mode == Mode::Standard);
    show_grid(g_scientific_buttons, state.mode == Mode::Scientific);
    show_grid(g_programmer_buttons, state.mode == Mode::Programmer);

    for (HWND button : g_scientific_buttons) {
        const int id = GetDlgCtrlID(button);
        const auto it = g_key_specs.find(id);
        if (it != g_key_specs.end() && it->second != nullptr &&
            it->second->command == Command::ToggleDegrees) {
            SetWindowTextW(button, state.degrees ? L"DEG" : L"RAD");
            break;
        }
    }

    for (const auto& buttons :
         {&g_standard_buttons, &g_scientific_buttons, &g_programmer_buttons}) {
        for (HWND button : *buttons) {
            const int id = GetDlgCtrlID(button);
            const auto it = g_key_specs.find(id);
            if (it != g_key_specs.end() && it->second != nullptr) {
                EnableWindow(
                    button,
                    g_controller.command_enabled(it->second->command) ? TRUE : FALSE);
            }
        }
    }

    refresh_history();
    redraw_mode_buttons();
    redraw_active_grid();
    InvalidateRect(g_status, nullptr, TRUE);
    InvalidateRect(g_main, nullptr, TRUE);
}

void dispatch_command(const ButtonSpec& spec) {
    sync_controller_expression();
    const auto result =
        g_controller.dispatch(spec.command, expression_cursor());
    render_state(result.cursor);
    SetFocus(g_expression);
}

void calculate_from_entry() {
    sync_controller_expression();
    const auto result =
        g_controller.dispatch(Command::Equals, expression_cursor());
    render_state(result.cursor);
    SetFocus(g_expression);
}

std::wstring history_text() {
    if (g_controller.session().history().empty()) {
        return L"No calculations yet.";
    }

    std::wstring text;
    std::size_t shown = 0;
    for (auto it = g_controller.session().history().rbegin();
         it != g_controller.session().history().rend() && shown < 50U;
         ++it, ++shown) {
        text += utf8_to_wide(it->input);
        text += L"\r\n  = ";
        if (it->result.ok) {
            text += format_value(it->result.value);
        } else {
            text += L"Error: ";
            text += utf8_to_wide(it->result.error);
        }
        text += L"\r\n\r\n";
    }
    return text;
}

void refresh_history() {
    const std::wstring text = history_text();
    if (g_history_edit != nullptr) {
        SetWindowTextW(g_history_edit, text.c_str());
    }
    if (g_history_dock != nullptr) {
        SetWindowTextW(g_history_dock, text.c_str());
    }
}

void show_history() {
    if (g_history_window != nullptr && IsWindow(g_history_window)) {
        refresh_history();
        ShowWindow(g_history_window, SW_SHOWNORMAL);
        SetForegroundWindow(g_history_window);
        return;
    }

    g_history_window = CreateWindowExW(
        WS_EX_TOOLWINDOW, kHistoryClass, L"Calculation History",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, sx(g_main, 620), sx(g_main, 540),
        g_main, nullptr, g_instance, nullptr);
}

ButtonKind button_kind(int id, const ButtonSpec* spec) {
    if (id == kIdHistory || id == kIdTheme) return ButtonKind::Toolbar;
    if (id == kIdModeStandard || id == kIdModeScientific ||
        id == kIdModeProgrammer) return ButtonKind::Mode;
    if (spec == nullptr) return ButtonKind::Operation;

    switch (spec->role) {
    case ButtonRole::Number: return ButtonKind::Number;
    case ButtonRole::Operation: return ButtonKind::Operation;
    case ButtonRole::Utility: return ButtonKind::Utility;
    case ButtonRole::Clear: return ButtonKind::Clear;
    case ButtonRole::Equals: return ButtonKind::Equals;
    }
    return ButtonKind::Operation;
}

bool is_memory_command(Command command) {
    return command == Command::MemoryClear ||
           command == Command::MemoryRecall ||
           command == Command::MemoryAdd ||
           command == Command::MemorySubtract;
}

bool is_selected_button(int id, const ButtonSpec* spec) {
    const auto& state = g_controller.state();

    if (id == kIdModeStandard) return state.mode == Mode::Standard;
    if (id == kIdModeScientific) return state.mode == Mode::Scientific;
    if (id == kIdModeProgrammer) return state.mode == Mode::Programmer;
    if (spec == nullptr) return false;

    switch (spec->command) {
    case Command::BaseBin:
        return state.programmer_base == calculator::ProgrammerBase::Binary;
    case Command::BaseOct:
        return state.programmer_base == calculator::ProgrammerBase::Octal;
    case Command::BaseDec:
        return state.programmer_base == calculator::ProgrammerBase::Decimal;
    case Command::BaseHex:
        return state.programmer_base == calculator::ProgrammerBase::Hexadecimal;
    case Command::Width8:
        return state.programmer_width == calculator::IntegerWidth::Bits8;
    case Command::Width16:
        return state.programmer_width == calculator::IntegerWidth::Bits16;
    case Command::Width32:
        return state.programmer_width == calculator::IntegerWidth::Bits32;
    case Command::Width64:
        return state.programmer_width == calculator::IntegerWidth::Bits64;
    case Command::ToggleSigned:
        return state.programmer_signed;
    default:
        return false;
    }
}

LRESULT draw_button(const DRAWITEMSTRUCT* item) {
    if (item == nullptr || item->CtlType != ODT_BUTTON) return FALSE;

    const int id = static_cast<int>(item->CtlID);
    const std::wstring label = window_text(item->hwndItem);
    const auto spec_it = g_key_specs.find(id);
    const ButtonSpec* spec =
        spec_it != g_key_specs.end() ? spec_it->second : nullptr;
    const ButtonKind kind = button_kind(id, spec);
    const bool selected = is_selected_button(id, spec);
    const bool pressed = (item->itemState & ODS_SELECTED) != 0U;
    const bool disabled = (item->itemState & ODS_DISABLED) != 0U;
    const bool hovered = item->hwndItem == g_hover_button;

    COLORREF fill = kCard;
    COLORREF text = kTitle;
    COLORREF border = kBorder;

    switch (kind) {
    case ButtonKind::Number:
        fill = kCard;
        text = kTitle;
        break;
    case ButtonKind::Operation:
        fill = kOperation;
        text = kButtonBackground;
        break;
    case ButtonKind::Utility:
        if (spec != nullptr && is_memory_command(spec->command)) {
            fill = kBackground;
            text = kMuted;
            border = kBackground;
        } else {
            fill = selected ? kSelection : kSurface;
            text = selected ? kTitle : kMuted;
            border = selected ? kNeutralAccent : kBorder;
        }
        break;
    case ButtonKind::Clear:
        fill = kCard;
        text = kWarning;
        border = kWarning;
        break;
    case ButtonKind::Equals:
        fill = kButtonBackground;
        text = kButtonForeground;
        border = kButtonBackground;
        break;
    case ButtonKind::Mode:
        fill = selected ? kButtonBackground : kSurface;
        text = selected ? kButtonForeground : kSubtle;
        border = selected ? kButtonBackground : kBorder;
        break;
    case ButtonKind::Toolbar:
        fill = kSurface;
        text = kMuted;
        border = kBorder;
        break;
    }

    if (hovered && !disabled && !pressed) {
        switch (kind) {
        case ButtonKind::Number:
        case ButtonKind::Clear:
            fill = kCardHover;
            break;
        case ButtonKind::Operation:
            fill = kOperationHover;
            break;
        case ButtonKind::Utility:
            if (!selected) fill = kSurfaceHover;
            break;
        case ButtonKind::Equals:
            fill = kEqualsHover;
            break;
        case ButtonKind::Mode:
            if (!selected) fill = kSurfaceHover;
            break;
        case ButtonKind::Toolbar:
            fill = kSurfaceHover;
            break;
        }
    }

    if (pressed && kind != ButtonKind::Equals && !selected) {
        fill = kSelection;
    }
    if (disabled) {
        fill = kInput;
        text = kSubtle;
    }

    RECT rect = item->rcItem;
    HBRUSH corner_brush = CreateSolidBrush(
        kind == ButtonKind::Mode ? kSurface : kBackground);
    FillRect(item->hDC, &rect, corner_brush);
    DeleteObject(corner_brush);

    HBRUSH brush = CreateSolidBrush(fill);
    HPEN pen = CreatePen(PS_SOLID, sx(g_main, 1), border);
    HGDIOBJ old_brush = SelectObject(item->hDC, brush);
    HGDIOBJ old_pen = SelectObject(item->hDC, pen);
    RoundRect(item->hDC, rect.left, rect.top, rect.right, rect.bottom,
              sx(g_main, 10), sx(g_main, 10));
    SelectObject(item->hDC, old_brush);
    SelectObject(item->hDC, old_pen);
    DeleteObject(brush);
    DeleteObject(pen);

    HFONT font = reinterpret_cast<HFONT>(
        SendMessageW(item->hwndItem, WM_GETFONT, 0, 0));
    HGDIOBJ old_font = font != nullptr ? SelectObject(item->hDC, font) : nullptr;
    const int old_mode = SetBkMode(item->hDC, TRANSPARENT);
    const COLORREF old_text = SetTextColor(item->hDC, text);
    RECT text_rect = rect;
    InflateRect(&text_rect, -sx(g_main, 6), -sx(g_main, 2));
    DrawTextW(item->hDC, label.c_str(), -1, &text_rect,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    SetTextColor(item->hDC, old_text);
    SetBkMode(item->hDC, old_mode);
    if (old_font != nullptr) SelectObject(item->hDC, old_font);

    if ((item->itemState & ODS_FOCUS) != 0U) {
        InflateRect(&rect, -sx(g_main, 4), -sx(g_main, 4));
        DrawFocusRect(item->hDC, &rect);
    }

    return TRUE;
}

LRESULT CALLBACK button_subclass_proc(HWND window, UINT message,
                                      WPARAM wparam, LPARAM lparam,
                                      UINT_PTR subclass_id, DWORD_PTR) {
    switch (message) {
    case WM_MOUSEMOVE:
        if (g_hover_button != window) {
            HWND previous = g_hover_button;
            g_hover_button = window;
            if (previous != nullptr) {
                RedrawWindow(previous, nullptr, nullptr,
                             RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW);
            }
            TRACKMOUSEEVENT tracking{sizeof(TRACKMOUSEEVENT), TME_LEAVE,
                                     window, HOVER_DEFAULT};
            TrackMouseEvent(&tracking);
            RedrawWindow(window, nullptr, nullptr,
                         RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW);
        }
        break;
    case WM_MOUSELEAVE:
        if (g_hover_button == window) g_hover_button = nullptr;
        RedrawWindow(window, nullptr, nullptr,
                     RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW);
        break;
    case WM_NCDESTROY:
        if (g_hover_button == window) g_hover_button = nullptr;
        RemoveWindowSubclass(window, button_subclass_proc, subclass_id);
        break;
    default:
        break;
    }
    return DefSubclassProc(window, message, wparam, lparam);
}

HWND create_button(HWND parent, int id, const wchar_t* label, HFONT font) {
    HWND button = CreateWindowExW(
        0, L"BUTTON", label,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
        0, 0, 0, 0, parent,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        g_instance, nullptr);
    apply_font(button, font);
    apply_control_theme(button);
    SetWindowSubclass(button, button_subclass_proc, 1, 0);
    return button;
}

void create_key_grid(const ButtonSpec* specs, std::size_t count,
                     std::vector<HWND>& destination, int& next_id) {
    destination.clear();
    destination.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        const int id = next_id++;
        const std::wstring label = utf8_to_wide(std::string(specs[i].label));
        g_key_specs.emplace(id, &specs[i]);
        destination.push_back(
            create_button(g_main, id, label.c_str(), g_ui_bold_font));
    }
}

void show_grid(std::vector<HWND>& buttons, bool visible) {
    for (HWND button : buttons) {
        ShowWindow(button, visible ? SW_SHOW : SW_HIDE);
    }
}

void redraw_button(HWND button) {
    if (button != nullptr) {
        RedrawWindow(button, nullptr, nullptr,
                     RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW);
    }
}

void redraw_buttons(const std::vector<HWND>& buttons) {
    for (HWND button : buttons) redraw_button(button);
}

void redraw_active_grid() {
    const Mode mode = g_controller.state().mode;
    if (mode == Mode::Standard) redraw_buttons(g_standard_buttons);
    else if (mode == Mode::Scientific) redraw_buttons(g_scientific_buttons);
    else redraw_buttons(g_programmer_buttons);
}

void redraw_mode_buttons() {
    for (HWND button : g_mode_buttons) redraw_button(button);
}

void update_mode_ui() {
    render_state();
    SetFocus(g_expression);
}

void layout_grid_range(const std::vector<HWND>& buttons,
                       std::size_t start_index, int rows,
                       int left, int top, int width, int height) {
    if (start_index >= buttons.size() || rows <= 0) return;

    const int gap = sx(
        g_main, calculator::ui::kDesktopMetrics.grid_gap_x);
    const int columns = 4;
    const int button_width = std::max(sx(g_main, 46),
                                      (width - gap * (columns - 1)) / columns);
    const int button_height = std::max(sx(g_main, 22),
                                       (height - gap * (rows - 1)) / rows);
    const std::size_t count = std::min(
        buttons.size() - start_index,
        static_cast<std::size_t>(rows * columns));

    for (std::size_t index = 0; index < count; ++index) {
        const int row = static_cast<int>(index) / columns;
        const int column = static_cast<int>(index) % columns;
        MoveWindow(buttons[start_index + index],
                   left + column * (button_width + gap),
                   top + row * (button_height + gap),
                   button_width, button_height, TRUE);
    }
}

void layout_grid(const std::vector<HWND>& buttons, int rows,
                 int left, int top, int width, int height) {
    layout_grid_range(buttons, 0, rows, left, top, width, height);
}

void layout_standard_grid(int left, int top, int width, int height) {
    if (g_standard_buttons.size() < 28U) return;

    const int memory_height = sx(
        g_main, calculator::ui::kDesktopMetrics.memory_height);
    const int memory_gap = sx(g_main, 2);
    const int memory_width = width / 4;

    for (int i = 0; i < 4; ++i) {
        const int x = left + i * memory_width;
        const int w = (i == 3) ? (left + width - x) : memory_width;
        MoveWindow(g_standard_buttons[static_cast<std::size_t>(i)],
                   x, top, w, memory_height, TRUE);
    }

    layout_grid_range(g_standard_buttons, 4, 6,
                      left, top + memory_height + memory_gap,
                      width, std::max(0, height - memory_height - memory_gap));
}

void layout_main(HWND window) {
    RECT client{};
    GetClientRect(window, &client);

    const auto& metrics = calculator::ui::kDesktopMetrics;
    const int width = client.right - client.left;
    const int height = client.bottom - client.top;
    const int dpi = window_dpi(window);
    const int logical_width = MulDiv(width, 96, dpi);
    const int logical_height = MulDiv(height, 96, dpi);
    const auto responsive =
        calculator::ui::responsive_layout(
            logical_width, logical_height);

    const int margin = sx(window, metrics.shell_padding);
    const int gap = sx(window, metrics.section_gap);
    const int calc_left = margin;
    int calc_right = width - margin;

    if (responsive.dock_history) {
        const int history_min = sx(window, metrics.history_min_width);
        const int calculator_min = sx(window, metrics.default_width);
        const int available = std::max(
            history_min,
            width - margin * 2 - gap - calculator_min);
        const int history_width = std::max(
            history_min,
            std::min(width / 3, available));

        calc_right = width - margin - gap - history_width;
        MoveWindow(
            g_history_dock,
            calc_right + gap, margin,
            history_width, std::max(0, height - margin * 2), TRUE);
        ShowWindow(g_history_dock, SW_SHOW);
        ShowWindow(g_history_button, SW_HIDE);
    } else {
        ShowWindow(g_history_dock, SW_HIDE);
        ShowWindow(g_history_button, SW_SHOW);
    }

    const int content_width = std::max(0, calc_right - calc_left);
    int y = margin;

    const int toolbar_width = sx(window, 76);
    const int toolbar_height = sx(window, 30);
    const int toolbar_count = responsive.dock_history ? 1 : 2;
    const int title_right =
        calc_right - toolbar_count * toolbar_width -
        (toolbar_count > 0 ? toolbar_count * gap : 0);

    MoveWindow(
        g_title, calc_left, y,
        std::max(0, title_right - calc_left), sx(window, 30), TRUE);
    ShowWindow(g_subtitle, SW_HIDE);

    MoveWindow(
        g_theme_button,
        calc_right - toolbar_count * toolbar_width -
            (toolbar_count - 1) * gap,
        y, toolbar_width, toolbar_height, TRUE);

    if (!responsive.dock_history) {
        MoveWindow(
            g_history_button,
            calc_right - toolbar_width, y,
            toolbar_width, toolbar_height, TRUE);
    }

    y += sx(window, responsive.compact_controls ? 34 : 38);

    g_mode_rect = RECT{
        calc_left, y, calc_right,
        y + sx(window, responsive.compact_controls ? 34 : 38)};
    const int mode_padding = sx(window, 3);
    const int mode_gap = sx(window, 3);
    const int mode_width =
        (content_width - mode_padding * 2 - mode_gap * 2) / 3;
    const int mode_height = sx(
        window, responsive.compact_controls ? 28 : 32);
    for (int i = 0; i < 3; ++i) {
        MoveWindow(
            g_mode_buttons[i],
            calc_left + mode_padding + i * (mode_width + mode_gap),
            y + mode_padding, mode_width, mode_height, TRUE);
    }

    y += sx(window, responsive.compact_controls ? 40 : 46);

    const int display_height = sx(
        window, responsive.compact_controls ? 92 : metrics.display_height);
    g_display_rect = RECT{
        calc_left, y, calc_right, y + display_height};
    const int display_padding = sx(window, responsive.compact_controls ? 9 : 12);
    MoveWindow(
        g_expression,
        calc_left + display_padding, y + sx(window, 7),
        content_width - display_padding * 2, sx(window, 24), TRUE);
    MoveWindow(
        g_result,
        calc_left + display_padding, y + sx(window, 30),
        content_width - display_padding * 2,
        sx(window, responsive.compact_controls ? 38 : 46), TRUE);
    MoveWindow(
        g_status,
        calc_left + display_padding,
        y + display_height - sx(window, 22),
        content_width - display_padding * 2, sx(window, 16), TRUE);

    y += display_height + sx(window, 8);

    const int grid_bottom = height - margin;
    const int minimum_grid = sx(window, responsive.compact_controls ? 260 : 300);
    const int grid_height = std::max(minimum_grid, grid_bottom - y);
    const Mode active_mode = g_controller.state().mode;
    if (active_mode == Mode::Standard) {
        layout_standard_grid(
            calc_left, y, content_width, grid_height);
    } else if (active_mode == Mode::Scientific) {
        layout_grid(
            g_scientific_buttons, 10,
            calc_left, y, content_width, grid_height);
    } else {
        layout_grid(
            g_programmer_buttons, 10,
            calc_left, y, content_width, grid_height);
    }

    ShowWindow(g_footer, SW_HIDE);
}

void draw_panel(HDC dc, const RECT& rect, COLORREF fill, COLORREF border,
                int radius) {
    HBRUSH brush = CreateSolidBrush(fill);
    HPEN pen = CreatePen(PS_SOLID, sx(g_main, 1), border);
    HGDIOBJ old_brush = SelectObject(dc, brush);
    HGDIOBJ old_pen = SelectObject(dc, pen);
    RoundRect(dc, rect.left, rect.top, rect.right, rect.bottom,
              sx(g_main, radius), sx(g_main, radius));
    SelectObject(dc, old_brush);
    SelectObject(dc, old_pen);
    DeleteObject(brush);
    DeleteObject(pen);
}

void apply_theme_to_window(HWND window) {
    if (window == nullptr || !IsWindow(window)) return;
    apply_nonclient_theme(window);
    EnumChildWindows(
        window,
        +[](HWND child, LPARAM) -> BOOL {
            apply_control_theme(child);
            InvalidateRect(child, nullptr, TRUE);
            return TRUE;
        },
        0);
    InvalidateRect(window, nullptr, TRUE);
}

void apply_theme(bool persist) {
    resolve_theme();
    recreate_theme_brushes();

    if (g_theme_button != nullptr) {
        const std::wstring label = utf8_to_wide(
            std::string(calculator::ui::theme_mode_name(g_theme_mode)));
        SetWindowTextW(g_theme_button, label.c_str());
    }

    apply_theme_to_window(g_main);
    apply_theme_to_window(g_history_window);
    if (persist) save_theme_mode();
}

void create_controls(HWND window) {
    g_title = CreateWindowExW(0, L"STATIC", L"Calculator",
                              WS_CHILD | WS_VISIBLE | SS_LEFT,
                              0, 0, 0, 0, window, nullptr, g_instance, nullptr);
    g_subtitle = CreateWindowExW(0, L"STATIC", L"PRECISION DESKTOP CALCULATOR",
                                 WS_CHILD | SS_LEFT,
                                 0, 0, 0, 0, window, nullptr, g_instance, nullptr);
    g_theme_button = create_button(window, kIdTheme, L"System", g_ui_bold_font);
    g_history_button = create_button(window, kIdHistory, L"History", g_ui_bold_font);

    const std::wstring standard_mode =
        utf8_to_wide(std::string(calculator::ui::mode_name(Mode::Standard)));
    const std::wstring scientific_mode =
        utf8_to_wide(std::string(calculator::ui::mode_name(Mode::Scientific)));
    const std::wstring programmer_mode =
        utf8_to_wide(std::string(calculator::ui::mode_name(Mode::Programmer)));
    g_mode_buttons[0] = create_button(
        window, kIdModeStandard, standard_mode.c_str(), g_ui_bold_font);
    g_mode_buttons[1] = create_button(
        window, kIdModeScientific, scientific_mode.c_str(), g_ui_bold_font);
    g_mode_buttons[2] = create_button(
        window, kIdModeProgrammer, programmer_mode.c_str(), g_ui_bold_font);

    g_expression = CreateWindowExW(
        0, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_RIGHT | ES_AUTOHSCROLL,
        0, 0, 0, 0, window, nullptr, g_instance, nullptr);
    apply_control_theme(g_expression);

    g_result = CreateWindowExW(0, L"STATIC", L"0",
                               WS_CHILD | WS_VISIBLE | SS_RIGHT | SS_NOPREFIX,
                               0, 0, 0, 0, window, nullptr, g_instance, nullptr);
    g_status = CreateWindowExW(0, L"STATIC", L"READY",
                               WS_CHILD | WS_VISIBLE | SS_RIGHT | SS_NOPREFIX,
                               0, 0, 0, 0, window, nullptr, g_instance, nullptr);
    g_footer = CreateWindowExW(
        0, L"STATIC",
        L"Keyboard ready · Variables, memory and history retained",
        WS_CHILD | SS_LEFT | SS_NOPREFIX,
        0, 0, 0, 0, window, nullptr, g_instance, nullptr);

    g_history_dock = CreateWindowExW(
        0, L"EDIT", L"",
        WS_CHILD | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
        0, 0, 0, 0, window, nullptr, g_instance, nullptr);
    apply_control_theme(g_history_dock);

    apply_font(g_title, g_title_font);
    apply_font(g_subtitle, g_small_font);
    apply_font(g_expression, g_ui_font);
    apply_font(g_result, g_result_font);
    apply_font(g_status, g_small_font);
    apply_font(g_footer, g_small_font);
    apply_font(g_history_dock, g_ui_font);

    g_old_edit_proc = reinterpret_cast<WNDPROC>(
        SetWindowLongPtrW(g_expression, GWLP_WNDPROC,
                          reinterpret_cast<LONG_PTR>(
                              +[](HWND edit, UINT message, WPARAM wparam, LPARAM lparam) -> LRESULT {
                                  if (message == WM_KEYDOWN && wparam == VK_RETURN) {
                                      calculate_from_entry();
                                      return 0;
                                  }
                                  return CallWindowProcW(g_old_edit_proc, edit, message, wparam, lparam);
                              })));

    int next_id = kIdKeyBase;

    g_standard_buttons.clear();
    g_standard_buttons.reserve(
        calculator::ui::kStandardMemory.size() +
        calculator::ui::kStandardKeypad.size());
    for (const ButtonSpec& spec : calculator::ui::kStandardMemory) {
        const int id = next_id++;
        const std::wstring label = utf8_to_wide(std::string(spec.label));
        g_key_specs.emplace(id, &spec);
        g_standard_buttons.push_back(
            create_button(g_main, id, label.c_str(), g_ui_bold_font));
    }
    for (const ButtonSpec& spec : calculator::ui::kStandardKeypad) {
        const int id = next_id++;
        const std::wstring label = utf8_to_wide(std::string(spec.label));
        g_key_specs.emplace(id, &spec);
        g_standard_buttons.push_back(
            create_button(g_main, id, label.c_str(), g_ui_bold_font));
    }

    create_key_grid(
        calculator::ui::kScientificKeypad.data(),
        calculator::ui::kScientificKeypad.size(),
        g_scientific_buttons, next_id);
    create_key_grid(
        calculator::ui::kProgrammerKeypad.data(),
        calculator::ui::kProgrammerKeypad.size(),
        g_programmer_buttons, next_id);

    update_mode_ui();
}

LRESULT CALLBACK history_proc(HWND window, UINT message,
                              WPARAM wparam, LPARAM lparam) {
    switch (message) {
    case WM_CREATE: {
        apply_nonclient_theme(window);
        g_history_edit = CreateWindowExW(
            0, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE |
                ES_READONLY | ES_AUTOVSCROLL,
            0, 0, 0, 0, window, nullptr, g_instance, nullptr);
        apply_control_theme(g_history_edit);
        apply_font(g_history_edit, g_ui_font);

        HWND clear = create_button(window, kIdHistoryClear, L"Clear History",
                                   g_ui_bold_font);
        (void)clear;
        refresh_history();
        return 0;
    }
    case WM_SIZE: {
        RECT client{};
        GetClientRect(window, &client);
        const int margin = sx(window, 16);
        const int button_height = sx(window, 38);
        MoveWindow(g_history_edit, margin, margin,
                   std::max(0, static_cast<int>(client.right) - margin * 2),
                   std::max(0, static_cast<int>(client.bottom) - margin * 3 - button_height),
                   TRUE);
        HWND clear = GetDlgItem(window, kIdHistoryClear);
        MoveWindow(clear, margin,
                   client.bottom - margin - button_height,
                   sx(window, 140), button_height, TRUE);
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(wparam) == kIdHistoryClear) {
            g_controller.clear_history();
            refresh_history();
            return 0;
        }
        break;
    case WM_CTLCOLORSTATIC: {
        HDC dc = reinterpret_cast<HDC>(wparam);
        SetTextColor(dc, kText);
        SetBkColor(dc, kBackground);
        return reinterpret_cast<LRESULT>(g_background_brush);
    }
    case WM_CTLCOLOREDIT: {
        HDC dc = reinterpret_cast<HDC>(wparam);
        SetTextColor(dc, kText);
        SetBkColor(dc, kInput);
        return reinterpret_cast<LRESULT>(g_input_brush);
    }
    case WM_DRAWITEM:
        if (draw_button(reinterpret_cast<const DRAWITEMSTRUCT*>(lparam))) return TRUE;
        break;
    case WM_ERASEBKGND: {
        HDC dc = reinterpret_cast<HDC>(wparam);
        RECT rect{};
        GetClientRect(window, &rect);
        FillRect(dc, &rect, g_background_brush);
        return 1;
    }
    case WM_CLOSE:
        DestroyWindow(window);
        return 0;
    case WM_DESTROY:
        g_history_window = nullptr;
        g_history_edit = nullptr;
        return 0;
    default:
        break;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

LRESULT CALLBACK main_proc(HWND window, UINT message,
                           WPARAM wparam, LPARAM lparam) {
    switch (message) {
    case WM_CREATE:
        // CreateWindowExW sends WM_CREATE before it returns to wWinMain, so
        // g_main has not yet been assigned there.  Set it here before any
        // child controls are created; the keypad factory uses g_main as its
        // parent.
        g_main = window;
        apply_nonclient_theme(window);
        create_controls(window);
        apply_theme(false);
        return 0;

    case WM_SETTINGCHANGE:
        if (g_theme_mode == ThemeMode::System) {
            apply_theme(false);
        }
        return 0;

    case WM_SIZE:
        layout_main(window);
        InvalidateRect(window, nullptr, TRUE);
        return 0;

    case WM_GETMINMAXINFO: {
        auto* info = reinterpret_cast<MINMAXINFO*>(lparam);
        info->ptMinTrackSize.x = sx(
            window, calculator::ui::kDesktopMetrics.minimum_width);
        info->ptMinTrackSize.y = sx(
            window, calculator::ui::kDesktopMetrics.minimum_height);
        return 0;
    }

    case WM_COMMAND: {
        const int id = LOWORD(wparam);
        if (id == kIdHistory) {
            show_history();
            return 0;
        }
        if (id == kIdTheme) {
            g_theme_mode =
                calculator::ui::next_theme_mode(g_theme_mode);
            apply_theme(true);
            return 0;
        }
        if (id == kIdModeStandard) {
            sync_controller_expression();
            g_controller.set_mode(Mode::Standard);
            update_mode_ui();
            layout_main(window);
            return 0;
        }
        if (id == kIdModeScientific) {
            sync_controller_expression();
            g_controller.set_mode(Mode::Scientific);
            update_mode_ui();
            layout_main(window);
            return 0;
        }
        if (id == kIdModeProgrammer) {
            sync_controller_expression();
            g_controller.set_mode(Mode::Programmer);
            update_mode_ui();
            layout_main(window);
            return 0;
        }

        const auto found = g_key_specs.find(id);
        if (found != g_key_specs.end() && found->second != nullptr) {
            dispatch_command(*found->second);
            InvalidateRect(window, nullptr, FALSE);
            return 0;
        }
        break;
    }

    case WM_DRAWITEM:
        if (draw_button(reinterpret_cast<const DRAWITEMSTRUCT*>(lparam))) return TRUE;
        break;

    case WM_CTLCOLOREDIT: {
        HDC dc = reinterpret_cast<HDC>(wparam);
        HWND control = reinterpret_cast<HWND>(lparam);
        SetTextColor(dc, kMuted);
        if (control == g_history_dock) {
            SetBkColor(dc, kPanel);
            return reinterpret_cast<LRESULT>(g_panel_brush);
        }
        SetBkColor(dc, kInput);
        return reinterpret_cast<LRESULT>(g_input_brush);
    }

    case WM_CTLCOLORSTATIC: {
        HDC dc = reinterpret_cast<HDC>(wparam);
        HWND control = reinterpret_cast<HWND>(lparam);

        SetBkMode(dc, TRANSPARENT);
        if (control == g_title || control == g_result) {
            SetTextColor(dc, kTitle);
        } else if (control == g_status) {
            SetTextColor(dc, g_status_fault ? kFault : kSubtle);
        } else {
            SetTextColor(dc, kSubtle);
        }

        if (control == g_result || control == g_status) {
            SetBkMode(dc, OPAQUE);
            SetBkColor(dc, kPanel);
            return reinterpret_cast<LRESULT>(g_panel_brush);
        }
        return reinterpret_cast<LRESULT>(g_background_brush);
    }

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC dc = BeginPaint(window, &ps);
        RECT client{};
        GetClientRect(window, &client);
        FillRect(dc, &client, g_background_brush);
        draw_panel(dc, g_mode_rect, kSurface, kBorder, 12);
        draw_panel(dc, g_display_rect, kPanel, kBorder, 12);
        EndPaint(window, &ps);
        return 0;
    }

    case WM_SETFOCUS:
        SetFocus(g_expression);
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    default:
        break;
    }

    return DefWindowProcW(window, message, wparam, lparam);
}

void destroy_resources() {
    if (g_ui_font != nullptr) DeleteObject(g_ui_font);
    if (g_ui_bold_font != nullptr) DeleteObject(g_ui_bold_font);
    if (g_title_font != nullptr) DeleteObject(g_title_font);
    if (g_result_font != nullptr) DeleteObject(g_result_font);
    if (g_small_font != nullptr) DeleteObject(g_small_font);
    for (HANDLE handle : g_private_font_handles) {
        if (handle != nullptr) RemoveFontMemResourceEx(handle);
    }
    g_private_font_handles.clear();
    if (g_background_brush != nullptr) DeleteObject(g_background_brush);
    if (g_panel_brush != nullptr) DeleteObject(g_panel_brush);
    if (g_input_brush != nullptr) DeleteObject(g_input_brush);
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show_command) {
    g_instance = instance;

    SetProcessDPIAware();

    if (!register_bundled_fonts()) {
        destroy_resources();
        return 1;
    }

    INITCOMMONCONTROLSEX controls{
        sizeof(INITCOMMONCONTROLSEX),
        ICC_STANDARD_CLASSES
    };
    InitCommonControlsEx(&controls);

    g_theme_mode = load_theme_mode();
    resolve_theme();
    recreate_theme_brushes();

    const wchar_t* ui_family = ui_font_family();
    const wchar_t* brand_family = brand_font_family();
    g_ui_font = make_font(nullptr, 10, FW_NORMAL, ui_family);
    g_ui_bold_font = make_font(nullptr, 10, FW_BOLD, ui_family);
    g_title_font = make_font(nullptr, 18, FW_NORMAL, brand_family);
    g_result_font = make_font(nullptr, 32, FW_NORMAL, brand_family);
    g_small_font = make_font(nullptr, 8, FW_BOLD, ui_family);

    WNDCLASSEXW main_class{};
    main_class.cbSize = sizeof(main_class);
    main_class.style = CS_HREDRAW | CS_VREDRAW;
    main_class.lpfnWndProc = main_proc;
    main_class.hInstance = instance;
    main_class.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    main_class.hIcon = LoadIconW(nullptr, MAKEINTRESOURCEW(32512));
    // Background painting is handled by the window procedures so the brush
    // can be recreated safely when the user switches theme at runtime.
    main_class.hbrBackground = nullptr;
    main_class.lpszClassName = kMainClass;
    main_class.hIconSm = main_class.hIcon;

    WNDCLASSEXW history_class = main_class;
    history_class.lpfnWndProc = history_proc;
    history_class.lpszClassName = kHistoryClass;

    if (RegisterClassExW(&main_class) == 0 ||
        RegisterClassExW(&history_class) == 0) {
        destroy_resources();
        return 1;
    }

    RECT desired{
        0, 0,
        calculator::ui::kDesktopMetrics.default_width,
        calculator::ui::kDesktopMetrics.default_height};
    AdjustWindowRectEx(&desired, WS_OVERLAPPEDWINDOW, FALSE, 0);

    g_main = CreateWindowExW(
        0, kMainClass, L"Calculator",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT,
        desired.right - desired.left, desired.bottom - desired.top,
        nullptr, nullptr, instance, nullptr);

    if (g_main == nullptr) {
        destroy_resources();
        return 1;
    }

    ShowWindow(g_main, show_command == 0 ? SW_SHOWNORMAL : show_command);
    UpdateWindow(g_main);
    SetFocus(g_expression);

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    destroy_resources();
    return static_cast<int>(message.wParam);
}
