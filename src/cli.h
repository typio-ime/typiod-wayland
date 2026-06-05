#ifndef TYPIO_DAEMON_CLI_H
#define TYPIO_DAEMON_CLI_H

#include "typio/runtime/instance.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct TypioOptions {
    TypioInstanceConfig instance_config;
    /** Engine directories from repeated -E/--engine-dir, in the order given,
     *  or NULL. Entries are borrowed argv pointers; the array itself is heap-
     *  allocated and owned by the caller. The full search list (these, then
     *  $TYPIO_ENGINE_PATH, then the system directory) is assembled in the host
     *  before init — see ADR-0025. */
    const char **engine_dirs;
    size_t engine_dir_count;
    bool verbose;
} TypioOptions;

void typio_options_init(TypioOptions *options);
int typio_parse_args(TypioOptions *options, int argc, char *argv[]);
void typio_print_help(const char *prog);
void typio_print_version(void);

#ifdef __cplusplus
}
#endif

#endif /* TYPIO_DAEMON_CLI_H */
