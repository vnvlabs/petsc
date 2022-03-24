#include <petsc/private/dmpleximpl.h>   /*I      "petscdmplex.h"   I*/
#include <petscblaslapack.h>

PetscLogEvent DMPLEX_MetricEnforceSPD, DMPLEX_MetricNormalize, DMPLEX_MetricAverage, DMPLEX_MetricIntersection;

PetscErrorCode DMPlexMetricSetFromOptions(DM dm)
{
  MPI_Comm       comm;
  PetscBool      isotropic = PETSC_FALSE, uniform = PETSC_FALSE, restrictAnisotropyFirst = PETSC_FALSE;
  PetscBool      noInsert = PETSC_FALSE, noSwap = PETSC_FALSE, noMove = PETSC_FALSE, noSurf = PETSC_FALSE;
  PetscErrorCode ierr;
  PetscInt       verbosity = -1, numIter = 3;
  PetscReal      h_min = 1.0e-30, h_max = 1.0e+30, a_max = 1.0e+05, p = 1.0, target = 1000.0, beta = 1.3, hausd = 0.01;

  PetscFunctionBegin;
  ierr = PetscObjectGetComm((PetscObject) dm, &comm);CHKERRQ(ierr);
  ierr = PetscOptionsBegin(comm, "", "Riemannian metric options", "DMPlexMetric");CHKERRQ(ierr);
  ierr = PetscOptionsBool("-dm_plex_metric_isotropic", "Is the metric isotropic?", "DMPlexMetricCreateIsotropic", isotropic, &isotropic, NULL);CHKERRQ(ierr);
  ierr = DMPlexMetricSetIsotropic(dm, isotropic);CHKERRQ(ierr);
  ierr = PetscOptionsBool("-dm_plex_metric_uniform", "Is the metric uniform?", "DMPlexMetricCreateUniform", uniform, &uniform, NULL);CHKERRQ(ierr);
  ierr = DMPlexMetricSetUniform(dm, uniform);CHKERRQ(ierr);
  ierr = PetscOptionsBool("-dm_plex_metric_restrict_anisotropy_first", "Should anisotropy be restricted before normalization?", "DMPlexNormalize", restrictAnisotropyFirst, &restrictAnisotropyFirst, NULL);CHKERRQ(ierr);
  ierr = DMPlexMetricSetRestrictAnisotropyFirst(dm, restrictAnisotropyFirst);CHKERRQ(ierr);
  ierr = PetscOptionsBool("-dm_plex_metric_no_insert", "Turn off node insertion and deletion", "DMAdaptMetric", noInsert, &noInsert, NULL);CHKERRQ(ierr);
  ierr = DMPlexMetricSetNoInsertion(dm, noInsert);CHKERRQ(ierr);
  ierr = PetscOptionsBool("-dm_plex_metric_no_swap", "Turn off facet swapping", "DMAdaptMetric", noSwap, &noSwap, NULL);CHKERRQ(ierr);
  ierr = DMPlexMetricSetNoSwapping(dm, noSwap);CHKERRQ(ierr);
  ierr = PetscOptionsBool("-dm_plex_metric_no_move", "Turn off facet node movement", "DMAdaptMetric", noMove, &noMove, NULL);CHKERRQ(ierr);
  ierr = DMPlexMetricSetNoMovement(dm, noMove);CHKERRQ(ierr);
  ierr = PetscOptionsBool("-dm_plex_metric_no_surf", "Turn off surface modification", "DMAdaptMetric", noSurf, &noSurf, NULL);CHKERRQ(ierr);
  ierr = DMPlexMetricSetNoSurf(dm, noSurf);CHKERRQ(ierr);
  ierr = PetscOptionsBoundedInt("-dm_plex_metric_num_iterations", "Number of ParMmg adaptation iterations", "DMAdaptMetric", numIter, &numIter, NULL, 0);CHKERRQ(ierr);
  ierr = DMPlexMetricSetNumIterations(dm, numIter);CHKERRQ(ierr);
  ierr = PetscOptionsRangeInt("-dm_plex_metric_verbosity", "Verbosity of metric-based mesh adaptation package (-1 = silent, 10 = maximum)", "DMAdaptMetric", verbosity, &verbosity, NULL, -1, 10);CHKERRQ(ierr);
  ierr = DMPlexMetricSetVerbosity(dm, verbosity);CHKERRQ(ierr);
  ierr = PetscOptionsReal("-dm_plex_metric_h_min", "Minimum tolerated metric magnitude", "DMPlexMetricEnforceSPD", h_min, &h_min, NULL);CHKERRQ(ierr);
  ierr = DMPlexMetricSetMinimumMagnitude(dm, h_min);CHKERRQ(ierr);
  ierr = PetscOptionsReal("-dm_plex_metric_h_max", "Maximum tolerated metric magnitude", "DMPlexMetricEnforceSPD", h_max, &h_max, NULL);CHKERRQ(ierr);
  ierr = DMPlexMetricSetMaximumMagnitude(dm, h_max);CHKERRQ(ierr);
  ierr = PetscOptionsReal("-dm_plex_metric_a_max", "Maximum tolerated anisotropy", "DMPlexMetricEnforceSPD", a_max, &a_max, NULL);CHKERRQ(ierr);
  ierr = DMPlexMetricSetMaximumAnisotropy(dm, a_max);CHKERRQ(ierr);
  ierr = PetscOptionsReal("-dm_plex_metric_p", "L-p normalization order", "DMPlexMetricNormalize", p, &p, NULL);CHKERRQ(ierr);
  ierr = DMPlexMetricSetNormalizationOrder(dm, p);CHKERRQ(ierr);
  ierr = PetscOptionsReal("-dm_plex_metric_target_complexity", "Target metric complexity", "DMPlexMetricNormalize", target, &target, NULL);CHKERRQ(ierr);
  ierr = DMPlexMetricSetTargetComplexity(dm, target);CHKERRQ(ierr);
  ierr = PetscOptionsReal("-dm_plex_metric_gradation_factor", "Metric gradation factor", "DMAdaptMetric", beta, &beta, NULL);CHKERRQ(ierr);
  ierr = DMPlexMetricSetGradationFactor(dm, beta);CHKERRQ(ierr);
  ierr = PetscOptionsReal("-dm_plex_metric_hausdorff_number", "Metric Hausdorff number", "DMAdaptMetric", hausd, &hausd, NULL);CHKERRQ(ierr);
  ierr = DMPlexMetricSetHausdorffNumber(dm, hausd);CHKERRQ(ierr);
  ierr = PetscOptionsEnd();CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/*@
  DMPlexMetricSetIsotropic - Record whether a metric is isotropic

  Input parameters:
+ dm        - The DM
- isotropic - Is the metric isotropic?

  Level: beginner

.seealso: DMPlexMetricIsIsotropic(), DMPlexMetricSetUniform(), DMPlexMetricSetRestrictAnisotropyFirst()
@*/
PetscErrorCode DMPlexMetricSetIsotropic(DM dm, PetscBool isotropic)
{
  DM_Plex       *plex = (DM_Plex *) dm->data;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  if (!plex->metricCtx) {
    ierr = PetscNew(&plex->metricCtx);CHKERRQ(ierr);
    ierr = DMPlexMetricSetFromOptions(dm);CHKERRQ(ierr);
  }
  plex->metricCtx->isotropic = isotropic;
  PetscFunctionReturn(0);
}

/*@
  DMPlexMetricIsIsotropic - Is a metric isotropic?

  Input parameters:
. dm        - The DM

  Output parameters:
. isotropic - Is the metric isotropic?

  Level: beginner

.seealso: DMPlexMetricSetIsotropic(), DMPlexMetricIsUniform(), DMPlexMetricRestrictAnisotropyFirst()
@*/
PetscErrorCode DMPlexMetricIsIsotropic(DM dm, PetscBool *isotropic)
{
  DM_Plex       *plex = (DM_Plex *) dm->data;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  if (!plex->metricCtx) {
    ierr = PetscNew(&plex->metricCtx);CHKERRQ(ierr);
    ierr = DMPlexMetricSetFromOptions(dm);CHKERRQ(ierr);
  }
  *isotropic = plex->metricCtx->isotropic;
  PetscFunctionReturn(0);
}

/*@
  DMPlexMetricSetUniform - Record whether a metric is uniform

  Input parameters:
+ dm      - The DM
- uniform - Is the metric uniform?

  Level: beginner

  Notes:

  If the metric is specified as uniform then it is assumed to be isotropic, too.

.seealso: DMPlexMetricIsUniform(), DMPlexMetricSetIsotropic(), DMPlexMetricSetRestrictAnisotropyFirst()
@*/
PetscErrorCode DMPlexMetricSetUniform(DM dm, PetscBool uniform)
{
  DM_Plex       *plex = (DM_Plex *) dm->data;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  if (!plex->metricCtx) {
    ierr = PetscNew(&plex->metricCtx);CHKERRQ(ierr);
    ierr = DMPlexMetricSetFromOptions(dm);CHKERRQ(ierr);
  }
  plex->metricCtx->uniform = uniform;
  if (uniform) plex->metricCtx->isotropic = uniform;
  PetscFunctionReturn(0);
}

/*@
  DMPlexMetricIsUniform - Is a metric uniform?

  Input parameters:
. dm      - The DM

  Output parameters:
. uniform - Is the metric uniform?

  Level: beginner

.seealso: DMPlexMetricSetUniform(), DMPlexMetricIsIsotropic(), DMPlexMetricRestrictAnisotropyFirst()
@*/
PetscErrorCode DMPlexMetricIsUniform(DM dm, PetscBool *uniform)
{
  DM_Plex       *plex = (DM_Plex *) dm->data;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  if (!plex->metricCtx) {
    ierr = PetscNew(&plex->metricCtx);CHKERRQ(ierr);
    ierr = DMPlexMetricSetFromOptions(dm);CHKERRQ(ierr);
  }
  *uniform = plex->metricCtx->uniform;
  PetscFunctionReturn(0);
}

/*@
  DMPlexMetricSetRestrictAnisotropyFirst - Record whether anisotropy should be restricted before normalization

  Input parameters:
+ dm                      - The DM
- restrictAnisotropyFirst - Should anisotropy be normalized first?

  Level: beginner

.seealso: DMPlexMetricSetIsotropic(), DMPlexMetricRestrictAnisotropyFirst()
@*/
PetscErrorCode DMPlexMetricSetRestrictAnisotropyFirst(DM dm, PetscBool restrictAnisotropyFirst)
{
  DM_Plex       *plex = (DM_Plex *) dm->data;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  if (!plex->metricCtx) {
    ierr = PetscNew(&plex->metricCtx);CHKERRQ(ierr);
    ierr = DMPlexMetricSetFromOptions(dm);CHKERRQ(ierr);
  }
  plex->metricCtx->restrictAnisotropyFirst = restrictAnisotropyFirst;
  PetscFunctionReturn(0);
}

/*@
  DMPlexMetricRestrictAnisotropyFirst - Is anisotropy restricted before normalization or after?

  Input parameters:
. dm                      - The DM

  Output parameters:
. restrictAnisotropyFirst - Is anisotropy be normalized first?

  Level: beginner

.seealso: DMPlexMetricIsIsotropic(), DMPlexMetricSetRestrictAnisotropyFirst()
@*/
PetscErrorCode DMPlexMetricRestrictAnisotropyFirst(DM dm, PetscBool *restrictAnisotropyFirst)
{
  DM_Plex       *plex = (DM_Plex *) dm->data;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  if (!plex->metricCtx) {
    ierr = PetscNew(&plex->metricCtx);CHKERRQ(ierr);
    ierr = DMPlexMetricSetFromOptions(dm);CHKERRQ(ierr);
  }
  *restrictAnisotropyFirst = plex->metricCtx->restrictAnisotropyFirst;
  PetscFunctionReturn(0);
}

/*@
  DMPlexMetricSetNoInsertion - Should node insertion and deletion be turned off?

  Input parameters:
+ dm       - The DM
- noInsert - Should node insertion and deletion be turned off?

  Level: beginner

  Notes:
  This is only used by Mmg and ParMmg (not Pragmatic).

.seealso: DMPlexMetricNoInsertion(), DMPlexMetricSetNoSwapping(), DMPlexMetricSetNoMovement(), DMPlexMetricSetNoSurf()
@*/
PetscErrorCode DMPlexMetricSetNoInsertion(DM dm, PetscBool noInsert)
{
  DM_Plex       *plex = (DM_Plex *) dm->data;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  if (!plex->metricCtx) {
    ierr = PetscNew(&plex->metricCtx);CHKERRQ(ierr);
    ierr = DMPlexMetricSetFromOptions(dm);CHKERRQ(ierr);
  }
  plex->metricCtx->noInsert = noInsert;
  PetscFunctionReturn(0);
}

/*@
  DMPlexMetricNoInsertion - Are node insertion and deletion turned off?

  Input parameters:
. dm       - The DM

  Output parameters:
. noInsert - Are node insertion and deletion turned off?

  Level: beginner

  Notes:
  This is only used by Mmg and ParMmg (not Pragmatic).

.seealso: DMPlexMetricSetNoInsertion(), DMPlexMetricNoSwapping(), DMPlexMetricNoMovement(), DMPlexMetricNoSurf()
@*/
PetscErrorCode DMPlexMetricNoInsertion(DM dm, PetscBool *noInsert)
{
  DM_Plex       *plex = (DM_Plex *) dm->data;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  if (!plex->metricCtx) {
    ierr = PetscNew(&plex->metricCtx);CHKERRQ(ierr);
    ierr = DMPlexMetricSetFromOptions(dm);CHKERRQ(ierr);
  }
  *noInsert = plex->metricCtx->noInsert;
  PetscFunctionReturn(0);
}

/*@
  DMPlexMetricSetNoSwapping - Should facet swapping be turned off?

  Input parameters:
+ dm     - The DM
- noSwap - Should facet swapping be turned off?

  Level: beginner

  Notes:
  This is only used by Mmg and ParMmg (not Pragmatic).

.seealso: DMPlexMetricNoSwapping(), DMPlexMetricSetNoInsertion(), DMPlexMetricSetNoMovement(), DMPlexMetricSetNoSurf()
@*/
PetscErrorCode DMPlexMetricSetNoSwapping(DM dm, PetscBool noSwap)
{
  DM_Plex       *plex = (DM_Plex *) dm->data;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  if (!plex->metricCtx) {
    ierr = PetscNew(&plex->metricCtx);CHKERRQ(ierr);
    ierr = DMPlexMetricSetFromOptions(dm);CHKERRQ(ierr);
  }
  plex->metricCtx->noSwap = noSwap;
  PetscFunctionReturn(0);
}

/*@
  DMPlexMetricNoSwapping - Is facet swapping turned off?

  Input parameters:
. dm     - The DM

  Output parameters:
. noSwap - Is facet swapping turned off?

  Level: beginner

  Notes:
  This is only used by Mmg and ParMmg (not Pragmatic).

.seealso: DMPlexMetricSetNoSwapping(), DMPlexMetricNoInsertion(), DMPlexMetricNoMovement(), DMPlexMetricNoSurf()
@*/
PetscErrorCode DMPlexMetricNoSwapping(DM dm, PetscBool *noSwap)
{
  DM_Plex       *plex = (DM_Plex *) dm->data;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  if (!plex->metricCtx) {
    ierr = PetscNew(&plex->metricCtx);CHKERRQ(ierr);
    ierr = DMPlexMetricSetFromOptions(dm);CHKERRQ(ierr);
  }
  *noSwap = plex->metricCtx->noSwap;
  PetscFunctionReturn(0);
}

/*@
  DMPlexMetricSetNoMovement - Should node movement be turned off?

  Input parameters:
+ dm     - The DM
- noMove - Should node movement be turned off?

  Level: beginner

  Notes:
  This is only used by Mmg and ParMmg (not Pragmatic).

.seealso: DMPlexMetricNoMovement(), DMPlexMetricSetNoInsertion(), DMPlexMetricSetNoSwapping(), DMPlexMetricSetNoSurf()
@*/
PetscErrorCode DMPlexMetricSetNoMovement(DM dm, PetscBool noMove)
{
  DM_Plex       *plex = (DM_Plex *) dm->data;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  if (!plex->metricCtx) {
    ierr = PetscNew(&plex->metricCtx);CHKERRQ(ierr);
    ierr = DMPlexMetricSetFromOptions(dm);CHKERRQ(ierr);
  }
  plex->metricCtx->noMove = noMove;
  PetscFunctionReturn(0);
}

/*@
  DMPlexMetricNoMovement - Is node movement turned off?

  Input parameters:
. dm     - The DM

  Output parameters:
. noMove - Is node movement turned off?

  Level: beginner

  Notes:
  This is only used by Mmg and ParMmg (not Pragmatic).

.seealso: DMPlexMetricSetNoMovement(), DMPlexMetricNoInsertion(), DMPlexMetricNoSwapping(), DMPlexMetricNoSurf()
@*/
PetscErrorCode DMPlexMetricNoMovement(DM dm, PetscBool *noMove)
{
  DM_Plex       *plex = (DM_Plex *) dm->data;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  if (!plex->metricCtx) {
    ierr = PetscNew(&plex->metricCtx);CHKERRQ(ierr);
    ierr = DMPlexMetricSetFromOptions(dm);CHKERRQ(ierr);
  }
  *noMove = plex->metricCtx->noMove;
  PetscFunctionReturn(0);
}

/*@
  DMPlexMetricSetNoSurf - Should surface modification be turned off?

  Input parameters:
+ dm     - The DM
- noSurf - Should surface modification be turned off?

  Level: beginner

  Notes:
  This is only used by Mmg and ParMmg (not Pragmatic).

.seealso: DMPlexMetricNoSurf(), DMPlexMetricSetNoMovement(), DMPlexMetricSetNoInsertion(), DMPlexMetricSetNoSwapping()
@*/
PetscErrorCode DMPlexMetricSetNoSurf(DM dm, PetscBool noSurf)
{
  DM_Plex       *plex = (DM_Plex *) dm->data;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  if (!plex->metricCtx) {
    ierr = PetscNew(&plex->metricCtx);CHKERRQ(ierr);
    ierr = DMPlexMetricSetFromOptions(dm);CHKERRQ(ierr);
  }
  plex->metricCtx->noSurf = noSurf;
  PetscFunctionReturn(0);
}

/*@
  DMPlexMetricNoSurf - Is surface modification turned off?

  Input parameters:
. dm     - The DM

  Output parameters:
. noSurf - Is surface modification turned off?

  Level: beginner

  Notes:
  This is only used by Mmg and ParMmg (not Pragmatic).

.seealso: DMPlexMetricSetNoSurf(), DMPlexMetricNoMovement(), DMPlexMetricNoInsertion(), DMPlexMetricNoSwapping()
@*/
PetscErrorCode DMPlexMetricNoSurf(DM dm, PetscBool *noSurf)
{
  DM_Plex       *plex = (DM_Plex *) dm->data;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  if (!plex->metricCtx) {
    ierr = PetscNew(&plex->metricCtx);CHKERRQ(ierr);
    ierr = DMPlexMetricSetFromOptions(dm);CHKERRQ(ierr);
  }
  *noSurf = plex->metricCtx->noSurf;
  PetscFunctionReturn(0);
}

/*@
  DMPlexMetricSetMinimumMagnitude - Set the minimum tolerated metric magnitude

  Input parameters:
+ dm    - The DM
- h_min - The minimum tolerated metric magnitude

  Level: beginner

.seealso: DMPlexMetricGetMinimumMagnitude(), DMPlexMetricSetMaximumMagnitude()
@*/
PetscErrorCode DMPlexMetricSetMinimumMagnitude(DM dm, PetscReal h_min)
{
  DM_Plex       *plex = (DM_Plex *) dm->data;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  if (!plex->metricCtx) {
    ierr = PetscNew(&plex->metricCtx);CHKERRQ(ierr);
    ierr = DMPlexMetricSetFromOptions(dm);CHKERRQ(ierr);
  }
  PetscCheckFalse(h_min <= 0.0,PETSC_COMM_SELF, PETSC_ERR_ARG_WRONG, "Metric magnitudes must be positive, not %.4e", h_min);
  plex->metricCtx->h_min = h_min;
  PetscFunctionReturn(0);
}

/*@
  DMPlexMetricGetMinimumMagnitude - Get the minimum tolerated metric magnitude

  Input parameters:
. dm    - The DM

  Output parameters:
. h_min - The minimum tolerated metric magnitude

  Level: beginner

.seealso: DMPlexMetricSetMinimumMagnitude(), DMPlexMetricGetMaximumMagnitude()
@*/
PetscErrorCode DMPlexMetricGetMinimumMagnitude(DM dm, PetscReal *h_min)
{
  DM_Plex       *plex = (DM_Plex *) dm->data;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  if (!plex->metricCtx) {
    ierr = PetscNew(&plex->metricCtx);CHKERRQ(ierr);
    ierr = DMPlexMetricSetFromOptions(dm);CHKERRQ(ierr);
  }
  *h_min = plex->metricCtx->h_min;
  PetscFunctionReturn(0);
}

/*@
  DMPlexMetricSetMaximumMagnitude - Set the maximum tolerated metric magnitude

  Input parameters:
+ dm    - The DM
- h_max - The maximum tolerated metric magnitude

  Level: beginner

.seealso: DMPlexMetricGetMaximumMagnitude(), DMPlexMetricSetMinimumMagnitude()
@*/
PetscErrorCode DMPlexMetricSetMaximumMagnitude(DM dm, PetscReal h_max)
{
  DM_Plex       *plex = (DM_Plex *) dm->data;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  if (!plex->metricCtx) {
    ierr = PetscNew(&plex->metricCtx);CHKERRQ(ierr);
    ierr = DMPlexMetricSetFromOptions(dm);CHKERRQ(ierr);
  }
  PetscCheckFalse(h_max <= 0.0,PETSC_COMM_SELF, PETSC_ERR_ARG_WRONG, "Metric magnitudes must be positive, not %.4e", h_max);
  plex->metricCtx->h_max = h_max;
  PetscFunctionReturn(0);
}

/*@
  DMPlexMetricGetMaximumMagnitude - Get the maximum tolerated metric magnitude

  Input parameters:
. dm    - The DM

  Output parameters:
. h_max - The maximum tolerated metric magnitude

  Level: beginner

.seealso: DMPlexMetricSetMaximumMagnitude(), DMPlexMetricGetMinimumMagnitude()
@*/
PetscErrorCode DMPlexMetricGetMaximumMagnitude(DM dm, PetscReal *h_max)
{
  DM_Plex       *plex = (DM_Plex *) dm->data;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  if (!plex->metricCtx) {
    ierr = PetscNew(&plex->metricCtx);CHKERRQ(ierr);
    ierr = DMPlexMetricSetFromOptions(dm);CHKERRQ(ierr);
  }
  *h_max = plex->metricCtx->h_max;
  PetscFunctionReturn(0);
}

/*@
  DMPlexMetricSetMaximumAnisotropy - Set the maximum tolerated metric anisotropy

  Input parameters:
+ dm    - The DM
- a_max - The maximum tolerated metric anisotropy

  Level: beginner

  Note: If the value zero is given then anisotropy will not be restricted. Otherwise, it should be at least one.

.seealso: DMPlexMetricGetMaximumAnisotropy(), DMPlexMetricSetMaximumMagnitude()
@*/
PetscErrorCode DMPlexMetricSetMaximumAnisotropy(DM dm, PetscReal a_max)
{
  DM_Plex       *plex = (DM_Plex *) dm->data;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  if (!plex->metricCtx) {
    ierr = PetscNew(&plex->metricCtx);CHKERRQ(ierr);
    ierr = DMPlexMetricSetFromOptions(dm);CHKERRQ(ierr);
  }
  PetscCheckFalse(a_max < 1.0,PETSC_COMM_SELF, PETSC_ERR_ARG_WRONG, "Anisotropy must be at least one, not %.4e", a_max);
  plex->metricCtx->a_max = a_max;
  PetscFunctionReturn(0);
}

/*@
  DMPlexMetricGetMaximumAnisotropy - Get the maximum tolerated metric anisotropy

  Input parameters:
. dm    - The DM

  Output parameters:
. a_max - The maximum tolerated metric anisotropy

  Level: beginner

.seealso: DMPlexMetricSetMaximumAnisotropy(), DMPlexMetricGetMaximumMagnitude()
@*/
PetscErrorCode DMPlexMetricGetMaximumAnisotropy(DM dm, PetscReal *a_max)
{
  DM_Plex       *plex = (DM_Plex *) dm->data;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  if (!plex->metricCtx) {
    ierr = PetscNew(&plex->metricCtx);CHKERRQ(ierr);
    ierr = DMPlexMetricSetFromOptions(dm);CHKERRQ(ierr);
  }
  *a_max = plex->metricCtx->a_max;
  PetscFunctionReturn(0);
}

/*@
  DMPlexMetricSetTargetComplexity - Set the target metric complexity

  Input parameters:
+ dm               - The DM
- targetComplexity - The target metric complexity

  Level: beginner

.seealso: DMPlexMetricGetTargetComplexity(), DMPlexMetricSetNormalizationOrder()
@*/
PetscErrorCode DMPlexMetricSetTargetComplexity(DM dm, PetscReal targetComplexity)
{
  DM_Plex       *plex = (DM_Plex *) dm->data;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  if (!plex->metricCtx) {
    ierr = PetscNew(&plex->metricCtx);CHKERRQ(ierr);
    ierr = DMPlexMetricSetFromOptions(dm);CHKERRQ(ierr);
  }
  PetscCheckFalse(targetComplexity <= 0.0,PETSC_COMM_SELF, PETSC_ERR_ARG_WRONG, "Metric complexity must be positive");
  plex->metricCtx->targetComplexity = targetComplexity;
  PetscFunctionReturn(0);
}

/*@
  DMPlexMetricGetTargetComplexity - Get the target metric complexity

  Input parameters:
. dm               - The DM

  Output parameters:
. targetComplexity - The target metric complexity

  Level: beginner

.seealso: DMPlexMetricSetTargetComplexity(), DMPlexMetricGetNormalizationOrder()
@*/
PetscErrorCode DMPlexMetricGetTargetComplexity(DM dm, PetscReal *targetComplexity)
{
  DM_Plex       *plex = (DM_Plex *) dm->data;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  if (!plex->metricCtx) {
    ierr = PetscNew(&plex->metricCtx);CHKERRQ(ierr);
    ierr = DMPlexMetricSetFromOptions(dm);CHKERRQ(ierr);
  }
  *targetComplexity = plex->metricCtx->targetComplexity;
  PetscFunctionReturn(0);
}

/*@
  DMPlexMetricSetNormalizationOrder - Set the order p for L-p normalization

  Input parameters:
+ dm - The DM
- p  - The normalization order

  Level: beginner

.seealso: DMPlexMetricGetNormalizationOrder(), DMPlexMetricSetTargetComplexity()
@*/
PetscErrorCode DMPlexMetricSetNormalizationOrder(DM dm, PetscReal p)
{
  DM_Plex       *plex = (DM_Plex *) dm->data;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  if (!plex->metricCtx) {
    ierr = PetscNew(&plex->metricCtx);CHKERRQ(ierr);
    ierr = DMPlexMetricSetFromOptions(dm);CHKERRQ(ierr);
  }
  PetscCheckFalse(p < 1.0,PETSC_COMM_SELF, PETSC_ERR_ARG_WRONG, "Normalization order must be one or greater");
  plex->metricCtx->p = p;
  PetscFunctionReturn(0);
}

/*@
  DMPlexMetricGetNormalizationOrder - Get the order p for L-p normalization

  Input parameters:
. dm - The DM

  Output parameters:
. p - The normalization order

  Level: beginner

.seealso: DMPlexMetricSetNormalizationOrder(), DMPlexMetricGetTargetComplexity()
@*/
PetscErrorCode DMPlexMetricGetNormalizationOrder(DM dm, PetscReal *p)
{
  DM_Plex       *plex = (DM_Plex *) dm->data;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  if (!plex->metricCtx) {
    ierr = PetscNew(&plex->metricCtx);CHKERRQ(ierr);
    ierr = DMPlexMetricSetFromOptions(dm);CHKERRQ(ierr);
  }
  *p = plex->metricCtx->p;
  PetscFunctionReturn(0);
}

/*@
  DMPlexMetricSetGradationFactor - Set the metric gradation factor

  Input parameters:
+ dm   - The DM
- beta - The metric gradation factor

  Level: beginner

  Notes:

  The gradation factor is the maximum tolerated length ratio between adjacent edges.

  Turn off gradation by passing the value -1. Otherwise, pass a positive value.

  This is only used by Mmg and ParMmg (not Pragmatic).

.seealso: DMPlexMetricGetGradationFactor(), DMPlexMetricSetHausdorffNumber()
@*/
PetscErrorCode DMPlexMetricSetGradationFactor(DM dm, PetscReal beta)
{
  DM_Plex       *plex = (DM_Plex *) dm->data;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  if (!plex->metricCtx) {
    ierr = PetscNew(&plex->metricCtx);CHKERRQ(ierr);
    ierr = DMPlexMetricSetFromOptions(dm);CHKERRQ(ierr);
  }
  plex->metricCtx->gradationFactor = beta;
  PetscFunctionReturn(0);
}

/*@
  DMPlexMetricGetGradationFactor - Get the metric gradation factor

  Input parameters:
. dm   - The DM

  Output parameters:
. beta - The metric gradation factor

  Level: beginner

  Notes:

  The gradation factor is the maximum tolerated length ratio between adjacent edges.

  The value -1 implies that gradation is turned off.

  This is only used by Mmg and ParMmg (not Pragmatic).

.seealso: DMPlexMetricSetGradationFactor(), DMPlexMetricGetHausdorffNumber()
@*/
PetscErrorCode DMPlexMetricGetGradationFactor(DM dm, PetscReal *beta)
{
  DM_Plex       *plex = (DM_Plex *) dm->data;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  if (!plex->metricCtx) {
    ierr = PetscNew(&plex->metricCtx);CHKERRQ(ierr);
    ierr = DMPlexMetricSetFromOptions(dm);CHKERRQ(ierr);
  }
  *beta = plex->metricCtx->gradationFactor;
  PetscFunctionReturn(0);
}

/*@
  DMPlexMetricSetHausdorffNumber - Set the metric Hausdorff number

  Input parameters:
+ dm    - The DM
- hausd - The metric Hausdorff number

  Level: beginner

  Notes:

  The Hausdorff number imposes the maximal distance between the piecewise linear approximation of the
  boundary and the reconstructed ideal boundary. Thus, a low Hausdorff parameter leads to refine the
  high curvature areas. By default, the Hausdorff value is set to 0.01, which is a suitable value for
  an object of size 1 in each direction. For smaller (resp. larger) objects, you may need to decrease
  (resp. increase) the Hausdorff parameter. (Taken from
  https://www.mmgtools.org/mmg-remesher-try-mmg/mmg-remesher-options/mmg-remesher-option-hausd).

  This is only used by Mmg and ParMmg (not Pragmatic).

.seealso: DMPlexMetricSetGradationFactor(), DMPlexMetricGetHausdorffNumber()
@*/
PetscErrorCode DMPlexMetricSetHausdorffNumber(DM dm, PetscReal hausd)
{
  DM_Plex       *plex = (DM_Plex *) dm->data;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  if (!plex->metricCtx) {
    ierr = PetscNew(&plex->metricCtx);CHKERRQ(ierr);
    ierr = DMPlexMetricSetFromOptions(dm);CHKERRQ(ierr);
  }
  plex->metricCtx->hausdorffNumber = hausd;
  PetscFunctionReturn(0);
}

/*@
  DMPlexMetricGetHausdorffNumber - Get the metric Hausdorff number

  Input parameters:
. dm    - The DM

  Output parameters:
. hausd - The metric Hausdorff number

  Level: beginner

  Notes:

  The Hausdorff number imposes the maximal distance between the piecewise linear approximation of the
  boundary and the reconstructed ideal boundary. Thus, a low Hausdorff parameter leads to refine the
  high curvature areas. By default, the Hausdorff value is set to 0.01, which is a suitable value for
  an object of size 1 in each direction. For smaller (resp. larger) objects, you may need to decrease
  (resp. increase) the Hausdorff parameter. (Taken from
  https://www.mmgtools.org/mmg-remesher-try-mmg/mmg-remesher-options/mmg-remesher-option-hausd).

  This is only used by Mmg and ParMmg (not Pragmatic).

.seealso: DMPlexMetricGetGradationFactor(), DMPlexMetricSetHausdorffNumber()
@*/
PetscErrorCode DMPlexMetricGetHausdorffNumber(DM dm, PetscReal *hausd)
{
  DM_Plex       *plex = (DM_Plex *) dm->data;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  if (!plex->metricCtx) {
    ierr = PetscNew(&plex->metricCtx);CHKERRQ(ierr);
    ierr = DMPlexMetricSetFromOptions(dm);CHKERRQ(ierr);
  }
  *hausd = plex->metricCtx->hausdorffNumber;
  PetscFunctionReturn(0);
}

/*@
  DMPlexMetricSetVerbosity - Set the verbosity of the mesh adaptation package

  Input parameters:
+ dm        - The DM
- verbosity - The verbosity, where -1 is silent and 10 is maximum

  Level: beginner

  Notes:
  This is only used by Mmg and ParMmg (not Pragmatic).

.seealso: DMPlexMetricGetVerbosity(), DMPlexMetricSetNumIterations()
@*/
PetscErrorCode DMPlexMetricSetVerbosity(DM dm, PetscInt verbosity)
{
  DM_Plex       *plex = (DM_Plex *) dm->data;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  if (!plex->metricCtx) {
    ierr = PetscNew(&plex->metricCtx);CHKERRQ(ierr);
    ierr = DMPlexMetricSetFromOptions(dm);CHKERRQ(ierr);
  }
  plex->metricCtx->verbosity = verbosity;
  PetscFunctionReturn(0);
}

/*@
  DMPlexMetricGetVerbosity - Get the verbosity of the mesh adaptation package

  Input parameters:
. dm        - The DM

  Output parameters:
. verbosity - The verbosity, where -1 is silent and 10 is maximum

  Level: beginner

  Notes:
  This is only used by Mmg and ParMmg (not Pragmatic).

.seealso: DMPlexMetricSetVerbosity(), DMPlexMetricGetNumIterations()
@*/
PetscErrorCode DMPlexMetricGetVerbosity(DM dm, PetscInt *verbosity)
{
  DM_Plex       *plex = (DM_Plex *) dm->data;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  if (!plex->metricCtx) {
    ierr = PetscNew(&plex->metricCtx);CHKERRQ(ierr);
    ierr = DMPlexMetricSetFromOptions(dm);CHKERRQ(ierr);
  }
  *verbosity = plex->metricCtx->verbosity;
  PetscFunctionReturn(0);
}

/*@
  DMPlexMetricSetNumIterations - Set the number of parallel adaptation iterations

  Input parameters:
+ dm      - The DM
- numIter - the number of parallel adaptation iterations

  Level: beginner

  Notes:
  This is only used by ParMmg (not Pragmatic or Mmg).

.seealso: DMPlexMetricSetVerbosity(), DMPlexMetricGetNumIterations()
@*/
PetscErrorCode DMPlexMetricSetNumIterations(DM dm, PetscInt numIter)
{
  DM_Plex       *plex = (DM_Plex *) dm->data;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  if (!plex->metricCtx) {
    ierr = PetscNew(&plex->metricCtx);CHKERRQ(ierr);
    ierr = DMPlexMetricSetFromOptions(dm);CHKERRQ(ierr);
  }
  plex->metricCtx->numIter = numIter;
  PetscFunctionReturn(0);
}

/*@
  DMPlexMetricGetNumIterations - Get the number of parallel adaptation iterations

  Input parameters:
. dm      - The DM

  Output parameters:
. numIter - the number of parallel adaptation iterations

  Level: beginner

  Notes:
  This is only used by Mmg and ParMmg (not Pragmatic or Mmg).

.seealso: DMPlexMetricSetNumIterations(), DMPlexMetricGetVerbosity()
@*/
PetscErrorCode DMPlexMetricGetNumIterations(DM dm, PetscInt *numIter)
{
  DM_Plex       *plex = (DM_Plex *) dm->data;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  if (!plex->metricCtx) {
    ierr = PetscNew(&plex->metricCtx);CHKERRQ(ierr);
    ierr = DMPlexMetricSetFromOptions(dm);CHKERRQ(ierr);
  }
  *numIter = plex->metricCtx->numIter;
  PetscFunctionReturn(0);
}

PetscErrorCode DMPlexP1FieldCreate_Private(DM dm, PetscInt f, PetscInt size, Vec *metric)
{
  MPI_Comm       comm;
  PetscErrorCode ierr;
  PetscFE        fe;
  PetscInt       dim;

  PetscFunctionBegin;

  /* Extract metadata from dm */
  ierr = PetscObjectGetComm((PetscObject) dm, &comm);CHKERRQ(ierr);
  ierr = DMGetDimension(dm, &dim);CHKERRQ(ierr);

  /* Create a P1 field of the requested size */
  ierr = PetscFECreateLagrange(comm, dim, size, PETSC_TRUE, 1, PETSC_DETERMINE, &fe);CHKERRQ(ierr);
  ierr = DMSetField(dm, f, NULL, (PetscObject)fe);CHKERRQ(ierr);
  ierr = DMCreateDS(dm);CHKERRQ(ierr);
  ierr = PetscFEDestroy(&fe);CHKERRQ(ierr);
  ierr = DMCreateLocalVector(dm, metric);CHKERRQ(ierr);

  PetscFunctionReturn(0);
}

/*@
  DMPlexMetricCreate - Create a Riemannian metric field

  Input parameters:
+ dm     - The DM
- f      - The field number to use

  Output parameter:
. metric - The metric

  Level: beginner

  Notes:

  It is assumed that the DM is comprised of simplices.

  Command line options for Riemannian metrics:

+ -dm_plex_metric_isotropic                 - Is the metric isotropic?
. -dm_plex_metric_uniform                   - Is the metric uniform?
. -dm_plex_metric_restrict_anisotropy_first - Should anisotropy be restricted before normalization?
. -dm_plex_metric_h_min                     - Minimum tolerated metric magnitude
. -dm_plex_metric_h_max                     - Maximum tolerated metric magnitude
. -dm_plex_metric_a_max                     - Maximum tolerated anisotropy
. -dm_plex_metric_p                         - L-p normalization order
- -dm_plex_metric_target_complexity         - Target metric complexity

  Switching between remeshers can be achieved using

. -dm_adaptor <pragmatic/mmg/parmmg>        - specify dm adaptor to use

  Further options that are only relevant to Mmg and ParMmg:

+ -dm_plex_metric_gradation_factor          - Maximum ratio by which edge lengths may grow during gradation
. -dm_plex_metric_num_iterations            - Number of parallel mesh adaptation iterations for ParMmg
. -dm_plex_metric_no_insert                 - Should node insertion/deletion be turned off?
. -dm_plex_metric_no_swap                   - Should facet swapping be turned off?
. -dm_plex_metric_no_move                   - Should node movement be turned off?
- -dm_plex_metric_verbosity                 - Choose a verbosity level from -1 (silent) to 10 (maximum).

.seealso: DMPlexMetricCreateUniform(), DMPlexMetricCreateIsotropic()
@*/
PetscErrorCode DMPlexMetricCreate(DM dm, PetscInt f, Vec *metric)
{
  DM_Plex       *plex = (DM_Plex *) dm->data;
  PetscBool      isotropic, uniform;
  PetscErrorCode ierr;
  PetscInt       coordDim, Nd;

  PetscFunctionBegin;
  if (!plex->metricCtx) {
    ierr = PetscNew(&plex->metricCtx);CHKERRQ(ierr);
    ierr = DMPlexMetricSetFromOptions(dm);CHKERRQ(ierr);
  }
  ierr = DMGetCoordinateDim(dm, &coordDim);CHKERRQ(ierr);
  Nd = coordDim*coordDim;
  ierr = DMPlexMetricIsUniform(dm, &uniform);CHKERRQ(ierr);
  ierr = DMPlexMetricIsIsotropic(dm, &isotropic);CHKERRQ(ierr);
  if (uniform) {
    MPI_Comm comm;

    ierr = PetscObjectGetComm((PetscObject) dm, &comm);CHKERRQ(ierr);
    PetscCheckFalse(!isotropic,comm, PETSC_ERR_ARG_WRONG, "Uniform anisotropic metrics not supported");
    ierr = VecCreate(comm, metric);CHKERRQ(ierr);
    ierr = VecSetSizes(*metric, 1, PETSC_DECIDE);CHKERRQ(ierr);
    ierr = VecSetFromOptions(*metric);CHKERRQ(ierr);
  } else if (isotropic) {
    ierr = DMPlexP1FieldCreate_Private(dm, f, 1, metric);CHKERRQ(ierr);
  } else {
    ierr = DMPlexP1FieldCreate_Private(dm, f, Nd, metric);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

/*@
  DMPlexMetricCreateUniform - Construct a uniform isotropic metric

  Input parameters:
+ dm     - The DM
. f      - The field number to use
- alpha  - Scaling parameter for the diagonal

  Output parameter:
. metric - The uniform metric

  Level: beginner

  Note: In this case, the metric is constant in space and so is only specified for a single vertex.

.seealso: DMPlexMetricCreate(), DMPlexMetricCreateIsotropic()
@*/
PetscErrorCode DMPlexMetricCreateUniform(DM dm, PetscInt f, PetscReal alpha, Vec *metric)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = DMPlexMetricSetUniform(dm, PETSC_TRUE);CHKERRQ(ierr);
  ierr = DMPlexMetricCreate(dm, f, metric);CHKERRQ(ierr);
  PetscCheck(alpha,PETSC_COMM_SELF, PETSC_ERR_ARG_WRONG, "Uniform metric scaling is undefined");
  PetscCheck(alpha >= 1.0e-30,PETSC_COMM_SELF, PETSC_ERR_ARG_WRONG, "Uniform metric scaling %e should be positive", alpha);
  ierr = VecSet(*metric, alpha);CHKERRQ(ierr);
  ierr = VecAssemblyBegin(*metric);CHKERRQ(ierr);
  ierr = VecAssemblyEnd(*metric);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

static void identity(PetscInt dim, PetscInt Nf, PetscInt NfAux,
                     const PetscInt uOff[], const PetscInt uOff_x[], const PetscScalar u[], const PetscScalar u_t[], const PetscScalar u_x[],
                     const PetscInt aOff[], const PetscInt aOff_x[], const PetscScalar a[], const PetscScalar a_t[], const PetscScalar a_x[],
                     PetscReal t, const PetscReal x[], PetscInt numConstants, const PetscScalar constants[], PetscScalar f0[])
{
  f0[0] = u[0];
}

/*@
  DMPlexMetricCreateIsotropic - Construct an isotropic metric from an error indicator

  Input parameters:
+ dm        - The DM
. f         - The field number to use
- indicator - The error indicator

  Output parameter:
. metric    - The isotropic metric

  Level: beginner

  Notes:

  It is assumed that the DM is comprised of simplices.

  The indicator needs to be a scalar field. If it is not defined vertex-wise, then it is projected appropriately.

.seealso: DMPlexMetricCreate(), DMPlexMetricCreateUniform()
@*/
PetscErrorCode DMPlexMetricCreateIsotropic(DM dm, PetscInt f, Vec indicator, Vec *metric)
{
  PetscErrorCode ierr;
  PetscInt       m, n;

  PetscFunctionBegin;
  ierr = DMPlexMetricSetIsotropic(dm, PETSC_TRUE);CHKERRQ(ierr);
  ierr = DMPlexMetricCreate(dm, f, metric);CHKERRQ(ierr);
  ierr = VecGetSize(indicator, &m);CHKERRQ(ierr);
  ierr = VecGetSize(*metric, &n);CHKERRQ(ierr);
  if (m == n) { ierr = VecCopy(indicator, *metric);CHKERRQ(ierr); }
  else {
    DM     dmIndi;
    void (*funcs[1])(PetscInt, PetscInt, PetscInt,
                     const PetscInt[], const PetscInt[], const PetscScalar[], const PetscScalar[], const PetscScalar[],
                     const PetscInt[], const PetscInt[], const PetscScalar[], const PetscScalar[], const PetscScalar[],
                     PetscReal, const PetscReal[], PetscInt, const PetscScalar[], PetscScalar[]);

    ierr = VecGetDM(indicator, &dmIndi);CHKERRQ(ierr);
    funcs[0] = identity;
    ierr = DMProjectFieldLocal(dmIndi, 0.0, indicator, funcs, INSERT_VALUES, *metric);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

static PetscErrorCode LAPACKsyevFail(PetscInt dim, PetscScalar Mpos[])
{
  PetscInt i, j;

  PetscFunctionBegin;
  PetscPrintf(PETSC_COMM_SELF, "Failed to apply LAPACKsyev to the matrix\n");
  for (i = 0; i < dim; ++i) {
    if (i == 0) PetscPrintf(PETSC_COMM_SELF, "    [[");
    else        PetscPrintf(PETSC_COMM_SELF, "     [");
    for (j = 0; j < dim; ++j) {
      if (j < dim-1) PetscPrintf(PETSC_COMM_SELF, "%15.8e, ", Mpos[i*dim+j]);
      else           PetscPrintf(PETSC_COMM_SELF, "%15.8e", Mpos[i*dim+j]);
    }
    if (i < dim-1) PetscPrintf(PETSC_COMM_SELF, "]\n");
    else           PetscPrintf(PETSC_COMM_SELF, "]]\n");
  }
  PetscFunctionReturn(0);
}

static PetscErrorCode DMPlexMetricModify_Private(PetscInt dim, PetscReal h_min, PetscReal h_max, PetscReal a_max, PetscScalar Mp[], PetscScalar *detMp)
{
  PetscErrorCode ierr;
  PetscInt       i, j, k;
  PetscReal     *eigs, max_eig, l_min = 1.0/(h_max*h_max), l_max = 1.0/(h_min*h_min), la_min = 1.0/(a_max*a_max);
  PetscScalar   *Mpos;

  PetscFunctionBegin;
  ierr = PetscMalloc2(dim*dim, &Mpos, dim, &eigs);CHKERRQ(ierr);

  /* Symmetrize */
  for (i = 0; i < dim; ++i) {
    Mpos[i*dim+i] = Mp[i*dim+i];
    for (j = i+1; j < dim; ++j) {
      Mpos[i*dim+j] = 0.5*(Mp[i*dim+j] + Mp[j*dim+i]);
      Mpos[j*dim+i] = Mpos[i*dim+j];
    }
  }

  /* Compute eigendecomposition */
  if (dim == 1) {

    /* Isotropic case */
    eigs[0] = PetscRealPart(Mpos[0]);
    Mpos[0] = 1.0;
  } else {

    /* Anisotropic case */
    PetscScalar  *work;
    PetscBLASInt lwork;

    lwork = 5*dim;
    ierr = PetscMalloc1(5*dim, &work);CHKERRQ(ierr);
    {
      PetscBLASInt lierr;
      PetscBLASInt nb;

      ierr = PetscBLASIntCast(dim, &nb);CHKERRQ(ierr);
      ierr = PetscFPTrapPush(PETSC_FP_TRAP_OFF);CHKERRQ(ierr);
#if defined(PETSC_USE_COMPLEX)
      {
        PetscReal *rwork;
        ierr = PetscMalloc1(3*dim, &rwork);CHKERRQ(ierr);
        PetscStackCallBLAS("LAPACKsyev",LAPACKsyev_("V","U",&nb,Mpos,&nb,eigs,work,&lwork,rwork,&lierr));
        ierr = PetscFree(rwork);CHKERRQ(ierr);
      }
#else
      PetscStackCallBLAS("LAPACKsyev",LAPACKsyev_("V","U",&nb,Mpos,&nb,eigs,work,&lwork,&lierr));
#endif
      if (lierr) {
        for (i = 0; i < dim; ++i) {
          Mpos[i*dim+i] = Mp[i*dim+i];
          for (j = i+1; j < dim; ++j) {
            Mpos[i*dim+j] = 0.5*(Mp[i*dim+j] + Mp[j*dim+i]);
            Mpos[j*dim+i] = Mpos[i*dim+j];
          }
        }
        LAPACKsyevFail(dim, Mpos);
        SETERRQ(PETSC_COMM_SELF, PETSC_ERR_LIB, "Error in LAPACK routine %d", (int) lierr);
      }
      ierr = PetscFPTrapPop();CHKERRQ(ierr);
    }
    ierr = PetscFree(work);CHKERRQ(ierr);
  }

  /* Reflect to positive orthant and enforce maximum and minimum size */
  max_eig = 0.0;
  for (i = 0; i < dim; ++i) {
    eigs[i] = PetscMin(l_max, PetscMax(l_min, PetscAbsReal(eigs[i])));
    max_eig = PetscMax(eigs[i], max_eig);
  }

  /* Enforce maximum anisotropy and compute determinant */
  *detMp = 1.0;
  for (i = 0; i < dim; ++i) {
    if (a_max > 1.0) eigs[i] = PetscMax(eigs[i], max_eig*la_min);
    *detMp *= eigs[i];
  }

  /* Reconstruct Hessian */
  for (i = 0; i < dim; ++i) {
    for (j = 0; j < dim; ++j) {
      Mp[i*dim+j] = 0.0;
      for (k = 0; k < dim; ++k) {
        Mp[i*dim+j] += Mpos[k*dim+i] * eigs[k] * Mpos[k*dim+j];
      }
    }
  }
  ierr = PetscFree2(Mpos, eigs);CHKERRQ(ierr);

  PetscFunctionReturn(0);
}

/*@
  DMPlexMetricEnforceSPD - Enforce symmetric positive-definiteness of a metric

  Input parameters:
+ dm                 - The DM
. metricIn           - The metric
. restrictSizes      - Should maximum/minimum metric magnitudes be enforced?
- restrictAnisotropy - Should maximum anisotropy be enforced?

  Output parameter:
+ metricOut          - The metric
- determinant        - Its determinant

  Level: beginner

  Notes:

  Relevant command line options:

+ -dm_plex_metric_isotropic - Is the metric isotropic?
. -dm_plex_metric_uniform   - Is the metric uniform?
. -dm_plex_metric_h_min     - Minimum tolerated metric magnitude
. -dm_plex_metric_h_max     - Maximum tolerated metric magnitude
- -dm_plex_metric_a_max     - Maximum tolerated anisotropy

.seealso: DMPlexMetricNormalize(), DMPlexMetricIntersection()
@*/
PetscErrorCode DMPlexMetricEnforceSPD(DM dm, Vec metricIn, PetscBool restrictSizes, PetscBool restrictAnisotropy, Vec *metricOut, Vec *determinant)
{
  DM             dmDet;
  PetscBool      isotropic, uniform;
  PetscErrorCode ierr;
  PetscInt       dim, vStart, vEnd, v;
  PetscScalar   *met, *det;
  PetscReal      h_min = 1.0e-30, h_max = 1.0e+30, a_max = 0.0;

  PetscFunctionBegin;
  ierr = PetscLogEventBegin(DMPLEX_MetricEnforceSPD,0,0,0,0);CHKERRQ(ierr);

  /* Extract metadata from dm */
  ierr = DMGetDimension(dm, &dim);CHKERRQ(ierr);
  if (restrictSizes) {
    ierr = DMPlexMetricGetMinimumMagnitude(dm, &h_min);CHKERRQ(ierr);
    ierr = DMPlexMetricGetMaximumMagnitude(dm, &h_max);CHKERRQ(ierr);
    h_min = PetscMax(h_min, 1.0e-30);
    h_max = PetscMin(h_max, 1.0e+30);
    PetscCheck(h_min < h_max,PETSC_COMM_SELF, PETSC_ERR_ARG_WRONG, "Incompatible min/max metric magnitudes (%.4e not smaller than %.4e)", h_min, h_max);
  }
  if (restrictAnisotropy) {
    ierr = DMPlexMetricGetMaximumAnisotropy(dm, &a_max);CHKERRQ(ierr);
    a_max = PetscMin(a_max, 1.0e+30);
  }

  /* Setup output metric */
  ierr = DMPlexMetricCreate(dm, 0, metricOut);CHKERRQ(ierr);
  ierr = VecCopy(metricIn, *metricOut);CHKERRQ(ierr);

  /* Enforce SPD and extract determinant */
  ierr = VecGetArray(*metricOut, &met);CHKERRQ(ierr);
  ierr = DMPlexMetricIsUniform(dm, &uniform);CHKERRQ(ierr);
  ierr = DMPlexMetricIsIsotropic(dm, &isotropic);CHKERRQ(ierr);
  if (uniform) {
    PetscCheck(isotropic, PETSC_COMM_SELF, PETSC_ERR_ARG_WRONG, "Uniform anisotropic metrics cannot exist");

    /* Uniform case */
    ierr = VecDuplicate(metricIn, determinant);CHKERRQ(ierr);
    ierr = VecGetArray(*determinant, &det);CHKERRQ(ierr);
    ierr = DMPlexMetricModify_Private(1, h_min, h_max, a_max, met, det);CHKERRQ(ierr);
    ierr = VecRestoreArray(*determinant, &det);CHKERRQ(ierr);
  } else {

    /* Spatially varying case */
    PetscInt nrow;

    if (isotropic) nrow = 1;
    else nrow = dim;
    ierr = DMClone(dm, &dmDet);CHKERRQ(ierr);
    ierr = DMPlexP1FieldCreate_Private(dmDet, 0, 1, determinant);CHKERRQ(ierr);
    ierr = DMPlexGetDepthStratum(dm, 0, &vStart, &vEnd);CHKERRQ(ierr);
    ierr = VecGetArray(*determinant, &det);CHKERRQ(ierr);
    for (v = vStart; v < vEnd; ++v) {
      PetscScalar *vmet, *vdet;
      ierr = DMPlexPointLocalRef(dm, v, met, &vmet);CHKERRQ(ierr);
      ierr = DMPlexPointLocalRef(dmDet, v, det, &vdet);CHKERRQ(ierr);
      ierr = DMPlexMetricModify_Private(nrow, h_min, h_max, a_max, vmet, vdet);CHKERRQ(ierr);
    }
    ierr = VecRestoreArray(*determinant, &det);CHKERRQ(ierr);
  }
  ierr = VecRestoreArray(*metricOut, &met);CHKERRQ(ierr);

  ierr = PetscLogEventEnd(DMPLEX_MetricEnforceSPD,0,0,0,0);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

static void detMFunc(PetscInt dim, PetscInt Nf, PetscInt NfAux,
                     const PetscInt uOff[], const PetscInt uOff_x[], const PetscScalar u[], const PetscScalar u_t[], const PetscScalar u_x[],
                     const PetscInt aOff[], const PetscInt aOff_x[], const PetscScalar a[], const PetscScalar a_t[], const PetscScalar a_x[],
                     PetscReal t, const PetscReal x[], PetscInt numConstants, const PetscScalar constants[], PetscScalar f0[])
{
  const PetscScalar p = constants[0];

  f0[0] = PetscPowReal(u[0], p/(2.0*p + dim));
}

/*@
  DMPlexMetricNormalize - Apply L-p normalization to a metric

  Input parameters:
+ dm                 - The DM
. metricIn           - The unnormalized metric
. restrictSizes      - Should maximum/minimum metric magnitudes be enforced?
- restrictAnisotropy - Should maximum metric anisotropy be enforced?

  Output parameter:
. metricOut          - The normalized metric

  Level: beginner

  Notes:

  Relevant command line options:

+ -dm_plex_metric_isotropic                 - Is the metric isotropic?
. -dm_plex_metric_uniform                   - Is the metric uniform?
. -dm_plex_metric_restrict_anisotropy_first - Should anisotropy be restricted before normalization?
. -dm_plex_metric_h_min                     - Minimum tolerated metric magnitude
. -dm_plex_metric_h_max                     - Maximum tolerated metric magnitude
. -dm_plex_metric_a_max                     - Maximum tolerated anisotropy
. -dm_plex_metric_p                         - L-p normalization order
- -dm_plex_metric_target_complexity         - Target metric complexity

.seealso: DMPlexMetricEnforceSPD(), DMPlexMetricIntersection()
@*/
PetscErrorCode DMPlexMetricNormalize(DM dm, Vec metricIn, PetscBool restrictSizes, PetscBool restrictAnisotropy, Vec *metricOut)
{
  DM               dmDet;
  MPI_Comm         comm;
  PetscBool        restrictAnisotropyFirst, isotropic, uniform;
  PetscDS          ds;
  PetscErrorCode   ierr;
  PetscInt         dim, Nd, vStart, vEnd, v, i;
  PetscScalar     *met, *det, integral, constants[1];
  PetscReal        p, h_min = 1.0e-30, h_max = 1.0e+30, a_max = 0.0, factGlob, fact, target, realIntegral;
  Vec              determinant;

  PetscFunctionBegin;
  ierr = PetscLogEventBegin(DMPLEX_MetricNormalize,0,0,0,0);CHKERRQ(ierr);

  /* Extract metadata from dm */
  ierr = PetscObjectGetComm((PetscObject) dm, &comm);CHKERRQ(ierr);
  ierr = DMGetDimension(dm, &dim);CHKERRQ(ierr);
  ierr = DMPlexMetricIsUniform(dm, &uniform);CHKERRQ(ierr);
  ierr = DMPlexMetricIsIsotropic(dm, &isotropic);CHKERRQ(ierr);
  if (isotropic) Nd = 1;
  else Nd = dim*dim;

  /* Set up metric and ensure it is SPD */
  ierr = DMPlexMetricRestrictAnisotropyFirst(dm, &restrictAnisotropyFirst);CHKERRQ(ierr);
  ierr = DMPlexMetricEnforceSPD(dm, metricIn, PETSC_FALSE, (PetscBool)(restrictAnisotropy && restrictAnisotropyFirst), metricOut, &determinant);CHKERRQ(ierr);

  /* Compute global normalization factor */
  ierr = DMPlexMetricGetTargetComplexity(dm, &target);CHKERRQ(ierr);
  ierr = DMPlexMetricGetNormalizationOrder(dm, &p);CHKERRQ(ierr);
  constants[0] = p;
  if (uniform) {
    PetscCheckFalse(!isotropic,comm, PETSC_ERR_ARG_WRONG, "Uniform anisotropic metrics not supported");
    DM  dmTmp;
    Vec tmp;

    ierr = DMClone(dm, &dmTmp);CHKERRQ(ierr);
    ierr = DMPlexP1FieldCreate_Private(dmTmp, 0, 1, &tmp);CHKERRQ(ierr);
    ierr = VecGetArray(determinant, &det);CHKERRQ(ierr);
    ierr = VecSet(tmp, det[0]);CHKERRQ(ierr);
    ierr = VecRestoreArray(determinant, &det);CHKERRQ(ierr);
    ierr = DMGetDS(dmTmp, &ds);CHKERRQ(ierr);
    ierr = PetscDSSetConstants(ds, 1, constants);CHKERRQ(ierr);
    ierr = PetscDSSetObjective(ds, 0, detMFunc);CHKERRQ(ierr);
    ierr = DMPlexComputeIntegralFEM(dmTmp, tmp, &integral, NULL);CHKERRQ(ierr);
    ierr = VecDestroy(&tmp);CHKERRQ(ierr);
    ierr = DMDestroy(&dmTmp);CHKERRQ(ierr);
  } else {
    ierr = VecGetDM(determinant, &dmDet);CHKERRQ(ierr);
    ierr = DMGetDS(dmDet, &ds);CHKERRQ(ierr);
    ierr = PetscDSSetConstants(ds, 1, constants);CHKERRQ(ierr);
    ierr = PetscDSSetObjective(ds, 0, detMFunc);CHKERRQ(ierr);
    ierr = DMPlexComputeIntegralFEM(dmDet, determinant, &integral, NULL);CHKERRQ(ierr);
  }
  realIntegral = PetscRealPart(integral);
  PetscCheckFalse(realIntegral < 1.0e-30,PETSC_COMM_SELF, PETSC_ERR_ARG_WRONG, "Global metric normalization factor should be strictly positive, not %.4e Is the input metric positive-definite?", realIntegral);
  factGlob = PetscPowReal(target/realIntegral, 2.0/dim);

  /* Apply local scaling */
  if (restrictSizes) {
    ierr = DMPlexMetricGetMinimumMagnitude(dm, &h_min);CHKERRQ(ierr);
    ierr = DMPlexMetricGetMaximumMagnitude(dm, &h_max);CHKERRQ(ierr);
    h_min = PetscMax(h_min, 1.0e-30);
    h_max = PetscMin(h_max, 1.0e+30);
    PetscCheckFalse(h_min >= h_max,PETSC_COMM_SELF, PETSC_ERR_ARG_WRONG, "Incompatible min/max metric magnitudes (%.4e not smaller than %.4e)", h_min, h_max);
  }
  if (restrictAnisotropy && !restrictAnisotropyFirst) {
    ierr = DMPlexMetricGetMaximumAnisotropy(dm, &a_max);CHKERRQ(ierr);
    a_max = PetscMin(a_max, 1.0e+30);
  }
  ierr = VecGetArray(*metricOut, &met);CHKERRQ(ierr);
  ierr = VecGetArray(determinant, &det);CHKERRQ(ierr);
  if (uniform) {

    /* Uniform case */
    met[0] *= factGlob * PetscPowReal(PetscAbsScalar(det[0]), -1.0/(2*p+dim));
    if (restrictSizes) { ierr = DMPlexMetricModify_Private(1, h_min, h_max, a_max, met, det);CHKERRQ(ierr); }
  } else {

    /* Spatially varying case */
    PetscInt nrow;

    if (isotropic) nrow = 1;
    else nrow = dim;
    ierr = DMPlexGetDepthStratum(dm, 0, &vStart, &vEnd);CHKERRQ(ierr);
    for (v = vStart; v < vEnd; ++v) {
      PetscScalar *Mp, *detM;

      ierr = DMPlexPointLocalRef(dm, v, met, &Mp);CHKERRQ(ierr);
      ierr = DMPlexPointLocalRef(dmDet, v, det, &detM);CHKERRQ(ierr);
      fact = factGlob * PetscPowReal(PetscAbsScalar(detM[0]), -1.0/(2*p+dim));
      for (i = 0; i < Nd; ++i) Mp[i] *= fact;
      if (restrictSizes) { ierr = DMPlexMetricModify_Private(nrow, h_min, h_max, a_max, Mp, detM);CHKERRQ(ierr); }
    }
  }
  ierr = VecRestoreArray(determinant, &det);CHKERRQ(ierr);
  ierr = VecRestoreArray(*metricOut, &met);CHKERRQ(ierr);
  ierr = VecDestroy(&determinant);CHKERRQ(ierr);
  if (!uniform) { ierr = DMDestroy(&dmDet);CHKERRQ(ierr); }

  ierr = PetscLogEventEnd(DMPLEX_MetricNormalize,0,0,0,0);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/*@
  DMPlexMetricAverage - Compute the average of a list of metrics

  Input Parameters:
+ dm         - The DM
. numMetrics - The number of metrics to be averaged
. weights    - Weights for the average
- metrics    - The metrics to be averaged

  Output Parameter:
. metricAvg  - The averaged metric

  Level: beginner

  Notes:
  The weights should sum to unity.

  If weights are not provided then an unweighted average is used.

.seealso: DMPlexMetricAverage2(), DMPlexMetricAverage3(), DMPlexMetricIntersection()
@*/
PetscErrorCode DMPlexMetricAverage(DM dm, PetscInt numMetrics, PetscReal weights[], Vec metrics[], Vec *metricAvg)
{
  PetscBool      haveWeights = PETSC_TRUE;
  PetscErrorCode ierr;
  PetscInt       i, m, n;
  PetscReal      sum = 0.0, tol = 1.0e-10;

  PetscFunctionBegin;
  ierr = PetscLogEventBegin(DMPLEX_MetricAverage,0,0,0,0);CHKERRQ(ierr);
  PetscCheck(numMetrics >= 1,PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Cannot average %d < 1 metrics", numMetrics);
  ierr = DMPlexMetricCreate(dm, 0, metricAvg);CHKERRQ(ierr);
  ierr = VecSet(*metricAvg, 0.0);CHKERRQ(ierr);
  ierr = VecGetSize(*metricAvg, &m);CHKERRQ(ierr);
  for (i = 0; i < numMetrics; ++i) {
    ierr = VecGetSize(metrics[i], &n);CHKERRQ(ierr);
    PetscCheckFalse(m != n,PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Averaging different metric types not implemented");
  }

  /* Default to the unweighted case */
  if (!weights) {
    ierr = PetscMalloc1(numMetrics, &weights);CHKERRQ(ierr);
    haveWeights = PETSC_FALSE;
    for (i = 0; i < numMetrics; ++i) {weights[i] = 1.0/numMetrics; }
  }

  /* Check weights sum to unity */
  for (i = 0; i < numMetrics; ++i) sum += weights[i];
  PetscCheckFalse(PetscAbsReal(sum - 1) > tol,PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Weights do not sum to unity");

  /* Compute metric average */
  for (i = 0; i < numMetrics; ++i) { ierr = VecAXPY(*metricAvg, weights[i], metrics[i]);CHKERRQ(ierr); }
  if (!haveWeights) { ierr = PetscFree(weights); }

  ierr = PetscLogEventEnd(DMPLEX_MetricAverage,0,0,0,0);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/*@
  DMPlexMetricAverage2 - Compute the unweighted average of two metrics

  Input Parameters:
+ dm         - The DM
. metric1    - The first metric to be averaged
- metric2    - The second metric to be averaged

  Output Parameter:
. metricAvg  - The averaged metric

  Level: beginner

.seealso: DMPlexMetricAverage(), DMPlexMetricAverage3()
@*/
PetscErrorCode DMPlexMetricAverage2(DM dm, Vec metric1, Vec metric2, Vec *metricAvg)
{
  PetscErrorCode ierr;
  PetscReal      weights[2] = {0.5, 0.5};
  Vec            metrics[2] = {metric1, metric2};

  PetscFunctionBegin;
  ierr = DMPlexMetricAverage(dm, 2, weights, metrics, metricAvg);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/*@
  DMPlexMetricAverage3 - Compute the unweighted average of three metrics

  Input Parameters:
+ dm         - The DM
. metric1    - The first metric to be averaged
. metric2    - The second metric to be averaged
- metric3    - The third metric to be averaged

  Output Parameter:
. metricAvg  - The averaged metric

  Level: beginner

.seealso: DMPlexMetricAverage(), DMPlexMetricAverage2()
@*/
PetscErrorCode DMPlexMetricAverage3(DM dm, Vec metric1, Vec metric2, Vec metric3, Vec *metricAvg)
{
  PetscErrorCode ierr;
  PetscReal      weights[3] = {1.0/3.0, 1.0/3.0, 1.0/3.0};
  Vec            metrics[3] = {metric1, metric2, metric3};

  PetscFunctionBegin;
  ierr = DMPlexMetricAverage(dm, 3, weights, metrics, metricAvg);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

static PetscErrorCode DMPlexMetricIntersection_Private(PetscInt dim, PetscScalar M1[], PetscScalar M2[])
{
  PetscErrorCode ierr;
  PetscInt       i, j, k, l, m;
  PetscReal     *evals, *evals1;
  PetscScalar   *evecs, *sqrtM1, *isqrtM1;

  PetscFunctionBegin;

  /* Isotropic case */
  if (dim == 1) {
    M2[0] = (PetscScalar)PetscMin(PetscRealPart(M1[0]), PetscRealPart(M2[0]));
    PetscFunctionReturn(0);
  }

  /* Anisotropic case */
  ierr = PetscMalloc5(dim*dim, &evecs, dim*dim, &sqrtM1, dim*dim, &isqrtM1, dim, &evals, dim, &evals1);CHKERRQ(ierr);
  for (i = 0; i < dim; ++i) {
    for (j = 0; j < dim; ++j) {
      evecs[i*dim+j] = M1[i*dim+j];
    }
  }
  {
    PetscScalar *work;
    PetscBLASInt lwork;

    lwork = 5*dim;
    ierr = PetscMalloc1(5*dim, &work);CHKERRQ(ierr);
    {
      PetscBLASInt lierr, nb;
      PetscReal    sqrtk;

      /* Compute eigendecomposition of M1 */
      ierr = PetscBLASIntCast(dim, &nb);CHKERRQ(ierr);
      ierr = PetscFPTrapPush(PETSC_FP_TRAP_OFF);CHKERRQ(ierr);
#if defined(PETSC_USE_COMPLEX)
      {
        PetscReal *rwork;
        ierr = PetscMalloc1(3*dim, &rwork);CHKERRQ(ierr);
        PetscStackCallBLAS("LAPACKsyev", LAPACKsyev_("V", "U", &nb, evecs, &nb, evals1, work, &lwork, rwork, &lierr));
        ierr = PetscFree(rwork);CHKERRQ(ierr);
      }
#else
      PetscStackCallBLAS("LAPACKsyev", LAPACKsyev_("V", "U", &nb, evecs, &nb, evals1, work, &lwork, &lierr));
#endif
      if (lierr) {
        LAPACKsyevFail(dim, M1);
        SETERRQ(PETSC_COMM_SELF, PETSC_ERR_LIB, "Error in LAPACK routine %d", (int) lierr);
      }
      ierr = PetscFPTrapPop();

      /* Compute square root and reciprocal */
      for (i = 0; i < dim; ++i) {
        for (j = 0; j < dim; ++j) {
          sqrtM1[i*dim+j] = 0.0;
          isqrtM1[i*dim+j] = 0.0;
          for (k = 0; k < dim; ++k) {
            sqrtk = PetscSqrtReal(evals1[k]);
            sqrtM1[i*dim+j] += evecs[k*dim+i] * sqrtk * evecs[k*dim+j];
            isqrtM1[i*dim+j] += evecs[k*dim+i] * (1.0/sqrtk) * evecs[k*dim+j];
          }
        }
      }

      /* Map into the space spanned by the eigenvectors of M1 */
      for (i = 0; i < dim; ++i) {
        for (j = 0; j < dim; ++j) {
          evecs[i*dim+j] = 0.0;
          for (k = 0; k < dim; ++k) {
            for (l = 0; l < dim; ++l) {
              evecs[i*dim+j] += isqrtM1[i*dim+k] * M2[l*dim+k] * isqrtM1[j*dim+l];
            }
          }
        }
      }

      /* Compute eigendecomposition */
      ierr = PetscFPTrapPush(PETSC_FP_TRAP_OFF);CHKERRQ(ierr);
#if defined(PETSC_USE_COMPLEX)
      {
        PetscReal *rwork;
        ierr = PetscMalloc1(3*dim, &rwork);CHKERRQ(ierr);
        PetscStackCallBLAS("LAPACKsyev", LAPACKsyev_("V", "U", &nb, evecs, &nb, evals, work, &lwork, rwork, &lierr));
        ierr = PetscFree(rwork);CHKERRQ(ierr);
      }
#else
      PetscStackCallBLAS("LAPACKsyev", LAPACKsyev_("V", "U", &nb, evecs, &nb, evals, work, &lwork, &lierr));
#endif
      if (lierr) {
        for (i = 0; i < dim; ++i) {
          for (j = 0; j < dim; ++j) {
            evecs[i*dim+j] = 0.0;
            for (k = 0; k < dim; ++k) {
              for (l = 0; l < dim; ++l) {
                evecs[i*dim+j] += isqrtM1[i*dim+k] * M2[l*dim+k] * isqrtM1[j*dim+l];
              }
            }
          }
        }
        LAPACKsyevFail(dim, evecs);
        SETERRQ(PETSC_COMM_SELF, PETSC_ERR_LIB, "Error in LAPACK routine %d", (int) lierr);
      }
      ierr = PetscFPTrapPop();

      /* Modify eigenvalues */
      for (i = 0; i < dim; ++i) evals[i] = PetscMin(evals[i], evals1[i]);

      /* Map back to get the intersection */
      for (i = 0; i < dim; ++i) {
        for (j = 0; j < dim; ++j) {
          M2[i*dim+j] = 0.0;
          for (k = 0; k < dim; ++k) {
            for (l = 0; l < dim; ++l) {
              for (m = 0; m < dim; ++m) {
                M2[i*dim+j] += sqrtM1[i*dim+k] * evecs[l*dim+k] * evals[l] * evecs[l*dim+m] * sqrtM1[j*dim+m];
              }
            }
          }
        }
      }
    }
    ierr = PetscFree(work);CHKERRQ(ierr);
  }
  ierr = PetscFree5(evecs, sqrtM1, isqrtM1, evals, evals1);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/*@
  DMPlexMetricIntersection - Compute the intersection of a list of metrics

  Input Parameters:
+ dm         - The DM
. numMetrics - The number of metrics to be intersected
- metrics    - The metrics to be intersected

  Output Parameter:
. metricInt  - The intersected metric

  Level: beginner

  Notes:

  The intersection of a list of metrics has the maximal ellipsoid which fits within the ellipsoids of the component metrics.

  The implementation used here is only consistent with the maximal ellipsoid definition in the case numMetrics = 2.

.seealso: DMPlexMetricIntersection2(), DMPlexMetricIntersection3(), DMPlexMetricAverage()
@*/
PetscErrorCode DMPlexMetricIntersection(DM dm, PetscInt numMetrics, Vec metrics[], Vec *metricInt)
{
  PetscBool      isotropic, uniform;
  PetscErrorCode ierr;
  PetscInt       v, i, m, n;
  PetscScalar   *met, *meti;

  PetscFunctionBegin;
  ierr = PetscLogEventBegin(DMPLEX_MetricIntersection,0,0,0,0);CHKERRQ(ierr);
  PetscCheck(numMetrics >= 1,PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Cannot intersect %d < 1 metrics", numMetrics);

  /* Copy over the first metric */
  ierr = DMPlexMetricCreate(dm, 0, metricInt);CHKERRQ(ierr);
  ierr = VecCopy(metrics[0], *metricInt);CHKERRQ(ierr);
  if (numMetrics == 1) PetscFunctionReturn(0);
  ierr = VecGetSize(*metricInt, &m);CHKERRQ(ierr);
  for (i = 0; i < numMetrics; ++i) {
    ierr = VecGetSize(metrics[i], &n);CHKERRQ(ierr);
    PetscCheckFalse(m != n,PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Intersecting different metric types not implemented");
  }

  /* Intersect subsequent metrics in turn */
  ierr = DMPlexMetricIsUniform(dm, &uniform);CHKERRQ(ierr);
  ierr = DMPlexMetricIsIsotropic(dm, &isotropic);CHKERRQ(ierr);
  if (uniform) {

    /* Uniform case */
    ierr = VecGetArray(*metricInt, &met);CHKERRQ(ierr);
    for (i = 1; i < numMetrics; ++i) {
      ierr = VecGetArray(metrics[i], &meti);CHKERRQ(ierr);
      ierr = DMPlexMetricIntersection_Private(1, meti, met);CHKERRQ(ierr);
      ierr = VecRestoreArray(metrics[i], &meti);CHKERRQ(ierr);
    }
    ierr = VecRestoreArray(*metricInt, &met);CHKERRQ(ierr);
  } else {

    /* Spatially varying case */
    PetscInt     dim, vStart, vEnd, nrow;
    PetscScalar *M, *Mi;

    ierr = DMGetDimension(dm, &dim);CHKERRQ(ierr);
    if (isotropic) nrow = 1;
    else nrow = dim;
    ierr = DMPlexGetDepthStratum(dm, 0, &vStart, &vEnd);CHKERRQ(ierr);
    ierr = VecGetArray(*metricInt, &met);CHKERRQ(ierr);
    for (i = 1; i < numMetrics; ++i) {
      ierr = VecGetArray(metrics[i], &meti);CHKERRQ(ierr);
      for (v = vStart; v < vEnd; ++v) {
        ierr = DMPlexPointLocalRef(dm, v, met, &M);CHKERRQ(ierr);
        ierr = DMPlexPointLocalRef(dm, v, meti, &Mi);CHKERRQ(ierr);
        ierr = DMPlexMetricIntersection_Private(nrow, Mi, M);CHKERRQ(ierr);
      }
      ierr = VecRestoreArray(metrics[i], &meti);CHKERRQ(ierr);
    }
    ierr = VecRestoreArray(*metricInt, &met);CHKERRQ(ierr);
  }

  ierr = PetscLogEventEnd(DMPLEX_MetricIntersection,0,0,0,0);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/*@
  DMPlexMetricIntersection2 - Compute the intersection of two metrics

  Input Parameters:
+ dm        - The DM
. metric1   - The first metric to be intersected
- metric2   - The second metric to be intersected

  Output Parameter:
. metricInt - The intersected metric

  Level: beginner

.seealso: DMPlexMetricIntersection(), DMPlexMetricIntersection3()
@*/
PetscErrorCode DMPlexMetricIntersection2(DM dm, Vec metric1, Vec metric2, Vec *metricInt)
{
  PetscErrorCode ierr;
  Vec            metrics[2] = {metric1, metric2};

  PetscFunctionBegin;
  ierr = DMPlexMetricIntersection(dm, 2, metrics, metricInt);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/*@
  DMPlexMetricIntersection3 - Compute the intersection of three metrics

  Input Parameters:
+ dm        - The DM
. metric1   - The first metric to be intersected
. metric2   - The second metric to be intersected
- metric3   - The third metric to be intersected

  Output Parameter:
. metricInt - The intersected metric

  Level: beginner

.seealso: DMPlexMetricIntersection(), DMPlexMetricIntersection2()
@*/
PetscErrorCode DMPlexMetricIntersection3(DM dm, Vec metric1, Vec metric2, Vec metric3, Vec *metricInt)
{
  PetscErrorCode ierr;
  Vec            metrics[3] = {metric1, metric2, metric3};

  PetscFunctionBegin;
  ierr = DMPlexMetricIntersection(dm, 3, metrics, metricInt);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}
