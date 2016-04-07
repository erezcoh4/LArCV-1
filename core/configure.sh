#!/bin/bash

# clean up previously set env
if [[ -z $FORCE_LARCV_BASEDIR ]]; then
    where="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
    export LARCV_BASEDIR=${where}
else
    export LARCV_BASEDIR=$FORCE_LARCV_BASEDIR
fi

if [[ -z $LARCV_BUILDDIR ]]; then
    export LARCV_BUILDDIR=$LARCV_BASEDIR/build
fi

export LARCV_LIBDIR=$LARCV_BUILDDIR/lib
export LARCV_INCDIR=$LARCV_BUILDDIR/include

# Abort if ROOT not installed. Let's check rootcint for this.
if [ `command -v rootcling` ]; then
    export LARCV_ROOT6=1
else 
    if [[ -z `command -v rootcint` ]]; then
	echo
	echo Looks like you do not have ROOT installed.
	echo You cannot use LArLite w/o ROOT!
	echo Aborting.
	echo
	return 1;
    fi
fi

# Check OpenCV
export LARCV_OPENCV=1
if [[ -z $OPENCV_INCDIR ]]; then
    export LARCV_OPENCV=0
fi
if [[ -z $OPENCV_LIBDIR ]]; then
    export LARCV_OPENCV=0
fi

# Check Numpy
export LARCV_NUMPY=`$LARCV_BASEDIR/bin/check_numpy`

# warning for missing support
missing=""
if [ $LARCV_OPENCV -eq 0 ]; then
    missing+=" OpenCV"
fi
if [ $LARCV_NUMPY -eq 0 ]; then
    missing+=" Numpy"
fi
if [[ $missing ]]; then
    printf "\033[93mWarning\033[00m ... missing$missing support. Build without them.\n";
fi

echo
printf "\033[93mLArCV\033[00m Setting up shell envs.\n"
printf "    \033[95mLARCV_BASEDIR\033[00m  = $LARCV_BASEDIR\n"
printf "    \033[95mLARCV_INCDIR\033[00m   = $LARCV_INCDIR\n"
printf "    \033[95mLARCV_LIBDIR\033[00m   = $LARCV_LIBDIR\n"
printf "    \033[95mLARCV_BUILDDIR\033[00m = $LARCV_BUILDDIR\n"

export PATH=$LARCV_BASEDIR/bin:$PATH
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$LARCV_LIBDIR
export DYLD_LIBRARY_PATH=$DYLD_LIBRARY_PATH:$LARCV_LIBDIR

if [[ -z $NO_OPENCV ]]; then
    export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$OPENCV_LIBDIR
    export DYLD_LIBRARY_PATH=$DYLD_LIBRARY_PATH:$OPENCV_LIBDIR
fi

mkdir -p $LARCV_BUILDDIR;

#if [ ! -f $LARCV_BASEDIR/APICaffe/caffe.pb.h ]; then
#    printf "\033[93mnotice\033[00m ... generating caffe proto-buf source code (one-time operation)\n"
#    protoc $LARCV_BASEDIR/APICaffe/caffe.proto --proto_path=$LARCV_BASEDIR/APICaffe --cpp_out=$LARCV_BASEDIR/APICaffe/
#    mv $LARCV_BASEDIR/APICaffe/caffe.pb.cc $LARCV_BASEDIR/APICaffe/caffe.pb.cxx
#fi

export LD_LIBRARY_PATH=$LARCV_LIBDIR:$LD_LIBRARY_PATH

#if [ "$LARLITE_OS" -eq "Darwin" ]; then
#    export DYLD_LIBRARY_PATH=$LARCV_LIBDIR:$DYLD_LIBRARY_PATH
#fi

if [[ $LARLITE_BASEDIR ]]; then
    printf "\033[93mLArLite\033[00m\n"
    echo "    Found larlite set up @ \$LARLITE_BASEDIR=${LARLITE_BASEDIR}"
    echo "    Preparing APILArLite package for build (making sym links)"
    target=$LARCV_BASEDIR/larfmwk_shared/*
    for f in $target
    do
	ln -sf $f $LARCV_BASEDIR/Supera/APILArLite/
    done
fi

if [[ -d $MRB_TOP/srcs/uboonecode/uboone ]]; then
    printf "\033[93mLArSoft\033[00m\n"
    echo "    Found local larsoft @ \$MRB_TOP=${MRB_TOP}"
    echo "    Preparing APILArSoft package for build (making sym links)"
    target=$LARCV_BASEDIR/larfmwk_shared/*
    for f in $target
    do
	ln -sf $f $LARCV_BASEDIR/Supera/APILArSoft/
    done
    ln -sf $LARCV_BASEDIR/APILArSoft $MRB_TOP/srcs/uboonecode/uboone/Supera
fi
#if [ -d $MRB_TOP/srcs/uboonecode/uboone ]; then
#    echo Found local uboonecode @ \$MRB_TOP=${MRB_TOP}
#    if [ ! -d $MRB_TOP/srcs/uboonecode/uboone/Supera ]; then
#	echo Making a sym-link for LArSoft API...
#	ln -s $LARCV_BASEDIR/APILArSoft $MRB_TOP/srcs/uboonecode/uboone/Supera
#    fi
#fi

export LARCV_CXX=clang++
if [ -z `command -v $LARCV_CXX` ]; then
    export LARCV_CXX=g++
    if [ -z `command -v $LARCV_CXX` ]; then
        echo
        echo Looks like you do not have neither clang or g++!
        echo You need one of those to compile LArCaffe... Abort config...
        echo
        return 1;
    fi
fi

echo
echo "Finish configuration. To build, type:"
echo "> cd \$LARCV_BUILDDIR"
echo "> cmake \$LARCV_BASEDIR -DCMAKE_CXX_COMPILER=$LARCV_CXX"
echo "> make "
echo
