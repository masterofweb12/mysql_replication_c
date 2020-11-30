/*
    Andrii Hlukhov.
    masterofweb12@gmail.com
    2020

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation; version 2 of
    the License ( https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html ).
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.
    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
    02110-1301  USA
*/

#ifndef __REPL_LOG_LISTENER_ROW
#define __REPL_LOG_LISTENER_ROW

#include <stdint.h>
#include <time.h>
#include <fcntl.h>
#include <inttypes.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>


#define REPL_LOG_X_INSERT 0
#define REPL_LOG_X_UPDATE 1
#define REPL_LOG_X_DELETE 2

#define MAX_TABLE_NAME_LEN    320
#define MAX_DATABASE_NAME_LEN 320

#define TRANSACTION_EV_TYPE_START  0
#define TRANSACTION_EV_TYPE_COMMIT 1


typedef struct __repl_display_blobs {

    uint8_t  BLOBS_DISPLAY;  // отображать или нет такие типы: JSON GEOMETRY BLOB TEXT
    uint8_t  GEOMETRY_AS_HEX;
    uint8_t  BLOB_AS_HEX;

} _repl_display_blobs;

typedef struct __repl_log_x_value {

    char     *val;      // строковое представление 
    int32_t  len;       // длина ( если поле не используется, то -1 )
    uint8_t  is_hex;    // для строковых типов и blob является ли представление бинарным ( значение 0 ),
                        // тогда его длина хранится в поле len 
                        // или это hex представление в текстовом виде ( значение 1 ), 
                        // тогда в len будет длина строки
} _repl_log_x_value;

typedef struct __repl_log_x_rows {

    struct timeval       when;      //  время изменений на мастере
    uint8_t              type_code; //  ( REPL_LOG_X_INSERT || REPL_LOG_X_UPDATE || REPL_LOG_X_DELETE )
    char                 tbl_name[ MAX_TABLE_NAME_LEN + 16 ];
    char                 db_name[ MAX_DATABASE_NAME_LEN + 16 ];
    uint64_t             tbl_id;
    uint32_t             col_count;
    char                 **col_names;
    uint8_t              *col_types;
    uint32_t             rows_count;

    _repl_log_x_value    **rows;
    _repl_log_x_value    **rows_after;
    //---------------------------------
    _repl_log_x_value    *rows_L2;
    _repl_log_x_value    *rows_after_L2;
     char                *r_values;
    //---------------------------------

    _repl_display_blobs  display_blobs;

} _repl_log_x_rows;


typedef struct __repl_log_x_transaction {

    struct timeval       when;      //  время изменений на мастере
    uint8_t              transaction_event_type; // ( TRANSACTION_EV_TYPE_START || TRANSACTION_EV_TYPE_COMMIT )

} _repl_log_x_transaction;


typedef struct __repl_log_x_heartbeat {

    struct timeval       when;      //  время изменений на мастере

} _repl_log_x_heartbeat;


typedef struct __repl_log_x_error {

    uint8_t     is_error;
    uint8_t     is_error_MySQL;
    int64_t     code;
    char        description[1024];
    char        file[1024];
    int64_t     line;

} _repl_log_x_error;


#endif
