#!/bin/sh


if [ -f ./rpl_test_2 ]; then
    rm ./rpl_test_2
fi

if [ -f ../../mysql_rpl_listener.so ]; then

echo "shared library present"

else

cd ../../
./build.sh
cd example/C/

fi

gcc -v -g -O0 `mysql_config --cflags` \
 -o ./rpl_test_2 ./rpl_test_2.c \
 -lm -lrt -ldl  -L../../ -lmysql_rpl_listener  `mysql_config --libs` -lpthread -ldl -Wl,-rpath,../../



