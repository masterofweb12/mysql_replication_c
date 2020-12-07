#!/bin/sh

#
#    Andrii Hlukhov.
#    masterofweb12@gmail.com
#    2020
#
#    This program is free software; you can redistribute it and/or
#    modify it under the terms of the GNU General Public License
#    as published by the Free Software Foundation; version 2 of
#    the License ( https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html ).
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
#    GNU General Public License for more details.
#    You should have received a copy of the GNU General Public License
#    along with this program; if not, write to the Free Software
#    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
#    02110-1301  USA
#


if [ -f ./rpl_test_2 ]; then
    rm ./rpl_test_2
fi

if [ -f ../../libmysql_rpl_listener.so ]; then

echo "shared library present"

else

cd ../../
./build.sh
cd example/C/

fi

gcc -v -g -O0 `mysql_config --cflags` \
 -o ./rpl_test_2 ./rpl_test_2.c \
 -lm -lrt -ldl  -L../../ -lmysql_rpl_listener  `mysql_config --libs` -lpthread -ldl -Wl,-rpath,../../



