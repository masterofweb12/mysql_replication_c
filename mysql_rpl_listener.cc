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


#include <fcntl.h>
#include <inttypes.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <algorithm>
#include <map>
#include <utility>


#include "mysql_rpl_listener.h"
#include "mysql_rpl_listener_2/mysql_rpl_listener_2.h"





extern "C" int mysql_rpl_listener__init(    RPL_LISTENER_H *rpl_h,
                                            void (*on_rpl_row)( _repl_log_x_rows*, void* ), 
                                            void *on_rpl_row_param_ptr,
                                            void (*on_rpl_heartbeat)( _repl_log_x_heartbeat*, void* ), 
                                            void *on_rpl_heartbeat_param_ptr,
                                            const char  *my_host,
                                            const char  *my_user,
                                            const char  *my_pass,
                                            uint32_t     my_port,
                                            unsigned int slave_id,
                                            uint8_t      blobs_display,
                                            uint8_t      geometry_as_hex,
                                            uint8_t      blob_as_hex,
                                            void (*on_rpl_transaction)( _repl_log_x_transaction*, void* ),
                                            void *on_rpl_transaction_param_ptr,
                                            char     *master_file_name,
                                            uint64_t  master_file_pos,
                                            RPL_TABLE_MAPS *tbl_maps_ptr ) {


    rpl_h[0] = NULL;

    mysql_rpl_listener_2 *listener_ptr;

    try {

    listener_ptr = new mysql_rpl_listener_2 (   on_rpl_row, 
                                                on_rpl_row_param_ptr,
                                                on_rpl_heartbeat,
                                                on_rpl_heartbeat_param_ptr,
                                                my_host,
                                                my_user,
                                                my_pass,
                                                my_port,
                                                slave_id,
                                                blobs_display,
                                                geometry_as_hex,
                                                blob_as_hex,
                                                on_rpl_transaction,
                                                on_rpl_transaction_param_ptr,
                                                master_file_name,
                                                master_file_pos,
                                                ( TBLMAP_EVNS* )tbl_maps_ptr );
    }
    catch ( ... ) {

        return 100;
    }

    rpl_h[0] = listener_ptr;
    return 0;
}


extern "C" int mysql_rpl_listener__prepare( RPL_LISTENER_H *rpl_h, _repl_log_x_error *error_ptr ) {

    memset( error_ptr, 0, sizeof( _repl_log_x_error ) );

    mysql_rpl_listener_2 *listener_ptr;
    if ( rpl_h[0] == NULL ) {

        return 1010;
    }
    listener_ptr = ( mysql_rpl_listener_2* )rpl_h[0];

    try {

        listener_ptr->prepare_replication( error_ptr );
        if ( error_ptr[0].is_error != 0 ) {

            return 1020;
        }
        return 0;
    }
    catch ( ... ) {

        return 1030;
    }
    return 1040;
}



extern "C" int mysql_rpl_listener__start( RPL_LISTENER_H *rpl_h, _repl_log_x_error *error_ptr ) {

    memset( error_ptr, 0, sizeof( _repl_log_x_error ) );
    
    mysql_rpl_listener_2 *listener_ptr;
    if ( rpl_h[0] == NULL ) {

        return 1010;
    }
    listener_ptr = ( mysql_rpl_listener_2* )rpl_h[0];

    try {

        listener_ptr->start_replication( error_ptr );
        if (  error_ptr[0].is_error != 0 ) {

            return 1020;
        }
        return 0;
    }
    catch ( ... ) {

        return 1030;
    }
    return 1040;
}


extern "C" int mysql_rpl_listener__stop( RPL_LISTENER_H *rpl_h  ) {

    mysql_rpl_listener_2 *listener_ptr;
    if ( rpl_h[0] == NULL ) {

        return 1010;
    }
    listener_ptr = ( mysql_rpl_listener_2* )rpl_h[0];

    try {

        return listener_ptr->stop_replication();
    }
    catch ( ... ) {

        return 1020;
    }
    return 1030;
}


extern "C" int mysql_rpl_listener__destroy( RPL_LISTENER_H *rpl_h ) {

    mysql_rpl_listener_2 *listener_ptr;
    if ( rpl_h[0] == NULL ) {

        return 1010;
    }
    listener_ptr = ( mysql_rpl_listener_2* )rpl_h[0];

    try {

        delete listener_ptr;
        rpl_h[0] = NULL;
        return 0;
    }
    catch ( ... ) {

        return 1020;
    }
    return 1030;
}


extern "C" int mysql_rpl_listener__in_process( RPL_LISTENER_H *rpl_h ) {

    mysql_rpl_listener_2 *listener_ptr;
    if ( rpl_h[0] == NULL ) {

        return REPL_PROC_STATUS_OFF;
    }
    listener_ptr = ( mysql_rpl_listener_2* )rpl_h[0];

    try {
    
        return listener_ptr->in_process();
    }
    catch ( ... ) {

        return REPL_PROC_STATUS_OFF;
    }
    return REPL_PROC_STATUS_OFF;
}


extern "C" int mysql_rpl_listener__table_add ( RPL_LISTENER_H *rpl_h, const char* db_name, const char* tbl_name ) {

    mysql_rpl_listener_2 *listener_ptr;
    if ( rpl_h[0] == NULL ) {

        return 1010;
    }

    listener_ptr = ( mysql_rpl_listener_2* )rpl_h[0];
    try {
    
        return listener_ptr->table_add( db_name, tbl_name );
    }
    catch ( ... ) {

        return 1020;
    }  
}



extern "C" int mysql_rpl_listener__get_last_sucess_pos( RPL_LISTENER_H *rpl_h, char *file_n, int file_maxlen, uint64_t *pos ) {

    mysql_rpl_listener_2 *listener_ptr;
    if ( rpl_h[0] == NULL ) {

        return 1010;
    }
    listener_ptr = ( mysql_rpl_listener_2* )rpl_h[0];

    try {
    
        listener_ptr->get_last_sucessful_position( file_n, file_maxlen, pos );
        return 0;
    }
    catch ( ... ) {

        return 1020;
    }
    return 1030; 
}



extern "C" int mysql_rpl_listener__get_tablemaps( RPL_LISTENER_H *rpl_h, RPL_TABLE_MAPS *dst_tbl_maps, _repl_log_x_error *error_ptr ) {

    memset( error_ptr, 0, sizeof( _repl_log_x_error ) );
    
    mysql_rpl_listener_2 *listener_ptr;
    if ( rpl_h[0] == NULL ) {

        return 1010;
    }
    listener_ptr = ( mysql_rpl_listener_2* )rpl_h[0];
    
    try {
    
        listener_ptr->get_tablemap_events( ( TBLMAP_EVNS* )dst_tbl_maps, error_ptr );
        if ( error_ptr[0].is_error == 0 ) {

            return 0;

        } else {

            return 1020;
        }
        
    }
    catch ( ... ) {

        return 1030;
    }
    return 1040; 
}




extern "C" int mysql_rpl_listener__free_tablemaps( RPL_TABLE_MAPS *tbl_maps ) {
    
    try {
    
        mysql_rpl_listener_2::destroy_tablemap_events( ( TBLMAP_EVNS* )tbl_maps );
        return 0;   
    }
    catch ( ... ) {

        return 1010;
    }
    return 1020; 
}

