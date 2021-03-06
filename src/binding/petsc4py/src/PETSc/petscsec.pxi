# --------------------------------------------------------------------

cdef extern from * nogil:

    int PetscSectionCreate(MPI_Comm,PetscSection*)
    int PetscSectionClone(PetscSection,PetscSection*)
    int PetscSectionSetUp(PetscSection)
    int PetscSectionSetUpBC(PetscSection)
    int PetscSectionView(PetscSection,PetscViewer)
    int PetscSectionReset(PetscSection)
    int PetscSectionDestroy(PetscSection*)

    int PetscSectionGetNumFields(PetscSection,PetscInt*)
    int PetscSectionSetNumFields(PetscSection,PetscInt)
    int PetscSectionGetFieldName(PetscSection,PetscInt,const char*[])
    int PetscSectionSetFieldName(PetscSection,PetscInt,const char[])
    int PetscSectionGetFieldComponents(PetscSection,PetscInt,PetscInt*)
    int PetscSectionSetFieldComponents(PetscSection,PetscInt,PetscInt)
    int PetscSectionGetChart(PetscSection,PetscInt*,PetscInt*)
    int PetscSectionSetChart(PetscSection,PetscInt,PetscInt)
    int PetscSectionGetPermutation(PetscSection,PetscIS*)
    int PetscSectionSetPermutation(PetscSection,PetscIS)
    int PetscSectionGetDof(PetscSection,PetscInt,PetscInt*)
    int PetscSectionSetDof(PetscSection,PetscInt,PetscInt)
    int PetscSectionAddDof(PetscSection,PetscInt,PetscInt)
    int PetscSectionGetFieldDof(PetscSection,PetscInt,PetscInt,PetscInt*)
    int PetscSectionSetFieldDof(PetscSection,PetscInt,PetscInt,PetscInt)
    int PetscSectionAddFieldDof(PetscSection,PetscInt,PetscInt,PetscInt)
    int PetscSectionGetConstraintDof(PetscSection,PetscInt,PetscInt*)
    int PetscSectionSetConstraintDof(PetscSection,PetscInt,PetscInt)
    int PetscSectionAddConstraintDof(PetscSection,PetscInt,PetscInt)
    int PetscSectionGetFieldConstraintDof(PetscSection,PetscInt,PetscInt,PetscInt*)
    int PetscSectionSetFieldConstraintDof(PetscSection,PetscInt,PetscInt,PetscInt)
    int PetscSectionAddFieldConstraintDof(PetscSection,PetscInt,PetscInt,PetscInt)
    int PetscSectionGetConstraintIndices(PetscSection,PetscInt,const PetscInt**)
    int PetscSectionSetConstraintIndices(PetscSection,PetscInt,const PetscInt*)
    int PetscSectionGetFieldConstraintIndices(PetscSection,PetscInt,PetscInt,const PetscInt**)
    int PetscSectionSetFieldConstraintIndices(PetscSection,PetscInt,PetscInt,const PetscInt*)
    int PetscSectionGetMaxDof(PetscSection,PetscInt*)
    int PetscSectionGetStorageSize(PetscSection,PetscInt*)
    int PetscSectionGetConstrainedStorageSize(PetscSection,PetscInt*)
    int PetscSectionGetOffset(PetscSection,PetscInt,PetscInt*)
    int PetscSectionSetOffset(PetscSection,PetscInt,PetscInt)
    int PetscSectionGetFieldOffset(PetscSection,PetscInt,PetscInt,PetscInt*)
    int PetscSectionSetFieldOffset(PetscSection,PetscInt,PetscInt,PetscInt)
    int PetscSectionGetOffsetRange(PetscSection,PetscInt*,PetscInt*)
    int PetscSectionCreateGlobalSection(PetscSection,PetscSF,PetscBool,PetscBool,PetscSection*)
    #int PetscSectionCreateGlobalSectionCensored(PetscSection,PetscSF,PetscBool,PetscInt,const PetscInt[],PetscSection*)
    int PetscSectionCreateSubsection(PetscSection,PetscInt,PetscInt[],PetscSection*)
    int PetscSectionCreateSubmeshSection(PetscSection,IS,PetscSection*)
    #int PetscSectionGetPointLayout(MPI_Comm,PetscSection,PetscLayout*)
    #int PetscSectionGetValueLayout(MPI_Comm,PetscSection,PetscLayout*)
