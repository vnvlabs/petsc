#ifndef lint
static char vcid[] = "$Id: inherit.c,v 1.2 1996/02/08 18:26:06 bsmith Exp curfman $";
#endif
/*
     Provides utility routines for manulating any type of PETSc object.
*/
#include "petsc.h"  /*I   "petsc.h"    I*/

static int PetscObjectInherit_DefaultCopy(void *in, void **out)
{
  *out = in;
  return 0;
}

/*@C
   PetscObjectInherit - Associate another object with a given PETSc object. 
   This is to provide a limited support for inheritence when using 
   PETSc from C++.

   Input Parameters:
.  obj - the PETSc object
.  ptr - the other object to associate with the PETSc object
.  copy - a function used to copy the other object when the PETSc object 
          is copied, or PETSC_NULL to indicate the pointer is copied.

   Notes:
   PetscObjectInherit() can be used with any PETSc object, such at
   Mat, Vec, KSP, SNES, etc. Current limitation: each object can have
   only one child - we may extend this eventually.

.keywords: object, inherit
@*/
int PetscObjectInherit(PetscObject obj,void *ptr, int (*copy)(void *,void **))
{
  if (obj->child) SETERRQ(1,"PetscObjectInherit:Child already set; object can have only 1 child.");
  obj->child = ptr;
  if (copy == PETSC_NULL) copy = PetscObjectInherit_DefaultCopy;
  obj->childcopy = copy;
  return 0;
}

