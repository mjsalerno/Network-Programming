#!/bin/sh
# scp all files to minix

# must give login name for minix
if test "$#" -ne 1; then
	echo "usage: $0 minixlogin"
        exit	
fi

# transfer the makefile and all .c/.h
FILES=$(ls *.c *.h makefile)

# remove the newlines
PRJFILES=$(echo $FILES | tr '\n' ' ')

echo "Trying to transfer:\n"$PRJFILES
# must have minix in ~/.ssh/config
# 130.245.156.19
scp $PRJFILES $1@minix:/users/cse533/students/$1/cse533
