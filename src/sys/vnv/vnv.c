#include <stddef.h>
#include "VnV.h"

INJECTION_LIBRARY(PETSC) 

static const char* petsc_vnv_schema = "{\"type\": \"object\", \"required\":[]}";

/**
 * VnV allows users to set options in hypre using the input file. This callback
 * registers the json schema for the available options with the toolkit and defines
 * the options callback. Because we are in C, we are using the C interface. 
 * 
 * Life would be way easier if we can just compile this file with c++ :)
 *
 * TODO: Add options to the schema and parse them in this function.
 */ 
INJECTION_OPTIONS(PETSC, petsc_vnv_schema, void) {

}

INJECTION_SUBPACKAGE(PETSC,VnVHypre)

