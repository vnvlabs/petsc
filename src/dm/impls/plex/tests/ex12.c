static char help[] = "Partition a mesh in parallel, perhaps with overlap\n\n";

#include <petscdmplex.h>
#include <petscsf.h>

/* Sample usage:

Load a file in serial and distribute it on 24 processes:

  make -f ./gmakefile test globsearch="dm_impls_plex_tests-ex12_0" EXTRA_OPTIONS="-filename $PETSC_DIR/share/petsc/datafiles/meshes/squaremotor-30.exo -orig_dm_view -dm_view" NP=24

Load a file in serial and distribute it on 24 processes using a custom partitioner:

  make -f ./gmakefile test globsearch="dm_impls_plex_tests-ex12_0" EXTRA_OPTIONS="-filename $PETSC_DIR/share/petsc/datafiles/meshes/cylinder.med -petscpartitioner_type simple -orig_dm_view -dm_view" NP=24

Load a file in serial, distribute it, and then redistribute it on 24 processes using two different partitioners:

  make -f ./gmakefile test globsearch="dm_impls_plex_tests-ex12_0" EXTRA_OPTIONS="-filename $PETSC_DIR/share/petsc/datafiles/meshes/squaremotor-30.exo -petscpartitioner_type simple -load_balance -lb_petscpartitioner_type parmetis -orig_dm_view -dm_view" NP=24

Load a file in serial, distribute it randomly, refine it in parallel, and then redistribute it on 24 processes using two different partitioners, and view to VTK:

  make -f ./gmakefile test globsearch="dm_impls_plex_tests-ex12_0" EXTRA_OPTIONS="-filename $PETSC_DIR/share/petsc/datafiles/meshes/squaremotor-30.exo -petscpartitioner_type shell -petscpartitioner_shell_random -dm_refine 1 -load_balance -lb_petscpartitioner_type parmetis -prelb_dm_view vtk:$PWD/prelb.vtk -dm_view vtk:$PWD/balance.vtk -dm_partition_view" NP=24

*/

enum {STAGE_LOAD, STAGE_DISTRIBUTE, STAGE_REFINE, STAGE_REDISTRIBUTE};

typedef struct {
  /* Domain and mesh definition */
  PetscInt  overlap;                      /* The cell overlap to use during partitioning */
  PetscBool testPartition;                /* Use a fixed partitioning for testing */
  PetscBool testRedundant;                /* Use a redundant partitioning for testing */
  PetscBool loadBalance;                  /* Load balance via a second distribute step */
  PetscBool partitionBalance;             /* Balance shared point partition */
  PetscLogStage stages[4];
} AppCtx;

PetscErrorCode ProcessOptions(MPI_Comm comm, AppCtx *options)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  options->overlap          = 0;
  options->testPartition    = PETSC_FALSE;
  options->testRedundant    = PETSC_FALSE;
  options->loadBalance      = PETSC_FALSE;
  options->partitionBalance = PETSC_FALSE;

  ierr = PetscOptionsBegin(comm, "", "Meshing Problem Options", "DMPLEX");CHKERRQ(ierr);
  ierr = PetscOptionsBoundedInt("-overlap", "The cell overlap for partitioning", "ex12.c", options->overlap, &options->overlap, NULL,0);CHKERRQ(ierr);
  ierr = PetscOptionsBool("-test_partition", "Use a fixed partition for testing", "ex12.c", options->testPartition, &options->testPartition, NULL);CHKERRQ(ierr);
  ierr = PetscOptionsBool("-test_redundant", "Use a redundant partition for testing", "ex12.c", options->testRedundant, &options->testRedundant, NULL);CHKERRQ(ierr);
  ierr = PetscOptionsBool("-load_balance", "Perform parallel load balancing in a second distribution step", "ex12.c", options->loadBalance, &options->loadBalance, NULL);CHKERRQ(ierr);
  ierr = PetscOptionsBool("-partition_balance", "Balance the ownership of shared points", "ex12.c", options->partitionBalance, &options->partitionBalance, NULL);CHKERRQ(ierr);
  ierr = PetscOptionsEnd();CHKERRQ(ierr);

  ierr = PetscLogStageRegister("MeshLoad",         &options->stages[STAGE_LOAD]);CHKERRQ(ierr);
  ierr = PetscLogStageRegister("MeshDistribute",   &options->stages[STAGE_DISTRIBUTE]);CHKERRQ(ierr);
  ierr = PetscLogStageRegister("MeshRefine",       &options->stages[STAGE_REFINE]);CHKERRQ(ierr);
  ierr = PetscLogStageRegister("MeshRedistribute", &options->stages[STAGE_REDISTRIBUTE]);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

PetscErrorCode CreateMesh(MPI_Comm comm, AppCtx *user, DM *dm)
{
  DM             pdm             = NULL;
  PetscInt       triSizes_n2[2]  = {4, 4};
  PetscInt       triPoints_n2[8] = {0, 1, 4, 6, 2, 3, 5, 7};
  PetscInt       triSizes_n3[3]  = {3, 2, 3};
  PetscInt       triPoints_n3[8] = {3, 5, 6, 1, 7, 0, 2, 4};
  PetscInt       triSizes_n4[4]  = {2, 2, 2, 2};
  PetscInt       triPoints_n4[8] = {0, 7, 1, 5, 2, 3, 4, 6};
  PetscInt       triSizes_n8[8]  = {1, 1, 1, 1, 1, 1, 1, 1};
  PetscInt       triPoints_n8[8] = {0, 1, 2, 3, 4, 5, 6, 7};
  PetscInt       quadSizes[2]    = {2, 2};
  PetscInt       quadPoints[4]   = {2, 3, 0, 1};
  PetscInt       overlap         = user->overlap >= 0 ? user->overlap : 0;
  PetscInt       dim;
  PetscBool      simplex;
  PetscMPIInt    rank, size;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = MPI_Comm_rank(comm, &rank);CHKERRMPI(ierr);
  ierr = MPI_Comm_size(comm, &size);CHKERRMPI(ierr);
  ierr = PetscLogStagePush(user->stages[STAGE_LOAD]);CHKERRQ(ierr);
  ierr = DMCreate(comm, dm);CHKERRQ(ierr);
  ierr = DMSetType(*dm, DMPLEX);CHKERRQ(ierr);
  ierr = DMPlexDistributeSetDefault(*dm, PETSC_FALSE);CHKERRQ(ierr);
  ierr = DMSetFromOptions(*dm);CHKERRQ(ierr);
  ierr = DMViewFromOptions(*dm, NULL, "-orig_dm_view");CHKERRQ(ierr);
  ierr = PetscLogStagePop();CHKERRQ(ierr);
  ierr = DMGetDimension(*dm, &dim);CHKERRQ(ierr);
  ierr = DMPlexIsSimplex(*dm, &simplex);CHKERRQ(ierr);
  ierr = PetscLogStagePush(user->stages[STAGE_DISTRIBUTE]);CHKERRQ(ierr);
  if (!user->testRedundant) {
    PetscPartitioner part;

    ierr = DMPlexGetPartitioner(*dm, &part);CHKERRQ(ierr);
    ierr = PetscPartitionerSetFromOptions(part);CHKERRQ(ierr);
    ierr = DMPlexSetPartitionBalance(*dm, user->partitionBalance);CHKERRQ(ierr);
    if (user->testPartition) {
      const PetscInt *sizes = NULL;
      const PetscInt *points = NULL;

      if (rank == 0) {
        if (dim == 2 && simplex && size == 2) {
          sizes = triSizes_n2; points = triPoints_n2;
        } else if (dim == 2 && simplex && size == 3) {
          sizes = triSizes_n3; points = triPoints_n3;
        } else if (dim == 2 && simplex && size == 4) {
          sizes = triSizes_n4; points = triPoints_n4;
        } else if (dim == 2 && simplex && size == 8) {
          sizes = triSizes_n8; points = triPoints_n8;
        } else if (dim == 2 && !simplex && size == 2) {
          sizes = quadSizes; points = quadPoints;
        }
      }
      ierr = PetscPartitionerSetType(part, PETSCPARTITIONERSHELL);CHKERRQ(ierr);
      ierr = PetscPartitionerShellSetPartition(part, size, sizes, points);CHKERRQ(ierr);
    }
    ierr = DMPlexDistribute(*dm, overlap, NULL, &pdm);CHKERRQ(ierr);
  } else {
    PetscSF sf;

    ierr = DMPlexGetRedundantDM(*dm, &sf, &pdm);CHKERRQ(ierr);
    if (sf) {
      DM test;

      ierr = DMPlexCreate(comm,&test);CHKERRQ(ierr);
      ierr = PetscObjectSetName((PetscObject)test, "Test SF-migrated Redundant Mesh");CHKERRQ(ierr);
      ierr = DMPlexMigrate(*dm, sf, test);CHKERRQ(ierr);
      ierr = DMViewFromOptions(test, NULL, "-redundant_migrated_dm_view");CHKERRQ(ierr);
      ierr = DMDestroy(&test);CHKERRQ(ierr);
    }
    ierr = PetscSFDestroy(&sf);CHKERRQ(ierr);
  }
  if (pdm) {
    ierr = DMDestroy(dm);CHKERRQ(ierr);
    *dm  = pdm;
  }
  ierr = PetscLogStagePop();CHKERRQ(ierr);
  ierr = DMSetFromOptions(*dm);CHKERRQ(ierr);
  if (user->loadBalance) {
    PetscPartitioner part;

    ierr = DMViewFromOptions(*dm, NULL, "-prelb_dm_view");CHKERRQ(ierr);
    ierr = DMPlexSetOptionsPrefix(*dm, "lb_");CHKERRQ(ierr);
    ierr = PetscLogStagePush(user->stages[STAGE_REDISTRIBUTE]);CHKERRQ(ierr);
    ierr = DMPlexGetPartitioner(*dm, &part);CHKERRQ(ierr);
    ierr = PetscObjectSetOptionsPrefix((PetscObject) part, "lb_");CHKERRQ(ierr);
    ierr = PetscPartitionerSetFromOptions(part);CHKERRQ(ierr);
    if (user->testPartition) {
      PetscInt         reSizes_n2[2]  = {2, 2};
      PetscInt         rePoints_n2[4] = {2, 3, 0, 1};
      if (rank) {rePoints_n2[0] = 1; rePoints_n2[1] = 2, rePoints_n2[2] = 0, rePoints_n2[3] = 3;}

      ierr = PetscPartitionerSetType(part, PETSCPARTITIONERSHELL);CHKERRQ(ierr);
      ierr = PetscPartitionerShellSetPartition(part, size, reSizes_n2, rePoints_n2);CHKERRQ(ierr);
    }
    ierr = DMPlexSetPartitionBalance(*dm, user->partitionBalance);CHKERRQ(ierr);
    ierr = DMPlexDistribute(*dm, overlap, NULL, &pdm);CHKERRQ(ierr);
    if (pdm) {
      ierr = DMDestroy(dm);CHKERRQ(ierr);
      *dm  = pdm;
    }
    ierr = PetscLogStagePop();CHKERRQ(ierr);
  }
  ierr = PetscLogStagePush(user->stages[STAGE_REFINE]);CHKERRQ(ierr);
  ierr = DMViewFromOptions(*dm, NULL, "-dm_view");CHKERRQ(ierr);
  ierr = PetscLogStagePop();CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

int main(int argc, char **argv)
{
  DM             dm;
  AppCtx         user; /* user-defined work context */
  PetscErrorCode ierr;

  ierr = PetscInitialize(&argc, &argv, NULL, help);if (ierr) return ierr;
  ierr = ProcessOptions(PETSC_COMM_WORLD, &user);CHKERRQ(ierr);
  ierr = CreateMesh(PETSC_COMM_WORLD, &user, &dm);CHKERRQ(ierr);
  ierr = DMDestroy(&dm);CHKERRQ(ierr);
  ierr = PetscFinalize();
  return ierr;
}

/*TEST
  # Parallel, no overlap tests 0-2
  test:
    suffix: 0
    requires: triangle
    args: -dm_coord_space 0 -dm_view ascii:mesh.tex:ascii_latex
  test:
    suffix: 1
    requires: triangle
    nsize: 3
    args: -dm_coord_space 0 -test_partition -dm_view ascii::ascii_info_detail
  test:
    suffix: 2
    requires: triangle
    nsize: 8
    args: -dm_coord_space 0 -test_partition -dm_view ascii::ascii_info_detail
  # Parallel, level-1 overlap tests 3-4
  test:
    suffix: 3
    requires: triangle
    nsize: 3
    args: -dm_coord_space 0 -test_partition -overlap 1 -dm_view ascii::ascii_info_detail
  test:
    suffix: 4
    requires: triangle
    nsize: 8
    args: -dm_coord_space 0 -test_partition -overlap 1 -dm_view ascii::ascii_info_detail
  # Parallel, level-2 overlap test 5
  test:
    suffix: 5
    requires: triangle
    nsize: 8
    args: -dm_coord_space 0 -test_partition -overlap 2 -dm_view ascii::ascii_info_detail
  # Parallel load balancing, test 6-7
  test:
    suffix: 6
    requires: triangle
    nsize: 2
    args: -dm_coord_space 0 -test_partition -overlap 1 -dm_view ascii::ascii_info_detail
  test:
    suffix: 7
    requires: triangle
    nsize: 2
    args: -dm_coord_space 0 -test_partition -overlap 1 -load_balance -dm_view ascii::ascii_info_detail
  # Parallel redundant copying, test 8
  test:
    suffix: 8
    requires: triangle
    nsize: 2
    args: -dm_coord_space 0 -test_redundant -redundant_migrated_dm_view ascii::ascii_info_detail -dm_view ascii::ascii_info_detail
  test:
    suffix: lb_0
    requires: parmetis
    nsize: 4
    args: -dm_coord_space 0 -dm_plex_simplex 0 -dm_plex_box_faces 4,4 -petscpartitioner_type shell -petscpartitioner_shell_random -lb_petscpartitioner_type parmetis -load_balance -lb_petscpartitioner_view -prelb_dm_view ::load_balance -dm_view ::load_balance

  # Same tests as above, but with balancing of the shared point partition
  test:
    suffix: 9
    requires: triangle
    args: -dm_coord_space 0 -dm_view ascii:mesh.tex:ascii_latex -partition_balance
  test:
    suffix: 10
    requires: triangle
    nsize: 3
    args: -dm_coord_space 0 -test_partition -dm_view ascii::ascii_info_detail -partition_balance
  test:
    suffix: 11
    requires: triangle
    nsize: 8
    args: -dm_coord_space 0 -test_partition -dm_view ascii::ascii_info_detail -partition_balance
  # Parallel, level-1 overlap tests 3-4
  test:
    suffix: 12
    requires: triangle
    nsize: 3
    args: -dm_coord_space 0 -test_partition -overlap 1 -dm_view ascii::ascii_info_detail -partition_balance
  test:
    suffix: 13
    requires: triangle
    nsize: 8
    args: -dm_coord_space 0 -test_partition -overlap 1 -dm_view ascii::ascii_info_detail -partition_balance
  # Parallel, level-2 overlap test 5
  test:
    suffix: 14
    requires: triangle
    nsize: 8
    args: -dm_coord_space 0 -test_partition -overlap 2 -dm_view ascii::ascii_info_detail -partition_balance
  # Parallel load balancing, test 6-7
  test:
    suffix: 15
    requires: triangle
    nsize: 2
    args: -dm_coord_space 0 -test_partition -overlap 1 -dm_view ascii::ascii_info_detail -partition_balance
  test:
    suffix: 16
    requires: triangle
    nsize: 2
    args: -dm_coord_space 0 -test_partition -overlap 1 -load_balance -dm_view ascii::ascii_info_detail -partition_balance
  # Parallel redundant copying, test 8
  test:
    suffix: 17
    requires: triangle
    nsize: 2
    args: -dm_coord_space 0 -test_redundant -dm_view ascii::ascii_info_detail -partition_balance
  test:
    suffix: lb_1
    requires: parmetis
    nsize: 4
    args: -dm_coord_space 0 -dm_plex_simplex 0 -dm_plex_box_faces 4,4 -petscpartitioner_type shell -petscpartitioner_shell_random -lb_petscpartitioner_type parmetis -load_balance -lb_petscpartitioner_view -partition_balance -prelb_dm_view ::load_balance -dm_view ::load_balance
TEST*/
