sed -i s/main/notmain/g server.c
sed -i s/main/notmain/g client.c

make server.o
make client.o
make xtcp.o
make common.o
gcc server.o common.o xtcp.o test.c -o test

sed -i s/notmain/main/g server.c 
sed -i s/notmain/main/g client.c