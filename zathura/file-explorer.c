#include "file-explorer.h"
#include "macros.h"
#include "zathura/document.h"
#include "zathura/types.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>

char*
get_path(const char *fp){
  if(fp == NULL){
    return NULL;
  }

  const char *last_slash = strrchr(fp, '/');

  if(last_slash == NULL){
    char *path = malloc(1);
    if(path != NULL){
      path[0] = '\0';
    }
    return path;
  }

  size_t path_len = last_slash - fp;

  char *path = (char*)malloc(path_len + 1);
  if(path == NULL){
    return NULL;
  }

  strncpy(path, fp, path_len);
  path[path_len] = '\0';

  return path;
}

const int*
get_valid_files(const char **files, const int num_files, const char ***res){
  const char **valid_files = malloc(num_files * sizeof(char*));
  int *size = malloc(sizeof(int));
  for(int i = 0; i < num_files; i++){
    if(files[i] == NULL){
      continue;
    }
    const char *dot = strchr(files[i], '.');
    if(dot == NULL){
      // it is a directory -- valid
      // valid_files[*size] = files[i];
      // (*size)++;
      continue;
    }else{
      if(strcmp(files[i], "fe.pdf") == 0){
        continue;
      }
      if(strcmp(dot, ".pdf") == 0){
        valid_files[*size] = files[i];
        (*size)++;
      }
    }
  }
  *res = realloc(valid_files, *size * sizeof(char*));
  return size;
}


const char *
write_typst_file(const char **files, const int num_files){
  FILE* fptr;
  const char* file_name= malloc(20 * sizeof(char));
  file_name = "fe.typ";
  fptr = fopen(file_name,"w+");

  if(fptr == NULL){
    return NULL;
  }

  fprintf(fptr, "= File Explorer\n");

  for(int i = 0; i < num_files; i++){
    fprintf(fptr, "#link(\"%s\")[- %i %s\n]", files[i], (i+1), files[i]);
  }

  fclose(fptr);

  return file_name;
}

