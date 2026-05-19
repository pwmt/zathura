#ifndef ZATHURA_FORK_MACOS_H
#define ZATHURA_FORK_MACOS_H

#ifdef __APPLE__
char** build_reexec_argv(char** orig_argv, int orig_argc, char** post_parse_files, int n_files, const char* specific_file);
#endif

#endif
