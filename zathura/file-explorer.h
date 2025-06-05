#include "config.h"
char*
get_path(const char *fp);  

const int*
get_valid_files(const char **files, const int num_files, const char ***res);

const char *
write_typst_file(const char **files, const int num_files);
