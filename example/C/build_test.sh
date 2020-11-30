#!/bin/sh


if [ -f ./rpl_test_2 ]; then
    rm ./rpl_test_2
fi

gcc -v -g -O0 `mysql_config --cflags` \
 -o ./rpl_test_2 ./rpl_test_2.c \
 -lm -lrt -ldl  -L../../ -lmysql_rpl_listener  `mysql_config --libs` -lpthread -ldl -Wl,-rpath,../../



