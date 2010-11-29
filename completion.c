/* See LICENSE file for license and copyright information */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "completion.h"
#include "utils.h"

girara_completion_t*
cc_print(girara_session_t* session, char* input)
{
  girara_completion_t* completion  = girara_completion_init();
  girara_completion_group_t* group = girara_completion_group_create(session, NULL);

  if(!session || !input || !completion || !group)
    return NULL;

  girara_completion_add_group(completion, group);

  char* const list_printers[] = {
    "lpstat", "-v", NULL
  };

  char* output;
  if(!execute_command(list_printers, &output)) {
    girara_completion_free(completion);
    return false;
  }

  /* parse single lines */
  unsigned int prefix_length = strlen("device for ");
  char* p = strtok(output, "\n");
  char* q;

  while(p)
  {
    if(!(p = strstr(p, "device for ")) || !(q = strchr(p, ':'))) {
      p = strtok(NULL, "\n");
      continue;
    }

    unsigned int printer_length = q - p - prefix_length;
    char* printer_name = malloc(sizeof(char) * printer_length);

    if(!printer_name) {
      p = strtok(NULL, "\n");
      continue;
    }

    strncpy(printer_name, p + prefix_length, printer_length - 1);
    printer_name[printer_length - 1] = '\0';

    /* add printer */
    girara_completion_group_add_element(group, printer_name, NULL);

    free(printer_name);

    /* next line */
    p = strtok(NULL, "\n");
  }

  free(output);

  return completion;
}
