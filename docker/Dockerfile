

from vnv_hypre:latest

COPY . ./petsc 
RUN  cd petsc && ./configure --prefix=~/software/petsc --with-hypre-dir=${HYPRE_DIR} --with-vnv-dir=${VNV_DIR} --with-debugging=no --with-shared-libraries=1 --download-fblaslapack=1 --download-metis=1 --download-ptscotch=1 --download-parmetis=1 --download-superlu_dist=1 --download-mumps=1 --download-strumpack=1 --download-scalapack=1 --download-slepc=1 --with-mpi=1 --with-cxx-dialect=C++11 --with-fortran-bindings=0 --with-sowing=0 --with-64-bit-indices && bear make &&  ${VNV_MATCHER} --package PETSC --extension c --output src/sys/vnv/vnv compile_commands.json && make install

ENV PETSC_DIR=~/software/petsc
ENV PETSC_ARCH=
 
