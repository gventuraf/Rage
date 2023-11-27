#./main main.ra > return_test.ll
llc -filetype=obj main.ra.ll -o testing.o
clang -no-pie testing.o -o testing.out
./testing.out
echo $?

rm testing.o testing.out