/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "linux_persistence.hpp"

#include <infiltratr/core.h>
#include <infiltratr/posix.h>
#include <glib.h>

#include <algorithm>
#include <cstdlib>
#include <string>
#include <string_view>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>

namespace calculator::app {
namespace {
constexpr char kProductDirectory[] = "calculator";

bool ensure_private_directory(const char* path) {
    return path && *path && infiltratr_mkdir_parents(path, 0700U) == 0;
}

bool write_private_file(const char* path, std::string_view data) {
    return path && infiltratr_atomic_file_write_bytes(
        path, INFILTRATR_ATOMIC_FILE_PRIVATE, data.data(), data.size()) == 0;
}

bool read_text_file(const char* path, std::string& data) {
    if (!path) return false;
    char* contents = nullptr;
    std::size_t length = 0U;
    const InfiltratrIoResult status = infiltratr_read_text_file_alloc(path, &contents, &length);
    if (status == INFILTRATR_IO_EMPTY) { data.clear(); return true; }
    if (status != INFILTRATR_IO_OK) { std::free(contents); return false; }
    data.assign(contents, length);
    std::free(contents);
    return true;
}

ui::ResultFormat result_format_from_name(const char* name) {
    if (!name) return ui::ResultFormat::Automatic;
    if (g_strcmp0(name, "fixed") == 0) return ui::ResultFormat::Fixed;
    if (g_strcmp0(name, "scientific") == 0) return ui::ResultFormat::Scientific;
    if (g_strcmp0(name, "engineering") == 0) return ui::ResultFormat::Engineering;
    return ui::ResultFormat::Automatic;
}
} // namespace

ui::ThemeMode LinuxPersistence::load_theme_mode() const {
    gchar* path = g_build_filename(g_get_user_config_dir(), kProductDirectory, "theme", nullptr);
    ui::ThemeMode mode = ui::ThemeMode::System;
    std::string value;
    if (read_text_file(path, value)) {
        while (!value.empty() && infiltratr_ascii_is_space(static_cast<unsigned char>(value.back()))) value.pop_back();
        ui::ThemeMode parsed = ui::ThemeMode::System;
        if (ui::theme_mode_parse(value.c_str(), parsed)) mode = parsed;
    }
    g_free(path);
    return mode;
}

void LinuxPersistence::save_theme_mode(ui::ThemeMode mode) const {
    gchar* directory = g_build_filename(g_get_user_config_dir(), kProductDirectory, nullptr);
    if (ensure_private_directory(directory)) {
        gchar* path = g_build_filename(directory, "theme", nullptr);
        (void)write_private_file(path, ui::theme_mode_key(mode));
        g_free(path);
    }
    g_free(directory);
}

void LinuxPersistence::load_legacy_variables() {
    gchar* path = g_build_filename(g_get_user_data_dir(), kProductDirectory, "variables", nullptr);
    std::string contents;
    if (read_text_file(path, contents) && !controller_.load_variables_text(contents))
        g_printerr("Calculator ignored malformed variables data at %s\n", path);
    g_free(path);
}

void LinuxPersistence::load_legacy_functions() {
    gchar* path = g_build_filename(g_get_user_data_dir(), kProductDirectory, "custom-functions", nullptr);
    std::string contents;
    if (read_text_file(path, contents) && !controller_.load_function_definitions_text(contents))
        g_printerr("Calculator ignored malformed custom-functions data at %s\n", path);
    g_free(path);
}

void LinuxPersistence::load_legacy_display_preferences() {
    gchar* path = g_build_filename(g_get_user_config_dir(), kProductDirectory, "presentation.ini", nullptr);
    GKeyFile* key = g_key_file_new();
    ui::DisplayPreferences prefs = controller_.display_preferences();
    if (g_key_file_load_from_file(key, path, G_KEY_FILE_NONE, nullptr)) {
        if (g_key_file_has_key(key, "Presentation", "format", nullptr)) {
            gchar* value = g_key_file_get_string(key, "Presentation", "format", nullptr);
            prefs.format = result_format_from_name(value); g_free(value);
        }
        if (g_key_file_has_key(key, "Presentation", "decimal-places", nullptr)) {
            const gint value = g_key_file_get_integer(key, "Presentation", "decimal-places", nullptr);
            prefs.decimal_places = static_cast<unsigned>(std::clamp(value, 0, 15));
        }
        if (g_key_file_has_key(key, "Presentation", "group-thousands", nullptr))
            prefs.group_thousands = g_key_file_get_boolean(key, "Presentation", "group-thousands", nullptr);
        if (g_key_file_has_key(key, "Presentation", "trailing-zeroes", nullptr))
            prefs.trailing_zeroes = g_key_file_get_boolean(key, "Presentation", "trailing-zeroes", nullptr);
    }
    controller_.set_display_preferences(prefs);
    g_key_file_unref(key); g_free(path);
}

bool LinuxPersistence::save_controller_state() const {
    gchar* directory = g_build_filename(g_get_user_data_dir(), kProductDirectory, nullptr);
    if (!ensure_private_directory(directory)) { g_free(directory); return false; }
    gchar* path = g_build_filename(directory, "state-v1", nullptr);
    gchar* lock_path = g_build_filename(directory, "state.lock", nullptr);
    const int lock = ::open(lock_path, O_CREAT|O_RDWR|O_CLOEXEC|O_NOFOLLOW, 0600);
    g_free(lock_path);
    std::string latest, merged;
    bool saved = false;
    // The stable sidecar lock spans read/merge/atomic publication. Locking the
    // document inode itself would fail when Common replaces it by rename.
    if (lock >= 0 && ::flock(lock, LOCK_EX|LOCK_NB) == 0) {
        const bool read = read_text_file(path, latest);
        if (((read && !latest.empty()) || !g_file_test(path, G_FILE_TEST_EXISTS)) &&
            controller_.merge_persistent_state_text(baseline_state_, latest, merged)) {
            saved = write_private_file(path, merged);
            if (saved) baseline_state_ = controller_.persistent_state_text();
        }
        (void)::flock(lock, LOCK_UN);
    }
    if (lock >= 0) ::close(lock);
    if (!saved) g_printerr("Calculator could not save shared state (I/O, busy or concurrent edit conflict): %s\n", path);
    g_free(path);
    g_free(directory);
    return saved;
}

bool LinuxPersistence::load_controller_state() {
    if (controller_state_loaded_) return shared_semantic_state_;
    controller_state_loaded_ = true;
    gchar* path = g_build_filename(g_get_user_data_dir(), kProductDirectory, "state-v1", nullptr);
    std::string contents;
    const bool found = read_text_file(path, contents);
    g_free(path);
    if (found && controller_.load_persistent_state_text(contents)) {
        baseline_state_=controller_.persistent_state_text();
        shared_semantic_state_=contents.rfind("CALCULATOR_STATE 2\n", 0U)==0U;
        return shared_semantic_state_;
    }
    if (found) g_printerr("Calculator ignored malformed shared state document.\n");
    load_legacy_functions(); load_legacy_variables(); load_legacy_display_preferences();
    // With no durable document the baseline is the default state, so migrated
    // names/preferences are treated as local additions during the first save.
    return false;
}

LinuxDesktopState LinuxPersistence::load_desktop_state() const {
    LinuxDesktopState state;
    gchar* path = g_build_filename(g_get_user_config_dir(), kProductDirectory, "desktop.ini", nullptr);
    GKeyFile* key = g_key_file_new();
    if (g_key_file_load_from_file(key, path, G_KEY_FILE_NONE, nullptr)) {
        if (g_key_file_has_key(key, "Window", "width", nullptr)) state.width = g_key_file_get_integer(key, "Window", "width", nullptr);
        if (g_key_file_has_key(key, "Window", "height", nullptr)) state.height = g_key_file_get_integer(key, "Window", "height", nullptr);
        if (g_key_file_has_key(key, "Window", "mode", nullptr)) {
            const gint mode = g_key_file_get_integer(key, "Window", "mode", nullptr);
            if (mode >= static_cast<gint>(ui::Mode::Standard) && mode <= static_cast<gint>(ui::Mode::Programmer)) state.selected_mode = static_cast<ui::Mode>(mode);
        }
        if (g_key_file_has_key(key, "Calculator", "angle-unit", nullptr)) {
            const gint value = g_key_file_get_integer(key, "Calculator", "angle-unit", nullptr);
            if (value >= static_cast<gint>(AngleUnit::Degrees) && value <= static_cast<gint>(AngleUnit::Gradians)) state.angle_code = value;
        }
        if (g_key_file_has_key(key, "Calculator", "programmer-base", nullptr)) {
            const gint value = g_key_file_get_integer(key, "Calculator", "programmer-base", nullptr);
            if (value >= static_cast<gint>(ProgrammerBase::Binary) && value <= static_cast<gint>(ProgrammerBase::Hexadecimal)) state.programmer_base_code = value;
        }
        if (g_key_file_has_key(key, "Calculator", "programmer-width", nullptr)) {
            const gint value = g_key_file_get_integer(key, "Calculator", "programmer-width", nullptr);
            if (value == 8 || value == 16 || value == 32 || value == 64) state.programmer_width_code = value;
        }
        if (g_key_file_has_key(key, "Calculator", "programmer-signed", nullptr)) state.programmer_signed_flag = g_key_file_get_boolean(key, "Calculator", "programmer-signed", nullptr);
    }
    g_key_file_unref(key); g_free(path);
    state.width = std::clamp(state.width, ui::kDesktopMetrics.minimum_width, 2000);
    state.height = std::clamp(state.height, ui::desktop_minimum_height(state.selected_mode), 1600);
    return state;
}

LinuxConversionPreferences LinuxPersistence::load_conversion_preferences() const {
    LinuxConversionPreferences preferences;
    gchar* path = g_build_filename(g_get_user_config_dir(), kProductDirectory, "conversion.ini", nullptr);
    GKeyFile* key = g_key_file_new();
    if (g_key_file_load_from_file(key, path, G_KEY_FILE_NONE, nullptr)) {
        auto load = [&](const char* name, std::string& target) {
            if (!g_key_file_has_key(key, "Conversion", name, nullptr)) return;
            gchar* value = g_key_file_get_string(key, "Conversion", name, nullptr);
            if (value && *value) target = value;
            g_free(value);
        };
        load("dimension", preferences.dimension);
        load("from", preferences.from);
        load("to", preferences.to);
    }
    g_key_file_unref(key);
    g_free(path);
    return preferences;
}

void LinuxPersistence::save_conversion_preferences(const LinuxConversionPreferences& preferences) const {
    gchar* directory = g_build_filename(g_get_user_config_dir(), kProductDirectory, nullptr);
    if (!ensure_private_directory(directory)) { g_free(directory); return; }
    gchar* path = g_build_filename(directory, "conversion.ini", nullptr);
    GKeyFile* key = g_key_file_new();
    g_key_file_set_string(key, "Conversion", "dimension", preferences.dimension.c_str());
    g_key_file_set_string(key, "Conversion", "from", preferences.from.c_str());
    g_key_file_set_string(key, "Conversion", "to", preferences.to.c_str());
    gsize length = 0U;
    gchar* data = g_key_file_to_data(key, &length, nullptr);
    if (data) { (void)write_private_file(path, std::string_view(data, static_cast<std::size_t>(length))); g_free(data); }
    g_key_file_unref(key); g_free(path); g_free(directory);
}

void LinuxPersistence::save_desktop_state(int width, int height) const {
    gchar* directory = g_build_filename(g_get_user_config_dir(), kProductDirectory, nullptr);
    if (!ensure_private_directory(directory)) { g_free(directory); return; }
    gchar* path = g_build_filename(directory, "desktop.ini", nullptr);
    GKeyFile* key = g_key_file_new();
    g_key_file_set_integer(key, "Window", "width", width);
    g_key_file_set_integer(key, "Window", "height", height);
    gsize length = 0U;
    gchar* data = g_key_file_to_data(key, &length, nullptr);
    if (data) { (void)write_private_file(path, std::string_view(data, static_cast<std::size_t>(length))); g_free(data); }
    g_key_file_unref(key); g_free(path); g_free(directory);
}

} // namespace calculator::app
