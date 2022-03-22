#include <petsc/private/dmpleximpl.h>   /*I "petscdmplex.h" I*/
#include <petsc/private/snesimpl.h>     /*I "petscsnes.h"   I*/
#include <petscds.h>
#include <petsc/private/petscimpl.h>
#include <petsc/private/petscfeimpl.h>

static void pressure_Private(PetscInt dim, PetscInt Nf, PetscInt NfAux,
                             const PetscInt uOff[], const PetscInt uOff_x[], const PetscScalar u[], const PetscScalar u_t[], const PetscScalar u_x[],
                             const PetscInt aOff[], const PetscInt aOff_x[], const PetscScalar a[], const PetscScalar a_t[], const PetscScalar a_x[],
                             PetscReal t, const PetscReal x[], PetscInt numConstants, const PetscScalar constants[], PetscScalar p[])
{
  p[0] = u[uOff[1]];
}

/*
  SNESCorrectDiscretePressure_Private - Add a vector in the nullspace to make the continuum integral of the pressure field equal to zero. This is normally used only to evaluate convergence rates for the pressure accurately.

  Collective on SNES

  Input Parameters:
+ snes      - The SNES
. pfield    - The field number for pressure
. nullspace - The pressure nullspace
. u         - The solution vector
- ctx       - An optional user context

  Output Parameter:
. u         - The solution with a continuum pressure integral of zero

  Notes:
  If int(u) = a and int(n) = b, then int(u - a/b n) = a - a/b b = 0. We assume that the nullspace is a single vector given explicitly.

  Level: developer

.seealso: SNESConvergedCorrectPressure()
*/
static PetscErrorCode SNESCorrectDiscretePressure_Private(SNES snes, PetscInt pfield, MatNullSpace nullspace, Vec u, void *ctx)
{
  DM             dm;
  PetscDS        ds;
  const Vec     *nullvecs;
  PetscScalar    pintd, *intc, *intn;
  MPI_Comm       comm;
  PetscInt       Nf, Nv;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = PetscObjectGetComm((PetscObject) snes, &comm);CHKERRQ(ierr);
  ierr = SNESGetDM(snes, &dm);CHKERRQ(ierr);
  PetscCheckFalse(!dm,comm, PETSC_ERR_ARG_WRONG, "Cannot compute test without a SNES DM");
  PetscCheckFalse(!nullspace,comm, PETSC_ERR_ARG_WRONG, "Cannot compute test without a Jacobian nullspace");
  ierr = DMGetDS(dm, &ds);CHKERRQ(ierr);
  ierr = PetscDSSetObjective(ds, pfield, pressure_Private);CHKERRQ(ierr);
  ierr = MatNullSpaceGetVecs(nullspace, NULL, &Nv, &nullvecs);CHKERRQ(ierr);
  PetscCheckFalse(Nv != 1,comm, PETSC_ERR_ARG_OUTOFRANGE, "Can only handle a single null vector for pressure, not %D", Nv);
  ierr = VecDot(nullvecs[0], u, &pintd);CHKERRQ(ierr);
  PetscCheckFalse(PetscAbsScalar(pintd) > PETSC_SMALL,comm, PETSC_ERR_ARG_WRONG, "Discrete integral of pressure: %g", (double) PetscRealPart(pintd));
  ierr = PetscDSGetNumFields(ds, &Nf);CHKERRQ(ierr);
  ierr = PetscMalloc2(Nf, &intc, Nf, &intn);CHKERRQ(ierr);
  ierr = DMPlexComputeIntegralFEM(dm, nullvecs[0], intn, ctx);CHKERRQ(ierr);
  ierr = DMPlexComputeIntegralFEM(dm, u, intc, ctx);CHKERRQ(ierr);
  ierr = VecAXPY(u, -intc[pfield]/intn[pfield], nullvecs[0]);CHKERRQ(ierr);
#if defined (PETSC_USE_DEBUG)
  ierr = DMPlexComputeIntegralFEM(dm, u, intc, ctx);CHKERRQ(ierr);
  PetscCheckFalse(PetscAbsScalar(intc[pfield]) > PETSC_SMALL,comm, PETSC_ERR_ARG_WRONG, "Continuum integral of pressure after correction: %g", (double) PetscRealPart(intc[pfield]));
#endif
  ierr = PetscFree2(intc, intn);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/*@C
   SNESConvergedCorrectPressure - Convergence test that adds a vector in the nullspace to make the continuum integral of the pressure field equal to zero. This is normally used only to evaluate convergence rates for the pressure accurately. The convergence test itself just mimics SNESConvergedDefault().

   Logically Collective on SNES

   Input Parameters:
+  snes - the SNES context
.  it - the iteration (0 indicates before any Newton steps)
.  xnorm - 2-norm of current iterate
.  snorm - 2-norm of current step
.  fnorm - 2-norm of function at current iterate
-  ctx   - Optional user context

   Output Parameter:
.  reason  - SNES_CONVERGED_ITERATING, SNES_CONVERGED_ITS, or SNES_DIVERGED_FNORM_NAN

   Notes:
   In order to use this monitor, you must setup several PETSc structures. First fields must be added to the DM, and a PetscDS must be created with discretizations of those fields. We currently assume that the pressure field has index 1. The pressure field must have a nullspace, likely created using the DMSetNullSpaceConstructor() interface. Last we must be able to integrate the pressure over the domain, so the DM attached to the SNES must be a Plex at this time.

   Level: advanced

.seealso: SNESConvergedDefault(), SNESSetConvergenceTest(), DMSetNullSpaceConstructor()
@*/
PetscErrorCode SNESConvergedCorrectPressure(SNES snes, PetscInt it, PetscReal xnorm, PetscReal gnorm, PetscReal f, SNESConvergedReason *reason, void *ctx)
{
  PetscBool      monitorIntegral = PETSC_FALSE;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = SNESConvergedDefault(snes, it, xnorm, gnorm, f, reason, ctx);CHKERRQ(ierr);
  if (monitorIntegral) {
    Mat          J;
    Vec          u;
    MatNullSpace nullspace;
    const Vec   *nullvecs;
    PetscScalar  pintd;

    ierr = SNESGetSolution(snes, &u);CHKERRQ(ierr);
    ierr = SNESGetJacobian(snes, &J, NULL, NULL, NULL);CHKERRQ(ierr);
    ierr = MatGetNullSpace(J, &nullspace);CHKERRQ(ierr);
    ierr = MatNullSpaceGetVecs(nullspace, NULL, NULL, &nullvecs);CHKERRQ(ierr);
    ierr = VecDot(nullvecs[0], u, &pintd);CHKERRQ(ierr);
    ierr = PetscInfo(snes, "SNES: Discrete integral of pressure: %g\n", (double) PetscRealPart(pintd));CHKERRQ(ierr);
  }
  if (*reason > 0) {
    Mat          J;
    Vec          u;
    MatNullSpace nullspace;
    PetscInt     pfield = 1;

    ierr = SNESGetSolution(snes, &u);CHKERRQ(ierr);
    ierr = SNESGetJacobian(snes, &J, NULL, NULL, NULL);CHKERRQ(ierr);
    ierr = MatGetNullSpace(J, &nullspace);CHKERRQ(ierr);
    ierr = SNESCorrectDiscretePressure_Private(snes, pfield, nullspace, u, ctx);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

/************************** Interpolation *******************************/

static PetscErrorCode DMSNESConvertPlex(DM dm, DM *plex, PetscBool copy)
{
  PetscBool      isPlex;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = PetscObjectTypeCompare((PetscObject) dm, DMPLEX, &isPlex);CHKERRQ(ierr);
  if (isPlex) {
    *plex = dm;
    ierr = PetscObjectReference((PetscObject) dm);CHKERRQ(ierr);
  } else {
    ierr = PetscObjectQuery((PetscObject) dm, "dm_plex", (PetscObject *) plex);CHKERRQ(ierr);
    if (!*plex) {
      ierr = DMConvert(dm,DMPLEX,plex);CHKERRQ(ierr);
      ierr = PetscObjectCompose((PetscObject) dm, "dm_plex", (PetscObject) *plex);CHKERRQ(ierr);
      if (copy) {
        ierr = DMCopyDMSNES(dm, *plex);CHKERRQ(ierr);
        ierr = DMCopyAuxiliaryVec(dm, *plex);CHKERRQ(ierr);
      }
    } else {
      ierr = PetscObjectReference((PetscObject) *plex);CHKERRQ(ierr);
    }
  }
  PetscFunctionReturn(0);
}

/*@C
  DMInterpolationCreate - Creates a DMInterpolationInfo context

  Collective

  Input Parameter:
. comm - the communicator

  Output Parameter:
. ctx - the context

  Level: beginner

.seealso: DMInterpolationEvaluate(), DMInterpolationAddPoints(), DMInterpolationDestroy()
@*/
PetscErrorCode DMInterpolationCreate(MPI_Comm comm, DMInterpolationInfo *ctx)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  PetscValidPointer(ctx, 2);
  ierr = PetscNew(ctx);CHKERRQ(ierr);

  (*ctx)->comm   = comm;
  (*ctx)->dim    = -1;
  (*ctx)->nInput = 0;
  (*ctx)->points = NULL;
  (*ctx)->cells  = NULL;
  (*ctx)->n      = -1;
  (*ctx)->coords = NULL;
  PetscFunctionReturn(0);
}

/*@C
  DMInterpolationSetDim - Sets the spatial dimension for the interpolation context

  Not collective

  Input Parameters:
+ ctx - the context
- dim - the spatial dimension

  Level: intermediate

.seealso: DMInterpolationGetDim(), DMInterpolationEvaluate(), DMInterpolationAddPoints()
@*/
PetscErrorCode DMInterpolationSetDim(DMInterpolationInfo ctx, PetscInt dim)
{
  PetscFunctionBegin;
  PetscCheckFalse((dim < 1) || (dim > 3),ctx->comm, PETSC_ERR_ARG_OUTOFRANGE, "Invalid dimension for points: %D", dim);
  ctx->dim = dim;
  PetscFunctionReturn(0);
}

/*@C
  DMInterpolationGetDim - Gets the spatial dimension for the interpolation context

  Not collective

  Input Parameter:
. ctx - the context

  Output Parameter:
. dim - the spatial dimension

  Level: intermediate

.seealso: DMInterpolationSetDim(), DMInterpolationEvaluate(), DMInterpolationAddPoints()
@*/
PetscErrorCode DMInterpolationGetDim(DMInterpolationInfo ctx, PetscInt *dim)
{
  PetscFunctionBegin;
  PetscValidIntPointer(dim, 2);
  *dim = ctx->dim;
  PetscFunctionReturn(0);
}

/*@C
  DMInterpolationSetDof - Sets the number of fields interpolated at a point for the interpolation context

  Not collective

  Input Parameters:
+ ctx - the context
- dof - the number of fields

  Level: intermediate

.seealso: DMInterpolationGetDof(), DMInterpolationEvaluate(), DMInterpolationAddPoints()
@*/
PetscErrorCode DMInterpolationSetDof(DMInterpolationInfo ctx, PetscInt dof)
{
  PetscFunctionBegin;
  PetscCheckFalse(dof < 1,ctx->comm, PETSC_ERR_ARG_OUTOFRANGE, "Invalid number of components: %D", dof);
  ctx->dof = dof;
  PetscFunctionReturn(0);
}

/*@C
  DMInterpolationGetDof - Gets the number of fields interpolated at a point for the interpolation context

  Not collective

  Input Parameter:
. ctx - the context

  Output Parameter:
. dof - the number of fields

  Level: intermediate

.seealso: DMInterpolationSetDof(), DMInterpolationEvaluate(), DMInterpolationAddPoints()
@*/
PetscErrorCode DMInterpolationGetDof(DMInterpolationInfo ctx, PetscInt *dof)
{
  PetscFunctionBegin;
  PetscValidIntPointer(dof, 2);
  *dof = ctx->dof;
  PetscFunctionReturn(0);
}

/*@C
  DMInterpolationAddPoints - Add points at which we will interpolate the fields

  Not collective

  Input Parameters:
+ ctx    - the context
. n      - the number of points
- points - the coordinates for each point, an array of size n * dim

  Note: The coordinate information is copied.

  Level: intermediate

.seealso: DMInterpolationSetDim(), DMInterpolationEvaluate(), DMInterpolationCreate()
@*/
PetscErrorCode DMInterpolationAddPoints(DMInterpolationInfo ctx, PetscInt n, PetscReal points[])
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  PetscCheckFalse(ctx->dim < 0,ctx->comm, PETSC_ERR_ARG_WRONGSTATE, "The spatial dimension has not been set");
  PetscCheckFalse(ctx->points,ctx->comm, PETSC_ERR_ARG_WRONGSTATE, "Cannot add points multiple times yet");
  ctx->nInput = n;

  ierr = PetscMalloc1(n*ctx->dim, &ctx->points);CHKERRQ(ierr);
  ierr = PetscArraycpy(ctx->points, points, n*ctx->dim);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/*@C
  DMInterpolationSetUp - Compute spatial indices for point location during interpolation

  Collective on ctx

  Input Parameters:
+ ctx - the context
. dm  - the DM for the function space used for interpolation
. redundantPoints - If PETSC_TRUE, all processes are passing in the same array of points. Otherwise, points need to be communicated among processes.
- ignoreOutsideDomain - If PETSC_TRUE, ignore points outside the domain, otherwise return an error

  Level: intermediate

.seealso: DMInterpolationEvaluate(), DMInterpolationAddPoints(), DMInterpolationCreate()
@*/
PetscErrorCode DMInterpolationSetUp(DMInterpolationInfo ctx, DM dm, PetscBool redundantPoints, PetscBool ignoreOutsideDomain)
{
  MPI_Comm          comm = ctx->comm;
  PetscScalar       *a;
  PetscInt          p, q, i;
  PetscMPIInt       rank, size;
  PetscErrorCode    ierr;
  Vec               pointVec;
  PetscSF           cellSF;
  PetscLayout       layout;
  PetscReal         *globalPoints;
  PetscScalar       *globalPointsScalar;
  const PetscInt    *ranges;
  PetscMPIInt       *counts, *displs;
  const PetscSFNode *foundCells;
  const PetscInt    *foundPoints;
  PetscMPIInt       *foundProcs, *globalProcs;
  PetscInt          n, N, numFound;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 2);
  ierr = MPI_Comm_size(comm, &size);CHKERRMPI(ierr);
  ierr = MPI_Comm_rank(comm, &rank);CHKERRMPI(ierr);
  PetscCheckFalse(ctx->dim < 0,comm, PETSC_ERR_ARG_WRONGSTATE, "The spatial dimension has not been set");
  /* Locate points */
  n = ctx->nInput;
  if (!redundantPoints) {
    ierr = PetscLayoutCreate(comm, &layout);CHKERRQ(ierr);
    ierr = PetscLayoutSetBlockSize(layout, 1);CHKERRQ(ierr);
    ierr = PetscLayoutSetLocalSize(layout, n);CHKERRQ(ierr);
    ierr = PetscLayoutSetUp(layout);CHKERRQ(ierr);
    ierr = PetscLayoutGetSize(layout, &N);CHKERRQ(ierr);
    /* Communicate all points to all processes */
    ierr = PetscMalloc3(N*ctx->dim,&globalPoints,size,&counts,size,&displs);CHKERRQ(ierr);
    ierr = PetscLayoutGetRanges(layout, &ranges);CHKERRQ(ierr);
    for (p = 0; p < size; ++p) {
      counts[p] = (ranges[p+1] - ranges[p])*ctx->dim;
      displs[p] = ranges[p]*ctx->dim;
    }
    ierr = MPI_Allgatherv(ctx->points, n*ctx->dim, MPIU_REAL, globalPoints, counts, displs, MPIU_REAL, comm);CHKERRMPI(ierr);
  } else {
    N = n;
    globalPoints = ctx->points;
    counts = displs = NULL;
    layout = NULL;
  }
#if 0
  ierr = PetscMalloc3(N,&foundCells,N,&foundProcs,N,&globalProcs);CHKERRQ(ierr);
  /* foundCells[p] = m->locatePoint(&globalPoints[p*ctx->dim]); */
#else
#if defined(PETSC_USE_COMPLEX)
  ierr = PetscMalloc1(N*ctx->dim,&globalPointsScalar);CHKERRQ(ierr);
  for (i=0; i<N*ctx->dim; i++) globalPointsScalar[i] = globalPoints[i];
#else
  globalPointsScalar = globalPoints;
#endif
  ierr = VecCreateSeqWithArray(PETSC_COMM_SELF, ctx->dim, N*ctx->dim, globalPointsScalar, &pointVec);CHKERRQ(ierr);
  ierr = PetscMalloc2(N,&foundProcs,N,&globalProcs);CHKERRQ(ierr);
  for (p = 0; p < N; ++p) {foundProcs[p] = size;}
  cellSF = NULL;
  ierr = DMLocatePoints(dm, pointVec, DM_POINTLOCATION_REMOVE, &cellSF);CHKERRQ(ierr);
  ierr = PetscSFGetGraph(cellSF,NULL,&numFound,&foundPoints,&foundCells);CHKERRQ(ierr);
#endif
  for (p = 0; p < numFound; ++p) {
    if (foundCells[p].index >= 0) foundProcs[foundPoints ? foundPoints[p] : p] = rank;
  }
  /* Let the lowest rank process own each point */
  ierr   = MPIU_Allreduce(foundProcs, globalProcs, N, MPI_INT, MPI_MIN, comm);CHKERRMPI(ierr);
  ctx->n = 0;
  for (p = 0; p < N; ++p) {
    if (globalProcs[p] == size) {
      PetscCheckFalse(!ignoreOutsideDomain,comm, PETSC_ERR_PLIB, "Point %d: %g %g %g not located in mesh", p, (double)globalPoints[p*ctx->dim+0], (double)(ctx->dim > 1 ? globalPoints[p*ctx->dim+1] : 0.0), (double)(ctx->dim > 2 ? globalPoints[p*ctx->dim+2] : 0.0));
      else if (rank == 0) ++ctx->n;
    } else if (globalProcs[p] == rank) ++ctx->n;
  }
  /* Create coordinates vector and array of owned cells */
  ierr = PetscMalloc1(ctx->n, &ctx->cells);CHKERRQ(ierr);
  ierr = VecCreate(comm, &ctx->coords);CHKERRQ(ierr);
  ierr = VecSetSizes(ctx->coords, ctx->n*ctx->dim, PETSC_DECIDE);CHKERRQ(ierr);
  ierr = VecSetBlockSize(ctx->coords, ctx->dim);CHKERRQ(ierr);
  ierr = VecSetType(ctx->coords,VECSTANDARD);CHKERRQ(ierr);
  ierr = VecGetArray(ctx->coords, &a);CHKERRQ(ierr);
  for (p = 0, q = 0, i = 0; p < N; ++p) {
    if (globalProcs[p] == rank) {
      PetscInt d;

      for (d = 0; d < ctx->dim; ++d, ++i) a[i] = globalPoints[p*ctx->dim+d];
      ctx->cells[q] = foundCells[q].index;
      ++q;
    }
    if (globalProcs[p] == size && rank == 0) {
      PetscInt d;

      for (d = 0; d < ctx->dim; ++d, ++i) a[i] = 0.;
      ctx->cells[q] = -1;
      ++q;
    }
  }
  ierr = VecRestoreArray(ctx->coords, &a);CHKERRQ(ierr);
#if 0
  ierr = PetscFree3(foundCells,foundProcs,globalProcs);CHKERRQ(ierr);
#else
  ierr = PetscFree2(foundProcs,globalProcs);CHKERRQ(ierr);
  ierr = PetscSFDestroy(&cellSF);CHKERRQ(ierr);
  ierr = VecDestroy(&pointVec);CHKERRQ(ierr);
#endif
  if ((void*)globalPointsScalar != (void*)globalPoints) {ierr = PetscFree(globalPointsScalar);CHKERRQ(ierr);}
  if (!redundantPoints) {ierr = PetscFree3(globalPoints,counts,displs);CHKERRQ(ierr);}
  ierr = PetscLayoutDestroy(&layout);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/*@C
  DMInterpolationGetCoordinates - Gets a Vec with the coordinates of each interpolation point

  Collective on ctx

  Input Parameter:
. ctx - the context

  Output Parameter:
. coordinates  - the coordinates of interpolation points

  Note: The local vector entries correspond to interpolation points lying on this process, according to the associated DM. This is a borrowed vector that the user should not destroy.

  Level: intermediate

.seealso: DMInterpolationEvaluate(), DMInterpolationAddPoints(), DMInterpolationCreate()
@*/
PetscErrorCode DMInterpolationGetCoordinates(DMInterpolationInfo ctx, Vec *coordinates)
{
  PetscFunctionBegin;
  PetscValidPointer(coordinates, 2);
  PetscCheckFalse(!ctx->coords,ctx->comm, PETSC_ERR_ARG_WRONGSTATE, "The interpolation context has not been setup.");
  *coordinates = ctx->coords;
  PetscFunctionReturn(0);
}

/*@C
  DMInterpolationGetVector - Gets a Vec which can hold all the interpolated field values

  Collective on ctx

  Input Parameter:
. ctx - the context

  Output Parameter:
. v  - a vector capable of holding the interpolated field values

  Note: This vector should be returned using DMInterpolationRestoreVector().

  Level: intermediate

.seealso: DMInterpolationRestoreVector(), DMInterpolationEvaluate(), DMInterpolationAddPoints(), DMInterpolationCreate()
@*/
PetscErrorCode DMInterpolationGetVector(DMInterpolationInfo ctx, Vec *v)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  PetscValidPointer(v, 2);
  PetscCheckFalse(!ctx->coords,ctx->comm, PETSC_ERR_ARG_WRONGSTATE, "The interpolation context has not been setup.");
  ierr = VecCreate(ctx->comm, v);CHKERRQ(ierr);
  ierr = VecSetSizes(*v, ctx->n*ctx->dof, PETSC_DECIDE);CHKERRQ(ierr);
  ierr = VecSetBlockSize(*v, ctx->dof);CHKERRQ(ierr);
  ierr = VecSetType(*v,VECSTANDARD);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/*@C
  DMInterpolationRestoreVector - Returns a Vec which can hold all the interpolated field values

  Collective on ctx

  Input Parameters:
+ ctx - the context
- v  - a vector capable of holding the interpolated field values

  Level: intermediate

.seealso: DMInterpolationGetVector(), DMInterpolationEvaluate(), DMInterpolationAddPoints(), DMInterpolationCreate()
@*/
PetscErrorCode DMInterpolationRestoreVector(DMInterpolationInfo ctx, Vec *v)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  PetscValidPointer(v, 2);
  PetscCheckFalse(!ctx->coords,ctx->comm, PETSC_ERR_ARG_WRONGSTATE, "The interpolation context has not been setup.");
  ierr = VecDestroy(v);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

static inline PetscErrorCode DMInterpolate_Segment_Private(DMInterpolationInfo ctx, DM dm, Vec xLocal, Vec v)
{
  PetscReal          v0, J, invJ, detJ;
  const PetscInt     dof = ctx->dof;
  const PetscScalar *coords;
  PetscScalar       *a;
  PetscInt           p;
  PetscErrorCode     ierr;

  PetscFunctionBegin;
  ierr = VecGetArrayRead(ctx->coords, &coords);CHKERRQ(ierr);
  ierr = VecGetArray(v, &a);CHKERRQ(ierr);
  for (p = 0; p < ctx->n; ++p) {
    PetscInt     c = ctx->cells[p];
    PetscScalar *x = NULL;
    PetscReal    xir[1];
    PetscInt     xSize, comp;

    ierr = DMPlexComputeCellGeometryFEM(dm, c, NULL, &v0, &J, &invJ, &detJ);CHKERRQ(ierr);
    PetscCheck(detJ > 0.0, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Invalid determinant %g for element %" PetscInt_FMT, (double) detJ, c);
    xir[0] = invJ*PetscRealPart(coords[p] - v0);
    ierr = DMPlexVecGetClosure(dm, NULL, xLocal, c, &xSize, &x);CHKERRQ(ierr);
    if (2*dof == xSize) {
      for (comp = 0; comp < dof; ++comp) a[p*dof+comp] = x[0*dof+comp]*(1 - xir[0]) + x[1*dof+comp]*xir[0];
    } else if (dof == xSize) {
      for (comp = 0; comp < dof; ++comp) a[p*dof+comp] = x[0*dof+comp];
    } else SETERRQ(PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Input closure size %" PetscInt_FMT " must be either %" PetscInt_FMT " or %" PetscInt_FMT, xSize, 2*dof, dof);
    ierr = DMPlexVecRestoreClosure(dm, NULL, xLocal, c, &xSize, &x);CHKERRQ(ierr);
  }
  ierr = VecRestoreArray(v, &a);CHKERRQ(ierr);
  ierr = VecRestoreArrayRead(ctx->coords, &coords);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

static inline PetscErrorCode DMInterpolate_Triangle_Private(DMInterpolationInfo ctx, DM dm, Vec xLocal, Vec v)
{
  PetscReal      *v0, *J, *invJ, detJ;
  const PetscScalar *coords;
  PetscScalar    *a;
  PetscInt       p;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = PetscMalloc3(ctx->dim,&v0,ctx->dim*ctx->dim,&J,ctx->dim*ctx->dim,&invJ);CHKERRQ(ierr);
  ierr = VecGetArrayRead(ctx->coords, &coords);CHKERRQ(ierr);
  ierr = VecGetArray(v, &a);CHKERRQ(ierr);
  for (p = 0; p < ctx->n; ++p) {
    PetscInt     c = ctx->cells[p];
    PetscScalar *x = NULL;
    PetscReal    xi[4];
    PetscInt     d, f, comp;

    ierr = DMPlexComputeCellGeometryFEM(dm, c, NULL, v0, J, invJ, &detJ);CHKERRQ(ierr);
    PetscCheckFalse(detJ <= 0.0,PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Invalid determinant %g for element %D", (double)detJ, c);
    ierr = DMPlexVecGetClosure(dm, NULL, xLocal, c, NULL, &x);CHKERRQ(ierr);
    for (comp = 0; comp < ctx->dof; ++comp) a[p*ctx->dof+comp] = x[0*ctx->dof+comp];

    for (d = 0; d < ctx->dim; ++d) {
      xi[d] = 0.0;
      for (f = 0; f < ctx->dim; ++f) xi[d] += invJ[d*ctx->dim+f]*0.5*PetscRealPart(coords[p*ctx->dim+f] - v0[f]);
      for (comp = 0; comp < ctx->dof; ++comp) a[p*ctx->dof+comp] += PetscRealPart(x[(d+1)*ctx->dof+comp] - x[0*ctx->dof+comp])*xi[d];
    }
    ierr = DMPlexVecRestoreClosure(dm, NULL, xLocal, c, NULL, &x);CHKERRQ(ierr);
  }
  ierr = VecRestoreArray(v, &a);CHKERRQ(ierr);
  ierr = VecRestoreArrayRead(ctx->coords, &coords);CHKERRQ(ierr);
  ierr = PetscFree3(v0, J, invJ);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

static inline PetscErrorCode DMInterpolate_Tetrahedron_Private(DMInterpolationInfo ctx, DM dm, Vec xLocal, Vec v)
{
  PetscReal      *v0, *J, *invJ, detJ;
  const PetscScalar *coords;
  PetscScalar    *a;
  PetscInt       p;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = PetscMalloc3(ctx->dim,&v0,ctx->dim*ctx->dim,&J,ctx->dim*ctx->dim,&invJ);CHKERRQ(ierr);
  ierr = VecGetArrayRead(ctx->coords, &coords);CHKERRQ(ierr);
  ierr = VecGetArray(v, &a);CHKERRQ(ierr);
  for (p = 0; p < ctx->n; ++p) {
    PetscInt       c = ctx->cells[p];
    const PetscInt order[3] = {2, 1, 3};
    PetscScalar   *x = NULL;
    PetscReal      xi[4];
    PetscInt       d, f, comp;

    ierr = DMPlexComputeCellGeometryFEM(dm, c, NULL, v0, J, invJ, &detJ);CHKERRQ(ierr);
    PetscCheckFalse(detJ <= 0.0,PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Invalid determinant %g for element %D", (double)detJ, c);
    ierr = DMPlexVecGetClosure(dm, NULL, xLocal, c, NULL, &x);CHKERRQ(ierr);
    for (comp = 0; comp < ctx->dof; ++comp) a[p*ctx->dof+comp] = x[0*ctx->dof+comp];

    for (d = 0; d < ctx->dim; ++d) {
      xi[d] = 0.0;
      for (f = 0; f < ctx->dim; ++f) xi[d] += invJ[d*ctx->dim+f]*0.5*PetscRealPart(coords[p*ctx->dim+f] - v0[f]);
      for (comp = 0; comp < ctx->dof; ++comp) a[p*ctx->dof+comp] += PetscRealPart(x[order[d]*ctx->dof+comp] - x[0*ctx->dof+comp])*xi[d];
    }
    ierr = DMPlexVecRestoreClosure(dm, NULL, xLocal, c, NULL, &x);CHKERRQ(ierr);
  }
  ierr = VecRestoreArray(v, &a);CHKERRQ(ierr);
  ierr = VecRestoreArrayRead(ctx->coords, &coords);CHKERRQ(ierr);
  ierr = PetscFree3(v0, J, invJ);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

static inline PetscErrorCode QuadMap_Private(SNES snes, Vec Xref, Vec Xreal, void *ctx)
{
  const PetscScalar *vertices = (const PetscScalar*) ctx;
  const PetscScalar x0        = vertices[0];
  const PetscScalar y0        = vertices[1];
  const PetscScalar x1        = vertices[2];
  const PetscScalar y1        = vertices[3];
  const PetscScalar x2        = vertices[4];
  const PetscScalar y2        = vertices[5];
  const PetscScalar x3        = vertices[6];
  const PetscScalar y3        = vertices[7];
  const PetscScalar f_1       = x1 - x0;
  const PetscScalar g_1       = y1 - y0;
  const PetscScalar f_3       = x3 - x0;
  const PetscScalar g_3       = y3 - y0;
  const PetscScalar f_01      = x2 - x1 - x3 + x0;
  const PetscScalar g_01      = y2 - y1 - y3 + y0;
  const PetscScalar *ref;
  PetscScalar       *real;
  PetscErrorCode    ierr;

  PetscFunctionBegin;
  ierr = VecGetArrayRead(Xref,  &ref);CHKERRQ(ierr);
  ierr = VecGetArray(Xreal, &real);CHKERRQ(ierr);
  {
    const PetscScalar p0 = ref[0];
    const PetscScalar p1 = ref[1];

    real[0] = x0 + f_1 * p0 + f_3 * p1 + f_01 * p0 * p1;
    real[1] = y0 + g_1 * p0 + g_3 * p1 + g_01 * p0 * p1;
  }
  ierr = PetscLogFlops(28);CHKERRQ(ierr);
  ierr = VecRestoreArrayRead(Xref,  &ref);CHKERRQ(ierr);
  ierr = VecRestoreArray(Xreal, &real);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#include <petsc/private/dmimpl.h>
static inline PetscErrorCode QuadJacobian_Private(SNES snes, Vec Xref, Mat J, Mat M, void *ctx)
{
  const PetscScalar *vertices = (const PetscScalar*) ctx;
  const PetscScalar x0        = vertices[0];
  const PetscScalar y0        = vertices[1];
  const PetscScalar x1        = vertices[2];
  const PetscScalar y1        = vertices[3];
  const PetscScalar x2        = vertices[4];
  const PetscScalar y2        = vertices[5];
  const PetscScalar x3        = vertices[6];
  const PetscScalar y3        = vertices[7];
  const PetscScalar f_01      = x2 - x1 - x3 + x0;
  const PetscScalar g_01      = y2 - y1 - y3 + y0;
  const PetscScalar *ref;
  PetscErrorCode    ierr;

  PetscFunctionBegin;
  ierr = VecGetArrayRead(Xref,  &ref);CHKERRQ(ierr);
  {
    const PetscScalar x       = ref[0];
    const PetscScalar y       = ref[1];
    const PetscInt    rows[2] = {0, 1};
    PetscScalar       values[4];

    values[0] = (x1 - x0 + f_01*y) * 0.5; values[1] = (x3 - x0 + f_01*x) * 0.5;
    values[2] = (y1 - y0 + g_01*y) * 0.5; values[3] = (y3 - y0 + g_01*x) * 0.5;
    ierr      = MatSetValues(J, 2, rows, 2, rows, values, INSERT_VALUES);CHKERRQ(ierr);
  }
  ierr = PetscLogFlops(30);CHKERRQ(ierr);
  ierr = VecRestoreArrayRead(Xref,  &ref);CHKERRQ(ierr);
  ierr = MatAssemblyBegin(J, MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  ierr = MatAssemblyEnd(J, MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

static inline PetscErrorCode DMInterpolate_Quad_Private(DMInterpolationInfo ctx, DM dm, Vec xLocal, Vec v)
{
  DM                 dmCoord;
  PetscFE            fem = NULL;
  SNES               snes;
  KSP                ksp;
  PC                 pc;
  Vec                coordsLocal, r, ref, real;
  Mat                J;
  PetscTabulation    T = NULL;
  const PetscScalar *coords;
  PetscScalar        *a;
  PetscReal          xir[2] = {0., 0.};
  PetscInt           Nf, p;
  const PetscInt     dof = ctx->dof;
  PetscErrorCode     ierr;

  PetscFunctionBegin;
  ierr = DMGetNumFields(dm, &Nf);CHKERRQ(ierr);
  if (Nf) {
    PetscObject  obj;
    PetscClassId id;

    ierr = DMGetField(dm, 0, NULL, &obj);CHKERRQ(ierr);
    ierr = PetscObjectGetClassId(obj, &id);CHKERRQ(ierr);
    if (id == PETSCFE_CLASSID) {fem = (PetscFE) obj; ierr = PetscFECreateTabulation(fem, 1, 1, xir, 0, &T);CHKERRQ(ierr);}
  }
  ierr = DMGetCoordinatesLocal(dm, &coordsLocal);CHKERRQ(ierr);
  ierr = DMGetCoordinateDM(dm, &dmCoord);CHKERRQ(ierr);
  ierr = SNESCreate(PETSC_COMM_SELF, &snes);CHKERRQ(ierr);
  ierr = SNESSetOptionsPrefix(snes, "quad_interp_");CHKERRQ(ierr);
  ierr = VecCreate(PETSC_COMM_SELF, &r);CHKERRQ(ierr);
  ierr = VecSetSizes(r, 2, 2);CHKERRQ(ierr);
  ierr = VecSetType(r,dm->vectype);CHKERRQ(ierr);
  ierr = VecDuplicate(r, &ref);CHKERRQ(ierr);
  ierr = VecDuplicate(r, &real);CHKERRQ(ierr);
  ierr = MatCreate(PETSC_COMM_SELF, &J);CHKERRQ(ierr);
  ierr = MatSetSizes(J, 2, 2, 2, 2);CHKERRQ(ierr);
  ierr = MatSetType(J, MATSEQDENSE);CHKERRQ(ierr);
  ierr = MatSetUp(J);CHKERRQ(ierr);
  ierr = SNESSetFunction(snes, r, QuadMap_Private, NULL);CHKERRQ(ierr);
  ierr = SNESSetJacobian(snes, J, J, QuadJacobian_Private, NULL);CHKERRQ(ierr);
  ierr = SNESGetKSP(snes, &ksp);CHKERRQ(ierr);
  ierr = KSPGetPC(ksp, &pc);CHKERRQ(ierr);
  ierr = PCSetType(pc, PCLU);CHKERRQ(ierr);
  ierr = SNESSetFromOptions(snes);CHKERRQ(ierr);

  ierr = VecGetArrayRead(ctx->coords, &coords);CHKERRQ(ierr);
  ierr = VecGetArray(v, &a);CHKERRQ(ierr);
  for (p = 0; p < ctx->n; ++p) {
    PetscScalar *x = NULL, *vertices = NULL;
    PetscScalar *xi;
    PetscInt     c = ctx->cells[p], comp, coordSize, xSize;

    /* Can make this do all points at once */
    ierr = DMPlexVecGetClosure(dmCoord, NULL, coordsLocal, c, &coordSize, &vertices);CHKERRQ(ierr);
    PetscCheckFalse(4*2 != coordSize,ctx->comm, PETSC_ERR_ARG_SIZ, "Invalid closure size %D should be %d", coordSize, 4*2);
    ierr   = DMPlexVecGetClosure(dm, NULL, xLocal, c, &xSize, &x);CHKERRQ(ierr);
    ierr   = SNESSetFunction(snes, NULL, NULL, vertices);CHKERRQ(ierr);
    ierr   = SNESSetJacobian(snes, NULL, NULL, NULL, vertices);CHKERRQ(ierr);
    ierr   = VecGetArray(real, &xi);CHKERRQ(ierr);
    xi[0]  = coords[p*ctx->dim+0];
    xi[1]  = coords[p*ctx->dim+1];
    ierr   = VecRestoreArray(real, &xi);CHKERRQ(ierr);
    ierr   = SNESSolve(snes, real, ref);CHKERRQ(ierr);
    ierr   = VecGetArray(ref, &xi);CHKERRQ(ierr);
    xir[0] = PetscRealPart(xi[0]);
    xir[1] = PetscRealPart(xi[1]);
    if (4*dof == xSize) {
      for (comp = 0; comp < dof; ++comp)
        a[p*dof+comp] = x[0*dof+comp]*(1 - xir[0])*(1 - xir[1]) + x[1*dof+comp]*xir[0]*(1 - xir[1]) + x[2*dof+comp]*xir[0]*xir[1] + x[3*dof+comp]*(1 - xir[0])*xir[1];
    } else if (dof == xSize) {
      for (comp = 0; comp < dof; ++comp) a[p*dof+comp] = x[0*dof+comp];
    } else {
      PetscInt d;

      PetscCheck(fem, ctx->comm, PETSC_ERR_ARG_WRONG, "Cannot have a higher order interpolant if the discretization is not PetscFE");
      xir[0] = 2.0*xir[0] - 1.0; xir[1] = 2.0*xir[1] - 1.0;
      ierr = PetscFEComputeTabulation(fem, 1, xir, 0, T);CHKERRQ(ierr);
      for (comp = 0; comp < dof; ++comp) {
        a[p*dof+comp] = 0.0;
        for (d = 0; d < xSize/dof; ++d) {
          a[p*dof+comp] += x[d*dof+comp]*T->T[0][d*dof+comp];
        }
      }
    }
    ierr = VecRestoreArray(ref, &xi);CHKERRQ(ierr);
    ierr = DMPlexVecRestoreClosure(dmCoord, NULL, coordsLocal, c, &coordSize, &vertices);CHKERRQ(ierr);
    ierr = DMPlexVecRestoreClosure(dm, NULL, xLocal, c, &xSize, &x);CHKERRQ(ierr);
  }
  ierr = PetscTabulationDestroy(&T);CHKERRQ(ierr);
  ierr = VecRestoreArray(v, &a);CHKERRQ(ierr);
  ierr = VecRestoreArrayRead(ctx->coords, &coords);CHKERRQ(ierr);

  ierr = SNESDestroy(&snes);CHKERRQ(ierr);
  ierr = VecDestroy(&r);CHKERRQ(ierr);
  ierr = VecDestroy(&ref);CHKERRQ(ierr);
  ierr = VecDestroy(&real);CHKERRQ(ierr);
  ierr = MatDestroy(&J);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

static inline PetscErrorCode HexMap_Private(SNES snes, Vec Xref, Vec Xreal, void *ctx)
{
  const PetscScalar *vertices = (const PetscScalar*) ctx;
  const PetscScalar x0        = vertices[0];
  const PetscScalar y0        = vertices[1];
  const PetscScalar z0        = vertices[2];
  const PetscScalar x1        = vertices[9];
  const PetscScalar y1        = vertices[10];
  const PetscScalar z1        = vertices[11];
  const PetscScalar x2        = vertices[6];
  const PetscScalar y2        = vertices[7];
  const PetscScalar z2        = vertices[8];
  const PetscScalar x3        = vertices[3];
  const PetscScalar y3        = vertices[4];
  const PetscScalar z3        = vertices[5];
  const PetscScalar x4        = vertices[12];
  const PetscScalar y4        = vertices[13];
  const PetscScalar z4        = vertices[14];
  const PetscScalar x5        = vertices[15];
  const PetscScalar y5        = vertices[16];
  const PetscScalar z5        = vertices[17];
  const PetscScalar x6        = vertices[18];
  const PetscScalar y6        = vertices[19];
  const PetscScalar z6        = vertices[20];
  const PetscScalar x7        = vertices[21];
  const PetscScalar y7        = vertices[22];
  const PetscScalar z7        = vertices[23];
  const PetscScalar f_1       = x1 - x0;
  const PetscScalar g_1       = y1 - y0;
  const PetscScalar h_1       = z1 - z0;
  const PetscScalar f_3       = x3 - x0;
  const PetscScalar g_3       = y3 - y0;
  const PetscScalar h_3       = z3 - z0;
  const PetscScalar f_4       = x4 - x0;
  const PetscScalar g_4       = y4 - y0;
  const PetscScalar h_4       = z4 - z0;
  const PetscScalar f_01      = x2 - x1 - x3 + x0;
  const PetscScalar g_01      = y2 - y1 - y3 + y0;
  const PetscScalar h_01      = z2 - z1 - z3 + z0;
  const PetscScalar f_12      = x7 - x3 - x4 + x0;
  const PetscScalar g_12      = y7 - y3 - y4 + y0;
  const PetscScalar h_12      = z7 - z3 - z4 + z0;
  const PetscScalar f_02      = x5 - x1 - x4 + x0;
  const PetscScalar g_02      = y5 - y1 - y4 + y0;
  const PetscScalar h_02      = z5 - z1 - z4 + z0;
  const PetscScalar f_012     = x6 - x0 + x1 - x2 + x3 + x4 - x5 - x7;
  const PetscScalar g_012     = y6 - y0 + y1 - y2 + y3 + y4 - y5 - y7;
  const PetscScalar h_012     = z6 - z0 + z1 - z2 + z3 + z4 - z5 - z7;
  const PetscScalar *ref;
  PetscScalar       *real;
  PetscErrorCode    ierr;

  PetscFunctionBegin;
  ierr = VecGetArrayRead(Xref,  &ref);CHKERRQ(ierr);
  ierr = VecGetArray(Xreal, &real);CHKERRQ(ierr);
  {
    const PetscScalar p0 = ref[0];
    const PetscScalar p1 = ref[1];
    const PetscScalar p2 = ref[2];

    real[0] = x0 + f_1*p0 + f_3*p1 + f_4*p2 + f_01*p0*p1 + f_12*p1*p2 + f_02*p0*p2 + f_012*p0*p1*p2;
    real[1] = y0 + g_1*p0 + g_3*p1 + g_4*p2 + g_01*p0*p1 + g_01*p0*p1 + g_12*p1*p2 + g_02*p0*p2 + g_012*p0*p1*p2;
    real[2] = z0 + h_1*p0 + h_3*p1 + h_4*p2 + h_01*p0*p1 + h_01*p0*p1 + h_12*p1*p2 + h_02*p0*p2 + h_012*p0*p1*p2;
  }
  ierr = PetscLogFlops(114);CHKERRQ(ierr);
  ierr = VecRestoreArrayRead(Xref,  &ref);CHKERRQ(ierr);
  ierr = VecRestoreArray(Xreal, &real);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

static inline PetscErrorCode HexJacobian_Private(SNES snes, Vec Xref, Mat J, Mat M, void *ctx)
{
  const PetscScalar *vertices = (const PetscScalar*) ctx;
  const PetscScalar x0        = vertices[0];
  const PetscScalar y0        = vertices[1];
  const PetscScalar z0        = vertices[2];
  const PetscScalar x1        = vertices[9];
  const PetscScalar y1        = vertices[10];
  const PetscScalar z1        = vertices[11];
  const PetscScalar x2        = vertices[6];
  const PetscScalar y2        = vertices[7];
  const PetscScalar z2        = vertices[8];
  const PetscScalar x3        = vertices[3];
  const PetscScalar y3        = vertices[4];
  const PetscScalar z3        = vertices[5];
  const PetscScalar x4        = vertices[12];
  const PetscScalar y4        = vertices[13];
  const PetscScalar z4        = vertices[14];
  const PetscScalar x5        = vertices[15];
  const PetscScalar y5        = vertices[16];
  const PetscScalar z5        = vertices[17];
  const PetscScalar x6        = vertices[18];
  const PetscScalar y6        = vertices[19];
  const PetscScalar z6        = vertices[20];
  const PetscScalar x7        = vertices[21];
  const PetscScalar y7        = vertices[22];
  const PetscScalar z7        = vertices[23];
  const PetscScalar f_xy      = x2 - x1 - x3 + x0;
  const PetscScalar g_xy      = y2 - y1 - y3 + y0;
  const PetscScalar h_xy      = z2 - z1 - z3 + z0;
  const PetscScalar f_yz      = x7 - x3 - x4 + x0;
  const PetscScalar g_yz      = y7 - y3 - y4 + y0;
  const PetscScalar h_yz      = z7 - z3 - z4 + z0;
  const PetscScalar f_xz      = x5 - x1 - x4 + x0;
  const PetscScalar g_xz      = y5 - y1 - y4 + y0;
  const PetscScalar h_xz      = z5 - z1 - z4 + z0;
  const PetscScalar f_xyz     = x6 - x0 + x1 - x2 + x3 + x4 - x5 - x7;
  const PetscScalar g_xyz     = y6 - y0 + y1 - y2 + y3 + y4 - y5 - y7;
  const PetscScalar h_xyz     = z6 - z0 + z1 - z2 + z3 + z4 - z5 - z7;
  const PetscScalar *ref;
  PetscErrorCode    ierr;

  PetscFunctionBegin;
  ierr = VecGetArrayRead(Xref,  &ref);CHKERRQ(ierr);
  {
    const PetscScalar x       = ref[0];
    const PetscScalar y       = ref[1];
    const PetscScalar z       = ref[2];
    const PetscInt    rows[3] = {0, 1, 2};
    PetscScalar       values[9];

    values[0] = (x1 - x0 + f_xy*y + f_xz*z + f_xyz*y*z) / 2.0;
    values[1] = (x3 - x0 + f_xy*x + f_yz*z + f_xyz*x*z) / 2.0;
    values[2] = (x4 - x0 + f_yz*y + f_xz*x + f_xyz*x*y) / 2.0;
    values[3] = (y1 - y0 + g_xy*y + g_xz*z + g_xyz*y*z) / 2.0;
    values[4] = (y3 - y0 + g_xy*x + g_yz*z + g_xyz*x*z) / 2.0;
    values[5] = (y4 - y0 + g_yz*y + g_xz*x + g_xyz*x*y) / 2.0;
    values[6] = (z1 - z0 + h_xy*y + h_xz*z + h_xyz*y*z) / 2.0;
    values[7] = (z3 - z0 + h_xy*x + h_yz*z + h_xyz*x*z) / 2.0;
    values[8] = (z4 - z0 + h_yz*y + h_xz*x + h_xyz*x*y) / 2.0;

    ierr = MatSetValues(J, 3, rows, 3, rows, values, INSERT_VALUES);CHKERRQ(ierr);
  }
  ierr = PetscLogFlops(152);CHKERRQ(ierr);
  ierr = VecRestoreArrayRead(Xref,  &ref);CHKERRQ(ierr);
  ierr = MatAssemblyBegin(J, MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  ierr = MatAssemblyEnd(J, MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

static inline PetscErrorCode DMInterpolate_Hex_Private(DMInterpolationInfo ctx, DM dm, Vec xLocal, Vec v)
{
  DM             dmCoord;
  SNES           snes;
  KSP            ksp;
  PC             pc;
  Vec            coordsLocal, r, ref, real;
  Mat            J;
  const PetscScalar *coords;
  PetscScalar    *a;
  PetscInt       p;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = DMGetCoordinatesLocal(dm, &coordsLocal);CHKERRQ(ierr);
  ierr = DMGetCoordinateDM(dm, &dmCoord);CHKERRQ(ierr);
  ierr = SNESCreate(PETSC_COMM_SELF, &snes);CHKERRQ(ierr);
  ierr = SNESSetOptionsPrefix(snes, "hex_interp_");CHKERRQ(ierr);
  ierr = VecCreate(PETSC_COMM_SELF, &r);CHKERRQ(ierr);
  ierr = VecSetSizes(r, 3, 3);CHKERRQ(ierr);
  ierr = VecSetType(r,dm->vectype);CHKERRQ(ierr);
  ierr = VecDuplicate(r, &ref);CHKERRQ(ierr);
  ierr = VecDuplicate(r, &real);CHKERRQ(ierr);
  ierr = MatCreate(PETSC_COMM_SELF, &J);CHKERRQ(ierr);
  ierr = MatSetSizes(J, 3, 3, 3, 3);CHKERRQ(ierr);
  ierr = MatSetType(J, MATSEQDENSE);CHKERRQ(ierr);
  ierr = MatSetUp(J);CHKERRQ(ierr);
  ierr = SNESSetFunction(snes, r, HexMap_Private, NULL);CHKERRQ(ierr);
  ierr = SNESSetJacobian(snes, J, J, HexJacobian_Private, NULL);CHKERRQ(ierr);
  ierr = SNESGetKSP(snes, &ksp);CHKERRQ(ierr);
  ierr = KSPGetPC(ksp, &pc);CHKERRQ(ierr);
  ierr = PCSetType(pc, PCLU);CHKERRQ(ierr);
  ierr = SNESSetFromOptions(snes);CHKERRQ(ierr);

  ierr = VecGetArrayRead(ctx->coords, &coords);CHKERRQ(ierr);
  ierr = VecGetArray(v, &a);CHKERRQ(ierr);
  for (p = 0; p < ctx->n; ++p) {
    PetscScalar *x = NULL, *vertices = NULL;
    PetscScalar *xi;
    PetscReal    xir[3];
    PetscInt     c = ctx->cells[p], comp, coordSize, xSize;

    /* Can make this do all points at once */
    ierr = DMPlexVecGetClosure(dmCoord, NULL, coordsLocal, c, &coordSize, &vertices);CHKERRQ(ierr);
    PetscCheck(8*3 == coordSize,ctx->comm, PETSC_ERR_ARG_SIZ, "Invalid coordinate closure size %" PetscInt_FMT " should be %d", coordSize, 8*3);
    ierr = DMPlexVecGetClosure(dm, NULL, xLocal, c, &xSize, &x);CHKERRQ(ierr);
    PetscCheck((8*ctx->dof == xSize) || (ctx->dof == xSize),ctx->comm, PETSC_ERR_ARG_SIZ, "Invalid input closure size %" PetscInt_FMT " should be %" PetscInt_FMT " or %" PetscInt_FMT, xSize, 8*ctx->dof, ctx->dof);
    ierr   = SNESSetFunction(snes, NULL, NULL, vertices);CHKERRQ(ierr);
    ierr   = SNESSetJacobian(snes, NULL, NULL, NULL, vertices);CHKERRQ(ierr);
    ierr   = VecGetArray(real, &xi);CHKERRQ(ierr);
    xi[0]  = coords[p*ctx->dim+0];
    xi[1]  = coords[p*ctx->dim+1];
    xi[2]  = coords[p*ctx->dim+2];
    ierr   = VecRestoreArray(real, &xi);CHKERRQ(ierr);
    ierr   = SNESSolve(snes, real, ref);CHKERRQ(ierr);
    ierr   = VecGetArray(ref, &xi);CHKERRQ(ierr);
    xir[0] = PetscRealPart(xi[0]);
    xir[1] = PetscRealPart(xi[1]);
    xir[2] = PetscRealPart(xi[2]);
    if (8*ctx->dof == xSize) {
      for (comp = 0; comp < ctx->dof; ++comp) {
        a[p*ctx->dof+comp] =
          x[0*ctx->dof+comp]*(1-xir[0])*(1-xir[1])*(1-xir[2]) +
          x[3*ctx->dof+comp]*    xir[0]*(1-xir[1])*(1-xir[2]) +
          x[2*ctx->dof+comp]*    xir[0]*    xir[1]*(1-xir[2]) +
          x[1*ctx->dof+comp]*(1-xir[0])*    xir[1]*(1-xir[2]) +
          x[4*ctx->dof+comp]*(1-xir[0])*(1-xir[1])*   xir[2] +
          x[5*ctx->dof+comp]*    xir[0]*(1-xir[1])*   xir[2] +
          x[6*ctx->dof+comp]*    xir[0]*    xir[1]*   xir[2] +
          x[7*ctx->dof+comp]*(1-xir[0])*    xir[1]*   xir[2];
      }
    } else {
      for (comp = 0; comp < ctx->dof; ++comp) a[p*ctx->dof+comp] = x[0*ctx->dof+comp];
    }
    ierr = VecRestoreArray(ref, &xi);CHKERRQ(ierr);
    ierr = DMPlexVecRestoreClosure(dmCoord, NULL, coordsLocal, c, &coordSize, &vertices);CHKERRQ(ierr);
    ierr = DMPlexVecRestoreClosure(dm, NULL, xLocal, c, &xSize, &x);CHKERRQ(ierr);
  }
  ierr = VecRestoreArray(v, &a);CHKERRQ(ierr);
  ierr = VecRestoreArrayRead(ctx->coords, &coords);CHKERRQ(ierr);

  ierr = SNESDestroy(&snes);CHKERRQ(ierr);
  ierr = VecDestroy(&r);CHKERRQ(ierr);
  ierr = VecDestroy(&ref);CHKERRQ(ierr);
  ierr = VecDestroy(&real);CHKERRQ(ierr);
  ierr = MatDestroy(&J);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/*@C
  DMInterpolationEvaluate - Using the input from dm and x, calculates interpolated field values at the interpolation points.

  Input Parameters:
+ ctx - The DMInterpolationInfo context
. dm  - The DM
- x   - The local vector containing the field to be interpolated

  Output Parameters:
. v   - The vector containing the interpolated values

  Note: A suitable v can be obtained using DMInterpolationGetVector().

  Level: beginner

.seealso: DMInterpolationGetVector(), DMInterpolationAddPoints(), DMInterpolationCreate()
@*/
PetscErrorCode DMInterpolationEvaluate(DMInterpolationInfo ctx, DM dm, Vec x, Vec v)
{
  PetscDS        ds;
  PetscInt       n, p, Nf, field;
  PetscBool      useDS = PETSC_FALSE;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 2);
  PetscValidHeaderSpecific(x, VEC_CLASSID, 3);
  PetscValidHeaderSpecific(v, VEC_CLASSID, 4);
  ierr = VecGetLocalSize(v, &n);CHKERRQ(ierr);
  PetscCheckFalse(n != ctx->n*ctx->dof,ctx->comm, PETSC_ERR_ARG_SIZ, "Invalid input vector size %D should be %D", n, ctx->n*ctx->dof);
  if (!n) PetscFunctionReturn(0);
  ierr = DMGetDS(dm, &ds);CHKERRQ(ierr);
  if (ds) {
    useDS = PETSC_TRUE;
    ierr = PetscDSGetNumFields(ds, &Nf);CHKERRQ(ierr);
    for (field = 0; field < Nf; ++field) {
      PetscObject  obj;
      PetscClassId id;

      ierr = PetscDSGetDiscretization(ds, field, &obj);CHKERRQ(ierr);
      ierr = PetscObjectGetClassId(obj, &id);CHKERRQ(ierr);
      if (id != PETSCFE_CLASSID) {useDS = PETSC_FALSE; break;}
    }
  }
  if (useDS) {
    const PetscScalar *coords;
    PetscScalar       *interpolant;
    PetscInt           cdim, d;

    ierr = DMGetCoordinateDim(dm, &cdim);CHKERRQ(ierr);
    ierr = VecGetArrayRead(ctx->coords, &coords);CHKERRQ(ierr);
    ierr = VecGetArrayWrite(v, &interpolant);CHKERRQ(ierr);
    for (p = 0; p < ctx->n; ++p) {
      PetscReal    pcoords[3], xi[3];
      PetscScalar *xa   = NULL;
      PetscInt     coff = 0, foff = 0, clSize;

      if (ctx->cells[p] < 0) continue;
      for (d = 0; d < cdim; ++d) pcoords[d] = PetscRealPart(coords[p*cdim+d]);
      ierr = DMPlexCoordinatesToReference(dm, ctx->cells[p], 1, pcoords, xi);CHKERRQ(ierr);
      ierr = DMPlexVecGetClosure(dm, NULL, x, ctx->cells[p], &clSize, &xa);CHKERRQ(ierr);
      for (field = 0; field < Nf; ++field) {
        PetscTabulation T;
        PetscFE         fe;

        ierr = PetscDSGetDiscretization(ds, field, (PetscObject *) &fe);CHKERRQ(ierr);
        ierr = PetscFECreateTabulation(fe, 1, 1, xi, 0, &T);CHKERRQ(ierr);
        {
          const PetscReal *basis = T->T[0];
          const PetscInt   Nb    = T->Nb;
          const PetscInt   Nc    = T->Nc;
          PetscInt         f, fc;

          for (fc = 0; fc < Nc; ++fc) {
            interpolant[p*ctx->dof+coff+fc] = 0.0;
            for (f = 0; f < Nb; ++f) {
              interpolant[p*ctx->dof+coff+fc] += xa[foff+f]*basis[(0*Nb + f)*Nc + fc];
            }
          }
          coff += Nc;
          foff += Nb;
        }
        ierr = PetscTabulationDestroy(&T);CHKERRQ(ierr);
      }
      ierr = DMPlexVecRestoreClosure(dm, NULL, x, ctx->cells[p], &clSize, &xa);CHKERRQ(ierr);
      PetscCheckFalse(coff != ctx->dof,PETSC_COMM_SELF, PETSC_ERR_PLIB, "Total components %D != %D dof specified for interpolation", coff, ctx->dof);
      PetscCheckFalse(foff != clSize,PETSC_COMM_SELF, PETSC_ERR_PLIB, "Total FE space size %D != %D closure size", foff, clSize);
    }
    ierr = VecRestoreArrayRead(ctx->coords, &coords);CHKERRQ(ierr);
    ierr = VecRestoreArrayWrite(v, &interpolant);CHKERRQ(ierr);
  } else {
    DMPolytopeType ct;

    /* TODO Check each cell individually */
    ierr = DMPlexGetCellType(dm, ctx->cells[0], &ct);CHKERRQ(ierr);
    switch (ct) {
      case DM_POLYTOPE_SEGMENT:       ierr = DMInterpolate_Segment_Private(ctx, dm, x, v);CHKERRQ(ierr);break;
      case DM_POLYTOPE_TRIANGLE:      ierr = DMInterpolate_Triangle_Private(ctx, dm, x, v);CHKERRQ(ierr);break;
      case DM_POLYTOPE_QUADRILATERAL: ierr = DMInterpolate_Quad_Private(ctx, dm, x, v);CHKERRQ(ierr);break;
      case DM_POLYTOPE_TETRAHEDRON:   ierr = DMInterpolate_Tetrahedron_Private(ctx, dm, x, v);CHKERRQ(ierr);break;
      case DM_POLYTOPE_HEXAHEDRON:    ierr = DMInterpolate_Hex_Private(ctx, dm, x, v);CHKERRQ(ierr);break;
      default: SETERRQ(PETSC_COMM_SELF, PETSC_ERR_SUP, "No support for cell type %s", DMPolytopeTypes[PetscMax(0, PetscMin(ct, DM_NUM_POLYTOPES))]);
    }
  }
  PetscFunctionReturn(0);
}

/*@C
  DMInterpolationDestroy - Destroys a DMInterpolationInfo context

  Collective on ctx

  Input Parameter:
. ctx - the context

  Level: beginner

.seealso: DMInterpolationEvaluate(), DMInterpolationAddPoints(), DMInterpolationCreate()
@*/
PetscErrorCode DMInterpolationDestroy(DMInterpolationInfo *ctx)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  PetscValidPointer(ctx, 1);
  ierr = VecDestroy(&(*ctx)->coords);CHKERRQ(ierr);
  ierr = PetscFree((*ctx)->points);CHKERRQ(ierr);
  ierr = PetscFree((*ctx)->cells);CHKERRQ(ierr);
  ierr = PetscFree(*ctx);CHKERRQ(ierr);
  *ctx = NULL;
  PetscFunctionReturn(0);
}

/*@C
  SNESMonitorFields - Monitors the residual for each field separately

  Collective on SNES

  Input Parameters:
+ snes   - the SNES context
. its    - iteration number
. fgnorm - 2-norm of residual
- vf  - PetscViewerAndFormat of type ASCII

  Notes:
  This routine prints the residual norm at each iteration.

  Level: intermediate

.seealso: SNESMonitorSet(), SNESMonitorDefault()
@*/
PetscErrorCode SNESMonitorFields(SNES snes, PetscInt its, PetscReal fgnorm, PetscViewerAndFormat *vf)
{
  PetscViewer        viewer = vf->viewer;
  Vec                res;
  DM                 dm;
  PetscSection       s;
  const PetscScalar *r;
  PetscReal         *lnorms, *norms;
  PetscInt           numFields, f, pStart, pEnd, p;
  PetscErrorCode     ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(viewer,PETSC_VIEWER_CLASSID,4);
  ierr = SNESGetFunction(snes, &res, NULL, NULL);CHKERRQ(ierr);
  ierr = SNESGetDM(snes, &dm);CHKERRQ(ierr);
  ierr = DMGetLocalSection(dm, &s);CHKERRQ(ierr);
  ierr = PetscSectionGetNumFields(s, &numFields);CHKERRQ(ierr);
  ierr = PetscSectionGetChart(s, &pStart, &pEnd);CHKERRQ(ierr);
  ierr = PetscCalloc2(numFields, &lnorms, numFields, &norms);CHKERRQ(ierr);
  ierr = VecGetArrayRead(res, &r);CHKERRQ(ierr);
  for (p = pStart; p < pEnd; ++p) {
    for (f = 0; f < numFields; ++f) {
      PetscInt fdof, foff, d;

      ierr = PetscSectionGetFieldDof(s, p, f, &fdof);CHKERRQ(ierr);
      ierr = PetscSectionGetFieldOffset(s, p, f, &foff);CHKERRQ(ierr);
      for (d = 0; d < fdof; ++d) lnorms[f] += PetscRealPart(PetscSqr(r[foff+d]));
    }
  }
  ierr = VecRestoreArrayRead(res, &r);CHKERRQ(ierr);
  ierr = MPIU_Allreduce(lnorms, norms, numFields, MPIU_REAL, MPIU_SUM, PetscObjectComm((PetscObject) dm));CHKERRMPI(ierr);
  ierr = PetscViewerPushFormat(viewer,vf->format);CHKERRQ(ierr);
  ierr = PetscViewerASCIIAddTab(viewer, ((PetscObject) snes)->tablevel);CHKERRQ(ierr);
  ierr = PetscViewerASCIIPrintf(viewer, "%3D SNES Function norm %14.12e [", its, (double) fgnorm);CHKERRQ(ierr);
  for (f = 0; f < numFields; ++f) {
    if (f > 0) {ierr = PetscViewerASCIIPrintf(viewer, ", ");CHKERRQ(ierr);}
    ierr = PetscViewerASCIIPrintf(viewer, "%14.12e", (double) PetscSqrtReal(norms[f]));CHKERRQ(ierr);
  }
  ierr = PetscViewerASCIIPrintf(viewer, "]\n");CHKERRQ(ierr);
  ierr = PetscViewerASCIISubtractTab(viewer, ((PetscObject) snes)->tablevel);CHKERRQ(ierr);
  ierr = PetscViewerPopFormat(viewer);CHKERRQ(ierr);
  ierr = PetscFree2(lnorms, norms);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/********************* Residual Computation **************************/

PetscErrorCode DMPlexGetAllCells_Internal(DM plex, IS *cellIS)
{
  PetscInt       depth;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = DMPlexGetDepth(plex, &depth);CHKERRQ(ierr);
  ierr = DMGetStratumIS(plex, "dim", depth, cellIS);CHKERRQ(ierr);
  if (!*cellIS) {ierr = DMGetStratumIS(plex, "depth", depth, cellIS);CHKERRQ(ierr);}
  PetscFunctionReturn(0);
}

/*@
  DMPlexSNESComputeResidualFEM - Sums the local residual into vector F from the local input X using pointwise functions specified by the user

  Input Parameters:
+ dm - The mesh
. X  - Local solution
- user - The user context

  Output Parameter:
. F  - Local output vector

  Notes:
  The residual is summed into F; the caller is responsible for using VecZeroEntries() or otherwise ensuring that any data in F is intentional.

  Level: developer

.seealso: DMPlexComputeJacobianAction()
@*/
PetscErrorCode DMPlexSNESComputeResidualFEM(DM dm, Vec X, Vec F, void *user)
{
  DM             plex;
  IS             allcellIS;
  PetscInt       Nds, s;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = DMSNESConvertPlex(dm, &plex, PETSC_TRUE);CHKERRQ(ierr);
  ierr = DMPlexGetAllCells_Internal(plex, &allcellIS);CHKERRQ(ierr);
  ierr = DMGetNumDS(dm, &Nds);CHKERRQ(ierr);
  for (s = 0; s < Nds; ++s) {
    PetscDS          ds;
    IS               cellIS;
    PetscFormKey key;

    ierr = DMGetRegionNumDS(dm, s, &key.label, NULL, &ds);CHKERRQ(ierr);
    key.value = 0;
    key.field = 0;
    key.part  = 0;
    if (!key.label) {
      ierr = PetscObjectReference((PetscObject) allcellIS);CHKERRQ(ierr);
      cellIS = allcellIS;
    } else {
      IS pointIS;

      key.value = 1;
      ierr = DMLabelGetStratumIS(key.label, key.value, &pointIS);CHKERRQ(ierr);
      ierr = ISIntersect_Caching_Internal(allcellIS, pointIS, &cellIS);CHKERRQ(ierr);
      ierr = ISDestroy(&pointIS);CHKERRQ(ierr);
    }
    ierr = DMPlexComputeResidual_Internal(plex, key, cellIS, PETSC_MIN_REAL, X, NULL, 0.0, F, user);CHKERRQ(ierr);
    ierr = ISDestroy(&cellIS);CHKERRQ(ierr);
  }
  ierr = ISDestroy(&allcellIS);CHKERRQ(ierr);
  ierr = DMDestroy(&plex);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

PetscErrorCode DMSNESComputeResidual(DM dm, Vec X, Vec F, void *user)
{
  DM             plex;
  IS             allcellIS;
  PetscInt       Nds, s;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = DMSNESConvertPlex(dm, &plex, PETSC_TRUE);CHKERRQ(ierr);
  ierr = DMPlexGetAllCells_Internal(plex, &allcellIS);CHKERRQ(ierr);
  ierr = DMGetNumDS(dm, &Nds);CHKERRQ(ierr);
  for (s = 0; s < Nds; ++s) {
    PetscDS ds;
    DMLabel label;
    IS      cellIS;

    ierr = DMGetRegionNumDS(dm, s, &label, NULL, &ds);CHKERRQ(ierr);
    {
      PetscWeakFormKind resmap[2] = {PETSC_WF_F0, PETSC_WF_F1};
      PetscWeakForm     wf;
      PetscInt          Nm = 2, m, Nk = 0, k, kp, off = 0;
      PetscFormKey *reskeys;

      /* Get unique residual keys */
      for (m = 0; m < Nm; ++m) {
        PetscInt Nkm;
        ierr = PetscHMapFormGetSize(ds->wf->form[resmap[m]], &Nkm);CHKERRQ(ierr);
        Nk  += Nkm;
      }
      ierr = PetscMalloc1(Nk, &reskeys);CHKERRQ(ierr);
      for (m = 0; m < Nm; ++m) {
        ierr = PetscHMapFormGetKeys(ds->wf->form[resmap[m]], &off, reskeys);CHKERRQ(ierr);
      }
      PetscCheckFalse(off != Nk,PETSC_COMM_SELF, PETSC_ERR_ARG_SIZ, "Number of keys %D should be %D", off, Nk);
      ierr = PetscFormKeySort(Nk, reskeys);CHKERRQ(ierr);
      for (k = 0, kp = 1; kp < Nk; ++kp) {
        if ((reskeys[k].label != reskeys[kp].label) || (reskeys[k].value != reskeys[kp].value)) {
          ++k;
          if (kp != k) reskeys[k] = reskeys[kp];
        }
      }
      Nk = k;

      ierr = PetscDSGetWeakForm(ds, &wf);CHKERRQ(ierr);
      for (k = 0; k < Nk; ++k) {
        DMLabel  label = reskeys[k].label;
        PetscInt val   = reskeys[k].value;

        if (!label) {
          ierr = PetscObjectReference((PetscObject) allcellIS);CHKERRQ(ierr);
          cellIS = allcellIS;
        } else {
          IS pointIS;

          ierr = DMLabelGetStratumIS(label, val, &pointIS);CHKERRQ(ierr);
          ierr = ISIntersect_Caching_Internal(allcellIS, pointIS, &cellIS);CHKERRQ(ierr);
          ierr = ISDestroy(&pointIS);CHKERRQ(ierr);
        }
        ierr = DMPlexComputeResidual_Internal(plex, reskeys[k], cellIS, PETSC_MIN_REAL, X, NULL, 0.0, F, user);CHKERRQ(ierr);
        ierr = ISDestroy(&cellIS);CHKERRQ(ierr);
      }
      ierr = PetscFree(reskeys);CHKERRQ(ierr);
    }
  }
  ierr = ISDestroy(&allcellIS);CHKERRQ(ierr);
  ierr = DMDestroy(&plex);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/*@
  DMPlexSNESComputeBoundaryFEM - Form the boundary values for the local input X

  Input Parameters:
+ dm - The mesh
- user - The user context

  Output Parameter:
. X  - Local solution

  Level: developer

.seealso: DMPlexComputeJacobianAction()
@*/
PetscErrorCode DMPlexSNESComputeBoundaryFEM(DM dm, Vec X, void *user)
{
  DM             plex;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = DMSNESConvertPlex(dm,&plex,PETSC_TRUE);CHKERRQ(ierr);
  ierr = DMPlexInsertBoundaryValues(plex, PETSC_TRUE, X, PETSC_MIN_REAL, NULL, NULL, NULL);CHKERRQ(ierr);
  ierr = DMDestroy(&plex);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/*@
  DMSNESComputeJacobianAction - Compute the action of the Jacobian J(X) on Y

  Input Parameters:
+ dm   - The DM
. X    - Local solution vector
. Y    - Local input vector
- user - The user context

  Output Parameter:
. F    - lcoal output vector

  Level: developer

  Notes:
  Users will typically use DMSNESCreateJacobianMF() followed by MatMult() instead of calling this routine directly.

.seealso: DMSNESCreateJacobianMF(), DMPlexSNESComputeResidualFEM()
@*/
PetscErrorCode DMSNESComputeJacobianAction(DM dm, Vec X, Vec Y, Vec F, void *user)
{
  DM             plex;
  IS             allcellIS;
  PetscInt       Nds, s;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = DMSNESConvertPlex(dm, &plex, PETSC_TRUE);CHKERRQ(ierr);
  ierr = DMPlexGetAllCells_Internal(plex, &allcellIS);CHKERRQ(ierr);
  ierr = DMGetNumDS(dm, &Nds);CHKERRQ(ierr);
  for (s = 0; s < Nds; ++s) {
    PetscDS ds;
    DMLabel label;
    IS      cellIS;

    ierr = DMGetRegionNumDS(dm, s, &label, NULL, &ds);CHKERRQ(ierr);
    {
      PetscWeakFormKind jacmap[4] = {PETSC_WF_G0, PETSC_WF_G1, PETSC_WF_G2, PETSC_WF_G3};
      PetscWeakForm     wf;
      PetscInt          Nm = 4, m, Nk = 0, k, kp, off = 0;
      PetscFormKey *jackeys;

      /* Get unique Jacobian keys */
      for (m = 0; m < Nm; ++m) {
        PetscInt Nkm;
        ierr = PetscHMapFormGetSize(ds->wf->form[jacmap[m]], &Nkm);CHKERRQ(ierr);
        Nk  += Nkm;
      }
      ierr = PetscMalloc1(Nk, &jackeys);CHKERRQ(ierr);
      for (m = 0; m < Nm; ++m) {
        ierr = PetscHMapFormGetKeys(ds->wf->form[jacmap[m]], &off, jackeys);CHKERRQ(ierr);
      }
      PetscCheckFalse(off != Nk,PETSC_COMM_SELF, PETSC_ERR_ARG_SIZ, "Number of keys %D should be %D", off, Nk);
      ierr = PetscFormKeySort(Nk, jackeys);CHKERRQ(ierr);
      for (k = 0, kp = 1; kp < Nk; ++kp) {
        if ((jackeys[k].label != jackeys[kp].label) || (jackeys[k].value != jackeys[kp].value)) {
          ++k;
          if (kp != k) jackeys[k] = jackeys[kp];
        }
      }
      Nk = k;

      ierr = PetscDSGetWeakForm(ds, &wf);CHKERRQ(ierr);
      for (k = 0; k < Nk; ++k) {
        DMLabel  label = jackeys[k].label;
        PetscInt val   = jackeys[k].value;

        if (!label) {
          ierr = PetscObjectReference((PetscObject) allcellIS);CHKERRQ(ierr);
          cellIS = allcellIS;
        } else {
          IS pointIS;

          ierr = DMLabelGetStratumIS(label, val, &pointIS);CHKERRQ(ierr);
          ierr = ISIntersect_Caching_Internal(allcellIS, pointIS, &cellIS);CHKERRQ(ierr);
          ierr = ISDestroy(&pointIS);CHKERRQ(ierr);
        }
        ierr = DMPlexComputeJacobian_Action_Internal(plex, jackeys[k], cellIS, 0.0, 0.0, X, NULL, Y, F, user);CHKERRQ(ierr);
        ierr = ISDestroy(&cellIS);CHKERRQ(ierr);
      }
      ierr = PetscFree(jackeys);CHKERRQ(ierr);
    }
  }
  ierr = ISDestroy(&allcellIS);CHKERRQ(ierr);
  ierr = DMDestroy(&plex);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/*@
  DMPlexSNESComputeJacobianFEM - Form the local portion of the Jacobian matrix J at the local solution X using pointwise functions specified by the user.

  Input Parameters:
+ dm - The mesh
. X  - Local input vector
- user - The user context

  Output Parameter:
. Jac  - Jacobian matrix

  Note:
  We form the residual one batch of elements at a time. This allows us to offload work onto an accelerator,
  like a GPU, or vectorize on a multicore machine.

  Level: developer

.seealso: FormFunctionLocal()
@*/
PetscErrorCode DMPlexSNESComputeJacobianFEM(DM dm, Vec X, Mat Jac, Mat JacP,void *user)
{
  DM             plex;
  IS             allcellIS;
  PetscBool      hasJac, hasPrec;
  PetscInt       Nds, s;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = DMSNESConvertPlex(dm, &plex, PETSC_TRUE);CHKERRQ(ierr);
  ierr = DMPlexGetAllCells_Internal(plex, &allcellIS);CHKERRQ(ierr);
  ierr = DMGetNumDS(dm, &Nds);CHKERRQ(ierr);
  for (s = 0; s < Nds; ++s) {
    PetscDS          ds;
    IS               cellIS;
    PetscFormKey key;

    ierr = DMGetRegionNumDS(dm, s, &key.label, NULL, &ds);CHKERRQ(ierr);
    key.value = 0;
    key.field = 0;
    key.part  = 0;
    if (!key.label) {
      ierr = PetscObjectReference((PetscObject) allcellIS);CHKERRQ(ierr);
      cellIS = allcellIS;
    } else {
      IS pointIS;

      key.value = 1;
      ierr = DMLabelGetStratumIS(key.label, key.value, &pointIS);CHKERRQ(ierr);
      ierr = ISIntersect_Caching_Internal(allcellIS, pointIS, &cellIS);CHKERRQ(ierr);
      ierr = ISDestroy(&pointIS);CHKERRQ(ierr);
    }
    if (!s) {
      ierr = PetscDSHasJacobian(ds, &hasJac);CHKERRQ(ierr);
      ierr = PetscDSHasJacobianPreconditioner(ds, &hasPrec);CHKERRQ(ierr);
      if (hasJac && hasPrec) {ierr = MatZeroEntries(Jac);CHKERRQ(ierr);}
      ierr = MatZeroEntries(JacP);CHKERRQ(ierr);
    }
    ierr = DMPlexComputeJacobian_Internal(plex, key, cellIS, 0.0, 0.0, X, NULL, Jac, JacP, user);CHKERRQ(ierr);
    ierr = ISDestroy(&cellIS);CHKERRQ(ierr);
  }
  ierr = ISDestroy(&allcellIS);CHKERRQ(ierr);
  ierr = DMDestroy(&plex);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

struct _DMSNESJacobianMFCtx
{
  DM    dm;
  Vec   X;
  void *ctx;
};

static PetscErrorCode DMSNESJacobianMF_Destroy_Private(Mat A)
{
  struct _DMSNESJacobianMFCtx *ctx;
  PetscErrorCode               ierr;

  PetscFunctionBegin;
  ierr = MatShellGetContext(A, &ctx);CHKERRQ(ierr);
  ierr = MatShellSetContext(A, NULL);CHKERRQ(ierr);
  ierr = DMDestroy(&ctx->dm);CHKERRQ(ierr);
  ierr = VecDestroy(&ctx->X);CHKERRQ(ierr);
  ierr = PetscFree(ctx);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

static PetscErrorCode DMSNESJacobianMF_Mult_Private(Mat A, Vec Y, Vec Z)
{
  struct _DMSNESJacobianMFCtx *ctx;
  PetscErrorCode               ierr;

  PetscFunctionBegin;
  ierr = MatShellGetContext(A, &ctx);CHKERRQ(ierr);
  ierr = DMSNESComputeJacobianAction(ctx->dm, ctx->X, Y, Z, ctx->ctx);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/*@
  DMSNESCreateJacobianMF - Create a Mat which computes the action of the Jacobian matrix-free

  Collective on dm

  Input Parameters:
+ dm   - The DM
. X    - The evaluation point for the Jacobian
- user - A user context, or NULL

  Output Parameter:
. J    - The Mat

  Level: advanced

  Notes:
  Vec X is kept in Mat J, so updating X then updates the evaluation point.

.seealso: DMSNESComputeJacobianAction()
@*/
PetscErrorCode DMSNESCreateJacobianMF(DM dm, Vec X, void *user, Mat *J)
{
  struct _DMSNESJacobianMFCtx *ctx;
  PetscInt                     n, N;
  PetscErrorCode               ierr;

  PetscFunctionBegin;
  ierr = MatCreate(PetscObjectComm((PetscObject) dm), J);CHKERRQ(ierr);
  ierr = MatSetType(*J, MATSHELL);CHKERRQ(ierr);
  ierr = VecGetLocalSize(X, &n);CHKERRQ(ierr);
  ierr = VecGetSize(X, &N);CHKERRQ(ierr);
  ierr = MatSetSizes(*J, n, n, N, N);CHKERRQ(ierr);
  ierr = PetscObjectReference((PetscObject) dm);CHKERRQ(ierr);
  ierr = PetscObjectReference((PetscObject) X);CHKERRQ(ierr);
  ierr = PetscMalloc1(1, &ctx);CHKERRQ(ierr);
  ctx->dm  = dm;
  ctx->X   = X;
  ctx->ctx = user;
  ierr = MatShellSetContext(*J, ctx);CHKERRQ(ierr);
  ierr = MatShellSetOperation(*J, MATOP_DESTROY, (void (*)(void)) DMSNESJacobianMF_Destroy_Private);CHKERRQ(ierr);
  ierr = MatShellSetOperation(*J, MATOP_MULT,    (void (*)(void)) DMSNESJacobianMF_Mult_Private);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/*
     MatComputeNeumannOverlap - Computes an unassembled (Neumann) local overlapping Mat in nonlinear context.

   Input Parameters:
+     X - SNES linearization point
.     ovl - index set of overlapping subdomains

   Output Parameter:
.     J - unassembled (Neumann) local matrix

   Level: intermediate

.seealso: DMCreateNeumannOverlap(), MATIS, PCHPDDMSetAuxiliaryMat()
*/
static PetscErrorCode MatComputeNeumannOverlap_Plex(Mat J, PetscReal t, Vec X, Vec X_t, PetscReal s, IS ovl, void *ctx)
{
  SNES           snes;
  Mat            pJ;
  DM             ovldm,origdm;
  DMSNES         sdm;
  PetscErrorCode (*bfun)(DM,Vec,void*);
  PetscErrorCode (*jfun)(DM,Vec,Mat,Mat,void*);
  void           *bctx,*jctx;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = PetscObjectQuery((PetscObject)ovl,"_DM_Overlap_HPDDM_MATIS",(PetscObject*)&pJ);CHKERRQ(ierr);
  PetscCheckFalse(!pJ,PETSC_COMM_SELF,PETSC_ERR_PLIB,"Missing overlapping Mat");
  ierr = PetscObjectQuery((PetscObject)ovl,"_DM_Original_HPDDM",(PetscObject*)&origdm);CHKERRQ(ierr);
  PetscCheckFalse(!origdm,PETSC_COMM_SELF,PETSC_ERR_PLIB,"Missing original DM");
  ierr = MatGetDM(pJ,&ovldm);CHKERRQ(ierr);
  ierr = DMSNESGetBoundaryLocal(origdm,&bfun,&bctx);CHKERRQ(ierr);
  ierr = DMSNESSetBoundaryLocal(ovldm,bfun,bctx);CHKERRQ(ierr);
  ierr = DMSNESGetJacobianLocal(origdm,&jfun,&jctx);CHKERRQ(ierr);
  ierr = DMSNESSetJacobianLocal(ovldm,jfun,jctx);CHKERRQ(ierr);
  ierr = PetscObjectQuery((PetscObject)ovl,"_DM_Overlap_HPDDM_SNES",(PetscObject*)&snes);CHKERRQ(ierr);
  if (!snes) {
    ierr = SNESCreate(PetscObjectComm((PetscObject)ovl),&snes);CHKERRQ(ierr);
    ierr = SNESSetDM(snes,ovldm);CHKERRQ(ierr);
    ierr = PetscObjectCompose((PetscObject)ovl,"_DM_Overlap_HPDDM_SNES",(PetscObject)snes);CHKERRQ(ierr);
    ierr = PetscObjectDereference((PetscObject)snes);CHKERRQ(ierr);
  }
  ierr = DMGetDMSNES(ovldm,&sdm);CHKERRQ(ierr);
  ierr = VecLockReadPush(X);CHKERRQ(ierr);
  PetscStackPush("SNES user Jacobian function");
  ierr = (*sdm->ops->computejacobian)(snes,X,pJ,pJ,sdm->jacobianctx);CHKERRQ(ierr);
  PetscStackPop;
  ierr = VecLockReadPop(X);CHKERRQ(ierr);
  /* this is a no-hop, just in case we decide to change the placeholder for the local Neumann matrix */
  {
    Mat locpJ;

    ierr = MatISGetLocalMat(pJ,&locpJ);CHKERRQ(ierr);
    ierr = MatCopy(locpJ,J,SAME_NONZERO_PATTERN);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

/*@
  DMPlexSetSNESLocalFEM - Use DMPlex's internal FEM routines to compute SNES boundary values, residual, and Jacobian.

  Input Parameters:
+ dm - The DM object
. boundaryctx - the user context that will be passed to pointwise evaluation of boundary values (see PetscDSAddBoundary())
. residualctx - the user context that will be passed to pointwise evaluation of finite element residual computations (see PetscDSSetResidual())
- jacobianctx - the user context that will be passed to pointwise evaluation of finite element Jacobian construction (see PetscDSSetJacobian())

  Level: developer
@*/
PetscErrorCode DMPlexSetSNESLocalFEM(DM dm, void *boundaryctx, void *residualctx, void *jacobianctx)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = DMSNESSetBoundaryLocal(dm,DMPlexSNESComputeBoundaryFEM,boundaryctx);CHKERRQ(ierr);
  ierr = DMSNESSetFunctionLocal(dm,DMPlexSNESComputeResidualFEM,residualctx);CHKERRQ(ierr);
  ierr = DMSNESSetJacobianLocal(dm,DMPlexSNESComputeJacobianFEM,jacobianctx);CHKERRQ(ierr);
  ierr = PetscObjectComposeFunction((PetscObject)dm,"MatComputeNeumannOverlap_C",MatComputeNeumannOverlap_Plex);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/*@C
  DMSNESCheckDiscretization - Check the discretization error of the exact solution

  Input Parameters:
+ snes - the SNES object
. dm   - the DM
. t    - the time
. u    - a DM vector
- tol  - A tolerance for the check, or -1 to print the results instead

  Output Parameters:
. error - An array which holds the discretization error in each field, or NULL

  Note: The user must call PetscDSSetExactSolution() beforehand

  Level: developer

.seealso: DNSNESCheckFromOptions(), DMSNESCheckResidual(), DMSNESCheckJacobian(), PetscDSSetExactSolution()
@*/
PetscErrorCode DMSNESCheckDiscretization(SNES snes, DM dm, PetscReal t, Vec u, PetscReal tol, PetscReal error[])
{
  PetscErrorCode (**exacts)(PetscInt, PetscReal, const PetscReal x[], PetscInt, PetscScalar *u, void *ctx);
  void            **ectxs;
  PetscReal        *err;
  MPI_Comm          comm;
  PetscInt          Nf, f;
  PetscErrorCode    ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(snes, SNES_CLASSID, 1);
  PetscValidHeaderSpecific(dm, DM_CLASSID, 2);
  PetscValidHeaderSpecific(u, VEC_CLASSID, 4);
  if (error) PetscValidRealPointer(error, 6);

  ierr = DMComputeExactSolution(dm, t, u, NULL);CHKERRQ(ierr);
  ierr = VecViewFromOptions(u, NULL, "-vec_view");CHKERRQ(ierr);

  ierr = PetscObjectGetComm((PetscObject) snes, &comm);CHKERRQ(ierr);
  ierr = DMGetNumFields(dm, &Nf);CHKERRQ(ierr);
  ierr = PetscCalloc3(Nf, &exacts, Nf, &ectxs, PetscMax(1, Nf), &err);CHKERRQ(ierr);
  {
    PetscInt Nds, s;

    ierr = DMGetNumDS(dm, &Nds);CHKERRQ(ierr);
    for (s = 0; s < Nds; ++s) {
      PetscDS         ds;
      DMLabel         label;
      IS              fieldIS;
      const PetscInt *fields;
      PetscInt        dsNf, f;

      ierr = DMGetRegionNumDS(dm, s, &label, &fieldIS, &ds);CHKERRQ(ierr);
      ierr = PetscDSGetNumFields(ds, &dsNf);CHKERRQ(ierr);
      ierr = ISGetIndices(fieldIS, &fields);CHKERRQ(ierr);
      for (f = 0; f < dsNf; ++f) {
        const PetscInt field = fields[f];
        ierr = PetscDSGetExactSolution(ds, field, &exacts[field], &ectxs[field]);CHKERRQ(ierr);
      }
      ierr = ISRestoreIndices(fieldIS, &fields);CHKERRQ(ierr);
    }
  }
  if (Nf > 1) {
    ierr = DMComputeL2FieldDiff(dm, t, exacts, ectxs, u, err);CHKERRQ(ierr);
    if (tol >= 0.0) {
      for (f = 0; f < Nf; ++f) {
        PetscCheckFalse(err[f] > tol,comm, PETSC_ERR_ARG_WRONG, "L_2 Error %g for field %D exceeds tolerance %g", (double) err[f], f, (double) tol);
      }
    } else if (error) {
      for (f = 0; f < Nf; ++f) error[f] = err[f];
    } else {
      ierr = PetscPrintf(comm, "L_2 Error: [");CHKERRQ(ierr);
      for (f = 0; f < Nf; ++f) {
        if (f) {ierr = PetscPrintf(comm, ", ");CHKERRQ(ierr);}
        ierr = PetscPrintf(comm, "%g", (double)err[f]);CHKERRQ(ierr);
      }
      ierr = PetscPrintf(comm, "]\n");CHKERRQ(ierr);
    }
  } else {
    ierr = DMComputeL2Diff(dm, t, exacts, ectxs, u, &err[0]);CHKERRQ(ierr);
    if (tol >= 0.0) {
      PetscCheckFalse(err[0] > tol,comm, PETSC_ERR_ARG_WRONG, "L_2 Error %g exceeds tolerance %g", (double) err[0], (double) tol);
    } else if (error) {
      error[0] = err[0];
    } else {
      ierr = PetscPrintf(comm, "L_2 Error: %g\n", (double) err[0]);CHKERRQ(ierr);
    }
  }
  ierr = PetscFree3(exacts, ectxs, err);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/*@C
  DMSNESCheckResidual - Check the residual of the exact solution

  Input Parameters:
+ snes - the SNES object
. dm   - the DM
. u    - a DM vector
- tol  - A tolerance for the check, or -1 to print the results instead

  Output Parameters:
. residual - The residual norm of the exact solution, or NULL

  Level: developer

.seealso: DNSNESCheckFromOptions(), DMSNESCheckDiscretization(), DMSNESCheckJacobian()
@*/
PetscErrorCode DMSNESCheckResidual(SNES snes, DM dm, Vec u, PetscReal tol, PetscReal *residual)
{
  MPI_Comm       comm;
  Vec            r;
  PetscReal      res;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(snes, SNES_CLASSID, 1);
  PetscValidHeaderSpecific(dm, DM_CLASSID, 2);
  PetscValidHeaderSpecific(u, VEC_CLASSID, 3);
  if (residual) PetscValidRealPointer(residual, 5);
  ierr = PetscObjectGetComm((PetscObject) snes, &comm);CHKERRQ(ierr);
  ierr = DMComputeExactSolution(dm, 0.0, u, NULL);CHKERRQ(ierr);
  ierr = VecDuplicate(u, &r);CHKERRQ(ierr);
  ierr = SNESComputeFunction(snes, u, r);CHKERRQ(ierr);
  ierr = VecNorm(r, NORM_2, &res);CHKERRQ(ierr);
  if (tol >= 0.0) {
    PetscCheckFalse(res > tol,comm, PETSC_ERR_ARG_WRONG, "L_2 Residual %g exceeds tolerance %g", (double) res, (double) tol);
  } else if (residual) {
    *residual = res;
  } else {
    ierr = PetscPrintf(comm, "L_2 Residual: %g\n", (double)res);CHKERRQ(ierr);
    ierr = VecChop(r, 1.0e-10);CHKERRQ(ierr);
    ierr = PetscObjectSetName((PetscObject) r, "Initial Residual");CHKERRQ(ierr);
    ierr = PetscObjectSetOptionsPrefix((PetscObject)r,"res_");CHKERRQ(ierr);
    ierr = VecViewFromOptions(r, NULL, "-vec_view");CHKERRQ(ierr);
  }
  ierr = VecDestroy(&r);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/*@C
  DMSNESCheckJacobian - Check the Jacobian of the exact solution against the residual using the Taylor Test

  Input Parameters:
+ snes - the SNES object
. dm   - the DM
. u    - a DM vector
- tol  - A tolerance for the check, or -1 to print the results instead

  Output Parameters:
+ isLinear - Flag indicaing that the function looks linear, or NULL
- convRate - The rate of convergence of the linear model, or NULL

  Level: developer

.seealso: DNSNESCheckFromOptions(), DMSNESCheckDiscretization(), DMSNESCheckResidual()
@*/
PetscErrorCode DMSNESCheckJacobian(SNES snes, DM dm, Vec u, PetscReal tol, PetscBool *isLinear, PetscReal *convRate)
{
  MPI_Comm       comm;
  PetscDS        ds;
  Mat            J, M;
  MatNullSpace   nullspace;
  PetscReal      slope, intercept;
  PetscBool      hasJac, hasPrec, isLin = PETSC_FALSE;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(snes, SNES_CLASSID, 1);
  PetscValidHeaderSpecific(dm, DM_CLASSID, 2);
  PetscValidHeaderSpecific(u, VEC_CLASSID, 3);
  if (isLinear) PetscValidBoolPointer(isLinear, 5);
  if (convRate) PetscValidRealPointer(convRate, 6);
  ierr = PetscObjectGetComm((PetscObject) snes, &comm);CHKERRQ(ierr);
  ierr = DMComputeExactSolution(dm, 0.0, u, NULL);CHKERRQ(ierr);
  /* Create and view matrices */
  ierr = DMCreateMatrix(dm, &J);CHKERRQ(ierr);
  ierr = DMGetDS(dm, &ds);CHKERRQ(ierr);
  ierr = PetscDSHasJacobian(ds, &hasJac);CHKERRQ(ierr);
  ierr = PetscDSHasJacobianPreconditioner(ds, &hasPrec);CHKERRQ(ierr);
  if (hasJac && hasPrec) {
    ierr = DMCreateMatrix(dm, &M);CHKERRQ(ierr);
    ierr = SNESComputeJacobian(snes, u, J, M);CHKERRQ(ierr);
    ierr = PetscObjectSetName((PetscObject) M, "Preconditioning Matrix");CHKERRQ(ierr);
    ierr = PetscObjectSetOptionsPrefix((PetscObject) M, "jacpre_");CHKERRQ(ierr);
    ierr = MatViewFromOptions(M, NULL, "-mat_view");CHKERRQ(ierr);
    ierr = MatDestroy(&M);CHKERRQ(ierr);
  } else {
    ierr = SNESComputeJacobian(snes, u, J, J);CHKERRQ(ierr);
  }
  ierr = PetscObjectSetName((PetscObject) J, "Jacobian");CHKERRQ(ierr);
  ierr = PetscObjectSetOptionsPrefix((PetscObject) J, "jac_");CHKERRQ(ierr);
  ierr = MatViewFromOptions(J, NULL, "-mat_view");CHKERRQ(ierr);
  /* Check nullspace */
  ierr = MatGetNullSpace(J, &nullspace);CHKERRQ(ierr);
  if (nullspace) {
    PetscBool isNull;
    ierr = MatNullSpaceTest(nullspace, J, &isNull);CHKERRQ(ierr);
    PetscCheckFalse(!isNull,comm, PETSC_ERR_PLIB, "The null space calculated for the system operator is invalid.");
  }
  /* Taylor test */
  {
    PetscRandom rand;
    Vec         du, uhat, r, rhat, df;
    PetscReal   h;
    PetscReal  *es, *hs, *errors;
    PetscReal   hMax = 1.0, hMin = 1e-6, hMult = 0.1;
    PetscInt    Nv, v;

    /* Choose a perturbation direction */
    ierr = PetscRandomCreate(comm, &rand);CHKERRQ(ierr);
    ierr = VecDuplicate(u, &du);CHKERRQ(ierr);
    ierr = VecSetRandom(du, rand);CHKERRQ(ierr);
    ierr = PetscRandomDestroy(&rand);CHKERRQ(ierr);
    ierr = VecDuplicate(u, &df);CHKERRQ(ierr);
    ierr = MatMult(J, du, df);CHKERRQ(ierr);
    /* Evaluate residual at u, F(u), save in vector r */
    ierr = VecDuplicate(u, &r);CHKERRQ(ierr);
    ierr = SNESComputeFunction(snes, u, r);CHKERRQ(ierr);
    /* Look at the convergence of our Taylor approximation as we approach u */
    for (h = hMax, Nv = 0; h >= hMin; h *= hMult, ++Nv);
    ierr = PetscCalloc3(Nv, &es, Nv, &hs, Nv, &errors);CHKERRQ(ierr);
    ierr = VecDuplicate(u, &uhat);CHKERRQ(ierr);
    ierr = VecDuplicate(u, &rhat);CHKERRQ(ierr);
    for (h = hMax, Nv = 0; h >= hMin; h *= hMult, ++Nv) {
      ierr = VecWAXPY(uhat, h, du, u);CHKERRQ(ierr);
      /* F(\hat u) \approx F(u) + J(u) (uhat - u) = F(u) + h * J(u) du */
      ierr = SNESComputeFunction(snes, uhat, rhat);CHKERRQ(ierr);
      ierr = VecAXPBYPCZ(rhat, -1.0, -h, 1.0, r, df);CHKERRQ(ierr);
      ierr = VecNorm(rhat, NORM_2, &errors[Nv]);CHKERRQ(ierr);

      es[Nv] = PetscLog10Real(errors[Nv]);
      hs[Nv] = PetscLog10Real(h);
    }
    ierr = VecDestroy(&uhat);CHKERRQ(ierr);
    ierr = VecDestroy(&rhat);CHKERRQ(ierr);
    ierr = VecDestroy(&df);CHKERRQ(ierr);
    ierr = VecDestroy(&r);CHKERRQ(ierr);
    ierr = VecDestroy(&du);CHKERRQ(ierr);
    for (v = 0; v < Nv; ++v) {
      if ((tol >= 0) && (errors[v] > tol)) break;
      else if (errors[v] > PETSC_SMALL)    break;
    }
    if (v == Nv) isLin = PETSC_TRUE;
    ierr = PetscLinearRegression(Nv, hs, es, &slope, &intercept);CHKERRQ(ierr);
    ierr = PetscFree3(es, hs, errors);CHKERRQ(ierr);
    /* Slope should be about 2 */
    if (tol >= 0) {
      PetscCheckFalse(!isLin && PetscAbsReal(2 - slope) > tol,comm, PETSC_ERR_ARG_WRONG, "Taylor approximation convergence rate should be 2, not %0.2f", (double) slope);
    } else if (isLinear || convRate) {
      if (isLinear) *isLinear = isLin;
      if (convRate) *convRate = slope;
    } else {
      if (!isLin) {ierr = PetscPrintf(comm, "Taylor approximation converging at order %3.2f\n", (double) slope);CHKERRQ(ierr);}
      else        {ierr = PetscPrintf(comm, "Function appears to be linear\n");CHKERRQ(ierr);}
    }
  }
  ierr = MatDestroy(&J);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

PetscErrorCode DMSNESCheck_Internal(SNES snes, DM dm, Vec u)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = DMSNESCheckDiscretization(snes, dm, 0.0, u, -1.0, NULL);CHKERRQ(ierr);
  ierr = DMSNESCheckResidual(snes, dm, u, -1.0, NULL);CHKERRQ(ierr);
  ierr = DMSNESCheckJacobian(snes, dm, u, -1.0, NULL, NULL);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/*@C
  DMSNESCheckFromOptions - Check the residual and Jacobian functions using the exact solution by outputting some diagnostic information

  Input Parameters:
+ snes - the SNES object
- u    - representative SNES vector

  Note: The user must call PetscDSSetExactSolution() beforehand

  Level: developer
@*/
PetscErrorCode DMSNESCheckFromOptions(SNES snes, Vec u)
{
  DM             dm;
  Vec            sol;
  PetscBool      check;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = PetscOptionsHasName(((PetscObject)snes)->options,((PetscObject)snes)->prefix, "-dmsnes_check", &check);CHKERRQ(ierr);
  if (!check) PetscFunctionReturn(0);
  ierr = SNESGetDM(snes, &dm);CHKERRQ(ierr);
  ierr = VecDuplicate(u, &sol);CHKERRQ(ierr);
  ierr = SNESSetSolution(snes, sol);CHKERRQ(ierr);
  ierr = DMSNESCheck_Internal(snes, dm, sol);CHKERRQ(ierr);
  ierr = VecDestroy(&sol);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}
