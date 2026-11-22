#ifndef EXWORD_QJS_BROWSER_H
#define EXWORD_QJS_BROWSER_H
#include "edl.h"
#include "toybox_abi.h"
/* Returns 1 for a selected script, 0 for cancel, negative on host failure. */
int qjs_choose_script(const struct edl_api *host,const struct shell_api *shell,
                      char out[EDL_PATH_MAX]);
/* Native EX-word file names can be case sensitive even for 8.3 files. */
void qjs_correct_shell_filename(const struct shell_api *shell,
                                char path[EDL_PATH_MAX]);
/* Unlike edl_api.read(), this obtains a file size before any data reads. */
int qjs_script_size(const struct edl_api *host,const struct shell_api *shell,
                    const char *path);
#endif
