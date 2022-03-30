ARG FROM_IMAGE=vnv_hypre
FROM ${FROM_IMAGE}:latest



#Install PETSC VNV
COPY . ./petsc
RUN ls /root/software/vnv/bin/ 
RUN  cd petsc && ./configure --prefix=/root/software/petsc --with-hypre-dir=${HYPRE_DIR} --with-vnv-dir=${VNV_DIR} --with-debugging=no --with-shared-libraries=1 --download-fblaslapack=1 --download-metis=1 --download-ptscotch=1 --download-parmetis=1 --download-superlu_dist=1 --download-mumps=1 --download-strumpack=1 --download-scalapack=1 --download-slepc=1 --with-mpi=1 --with-cxx-dialect=C++11 --with-fortran-bindings=0 --with-sowing=0 --with-64-bit-indices 
RUN  cd petsc && bear make &&  ${VNV_MATCHER} --package PETSC --output src/sys/vnv/vnv_PETSC.c --cache src/sys/vnv/vnv.__cache__ compile_commands.json && make install
ENV PETSC_DIR=/root/software/petsc
ENV PETSC_ARCH=

ARG COMMIT_HASH=unknown
LABEL vnv.version.petsc=${COMMIT_HASH}

ENV VNV_CONFIG=${VNV_CONFIG}:/petsc/.vnv-config.json



