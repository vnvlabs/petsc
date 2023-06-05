ARG FROM_IMAGE=vnv_hypre
FROM ${FROM_IMAGE}

ARG COMMIT_HASH=unknown
LABEL vnv.version.petsc=${COMMIT_HASH}


#Install PETSC VNV
COPY . ${SOURCE_DIR}/petsc

RUN  cd ${SOURCE_DIR}/petsc \
     && ./configure --prefix=${SOFTWARE_DIR}/petsc \
                    --with-hypre-dir=${HYPRE_DIR}\
                    --with-vnv-dir=${VNV_DIR}\
                    --with-debugging=no\
                    --with-shared-libraries=1\
                    --download-fblaslapack=1\
                    --download-metis=1\
                    --download-ptscotch=1\
                    --download-parmetis=1\
                    --download-superlu_dist=1\
                    --download-mumps=1\
                    --download-strumpack=1\
                    --download-scalapack=1\
                    --download-slepc=1 \
                    --with-mpi=1 \
                    --with-cxx-dialect=C++11 \
                    --with-fortran-bindings=0\
                    --with-sowing=0 \
                    --with-64-bit-indices \
     && bear make \
     &&  ${VNV_MATCHER} --package PETSC \
                        --output src/sys/vnv/vnv_PETSC.c \
                        --cache src/sys/vnv/vnv.__cache__ \
                        --fix-omp \
			compile_commands.json \
     && make \
     && make install \
     && cp -r ${SOURCE_DIR}/petsc/vnv ${SOFTWARE_DIR}/petsc/examples  \
     && cp ${SOFTWARE_DIR}/petsc/examples && make ex200 && make ex201 && make ex202 \
     && rm -r ${SOURCE_DIR}/petsc
     
ENV PETSC_DIR=${SOFTWARE_DIR}/petsc
ENV PETSC_ARCH=
ENV VNV_CONFIG=${VNV_CONFIG}:${SOURCE_DIR}/petsc/vnv.__registration__
ENV VNV_CONFIG=${VNV_CONFIG}:${SOURCE_DIR}/petsc/docker/config/vnv.__registration__


