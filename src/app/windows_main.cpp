/* SPDX-License-Identifier: GPL-3.0-or-later */
#define WIN32_LEAN_AND_MEAN
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif

#include <windows.h>
#include <commctrl.h>

#include "../core/programmer.hpp"
#include "../core/session.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

constexpr wchar_t kMainClass[] = L"InfiltratorCalcWindow";
constexpr wchar_t kHistoryClass[] = L"InfiltratorCalcHistoryWindow";

constexpr int kIdHistory = 1001;
constexpr int kIdModeStandard = 1010;
constexpr int kIdModeScientific = 1011;
constexpr int kIdModeProgrammer = 1012;
constexpr int kIdKeyBase = 2000;
constexpr int kIdHistoryClear = 3001;

constexpr double kPi = 3.14159265358979323846;

constexpr COLORREF kBackground = RGB(5, 6, 8);
constexpr COLORREF kPanel = RGB(16, 19, 24);
constexpr COLORREF kCard = RGB(23, 27, 32);
constexpr COLORREF kSurface = RGB(13, 16, 20);
constexpr COLORREF kInput = RGB(14, 17, 21);
constexpr COLORREF kBorder = RGB(53, 58, 64);
constexpr COLORREF kText = RGB(232, 236, 239);
constexpr COLORREF kTitle = RGB(238, 241, 243);
constexpr COLORREF kMuted = RGB(174, 182, 189);
constexpr COLORREF kSubtle = RGB(137, 145, 152);
constexpr COLORREF kButtonBackground = RGB(215, 221, 226);
constexpr COLORREF kButtonForeground = RGB(17, 20, 24);
constexpr COLORREF kSelection = RGB(43, 49, 55);
constexpr COLORREF kNeutralAccent = RGB(190, 199, 207);
constexpr COLORREF kWarning = RGB(209, 158, 71);
constexpr COLORREF kFault = RGB(201, 107, 107);
constexpr COLORREF kOperation = RGB(32, 37, 43);

enum class Mode { Standard = 0, Scientific = 1, Programmer = 2 };
enum class ButtonKind { Number, Operation, Utility, Clear, Equals, Mode, Toolbar };

HINSTANCE g_instance = nullptr;
HWND g_main = nullptr;
HWND g_title = nullptr;
HWND g_subtitle = nullptr;
HWND g_history_button = nullptr;
HWND g_expression = nullptr;
HWND g_result = nullptr;
HWND g_status = nullptr;
HWND g_footer = nullptr;
HWND g_mode_buttons[3] = {nullptr, nullptr, nullptr};
HWND g_history_window = nullptr;
HWND g_history_edit = nullptr;

std::vector<HWND> g_standard_buttons;
std::vector<HWND> g_scientific_buttons;
std::vector<HWND> g_programmer_buttons;
std::unordered_map<int, std::wstring> g_key_labels;

HFONT g_ui_font = nullptr;
HFONT g_ui_bold_font = nullptr;
HFONT g_title_font = nullptr;
HFONT g_result_font = nullptr;
HFONT g_small_font = nullptr;

HBRUSH g_background_brush = nullptr;
HBRUSH g_panel_brush = nullptr;
HBRUSH g_input_brush = nullptr;

RECT g_mode_rect{};
RECT g_display_rect{};

WNDPROC g_old_edit_proc = nullptr;

infiltrator::calc::Session g_session;
Mode g_mode = Mode::Standard;
bool g_degrees = true;
bool g_status_fault = false;
infiltrator::calc::ProgrammerBase g_programmer_base =
    infiltrator::calc::ProgrammerBase::Decimal;
infiltrator::calc::IntegerWidth g_programmer_width =
    infiltrator::calc::IntegerWidth::Bits64;
bool g_programmer_signed = false;

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

void apply_dark_nonclient(HWND window) {
    using DwmSetWindowAttributeFn = HRESULT(WINAPI*)(HWND, DWORD, LPCVOID, DWORD);
    HMODULE module = LoadLibraryW(L"dwmapi.dll");
    if (module == nullptr) return;

    auto set_attribute = reinterpret_cast<DwmSetWindowAttributeFn>(
        GetProcAddress(module, "DwmSetWindowAttribute"));
    if (set_attribute != nullptr) {
        BOOL dark = TRUE;
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

void apply_dark_control_theme(HWND control) {
    using SetWindowThemeFn = HRESULT(WINAPI*)(HWND, LPCWSTR, LPCWSTR);
    HMODULE module = LoadLibraryW(L"uxtheme.dll");
    if (module == nullptr) return;
    auto set_theme = reinterpret_cast<SetWindowThemeFn>(
        GetProcAddress(module, "SetWindowTheme"));
    if (set_theme != nullptr) {
        (void)set_theme(control, L"DarkMode_Explorer", nullptr);
    }
    FreeLibrary(module);
}

std::wstring format_value(double value) {
    std::ostringstream out;
    out << std::setprecision(15) << value;
    return utf8_to_wide(out.str());
}

void set_status(const std::wstring& text, bool fault = false) {
    g_status_fault = fault;
    SetWindowTextW(g_status, text.c_str());
    InvalidateRect(g_status, nullptr, TRUE);
}

void set_result(const std::wstring& text) {
    SetWindowTextW(g_result, text.c_str());
}

void set_expression(const std::wstring& text) {
    SetWindowTextW(g_expression, text.c_str());
    SendMessageW(g_expression, EM_SETSEL, static_cast<WPARAM>(-1),
                 static_cast<LPARAM>(-1));
    SetFocus(g_expression);
}

void insert_text(const std::wstring& text) {
    SendMessageW(g_expression, EM_REPLACESEL, TRUE,
                 reinterpret_cast<LPARAM>(text.c_str()));
    SetFocus(g_expression);
}

void backspace() {
    DWORD start = 0;
    DWORD end = 0;
    SendMessageW(g_expression, EM_GETSEL,
                 reinterpret_cast<WPARAM>(&start),
                 reinterpret_cast<LPARAM>(&end));
    if (start == end && start > 0) {
        SendMessageW(g_expression, EM_SETSEL, start - 1U, start);
    }
    SendMessageW(g_expression, EM_REPLACESEL, TRUE,
                 reinterpret_cast<LPARAM>(L""));
    SetFocus(g_expression);
}

bool current_value(double& value) {
    const std::string expression = wide_to_utf8(window_text(g_expression));
    const auto result = g_session.evaluate(expression);
    if (!result.ok) {
        set_result(L"Error: " + utf8_to_wide(result.error));
        set_status(L"CALCULATION ERROR", true);
        return false;
    }
    value = result.value;
    return true;
}

bool current_programmer_value(std::uint64_t& value) {
    const auto result = infiltrator::calc::evaluate_programmer(
        wide_to_utf8(window_text(g_expression)),
        g_programmer_base, g_programmer_width);
    if (!result.ok) {
        set_result(L"Error: " + utf8_to_wide(result.error));
        set_status(L"PROGRAMMER ERROR", true);
        return false;
    }
    value = result.value;
    return true;
}

std::wstring programmer_base_name() {
    switch (g_programmer_base) {
    case infiltrator::calc::ProgrammerBase::Binary: return L"BIN";
    case infiltrator::calc::ProgrammerBase::Octal: return L"OCT";
    case infiltrator::calc::ProgrammerBase::Decimal: return L"DEC";
    case infiltrator::calc::ProgrammerBase::Hexadecimal: return L"HEX";
    }
    return L"DEC";
}

std::wstring programmer_status_text() {
    std::wostringstream out;
    out << L"PROGRAMMER · " << programmer_base_name()
        << L" · " << static_cast<unsigned>(g_programmer_width) << L" BIT · "
        << (g_programmer_signed ? L"SIGNED" : L"UNSIGNED");
    return out.str();
}

void calculate_programmer() {
    std::uint64_t value = 0;
    if (!current_programmer_value(value)) return;
    set_result(utf8_to_wide(infiltrator::calc::format_programmer(
        value, g_programmer_base, g_programmer_width, g_programmer_signed)));
    set_status(programmer_status_text());
}

void calculate() {
    if (g_mode == Mode::Programmer) {
        calculate_programmer();
        return;
    }

    double value = 0.0;
    if (!current_value(value)) return;
    set_result(format_value(value));

    if (g_mode == Mode::Scientific) {
        set_status(g_degrees ? L"SCIENTIFIC · DEGREES"
                             : L"SCIENTIFIC · RADIANS");
    } else {
        set_status(L"READY");
    }
}

void clear_calculation() {
    set_expression(L"");
    set_result(L"0");
    if (g_mode == Mode::Programmer) {
        set_status(programmer_status_text());
    } else if (g_mode == Mode::Scientific) {
        set_status(g_degrees ? L"SCIENTIFIC · DEGREES"
                             : L"SCIENTIFIC · RADIANS");
    } else {
        set_status(L"READY");
    }
}

void unary_transform(const std::wstring& op) {
    double value = 0.0;
    if (!current_value(value)) return;

    if (op == L"±") {
        value = -value;
    } else if (op == L"x²") {
        value *= value;
    } else if (op == L"√") {
        if (value < 0.0) {
            set_status(L"DOMAIN ERROR", true);
            return;
        }
        value = std::sqrt(value);
    } else if (op == L"1/x") {
        if (value == 0.0) {
            set_status(L"DIVISION BY ZERO", true);
            return;
        }
        value = 1.0 / value;
    }

    const std::wstring formatted = format_value(value);
    set_expression(formatted);
    set_result(formatted);
    set_status(L"READY");
}

void scientific_transform(const std::wstring& name) {
    double value = 0.0;
    if (!current_value(value)) return;

    double argument = value;
    if (g_degrees && (name == L"sin" || name == L"cos" || name == L"tan")) {
        argument = value * kPi / 180.0;
    }

    if (name == L"sin") value = std::sin(argument);
    else if (name == L"cos") value = std::cos(argument);
    else if (name == L"tan") value = std::tan(argument);
    else if (name == L"asin") {
        value = std::asin(value);
        if (g_degrees) value = value * 180.0 / kPi;
    } else if (name == L"acos") {
        value = std::acos(value);
        if (g_degrees) value = value * 180.0 / kPi;
    } else if (name == L"atan") {
        value = std::atan(value);
        if (g_degrees) value = value * 180.0 / kPi;
    } else if (name == L"ln") value = std::log(value);
    else if (name == L"log") value = std::log10(value);
    else if (name == L"exp") value = std::exp(value);
    else if (name == L"abs") value = std::fabs(value);

    if (!std::isfinite(value)) {
        set_status(L"DOMAIN ERROR", true);
        return;
    }

    const std::wstring formatted = format_value(value);
    set_expression(formatted);
    set_result(formatted);
    set_status(g_degrees ? L"SCIENTIFIC · DEGREES"
                         : L"SCIENTIFIC · RADIANS");
}

std::wstring insertion_for_label(const std::wstring& label) {
    if (label == L"×") return L"*";
    if (label == L"÷") return L"/";
    if (label == L"−") return L"-";
    return label;
}

void programmer_mode_change(const std::wstring& label) {
    if (label == L"BIN") g_programmer_base = infiltrator::calc::ProgrammerBase::Binary;
    else if (label == L"OCT") g_programmer_base = infiltrator::calc::ProgrammerBase::Octal;
    else if (label == L"DEC") g_programmer_base = infiltrator::calc::ProgrammerBase::Decimal;
    else if (label == L"HEX") g_programmer_base = infiltrator::calc::ProgrammerBase::Hexadecimal;
    else if (label == L"W8") g_programmer_width = infiltrator::calc::IntegerWidth::Bits8;
    else if (label == L"W16") g_programmer_width = infiltrator::calc::IntegerWidth::Bits16;
    else if (label == L"W32") g_programmer_width = infiltrator::calc::IntegerWidth::Bits32;
    else if (label == L"W64") g_programmer_width = infiltrator::calc::IntegerWidth::Bits64;
    else if (label == L"U/S") g_programmer_signed = !g_programmer_signed;
    else return;

    if (!window_text(g_expression).empty()) {
        calculate_programmer();
    } else {
        set_status(programmer_status_text());
    }
    InvalidateRect(g_main, nullptr, TRUE);
}

std::wstring history_text() {
    if (g_session.history().empty()) {
        return L"No calculations yet.";
    }

    std::wstring text;
    std::size_t shown = 0;
    for (auto it = g_session.history().rbegin();
         it != g_session.history().rend() && shown < 50U;
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
    if (g_history_edit != nullptr) {
        const std::wstring text = history_text();
        SetWindowTextW(g_history_edit, text.c_str());
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

void handle_key(const std::wstring& label) {
    if (g_mode == Mode::Programmer) {
        if (label == L"BIN" || label == L"OCT" || label == L"DEC" ||
            label == L"HEX" || label == L"W8" || label == L"W16" ||
            label == L"W32" || label == L"W64" || label == L"U/S") {
            programmer_mode_change(label);
            return;
        }
        if (label == L"=") {
            calculate_programmer();
            return;
        }
        if (label == L"AC") {
            clear_calculation();
            return;
        }
        if (label == L"⌫") {
            backspace();
            return;
        }
        insert_text(insertion_for_label(label));
        return;
    }

    if (label == L"DEG" || label == L"RAD") {
        g_degrees = !g_degrees;
        for (HWND button : g_scientific_buttons) {
            if (window_text(button) == L"DEG" || window_text(button) == L"RAD") {
                SetWindowTextW(button, g_degrees ? L"DEG" : L"RAD");
                break;
            }
        }
        set_status(g_degrees ? L"SCIENTIFIC · DEGREES"
                             : L"SCIENTIFIC · RADIANS");
        return;
    }
    if (label == L"=") {
        calculate();
        return;
    }
    if (label == L"C") {
        clear_calculation();
        return;
    }
    if (label == L"⌫") {
        backspace();
        return;
    }
    if (label == L"±" || label == L"x²" || label == L"√" || label == L"1/x") {
        unary_transform(label);
        return;
    }
    if (label == L"MC") {
        g_session.memory_clear();
        set_status(L"MEMORY CLEARED");
        return;
    }
    if (label == L"MR") {
        insert_text(format_value(g_session.memory_recall()));
        set_status(L"MEMORY RECALL");
        return;
    }
    if (label == L"M+" || label == L"M−") {
        double value = 0.0;
        if (current_value(value)) {
            if (label == L"M+") g_session.memory_add(value);
            else g_session.memory_subtract(value);
            set_status(L"MEMORY UPDATED");
        }
        return;
    }
    if (label == L"sin" || label == L"cos" || label == L"tan" ||
        label == L"asin" || label == L"acos" || label == L"atan" ||
        label == L"ln" || label == L"log" || label == L"exp" ||
        label == L"abs") {
        scientific_transform(label);
        return;
    }
    if (label == L"π") {
        insert_text(L"pi");
        return;
    }
    if (label == L"e") {
        insert_text(L"e");
        return;
    }
    if (label == L"x!") {
        insert_text(L"!");
        return;
    }
    if (label == L"∛") {
        insert_text(L"cbrt(");
        return;
    }

    insert_text(insertion_for_label(label));
}

bool is_number_label(const std::wstring& label) {
    if (label.size() != 1U) return false;
    const wchar_t ch = label[0];
    return (ch >= L'0' && ch <= L'9') ||
           (ch >= L'A' && ch <= L'F') ||
           ch == L'.';
}

ButtonKind button_kind(int id, const std::wstring& label) {
    if (id == kIdHistory) return ButtonKind::Toolbar;
    if (id == kIdModeStandard || id == kIdModeScientific ||
        id == kIdModeProgrammer) return ButtonKind::Mode;
    if (label == L"=") return ButtonKind::Equals;
    if (label == L"C" || label == L"AC" || label == L"⌫")
        return ButtonKind::Clear;
    if (label == L"MC" || label == L"MR" || label == L"M+" ||
        label == L"M−" || label == L"DEG" || label == L"RAD" ||
        label == L"BIN" || label == L"OCT" || label == L"DEC" ||
        label == L"HEX" || label == L"W8" || label == L"W16" ||
        label == L"W32" || label == L"W64" || label == L"U/S")
        return ButtonKind::Utility;
    if (is_number_label(label)) return ButtonKind::Number;
    return ButtonKind::Operation;
}

bool is_selected_button(int id, const std::wstring& label) {
    if (id == kIdModeStandard) return g_mode == Mode::Standard;
    if (id == kIdModeScientific) return g_mode == Mode::Scientific;
    if (id == kIdModeProgrammer) return g_mode == Mode::Programmer;

    if (label == L"BIN") return g_programmer_base == infiltrator::calc::ProgrammerBase::Binary;
    if (label == L"OCT") return g_programmer_base == infiltrator::calc::ProgrammerBase::Octal;
    if (label == L"DEC") return g_programmer_base == infiltrator::calc::ProgrammerBase::Decimal;
    if (label == L"HEX") return g_programmer_base == infiltrator::calc::ProgrammerBase::Hexadecimal;
    if (label == L"W8") return g_programmer_width == infiltrator::calc::IntegerWidth::Bits8;
    if (label == L"W16") return g_programmer_width == infiltrator::calc::IntegerWidth::Bits16;
    if (label == L"W32") return g_programmer_width == infiltrator::calc::IntegerWidth::Bits32;
    if (label == L"W64") return g_programmer_width == infiltrator::calc::IntegerWidth::Bits64;
    if (label == L"U/S") return g_programmer_signed;
    return false;
}

LRESULT draw_button(const DRAWITEMSTRUCT* item) {
    if (item == nullptr || item->CtlType != ODT_BUTTON) return FALSE;

    const int id = static_cast<int>(item->CtlID);
    const std::wstring label = window_text(item->hwndItem);
    const ButtonKind kind = button_kind(id, label);
    const bool selected = is_selected_button(id, label);
    const bool pressed = (item->itemState & ODS_SELECTED) != 0U;
    const bool disabled = (item->itemState & ODS_DISABLED) != 0U;

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
        fill = selected ? kSelection : kSurface;
        text = selected ? kTitle : kMuted;
        border = selected ? kNeutralAccent : kBorder;
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

    if (pressed && kind != ButtonKind::Equals && !selected) {
        fill = kSelection;
    }
    if (disabled) {
        fill = kInput;
        text = kSubtle;
    }

    RECT rect = item->rcItem;
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

HWND create_button(HWND parent, int id, const wchar_t* label, HFONT font) {
    HWND button = CreateWindowExW(
        0, L"BUTTON", label,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
        0, 0, 0, 0, parent,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        g_instance, nullptr);
    apply_font(button, font);
    apply_dark_control_theme(button);
    return button;
}

void create_key_grid(const wchar_t* const* labels, int rows,
                     std::vector<HWND>& destination, int& next_id) {
    destination.clear();
    destination.reserve(static_cast<std::size_t>(rows * 4));
    for (int i = 0; i < rows * 4; ++i) {
        const int id = next_id++;
        g_key_labels.emplace(id, labels[i]);
        destination.push_back(create_button(g_main, id, labels[i], g_ui_bold_font));
    }
}

void show_grid(std::vector<HWND>& buttons, bool visible) {
    for (HWND button : buttons) {
        ShowWindow(button, visible ? SW_SHOW : SW_HIDE);
    }
}

void update_mode_ui() {
    show_grid(g_standard_buttons, g_mode == Mode::Standard);
    show_grid(g_scientific_buttons, g_mode == Mode::Scientific);
    show_grid(g_programmer_buttons, g_mode == Mode::Programmer);

    if (g_mode == Mode::Scientific) {
        set_status(g_degrees ? L"SCIENTIFIC · DEGREES"
                             : L"SCIENTIFIC · RADIANS");
    } else if (g_mode == Mode::Programmer) {
        set_status(programmer_status_text());
    } else {
        set_status(L"READY");
    }

    InvalidateRect(g_main, nullptr, TRUE);
    SetFocus(g_expression);
}

void layout_grid(const std::vector<HWND>& buttons, int rows,
                 int left, int top, int width, int height) {
    if (buttons.empty() || rows <= 0) return;

    const int gap = sx(g_main, 10);
    const int columns = 4;
    const int button_width = std::max(sx(g_main, 54),
                                      (width - gap * (columns - 1)) / columns);
    const int button_height = std::max(sx(g_main, 34),
                                       (height - gap * (rows - 1)) / rows);

    for (std::size_t index = 0; index < buttons.size(); ++index) {
        const int row = static_cast<int>(index) / columns;
        const int column = static_cast<int>(index) % columns;
        MoveWindow(buttons[index],
                   left + column * (button_width + gap),
                   top + row * (button_height + gap),
                   button_width, button_height, TRUE);
    }
}

void layout_main(HWND window) {
    RECT client{};
    GetClientRect(window, &client);

    const int margin = sx(window, 20);
    const int gap = sx(window, 10);
    const int width = client.right - client.left;
    const int height = client.bottom - client.top;
    const int content_width = std::max(0, width - margin * 2);

    int y = margin;
    const int header_height = sx(window, 58);
    const int history_width = sx(window, 96);

    MoveWindow(g_title, margin, y, std::max(0, content_width - history_width - gap),
               sx(window, 34), TRUE);
    MoveWindow(g_subtitle, margin, y + sx(window, 34),
               std::max(0, content_width - history_width - gap),
               sx(window, 18), TRUE);
    MoveWindow(g_history_button,
               width - margin - history_width, y + sx(window, 8),
               history_width, sx(window, 38), TRUE);

    y += header_height + sx(window, 8);

    g_mode_rect = RECT{margin, y, width - margin, y + sx(window, 48)};
    const int mode_padding = sx(window, 5);
    const int mode_gap = sx(window, 5);
    const int mode_width =
        (content_width - mode_padding * 2 - mode_gap * 2) / 3;
    for (int i = 0; i < 3; ++i) {
        MoveWindow(g_mode_buttons[i],
                   margin + mode_padding + i * (mode_width + mode_gap),
                   y + mode_padding, mode_width, sx(window, 38), TRUE);
    }

    y += sx(window, 62);

    g_display_rect = RECT{margin, y, width - margin, y + sx(window, 146)};
    const int display_padding = sx(window, 16);
    MoveWindow(g_expression, margin + display_padding, y + display_padding,
               content_width - display_padding * 2, sx(window, 36), TRUE);
    MoveWindow(g_result, margin + display_padding, y + sx(window, 58),
               content_width - display_padding * 2, sx(window, 54), TRUE);
    MoveWindow(g_status, margin + display_padding, y + sx(window, 116),
               content_width - display_padding * 2, sx(window, 18), TRUE);

    y += sx(window, 160);

    const int footer_height = sx(window, 18);
    const int footer_y = height - margin - footer_height;
    const int grid_height = std::max(sx(window, 260), footer_y - y - sx(window, 8));
    const int rows = g_mode == Mode::Standard ? 7 : 10;

    if (g_mode == Mode::Standard) {
        layout_grid(g_standard_buttons, rows, margin, y, content_width, grid_height);
    } else if (g_mode == Mode::Scientific) {
        layout_grid(g_scientific_buttons, rows, margin, y, content_width, grid_height);
    } else {
        layout_grid(g_programmer_buttons, rows, margin, y, content_width, grid_height);
    }

    MoveWindow(g_footer, margin, footer_y, content_width, footer_height, TRUE);
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

void create_controls(HWND window) {
    g_title = CreateWindowExW(0, L"STATIC", L"Infiltrator Calc",
                              WS_CHILD | WS_VISIBLE | SS_LEFT,
                              0, 0, 0, 0, window, nullptr, g_instance, nullptr);
    g_subtitle = CreateWindowExW(0, L"STATIC", L"PRECISION DESKTOP CALCULATOR",
                                 WS_CHILD | WS_VISIBLE | SS_LEFT,
                                 0, 0, 0, 0, window, nullptr, g_instance, nullptr);
    g_history_button = create_button(window, kIdHistory, L"History", g_ui_bold_font);

    g_mode_buttons[0] = create_button(window, kIdModeStandard, L"Standard", g_ui_bold_font);
    g_mode_buttons[1] = create_button(window, kIdModeScientific, L"Scientific", g_ui_bold_font);
    g_mode_buttons[2] = create_button(window, kIdModeProgrammer, L"Programmer", g_ui_bold_font);

    g_expression = CreateWindowExW(
        0, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_RIGHT | ES_AUTOHSCROLL,
        0, 0, 0, 0, window, nullptr, g_instance, nullptr);
    apply_dark_control_theme(g_expression);

    g_result = CreateWindowExW(0, L"STATIC", L"0",
                               WS_CHILD | WS_VISIBLE | SS_RIGHT | SS_NOPREFIX,
                               0, 0, 0, 0, window, nullptr, g_instance, nullptr);
    g_status = CreateWindowExW(0, L"STATIC", L"READY",
                               WS_CHILD | WS_VISIBLE | SS_RIGHT | SS_NOPREFIX,
                               0, 0, 0, 0, window, nullptr, g_instance, nullptr);
    g_footer = CreateWindowExW(
        0, L"STATIC",
        L"Keyboard ready · Variables, memory and history retained",
        WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX,
        0, 0, 0, 0, window, nullptr, g_instance, nullptr);

    apply_font(g_title, g_title_font);
    apply_font(g_subtitle, g_small_font);
    apply_font(g_expression, g_ui_font);
    apply_font(g_result, g_result_font);
    apply_font(g_status, g_small_font);
    apply_font(g_footer, g_small_font);

    g_old_edit_proc = reinterpret_cast<WNDPROC>(
        SetWindowLongPtrW(g_expression, GWLP_WNDPROC,
                          reinterpret_cast<LONG_PTR>(
                              +[](HWND edit, UINT message, WPARAM wparam, LPARAM lparam) -> LRESULT {
                                  if (message == WM_KEYDOWN && wparam == VK_RETURN) {
                                      calculate();
                                      return 0;
                                  }
                                  return CallWindowProcW(g_old_edit_proc, edit, message, wparam, lparam);
                              })));

    const wchar_t* standard_keys[] = {
        L"MC", L"MR", L"M+", L"M−",
        L"C", L"⌫", L"%", L"÷",
        L"1/x", L"x²", L"√", L"^",
        L"7", L"8", L"9", L"×",
        L"4", L"5", L"6", L"−",
        L"1", L"2", L"3", L"+",
        L"±", L"0", L".", L"="
    };
    const wchar_t* scientific_keys[] = {
        L"DEG", L"π", L"e", L"C",
        L"sin", L"cos", L"tan", L"⌫",
        L"asin", L"acos", L"atan", L"^",
        L"ln", L"log", L"exp", L"x!",
        L"√", L"∛", L"abs", L"%",
        L"7", L"8", L"9", L"÷",
        L"4", L"5", L"6", L"×",
        L"1", L"2", L"3", L"−",
        L"(", L"0", L")", L"+",
        L"±", L".", L"1/x", L"="
    };
    const wchar_t* programmer_keys[] = {
        L"BIN", L"OCT", L"DEC", L"HEX",
        L"W8", L"W16", L"W32", L"W64",
        L"U/S", L"~", L"&", L"|",
        L"^", L"<<", L">>", L"AC",
        L"(", L")", L"÷", L"×",
        L"7", L"8", L"9", L"−",
        L"4", L"5", L"6", L"+",
        L"1", L"2", L"3", L"=",
        L"0", L"A", L"B", L"⌫",
        L"C", L"D", L"E", L"F"
    };

    int next_id = kIdKeyBase;
    create_key_grid(standard_keys, 7, g_standard_buttons, next_id);
    create_key_grid(scientific_keys, 10, g_scientific_buttons, next_id);
    create_key_grid(programmer_keys, 10, g_programmer_buttons, next_id);

    update_mode_ui();
}

LRESULT CALLBACK history_proc(HWND window, UINT message,
                              WPARAM wparam, LPARAM lparam) {
    switch (message) {
    case WM_CREATE: {
        apply_dark_nonclient(window);
        g_history_edit = CreateWindowExW(
            0, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE |
                ES_READONLY | ES_AUTOVSCROLL,
            0, 0, 0, 0, window, nullptr, g_instance, nullptr);
        apply_dark_control_theme(g_history_edit);
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
                   std::max(0, client.right - margin * 2),
                   std::max(0, client.bottom - margin * 3 - button_height),
                   TRUE);
        HWND clear = GetDlgItem(window, kIdHistoryClear);
        MoveWindow(clear, margin,
                   client.bottom - margin - button_height,
                   sx(window, 140), button_height, TRUE);
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(wparam) == kIdHistoryClear) {
            g_session.clear_history();
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
        apply_dark_nonclient(window);
        create_controls(window);
        return 0;

    case WM_SIZE:
        layout_main(window);
        InvalidateRect(window, nullptr, TRUE);
        return 0;

    case WM_GETMINMAXINFO: {
        auto* info = reinterpret_cast<MINMAXINFO*>(lparam);
        info->ptMinTrackSize.x = sx(window, 420);
        info->ptMinTrackSize.y = sx(window, 700);
        return 0;
    }

    case WM_COMMAND: {
        const int id = LOWORD(wparam);
        if (id == kIdHistory) {
            show_history();
            return 0;
        }
        if (id == kIdModeStandard) {
            g_mode = Mode::Standard;
            update_mode_ui();
            layout_main(window);
            return 0;
        }
        if (id == kIdModeScientific) {
            g_mode = Mode::Scientific;
            update_mode_ui();
            layout_main(window);
            return 0;
        }
        if (id == kIdModeProgrammer) {
            g_mode = Mode::Programmer;
            update_mode_ui();
            layout_main(window);
            return 0;
        }

        const auto found = g_key_labels.find(id);
        if (found != g_key_labels.end()) {
            handle_key(found->second);
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
        SetTextColor(dc, kMuted);
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
    if (g_background_brush != nullptr) DeleteObject(g_background_brush);
    if (g_panel_brush != nullptr) DeleteObject(g_panel_brush);
    if (g_input_brush != nullptr) DeleteObject(g_input_brush);
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show_command) {
    g_instance = instance;

    SetProcessDPIAware();

    INITCOMMONCONTROLSEX controls{
        sizeof(INITCOMMONCONTROLSEX),
        ICC_STANDARD_CLASSES
    };
    InitCommonControlsEx(&controls);

    g_background_brush = CreateSolidBrush(kBackground);
    g_panel_brush = CreateSolidBrush(kPanel);
    g_input_brush = CreateSolidBrush(kInput);

    g_ui_font = make_font(nullptr, 11, FW_NORMAL, L"MB Corpo S Title WEB");
    g_ui_bold_font = make_font(nullptr, 11, FW_BOLD, L"MB Corpo S Title WEB");
    g_title_font = make_font(nullptr, 24, FW_NORMAL, L"MB Corpo A Title Cond WEB");
    g_result_font = make_font(nullptr, 34, FW_NORMAL, L"MB Corpo A Title Cond WEB");
    g_small_font = make_font(nullptr, 9, FW_BOLD, L"MB Corpo S Title WEB");

    WNDCLASSEXW main_class{};
    main_class.cbSize = sizeof(main_class);
    main_class.style = CS_HREDRAW | CS_VREDRAW;
    main_class.lpfnWndProc = main_proc;
    main_class.hInstance = instance;
    main_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    main_class.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    main_class.hbrBackground = g_background_brush;
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

    RECT desired{0, 0, 500, 850};
    AdjustWindowRectEx(&desired, WS_OVERLAPPEDWINDOW, FALSE, 0);

    g_main = CreateWindowExW(
        0, kMainClass, L"Infiltrator Calc",
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
