/* SPDX-License-Identifier: GPL-3.0-or-later */
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif

#include <windows.h>
#include <commctrl.h>

#include "../core/programmer.hpp"
#include "../core/session.hpp"
#include "../ui/calculator_ui_contract.hpp"

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
constexpr COLORREF kCardHover = RGB(34, 39, 45);
constexpr COLORREF kSurfaceHover = RGB(23, 27, 32);
constexpr COLORREF kOperationHover = RGB(43, 49, 55);
constexpr COLORREF kEqualsHover = RGB(238, 241, 243);

using infiltrator::calc::ui::ButtonRole;
using infiltrator::calc::ui::ButtonSpec;
using infiltrator::calc::ui::Command;
using infiltrator::calc::ui::Mode;

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

bool font_family_available(const wchar_t* family) {
    if (family == nullptr || *family == L'\0') return false;

    HDC dc = GetDC(nullptr);
    if (dc == nullptr) return false;

    LOGFONTW query{};
    query.lfCharSet = DEFAULT_CHARSET;
    wcsncpy_s(query.lfFaceName, family, _TRUNCATE);

    bool found = false;
    auto callback = +[](const LOGFONTW*, const TEXTMETRICW*, DWORD, LPARAM data) -> int {
        *reinterpret_cast<bool*>(data) = true;
        return 0;
    };
    EnumFontFamiliesExW(dc, &query, callback,
                        reinterpret_cast<LPARAM>(&found), 0);
    ReleaseDC(nullptr, dc);
    return found;
}

const wchar_t* ui_font_family() {
    return font_family_available(L"MB Corpo S Title WEB")
        ? L"MB Corpo S Title WEB"
        : L"Segoe UI";
}

const wchar_t* brand_font_family() {
    return font_family_available(L"MB Corpo A Title Cond WEB")
        ? L"MB Corpo A Title Cond WEB"
        : L"Segoe UI";
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

void programmer_mode_change(Command command) {
    switch (command) {
    case Command::BaseBin:
        g_programmer_base = infiltrator::calc::ProgrammerBase::Binary;
        break;
    case Command::BaseOct:
        g_programmer_base = infiltrator::calc::ProgrammerBase::Octal;
        break;
    case Command::BaseDec:
        g_programmer_base = infiltrator::calc::ProgrammerBase::Decimal;
        break;
    case Command::BaseHex:
        g_programmer_base = infiltrator::calc::ProgrammerBase::Hexadecimal;
        break;
    case Command::Width8:
        g_programmer_width = infiltrator::calc::IntegerWidth::Bits8;
        break;
    case Command::Width16:
        g_programmer_width = infiltrator::calc::IntegerWidth::Bits16;
        break;
    case Command::Width32:
        g_programmer_width = infiltrator::calc::IntegerWidth::Bits32;
        break;
    case Command::Width64:
        g_programmer_width = infiltrator::calc::IntegerWidth::Bits64;
        break;
    case Command::ToggleSigned:
        g_programmer_signed = !g_programmer_signed;
        break;
    default:
        return;
    }

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

void handle_key(const ButtonSpec& spec) {
    const Command command = spec.command;
    const std::wstring label = utf8_to_wide(std::string(spec.label));

    if (g_mode == Mode::Programmer) {
        if (infiltrator::calc::ui::is_programmer_selector(command)) {
            programmer_mode_change(command);
            return;
        }
        if (command == Command::Equals) {
            calculate_programmer();
            return;
        }
        if (command == Command::AllClear) {
            clear_calculation();
            return;
        }
        if (command == Command::Backspace) {
            backspace();
            return;
        }

        const std::string_view insertion =
            infiltrator::calc::ui::insertion_text(command);
        if (!insertion.empty()) {
            insert_text(utf8_to_wide(std::string(insertion)));
        }
        return;
    }

    switch (command) {
    case Command::ToggleDegrees:
        g_degrees = !g_degrees;
        for (HWND button : g_scientific_buttons) {
            const auto it = std::find_if(
                g_key_specs.begin(), g_key_specs.end(),
                [button](const auto& item) {
                    return GetDlgCtrlID(button) == item.first &&
                           item.second != nullptr &&
                           item.second->command == Command::ToggleDegrees;
                });
            if (it != g_key_specs.end()) {
                SetWindowTextW(button, g_degrees ? L"DEG" : L"RAD");
                break;
            }
        }
        set_status(g_degrees ? L"SCIENTIFIC · DEGREES"
                             : L"SCIENTIFIC · RADIANS");
        return;
    case Command::Equals:
        calculate();
        return;
    case Command::Clear:
        clear_calculation();
        return;
    case Command::Backspace:
        backspace();
        return;
    case Command::Negate:
    case Command::Square:
    case Command::SquareRoot:
    case Command::Reciprocal:
        unary_transform(label);
        return;
    case Command::MemoryClear:
        g_session.memory_clear();
        set_status(L"MEMORY CLEARED");
        return;
    case Command::MemoryRecall:
        insert_text(format_value(g_session.memory_recall()));
        set_status(L"MEMORY RECALL");
        return;
    case Command::MemoryAdd:
    case Command::MemorySubtract: {
        double value = 0.0;
        if (current_value(value)) {
            if (command == Command::MemoryAdd) g_session.memory_add(value);
            else g_session.memory_subtract(value);
            set_status(L"MEMORY UPDATED");
        }
        return;
    }
    case Command::Sin:
    case Command::Cos:
    case Command::Tan:
    case Command::Asin:
    case Command::Acos:
    case Command::Atan:
    case Command::Ln:
    case Command::Log10:
    case Command::Exp:
    case Command::Abs:
        scientific_transform(label);
        return;
    case Command::Pi:
        insert_text(L"pi");
        return;
    case Command::Euler:
        insert_text(L"e");
        return;
    case Command::Factorial:
        insert_text(L"!");
        return;
    case Command::CubeRoot:
        insert_text(L"cbrt(");
        return;
    default:
        break;
    }

    const std::string_view insertion =
        infiltrator::calc::ui::insertion_text(command);
    if (!insertion.empty()) {
        insert_text(utf8_to_wide(std::string(insertion)));
    }
}

ButtonKind button_kind(int id, const ButtonSpec* spec) {
    if (id == kIdHistory) return ButtonKind::Toolbar;
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
    if (id == kIdModeStandard) return g_mode == Mode::Standard;
    if (id == kIdModeScientific) return g_mode == Mode::Scientific;
    if (id == kIdModeProgrammer) return g_mode == Mode::Programmer;
    if (spec == nullptr) return false;

    switch (spec->command) {
    case Command::BaseBin:
        return g_programmer_base == infiltrator::calc::ProgrammerBase::Binary;
    case Command::BaseOct:
        return g_programmer_base == infiltrator::calc::ProgrammerBase::Octal;
    case Command::BaseDec:
        return g_programmer_base == infiltrator::calc::ProgrammerBase::Decimal;
    case Command::BaseHex:
        return g_programmer_base == infiltrator::calc::ProgrammerBase::Hexadecimal;
    case Command::Width8:
        return g_programmer_width == infiltrator::calc::IntegerWidth::Bits8;
    case Command::Width16:
        return g_programmer_width == infiltrator::calc::IntegerWidth::Bits16;
    case Command::Width32:
        return g_programmer_width == infiltrator::calc::IntegerWidth::Bits32;
    case Command::Width64:
        return g_programmer_width == infiltrator::calc::IntegerWidth::Bits64;
    case Command::ToggleSigned:
        return g_programmer_signed;
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
    apply_dark_control_theme(button);
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
    if (g_mode == Mode::Standard) redraw_buttons(g_standard_buttons);
    else if (g_mode == Mode::Scientific) redraw_buttons(g_scientific_buttons);
    else redraw_buttons(g_programmer_buttons);
}

void redraw_mode_buttons() {
    for (HWND button : g_mode_buttons) redraw_button(button);
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

    redraw_mode_buttons();
    redraw_active_grid();
    InvalidateRect(g_main, nullptr, TRUE);
    SetFocus(g_expression);
}

void layout_grid_range(const std::vector<HWND>& buttons,
                       std::size_t start_index, int rows,
                       int left, int top, int width, int height) {
    if (start_index >= buttons.size() || rows <= 0) return;

    const int gap = sx(
        g_main, infiltrator::calc::ui::kDesktopMetrics.grid_gap_x);
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
        g_main, infiltrator::calc::ui::kDesktopMetrics.memory_height);
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

    const int margin = sx(window, 14);
    const int gap = sx(window, 6);
    const int width = client.right - client.left;
    const int height = client.bottom - client.top;
    const int content_width = std::max(0, width - margin * 2);

    int y = margin;

    const int history_width = sx(window, 76);
    const int history_height = sx(window, 30);
    MoveWindow(g_title, margin, y,
               std::max(0, content_width - history_width - gap),
               sx(window, 30), TRUE);
    ShowWindow(g_subtitle, SW_HIDE);
    MoveWindow(g_history_button,
               width - margin - history_width, y,
               history_width, history_height, TRUE);

    y += sx(window, 38);

    g_mode_rect = RECT{margin, y, width - margin, y + sx(window, 38)};
    const int mode_padding = sx(window, 3);
    const int mode_gap = sx(window, 3);
    const int mode_width =
        (content_width - mode_padding * 2 - mode_gap * 2) / 3;
    for (int i = 0; i < 3; ++i) {
        MoveWindow(g_mode_buttons[i],
                   margin + mode_padding + i * (mode_width + mode_gap),
                   y + mode_padding, mode_width, sx(window, 32), TRUE);
    }

    y += sx(window, 46);

    g_display_rect = RECT{margin, y, width - margin, y + sx(window, 104)};
    const int display_padding = sx(window, 12);
    MoveWindow(g_expression,
               margin + display_padding, y + sx(window, 8),
               content_width - display_padding * 2, sx(window, 24), TRUE);
    MoveWindow(g_result,
               margin + display_padding, y + sx(window, 32),
               content_width - display_padding * 2, sx(window, 46), TRUE);
    MoveWindow(g_status,
               margin + display_padding, y + sx(window, 80),
               content_width - display_padding * 2, sx(window, 16), TRUE);

    y += sx(window, 112);

    const int grid_bottom = height - margin;
    const int grid_height = std::max(sx(window, 300), grid_bottom - y);
    if (g_mode == Mode::Standard) {
        layout_standard_grid(margin, y, content_width, grid_height);
    } else if (g_mode == Mode::Scientific) {
        layout_grid(g_scientific_buttons, 10, margin, y, content_width, grid_height);
    } else {
        layout_grid(g_programmer_buttons, 10, margin, y, content_width, grid_height);
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

void create_controls(HWND window) {
    g_title = CreateWindowExW(0, L"STATIC", L"Infiltrator Calc",
                              WS_CHILD | WS_VISIBLE | SS_LEFT,
                              0, 0, 0, 0, window, nullptr, g_instance, nullptr);
    g_subtitle = CreateWindowExW(0, L"STATIC", L"PRECISION DESKTOP CALCULATOR",
                                 WS_CHILD | SS_LEFT,
                                 0, 0, 0, 0, window, nullptr, g_instance, nullptr);
    g_history_button = create_button(window, kIdHistory, L"History", g_ui_bold_font);

    const std::wstring standard_mode =
        utf8_to_wide(std::string(infiltrator::calc::ui::mode_name(Mode::Standard)));
    const std::wstring scientific_mode =
        utf8_to_wide(std::string(infiltrator::calc::ui::mode_name(Mode::Scientific)));
    const std::wstring programmer_mode =
        utf8_to_wide(std::string(infiltrator::calc::ui::mode_name(Mode::Programmer)));
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
        WS_CHILD | SS_LEFT | SS_NOPREFIX,
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

    int next_id = kIdKeyBase;

    g_standard_buttons.clear();
    g_standard_buttons.reserve(
        infiltrator::calc::ui::kStandardMemory.size() +
        infiltrator::calc::ui::kStandardKeypad.size());
    for (const ButtonSpec& spec : infiltrator::calc::ui::kStandardMemory) {
        const int id = next_id++;
        const std::wstring label = utf8_to_wide(std::string(spec.label));
        g_key_specs.emplace(id, &spec);
        g_standard_buttons.push_back(
            create_button(g_main, id, label.c_str(), g_ui_bold_font));
    }
    for (const ButtonSpec& spec : infiltrator::calc::ui::kStandardKeypad) {
        const int id = next_id++;
        const std::wstring label = utf8_to_wide(std::string(spec.label));
        g_key_specs.emplace(id, &spec);
        g_standard_buttons.push_back(
            create_button(g_main, id, label.c_str(), g_ui_bold_font));
    }

    create_key_grid(
        infiltrator::calc::ui::kScientificKeypad.data(),
        infiltrator::calc::ui::kScientificKeypad.size(),
        g_scientific_buttons, next_id);
    create_key_grid(
        infiltrator::calc::ui::kProgrammerKeypad.data(),
        infiltrator::calc::ui::kProgrammerKeypad.size(),
        g_programmer_buttons, next_id);

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
        // CreateWindowExW sends WM_CREATE before it returns to wWinMain, so
        // g_main has not yet been assigned there.  Set it here before any
        // child controls are created; the keypad factory uses g_main as its
        // parent.
        g_main = window;
        apply_dark_nonclient(window);
        create_controls(window);
        return 0;

    case WM_SIZE:
        layout_main(window);
        InvalidateRect(window, nullptr, TRUE);
        return 0;

    case WM_GETMINMAXINFO: {
        auto* info = reinterpret_cast<MINMAXINFO*>(lparam);
        info->ptMinTrackSize.x = sx(
            window, infiltrator::calc::ui::kDesktopMetrics.minimum_width);
        info->ptMinTrackSize.y = sx(
            window, infiltrator::calc::ui::kDesktopMetrics.minimum_height);
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

        const auto found = g_key_specs.find(id);
        if (found != g_key_specs.end() && found->second != nullptr) {
            handle_key(*found->second);
            redraw_active_grid();
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

    const wchar_t* ui_family = ui_font_family();
    const wchar_t* brand_family = brand_font_family();
    g_ui_font = make_font(nullptr, 10, FW_NORMAL, ui_family);
    g_ui_bold_font = make_font(nullptr, 10, FW_SEMIBOLD, ui_family);
    g_title_font = make_font(nullptr, 18, FW_NORMAL, brand_family);
    g_result_font = make_font(nullptr, 32, FW_NORMAL, brand_family);
    g_small_font = make_font(nullptr, 8, FW_SEMIBOLD, ui_family);

    WNDCLASSEXW main_class{};
    main_class.cbSize = sizeof(main_class);
    main_class.style = CS_HREDRAW | CS_VREDRAW;
    main_class.lpfnWndProc = main_proc;
    main_class.hInstance = instance;
    main_class.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    main_class.hIcon = LoadIconW(nullptr, MAKEINTRESOURCEW(32512));
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

    RECT desired{
        0, 0,
        infiltrator::calc::ui::kDesktopMetrics.default_width,
        infiltrator::calc::ui::kDesktopMetrics.default_height};
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
