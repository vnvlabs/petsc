#ifndef PETSC_VNV_H
#define PETSC_VNV_H
#include <mpi.h>
#include <petscsys.h>

#include "VnV.h"

#define VPETSC(obj) VCUST(PetscObjectComm((PetscObject)obj))

#endif


