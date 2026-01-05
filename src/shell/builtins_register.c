#include "builtins.h"

extern const builtin_cmd cmd_cd_def;
extern const builtin_cmd cmd_pwd_def;
extern const builtin_cmd cmd_ls_def;
extern const builtin_cmd cmd_cat_def;
extern const builtin_cmd cmd_touch_def;
extern const builtin_cmd cmd_rm_def;
extern const builtin_cmd cmd_echo_def;
extern const builtin_cmd cmd_kill_def;
extern const builtin_cmd cmd_shutdown_def;
extern const builtin_cmd cmd_reboot_def;
extern const builtin_cmd cmd_run_def;

void builtins_register_all(void) {
    builtins_register_descriptor(&cmd_cd_def);
    builtins_register_descriptor(&cmd_pwd_def);
    builtins_register_descriptor(&cmd_ls_def);
    builtins_register_descriptor(&cmd_cat_def);
    builtins_register_descriptor(&cmd_touch_def);
    builtins_register_descriptor(&cmd_rm_def);
    builtins_register_descriptor(&cmd_echo_def);
    builtins_register_descriptor(&cmd_kill_def);
    builtins_register_descriptor(&cmd_shutdown_def);
    builtins_register_descriptor(&cmd_reboot_def);
    builtins_register_descriptor(&cmd_run_def);
}

