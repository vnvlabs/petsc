#include <petsc/private/dmpleximpl.h>   /*I      "petscdmplex.h"   I*/

/*@C
  DMPlexInvertCell - Flips cell orientations since Plex stores some of them internally with outward normals.

  Input Parameters:
+ cellType - The cell type
- cone - The incoming cone

  Output Parameter:
. cone - The inverted cone (in-place)

  Level: developer

.seealso: DMPlexGenerate()
@*/
PetscErrorCode DMPlexInvertCell(DMPolytopeType cellType, PetscInt cone[])
{
#define SWAPCONE(cone,i,j)  \
  do {                      \
    PetscInt _cone_tmp;     \
    _cone_tmp = (cone)[i];  \
    (cone)[i] = (cone)[j];  \
    (cone)[j] = _cone_tmp;  \
  } while (0)

  PetscFunctionBegin;
  switch (cellType) {
  case DM_POLYTOPE_POINT:              break;
  case DM_POLYTOPE_SEGMENT:            break;
  case DM_POLYTOPE_POINT_PRISM_TENSOR: break;
  case DM_POLYTOPE_TRIANGLE:           break;
  case DM_POLYTOPE_QUADRILATERAL:      break;
  case DM_POLYTOPE_SEG_PRISM_TENSOR:   SWAPCONE(cone,2,3); break;
  case DM_POLYTOPE_TETRAHEDRON:        SWAPCONE(cone,0,1); break;
  case DM_POLYTOPE_HEXAHEDRON:         SWAPCONE(cone,1,3); break;
  case DM_POLYTOPE_TRI_PRISM:          SWAPCONE(cone,1,2); break;
  case DM_POLYTOPE_TRI_PRISM_TENSOR:   break;
  case DM_POLYTOPE_QUAD_PRISM_TENSOR:  break;
  default: break;
  }
  PetscFunctionReturn(0);
#undef SWAPCONE
}

/*@C
  DMPlexReorderCell - Flips cell orientations since Plex stores some of them internally with outward normals.

  Input Parameters:
+ dm - The DMPlex object
. cell - The cell
- cone - The incoming cone

  Output Parameter:
. cone - The reordered cone (in-place)

  Level: developer

.seealso: DMPlexGenerate()
@*/
PetscErrorCode DMPlexReorderCell(DM dm, PetscInt cell, PetscInt cone[])
{
  DMPolytopeType cellType;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = DMPlexGetCellType(dm, cell, &cellType);CHKERRQ(ierr);
  ierr = DMPlexInvertCell(cellType, cone);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/*@C
  DMPlexTriangleSetOptions - Set the options used for the Triangle mesh generator

  Not Collective

  Inputs Parameters:
+ dm - The DMPlex object
- opts - The command line options

  Level: developer

.seealso: DMPlexTetgenSetOptions(), DMPlexGenerate()
@*/
PetscErrorCode DMPlexTriangleSetOptions(DM dm, const char *opts)
{
  DM_Plex       *mesh = (DM_Plex*) dm->data;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 1);
  PetscValidPointer(opts, 2);
  ierr = PetscFree(mesh->triangleOpts);CHKERRQ(ierr);
  ierr = PetscStrallocpy(opts, &mesh->triangleOpts);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/*@C
  DMPlexTetgenSetOptions - Set the options used for the Tetgen mesh generator

  Not Collective

  Inputs Parameters:
+ dm - The DMPlex object
- opts - The command line options

  Level: developer

.seealso: DMPlexTriangleSetOptions(), DMPlexGenerate()
@*/
PetscErrorCode DMPlexTetgenSetOptions(DM dm, const char *opts)
{
  DM_Plex       *mesh = (DM_Plex*) dm->data;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 1);
  PetscValidPointer(opts, 2);
  ierr = PetscFree(mesh->tetgenOpts);CHKERRQ(ierr);
  ierr = PetscStrallocpy(opts, &mesh->tetgenOpts);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/*@C
  DMPlexGenerate - Generates a mesh.

  Not Collective

  Input Parameters:
+ boundary - The DMPlex boundary object
. name - The mesh generation package name
- interpolate - Flag to create intermediate mesh elements

  Output Parameter:
. mesh - The DMPlex object

  Options Database:
+  -dm_plex_generate <name> - package to generate mesh, for example, triangle, ctetgen or tetgen
-  -dm_generator <name> - package to generate mesh, for example, triangle, ctetgen or tetgen

  Level: intermediate

.seealso: DMPlexCreate(), DMRefine()
@*/
PetscErrorCode DMPlexGenerate(DM boundary, const char name[], PetscBool interpolate, DM *mesh)
{
  DMGeneratorFunctionList fl;
  char                    genname[PETSC_MAX_PATH_LEN];
  const char             *suggestions;
  PetscInt                dim;
  PetscBool               flg;
  PetscErrorCode          ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(boundary, DM_CLASSID, 1);
  PetscValidLogicalCollectiveBool(boundary, interpolate, 3);
  ierr = DMGetDimension(boundary, &dim);CHKERRQ(ierr);
  ierr = PetscOptionsGetString(((PetscObject) boundary)->options,((PetscObject) boundary)->prefix, "-dm_generator", genname, sizeof(genname), &flg);CHKERRQ(ierr);
  if (flg) name = genname;
  else {
    ierr = PetscOptionsGetString(((PetscObject) boundary)->options,((PetscObject) boundary)->prefix, "-dm_plex_generate", genname, sizeof(genname), &flg);CHKERRQ(ierr);
    if (flg) name = genname;
  }

  fl = DMGenerateList;
  if (name) {
    while (fl) {
      ierr = PetscStrcmp(fl->name,name,&flg);CHKERRQ(ierr);
      if (flg) {
        ierr = (*fl->generate)(boundary,interpolate,mesh);CHKERRQ(ierr);
        PetscFunctionReturn(0);
      }
      fl = fl->next;
    }
    SETERRQ(PETSC_COMM_SELF,PETSC_ERR_ARG_OUTOFRANGE,"Grid generator %s not registered; you may need to add --download-%s to your ./configure options",name,name);
  } else {
    while (fl) {
      if (boundary->dim == fl->dim) {
        ierr = (*fl->generate)(boundary,interpolate,mesh);CHKERRQ(ierr);
        PetscFunctionReturn(0);
      }
      fl = fl->next;
    }
    suggestions = "";
    if (boundary->dim+1 == 2) suggestions = " You may need to add --download-triangle to your ./configure options";
    else if (boundary->dim+1 == 3) suggestions = " You may need to add --download-ctetgen or --download-tetgen in your ./configure options";
    SETERRQ(PETSC_COMM_SELF,PETSC_ERR_ARG_OUTOFRANGE,"No grid generator of dimension %D registered%s",boundary->dim+1,suggestions);
  }
}
