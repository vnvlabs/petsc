#ifdef PETSC_RCS_HEADER
"$Id: petscconf.h,v 1.18 2000/11/28 17:26:31 bsmith Exp bsmith $"
"Defines the configuration for this machine"
#endif

#if !defined(INCLUDED_PETSCCONF_H)
#define INCLUDED_PETSCCONF_H

#define PARCH_solaris 
#define PETSC_ARCH_NAME "solaris"

#define PETSC_HAVE_POPEN
#define PETSC_HAVE_FORTRAN_UNDERSCORE 
#define PETSC_HAVE_SYS_WAIT_H 
#define STDC_HEADERS 
#define PETSC_HAVE_DRAND48 
#define PETSC_HAVE_GETCWD 
#define PETSC_HAVE_GETDOMAINNAME 
#define PETSC_HAVE_REALPATH 
#define PETSC_HAVE_SIGACTION 
#define PETSC_HAVE_SIGNAL 
#define PETSC_HAVE_SIGSET 
#define PETSC_HAVE_STRSTR 
#define PETSC_HAVE_SYSINFO 
#define PETSC_HAVE_UNAME
#define PETSC_HAVE_FCNTL_H 
#define PETSC_HAVE_LIMITS_H 
#define PETSC_HAVE_MALLOC_H 
#define PETSC_HAVE_PWD_H 
#define PETSC_HAVE_SEARCH_H 
#define PETSC_HAVE_STDLIB_H 
#define PETSC_HAVE_STRING_H 
#define PETSC_HAVE_STRINGS_H 
#define PETSC_HAVE_STROPTS_H 
#define PETSC_HAVE_UNISTD_H 
#define PETSC_HAVE_SYS_TIME_H 
#define PETSC_HAVE_SYS_SYSTEMINFO_H 

#define PETSC_HAVE_FORTRAN_UNDERSCORE
#define PETSC_HAVE_FORTRAN_UNDERSCORE_UNDERSCORE

#define PETSC_HAVE_READLINK
#define PETSC_HAVE_MEMMOVE

#define PETSC_HAVE_MEMALIGN
#define PETSC_HAVE_SYS_RESOURCE_H

#define PETSC_HAVE_SYS_PROCFS_H
#define PETSC_USE_PROCFS_FOR_SIZE
#define PETSC_HAVE_FCNTL_H
#define PETSC_SIZEOF_VOIDP 4
#define PETSC_SIZEOF_INT 4
#define PETSC_SIZEOF_DOUBLE 8

#define PETSC_WORDS_BIGENDIAN 1
#define PETSC_USE_DYNAMIC_LIBRARIES 1

#define PETSC_HAVE_RTLD_GLOBAL 1

#define PETSC_HAVE_TEMPLATED_COMPLEX
#define PETSC_HAVE_TIMES

#define PETSC_HAVE_UCBPS

#endif
