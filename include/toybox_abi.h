/* Minimal, exact ABI mirror of Exword-toybox/include/shell_api.h.
 * The edl_api prefix must match Edelweiss's original, unextended layout.
 * Keep changes in sync with the Toybox SHL1 v1 API. */
#ifndef EXWORD_MQJS_TOYBOX_ABI_H
#define EXWORD_MQJS_TOYBOX_ABI_H
#include <edl.h>
#include <stdint.h>
#define SHELL_SERVICE_MAGIC 0x53484c31u
#define SHELL_SERVICE_VERSION 1u
#define SHELL_MAX_COMMANDS 24
#define SHELL_NAME_SIZE 16
struct shell_dirent {
    char name[128];
    int directory;
};
struct shell_api {
    struct edl_api edl;
    uint32_t shell_magic, shell_version;
    const char *command;
    int argc;
    const char *const *argv;
    const char *cwd;
    int (*console_write)(const void *bytes, unsigned length);
    int (*read_key)(void);
    int (*resolve_path)(const char *given, char out[EDL_PATH_MAX]);
    int (*file_size)(const char *absolute);
    int (*list_dir)(const char *absolute, unsigned first,
                    struct shell_dirent *items, unsigned capacity);
    int (*file_write)(const char *absolute, unsigned offset, const void *bytes,
                      unsigned length, unsigned flags);
    int (*file_delete)(const char *absolute);
    int (*file_rename)(const char *old_absolute, const char *new_absolute);
    int (*file_create)(const char *absolute, int directory);
    int (*tty_size)(unsigned *cols, unsigned *rows);
};
struct shell_manifest {
    uint32_t magic, version, count;
    char names[SHELL_MAX_COMMANDS][SHELL_NAME_SIZE];
};
static inline const struct shell_api *toybox_api(const struct edl_api *api)
{
    if (!api || api->size < sizeof(struct shell_api)) return 0;
    const struct shell_api *sh = (const struct shell_api *)api;
    return sh->shell_magic == SHELL_SERVICE_MAGIC &&
           sh->shell_version == SHELL_SERVICE_VERSION ? sh : 0;
}
#endif
