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


MYSQL_SOURCE_VERSION="mysql-8.0.22"

rpl > /dev/null 2>&1;
RPL_TEST_RES="$?"
if [ "$RPL_TEST_RES" -eq "127"  ]; then

   echo "ERROR!!! Can not find rpl ( apt install rpl for ubuntu  )!!!";
   exit;
fi;


CURR_DIR=$PWD

echo $CURR_DIR
echo

if [ -f $CURR_DIR/mysql_rpl_listener_2/mysql_rpl_listener_2.cc.o  ]; then
rm $CURR_DIR/mysql_rpl_listener_2/mysql_rpl_listener_2.cc.o
fi
if [ -f $CURR_DIR/mysql_rpl_listener.cc.o  ]; then
rm $CURR_DIR/mysql_rpl_listener.cc.o
fi
if [ -f $CURR_DIR/libmysql_rpl_listener.so  ]; then
rm $CURR_DIR/libmysql_rpl_listener.so
fi


if [ -d $CURR_DIR/mysql_source/mysql ]; then


    if [ -f $CURR_DIR/mysql_source/mysql/MYSQL_VERSION ]; then

        FMSV=`cat $CURR_DIR/mysql_source/mysql/MYSQL_VERSION`

        M_MAJOR=`echo $FMSV | awk -F " " '{ print $1 }' | awk -F "=" '{ print $2 }'`
        M_MINOR=`echo $FMSV | awk -F " " '{ print $2 }' | awk -F "=" '{ print $2 }'`
        M_PATCH=`echo $FMSV | awk -F " " '{ print $3 }' | awk -F "=" '{ print $2 }'`
        M_EXTRA=`echo $FMSV | awk -F " " '{ print $4 }' | awk -F "=" '{ print $2 }'`

        M_F_VER="mysql-$M_MAJOR.$M_MINOR.$M_PATCH"
        ##echo $M_F_VER

        if [ "x$M_F_VER" != "x$MYSQL_SOURCE_VERSION" ]; then

              rm -r -f $CURR_DIR/mysql_source/mysql
              git clone -v --branch "$MYSQL_SOURCE_VERSION" --depth 1 https://github.com/mysql/mysql-server.git mysql_source/mysql



        fi

    else

         rm -r -f $CURR_DIR/mysql_source/mysql
         git clone -v --branch "$MYSQL_SOURCE_VERSION" --depth 1 https://github.com/mysql/mysql-server.git mysql_source/mysql
    fi

else

    git clone -v --branch "$MYSQL_SOURCE_VERSION" --depth 1 https://github.com/mysql/mysql-server.git mysql_source/mysql

fi


rm -r ./mysql_source/build/*
echo "" > ./mysql_source/build/1.txt



cd    ./mysql_source/build
cmake ../mysql -DDOWNLOAD_BOOST=1 -DWITH_BOOST=../mysql  -DCMAKE_VERBOSE_MAKEFILE:BOOL=ON -DCMAKE_POSITION_INDEPENDENT_CODE=ON
cd ../../


find ./mysql_source/build/ -name '*.make' | xargs rpl -v ' -fPIE ' ' -fPIC '



cd ./mysql_source/build/client



make mysqlbinlog



/usr/bin/c++ \
-DDISABLE_PSI_MUTEX -DHAVE_CONFIG_H -DHAVE_LIBEVENT2 -DHAVE_OPENSSL -DLZ4_DISABLE_DEPRECATE_WARNINGS \
-DRAPIDJSON_NO_SIZETYPEDEFINE -DRAPIDJSON_SCHEMA_USE_INTERNALREGEX=0 -DRAPIDJSON_SCHEMA_USE_STDREGEX=1 \
-DUNISTR_FROM_CHAR_EXPLICIT=explicit -DUNISTR_FROM_STRING_EXPLICIT=explicit -D_FILE_OFFSET_BITS=64 \
-D_GNU_SOURCE -D_USE_MATH_DEFINES -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS \
-isystem $CURR_DIR/mysql_source/mysql/extra/rapidjson/include \
-isystem $CURR_DIR/mysql_source/mysql/extra/lz4 \
-isystem $CURR_DIR/mysql_source/mysql/extra/libedit/editline \
-isystem $CURR_DIR/mysql_source/mysql/extra/zstd/lib \
-isystem $CURR_DIR/mysql_source/build/extra/zlib \
-isystem $CURR_DIR/mysql_source/mysql/extra/zlib \
-I$CURR_DIR/mysql_source/build \
-I$CURR_DIR/mysql_source/build/include \
-I$CURR_DIR/mysql_source/mysql \
-I$CURR_DIR/mysql_source/mysql/include \
-I$CURR_DIR/mysql_source/mysql/sql \
-fPIC \
 -std=c++14 -fno-omit-frame-pointer  -Wall -Wextra -Wformat-security -Wvla -Wundef \
-Wmissing-format-attribute -Woverloaded-virtual -Wcast-qual -Wlogical-op \
-DDBUG_OFF -ffunction-sections -fdata-sections -O0 -g -DNDEBUG \
-Wno-unused-parameter \
-o $CURR_DIR/mysql_rpl_listener_2/mysql_rpl_listener_2.cc.o \
-c $CURR_DIR/mysql_rpl_listener_2/mysql_rpl_listener_2.cc




/usr/bin/c++ \
-DDISABLE_PSI_MUTEX -DHAVE_CONFIG_H -DHAVE_LIBEVENT2 -DHAVE_OPENSSL -DLZ4_DISABLE_DEPRECATE_WARNINGS \
-DRAPIDJSON_NO_SIZETYPEDEFINE -DRAPIDJSON_SCHEMA_USE_INTERNALREGEX=0 -DRAPIDJSON_SCHEMA_USE_STDREGEX=1 \
-DUNISTR_FROM_CHAR_EXPLICIT=explicit -DUNISTR_FROM_STRING_EXPLICIT=explicit -D_FILE_OFFSET_BITS=64 \
-D_GNU_SOURCE -D_USE_MATH_DEFINES -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS \
-isystem $CURR_DIR/mysql_source/mysql/extra/rapidjson/include \
-isystem $CURR_DIR/mysql_source/mysql/extra/lz4 \
-isystem $CURR_DIR/mysql_source/mysql/extra/libedit/editline \
-isystem $CURR_DIR/mysql_source/mysql/extra/zstd/lib \
-isystem $CURR_DIR/mysql_source/build/extra/zlib \
-isystem $CURR_DIR/mysql_source/mysql/extra/zlib \
-I$CURR_DIR/mysql_source/build \
-I$CURR_DIR/mysql_source/build/include \
-I$CURR_DIR/mysql_source/mysql \
-I$CURR_DIR/mysql_source/mysql/include \
-I$CURR_DIR/mysql_source/mysql/sql \
-fPIC \
-std=c++14 -fno-omit-frame-pointer  -Wall -Wextra -Wformat-security -Wvla -Wundef \
-Wmissing-format-attribute -Woverloaded-virtual -Wcast-qual -Wlogical-op \
-DDBUG_OFF -ffunction-sections -fdata-sections -O0 -g -DNDEBUG \
-Wno-unused-parameter \
-o $CURR_DIR/mysql_rpl_listener.cc.o \
-c $CURR_DIR/mysql_rpl_listener.cc




/usr/bin/c++  -std=c++14 -fno-omit-frame-pointer  -Wall -Wextra -Wformat-security -Wvla -Wundef -Wmissing-format-attribute -Woverloaded-virtual \
-Wcast-qual  -Wlogical-op -DDBUG_OFF -ffunction-sections -fdata-sections \
-O2 -g -DNDEBUG  -fuse-ld=gold -Wl,--gc-sections \
-Wl,--version-script=$CURR_DIR/libcode.version \
-fPIC \
-shared \
$CURR_DIR/mysql_rpl_listener.cc.o \
$CURR_DIR/mysql_rpl_listener_2/mysql_rpl_listener_2.cc.o \
CMakeFiles/mysqlbinlog.dir/__/sql/json_binary.cc.o \
CMakeFiles/mysqlbinlog.dir/__/sql/json_dom.cc.o \
CMakeFiles/mysqlbinlog.dir/__/sql/json_syntax_check.cc.o \
CMakeFiles/mysqlbinlog.dir/__/sql/log_event.cc.o \
CMakeFiles/mysqlbinlog.dir/__/sql/rpl_utility.cc.o \
CMakeFiles/mysqlbinlog.dir/__/sql/rpl_gtid_sid_map.cc.o \
CMakeFiles/mysqlbinlog.dir/__/sql/rpl_gtid_misc.cc.o \
CMakeFiles/mysqlbinlog.dir/__/sql/rpl_gtid_set.cc.o \
CMakeFiles/mysqlbinlog.dir/__/sql/rpl_gtid_specification.cc.o \
CMakeFiles/mysqlbinlog.dir/__/sql/rpl_tblmap.cc.o \
CMakeFiles/mysqlbinlog.dir/__/sql/basic_istream.cc.o \
CMakeFiles/mysqlbinlog.dir/__/sql/binlog_istream.cc.o \
CMakeFiles/mysqlbinlog.dir/__/sql/binlog_reader.cc.o \
CMakeFiles/mysqlbinlog.dir/__/sql/stream_cipher.cc.o \
CMakeFiles/mysqlbinlog.dir/__/sql/rpl_log_encryption.cc.o \
CMakeFiles/mysqlbinlog.dir/__/libbinlogevents/src/trx_boundary_parser.cpp.o  \
-o $CURR_DIR/libmysql_rpl_listener.so -lpthread \
../archive_output_directory/libmysqlclient.a base/libclient_base.a \
../libbinlogevents/lib/libbinlogevents.a \
../archive_output_directory/libmysqlclient.a \
../archive_output_directory/libmysys.a -lm -lrt \
/usr/lib/x86_64-linux-gnu/libssl.so \
/usr/lib/x86_64-linux-gnu/libcrypto.so -ldl \
../archive_output_directory/libmytime.a \
../archive_output_directory/libstrings.a \
../archive_output_directory/libz.a \
../archive_output_directory/libzstd.a -lpthread




cd ../../../










