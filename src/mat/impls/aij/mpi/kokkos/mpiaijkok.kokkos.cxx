#include <petscvec_kokkos.hpp>
#include <petscsf.h>
#include <petsc/private/sfimpl.h>
#include <../src/mat/impls/aij/mpi/mpiaij.h>
#include <../src/mat/impls/aij/mpi/kokkos/mpiaijkok.hpp>
#include <KokkosSparse_spadd.hpp>

PetscErrorCode MatAssemblyEnd_MPIAIJKokkos(Mat A,MatAssemblyType mode)
{
  PetscErrorCode   ierr;
  Mat_MPIAIJ       *mpiaij = (Mat_MPIAIJ*)A->data;
  Mat_SeqAIJKokkos *aijkok = mpiaij->A->spptr ? static_cast<Mat_SeqAIJKokkos*>(mpiaij->A->spptr) : NULL;

  PetscFunctionBegin;
  ierr = MatAssemblyEnd_MPIAIJ(A,mode);CHKERRQ(ierr);
  if (aijkok && aijkok->device_mat_d.data()) {
    A->offloadmask = PETSC_OFFLOAD_GPU; // in GPU mode, no going back. MatSetValues checks this
  }

  PetscFunctionReturn(0);
}

PetscErrorCode MatMPIAIJSetPreallocation_MPIAIJKokkos(Mat mat,PetscInt d_nz,const PetscInt d_nnz[],PetscInt o_nz,const PetscInt o_nnz[])
{
  PetscErrorCode ierr;
  Mat_MPIAIJ     *mpiaij = (Mat_MPIAIJ*)mat->data;

  PetscFunctionBegin;
  ierr = PetscLayoutSetUp(mat->rmap);CHKERRQ(ierr);
  ierr = PetscLayoutSetUp(mat->cmap);CHKERRQ(ierr);
#if defined(PETSC_USE_DEBUG)
  if (d_nnz) {
    PetscInt i;
    for (i=0; i<mat->rmap->n; i++) {
      PetscCheckFalse(d_nnz[i] < 0,PETSC_COMM_SELF,PETSC_ERR_ARG_OUTOFRANGE,"d_nnz cannot be less than 0: local row %" PetscInt_FMT " value %" PetscInt_FMT,i,d_nnz[i]);
    }
  }
  if (o_nnz) {
    PetscInt i;
    for (i=0; i<mat->rmap->n; i++) {
      PetscCheckFalse(o_nnz[i] < 0,PETSC_COMM_SELF,PETSC_ERR_ARG_OUTOFRANGE,"o_nnz cannot be less than 0: local row %" PetscInt_FMT " value %" PetscInt_FMT,i,o_nnz[i]);
    }
  }
#endif
#if defined(PETSC_USE_CTABLE)
  ierr = PetscTableDestroy(&mpiaij->colmap);CHKERRQ(ierr);
#else
  ierr = PetscFree(mpiaij->colmap);CHKERRQ(ierr);
#endif
  ierr = PetscFree(mpiaij->garray);CHKERRQ(ierr);
  ierr = VecDestroy(&mpiaij->lvec);CHKERRQ(ierr);
  ierr = VecScatterDestroy(&mpiaij->Mvctx);CHKERRQ(ierr);
  /* Because the B will have been resized we simply destroy it and create a new one each time */
  ierr = MatDestroy(&mpiaij->B);CHKERRQ(ierr);

  if (!mpiaij->A) {
    ierr = MatCreate(PETSC_COMM_SELF,&mpiaij->A);CHKERRQ(ierr);
    ierr = MatSetSizes(mpiaij->A,mat->rmap->n,mat->cmap->n,mat->rmap->n,mat->cmap->n);CHKERRQ(ierr);
    ierr = PetscLogObjectParent((PetscObject)mat,(PetscObject)mpiaij->A);CHKERRQ(ierr);
  }
  if (!mpiaij->B) {
    PetscMPIInt size;
    ierr = MPI_Comm_size(PetscObjectComm((PetscObject)mat),&size);CHKERRMPI(ierr);
    ierr = MatCreate(PETSC_COMM_SELF,&mpiaij->B);CHKERRQ(ierr);
    ierr = MatSetSizes(mpiaij->B,mat->rmap->n,size > 1 ? mat->cmap->N : 0,mat->rmap->n,size > 1 ? mat->cmap->N : 0);CHKERRQ(ierr);
    ierr = PetscLogObjectParent((PetscObject)mat,(PetscObject)mpiaij->B);CHKERRQ(ierr);
  }
  ierr = MatSetType(mpiaij->A,MATSEQAIJKOKKOS);CHKERRQ(ierr);
  ierr = MatSetType(mpiaij->B,MATSEQAIJKOKKOS);CHKERRQ(ierr);
  ierr = MatSeqAIJSetPreallocation(mpiaij->A,d_nz,d_nnz);CHKERRQ(ierr);
  ierr = MatSeqAIJSetPreallocation(mpiaij->B,o_nz,o_nnz);CHKERRQ(ierr);
  mat->preallocated = PETSC_TRUE;
  PetscFunctionReturn(0);
}

PetscErrorCode MatMult_MPIAIJKokkos(Mat mat,Vec xx,Vec yy)
{
  Mat_MPIAIJ     *mpiaij = (Mat_MPIAIJ*)mat->data;
  PetscErrorCode ierr;
  PetscInt       nt;

  PetscFunctionBegin;
  ierr = VecGetLocalSize(xx,&nt);CHKERRQ(ierr);
  PetscCheckFalse(nt != mat->cmap->n,PETSC_COMM_SELF,PETSC_ERR_ARG_SIZ,"Incompatible partition of mat (%" PetscInt_FMT ") and xx (%" PetscInt_FMT ")",mat->cmap->n,nt);
  ierr = VecScatterBegin(mpiaij->Mvctx,xx,mpiaij->lvec,INSERT_VALUES,SCATTER_FORWARD);CHKERRQ(ierr);
  ierr = (*mpiaij->A->ops->mult)(mpiaij->A,xx,yy);CHKERRQ(ierr);
  ierr = VecScatterEnd(mpiaij->Mvctx,xx,mpiaij->lvec,INSERT_VALUES,SCATTER_FORWARD);CHKERRQ(ierr);
  ierr = (*mpiaij->B->ops->multadd)(mpiaij->B,mpiaij->lvec,yy,yy);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

PetscErrorCode MatMultAdd_MPIAIJKokkos(Mat mat,Vec xx,Vec yy,Vec zz)
{
  Mat_MPIAIJ     *mpiaij = (Mat_MPIAIJ*)mat->data;
  PetscErrorCode ierr;
  PetscInt       nt;

  PetscFunctionBegin;
  ierr = VecGetLocalSize(xx,&nt);CHKERRQ(ierr);
  PetscCheckFalse(nt != mat->cmap->n,PETSC_COMM_SELF,PETSC_ERR_ARG_SIZ,"Incompatible partition of mat (%" PetscInt_FMT ") and xx (%" PetscInt_FMT ")",mat->cmap->n,nt);
  ierr = VecScatterBegin(mpiaij->Mvctx,xx,mpiaij->lvec,INSERT_VALUES,SCATTER_FORWARD);CHKERRQ(ierr);
  ierr = (*mpiaij->A->ops->multadd)(mpiaij->A,xx,yy,zz);CHKERRQ(ierr);
  ierr = VecScatterEnd(mpiaij->Mvctx,xx,mpiaij->lvec,INSERT_VALUES,SCATTER_FORWARD);CHKERRQ(ierr);
  ierr = (*mpiaij->B->ops->multadd)(mpiaij->B,mpiaij->lvec,zz,zz);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

PetscErrorCode MatMultTranspose_MPIAIJKokkos(Mat mat,Vec xx,Vec yy)
{
  Mat_MPIAIJ     *mpiaij = (Mat_MPIAIJ*)mat->data;
  PetscErrorCode ierr;
  PetscInt       nt;

  PetscFunctionBegin;
  ierr = VecGetLocalSize(xx,&nt);CHKERRQ(ierr);
  PetscCheckFalse(nt != mat->rmap->n,PETSC_COMM_SELF,PETSC_ERR_ARG_SIZ,"Incompatible partition of mat (%" PetscInt_FMT ") and xx (%" PetscInt_FMT ")",mat->rmap->n,nt);
  ierr = (*mpiaij->B->ops->multtranspose)(mpiaij->B,xx,mpiaij->lvec);CHKERRQ(ierr);
  ierr = (*mpiaij->A->ops->multtranspose)(mpiaij->A,xx,yy);CHKERRQ(ierr);
  ierr = VecScatterBegin(mpiaij->Mvctx,mpiaij->lvec,yy,ADD_VALUES,SCATTER_REVERSE);CHKERRQ(ierr);
  ierr = VecScatterEnd(mpiaij->Mvctx,mpiaij->lvec,yy,ADD_VALUES,SCATTER_REVERSE);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/* Merge the "A, B" matrices of mat into a matrix C.  mat's type is MPIAIJKOKKOS. C's type is MATSEQAIJKOKKOS.
   A is put before B. C's size would be A->rmap->n by (A->cmap->n + B->cmap->n).
   C still uses local column ids. Their corresponding global column ids are returned in glob.
*/
PetscErrorCode MatMPIAIJGetLocalMatMerge_MPIAIJKokkos(Mat mat,MatReuse reuse,IS *glob,Mat *C)
{
  Mat            Ad,Ao;
  const PetscInt *cmap;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = MatMPIAIJGetSeqAIJ(mat,&Ad,&Ao,&cmap);CHKERRQ(ierr);
  ierr = MatSeqAIJKokkosMergeMats(Ad,Ao,reuse,C);CHKERRQ(ierr);
  if (glob) {
    PetscInt cst, i, dn, on, *gidx;
    ierr = MatGetLocalSize(Ad,NULL,&dn);CHKERRQ(ierr);
    ierr = MatGetLocalSize(Ao,NULL,&on);CHKERRQ(ierr);
    ierr = MatGetOwnershipRangeColumn(mat,&cst,NULL);CHKERRQ(ierr);
    ierr = PetscMalloc1(dn+on,&gidx);CHKERRQ(ierr);
    for (i=0; i<dn; i++) gidx[i]    = cst + i;
    for (i=0; i<on; i++) gidx[i+dn] = cmap[i];
    ierr = ISCreateGeneral(PetscObjectComm((PetscObject)Ad),dn+on,gidx,PETSC_OWN_POINTER,glob);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

/* Structs used in matrix product C=AB, C=A^tB and C=B^tAB */
struct MatMatStruct {
  MatRowMapKokkosView   Cdstart; /* Used to split sequential matrix into petsc's A, B format */
  PetscSF               sf; /* SF to send/recv matrix entries */
  MatScalarKokkosView   abuf; /* buf of mat values in send/recv */
  Mat                   C1,C2,B_local;
  KokkosCsrMatrix       C1_global,C2_global,C_global;
  KernelHandle          kh;
  MatMatStruct() {
    C1 = C2 = B_local = NULL;
    sf = NULL;
  }

  ~MatMatStruct() {
    MatDestroy(&C1);
    MatDestroy(&C2);
    MatDestroy(&B_local);
    PetscSFDestroy(&sf);
    kh.destroy_spadd_handle();
  }
};

struct MatMatStruct_AB : public MatMatStruct {
  MatColIdxKokkosView   rows;
  MatRowMapKokkosView   rowoffset;
  Mat                   B_other,C_petsc; /* SEQAIJKOKKOS matrices. TODO: have a better var name than C_petsc */

  MatMatStruct_AB() : B_other(NULL),C_petsc(NULL){}
  ~MatMatStruct_AB() {
    MatDestroy(&B_other);
    MatDestroy(&C_petsc);
  }
};

struct MatMatStruct_AtB : public MatMatStruct {
  MatRowMapKokkosView   srcrowoffset,dstrowoffset;
};

struct MatProductData_MPIAIJKokkos
{
  MatMatStruct_AB   *mmAB;
  MatMatStruct_AtB  *mmAtB;
  PetscBool         reusesym;

  MatProductData_MPIAIJKokkos(): mmAB(NULL),mmAtB(NULL),reusesym(PETSC_FALSE){}
  ~MatProductData_MPIAIJKokkos() {
    delete mmAB;
    delete mmAtB;
  }
};

static PetscErrorCode MatProductDataDestroy_MPIAIJKokkos(void *data)
{
  PetscFunctionBegin;
  CHKERRCXX(delete static_cast<MatProductData_MPIAIJKokkos*>(data));
  PetscFunctionReturn(0);
}

/* MatSeqAIJKokkosGetCSRMatrixWithGlobalColumnIds - Get a KokkosCsrMatrix from a MATSEQAIJKOKKOS matrix

   Input Parameters:
+  A       - the MATSEQAIJKOKKOS matrix
.  N       - new column size for the returned Kokkos matrix
-  l2g     - a map that maps old col ids to new col ids

   Output Parameters:
.  csrmat  - the Kokkos matrix, which has the same row size as A, shares a, i but not j with A.
 */
static PetscErrorCode MatSeqAIJKokkosGetCSRMatrixWithGlobalColumnIds(Mat A,PetscInt N,const ConstMatColIdxKokkosView& l2g,KokkosCsrMatrix& csrmat)
{
  KokkosCsrMatrix&         orig = static_cast<Mat_SeqAIJKokkos*>(A->spptr)->csrmat;
  MatColIdxKokkosView      jg("jg",orig.nnz()); /* New j array for csrmat */

  PetscFunctionBegin;
  CHKERRCXX(Kokkos::parallel_for(orig.nnz(), KOKKOS_LAMBDA(const PetscInt i) {jg(i) = l2g(orig.graph.entries(i));}));
  CHKERRCXX(csrmat = KokkosCsrMatrix("csrmat",orig.numRows(),N,orig.nnz(),orig.values,orig.graph.row_map,jg));
  PetscFunctionReturn(0);
}

/* MatSetMPIAIJKokkosWithSplitSeqAIJKokkosMatrices - Set the diag and offdiag matrices of a MATMPIAIJKOKKOS matrix.
   It is similar to MatCreateMPIAIJWithSplitArrays.

  Input Parameters:
+  mat   - the MATMPIAIJKOKKOS matrix, which should have its type and layout set, but should not have its diag, offdiag matrices set
.  A     - the diag matrix using local col ids
-  B     - the offdiag matrix using global col ids

  Output Parameters:
.  mat   - the updated MATMPIAIJKOKKOS matrix
*/
static PetscErrorCode MatSetMPIAIJKokkosWithSplitSeqAIJKokkosMatrices(Mat mat,Mat A,Mat B)
{
  PetscErrorCode      ierr;
  Mat_MPIAIJ          *mpiaij = static_cast<Mat_MPIAIJ*>(mat->data);
  PetscInt            m,n,M,N,Am,An,Bm,Bn;
  Mat_SeqAIJKokkos    *bkok = static_cast<Mat_SeqAIJKokkos*>(B->spptr);

  PetscFunctionBegin;
  ierr = MatGetSize(mat,&M,&N);CHKERRQ(ierr);
  ierr = MatGetLocalSize(mat,&m,&n);CHKERRQ(ierr);
  ierr = MatGetLocalSize(A,&Am,&An);CHKERRQ(ierr);
  ierr = MatGetLocalSize(B,&Bm,&Bn);CHKERRQ(ierr);

  PetscCheckFalse(m != Am || m != Bm,PETSC_COMM_SELF,PETSC_ERR_PLIB,"local number of rows do not match");
  PetscCheckFalse(n != An,PETSC_COMM_SELF,PETSC_ERR_PLIB,"local number of columns do not match");
  PetscCheckFalse(N != Bn,PETSC_COMM_SELF,PETSC_ERR_PLIB,"global number of columns do not match");
  PetscCheckFalse(mpiaij->A || mpiaij->B,PETSC_COMM_SELF,PETSC_ERR_PLIB,"A, B of the MPIAIJ matrix are not empty");
  mpiaij->A = A;
  mpiaij->B = B;

  mat->preallocated     = PETSC_TRUE;
  mat->nooffprocentries = PETSC_TRUE; /* See MatAssemblyBegin_MPIAIJ. In effect, making MatAssemblyBegin a nop */

  ierr = MatSetOption(mat,MAT_NO_OFF_PROC_ENTRIES,PETSC_TRUE);CHKERRQ(ierr);
  ierr = MatAssemblyBegin(mat,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  /* MatAssemblyEnd is critical here. It sets mat->offloadmask according to A and B's, and
    also gets mpiaij->B compacted, with its col ids and size reduced
  */
  ierr = MatAssemblyEnd(mat,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  ierr = MatSetOption(mat,MAT_NO_OFF_PROC_ENTRIES,PETSC_FALSE);CHKERRQ(ierr);
  ierr = MatSetOption(mat,MAT_NEW_NONZERO_LOCATION_ERR,PETSC_TRUE);CHKERRQ(ierr);

  /* Update bkok with new local col ids (stored on host) and size */
  bkok->j_dual.modify_host();
  bkok->j_dual.sync_device();
  bkok->SetColSize(mpiaij->B->cmap->n);
  PetscFunctionReturn(0);
}

/* MatSeqAIJKokkosBcast - Bcast rows of a SEQAIJKOKKOS matrice (B) to form a SEQAIJKOKKOS matrix (C).

   It is essentially the MPIAIJKOKKOS counterpart of MatGetBrowsOfAoCols_MPIAIJ, but supports device and uses PetscSF.
   In the given ownerSF, leaves correspond to rows in C, and roots correspond to rows in B. Roots may connect to multiple leaves.
   Suppose C's j-th row is connected to a root identified by PetscSFNode (k,i), it means we will bcast the i-th row of B on rank k
   to j-th row of C. ownerSF's leaves must be contiguous (in other words, as if ilocal=NULL was used to set its graph).

   Collective on comm of ownerSF

   Input Parameters:
+   B       - the SEQAIJKOKKOS matrix, using local col ids
.   reuse   - either MAT_INITIAL_MATRIX or MAT_REUSE_MATRIX
.   N       - global col ids are in range of [0,N). N Must be the same across ranks (nonsignificant in MAT_REUSE_MATRIX)
.   l2g     - a map mapping B's local col ids to global ones (nonsignificant in MAT_REUSE_MATRIX)
.   ownerSF - the ownership SF (nonsignificant in MAT_REUSE_MATRIX)

   Input/Output Parameters (out when resue = MAT_INITIAL_MATRIX, inout when reuse = MAT_REUSE_MATRIX)
+   bcastSF   - the SF used to bcast rows of B. This plain SF does buffer (abuf) to buffer (Ca) send/recv. In this SF, vertices are nonzeros.
.   abuf      - buffer for sending matrix values
.   rows      - array containing indices of (local) rows that this rank needs to bcast to others. Each receiver rank has a chunk in rows[].
                Values in rows[] might have repeats, which simply indicates a row will be bcast'ed to multiple neighbors.
.   rowoffset - For each row in rows[], it will be copied to rowoffset[] at abuf[]
-   C         -  the SEQAIJKOKKOS matrix made of the bcast'ed rows, using local col ids.
*/
static PetscErrorCode MatSeqAIJKokkosBcast(Mat B,MatReuse reuse,PetscInt N,const ConstMatColIdxKokkosView& l2g,PetscSF ownerSF,
                                           PetscSF& bcastSF,MatScalarKokkosView& abuf,MatColIdxKokkosView& rows,
                                           MatRowMapKokkosView& rowoffset,Mat& C)
{
  PetscErrorCode               ierr;
  Mat_SeqAIJKokkos             *bkok,*ckok;

  PetscFunctionBegin;
  ierr = MatSeqAIJKokkosSyncDevice(B);CHKERRQ(ierr); /* Make sure B->spptr is accessible */
  bkok = static_cast<Mat_SeqAIJKokkos*>(B->spptr);

  if (reuse == MAT_REUSE_MATRIX) {
    ckok = static_cast<Mat_SeqAIJKokkos*>(C->spptr);

    const auto& Ba = bkok->a_dual.view_device();
    const auto& Bi = bkok->i_dual.view_device();
    const auto& Ca = ckok->a_dual.view_device();

    /* Copy Ba to abuf */
    Kokkos::parallel_for(Kokkos::TeamPolicy<>(rows.extent(0), Kokkos::AUTO()),KOKKOS_LAMBDA(const KokkosTeamMemberType& t) {
      PetscInt i    = t.league_rank(); /* rows[i] is r-th row of B */
      PetscInt r    = rows(i);
      PetscInt base = rowoffset(i); /* Copy r-th row of B to this offset in abuf[] */
      Kokkos::parallel_for(Kokkos::TeamThreadRange(t,Bi(r+1)-Bi(r)),[&](PetscInt k) {
        abuf(base+k) = Ba(Bi(r)+k);
      });
    });

    /* Send abuf to Ca through bcastSF and then mark C is updated on device */
    ierr = PetscSFBcastBegin(bcastSF,MPIU_SCALAR,abuf.data(),Ca.data(),MPI_REPLACE);CHKERRQ(ierr); /* TODO: get memtype for abuf */
    ierr = PetscSFBcastEnd  (bcastSF,MPIU_SCALAR,abuf.data(),Ca.data(),MPI_REPLACE);CHKERRQ(ierr);
    ckok->a_dual.modify_device();
  } else if (reuse == MAT_INITIAL_MATRIX) {
    MPI_Comm       comm;
    PetscMPIInt    tag;
    PetscInt       k,Cm,Cn,Cnnz,*Ci_h,nroots,nleaves;

    ierr = PetscObjectGetComm((PetscObject)ownerSF,&comm);CHKERRMPI(ierr);
    ierr = PetscSFGetGraph(ownerSF,&nroots,&nleaves,NULL,NULL);CHKERRQ(ierr);
    Cm   = nleaves; /* row size of C */
    Cn   = N;  /* col size of C, which initially uses global ids, so we can safely set its col size as N */

    /* Get row lens (nz) of B's rows for later fast query */
    PetscInt       *Browlens;
    const PetscInt *tmp = bkok->i_host_data();
    ierr = PetscMalloc1(nroots,&Browlens);CHKERRQ(ierr);
    for (k=0; k<nroots; k++) Browlens[k] = tmp[k+1]-tmp[k];

    /* By ownerSF, each proc gets lens of rows of C */
    MatRowMapKokkosDualView Ci("i",Cm+1); /* C's rowmap */
    Ci_h    = Ci.view_host().data();
    Ci_h[0] = 0;
    ierr    = PetscSFBcastWithMemTypeBegin(ownerSF,MPIU_INT,PETSC_MEMTYPE_HOST,Browlens,PETSC_MEMTYPE_HOST,&Ci_h[1],MPI_REPLACE);CHKERRQ(ierr);
    ierr    = PetscSFBcastEnd(ownerSF,MPIU_INT,Browlens,&Ci_h[1],MPI_REPLACE);CHKERRQ(ierr);
    for (k=1; k<Cm+1; k++) Ci_h[k] += Ci_h[k-1]; /* Convert lens to CSR */
    Cnnz    = Ci_h[Cm];
    Ci.modify_host();
    Ci.sync_device();

    /* With the newly known Cnnz, we are able to allocate (j, a) for C on host & device */
    MatColIdxKokkosDualView  Cj("j",Cnnz);
    MatScalarKokkosDualView  Ca("a",Cnnz);

    /* Now build the bcastSF to fill Ca, Cj. This plain SF only does (contiguous) buffer to buffer send/recv */
    const PetscMPIInt *iranks,*ranks;
    const PetscInt    *ioffset,*irootloc,*roffset;
    PetscInt          i,j,niranks,nranks,*sdisp,*rdisp,*rowptr;
    MPI_Request       *reqs;

    ierr = PetscSFGetLeafRanks(ownerSF,&niranks,&iranks,&ioffset,&irootloc);CHKERRQ(ierr); /* irootloc[] contains indices of rows I need to send to each receiver */
    ierr = PetscSFGetRootRanks(ownerSF,&nranks,&ranks,&roffset,NULL/*rmine*/,NULL/*rremote*/);CHKERRQ(ierr); /* recv info */

    /* figure out offsets at the send buffer, to build the SF
      sdisp[]  - stores offsets of nonzeros (in abuf or jbuf, see later) I need to send, per receiver.
      rowptr[] - stores offsets for data of each row in abuf

      rdisp[]  - to receive sdisp[]
    */
    ierr = PetscMalloc3(niranks+1,&sdisp,nranks,&rdisp,niranks+nranks,&reqs);CHKERRQ(ierr);
    MatRowMapKokkosViewHost rowptr_h("rowptr_h",ioffset[niranks]+1); /* Let Kokkos do the allocation, so that we can do an easy mirror later */
    rowptr = rowptr_h.data();

    sdisp[0] = 0;
    rowptr[0]  = 0;
    for (i=0; i<niranks; i++) { /* for each receiver */
      PetscInt len, nz = 0;
      for (j=ioffset[i]; j<ioffset[i+1]; j++) { /* for each row to this receiver */
        len         = Browlens[irootloc[j]];
        rowptr[j+1] = rowptr[j] + len;
        nz         += len;
      }
      sdisp[i+1] = sdisp[i] + nz;
    }
    ierr = PetscCommGetNewTag(comm,&tag);CHKERRMPI(ierr);
    for (i=0; i<nranks; i++)  {ierr = MPI_Irecv(&rdisp[i],1,MPIU_INT,ranks[i],tag,comm,&reqs[i]);CHKERRMPI(ierr);}
    for (i=0; i<niranks; i++) {ierr = MPI_Isend(&sdisp[i],1,MPIU_INT,iranks[i],tag,comm,&reqs[nranks+i]);CHKERRMPI(ierr);}
    ierr = MPI_Waitall(niranks+nranks,reqs,MPI_STATUSES_IGNORE);CHKERRMPI(ierr);

    PetscInt    nleaves2 = Cnnz; /* leaves are the nonzeros I will receive */
    PetscInt    nroots2  = sdisp[niranks]; /* roots are the nonzeros (in abuf) I will send */
    PetscSFNode *iremote;
    ierr = PetscMalloc1(nleaves2,&iremote);CHKERRQ(ierr);
    for (i=0; i<nranks; i++) { /* for each sender */
      k = 0;
      for (j=Ci_h[roffset[i]]; j<Ci_h[roffset[i+1]]; j++) {
        iremote[j].rank  = ranks[i];
        iremote[j].index = rdisp[i] + k;
        k++;
      }
    }
    /* TODO: we should extend PetscSF APIs for this buffer-to-buffer send/recv */
    ierr = PetscSFCreate(comm,&bcastSF);CHKERRQ(ierr);
    ierr = PetscSFSetGraph(bcastSF,nroots2,nleaves2,NULL/*ilocal*/,PETSC_OWN_POINTER,iremote,PETSC_OWN_POINTER);CHKERRQ(ierr);

    /* Extract selected rows of B, and copy their (a, j) into abuf[] and jbuf[], with j converted
      from local to global. Then use bcastSF to fill Ca, Cj.
    */
    ConstMatColIdxKokkosViewHost rows_h(irootloc,ioffset[niranks]); /* irootloc[] stores indices of rows I need to send */
    MatColIdxKokkosView          rows("rows",ioffset[niranks]);
    Kokkos::deep_copy(rows,rows_h); /* Use deep copy since irootoc is managed by PetscSF and we want 'rows' to be standalone */

    rowoffset = Kokkos::create_mirror_view_and_copy(DefaultMemorySpace(),rowptr_h); /* If no device, rowoffset will be an alias to rowptr_h */

    MatColIdxKokkosView jbuf("jbuf",sdisp[niranks]); /* send buf for (global) col ids */
    abuf = MatScalarKokkosView("abuf",sdisp[niranks]); /* send buf for mat values */

    const auto& Ba = bkok->a_dual.view_device();
    const auto& Bi = bkok->i_dual.view_device();
    const auto& Bj = bkok->j_dual.view_device();

    /* Copy Ba, Bj to abuf, jbuf with change col ids from local to global */
    Kokkos::parallel_for(Kokkos::TeamPolicy<>(rows.extent(0), Kokkos::AUTO()),KOKKOS_LAMBDA(const KokkosTeamMemberType& t) {
      PetscInt i    = t.league_rank(); /* rows[i] is r-th row of B */
      PetscInt r    = rows(i);
      PetscInt base = rowoffset(i); /* Copy r-th row of B to this offset in abuf[] */
      Kokkos::parallel_for(Kokkos::TeamThreadRange(t,Bi(r+1)-Bi(r)),[&](PetscInt k) {
        abuf(base+k) = Ba(Bi(r)+k);
        jbuf(base+k) = l2g(Bj(Bi(r)+k));
      });
    });

    /* Send abuf & jbuf to fill Ca, Cj */
    ierr = PetscSFBcastBegin(bcastSF,MPIU_INT,   jbuf.data(),Cj.view_device().data(),MPI_REPLACE);CHKERRQ(ierr);
    ierr = PetscSFBcastBegin(bcastSF,MPIU_SCALAR,abuf.data(),Ca.view_device().data(),MPI_REPLACE);CHKERRQ(ierr);
    ierr = PetscSFBcastEnd  (bcastSF,MPIU_INT,   jbuf.data(),Cj.view_device().data(),MPI_REPLACE);CHKERRQ(ierr);
    ierr = PetscSFBcastEnd  (bcastSF,MPIU_SCALAR,abuf.data(),Ca.view_device().data(),MPI_REPLACE);CHKERRQ(ierr);
    Cj.modify_device(); /* Mark Cj, Ca modified on device, but only sync Cj since we might not need Ca on host at all */
    Cj.sync_host();
    Ca.modify_device();

    /* Construct C with Ca, Ci, Cj */
    auto ckok = new Mat_SeqAIJKokkos(Cm,Cn,Cnnz,Ci,Cj,Ca);
    ierr = MatCreateSeqAIJKokkosWithCSRMatrix(PETSC_COMM_SELF,ckok,&C);CHKERRQ(ierr);
    ierr = PetscFree3(sdisp,rdisp,reqs);CHKERRQ(ierr);
    ierr = PetscFree(Browlens);CHKERRQ(ierr);
  } else SETERRQ(PETSC_COMM_SELF,PETSC_ERR_PLIB,"Unsupported MatReuse enum %d",reuse);
  PetscFunctionReturn(0);
}

/* MatSeqAIJKokkosReduce - Reduce rows of a SEQAIJKOKKOS matrix (A) to form a Kokkos Csr matrix (C)

  It is the reverse of MatSeqAIJKokkosBcast in some sense.

  Think each row of A as a leaf, then the given ownerSF specifies roots of the leaves. Roots may connect to multiple leaves.
  In this routine, we reduce (i.e., concatenate) leaves (rows) at their roots to form potentially longer rows in C. Such rows might
  contain repeats, which does not matter since they will be summed up by other routines. C's row size will be nroots of ownerSF.

  Input Parameters:
+  A        - the SEQAIJKOKKOS matrix to be reduced
.  reuse    - either MAT_INITIAL_MATRIX or MAT_REUSE_MATRIX
.  local    - true if A uses local col ids; false if A is already in global col ids.
.  N        - if local, N is A's global col size
.  l2g      - if local, a map mapping A's local col ids to global ones, which are in range of [0,N).
-  ownerSF  - the SF specifies ownership (root) of rows in A

  Output Parameters:
+  reduceSF    - the SF to reduce A's rows to contiguous buffers at the receiver side
.  abuf         - a contiguous buffer to receive A's rows sent to this proc. Suppose there are 'nrows' such rows.
.  srcrowoffset - offset array of size nrows+1. Each entry is the corresponding row's offset in abuf[]. srcrowoffset[i+1]-srcrowoffset[i] is row i's len.
.  dstrowoffset - offset array of size nrows. Each entry is the corresponding row's offset in Ca[], i.e., C's 'a' array. Row i, i+1 in abuf[] may go to
                  unrelated places in Ca, so dstrowoffset is not in CSR-like format as srcrowoffset.
-  C            - the matrix made up by rows sent to me from other ranks, using global col ids

   TODO: we can even have MatSeqAIJKokkosReduceBegin/End to provide oppertunity for callers to overlap comp./comm. when reuse = MAT_REUSE_MATRIX.
 */
static PetscErrorCode MatSeqAIJKokkosReduce(Mat A,MatReuse reuse,PetscBool local,PetscInt N,const ConstMatColIdxKokkosView& l2g,PetscSF ownerSF,
                                            PetscSF& reduceSF,MatScalarKokkosView& abuf,
                                            MatRowMapKokkosView& srcrowoffset,MatRowMapKokkosView& dstrowoffset,
                                            KokkosCsrMatrix& C)
{
  PetscErrorCode         ierr;
  PetscInt               i,r,Am,An,Annz,Cnnz,nrows;
  const PetscInt         *Ai;
  Mat_SeqAIJKokkos       *akok;

  PetscFunctionBegin;
  ierr = MatSeqAIJKokkosSyncDevice(A);CHKERRQ(ierr); /* So that A's latest data is on device */
  ierr = MatGetSize(A,&Am,&An);
  Ai   = static_cast<Mat_SeqAIJ*>(A->data)->i;
  akok = static_cast<Mat_SeqAIJKokkos*>(A->spptr);
  Annz = Ai[Am];

  if (reuse == MAT_REUSE_MATRIX) {
    /* Send Aa to abuf */
    ierr = PetscSFReduceBegin(reduceSF,MPIU_SCALAR,akok->a_device_data(),abuf.data(),MPI_REPLACE);CHKERRMPI(ierr);
    ierr = PetscSFReduceEnd  (reduceSF,MPIU_SCALAR,akok->a_device_data(),abuf.data(),MPI_REPLACE);CHKERRMPI(ierr);

    /* Copy abuf to Ca */
    const MatScalarKokkosView& Ca = C.values;
    nrows = dstrowoffset.extent(0); /* Not srcrowoffset[] since it has an extra entry for CSR */
    Kokkos::parallel_for(Kokkos::TeamPolicy<>(nrows, Kokkos::AUTO()),KOKKOS_LAMBDA(const KokkosTeamMemberType& t) {
      PetscInt i   = t.league_rank();
      PetscInt src = srcrowoffset(i), dst = dstrowoffset(i);
      PetscInt len = srcrowoffset(i+1) - srcrowoffset(i);
      Kokkos::parallel_for(Kokkos::TeamThreadRange(t,len), [&](PetscInt k) {Ca(dst+k) = abuf(src+k);});
    });
  } else if (reuse == MAT_INITIAL_MATRIX) {
    MPI_Comm               comm;
    MPI_Request            *reqs;
    PetscMPIInt            tag;
    PetscInt               Cm;

    ierr = PetscObjectGetComm((PetscObject)ownerSF,&comm);CHKERRQ(ierr);
    ierr = PetscCommGetNewTag(comm,&tag);CHKERRQ(ierr);

    PetscInt niranks,nranks,nroots,nleaves;
    const PetscMPIInt *iranks,*ranks;
    const PetscInt *ioffset,*rows,*roffset;  /* rows[] contains local indices of rows scattered to me from others. ioffset[] is a CSR on rows[] */
    ierr = PetscSFSetUp(ownerSF);CHKERRQ(ierr);
    ierr = PetscSFGetLeafRanks(ownerSF,&niranks,&iranks,&ioffset,&rows);CHKERRQ(ierr); /* recv info: iranks[] will send rows to me */
    ierr = PetscSFGetRootRanks(ownerSF,&nranks,&ranks,&roffset,NULL/*rmine*/,NULL/*rremote*/);CHKERRQ(ierr); /* send info */
    ierr = PetscSFGetGraph(ownerSF,&nroots,&nleaves,NULL,NULL);CHKERRQ(ierr);
    PetscCheckFalse(nleaves != Am,PETSC_COMM_SELF,PETSC_ERR_PLIB,"ownerSF's nleaves(%" PetscInt_FMT ") != row size of A(%" PetscInt_FMT ")",nleaves,Am);
    Cm    = nroots;
    nrows = ioffset[niranks]; /* # of rows to be received. Might receive same row (each is partial) from different senders */

    /* Tell owners how long each row I will send */
    PetscInt                *srowlens; /* send buf of row lens */
    MatRowMapKokkosViewHost rrowlens_h("rrowoffset_h",nrows+1); /* recv buf of row lens. +1 to make CSR later. Memory might be passed to other views */
    PetscInt                *rrowlens = rrowlens_h.data();

    ierr = PetscMalloc2(Am,&srowlens,niranks+nranks,&reqs);CHKERRQ(ierr);
    for (i=0; i<Am; i++) srowlens[i] = Ai[i+1] - Ai[i];
    rrowlens[0] = 0;
    rrowlens++; /* shift the pointer to make the following expression more readable */
    for (i=0; i<niranks; i++){ierr = MPI_Irecv(&rrowlens[ioffset[i]],ioffset[i+1]-ioffset[i],MPIU_INT,iranks[i],tag,comm,&reqs[i]);CHKERRMPI(ierr);}
    for (i=0; i<nranks; i++) {ierr = MPI_Isend(&srowlens[roffset[i]],roffset[i+1]-roffset[i],MPIU_INT,ranks[i],tag,comm,&reqs[niranks+i]);CHKERRMPI(ierr);}
    ierr = MPI_Waitall(niranks+nranks,reqs,MPI_STATUSES_IGNORE);CHKERRMPI(ierr);

    /* Owner builds Ci on host by histogramming rrowlens[] */
    MatRowMapKokkosViewHost Ci_h("i",Cm+1);
    Kokkos::deep_copy(Ci_h,0); /* Zero Ci */
    MatRowMapType *Ci_ptr = Ci_h.data();

    for (i=0; i<nrows; i++) {
      r = rows[i]; /* local row id of i-th received row */
     #if defined(PETSC_USE_DEBUG)
      PetscCheckFalse(r<0 || r>=Cm,PETSC_COMM_SELF,PETSC_ERR_PLIB,"local row id (%" PetscInt_FMT ") is out of range [0,%" PetscInt_FMT ")",r,Cm);
     #endif
      Ci_ptr[r+1] += rrowlens[i]; /* add to length of row r in C */
    }
    for (i=0; i<Cm; i++) Ci_ptr[i+1] += Ci_ptr[i]; /* to CSR format */
    Cnnz = Ci_ptr[Cm];

    /* For each received row, compute src & dst offsets in memory copying (from recv bufs abuf, jbuf to Ca, Cj) */
    MatRowMapKokkosViewHost dstrowoffset_h("dstrowoffset_h",nrows);
    PetscInt                *dstrowoffset_hptr = dstrowoffset_h.data();
    PetscInt                *currowlens; /* Current row lens. They are temp accumulators for row lens in C, to help build dstrowoffset */

    ierr = PetscCalloc1(Cm,&currowlens);CHKERRQ(ierr); /* Init with zero, to be added to */
    for (i=0; i<nrows; i++) { /* for each row I receive */
      r                    = rows[i]; /* row id in C */
      dstrowoffset_hptr[i] = Ci_ptr[r] + currowlens[r]; /* dst offset of the new place for each recv'ed row in Ca/Cj */
      currowlens[r]       += rrowlens[i]; /* accumulate to length of row r in C */
    }
    ierr = PetscFree(currowlens);CHKERRQ(ierr);

    rrowlens--;
    for (i=0; i<nrows; i++) rrowlens[i+1] += rrowlens[i]; /* Change rrowlens[] to CSR format */
    dstrowoffset = Kokkos::create_mirror_view_and_copy(DefaultMemorySpace(),dstrowoffset_h);
    srcrowoffset = Kokkos::create_mirror_view_and_copy(DefaultMemorySpace(),rrowlens_h); /* src offset of each recv'ed row in abuf/jbuf */

    /* Build the reduceSF, which performs buffer to buffer send/recv */
    PetscInt *sdisp,*rdisp; /* buffer to send offsets of roots, and buffer to recv them */
    ierr = PetscMalloc2(niranks,&sdisp,nranks,&rdisp);CHKERRQ(ierr);
    for (i=0; i<niranks; i++) sdisp[i] = rrowlens[ioffset[i]];
    for (i=0; i<nranks; i++)  {ierr = MPI_Irecv(&rdisp[i],1,MPIU_INT,ranks[i],tag,comm,&reqs[i]);CHKERRMPI(ierr);}
    for (i=0; i<niranks; i++) {ierr = MPI_Isend(&sdisp[i],1,MPIU_INT,iranks[i],tag,comm,&reqs[nranks+i]);CHKERRMPI(ierr);}
    ierr = MPI_Waitall(niranks+nranks,reqs,MPI_STATUSES_IGNORE);CHKERRMPI(ierr);

    /* Nonzeros in abuf/jbuf are roots and those in A are leaves */
    PetscInt    nroots2 = Cnnz,nleaves2 = Annz;
    PetscSFNode *iremote;
    ierr = PetscMalloc1(nleaves2,&iremote);CHKERRQ(ierr); /* no free, since memory will be given to reduceSF */
    for (i=0; i<nranks; i++) {
      PetscInt rootbase = rdisp[i]; /* root offset at this root rank */
      PetscInt leafbase = Ai[roffset[i]]; /* leaf base */
      PetscInt nz       = Ai[roffset[i+1]] - leafbase; /* I will send nz nonzeros to this root rank */
      for (PetscInt k=0; k<nz; k++) {
        iremote[leafbase+k].rank  = ranks[i];
        iremote[leafbase+k].index = rootbase + k;
      }
    }
    ierr = PetscSFCreate(comm,&reduceSF);CHKERRQ(ierr);
    ierr = PetscSFSetGraph(reduceSF,nroots2,nleaves2,NULL,PETSC_OWN_POINTER,iremote,PETSC_OWN_POINTER);CHKERRQ(ierr);
    ierr = PetscFree2(sdisp,rdisp);CHKERRQ(ierr);

    /* Reduce Aa, Ajg to abuf and jbuf */

    /* If A uses local col ids, convert them to global ones before sending */
    MatColIdxKokkosView Ajg;
    if (local) {
      Ajg = MatColIdxKokkosView("j",Annz);
      const MatColIdxKokkosView& Aj = akok->j_dual.view_device();
      Kokkos::parallel_for(Annz,KOKKOS_LAMBDA(const PetscInt i) {Ajg(i) = l2g(Aj(i));});
    } else {
      Ajg = akok->j_dual.view_device(); /* no data copy, just take a reference */
    }

    MatColIdxKokkosView   jbuf("jbuf",Cnnz);
    abuf = MatScalarKokkosView("abuf",Cnnz);
    ierr = PetscSFReduceBegin(reduceSF,MPIU_INT,   Ajg.data(),           jbuf.data(),MPI_REPLACE);CHKERRMPI(ierr);
    ierr = PetscSFReduceEnd  (reduceSF,MPIU_INT,   Ajg.data(),           jbuf.data(),MPI_REPLACE);CHKERRMPI(ierr);
    ierr = PetscSFReduceBegin(reduceSF,MPIU_SCALAR,akok->a_device_data(),abuf.data(),MPI_REPLACE);CHKERRMPI(ierr);
    ierr = PetscSFReduceEnd  (reduceSF,MPIU_SCALAR,akok->a_device_data(),abuf.data(),MPI_REPLACE);CHKERRMPI(ierr);

    /* Copy data from abuf, jbuf to Ca, Cj */
    MatRowMapKokkosView    Ci = Kokkos::create_mirror_view_and_copy(DefaultMemorySpace(),Ci_h); /* Ci is an alias of Ci_h if no device */
    MatColIdxKokkosView    Cj("j",Cnnz);
    MatScalarKokkosView    Ca("a",Cnnz);

    Kokkos::parallel_for(Kokkos::TeamPolicy<>(nrows, Kokkos::AUTO()),KOKKOS_LAMBDA(const KokkosTeamMemberType& t) {
      PetscInt i   = t.league_rank();
      PetscInt src = srcrowoffset(i), dst = dstrowoffset(i);
      PetscInt len = srcrowoffset(i+1) - srcrowoffset(i);
      Kokkos::parallel_for(Kokkos::TeamThreadRange(t,len), [&](PetscInt k) {
        Ca(dst+k) = abuf(src+k);
        Cj(dst+k) = jbuf(src+k);
      });
    });

    /* Build C with Ca, Ci, Cj */
    C    = KokkosCsrMatrix("csrmat",Cm,N,Cnnz,Ca,Ci,Cj);
    ierr = PetscFree2(srowlens,reqs);CHKERRQ(ierr);
  } else SETERRQ(PETSC_COMM_SELF,PETSC_ERR_PLIB,"Unsupported MatReuse enum %d",reuse);
  PetscFunctionReturn(0);
}

/* MatSetMPIAIJKokkosWithGlobalCSRMatrix - Set the diag and offdiag parts of a MATMPIAIJKOKKOS matrix by splitting a KokkosCsrMatrix

  Input Parameters:
+  C        - the MATMPIAIJKOKKOS matrix, of size m,n,M,N
.  reuse    - indicate whether the matrix has called this function before
.  csrmat   - the KokkosCsrMatrix, of size m,N
-  Cdstart  - when reuse == MAT_REUSE_MATRIX, it is an input parameter. For each row in csrmat, it stores the start of the first
              entry of the diag block of C in csrmat's j array. E.g, if row i has col ids = {0, 3, 4, 5, 7, 9} and the first diag
              entry is 5, then Cdstart[i] = 3.

  Output Parameters:
+  C        - the updated MATMPIAIJKOKKOS matrix
-  Cdstart - when reuse == MAT_INITIAL_MATRIX, it is an output parameter

  Notes:
   Between calls with MAT_INITIAL_MATRIX or MAT_REUSE_MATRIX, csrmat must have the same nonzero pattern
 */
static PetscErrorCode MatSetMPIAIJKokkosWithGlobalCSRMatrix(Mat C,MatReuse reuse,const KokkosCsrMatrix& csrmat,MatRowMapKokkosView& Cdstart)
{
  PetscErrorCode                  ierr;
  const MatScalarKokkosView&      Ca = csrmat.values;
  const ConstMatRowMapKokkosView& Ci = csrmat.graph.row_map;
  PetscInt                        m,n,N;

  PetscFunctionBegin;
  ierr = MatGetLocalSize(C,&m,&n);CHKERRQ(ierr);
  ierr = MatGetSize(C,NULL,&N);CHKERRQ(ierr);

  if (reuse == MAT_REUSE_MATRIX) {
    Mat_MPIAIJ                  *mpiaij = static_cast<Mat_MPIAIJ*>(C->data);
    Mat_SeqAIJKokkos            *akok = static_cast<Mat_SeqAIJKokkos*>(mpiaij->A->spptr);
    Mat_SeqAIJKokkos            *bkok = static_cast<Mat_SeqAIJKokkos*>(mpiaij->B->spptr);
    const MatScalarKokkosView&  Cda = akok->a_dual.view_device(),Coa = bkok->a_dual.view_device();
    const MatRowMapKokkosView&  Cdi = akok->i_dual.view_device(),Coi = bkok->i_dual.view_device();

    /* Fill 'a' of Cd and Co on device */
    Kokkos::parallel_for(Kokkos::TeamPolicy<>(m, Kokkos::AUTO()),KOKKOS_LAMBDA(const KokkosTeamMemberType& t) {
      PetscInt i       = t.league_rank(); /* row i */
      PetscInt clen    = Ci(i+1) - Ci(i); /* len of row i of C */
      PetscInt cdlen   = Cdi(i+1) - Cdi(i); /* len of row i of Cd */
      PetscInt cdstart = Cdstart(i); /* [start, end) of row i of Cd in C */
      PetscInt cdend   = cdstart + cdlen;
      /* [0, clen) is cut into three blocks: [0, cdstart), [cdstart, cdend), [cdend, clen) */
      Kokkos::parallel_for(Kokkos::TeamThreadRange(t, clen), [&](PetscInt k) {
        if (k < cdstart) {  /* k in [0, cdstart) */
          Coa(Coi(i)+k) = Ca(Ci(i)+k);
        } else if (k < cdend) { /* k in [cdstart, cdend) */
          Cda(Cdi(i)+(k-cdstart)) = Ca(Ci(i)+k);
        } else { /* k in [cdend, clen) */
          Coa(Coi(i)+k-cdlen) = Ca(Ci(i)+k);
        }
      });
    });

    akok->a_dual.modify_device();
    bkok->a_dual.modify_device();
  } else if (reuse == MAT_INITIAL_MATRIX) {
    Mat                         Cd,Co;
    const MatColIdxKokkosView&  Cj = csrmat.graph.entries;
    MatRowMapKokkosDualView     Cdi_dual("i",m+1),Coi_dual("i",m+1);
    MatRowMapKokkosView         Cdi = Cdi_dual.view_device(),Coi = Coi_dual.view_device();
    PetscInt                    cstart,cend;

    /* Note that each row of C is sorted by col ids. We want to find out how to cut each row into three blocks:
       left to the diag block, diag block, right to the diag block. The diag block have col ids in [cstart,cend).
       Suppose a row of C has len nonzeros, indexed by [0, len). We want to know two indices: cdstart and cdend,
       such that the three blocks are [0,cdstart), [cdstart,cdend), [cdend,len). The following code equivalentaly
       stores values of cdstart and cdend-cstart (aka Cdi[]) instead.
     */
    Cdstart = MatRowMapKokkosView("Cdstart",m);
    ierr    = PetscLayoutGetRange(C->cmap,&cstart,&cend);CHKERRQ(ierr); /* Not MatGetOwnershipRangeColumn() since C has not been preallocated yet */

    /* I could use RangePolicy and one thread per row. But since each thread essentially does binary search, threads in a
      CUDA warp would completely diverge. So I use TeamPolicy with a team size 1.
     */
    Kokkos::parallel_for(Kokkos::TeamPolicy<>(m, 1),KOKKOS_LAMBDA(const KokkosTeamMemberType& t) {
      Kokkos::single(Kokkos::PerTeam(t), [=] () { /* Only one thread works in a team */
        PetscInt i = t.league_rank(); /* row i */
        PetscInt j,first,count,step;

        if (i == 0) { /* Set the first entry of the i arrays to zero on device, to be used in CSR */
          Cdi(0) = 0;
          Coi(0) = 0;
        }

        /* Do std::lower_bound(Ci(i),Ci(i+1),cstart) on Cj[]. We use j as the iterator. lower_bound() returns
          in 'first' the first iterator with a value >= cstart, or last iterator if no such element is found.
        */
        count = Ci(i+1)-Ci(i);
        first = Ci(i);
        while (count > 0) {
          j    = first;
          step = count / 2;
          j   += step;
          if (Cj(j) < cstart) {
            first  = ++j;
            count -= step + 1;
          } else count = step;
        }
        Cdstart(i) = first - Ci(i); /* 'first' is the while-loop's output */

        /* Do std::lower_bound(first,Ci(i+1),cend) on Cj[] */
        count = Ci(i+1) - first;
        while (count > 0) {
          j    = first;
          step = count / 2;
          j   += step;
          if (Cj(j) < cend) {
            first  = ++j;
            count -= step + 1;
          } else count = step;
        }
        Cdi(i+1) = first - (Ci(i)+Cdstart(i)); /* 'first' is the while-loop's output */
        Coi(i+1) = (Ci(i+1)-Ci(i)) - Cdi(i+1); /* Co's row len = C's row len - Cd's row len */
      });
    });

    /* Convert row lens in Cdi[], Coi[] to CSR format using inclusive scan, e.g., changing [0,1,2,3] into [0,1,3,6] */
    Kokkos::parallel_scan(m+1,KOKKOS_LAMBDA(const PetscInt i,PetscInt& update,const bool final) {
      update += Cdi(i);
      if (final) Cdi(i) = update;
    });
    Kokkos::parallel_scan(m+1,KOKKOS_LAMBDA(const PetscInt i,PetscInt& update,const bool final) {
      update += Coi(i);
      if (final) Coi(i) = update;
    });

    /* Get Cdi, Coi on host (it is not a waste, since we do need them on host in
       MatCreateSeqAIJKokkosWithCSRMatrix() below), then get nnz of Cd and Co.
    */
    Cdi_dual.modify_device();
    Coi_dual.modify_device();
    Cdi_dual.sync_host();
    Coi_dual.sync_host();
    PetscInt Cd_nnz = Cdi_dual.view_host().data()[m];
    PetscInt Co_nnz = Coi_dual.view_host().data()[m];

    /* With nnz, allocate a, j for Cd and Co */
    MatColIdxKokkosDualView Cdj_dual("j",Cd_nnz),Coj_dual("j",Co_nnz);
    MatScalarKokkosDualView Cda_dual("a",Cd_nnz),Coa_dual("a",Co_nnz);

    /* Fill a, j of Cd and Co on device */
    MatColIdxKokkosView     Cdj = Cdj_dual.view_device(),Coj = Coj_dual.view_device();
    MatScalarKokkosView     Cda = Cda_dual.view_device(),Coa = Coa_dual.view_device();

    Kokkos::parallel_for(Kokkos::TeamPolicy<>(m, Kokkos::AUTO()),KOKKOS_LAMBDA(const KokkosTeamMemberType& t) {
      PetscInt i       = t.league_rank(); /* row i */
      PetscInt clen    = Ci(i+1) - Ci(i); /* len of row i of C */
      PetscInt cdlen   = Cdi(i+1) - Cdi(i); /* len of row i of Cd */
      PetscInt cdstart = Cdstart(i); /* [start, end) of row i of Cd in C */
      PetscInt cdend   = cdstart + cdlen;
      /* [0, clen) is cut into three blocks: [0, cdstart), [cdstart, cdend), [cdend, clen) */
      Kokkos::parallel_for(Kokkos::TeamThreadRange(t, clen), [&](PetscInt k) {
        if (k < cdstart) { /* k in [0, cdstart) */
          Coa(Coi(i)+k) = Ca(Ci(i)+k);
          Coj(Coi(i)+k) = Cj(Ci(i)+k);
        } else if (k < cdend) { /* k in [cdstart, cdend) */
          Cda(Cdi(i)+(k-cdstart)) = Ca(Ci(i)+k);
          Cdj(Cdi(i)+(k-cdstart)) = Cj(Ci(i)+k) - cstart; /* Use local col ids in Cdj */
        } else { /* k in [cdend, clen) */
          Coa(Coi(i)+k-cdlen) = Ca(Ci(i)+k);
          Coj(Coi(i)+k-cdlen) = Cj(Ci(i)+k);
        }
      });
    });

    Cdj_dual.modify_device();
    Cda_dual.modify_device();
    Coj_dual.modify_device();
    Coa_dual.modify_device();
    /* With a, i, j for Cd and Co, finally build Cd, Co and then C. Their offloadmask will be set in each's MatAssemblyEnd */
    auto cdkok = new Mat_SeqAIJKokkos(m,n,Cd_nnz,Cdi_dual,Cdj_dual,Cda_dual);
    auto cokok = new Mat_SeqAIJKokkos(m,N,Co_nnz,Coi_dual,Coj_dual,Coa_dual);
    ierr = MatCreateSeqAIJKokkosWithCSRMatrix(PETSC_COMM_SELF,cdkok,&Cd);CHKERRQ(ierr);
    ierr = MatCreateSeqAIJKokkosWithCSRMatrix(PETSC_COMM_SELF,cokok,&Co);CHKERRQ(ierr);
    ierr = MatSetMPIAIJKokkosWithSplitSeqAIJKokkosMatrices(C,Cd,Co);CHKERRQ(ierr); /* Coj will be converted to local ids within */
  }
  PetscFunctionReturn(0);
}

/* MatSeqAIJCompactOutExtraColumns_SeqAIJKokkos - Compact a SEQAIJKOKKS matrix's global col ids.

  It is similar to MatSeqAIJCompactOutExtraColumns_SeqAIJ, but it applies to SEQAIJKOKKOS and returns the l2g map in Kokkos view.

  Input Parameters:
+  C        - the MATMPIAIJKOKKOS matrix, of size m,n,M,N
.  reuse    - indicate whether the matrix has called this function before
.  csrmat   - the KokkosCsrMatrix, of size m,N
-  Cdoffset - when reuse == MAT_REUSE_MATRIX, it is an input parameter. For each row in csrmat, it stores the offset of the first
              entry of the diag block of C in csrmat's j array.

  Output Parameters:
+  C        - the updated MATMPIAIJKOKKOS matrix
-  Cdoffset - when reuse == MAT_INITIAL_MATRIX, it is an output parameter

  Notes: the input matrix's col ids and col size will be changed.
*/
static PetscErrorCode MatSeqAIJCompactOutExtraColumns_SeqAIJKokkos(Mat C,MatColIdxKokkosView& l2g)
{
  PetscErrorCode         ierr;
  Mat_SeqAIJKokkos       *ckok;
  ISLocalToGlobalMapping l2gmap;
  const PetscInt         *garray;
  PetscInt               sz;

  PetscFunctionBegin;
  /* Compact P_other's global col ids and col size. We do it since we guess with local ids KK might be more memory scalable */
  ierr = MatSeqAIJCompactOutExtraColumns_SeqAIJ(C,&l2gmap);CHKERRQ(ierr);
  ckok = static_cast<Mat_SeqAIJKokkos*>(C->spptr);
  ckok->j_dual.modify_host(); /* P_other's j is modified on host; we need to sync it on device */
  ckok->j_dual.sync_device();
  ckok->SetColSize(C->cmap->n); /* Update col size of the csrmat in spptr */

  /* Build l2g -- the local to global mapping of C's cols */
  ierr = ISLocalToGlobalMappingGetIndices(l2gmap,&garray);CHKERRQ(ierr);
  ierr = ISLocalToGlobalMappingGetSize(l2gmap,&sz);CHKERRQ(ierr);
  PetscCheckFalse(C->cmap->n != sz,PETSC_COMM_SELF,PETSC_ERR_PLIB,"matrix column size(%" PetscInt_FMT ") != l2g mapping size(%" PetscInt_FMT ")", C->cmap->n,sz);

  ConstMatColIdxKokkosViewHost tmp(garray,sz);
  l2g = MatColIdxKokkosView("l2g",sz);
  Kokkos::deep_copy(l2g,tmp);

  ierr = ISLocalToGlobalMappingRestoreIndices(l2gmap,&garray);CHKERRQ(ierr);
  ierr = ISLocalToGlobalMappingDestroy(&l2gmap);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/* MatProductSymbolic_MPIAIJKokkos_AB - AB flavor of MatProductSymbolic_MPIAIJKokkos

  Input Parameters:
+  product  - Mat_Product which carried out the computation. Passed in to access info about this mat product.
.  A        - an MPIAIJKOKKOS matrix
.  B        - an MPIAIJKOKKOS matrix
-  mm       - a struct used to stash intermediate data when computing AB. Persist from symbolic to numeric operations.

  Notes: The local part of the result C is stored as mm->C_global, which is of type KokkosCsrMatrix and uses global col ids.
*/
static PetscErrorCode MatProductSymbolic_MPIAIJKokkos_AB(Mat_Product *product,Mat A,Mat B,MatMatStruct_AB *mm)
{
  PetscErrorCode              ierr;
  Mat_MPIAIJ                  *a = static_cast<Mat_MPIAIJ*>(A->data);
  Mat                         Ad = a->A,Ao = a->B; /* diag and offdiag of A */
  IS                          glob = NULL;
  const PetscInt              *garray;
  PetscInt                    N = B->cmap->N,sz;
  ConstMatColIdxKokkosView    l2g1; /* two temp maps mapping local col ids to global ones */
  MatColIdxKokkosView         l2g2;
  Mat                         C1,C2; /* intermediate matrices */

  PetscFunctionBegin;
  /* C1 = Ad * B_local. B_local is a matrix got by merging Bd and Bo, and uses local col ids */
  ierr = MatMPIAIJGetLocalMatMerge(B,MAT_INITIAL_MATRIX,&glob,&mm->B_local);CHKERRQ(ierr);
  ierr = MatProductCreate(Ad,mm->B_local,NULL,&C1);CHKERRQ(ierr);
  ierr = MatProductSetType(C1,MATPRODUCT_AB);CHKERRQ(ierr);
  ierr = MatProductSetFill(C1,product->fill);CHKERRQ(ierr);
  C1->product->api_user = product->api_user;
  ierr = MatProductSetFromOptions(C1);CHKERRQ(ierr);
  PetscCheckFalse(!C1->ops->productsymbolic,PETSC_COMM_SELF,PETSC_ERR_PLIB,"Missing symbolic op for %s",MatProductTypes[C1->product->type]);
  ierr = (*C1->ops->productsymbolic)(C1);CHKERRQ(ierr);

  ierr = ISGetIndices(glob,&garray);CHKERRQ(ierr);
  ierr = ISGetSize(glob,&sz);CHKERRQ(ierr);
  const auto& tmp  = ConstMatColIdxKokkosViewHost(garray,sz); /* wrap garray as a view */
  l2g1 = Kokkos::create_mirror_view_and_copy(DefaultMemorySpace(),tmp); /* maybe just an alias to tmp, so we restore garray at the very end */
  ierr = MatSeqAIJKokkosGetCSRMatrixWithGlobalColumnIds(C1,N,l2g1,mm->C1_global);

  /* C2 = Ao * B_other. B_other is a matrix consisting of needed rows of B gathered from other procs */
  ierr = MatSeqAIJKokkosBcast(mm->B_local,MAT_INITIAL_MATRIX,N,l2g1,a->Mvctx,mm->sf,
                              mm->abuf,mm->rows,mm->rowoffset,mm->B_other);CHKERRQ(ierr);

  /* Compact B_other to use local ids as we guess KK spgemm is more memroy scalable with that; We could skip the compaction to simplify code */
  ierr = MatSeqAIJCompactOutExtraColumns_SeqAIJKokkos(mm->B_other,l2g2);CHKERRQ(ierr);
  ierr = MatProductCreate(Ao,mm->B_other,NULL,&C2);CHKERRQ(ierr);
  ierr = MatProductSetType(C2,MATPRODUCT_AB);CHKERRQ(ierr);
  ierr = MatProductSetFill(C2,product->fill);CHKERRQ(ierr);
  C2->product->api_user = product->api_user;
  ierr = MatProductSetFromOptions(C2);CHKERRQ(ierr);
  PetscCheckFalse(!C2->ops->productsymbolic,PETSC_COMM_SELF,PETSC_ERR_PLIB,"Missing symbolic op for %s",MatProductTypes[C2->product->type]);
  ierr = (*C2->ops->productsymbolic)(C2);CHKERRQ(ierr);
  ierr = MatSeqAIJKokkosGetCSRMatrixWithGlobalColumnIds(C2,N,l2g2,mm->C2_global);

  /* C = C1 + C2.  We actually use their global col ids versions in adding */
  mm->kh.create_spadd_handle(false); /* Input C1, C2 are NOT sorted, since B_local, B_other are not */
  KokkosSparse::spadd_symbolic(&mm->kh,mm->C1_global,mm->C2_global,mm->C_global);
  /* Have to do numeric since spadd_symbolic does not really populate column indices of the result matrix */
  KokkosSparse::spadd_numeric(&mm->kh,(MatScalarType)1.0,mm->C1_global,(MatScalarType)1.0,mm->C2_global,mm->C_global);

  mm->C1 = C1;
  mm->C2 = C2;
  ierr = ISRestoreIndices(glob,&garray);CHKERRQ(ierr);
  ierr = ISDestroy(&glob);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/* MatProductSymbolic_MPIAIJKokkos_AtB - A^tB flavor of MatProductSymbolic_MPIAIJKokkos

  Input Parameters:
+  product  - Mat_Product which carried out the computation. Passed in to access info about this mat product.
.  A        - an MPIAIJKOKKOS matrix
.  B        - a SEQAIJKOKKOS matrix. It works as if A^t is multiplied by a parallel matrix made up of Bs on each rank.
.  localB   - Does B use local col ids? If false, then B is already in global col ids.
.  N        - col size of the "parallel B matrix". It implies B's global col ids are in range of [0,N) and N is the same across the communicator.
.  l2g      - If localB, then l2g maps B's local col ids to global ones.
-  mm       - a struct used to stash intermediate data in AtB

  Notes: The local part of the result C is stored as mm->C_global, which is of type KokkosCsrMatrix and uses global col ids.
*/
static PetscErrorCode MatProductSymbolic_MPIAIJKokkos_AtB(Mat_Product *product,Mat A,Mat B,PetscBool localB,PetscInt N,const ConstMatColIdxKokkosView& l2g,MatMatStruct_AtB *mm)
{
  PetscErrorCode         ierr;
  Mat_MPIAIJ             *a = static_cast<Mat_MPIAIJ*>(A->data);
  Mat                    Ad = a->A,Ao = a->B; /* diag and offdiag of A */
  Mat                    C1,C2; /* intermediate matrices */

  PetscFunctionBegin;
  /* C1 = Ad^t * B */
  ierr = MatProductCreate(Ad,B,NULL,&C1);CHKERRQ(ierr);
  ierr = MatProductSetType(C1,MATPRODUCT_AtB);CHKERRQ(ierr);
  ierr = MatProductSetFill(C1,product->fill);CHKERRQ(ierr);
  C1->product->api_user = product->api_user;
  ierr = MatProductSetFromOptions(C1);CHKERRQ(ierr);
  PetscCheckFalse(!C1->ops->productsymbolic,PETSC_COMM_SELF,PETSC_ERR_PLIB,"Missing symbolic op for %s",MatProductTypes[C1->product->type]);
  ierr = (*C1->ops->productsymbolic)(C1);CHKERRQ(ierr);

  if (localB) {ierr = MatSeqAIJKokkosGetCSRMatrixWithGlobalColumnIds(C1,N,l2g,mm->C1_global);}
  else mm->C1_global = static_cast<Mat_SeqAIJKokkos*>(C1->spptr)->csrmat; /* the csrmat already uses global col ids */

  /* C2 = Ao^t * B */
  ierr = MatProductCreate(Ao,B,NULL,&C2);CHKERRQ(ierr);
  ierr = MatProductSetType(C2,MATPRODUCT_AtB);CHKERRQ(ierr);
  ierr = MatProductSetFill(C2,product->fill);CHKERRQ(ierr);
  C2->product->api_user = product->api_user;
  ierr = MatProductSetFromOptions(C2);CHKERRQ(ierr);
  PetscCheckFalse(!C2->ops->productsymbolic,PETSC_COMM_SELF,PETSC_ERR_PLIB,"Missing symbolic op for %s",MatProductTypes[C2->product->type]);
  ierr = (*C2->ops->productsymbolic)(C2);CHKERRQ(ierr);

  ierr = MatSeqAIJKokkosReduce(C2,MAT_INITIAL_MATRIX,localB,N,l2g,a->Mvctx,mm->sf,mm->abuf,
                               mm->srcrowoffset,mm->dstrowoffset,mm->C2_global);CHKERRQ(ierr);

  mm->kh.create_spadd_handle(false); /* Input C1, C2 are NOT sorted, since B may be not */
  KokkosSparse::spadd_symbolic(&mm->kh,mm->C1_global,mm->C2_global,mm->C_global);
  /* Have to do numeric since spadd_symbolic does not really populate column indices of the result matrix */
  KokkosSparse::spadd_numeric(&mm->kh,(MatScalarType)1.0,mm->C1_global,(MatScalarType)1.0,mm->C2_global,mm->C_global);
  mm->C1 = C1;
  mm->C2 = C2;
  PetscFunctionReturn(0);
}

PetscErrorCode MatProductNumeric_MPIAIJKokkos(Mat C)
{
  PetscErrorCode                ierr;
  Mat_Product                   *product = C->product;
  MatProductType                ptype;
  MatProductData_MPIAIJKokkos   *mmdata;
  MatMatStruct                  *mm = NULL;
  MatMatStruct_AB               *ab;
  MatMatStruct_AtB              *atb;
  Mat                           A,B,Ad,Ao,Bd,Bo;
  const MatScalarType           one = 1.0; /* Not use literal 1.0 directly, to avoid wrong template instantiation in KokkosSparse::spadd_numeric */

  PetscFunctionBegin;
  MatCheckProduct(C,1);
  mmdata = static_cast<MatProductData_MPIAIJKokkos*>(product->data);
  ptype  = product->type;
  A      = product->A;
  B      = product->B;
  Ad     = static_cast<Mat_MPIAIJ*>(A->data)->A;
  Ao     = static_cast<Mat_MPIAIJ*>(A->data)->B;
  Bd     = static_cast<Mat_MPIAIJ*>(B->data)->A;
  Bo     = static_cast<Mat_MPIAIJ*>(B->data)->B;

  if (mmdata->reusesym) { /* We reached here through e.g., MatMatMult(A,B,MAT_INITIAL_MATRIX,..,C), where symbolic/numeric are combined */
    mmdata->reusesym = PETSC_FALSE; /* So that next time when user calls MatMatMult(E,F,MAT_REUSE_MATRIX,..,C), we still do numeric  */
    ab  = mmdata->mmAB;
    atb = mmdata->mmAtB;
    if (ab) {
      static_cast<MatProductData_SeqAIJKokkos*>(ab->C1->product->data)->reusesym = PETSC_FALSE;
      static_cast<MatProductData_SeqAIJKokkos*>(ab->C2->product->data)->reusesym = PETSC_FALSE;
    }
    if (atb) {
      static_cast<MatProductData_SeqAIJKokkos*>(atb->C1->product->data)->reusesym = PETSC_FALSE;
      static_cast<MatProductData_SeqAIJKokkos*>(atb->C2->product->data)->reusesym = PETSC_FALSE;
    }
    PetscFunctionReturn(0);
  }

  if (ptype == MATPRODUCT_AB) {
    ab   = mmdata->mmAB;
    /* C1 = Ad * B_local */
    PetscCheckFalse(!ab->C1->ops->productnumeric || !ab->C2->ops->productnumeric,PETSC_COMM_SELF,PETSC_ERR_PLIB,"Missing numeric op for MATPRODUCT_AB");
    ierr = MatMPIAIJGetLocalMatMerge(B,MAT_REUSE_MATRIX,NULL/*glob*/,&ab->B_local);CHKERRQ(ierr);
    PetscCheckFalse(ab->C1->product->B != ab->B_local,PETSC_COMM_SELF,PETSC_ERR_PLIB,"In MATPRODUCT_AB, internal mat product matrix C1->B has unexpectedly changed");
    if (ab->C1->product->A != Ad) {ierr = MatProductReplaceMats(Ad,NULL,NULL,ab->C1);CHKERRQ(ierr);}
    ierr = (*ab->C1->ops->productnumeric)(ab->C1);CHKERRQ(ierr);
    ierr = MatSeqAIJKokkosBcast(ab->B_local,MAT_REUSE_MATRIX,0/*N*/,MatColIdxKokkosView()/*l2g*/,NULL/*ownerSF*/,ab->sf,
                                ab->abuf,ab->rows,ab->rowoffset,ab->B_other);CHKERRQ(ierr);
    /* C2 = Ao * B_other */
    PetscCheckFalse(ab->C2->product->B != ab->B_other,PETSC_COMM_SELF,PETSC_ERR_PLIB,"In MATPRODUCT_AB, internal mat product matrix C2->B has unexpectedly changed");
    if (ab->C1->product->A != Ao) {ierr = MatProductReplaceMats(Ao,NULL,NULL,ab->C2);CHKERRQ(ierr);}
    ierr = (*ab->C2->ops->productnumeric)(ab->C2);CHKERRQ(ierr);
    /* C = C1_global + C2_global */
    KokkosSparse::spadd_numeric(&ab->kh,one,ab->C1_global,one,ab->C2_global,ab->C_global);
    mm = static_cast<MatMatStruct*>(ab);
  } else if (ptype == MATPRODUCT_AtB) {
    atb  = mmdata->mmAtB;
    PetscCheckFalse(!atb->C1->ops->productnumeric || !atb->C2->ops->productnumeric,PETSC_COMM_SELF,PETSC_ERR_PLIB,"Missing numeric op for MATPRODUCT_AtB");
    /* C1 = Ad^t * B_local */
    ierr = MatMPIAIJGetLocalMatMerge(B,MAT_REUSE_MATRIX,NULL/*glob*/,&atb->B_local);CHKERRQ(ierr);
    PetscCheckFalse(atb->C1->product->B != atb->B_local,PETSC_COMM_SELF,PETSC_ERR_PLIB,"In MATPRODUCT_AtB, internal mat product matrix C1->B has unexpectedly changed");
    if (atb->C1->product->A != Ad) {ierr = MatProductReplaceMats(Ad,NULL,NULL,atb->C1);CHKERRQ(ierr);}
    ierr = (*atb->C1->ops->productnumeric)(atb->C1);CHKERRQ(ierr);

    /* C2 = Ao^t * B_local */
    PetscCheckFalse(atb->C2->product->B != atb->B_local,PETSC_COMM_SELF,PETSC_ERR_PLIB,"In MATPRODUCT_AtB, internal mat product matrix C2->B has unexpectedly changed");
    if (atb->C2->product->A != Ao) {ierr = MatProductReplaceMats(Ao,NULL,NULL,atb->C2);CHKERRQ(ierr);}
    ierr = (*atb->C2->ops->productnumeric)(atb->C2);CHKERRQ(ierr);
    /* Form C2_global */
    ierr = MatSeqAIJKokkosReduce(atb->C2,MAT_REUSE_MATRIX,PETSC_TRUE,0/*N*/,MatColIdxKokkosView()/*l2g*/,NULL/*ownerSF*/,atb->sf,
                                 atb->abuf,atb->srcrowoffset,atb->dstrowoffset,atb->C2_global);CHKERRQ(ierr);
    /* C = C1_global + C2_global */
    KokkosSparse::spadd_numeric(&atb->kh,one,atb->C1_global,one,atb->C2_global,atb->C_global);
    mm = static_cast<MatMatStruct*>(atb);
  } else if (ptype == MATPRODUCT_PtAP) { /* BtAB */
    ab   = mmdata->mmAB;
    ierr = MatMPIAIJGetLocalMatMerge(B,MAT_REUSE_MATRIX,NULL/*glob*/,&ab->B_local);CHKERRQ(ierr);

    /* ab->C1 = Ad * B_local */
    PetscCheckFalse(ab->C1->product->B != ab->B_local,PETSC_COMM_SELF,PETSC_ERR_PLIB,"In MATPRODUCT_PtAP, internal mat product matrix ab->C1->B has unexpectedly changed");
    if (ab->C1->product->A != Ad) {ierr = MatProductReplaceMats(Ad,NULL,NULL,ab->C1);CHKERRQ(ierr);}
    ierr = (*ab->C1->ops->productnumeric)(ab->C1);CHKERRQ(ierr);
    ierr = MatSeqAIJKokkosBcast(ab->B_local,MAT_REUSE_MATRIX,0/*N*/,MatColIdxKokkosView()/*l2g*/,NULL/*ownerSF*/,ab->sf,
                                ab->abuf,ab->rows,ab->rowoffset,ab->B_other);CHKERRQ(ierr);
    /* ab->C2 = Ao * B_other */
    if (ab->C2->product->A != Ao) {ierr = MatProductReplaceMats(Ao,NULL,NULL,ab->C2);CHKERRQ(ierr);}
    ierr = (*ab->C2->ops->productnumeric)(ab->C2);CHKERRQ(ierr); /* C2 = Ao * B_other */
    KokkosSparse::spadd_numeric(&ab->kh,one,ab->C1_global,one,ab->C2_global,ab->C_global);

    /* atb->C1 = Bd^t * ab->C_petsc */
    atb  = mmdata->mmAtB;
    PetscCheckFalse(atb->C1->product->B != ab->C_petsc,PETSC_COMM_SELF,PETSC_ERR_PLIB,"In MATPRODUCT_PtAP, internal mat product matrix atb->C1->B has unexpectedly changed");
    if (atb->C1->product->A != Bd) {ierr = MatProductReplaceMats(Bd,NULL,NULL,atb->C1);CHKERRQ(ierr);}
    ierr = (*atb->C1->ops->productnumeric)(atb->C1);CHKERRQ(ierr);
    /* atb->C2 = Bo^t * ab->C_petsc */
    if (atb->C2->product->A != Bo) {ierr = MatProductReplaceMats(Bo,NULL,NULL,atb->C2);CHKERRQ(ierr);}
    ierr = (*atb->C2->ops->productnumeric)(atb->C2);CHKERRQ(ierr);
    ierr = MatSeqAIJKokkosReduce(atb->C2,MAT_REUSE_MATRIX,PETSC_FALSE,0/*N*/,MatColIdxKokkosView()/*l2g*/,NULL/*ownerSF*/,atb->sf,
                                 atb->abuf,atb->srcrowoffset,atb->dstrowoffset,atb->C2_global);CHKERRQ(ierr);
    KokkosSparse::spadd_numeric(&atb->kh,one,atb->C1_global,one,atb->C2_global,atb->C_global);
    mm = static_cast<MatMatStruct*>(atb);
  }
  /* Split C_global to form C */
  ierr = MatSetMPIAIJKokkosWithGlobalCSRMatrix(C,MAT_REUSE_MATRIX,mm->C_global,mm->Cdstart);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

PetscErrorCode MatProductSymbolic_MPIAIJKokkos(Mat C)
{
  PetscErrorCode              ierr;
  Mat                         A,B;
  Mat_Product                 *product = C->product;
  MatProductType              ptype;
  MatProductData_MPIAIJKokkos *mmdata;
  MatMatStruct                *mm = NULL;
  IS                          glob = NULL;
  const PetscInt              *garray;
  PetscInt                    m,n,M,N,sz;
  ConstMatColIdxKokkosView    l2g; /* map local col ids to global ones */

  PetscFunctionBegin;
  MatCheckProduct(C,1);
  PetscCheckFalse(product->data,PetscObjectComm((PetscObject)C),PETSC_ERR_PLIB,"Product data not empty");
  ptype = product->type;
  A     = product->A;
  B     = product->B;

  switch (ptype) {
    case MATPRODUCT_AB:   m = A->rmap->n; n = B->cmap->n; M = A->rmap->N; N = B->cmap->N; break;
    case MATPRODUCT_AtB:  m = A->cmap->n; n = B->cmap->n; M = A->cmap->N; N = B->cmap->N; break;
    case MATPRODUCT_PtAP: m = B->cmap->n; n = B->cmap->n; M = B->cmap->N; N = B->cmap->N; break; /* BtAB */
    default: SETERRQ(PetscObjectComm((PetscObject)C),PETSC_ERR_PLIB,"Not for product type %s",MatProductTypes[ptype]);
  }

  ierr = MatSetSizes(C,m,n,M,N);CHKERRQ(ierr);
  ierr = PetscLayoutSetUp(C->rmap);CHKERRQ(ierr);
  ierr = PetscLayoutSetUp(C->cmap);CHKERRQ(ierr);
  ierr = MatSetType(C,((PetscObject)A)->type_name);CHKERRQ(ierr);

  mmdata           = new MatProductData_MPIAIJKokkos();
  mmdata->reusesym = product->api_user;

  if (ptype == MATPRODUCT_AB) {
    mmdata->mmAB = new MatMatStruct_AB();
    ierr = MatProductSymbolic_MPIAIJKokkos_AB(product,A,B,mmdata->mmAB);CHKERRQ(ierr);
    mm   = static_cast<MatMatStruct*>(mmdata->mmAB);
  } else if (ptype == MATPRODUCT_AtB) {
    mmdata->mmAtB = new MatMatStruct_AtB();
    auto atb      = mmdata->mmAtB;
    ierr = MatMPIAIJGetLocalMatMerge(B,MAT_INITIAL_MATRIX,&glob,&atb->B_local);CHKERRQ(ierr);
    ierr = ISGetIndices(glob,&garray);CHKERRQ(ierr);
    ierr = ISGetSize(glob,&sz);CHKERRQ(ierr);
    l2g  = Kokkos::create_mirror_view_and_copy(DefaultMemorySpace(),ConstMatColIdxKokkosViewHost(garray,sz));
    ierr = MatProductSymbolic_MPIAIJKokkos_AtB(product,A,atb->B_local,PETSC_TRUE,N,l2g,atb);CHKERRQ(ierr);
    ierr = ISRestoreIndices(glob,&garray);CHKERRQ(ierr);
    ierr = ISDestroy(&glob);CHKERRQ(ierr);
    mm   = static_cast<MatMatStruct*>(atb);
  } else if (ptype == MATPRODUCT_PtAP) { /* BtAB */
    mmdata->mmAB  = new MatMatStruct_AB(); /* tmp=A*B */
    mmdata->mmAtB = new MatMatStruct_AtB(); /* C=B^t*tmp */
    auto ab       = mmdata->mmAB;
    auto atb      = mmdata->mmAtB;
    ierr = MatProductSymbolic_MPIAIJKokkos_AB(product,A,B,ab);CHKERRQ(ierr);
    auto tmp = new Mat_SeqAIJKokkos(ab->C_global); /* Memory will be owned by ab->C_petsc */
    ierr = MatCreateSeqAIJKokkosWithCSRMatrix(PETSC_COMM_SELF,tmp,&ab->C_petsc);CHKERRQ(ierr);
    ierr = MatProductSymbolic_MPIAIJKokkos_AtB(product,B,ab->C_petsc,PETSC_FALSE,N,l2g/*not used*/,atb);CHKERRQ(ierr);
    mm   = static_cast<MatMatStruct*>(atb);
  }
  /* Split the C_global into petsc A, B format */
  ierr = MatSetMPIAIJKokkosWithGlobalCSRMatrix(C,MAT_INITIAL_MATRIX,mm->C_global,mm->Cdstart);CHKERRQ(ierr);
  C->product->data        = mmdata;
  C->product->destroy     = MatProductDataDestroy_MPIAIJKokkos;
  C->ops->productnumeric  = MatProductNumeric_MPIAIJKokkos;
  PetscFunctionReturn(0);
}

PETSC_INTERN PetscErrorCode MatProductSetFromOptions_MPIAIJKokkos(Mat mat)
{
  PetscErrorCode ierr;
  Mat_Product    *product = mat->product;
  PetscBool      match = PETSC_FALSE;
  PetscBool      usecpu = PETSC_FALSE;

  PetscFunctionBegin;
  MatCheckProduct(mat,1);
  if (!product->A->boundtocpu && !product->B->boundtocpu) {
    ierr = PetscObjectTypeCompare((PetscObject)product->B,((PetscObject)product->A)->type_name,&match);CHKERRQ(ierr);
  }
  if (match) { /* we can always fallback to the CPU if requested */
    switch (product->type) {
    case MATPRODUCT_AB:
      if (product->api_user) {
        ierr = PetscOptionsBegin(PetscObjectComm((PetscObject)mat),((PetscObject)mat)->prefix,"MatMatMult","Mat");CHKERRQ(ierr);
        ierr = PetscOptionsBool("-matmatmult_backend_cpu","Use CPU code","MatMatMult",usecpu,&usecpu,NULL);CHKERRQ(ierr);
        ierr = PetscOptionsEnd();CHKERRQ(ierr);
      } else {
        ierr = PetscOptionsBegin(PetscObjectComm((PetscObject)mat),((PetscObject)mat)->prefix,"MatProduct_AB","Mat");CHKERRQ(ierr);
        ierr = PetscOptionsBool("-mat_product_algorithm_backend_cpu","Use CPU code","MatMatMult",usecpu,&usecpu,NULL);CHKERRQ(ierr);
        ierr = PetscOptionsEnd();CHKERRQ(ierr);
      }
      break;
    case MATPRODUCT_AtB:
      if (product->api_user) {
        ierr = PetscOptionsBegin(PetscObjectComm((PetscObject)mat),((PetscObject)mat)->prefix,"MatTransposeMatMult","Mat");CHKERRQ(ierr);
        ierr = PetscOptionsBool("-mattransposematmult_backend_cpu","Use CPU code","MatTransposeMatMult",usecpu,&usecpu,NULL);CHKERRQ(ierr);
        ierr = PetscOptionsEnd();CHKERRQ(ierr);
      } else {
        ierr = PetscOptionsBegin(PetscObjectComm((PetscObject)mat),((PetscObject)mat)->prefix,"MatProduct_AtB","Mat");CHKERRQ(ierr);
        ierr = PetscOptionsBool("-mat_product_algorithm_backend_cpu","Use CPU code","MatTransposeMatMult",usecpu,&usecpu,NULL);CHKERRQ(ierr);
        ierr = PetscOptionsEnd();CHKERRQ(ierr);
      }
      break;
    case MATPRODUCT_PtAP:
      if (product->api_user) {
        ierr = PetscOptionsBegin(PetscObjectComm((PetscObject)mat),((PetscObject)mat)->prefix,"MatPtAP","Mat");CHKERRQ(ierr);
        ierr = PetscOptionsBool("-matptap_backend_cpu","Use CPU code","MatPtAP",usecpu,&usecpu,NULL);CHKERRQ(ierr);
        ierr = PetscOptionsEnd();CHKERRQ(ierr);
      } else {
        ierr = PetscOptionsBegin(PetscObjectComm((PetscObject)mat),((PetscObject)mat)->prefix,"MatProduct_PtAP","Mat");CHKERRQ(ierr);
        ierr = PetscOptionsBool("-mat_product_algorithm_backend_cpu","Use CPU code","MatPtAP",usecpu,&usecpu,NULL);CHKERRQ(ierr);
        ierr = PetscOptionsEnd();CHKERRQ(ierr);
      }
      break;
    default:
      break;
    }
    match = (PetscBool)!usecpu;
  }
  if (match) {
    switch (product->type) {
    case MATPRODUCT_AB:
    case MATPRODUCT_AtB:
    case MATPRODUCT_PtAP:
      mat->ops->productsymbolic = MatProductSymbolic_MPIAIJKokkos;
      break;
    default:
      break;
    }
  }
  /* fallback to MPIAIJ ops */
  if (!mat->ops->productsymbolic) {
    ierr = MatProductSetFromOptions_MPIAIJ(mat);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

static PetscErrorCode MatSetPreallocationCOO_MPIAIJKokkos(Mat mat, PetscCount coo_n, const PetscInt coo_i[], const PetscInt coo_j[])
{
  Mat_MPIAIJ                *mpiaij = (Mat_MPIAIJ*)mat->data;
  Mat_MPIAIJKokkos          *mpikok;
  PetscErrorCode            ierr;

  PetscFunctionBegin;
  ierr = MatSetPreallocationCOO_MPIAIJ(mat,coo_n,coo_i,coo_j);CHKERRQ(ierr);
  mat->preallocated = PETSC_TRUE;
  ierr = MatAssemblyBegin(mat,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  ierr = MatAssemblyEnd(mat,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  ierr = MatZeroEntries(mat);CHKERRQ(ierr);
  mpikok = static_cast<Mat_MPIAIJKokkos*>(mpiaij->spptr);
  delete mpikok;
  mpiaij->spptr = new Mat_MPIAIJKokkos(mpiaij);
  PetscFunctionReturn(0);
}

static PetscErrorCode MatSetValuesCOO_MPIAIJKokkos(Mat mat,const PetscScalar v[],InsertMode imode)
{
  PetscErrorCode                 ierr;
  Mat_MPIAIJ                     *mpiaij = static_cast<Mat_MPIAIJ*>(mat->data);
  Mat_MPIAIJKokkos               *mpikok = static_cast<Mat_MPIAIJKokkos*>(mpiaij->spptr);
  Mat                            A = mpiaij->A,B = mpiaij->B;
  PetscCount                     Annz1 = mpiaij->Annz1,Annz2 = mpiaij->Annz2,Bnnz1 = mpiaij->Bnnz1,Bnnz2 = mpiaij->Bnnz2;
  MatScalarKokkosView            Aa,Ba;
  MatScalarKokkosView            v1;
  MatScalarKokkosView&           vsend = mpikok->sendbuf_d;
  const MatScalarKokkosView&     v2 = mpikok->recvbuf_d;
  const PetscCountKokkosView&    Ajmap1 = mpikok->Ajmap1_d,Ajmap2 = mpikok->Ajmap2_d,Aimap1 = mpikok->Aimap1_d,Aimap2 = mpikok->Aimap2_d;
  const PetscCountKokkosView&    Bjmap1 = mpikok->Bjmap1_d,Bjmap2 = mpikok->Bjmap2_d,Bimap1 = mpikok->Bimap1_d,Bimap2 = mpikok->Bimap2_d;
  const PetscCountKokkosView&    Aperm1 = mpikok->Aperm1_d,Aperm2 = mpikok->Aperm2_d,Bperm1 = mpikok->Bperm1_d,Bperm2 = mpikok->Bperm2_d;
  const PetscCountKokkosView&    Cperm1 = mpikok->Cperm1_d;
  PetscMemType                   memtype;

  PetscFunctionBegin;
  ierr = PetscGetMemType(v,&memtype);CHKERRQ(ierr); /* Return PETSC_MEMTYPE_HOST when v is NULL */
  if (PetscMemTypeHost(memtype)) { /* If user gave v[] in host, we need to copy it to device if any */
    v1 = Kokkos::create_mirror_view_and_copy(DefaultMemorySpace(),MatScalarKokkosViewHost((PetscScalar*)v,mpiaij->coo_n));
  } else {
    v1 = MatScalarKokkosView((PetscScalar*)v,mpiaij->coo_n); /* Directly use v[]'s memory */
  }

  if (imode == INSERT_VALUES) {
    ierr = MatSeqAIJGetKokkosViewWrite(A,&Aa);CHKERRQ(ierr); /* write matrix values */
    ierr = MatSeqAIJGetKokkosViewWrite(B,&Ba);CHKERRQ(ierr);
    Kokkos::deep_copy(Aa,0.0); /* Zero matrix values since INSERT_VALUES still requires summing replicated values in v[] */
    Kokkos::deep_copy(Ba,0.0);
  } else {
    ierr = MatSeqAIJGetKokkosView(A,&Aa);CHKERRQ(ierr); /* read & write matrix values */
    ierr = MatSeqAIJGetKokkosView(B,&Ba);CHKERRQ(ierr);
  }

  /* Pack entries to be sent to remote */
  Kokkos::parallel_for(vsend.extent(0),KOKKOS_LAMBDA(const PetscCount i) {vsend(i) = v1(Cperm1(i));});

  /* Send remote entries to their owner and overlap the communication with local computation */
  ierr = PetscSFReduceWithMemTypeBegin(mpiaij->coo_sf,MPIU_SCALAR,PETSC_MEMTYPE_KOKKOS,vsend.data(),PETSC_MEMTYPE_KOKKOS,v2.data(),MPI_REPLACE);CHKERRQ(ierr);
  /* Add local entries to A and B */
  Kokkos::parallel_for(Annz1,KOKKOS_LAMBDA(const PetscCount i) {for (PetscCount k=Ajmap1(i); k<Ajmap1(i+1); k++) Aa(Aimap1(i)) += v1(Aperm1(k));});
  Kokkos::parallel_for(Bnnz1,KOKKOS_LAMBDA(const PetscCount i) {for (PetscCount k=Bjmap1(i); k<Bjmap1(i+1); k++) Ba(Bimap1(i)) += v1(Bperm1(k));});
  ierr = PetscSFReduceEnd(mpiaij->coo_sf,MPIU_SCALAR,vsend.data(),v2.data(),MPI_REPLACE);CHKERRQ(ierr);

  /* Add received remote entries to A and B */
  Kokkos::parallel_for(Annz2,KOKKOS_LAMBDA(const PetscCount i) {for (PetscCount k=Ajmap2(i); k<Ajmap2(i+1); k++) Aa(Aimap2(i)) += v2(Aperm2(k));});
  Kokkos::parallel_for(Bnnz2,KOKKOS_LAMBDA(const PetscCount i) {for (PetscCount k=Bjmap2(i); k<Bjmap2(i+1); k++) Ba(Bimap2(i)) += v2(Bperm2(k));});

  if (imode == INSERT_VALUES) {
    ierr = MatSeqAIJRestoreKokkosViewWrite(A,&Aa);CHKERRQ(ierr); /* Increase A & B's state etc. */
    ierr = MatSeqAIJRestoreKokkosViewWrite(B,&Ba);CHKERRQ(ierr);
  } else {
    ierr = MatSeqAIJRestoreKokkosView(A,&Aa);CHKERRQ(ierr);
    ierr = MatSeqAIJRestoreKokkosView(B,&Ba);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

PetscErrorCode MatDestroy_MPIAIJKokkos(Mat A)
{
  PetscErrorCode     ierr;
  Mat_MPIAIJ         *mpiaij = (Mat_MPIAIJ*)A->data;

  PetscFunctionBegin;
  ierr = PetscObjectComposeFunction((PetscObject)A,"MatMPIAIJSetPreallocation_C",NULL);CHKERRQ(ierr);
  ierr = PetscObjectComposeFunction((PetscObject)A,"MatMPIAIJGetLocalMatMerge_C",NULL);CHKERRQ(ierr);
  ierr = PetscObjectComposeFunction((PetscObject)A,"MatSetPreallocationCOO_C",   NULL);CHKERRQ(ierr);
  ierr = PetscObjectComposeFunction((PetscObject)A,"MatSetValuesCOO_C",          NULL);CHKERRQ(ierr);
  delete (Mat_MPIAIJKokkos*)mpiaij->spptr;
  ierr = MatDestroy_MPIAIJ(A);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

PETSC_INTERN PetscErrorCode MatConvert_MPIAIJ_MPIAIJKokkos(Mat A, MatType mtype, MatReuse reuse, Mat* newmat)
{
  PetscErrorCode     ierr;
  Mat                B;
  Mat_MPIAIJ         *a;

  PetscFunctionBegin;
  if (reuse == MAT_INITIAL_MATRIX) {
    ierr = MatDuplicate(A,MAT_COPY_VALUES,newmat);CHKERRQ(ierr);
  } else if (reuse == MAT_REUSE_MATRIX) {
    ierr = MatCopy(A,*newmat,SAME_NONZERO_PATTERN);CHKERRQ(ierr);
  }
  B = *newmat;

  B->boundtocpu = PETSC_FALSE;
  ierr = PetscFree(B->defaultvectype);CHKERRQ(ierr);
  ierr = PetscStrallocpy(VECKOKKOS,&B->defaultvectype);CHKERRQ(ierr);
  ierr = PetscObjectChangeTypeName((PetscObject)B,MATMPIAIJKOKKOS);CHKERRQ(ierr);

  a = static_cast<Mat_MPIAIJ*>(A->data);
  if (a->A) {ierr = MatSetType(a->A,MATSEQAIJKOKKOS);CHKERRQ(ierr);}
  if (a->B) {ierr = MatSetType(a->B,MATSEQAIJKOKKOS);CHKERRQ(ierr);}
  if (a->lvec) {ierr = VecSetType(a->lvec,VECSEQKOKKOS);CHKERRQ(ierr);}

  B->ops->assemblyend           = MatAssemblyEnd_MPIAIJKokkos;
  B->ops->mult                  = MatMult_MPIAIJKokkos;
  B->ops->multadd               = MatMultAdd_MPIAIJKokkos;
  B->ops->multtranspose         = MatMultTranspose_MPIAIJKokkos;
  B->ops->productsetfromoptions = MatProductSetFromOptions_MPIAIJKokkos;
  B->ops->destroy               = MatDestroy_MPIAIJKokkos;

  ierr = PetscObjectComposeFunction((PetscObject)B,"MatMPIAIJSetPreallocation_C",MatMPIAIJSetPreallocation_MPIAIJKokkos);CHKERRQ(ierr);
  ierr = PetscObjectComposeFunction((PetscObject)B,"MatMPIAIJGetLocalMatMerge_C",MatMPIAIJGetLocalMatMerge_MPIAIJKokkos);CHKERRQ(ierr);
  ierr = PetscObjectComposeFunction((PetscObject)B,"MatSetPreallocationCOO_C",   MatSetPreallocationCOO_MPIAIJKokkos);CHKERRQ(ierr);
  ierr = PetscObjectComposeFunction((PetscObject)B,"MatSetValuesCOO_C",          MatSetValuesCOO_MPIAIJKokkos);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}
/*MC
   MATSMPIAIJKOKKOS - MATAIJKOKKOS = "(mpi)aijkokkos" - A matrix type to be used for sparse matrices with Kokkos

   A matrix type type using Kokkos-Kernels CrsMatrix type for portability across different device types

   Options Database Keys:
.  -mat_type aijkokkos - sets the matrix type to "aijkokkos" during a call to MatSetFromOptions()

  Level: beginner

.seealso: MatCreateAIJKokkos(), MATSEQAIJKOKKOS
M*/
PETSC_EXTERN PetscErrorCode MatCreate_MPIAIJKokkos(Mat A)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = PetscKokkosInitializeCheck();CHKERRQ(ierr);
  ierr = MatCreate_MPIAIJ(A);CHKERRQ(ierr);
  ierr = MatConvert_MPIAIJ_MPIAIJKokkos(A,MATMPIAIJKOKKOS,MAT_INPLACE_MATRIX,&A);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/*@C
   MatCreateAIJKokkos - Creates a sparse matrix in AIJ (compressed row) format
   (the default parallel PETSc format).  This matrix will ultimately pushed down
   to Kokkos for calculations. For good matrix
   assembly performance the user should preallocate the matrix storage by setting
   the parameter nz (or the array nnz).  By setting these parameters accurately,
   performance during matrix assembly can be increased by more than a factor of 50.

   Collective

   Input Parameters:
+  comm - MPI communicator, set to PETSC_COMM_SELF
.  m - number of rows
.  n - number of columns
.  nz - number of nonzeros per row (same for all rows)
-  nnz - array containing the number of nonzeros in the various rows
         (possibly different for each row) or NULL

   Output Parameter:
.  A - the matrix

   It is recommended that one use the MatCreate(), MatSetType() and/or MatSetFromOptions(),
   MatXXXXSetPreallocation() paradigm instead of this routine directly.
   [MatXXXXSetPreallocation() is, for example, MatSeqAIJSetPreallocation]

   Notes:
   If nnz is given then nz is ignored

   The AIJ format (also called the Yale sparse matrix format or
   compressed row storage), is fully compatible with standard Fortran 77
   storage.  That is, the stored row and column indices can begin at
   either one (as in Fortran) or zero.  See the users' manual for details.

   Specify the preallocated storage with either nz or nnz (not both).
   Set nz=PETSC_DEFAULT and nnz=NULL for PETSc to control dynamic memory
   allocation.  For large problems you MUST preallocate memory or you
   will get TERRIBLE performance, see the users' manual chapter on matrices.

   By default, this format uses inodes (identical nodes) when possible, to
   improve numerical efficiency of matrix-vector products and solves. We
   search for consecutive rows with the same nonzero structure, thereby
   reusing matrix information to achieve increased efficiency.

   Level: intermediate

.seealso: MatCreate(), MatCreateAIJ(), MatSetValues(), MatSeqAIJSetColumnIndices(), MatCreateSeqAIJWithArrays(), MatCreateAIJ(), MATMPIAIJKOKKOS, MATAIJKokkos
@*/
PetscErrorCode  MatCreateAIJKokkos(MPI_Comm comm,PetscInt m,PetscInt n,PetscInt M,PetscInt N,PetscInt d_nz,const PetscInt d_nnz[],PetscInt o_nz,const PetscInt o_nnz[],Mat *A)
{
  PetscErrorCode ierr;
  PetscMPIInt    size;

  PetscFunctionBegin;
  ierr = MatCreate(comm,A);CHKERRQ(ierr);
  ierr = MatSetSizes(*A,m,n,M,N);CHKERRQ(ierr);
  ierr = MPI_Comm_size(comm,&size);CHKERRMPI(ierr);
  if (size > 1) {
    ierr = MatSetType(*A,MATMPIAIJKOKKOS);CHKERRQ(ierr);
    ierr = MatMPIAIJSetPreallocation(*A,d_nz,d_nnz,o_nz,o_nnz);CHKERRQ(ierr);
  } else {
    ierr = MatSetType(*A,MATSEQAIJKOKKOS);CHKERRQ(ierr);
    ierr = MatSeqAIJSetPreallocation(*A,d_nz,d_nnz);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

// get GPU pointer to stripped down Mat. For both Seq and MPI Mat.
PetscErrorCode MatKokkosGetDeviceMatWrite(Mat A, PetscSplitCSRDataStructure *B)
{
  PetscMPIInt                size,rank;
  MPI_Comm                   comm;
  PetscErrorCode             ierr;
  PetscSplitCSRDataStructure d_mat=NULL;

  PetscFunctionBegin;
  ierr = PetscObjectGetComm((PetscObject)A,&comm);CHKERRQ(ierr);
  ierr = MPI_Comm_size(comm,&size);CHKERRMPI(ierr);
  ierr = MPI_Comm_rank(comm,&rank);CHKERRMPI(ierr);
  if (size == 1) {
    ierr   = MatSeqAIJKokkosGetDeviceMat(A,&d_mat);CHKERRQ(ierr);
    ierr   = MatSeqAIJKokkosModifyDevice(A);CHKERRQ(ierr); /* Since we are going to modify matrix values on device */
  } else {
    Mat_MPIAIJ  *aij = (Mat_MPIAIJ*)A->data;
    ierr   = MatSeqAIJKokkosGetDeviceMat(aij->A,&d_mat);CHKERRQ(ierr);
    ierr   = MatSeqAIJKokkosModifyDevice(aij->A);CHKERRQ(ierr);
    ierr   = MatSeqAIJKokkosModifyDevice(aij->B);CHKERRQ(ierr);
    PetscCheck(A->nooffprocentries || aij->donotstash,PetscObjectComm((PetscObject)A),PETSC_ERR_SUP,"Device assembly does not currently support offproc values insertion. Use MatSetOption(A,MAT_NO_OFF_PROC_ENTRIES,PETSC_TRUE) or MatSetOption(A,MAT_IGNORE_OFF_PROC_ENTRIES,PETSC_TRUE)");
  }
  // act like MatSetValues because not called on host
  if (A->assembled) {
    if (A->was_assembled) {
      ierr = PetscInfo(A,"Assemble more than once already\n");CHKERRQ(ierr);
    }
    A->was_assembled = PETSC_TRUE; // this is done (lazy) in MatAssemble but we are not calling it anymore - done in AIJ AssemblyEnd, need here?
  } else {
    ierr = PetscInfo(A,"Warning !assemble ??? assembled=%" PetscInt_FMT "\n",A->assembled);CHKERRQ(ierr);
  }
  if (!d_mat) {
    struct _n_SplitCSRMat h_mat; /* host container */
    Mat_SeqAIJKokkos      *aijkokA;
    Mat_SeqAIJ            *jaca;
    PetscInt              n = A->rmap->n, nnz;
    Mat                   Amat;
    PetscInt              *colmap;

    /* create and copy h_mat */
    h_mat.M = A->cmap->N; // use for debug build
    ierr = PetscInfo(A,"Create device matrix in Kokkos\n");CHKERRQ(ierr);
    if (size == 1) {
      Amat = A;
      jaca = (Mat_SeqAIJ*)A->data;
      h_mat.rstart = 0; h_mat.rend = A->rmap->n;
      h_mat.cstart = 0; h_mat.cend = A->cmap->n;
      h_mat.offdiag.i = h_mat.offdiag.j = NULL;
      h_mat.offdiag.a = NULL;
      aijkokA = static_cast<Mat_SeqAIJKokkos*>(A->spptr);
    } else {
      Mat_MPIAIJ       *aij = (Mat_MPIAIJ*)A->data;
      Mat_SeqAIJ       *jacb = (Mat_SeqAIJ*)aij->B->data;
      PetscInt         ii;
      Mat_SeqAIJKokkos *aijkokB;

      Amat = aij->A;
      aijkokA = static_cast<Mat_SeqAIJKokkos*>(aij->A->spptr);
      aijkokB = static_cast<Mat_SeqAIJKokkos*>(aij->B->spptr);
      jaca = (Mat_SeqAIJ*)aij->A->data;
      PetscCheckFalse(aij->B->cmap->n && !aij->garray,comm,PETSC_ERR_PLIB,"MPIAIJ Matrix was assembled but is missing garray");
      PetscCheckFalse(aij->B->rmap->n != aij->A->rmap->n,comm,PETSC_ERR_SUP,"Only support aij->B->rmap->n == aij->A->rmap->n");
      aij->donotstash = PETSC_TRUE;
      aij->A->nooffprocentries = aij->B->nooffprocentries = A->nooffprocentries = PETSC_TRUE;
      jaca->nonew = jacb->nonew = PETSC_TRUE; // no more disassembly
      ierr = PetscCalloc1(A->cmap->N,&colmap);CHKERRQ(ierr);
      ierr = PetscLogObjectMemory((PetscObject)A,(A->cmap->N)*sizeof(PetscInt));CHKERRQ(ierr);
      for (ii=0; ii<aij->B->cmap->n; ii++) colmap[aij->garray[ii]] = ii+1;
      // allocate B copy data
      h_mat.rstart = A->rmap->rstart; h_mat.rend = A->rmap->rend;
      h_mat.cstart = A->cmap->rstart; h_mat.cend = A->cmap->rend;
      nnz = jacb->i[n];
      if (jacb->compressedrow.use) {
        const Kokkos::View<PetscInt*, Kokkos::HostSpace, Kokkos::MemoryTraits<Kokkos::Unmanaged> > h_i_k (jacb->i,n+1);
        aijkokB->i_uncompressed_d = Kokkos::View<PetscInt*>(Kokkos::create_mirror(DefaultMemorySpace(),h_i_k));
        Kokkos::deep_copy (aijkokB->i_uncompressed_d, h_i_k);
        h_mat.offdiag.i = aijkokB->i_uncompressed_d.data();
      } else {
         h_mat.offdiag.i = aijkokB->i_device_data();
      }
      h_mat.offdiag.j = aijkokB->j_device_data();
      h_mat.offdiag.a = aijkokB->a_device_data();
      {
        Kokkos::View<PetscInt*, Kokkos::HostSpace, Kokkos::MemoryTraits<Kokkos::Unmanaged> > h_colmap_k (colmap,A->cmap->N);
        aijkokB->colmap_d = Kokkos::View<PetscInt*>(Kokkos::create_mirror(DefaultMemorySpace(),h_colmap_k));
        Kokkos::deep_copy (aijkokB->colmap_d, h_colmap_k);
        h_mat.colmap = aijkokB->colmap_d.data();
        ierr = PetscFree(colmap);CHKERRQ(ierr);
      }
      h_mat.offdiag.ignorezeroentries = jacb->ignorezeroentries;
      h_mat.offdiag.n = n;
    }
    // allocate A copy data
    nnz = jaca->i[n];
    h_mat.diag.n = n;
    h_mat.diag.ignorezeroentries = jaca->ignorezeroentries;
    ierr = MPI_Comm_rank(comm,&h_mat.rank);CHKERRMPI(ierr);
    PetscCheckFalse(jaca->compressedrow.use,PETSC_COMM_SELF,PETSC_ERR_PLIB,"A does not suppport compressed row (todo)");
    else {
      h_mat.diag.i = aijkokA->i_device_data();
    }
    h_mat.diag.j = aijkokA->j_device_data();
    h_mat.diag.a = aijkokA->a_device_data();
    // copy pointers and metdata to device
    ierr = MatSeqAIJKokkosSetDeviceMat(Amat,&h_mat);CHKERRQ(ierr);
    ierr = MatSeqAIJKokkosGetDeviceMat(Amat,&d_mat);CHKERRQ(ierr);
    ierr = PetscInfo(A,"Create device Mat n=%" PetscInt_FMT " nnz=%" PetscInt_FMT "\n",h_mat.diag.n, nnz);CHKERRQ(ierr);
  }
  *B = d_mat; // return it, set it in Mat, and set it up
  A->assembled = PETSC_FALSE; // ready to write with matsetvalues - this done (lazy) in normal MatSetValues
  PetscFunctionReturn(0);
}

PETSC_INTERN PetscErrorCode MatSeqAIJKokkosGetOffloadMask(Mat A, const char **mask)
{
  Mat_SeqAIJKokkos  *aijkok = static_cast<Mat_SeqAIJKokkos*>(A->spptr);

  PetscFunctionBegin;
  if (!aijkok) *mask = "AIJKOK_UNALLOCATED";
  else if (aijkok->a_dual.need_sync_host()) *mask = "PETSC_OFFLOAD_GPU";
  else if (aijkok->a_dual.need_sync_device()) *mask = "PETSC_OFFLOAD_CPU";
  else *mask = "PETSC_OFFLOAD_BOTH";
  PetscFunctionReturn(0);
}

PETSC_INTERN PetscErrorCode MatAIJKokkosPrintOffloadMask(Mat A)
{
  PetscErrorCode    ierr;
  PetscMPIInt       size;
  Mat               Ad,Ao;
  const char        *amask,*bmask;

  PetscFunctionBegin;
  ierr = MPI_Comm_size(PetscObjectComm((PetscObject)A),&size);CHKERRMPI(ierr);

  if (size == 1) {
    ierr = MatSeqAIJKokkosGetOffloadMask(A,&amask);CHKERRQ(ierr);
    ierr = PetscPrintf(PETSC_COMM_SELF,"%s\n",amask);CHKERRQ(ierr);
  } else {
    Ad  = ((Mat_MPIAIJ*)A->data)->A;
    Ao  = ((Mat_MPIAIJ*)A->data)->B;
    ierr = MatSeqAIJKokkosGetOffloadMask(Ad,&amask);CHKERRQ(ierr);
    ierr = MatSeqAIJKokkosGetOffloadMask(Ao,&bmask);CHKERRQ(ierr);
    ierr = PetscPrintf(PETSC_COMM_SELF,"Diag : Off-diag = %s : %s\n",amask,bmask);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}
