#ifndef PETSCBT_H
#define PETSCBT_H

#include <petscviewer.h>

/*S
     PetscBT - PETSc bitarrays

     Level: advanced

     PetscBTCreate(m,&bt)         - creates a bit array with enough room to hold m values
     PetscBTDestroy(&bt)          - destroys the bit array
     PetscBTMemzero(m,bt)         - zeros the entire bit array (sets all values to false)
     PetscBTSet(bt,index)         - sets a particular entry as true
     PetscBTClear(bt,index)       - sets a particular entry as false
     PetscBTLookup(bt,index)      - returns the value
     PetscBTLookupSet(bt,index)   - returns the value and then sets it true
     PetscBTLookupClear(bt,index) - returns the value and then sets it false
     PetscBTLength(m)             - returns number of bytes in array with m bits
     PetscBTView(m,bt,viewer)     - prints all the entries in a bit array

    We do not currently check error flags on PetscBTLookup(), PetcBTLookupSet(), PetscBTLength() cause error checking
    would cost hundreds more cycles then the operation.

S*/
typedef char* PetscBT;

/* convert an index i to an index suitable for indexing a PetscBT, such that
 * bt[PetscBTIndex(i)] returns the i'th value of the bt */
static inline size_t PetscBTIndex_Internal(PetscInt index)
{
  return (size_t)index/PETSC_BITS_PER_BYTE;
}

static inline char PetscBTMask_Internal(PetscInt index)
{
  return 1 << index%PETSC_BITS_PER_BYTE;
}

static inline size_t PetscBTLength(PetscInt m)
{
  return (size_t)m/PETSC_BITS_PER_BYTE+1;
}

static inline PetscErrorCode PetscBTMemzero(PetscInt m, PetscBT array)
{
  return PetscArrayzero(array,PetscBTLength(m));
}

static inline PetscErrorCode PetscBTDestroy(PetscBT *array)
{
  return (*array) ? PetscFree(*array) : 0;
}

static inline PetscErrorCode PetscBTCreate(PetscInt m, PetscBT *array)
{
  return PetscCalloc1(PetscBTLength(m),array);
}

static inline char PetscBTLookup(PetscBT array, PetscInt index)
{
  return array[PetscBTIndex_Internal(index)] & PetscBTMask_Internal(index);
}

static inline PetscErrorCode PetscBTSet(PetscBT array, PetscInt index)
{
  PetscFunctionBegin;
  array[PetscBTIndex_Internal(index)] |= PetscBTMask_Internal(index);
  PetscFunctionReturn(0);
}

static inline PetscErrorCode PetscBTNegate(PetscBT array, PetscInt index)
{
  PetscFunctionBegin;
  array[PetscBTIndex_Internal(index)] ^= PetscBTMask_Internal(index);
  PetscFunctionReturn(0);
}

static inline PetscErrorCode PetscBTClear(PetscBT array, PetscInt index)
{
  PetscFunctionBegin;
  array[PetscBTIndex_Internal(index)] &= (char) ~PetscBTMask_Internal(index);
  PetscFunctionReturn(0);
}

static inline char PetscBTLookupSet(PetscBT array, PetscInt index)
{
  const char ret = PetscBTLookup(array,index);
  CHKERRCONTINUE(PetscBTSet(array,index));
  return ret;
}

static inline char PetscBTLookupClear(PetscBT array, PetscInt index)
{
  const char ret = PetscBTLookup(array,index);
  CHKERRCONTINUE(PetscBTClear(array,index));
  return ret;
}

static inline PetscErrorCode PetscBTView(PetscInt m, const PetscBT bt, PetscViewer viewer)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  if (m < 1) PetscFunctionReturn(0);
  if (!viewer) {ierr = PetscViewerASCIIGetStdout(PETSC_COMM_SELF,&viewer);CHKERRQ(ierr);}
  ierr = PetscViewerASCIIPushSynchronized(viewer);CHKERRQ(ierr);
  for (PetscInt i = 0; i < m; ++i) {
    ierr = PetscViewerASCIISynchronizedPrintf(viewer,"%" PetscInt_FMT " %d\n",i,(int)PetscBTLookup(bt,i));CHKERRQ(ierr);
  }
  ierr = PetscViewerFlush(viewer);CHKERRQ(ierr);
  ierr = PetscViewerASCIIPopSynchronized(viewer);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}
#endif /* PETSCBT_H */
