/*$Id: matioall.c,v 1.19 2000/09/01 20:35:59 balay Exp bsmith $*/

#include "petscmat.h"

EXTERN_C_BEGIN
EXTERN int MatLoad_MPIRowbs(Viewer,MatType,Mat*);
EXTERN int MatLoad_SeqAIJ(Viewer,MatType,Mat*);
EXTERN int MatLoad_MPIAIJ(Viewer,MatType,Mat*);
EXTERN int MatLoad_SeqBDiag(Viewer,MatType,Mat*);
EXTERN int MatLoad_MPIBDiag(Viewer,MatType,Mat*);
EXTERN int MatLoad_SeqDense(Viewer,MatType,Mat*);
EXTERN int MatLoad_MPIDense(Viewer,MatType,Mat*);
EXTERN int MatLoad_SeqBAIJ(Viewer,MatType,Mat*);
EXTERN int MatLoad_SeqAdj(Viewer,MatType,Mat*);
EXTERN int MatLoad_MPIBAIJ(Viewer,MatType,Mat*);
EXTERN int MatLoad_SeqSBAIJ(Viewer,MatType,Mat*);
EXTERN int MatLoad_MPISBAIJ(Viewer,MatType,Mat*);
EXTERN int MatLoad_MPIRowbs(Viewer,MatType,Mat*);
EXTERN_C_END
extern PetscTruth MatLoadRegisterAllCalled;

#undef __FUNC__  
#define __FUNC__ /*<a name=""></a>*/"MatLoadRegisterAll"
/*@C
    MatLoadRegisterAll - Registers all standard matrix type routines to load
        matrices from a binary file.

  Not Collective

  Level: developer

  Notes: To prevent registering all matrix types; copy this routine to 
         your source code and comment out the versions below that you do not need.

.seealso: MatLoadRegister(), MatLoad()

@*/
int MatLoadRegisterAll(char *path)
{
  int ierr;

  PetscFunctionBegin;
  MatLoadRegisterAllCalled = PETSC_TRUE;
  ierr = MatLoadRegisterDynamic(MATSEQAIJ,path,"MatLoad_SeqAIJ",MatLoad_SeqAIJ);CHKERRQ(ierr);
  ierr = MatLoadRegisterDynamic(MATMPIAIJ,path,"MatLoad_MPIAIJ",MatLoad_MPIAIJ);CHKERRQ(ierr);
  ierr = MatLoadRegisterDynamic(MATSEQBDIAG,path,"MatLoad_SeqBDiag",MatLoad_SeqBDiag);CHKERRQ(ierr);
  ierr = MatLoadRegisterDynamic(MATMPIBDIAG,path,"MatLoad_MPIBDiag",MatLoad_MPIBDiag);CHKERRQ(ierr);
  ierr = MatLoadRegisterDynamic(MATSEQDENSE,path,"MatLoad_SeqDense",MatLoad_SeqDense);CHKERRQ(ierr);
  ierr = MatLoadRegisterDynamic(MATMPIDENSE,path,"MatLoad_MPIDense",MatLoad_MPIDense);CHKERRQ(ierr);
  ierr = MatLoadRegisterDynamic(MATSEQBAIJ,path,"MatLoad_SeqBAIJ",MatLoad_SeqBAIJ);CHKERRQ(ierr);
  ierr = MatLoadRegisterDynamic(MATMPIBAIJ,path,"MatLoad_MPIBAIJ",MatLoad_MPIBAIJ);CHKERRQ(ierr);
  ierr = MatLoadRegisterDynamic(MATSEQSBAIJ,path,"MatLoad_SeqSBAIJ",MatLoad_SeqSBAIJ);CHKERRQ(ierr);
  ierr = MatLoadRegisterDynamic(MATMPISBAIJ,path,"MatLoad_MPISBAIJ",MatLoad_MPISBAIJ);CHKERRQ(ierr);
#if defined(PETSC_HAVE_BLOCKSOLVE)
  ierr = MatLoadRegisterDynamic(MATMPIROWBS,path,"MatLoad_MPIRowbs",MatLoad_MPIRowbs);CHKERRQ(ierr);
#endif
  PetscFunctionReturn(0);
}  

EXTERN_C_BEGIN
EXTERN int MatConvertTo_MPIAdj(Mat,MatType,Mat*);
EXTERN_C_END

#undef __FUNC__  
#define __FUNC__ /*<a name=""></a>*/"MatConvertRegisterAll"
/*@C
    MatConvertRegisterAll - Registers all standard matrix type routines to convert to

  Not Collective

  Level: developer

  Notes: To prevent registering all matrix types; copy this routine to 
         your source code and comment out the versions below that you do not need.

.seealso: MatLoadRegister(), MatLoad()

@*/
int MatConvertRegisterAll(char *path)
{
  int ierr;

  PetscFunctionBegin;
  MatConvertRegisterAllCalled = PETSC_TRUE;
  ierr = MatConvertRegisterDynamic(MATMPIADJ,path,"MatConvertTo_MPIAdj",MatConvertTo_MPIAdj);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}  
