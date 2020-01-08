# How to:
#
# 1) If both dependencies, Intel MKL and GSL, are installed in their
# default location, the user defined potential energy surface (PES)
# is pre-compiled and named pot.o (default), then type
#
# make
#
# to build and link all codes using GNU gcc compiler (default). Notice,
# the environment variable MKLROOT must be properly defined before make.
#
# 2) If Intel icc compiler is the C compiler of choice instead, type
#
# make CC=icc
#
# 3) If MPI functionalities are to be included in the build, type
#
# make CC=icc with_mpi
#
# or, "make with_mpi" to use the default GNU compiler.
#
# 4) If the user defined PES happens to have a different filename, then
# type
#
# make CC=icc PES_LIB=[filename.o]
#
# or, "make PES_LIB=[filename.o]". And, if the PES also depends on another
# set of object files,
#
# make PES_LIB="[filename.o] [dependence_a.o] [dependence_b.o] [etc.o]"
#
# 5) If the GSL library is not installed in the default directory, type
#
# make GSL_INC=[path]/include GSL_LIB=[path]/bin/libgsl.a
#
# where, [path] points the location where the library is installed. As
# usual the include directory with all header files and the library as
# an .a binary file are needed. Often when working at machines in which
# no root permissions are available one is forced to install GSL
# somewhere else or asking the administrator to install it.
#
# 7) For fresh builds type
#
# make clean
#
# In particular a good practice would be to run "make clean" between the
# building process of routines with and without MPI.
#
# Humberto Jr
# Dec, 2018
#

SHELL = /bin/sh
LINEAR_ALGEBRA = GSL
GSLROOT = /usr/local

#
# GNU gcc compiler:
#

CC = gcc
CFLAGS = -W -Wall -std=c99 -pedantic -fopenmp -O3 -I$(GSLROOT)/include
LDFLAGS = -L$(GSLROOT)/lib -lgsl -lgslcblas -lm

ifeq ($(CC), gcc)
	CFLAGS = -W -Wall -std=c99 -pedantic -fopenmp -O3 -I$(GSLROOT)/include
	LDFLAGS = -L$(GSLROOT)/lib -lgsl -lgslcblas -lm
	FORT_LIB = -lgfortran
endif

#
# clang compiler:
#

ifeq ($(CC), clang)
	CFLAGS = -W -Wall -std=c99 -pedantic -fopenmp -O3 -I$(GSLROOT)/include
	LDFLAGS = -L$(GSLROOT)/lib -lgsl -lgslcblas -lm
	FORT_LIB = -lgfortran
endif

#
# Intel icc compiler:
#

ifeq ($(CC), icc)
	CFLAGS = -W -Wall -std=c99 -pedantic -fopenmp -O3 -I$(GSLROOT)/include
	LDFLAGS = -L$(GSLROOT)/lib -lgsl -lgslcblas -lm
	FORT_LIB = -lgfortran
endif

#
# (GNU or Intel) mpicc:
#

ifeq ($(CC), mpicc)
	CFLAGS = -W -Wall -std=c99 -pedantic -fopenmp -O3 -DUSE_MPI -I$(GSLROOT)/include
	LDFLAGS = -L$(GSLROOT)/lib -lgsl -lgslcblas -lm
	FORT_LIB = -lgfortran
endif

#
# IBM xlc compiler:
#

XLFROOT = /opt/ibmcmp/xlf/bg/14.1

ifeq ($(CC), xlc)
	CFLAGS = -std=c99 -q64 -qstrict -qsmp=omp -qthreaded -O5
	FORT_LIB = -lxlf90_r -lxl -lxlfmath -L$(XLFROOT)/bglib64
endif

#
# IBM xlc_r compiler:
#

ifeq ($(CC), xlc_r)
	CFLAGS = -std=c99 -q64 -qstrict -qsmp=omp -qthreaded -O5
	FORT_LIB = -lxlf90_r -lxl -lxlfmath -L$(XLFROOT)/bglib64
endif

#
# IBM mpixlc compiler:
#

ifeq ($(CC), mpixlc)
	CFLAGS = -std=c99 -q64 -qstrict -qsmp=omp -qthreaded -O5 -DUSE_MPI
	FORT_LIB = -lxlf90_r -lxl -lxlfmath -L$(XLFROOT)/bglib64
endif

#
# IBM mpixlc_r compiler:
#

ifeq ($(CC), mpixlc_r)
	CFLAGS = -std=c99 -q64 -qstrict -qsmp=omp -qthreaded -O5 -DUSE_MPI
	FORT_LIB = -lxlf90_r -lxl -lxlfmath -L$(XLFROOT)/bglib64
endif

#
# User defined potential energy surface (PES):
#

PES_NAME =
PES_OBJECT =

ifeq ($(PES_NAME), )
	PES_MACRO =
else
	ifeq ($(PES_OBJECT), )
		PES_OBJECT = $(PES_NAME).o
	endif

	PES_MACRO = -DEXTERNAL_PES_NAME=$(PES_NAME)
endif

#
# Intel MKL (for GNU gcc or Intel icc compilers):
#

ifeq ($(LINEAR_ALGEBRA), MKL)
	LINEAR_ALGEBRA_INC = -DMKL_ILP64 -m64 -I$(MKLROOT)/include -DUSE_MKL

	ifeq ($(CC), icc)
		LINEAR_ALGEBRA_LIB = -parallel -Wl,--start-group $(MKLROOT)/lib/intel64/libmkl_intel_ilp64.a $(MKLROOT)/lib/intel64/libmkl_intel_thread.a $(MKLROOT)/lib/intel64/libmkl_core.a -Wl,--end-group -liomp5 -lpthread -lm -ldl
	else
		LINEAR_ALGEBRA_LIB = -Wl,--start-group $(MKLROOT)/lib/intel64/libmkl_intel_ilp64.a $(MKLROOT)/lib/intel64/libmkl_gnu_thread.a $(MKLROOT)/lib/intel64/libmkl_core.a -Wl,--end-group -lgomp -lpthread -lm -ldl
	endif
endif

#
# LAPACKE library:
#

LAPACKEROOT = /usr/local
LAPACKROOT = /usr/local
BLASROOT = /usr/local

ifeq ($(LINEAR_ALGEBRA), LAPACKE)
	LINEAR_ALGEBRA_INC = -I$(LAPACKEROOT)/include -DUSE_LAPACKE
	LINEAR_ALGEBRA_LIB = -L$(LAPACKEROOT)/lib -llapacke -llapack -lblas -lgfortran -lm
endif

#
# IBM ESSL (for Blue Gene machines):
# TODO: XLFROOT is IDRIS dependent!
#

ESSLROOT = /usr

ifeq ($(LINEAR_ALGEBRA), ESSL)
	LINEAR_ALGEBRA_INC = -I$(ESSLROOT)/include -DUSE_ESSL
	LINEAR_ALGEBRA_LIB = -L$(ESSLROOT)/lib64 -lesslbg -lm
endif

#
# MAGMA library:
#

MAGMAROOT = /usr/local/magma
CUDAROOT = /usr/local/cuda

ifeq ($(LINEAR_ALGEBRA), MAGMA)
	LINEAR_ALGEBRA_INC = -I$(CUDAROOT)/include -I$(MAGMAROOT)/include -DUSE_MAGMA -DADD_
	LINEAR_ALGEBRA_LIB = -L$(MAGMAROOT)/lib -lmagma -lm
endif

#
# ATLAS library:
#

ATLASROOT = /usr/local/atlas

ifeq ($(LINEAR_ALGEBRA), ATLAS)
	LINEAR_ALGEBRA_INC = -I$(ATLASROOT)/include -DUSE_ATLAS
	LINEAR_ALGEBRA_LIB = -L$(ATLASROOT)/lib -latlas -lptcblas -lm
endif

#
# scatt_utils module + dependencies:
#

SCATT_UTILS_LIB = scatt_utils.o $(PES_LIB) $(GSL_LIB) $(LINEAR_ALGEBRA_LIB) $(FORT_LIB)

#
# Extra macros, if any, in order to tune the building:
#

USE_MACRO = DUMMY_MACRO

#
# General rules:
#

all: modules drivers
modules: matrix tools nist johnson pes mass coor dvr file miller network
drivers: dbasis about

#
# Rules for modules:
#

MODULES_DIR = modules

matrix: $(MODULES_DIR)/matrix.c $(MODULES_DIR)/matrix.h $(MODULES_DIR)/globals.h
	@echo "\033[31m$<\033[0m"
	$(CC) $(CFLAGS) $(LINEAR_ALGEBRA_INC) -D$(USE_MACRO) -c $<
	@echo

tools: $(MODULES_DIR)/tools.c $(MODULES_DIR)/tools.h $(MODULES_DIR)/globals.h
	@echo "\033[31m$<\033[0m"
	$(CC) $(CFLAGS) -D$(USE_MACRO) -c $<
	@echo

nist: $(MODULES_DIR)/nist.c $(MODULES_DIR)/nist.h $(MODULES_DIR)/globals.h
	@echo "\033[31m$<\033[0m"
	$(CC) $(CFLAGS) -c $<
	@echo

johnson: $(MODULES_DIR)/johnson.c $(MODULES_DIR)/johnson.h $(MODULES_DIR)/matrix.h $(MODULES_DIR)/globals.h
	@echo "\033[31m$<\033[0m"
	$(CC) $(CFLAGS) -c $<
	@echo

pes: $(MODULES_DIR)/pes.c $(MODULES_DIR)/pes.h $(MODULES_DIR)/globals.h
	@echo "\033[31m$<\033[0m"
	$(CC) $(CFLAGS) $(PES_MACRO) -c $<
	@echo

mass: $(MODULES_DIR)/mass.c $(MODULES_DIR)/mass.h $(MODULES_DIR)/globals.h $(MODULES_DIR)/tools.h $(MODULES_DIR)/nist.h
	@echo "\033[31m$<\033[0m"
	$(CC) $(CFLAGS) -c $<
	@echo

coor: $(MODULES_DIR)/coor.c $(MODULES_DIR)/coor.h $(MODULES_DIR)/globals.h $(MODULES_DIR)/mass.h
	@echo "\033[31m$<\033[0m"
	$(CC) $(CFLAGS) -c $<
	@echo

dvr: $(MODULES_DIR)/dvr.c $(MODULES_DIR)/dvr.h $(MODULES_DIR)/globals.h $(MODULES_DIR)/matrix.h
	@echo "\033[31m$<\033[0m"
	$(CC) $(CFLAGS) -c $<
	@echo

file: $(MODULES_DIR)/file.c $(MODULES_DIR)/file.h $(MODULES_DIR)/globals.h
	@echo "\033[31m$<\033[0m"
	$(CC) $(CFLAGS) -c $<
	@echo

phys: $(MODULES_DIR)/phys.c $(MODULES_DIR)/phys.h $(MODULES_DIR)/globals.h
	@echo "\033[31m$<\033[0m"
	$(CC) $(CFLAGS) -c $<
	@echo

miller: $(MODULES_DIR)/clib.h $(MODULES_DIR)/miller.c $(MODULES_DIR)/miller.h $(MODULES_DIR)/globals.h $(MODULES_DIR)/pes.h $(MODULES_DIR)/dvr.h $(MODULES_DIR)/coor.h $(MODULES_DIR)/mass.h $(MODULES_DIR)/matrix.h
	@echo "\033[31m$<\033[0m"
	$(CC) $(CFLAGS) -c $<
	@echo

#network: $(MODULES_DIR)/network.c $(MODULES_DIR)/network.h $(MODULES_DIR)/globals.h $(MODULES_DIR)/matrix.h
#	@echo "\033[31m$<\033[0m"
#	$(CC) $(CFLAGS) -c $<
#	@ech

#
# Rules for drivers:
#

dbasis: dbasis.c $(MODULES_DIR)/globals.h $(MODULES_DIR)/matrix.h $(MODULES_DIR)/file.h $(MODULES_DIR)/mass.h $(MODULES_DIR)/dvr.h $(MODULES_DIR)/pes.h $(PES_OBJECT)
	@echo "\033[31m$<\033[0m"
	$(CC) $(CFLAGS) -D$(USE_MACRO) $< -o $@.out matrix.o file.o mass.o dvr.o pes.o nist.o coor.o $(PES_OBJECT) $(LDFLAGS) $(LINEAR_ALGEBRA_LIB) $(FORT_LIB)
	@echo

about: about.c $(MODULES_DIR)/matrix.h $(MODULES_DIR)/pes.h matrix.o pes.o $(PES_OBJECT)
	@echo "\033[31m$<\033[0m"
	$(CC) $(CFLAGS) -D$(USE_MACRO) $< -o $@.out matrix.o pes.o $(PES_OBJECT) $(LDFLAGS) $(LINEAR_ALGEBRA_LIB) $(FORT_LIB)
	@echo

network: network.c $(MODULES_DIR)/globals.h $(MODULES_DIR)/tools.h $(MODULES_DIR)/matrix.h matrix.o
	@echo "\033[31m$<\033[0m"
	$(CC) $(CFLAGS) $< -o $@.out matrix.o tools.o $(LDFLAGS) $(LINEAR_ALGEBRA_LIB)
	@echo

test_suit: test_suit.c $(MODULES_DIR)/matrix.h
	@echo "\033[31m$<\033[0m"
	$(CC) $(CFLAGS) -D$(USE_MACRO) $< -o $@.out matrix.o file.o $(LDFLAGS) $(LINEAR_ALGEBRA_LIB) $(FORT_LIB)
	@echo

#
# Rules to build/install of external libraries (using default locations):
#

LIB_DIR = lib

gsl: $(LIB_DIR)/gsl-2.5.tar.gz
	tar -zxvf $<
	cd gsl-2.5/; ./configure; make; make install
	rm -rf gsl-2.5/

lapacke: $(LIB_DIR)/lapack-3.5.0.tar
	tar -xvf $<
	cd lapack-3.5.0/; cp make.inc.example make.inc
	cd lapack-3.5.0/BLAS/SRC; make
	cd lapack-3.5.0; make; make lapackelib
	cd lapack-3.5.0; mv librefblas.a libblas.a; cp lib*.a $(LAPACKEROOT)/lib/; cp lapacke/include/*.h $(LAPACKEROOT)/include/
	rm -rf lapack-3.5.0/

clean:
	rm -f *.o
