/*$Id: ex71.c,v 1.38 2000/05/05 22:16:17 balay Exp bsmith $*/

static char help[] = "Passes a sparse matrix to Matlab.\n\n";

#include "petscmat.h"

#undef __FUNC__
#define __FUNC__ "main"
int main(int argc,char **args)
{
  int     ierr,m = 4,n = 5,i,j,I,J;
  Scalar  one = 1.0,v;
  Vec     x;
  Mat     A;
  Viewer  viewer;

  PetscInitialize(&argc,&args,(char *)0,help);
  ierr = OptionsGetInt(PETSC_NULL,"-m",&m,PETSC_NULL);CHKERRA(ierr);
  ierr = OptionsGetInt(PETSC_NULL,"-n",&n,PETSC_NULL);CHKERRA(ierr);

  ierr = ViewerSocketOpen(PETSC_COMM_WORLD,"eagle",-1,&viewer);CHKERRA(ierr);
  ierr = MatCreate(PETSC_COMM_WORLD,PETSC_DECIDE,PETSC_DECIDE,m*n,m*n,&A);CHKERRA(ierr);
  ierr = MatSetFromOptions(A);CHKERRA(ierr);

  for (i=0; i<m; i++) {
    for (j=0; j<n; j++) {
      v = -1.0;  I = j + n*i;
      if (i>0)   {J = I - n; ierr = MatSetValues(A,1,&I,1,&J,&v,INSERT_VALUES);CHKERRA(ierr);}
      if (i<m-1) {J = I + n; ierr = MatSetValues(A,1,&I,1,&J,&v,INSERT_VALUES);CHKERRA(ierr);}
      if (j>0)   {J = I - 1; ierr = MatSetValues(A,1,&I,1,&J,&v,INSERT_VALUES);CHKERRA(ierr);}
      if (j<n-1) {J = I + 1; ierr = MatSetValues(A,1,&I,1,&J,&v,INSERT_VALUES);CHKERRA(ierr);}
      v = 4.0; ierr = MatSetValues(A,1,&I,1,&I,&v,INSERT_VALUES);CHKERRA(ierr);
    }
  }
  ierr = MatAssemblyBegin(A,MAT_FINAL_ASSEMBLY);CHKERRA(ierr);
  ierr = MatAssemblyEnd(A,MAT_FINAL_ASSEMBLY);CHKERRA(ierr);

  ierr = MatView(A,viewer);CHKERRA(ierr);

  ierr = VecCreateSeq(PETSC_COMM_SELF,m,&x);CHKERRA(ierr);
  ierr = VecSet(&one,x);CHKERRA(ierr);
  ierr = VecView(x,viewer);CHKERRA(ierr);
  
  ierr = PetscSleep(30);CHKERRA(ierr);
  ierr = PetscObjectDestroy((PetscObject)viewer);CHKERRA(ierr);

  ierr = VecDestroy(x);CHKERRA(ierr);
  ierr = MatDestroy(A);CHKERRA(ierr);
  PetscFinalize();
  return 0;
}
    


