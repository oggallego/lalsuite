#!/usr/bin/env bash

set -e
TMPDIR=/tmp/rachel

CFLAGS="-O3 -march=native"

STARTDIR=${PWD}

mkdir -p $TMPDIR

if [ $(basename $PWD) != "lalsuite" ];
then
		cd lalsuite
fi

BRANCH=$(git rev-parse --abbrev-ref HEAD)
#INSTDIR=${HOME}/opt/lalsuites/${BRANCH}
INSTDIR=${CONDA_PREFIX}

echo "Installing branch $BRANCH in $INSTDIR"
if [ -z "$PYTHON" ];
then
	export PYTHON="$(which python)"
fi
echo "Python interpreter: $PYTHON"
./00boot
./configure --prefix=$INSTDIR --enable-swig-python --enable-openmp --enable-mpi
make -j install

source $INSTDIR/etc/lalsuiterc

#cd ../pylal/
#python setup.py install --prefix=$INSTDIR
#cd ../glue
#python setup.py install --prefix=$INSTDIR
cd $STARTDIR

