#pragma once
#include "project.h"
#include <stdbool.h>

typedef struct {
    char **errors;
    int error_count;
    char **warnings;
    int warning_count;
} CodegenResult;

char *codegen_fil_script(BuilderProject *p);
CodegenResult codegen_c_plugin(BuilderProject *p, const char *output_dir);
CodegenResult codegen_validate(BuilderProject *p);
void codegen_result_free(CodegenResult *r);