#ifdef PETSC_RCS_HEADER
static char vcid[] = "$Id: dline.c,v 1.20 1999/10/01 21:20:18 bsmith Exp bsmith $";
#endif
/*
       Provides the calling sequences for all the basic Draw routines.
*/
#include "src/sys/src/draw/drawimpl.h"  /*I "draw.h" I*/
  
#undef __FUNC__  
#define __FUNC__ "DrawLine" 
/*@
   DrawLine - Draws a line onto a drawable.

   Not collective

   Input Parameters:
+  draw - the drawing context
.  xl,yl,xr,yr - the coordinates of the line endpoints
-  cl - the colors of the endpoints

   Level: beginner

.keywords:  draw, line
@*/
int DrawLine(Draw draw,double xl,double yl,double xr,double yr,int cl)
{
  int ierr,isdrawnull;
  PetscFunctionBegin;
  PetscValidHeaderSpecific(draw,DRAW_COOKIE);
  isdrawnull = PetscTypeCompare(draw,DRAW_NULL);
  if (isdrawnull) PetscFunctionReturn(0);
  ierr = (*draw->ops->line)(draw,xl,yl,xr,yr,cl);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ "DrawIsNull" 
/*@
   DrawIsNull - Returns PETSC_TRUE if draw is a null draw object.

   Not collective

   Input Parameter:
.  draw - the draw context

   Output Parameter:
.  yes - PETSC_TRUE if it is a null draw object; otherwise PETSC_FALSE

   Level: advanced

@*/
int DrawIsNull(Draw draw,PetscTruth *yes)
{
  int isdrawnull;
  PetscFunctionBegin;
  isdrawnull = PetscTypeCompare(draw,DRAW_NULL);
  if (isdrawnull) *yes = PETSC_TRUE;
  else            *yes = PETSC_FALSE;
  PetscFunctionReturn(0);
}
