/*$Id: zviewer.c,v 1.26 2000/06/25 15:39:14 balay Exp bsmith $*/

#include "src/fortran/custom/zpetsc.h"
#include "petsc.h"

#ifdef PETSC_HAVE_FORTRAN_CAPS
#define viewerdestroy_        VIEWERDESTROY
#define viewerasciiopen_      VIEWERASCIIOPEN
#define viewersetformat_      VIEWERSETFORMAT
#define viewerpushformat_     VIEWERPUSHFORMAT
#define viewerpopformat_      VIEWERPOPFORMAT
#define viewerbinaryopen_     VIEWERBINARYOPEN
#define viewersocketopen_     VIEWERSOCKETOPEN
#define viewerstringopen_     VIEWERSTRINGOPEN
#define viewerdrawopen_       VIEWERDRAWOPEN
#define viewerbinarysettype_  VIEWERBINARYSETTYPE
#define viewersetfilename_    VIEWERSETFILENAME
#define viewersocketputscalar_ VIEWERSOCKETPUTSCALAR
#define viewersocketputint_    VIEWERSOCKETPUTINT
#define viewersocketputreal_   VIEWERSOCKETPUTREAL
#elif !defined(PETSC_HAVE_FORTRAN_UNDERSCORE)
#define viewersocketputscalar_ viewersocketputscalar
#define viewersocketputint_    viewersocketputint
#define viewersocketputreal_   viewersocketputreal
#define viewerdestroy_        viewerdestroy
#define viewerasciiopen_      viewerasciiopen
#define viewersetformat_      viewersetformat
#define viewerpushformat_     viewerpushformat
#define viewerpopformat_      viewerpopformat
#define viewerbinaryopen_     viewerbinaryopen
#define viewersocketopen_     viewersocketopen
#define viewerstringopen_     viewerstringopen
#define viewerdrawopen_       viewerdrawopen
#define viewerbinarysettype_  viewerbinarysettype
#define viewersetfilename_    viewersetfilename
#endif

EXTERN_C_BEGIN

void PETSC_STDCALL viewersocketputscalar(Viewer *viewer,int *m,int *n,Scalar *s,int *ierr)
{
  Viewer v;
  PetscPatchDefaultViewers_Fortran(viewer,v);
  *ierr = ViewerSocketPutScalar(v,*m,*n,s);
}

void PETSC_STDCALL viewersocketputreal(Viewer *viewer,int *m,int *n,PetscReal *s,int *ierr)
{
  Viewer v;
  PetscPatchDefaultViewers_Fortran(viewer,v);
  *ierr = ViewerSocketPutReal(v,*m,*n,s);
}

void PETSC_STDCALL viewersocketputint(Viewer *viewer,int *m,int *s,int *ierr)
{
  Viewer v;
  PetscPatchDefaultViewers_Fortran(viewer,v);
  *ierr = ViewerSocketPutInt(v,*m,s);
}

void PETSC_STDCALL viewersetfilename_(Viewer *viewer,CHAR name PETSC_MIXED_LEN(len),
                                      int *ierr PETSC_END_LEN(len))
{
  char   *c1;
  Viewer v;
  PetscPatchDefaultViewers_Fortran(viewer,v);
  FIXCHAR(name,len,c1);
  *ierr = ViewerSetFilename(v,c1);
  FREECHAR(name,c1);
}

void PETSC_STDCALL  viewerbinarysettype_(Viewer *viewer,ViewerBinaryType *type,int *ierr)
{
  Viewer v;
  PetscPatchDefaultViewers_Fortran(viewer,v);
  *ierr = ViewerBinarySetType(v,*type);
}

void PETSC_STDCALL viewersocketopen_(MPI_Comm *comm,CHAR name PETSC_MIXED_LEN(len),int *port,Viewer *lab,int *ierr PETSC_END_LEN(len))
{
  char   *c1;
  FIXCHAR(name,len,c1);
  *ierr = ViewerSocketOpen((MPI_Comm)PetscToPointerComm(*comm),c1,*port,lab);
  FREECHAR(name,c1);
}

void PETSC_STDCALL viewerbinaryopen_(MPI_Comm *comm,CHAR name PETSC_MIXED_LEN(len),ViewerBinaryType *type,
                           Viewer *binv,int *ierr PETSC_END_LEN(len))
{
  char   *c1;
  FIXCHAR(name,len,c1);
  *ierr = ViewerBinaryOpen((MPI_Comm)PetscToPointerComm(*comm),c1,*type,binv);
  FREECHAR(name,c1);
}

void PETSC_STDCALL viewerasciiopen_(MPI_Comm *comm,CHAR name PETSC_MIXED_LEN(len),Viewer *lab,
                                    int *ierr PETSC_END_LEN(len))
{
  char   *c1;
  FIXCHAR(name,len,c1);
  *ierr = ViewerASCIIOpen((MPI_Comm)PetscToPointerComm(*comm),c1,lab);
  FREECHAR(name,c1);
}

void PETSC_STDCALL viewersetformat_(Viewer *vin,int *format,CHAR name PETSC_MIXED_LEN(len),
                                    int *ierr PETSC_END_LEN(len))
{
  Viewer v;
  char   *c1;
  PetscPatchDefaultViewers_Fortran(vin,v);
  FIXCHAR(name,len,c1);
  *ierr = ViewerSetFormat(v,*format,c1);
}

void PETSC_STDCALL viewerpushformat_(Viewer *vin,int *format,CHAR name PETSC_MIXED_LEN(len),
                                     int *ierr PETSC_END_LEN(len))
{
  Viewer v;
  char   *c1;
  PetscPatchDefaultViewers_Fortran(vin,v);
  FIXCHAR(name,len,c1);
  *ierr = ViewerPushFormat(v,*format,c1);
}

void PETSC_STDCALL viewerpopformat_(Viewer *vin,int *ierr)
{
  Viewer v;
  PetscPatchDefaultViewers_Fortran(vin,v);
  *ierr = ViewerPopFormat(v);
}

void PETSC_STDCALL viewerdestroy_(Viewer *v,int *ierr)
{
  *ierr = ViewerDestroy(*v);
}

void PETSC_STDCALL viewerstringopen_(MPI_Comm *comm,CHAR name PETSC_MIXED_LEN(len1),int *len,Viewer *str,
                                     int *ierr PETSC_END_LEN(len1))
{
#if defined(PETSC_USES_CPTOFCD)
  *ierr = ViewerStringOpen((MPI_Comm)PetscToPointerComm(*comm),_fcdtocp(name),*len,str);
#else
  *ierr = ViewerStringOpen((MPI_Comm)PetscToPointerComm(*comm),name,*len,str);
#endif
}
  
void PETSC_STDCALL viewerdrawopen_(MPI_Comm *comm,CHAR display PETSC_MIXED_LEN(len1),
                   CHAR title PETSC_MIXED_LEN(len2),int *x,int*y,int*w,int*h,Viewer *v,
                   int *ierr PETSC_END_LEN(len1) PETSC_END_LEN(len2))
{
  char   *c1,*c2;

  FIXCHAR(display,len1,c1);
  FIXCHAR(title,len2,c2);
  *ierr = ViewerDrawOpen((MPI_Comm)PetscToPointerComm(*comm),c1,c2,*x,*y,*w,*h,v);
  FREECHAR(display,c1);
  FREECHAR(title,c2);
}

EXTERN_C_END


