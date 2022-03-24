#include <petsc/private/dmpleximpl.h>   /*I      "petscdmplex.h"   I*/

static PetscErrorCode DMPlexLabelToVolumeConstraint(DM dm, DMLabel adaptLabel, PetscInt cStart, PetscInt cEnd, PetscReal refRatio, PetscReal maxVolumes[])
{
  PetscInt       dim, c;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = DMGetDimension(dm, &dim);CHKERRQ(ierr);
  refRatio = refRatio == PETSC_DEFAULT ? (PetscReal) ((PetscInt) 1 << dim) : refRatio;
  for (c = cStart; c < cEnd; c++) {
    PetscReal vol;
    PetscInt  closureSize = 0, cl;
    PetscInt *closure     = NULL;
    PetscBool anyRefine   = PETSC_FALSE;
    PetscBool anyCoarsen  = PETSC_FALSE;
    PetscBool anyKeep     = PETSC_FALSE;

    ierr = DMPlexComputeCellGeometryFVM(dm, c, &vol, NULL, NULL);CHKERRQ(ierr);
    maxVolumes[c - cStart] = vol;
    ierr = DMPlexGetTransitiveClosure(dm, c, PETSC_TRUE, &closureSize, &closure);CHKERRQ(ierr);
    for (cl = 0; cl < closureSize*2; cl += 2) {
      const PetscInt point = closure[cl];
      PetscInt       refFlag;

      ierr = DMLabelGetValue(adaptLabel, point, &refFlag);CHKERRQ(ierr);
      switch (refFlag) {
      case DM_ADAPT_REFINE:
        anyRefine  = PETSC_TRUE;break;
      case DM_ADAPT_COARSEN:
        anyCoarsen = PETSC_TRUE;break;
      case DM_ADAPT_KEEP:
        anyKeep    = PETSC_TRUE;break;
      case DM_ADAPT_DETERMINE:
        break;
      default:
        SETERRQ(PETSC_COMM_SELF,PETSC_ERR_SUP, "DMPlex does not support refinement flag %D", refFlag);
      }
      if (anyRefine) break;
    }
    ierr = DMPlexRestoreTransitiveClosure(dm, c, PETSC_TRUE, &closureSize, &closure);CHKERRQ(ierr);
    if (anyRefine) {
      maxVolumes[c - cStart] = vol / refRatio;
    } else if (anyKeep) {
      maxVolumes[c - cStart] = vol;
    } else if (anyCoarsen) {
      maxVolumes[c - cStart] = vol * refRatio;
    }
  }
  PetscFunctionReturn(0);
}

static PetscErrorCode DMPlexLabelToMetricConstraint(DM dm, DMLabel adaptLabel, PetscInt cStart, PetscInt cEnd, PetscInt vStart, PetscInt vEnd, PetscReal refRatio, Vec *metricVec)
{
  DM              udm, coordDM;
  PetscSection    coordSection;
  Vec             coordinates, mb, mx;
  Mat             A;
  PetscScalar    *metric, *eqns;
  const PetscReal coarseRatio = refRatio == PETSC_DEFAULT ? PetscSqr(0.5) : 1/refRatio;
  PetscInt        dim, Nv, Neq, c, v;
  PetscErrorCode  ierr;

  PetscFunctionBegin;
  ierr = DMPlexUninterpolate(dm, &udm);CHKERRQ(ierr);
  ierr = DMGetDimension(dm, &dim);CHKERRQ(ierr);
  ierr = DMGetCoordinateDM(dm, &coordDM);CHKERRQ(ierr);
  ierr = DMGetLocalSection(coordDM, &coordSection);CHKERRQ(ierr);
  ierr = DMGetCoordinatesLocal(dm, &coordinates);CHKERRQ(ierr);
  Nv   = vEnd - vStart;
  ierr = VecCreateSeq(PETSC_COMM_SELF, Nv*PetscSqr(dim), metricVec);CHKERRQ(ierr);
  ierr = VecGetArray(*metricVec, &metric);CHKERRQ(ierr);
  Neq  = (dim*(dim+1))/2;
  ierr = PetscMalloc1(PetscSqr(Neq), &eqns);CHKERRQ(ierr);
  ierr = MatCreateSeqDense(PETSC_COMM_SELF, Neq, Neq, eqns, &A);CHKERRQ(ierr);
  ierr = MatCreateVecs(A, &mx, &mb);CHKERRQ(ierr);
  ierr = VecSet(mb, 1.0);CHKERRQ(ierr);
  for (c = cStart; c < cEnd; ++c) {
    const PetscScalar *sol;
    PetscScalar       *cellCoords = NULL;
    PetscReal          e[3], vol;
    const PetscInt    *cone;
    PetscInt           coneSize, cl, i, j, d, r;

    ierr = DMPlexVecGetClosure(dm, coordSection, coordinates, c, NULL, &cellCoords);CHKERRQ(ierr);
    /* Only works for simplices */
    for (i = 0, r = 0; i < dim+1; ++i) {
      for (j = 0; j < i; ++j, ++r) {
        for (d = 0; d < dim; ++d) e[d] = PetscRealPart(cellCoords[i*dim+d] - cellCoords[j*dim+d]);
        /* FORTRAN ORDERING */
        switch (dim) {
        case 2:
          eqns[0*Neq+r] = PetscSqr(e[0]);
          eqns[1*Neq+r] = 2.0*e[0]*e[1];
          eqns[2*Neq+r] = PetscSqr(e[1]);
          break;
        case 3:
          eqns[0*Neq+r] = PetscSqr(e[0]);
          eqns[1*Neq+r] = 2.0*e[0]*e[1];
          eqns[2*Neq+r] = 2.0*e[0]*e[2];
          eqns[3*Neq+r] = PetscSqr(e[1]);
          eqns[4*Neq+r] = 2.0*e[1]*e[2];
          eqns[5*Neq+r] = PetscSqr(e[2]);
          break;
        }
      }
    }
    ierr = MatSetUnfactored(A);CHKERRQ(ierr);
    ierr = DMPlexVecRestoreClosure(dm, coordSection, coordinates, c, NULL, &cellCoords);CHKERRQ(ierr);
    ierr = MatLUFactor(A, NULL, NULL, NULL);CHKERRQ(ierr);
    ierr = MatSolve(A, mb, mx);CHKERRQ(ierr);
    ierr = VecGetArrayRead(mx, &sol);CHKERRQ(ierr);
    ierr = DMPlexComputeCellGeometryFVM(dm, c, &vol, NULL, NULL);CHKERRQ(ierr);
    ierr = DMPlexGetCone(udm, c, &cone);CHKERRQ(ierr);
    ierr = DMPlexGetConeSize(udm, c, &coneSize);CHKERRQ(ierr);
    for (cl = 0; cl < coneSize; ++cl) {
      const PetscInt v = cone[cl] - vStart;

      if (dim == 2) {
        metric[v*4+0] += vol*coarseRatio*sol[0];
        metric[v*4+1] += vol*coarseRatio*sol[1];
        metric[v*4+2] += vol*coarseRatio*sol[1];
        metric[v*4+3] += vol*coarseRatio*sol[2];
      } else {
        metric[v*9+0] += vol*coarseRatio*sol[0];
        metric[v*9+1] += vol*coarseRatio*sol[1];
        metric[v*9+3] += vol*coarseRatio*sol[1];
        metric[v*9+2] += vol*coarseRatio*sol[2];
        metric[v*9+6] += vol*coarseRatio*sol[2];
        metric[v*9+4] += vol*coarseRatio*sol[3];
        metric[v*9+5] += vol*coarseRatio*sol[4];
        metric[v*9+7] += vol*coarseRatio*sol[4];
        metric[v*9+8] += vol*coarseRatio*sol[5];
      }
    }
    ierr = VecRestoreArrayRead(mx, &sol);CHKERRQ(ierr);
  }
  for (v = 0; v < Nv; ++v) {
    const PetscInt *support;
    PetscInt        supportSize, s;
    PetscReal       vol, totVol = 0.0;

    ierr = DMPlexGetSupport(udm, v+vStart, &support);CHKERRQ(ierr);
    ierr = DMPlexGetSupportSize(udm, v+vStart, &supportSize);CHKERRQ(ierr);
    for (s = 0; s < supportSize; ++s) {ierr = DMPlexComputeCellGeometryFVM(dm, support[s], &vol, NULL, NULL);CHKERRQ(ierr); totVol += vol;}
    for (s = 0; s < PetscSqr(dim); ++s) metric[v*PetscSqr(dim)+s] /= totVol;
  }
  ierr = PetscFree(eqns);CHKERRQ(ierr);
  ierr = VecRestoreArray(*metricVec, &metric);CHKERRQ(ierr);
  ierr = VecDestroy(&mx);CHKERRQ(ierr);
  ierr = VecDestroy(&mb);CHKERRQ(ierr);
  ierr = MatDestroy(&A);CHKERRQ(ierr);
  ierr = DMDestroy(&udm);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/*
   Contains the list of registered DMPlexGenerators routines
*/
PetscErrorCode DMPlexRefine_Internal(DM dm, PETSC_UNUSED Vec metric, DMLabel adaptLabel, PETSC_UNUSED DMLabel rgLabel, DM *dmRefined)
{
  DMGeneratorFunctionList fl;
  PetscErrorCode        (*refine)(DM,PetscReal*,DM*);
  PetscErrorCode        (*adapt)(DM,Vec,DMLabel,DMLabel,DM*);
  PetscErrorCode        (*refinementFunc)(const PetscReal [], PetscReal *);
  char                    genname[PETSC_MAX_PATH_LEN], *name = NULL;
  PetscReal               refinementLimit;
  PetscReal              *maxVolumes;
  PetscInt                dim, cStart, cEnd, c;
  PetscBool               flg, flg2, localized;
  PetscErrorCode          ierr;

  PetscFunctionBegin;
  ierr = DMGetCoordinatesLocalized(dm, &localized);CHKERRQ(ierr);
  ierr = DMPlexGetRefinementLimit(dm, &refinementLimit);CHKERRQ(ierr);
  ierr = DMPlexGetRefinementFunction(dm, &refinementFunc);CHKERRQ(ierr);
  if (refinementLimit == 0.0 && !refinementFunc && !adaptLabel) PetscFunctionReturn(0);
  ierr = DMGetDimension(dm, &dim);CHKERRQ(ierr);
  ierr = DMPlexGetHeightStratum(dm, 0, &cStart, &cEnd);CHKERRQ(ierr);
  ierr = PetscOptionsGetString(((PetscObject) dm)->options,((PetscObject) dm)->prefix, "-dm_adaptor", genname, sizeof(genname), &flg);CHKERRQ(ierr);
  if (flg) name = genname;
  else {
    ierr = PetscOptionsGetString(((PetscObject) dm)->options,((PetscObject) dm)->prefix, "-dm_generator", genname, sizeof(genname), &flg2);CHKERRQ(ierr);
    if (flg2) name = genname;
  }

  fl = DMGenerateList;
  if (name) {
    while (fl) {
      ierr = PetscStrcmp(fl->name,name,&flg);CHKERRQ(ierr);
      if (flg) {
        refine = fl->refine;
        adapt  = fl->adapt;
        goto gotit;
      }
      fl = fl->next;
    }
    SETERRQ(PETSC_COMM_SELF,PETSC_ERR_ARG_OUTOFRANGE,"Grid refiner %s not registered",name);
  } else {
    while (fl) {
      if (fl->dim < 0 || dim-1 == fl->dim) {
        refine = fl->refine;
        adapt  = fl->adapt;
        goto gotit;
      }
      fl = fl->next;
    }
    SETERRQ(PETSC_COMM_SELF,PETSC_ERR_ARG_OUTOFRANGE,"No grid refiner of dimension %D registered",dim);
  }

  gotit:
  switch (dim) {
    case 1:
    case 2:
    case 3:
      if (adapt) {
        ierr = (*adapt)(dm, NULL, adaptLabel, NULL, dmRefined);CHKERRQ(ierr);
      } else {
        ierr = PetscMalloc1(cEnd - cStart, &maxVolumes);CHKERRQ(ierr);
        if (adaptLabel) {
          ierr = DMPlexLabelToVolumeConstraint(dm, adaptLabel, cStart, cEnd, PETSC_DEFAULT, maxVolumes);CHKERRQ(ierr);
        } else if (refinementFunc) {
          for (c = cStart; c < cEnd; ++c) {
            PetscReal vol, centroid[3];

            ierr = DMPlexComputeCellGeometryFVM(dm, c, &vol, centroid, NULL);CHKERRQ(ierr);
            ierr = (*refinementFunc)(centroid, &maxVolumes[c-cStart]);CHKERRQ(ierr);
          }
        } else {
          for (c = 0; c < cEnd-cStart; ++c) maxVolumes[c] = refinementLimit;
        }
        ierr = (*refine)(dm, maxVolumes, dmRefined);CHKERRQ(ierr);
        ierr = PetscFree(maxVolumes);CHKERRQ(ierr);
      }
      break;
    default: SETERRQ(PetscObjectComm((PetscObject)dm), PETSC_ERR_SUP, "Mesh refinement in dimension %D is not supported.", dim);
  }
  ierr = DMCopyDisc(dm, *dmRefined);CHKERRQ(ierr);
  ierr = DMPlexCopy_Internal(dm, PETSC_TRUE, *dmRefined);CHKERRQ(ierr);
  if (localized) {ierr = DMLocalizeCoordinates(*dmRefined);CHKERRQ(ierr);}
  PetscFunctionReturn(0);
}

PetscErrorCode DMPlexCoarsen_Internal(DM dm, PETSC_UNUSED Vec metric, DMLabel adaptLabel, PETSC_UNUSED DMLabel rgLabel, DM *dmCoarsened)
{
  Vec            metricVec;
  PetscInt       cStart, cEnd, vStart, vEnd;
  DMLabel        bdLabel = NULL;
  char           bdLabelName[PETSC_MAX_PATH_LEN], rgLabelName[PETSC_MAX_PATH_LEN];
  PetscBool      localized, flg;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = DMGetCoordinatesLocalized(dm, &localized);CHKERRQ(ierr);
  ierr = DMPlexGetHeightStratum(dm, 0, &cStart, &cEnd);CHKERRQ(ierr);
  ierr = DMPlexGetDepthStratum(dm, 0, &vStart, &vEnd);CHKERRQ(ierr);
  ierr = DMPlexLabelToMetricConstraint(dm, adaptLabel, cStart, cEnd, vStart, vEnd, PETSC_DEFAULT, &metricVec);CHKERRQ(ierr);
  ierr = PetscOptionsGetString(NULL, dm->hdr.prefix, "-dm_plex_coarsen_bd_label", bdLabelName, sizeof(bdLabelName), &flg);CHKERRQ(ierr);
  if (flg) {ierr = DMGetLabel(dm, bdLabelName, &bdLabel);CHKERRQ(ierr);}
  ierr = PetscOptionsGetString(NULL, dm->hdr.prefix, "-dm_plex_coarsen_rg_label", rgLabelName, sizeof(rgLabelName), &flg);CHKERRQ(ierr);
  if (flg) {ierr = DMGetLabel(dm, rgLabelName, &rgLabel);CHKERRQ(ierr);}
  ierr = DMAdaptMetric(dm, metricVec, bdLabel, rgLabel, dmCoarsened);CHKERRQ(ierr);
  ierr = VecDestroy(&metricVec);CHKERRQ(ierr);
  ierr = DMCopyDisc(dm, *dmCoarsened);CHKERRQ(ierr);
  ierr = DMPlexCopy_Internal(dm, PETSC_TRUE, *dmCoarsened);CHKERRQ(ierr);
  if (localized) {ierr = DMLocalizeCoordinates(*dmCoarsened);CHKERRQ(ierr);}
  PetscFunctionReturn(0);
}

PetscErrorCode DMAdaptLabel_Plex(DM dm, PETSC_UNUSED Vec metric, DMLabel adaptLabel, PETSC_UNUSED DMLabel rgLabel, DM *dmAdapted)
{
  IS              flagIS;
  const PetscInt *flags;
  PetscInt        defFlag, minFlag, maxFlag, numFlags, f;
  PetscErrorCode  ierr;

  PetscFunctionBegin;
  ierr = DMLabelGetDefaultValue(adaptLabel, &defFlag);CHKERRQ(ierr);
  minFlag = defFlag;
  maxFlag = defFlag;
  ierr = DMLabelGetValueIS(adaptLabel, &flagIS);CHKERRQ(ierr);
  ierr = ISGetLocalSize(flagIS, &numFlags);CHKERRQ(ierr);
  ierr = ISGetIndices(flagIS, &flags);CHKERRQ(ierr);
  for (f = 0; f < numFlags; ++f) {
    const PetscInt flag = flags[f];

    minFlag = PetscMin(minFlag, flag);
    maxFlag = PetscMax(maxFlag, flag);
  }
  ierr = ISRestoreIndices(flagIS, &flags);CHKERRQ(ierr);
  ierr = ISDestroy(&flagIS);CHKERRQ(ierr);
  {
    PetscInt minMaxFlag[2], minMaxFlagGlobal[2];

    minMaxFlag[0] =  minFlag;
    minMaxFlag[1] = -maxFlag;
    ierr = MPI_Allreduce(minMaxFlag, minMaxFlagGlobal, 2, MPIU_INT, MPI_MIN, PetscObjectComm((PetscObject)dm));CHKERRMPI(ierr);
    minFlag =  minMaxFlagGlobal[0];
    maxFlag = -minMaxFlagGlobal[1];
  }
  if (minFlag == maxFlag) {
    switch (minFlag) {
    case DM_ADAPT_DETERMINE:
      *dmAdapted = NULL;break;
    case DM_ADAPT_REFINE:
      ierr = DMPlexSetRefinementUniform(dm, PETSC_TRUE);CHKERRQ(ierr);
      ierr = DMRefine(dm, MPI_COMM_NULL, dmAdapted);CHKERRQ(ierr);break;
    case DM_ADAPT_COARSEN:
      ierr = DMCoarsen(dm, MPI_COMM_NULL, dmAdapted);CHKERRQ(ierr);break;
    default:
      SETERRQ(PETSC_COMM_SELF, PETSC_ERR_SUP,"DMPlex does not support refinement flag %D", minFlag);
    }
  } else {
    ierr = DMPlexSetRefinementUniform(dm, PETSC_FALSE);CHKERRQ(ierr);
    ierr = DMPlexRefine_Internal(dm, NULL, adaptLabel, NULL, dmAdapted);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}
