#!/bin/sh
# scp all files to minix

# must give login name for minix
if [ "$#" -ne 1 ] || [[ ! $1 =~ ^[0-9][0-9]?$ ]] ; then
	echo "usage: $0 minixlogin suffix"
	echo "e.g. if login is cse533-8, then arg should be 8"
    exit
fi

# transfer the makefile and all .c/.h
FILES=$(ls *.c *.h makefile)

# remove the newlines
PRJFILES=$(echo $FILES | tr '\n' ' ')

echo "Trying to transfer:\n"$PRJFILES
# must have minix in ~/.ssh/config
# 130.245.156.19
scp $PRJFILES cse533-$1@minix:/users/cse533/students/cse533-$1/cse533
