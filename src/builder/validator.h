#pragma once
#include "project.h"
#include <stdbool.h>

typedef enum { V_ERROR, V_WARNING, V_INFO } ValidationSeverity;

typedef struct {
    ValidationSeverity severity;
    char *message;
    int item_id;
    int node_id;
    int edge_idx;
    int keymap_idx;
} ValidationIssue;

typedef struct {
    ValidationIssue *issues;
    int count;
} ValidationReport;

ValidationReport *validator_check_all(BuilderProject *p);
void validation_report_free(ValidationReport *r);