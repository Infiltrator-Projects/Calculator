/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../src/app/linux_persistence.hpp"
#include "../src/app/cli_mode.hpp"
#include <infiltratr/posix.h>
#include <glib.h>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace {
int failures=0;
void check(bool value,const char* label) {
    if(!value){std::cerr<<"FAIL: "<<label<<'\n';++failures;}
}
int cli(std::string expression) {
    char* arguments[]{expression.data()};
    return calculator::app::run_cli_mode(1,arguments);
}
}

int main() {
    using calculator::app::LinuxPersistence;
    using calculator::ui::Controller;
    gchar* temporary=g_dir_make_tmp("calculator-state-test-XXXXXX",nullptr);
    if(!temporary) return 1;
    const std::filesystem::path root(temporary);
    g_free(temporary);
    g_setenv("XDG_DATA_HOME",root.c_str(),TRUE);
    g_setenv("XDG_CONFIG_HOME",root.c_str(),TRUE);
    struct Cleanup {
        std::filesystem::path root;
        ~Cleanup(){std::error_code error;std::filesystem::remove_all(root,error);}
    } cleanup{root};

    Controller seed;
    LinuxPersistence seed_store(seed);
    check(!seed_store.load_controller_state(),"fresh state is absent");
    seed.set_mode(calculator::ui::Mode::Programmer);
    seed.set_scientific_digits(250);
    check(seed.load_function_definitions_text("savedfunc(x)=x+1\n"),"seed shared function");
    check(seed.load_variables_text("savedvar=7\n"),"seed shared variable");
    check(seed_store.save_controller_state(),"write canonical GUI state");

    std::ostringstream output;
    auto* previous=std::cout.rdbuf(output.rdbuf());
    check(cli("savedfunc(savedvar)")==0,"CLI reads GUI names");
    check(output.str()=="8\n","CLI evaluates shared function and variable");
    output.str("");
    check(cli("clifunc(x)=x*3")==0,"CLI persists its function");
    std::cout.rdbuf(previous);
    Controller from_cli;
    LinuxPersistence from_cli_store(from_cli);
    check(from_cli_store.load_controller_state(),"GUI reads CLI document");
    check(from_cli.function_definitions_text().find("clifunc(x)=x*3")!=std::string::npos,
          "CLI function reaches GUI authority");
    check(from_cli.state().mode==calculator::ui::Mode::Programmer &&
          from_cli.scientific_digits()==250U,"CLI preserves GUI preferences");
    check(!std::filesystem::exists(root/"calculator/custom-functions"),
          "CLI does not create a competing legacy authority");

    Controller a,b;
    LinuxPersistence store_a(a),store_b(b);
    check(store_a.load_controller_state() && store_b.load_controller_state(),"concurrent readers");
    check(a.load_function_definitions_text(a.function_definitions_text()+"other(x)=x+2\n"),"concurrent named edit");
    check(store_a.save_controller_state(),"save first instance");
    b.set_scientific_digits(500);
    check(store_b.save_controller_state(),"merge unrelated second edit");
    check(store_b.save_controller_state(),"repeated save preserves unseen concurrent names");
    Controller merged;
    LinuxPersistence merged_store(merged);
    check(merged_store.load_controller_state(),"load merged document");
    check(merged.scientific_digits()==500U &&
          merged.function_definitions_text().find("other(x)=x+2")!=std::string::npos,
          "both concurrent changes survive");

    Controller c,d;
    LinuxPersistence store_c(c),store_d(d);
    check(store_c.load_controller_state() && store_d.load_controller_state(),"conflict readers");
    c.set_scientific_digits(100);
    d.set_scientific_digits(200);
    check(store_c.save_controller_state(),"first conflicting edit wins publication");
    check(!store_d.save_controller_state(),"conflict refuses to overwrite newer settings");

    Controller e,f;
    LinuxPersistence store_e(e),store_f(f);
    check(store_e.load_controller_state() && store_f.load_controller_state(),"name conflict readers");
    check(e.load_function_definitions_text(e.function_definitions_text()+"conflict(x)=1\n"),"first name edit");
    check(f.load_function_definitions_text(f.function_definitions_text()+"conflict(x)=2\n"),"second name edit");
    check(store_e.save_controller_state(),"publish first name edit");
    check(!store_f.save_controller_state(),"same-name conflict is transactional");

    const std::string path=(root/"calculator/state-v1").string();
    const std::string lock_path=(root/"calculator/state.lock").string();
    const int lock=::open(lock_path.c_str(),O_RDWR);
    check(lock>=0&&::flock(lock,LOCK_EX|LOCK_NB)==0,"held lock fixture");
    check(!store_e.save_controller_state(),"busy durable document is not overwritten");
    if(lock>=0) { (void)::flock(lock,LOCK_UN); ::close(lock); }
    check(infiltratr_atomic_file_write_bytes(path.c_str(),INFILTRATR_ATOMIC_FILE_PRIVATE,
          "malformed",9U)==0,"malformed state fixture");
    check(!store_e.save_controller_state(),"malformed stored document is not overwritten");
    std::ifstream input(path);
    std::string retained;input>>retained;
    check(retained=="malformed","failed save preserves original bytes");
    check(infiltratr_atomic_file_write_bytes(path.c_str(),INFILTRATR_ATOMIC_FILE_PRIVATE,
          "",0U)==0,"empty state fixture");
    check(!store_e.save_controller_state()&&std::filesystem::file_size(path)==0U,
          "empty malformed document is preserved");
    if(failures) return 1;
    std::cout<<"Linux GUI/CLI persistence tests passed.\n";
    return 0;
}
