/* See LICENSE file for license and copyright information */

#include <stdbool.h>
#include <girara.h>

bool cmd_bookmark_create(girara_session_t* session, int argc, char** argv);
bool cmd_bookmark_delete(girara_session_t* session, int argc, char** argv);
bool cmd_bookmark_open(girara_session_t* session, int argc, char** argv);
bool cmd_close(girara_session_t* session, int argc, char** argv);
bool cmd_info(girara_session_t* session, int argc, char** argv);
bool cmd_print(girara_session_t* session, int argc, char** argv);
bool cmd_save(girara_session_t* session, int argc, char** argv);
