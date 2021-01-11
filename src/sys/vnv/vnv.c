#include <stddef.h>
#include "VnV.h"

INJECTION_EXECUTABLE(PETSC, VNV, mpi) 

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
INJECTION_OPTIONS(PETSC, petsc_vnv_schema) {

}
INJECTION_SUBPACKAGE(PETSC,UMFPACK)
INJECTION_SUBPACKAGE(PETSC,CHOLMOD)
INJECTION_SUBPACKAGE(PETSC,VnVHypre)



/** For testing purposes only -- if you are seeing this, i forgot to remove it
 *
 */

int petsc_vnv_test_function(int x) {
   
  INJECTION_LOOP_BEGIN(PETSC, VWORLD(PETSC), SanityCheck, x)
  for (int i = 0; i < 10; i++) {
    x += i;
    INJECTION_LOOP_ITER(PETSC,SanityCheck, inner);
  }

  INJECTION_LOOP_END(PETSC,SanityCheck);
  return x;
}
