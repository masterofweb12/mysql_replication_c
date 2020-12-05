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

#include "client/mysqlbinlog.h"

#include "caching_sha2_passwordopt-vars.h"
#include "client/client_priv.h"
#include "compression.h"
#include "libbinlogevents/include/trx_boundary_parser.h"
#include "my_byteorder.h"
#include "my_dbug.h"
#include "my_default.h"
#include "my_dir.h"
#include "my_io.h"
#include "my_macros.h"
#include "my_time.h"
#include "prealloced_array.h"
#include "print_version.h"
#include "sql/binlog_reader.h"
#include "sql/log_event.h"
#include "sql/my_decimal.h"
#include "sql/rpl_constants.h"
#include "sql/rpl_gtid.h"
#include "sql_common.h"
#include "sql_string.h"
#include "sslopt-vars.h"
#include "typelib.h"
#include "welcome_copyright_notice.h"  // ORACLE_WELCOME_COPYRIGHT_NOTICE

#include "sql/rpl_record.h"  // enum_row_image_type, Bit_reader
#include "sql/system_variables.h"
#include "libbinlogevents/export/binary_log_funcs.h"  // my_timestamp_binary_length
#include "libbinlogevents/include/control_events.h"
#include "libbinlogevents/include/rows_event.h"


#include "mysql_rpl_listener_2.h"

ulong opt_server_id_mask = 0;
bool  short_form = false;
char  server_version[SERVER_VERSION_LENGTH];

Checkable_rwlock *global_sid_lock = nullptr;
Sid_map *global_sid_map = nullptr;

#include "../rows_log_event_2/rows_log_event_2.cc"



void repl_log_x_error_clear( _repl_log_x_error *err ) {

    memset( err, 0, sizeof( _repl_log_x_error ) );
}

void repl_log_x_error_set( _repl_log_x_error *err, bool is_error_MySQL, int32_t  code, const char * desc, const char* file, int64_t line ) {

    err[0].is_error = 1;
    err[0].is_error_MySQL = is_error_MySQL;
    err[0].code = (int64_t)code;
    snprintf( err[0].description, 1024, "%s", desc );

    err[0].line = line;
    snprintf( err[0].file, 1024, "%s", file );

}

const char *extract_filename_from_full_path( const char *fp ) {

    const char *p = fp;
    int fp_len = strlen( p );
    for ( int i=fp_len-1; i>=0; i-- ) {

        if ( p[i] == '/' ) {

            return ( &p[i+1] );
        }
    }
    return p;
}

#define repl_log_error_set(err,is_error_MySQL,code,desc)  \
repl_log_x_error_set(err,is_error_MySQL,code,desc,extract_filename_from_full_path(__FILE__),__LINE__)




// TODO доработать то, что пока репликация стартанула, но ещё не запецилась 
// (  mysql_rpl_listener_2::__State::INPROCESS_1 ),
// то, что у нас хранится в MY_RPL_H.file_name и MY_RPL_H.start_position нельзя 
// считать последней успешно выполненной позицией
void mysql_rpl_listener_2::get_last_sucessful_position( char *file, int file_maxlen, uint64_t *pos ) {

    if ( this->MY_RPL_H.file_name != NULL ) {
    
        snprintf( file, file_maxlen, "%s", this->MY_RPL_H.file_name );
        pos[0]  =  this->MY_RPL_H.start_position;
    
    } else {

        snprintf( file, file_maxlen, "%s", "" );
        pos[0]  =  0;
    }
}





void mysql_rpl_listener_2::tables_clear ( void ) {

    for( uint32_t i=0; i<this->tables.size(); i++ ) {

        if ( this->tables.at(i).info.columns != NULL ) {

            free ( this->tables.at(i).info.columns );
        }
        this->tables.at(i).info.col_count = 0;
    }
}



mysql_rpl_listener_2::~mysql_rpl_listener_2() {

    if ( this->in_process_global() ) {

        this->stop_replication();
    }
    
    usleep( 100000 );
    
    if ( this->my_conn != NULL ) {

        mysql_close( this->my_conn );
        this->my_conn = NULL;
    }

    this->tables_clear();

    if ( this->glob_description_event != nullptr ) {
            
        delete ( Format_description_event* )( this->glob_description_event );
        this->glob_description_event = nullptr;
    }

    for (auto it = this->glob_table_map_events.begin(); it != this->glob_table_map_events.end(); ++it) {
    
        if ( (*it).second != NULL ) {
        
            delete ( Table_map_log_event* )( (*it).second );
        }
    }
    this->glob_table_map_events.clear();

}





int32_t mysql_rpl_listener_2::in_process_global( void ) {

    uint32_t st;
    st = __sync_fetch_and_add( &( this->state ), (uint32_t)0 );
    if ( st == mysql_rpl_listener_2::__State::INPROCESS_1 ) {

        return 1;
    }
    if ( st == mysql_rpl_listener_2::__State::INPROCESS ) {

        return 1;
    }

    return 0;
}

int32_t mysql_rpl_listener_2::in_process( void ) {

    uint32_t st;
    st = __sync_fetch_and_add( &( this->state ), (uint32_t)0 );
    if ( ( st == mysql_rpl_listener_2::__State::INPROCESS ) || ( st == mysql_rpl_listener_2::__State::INPROCESS_1 ) ) {

        return st;
    }

    return mysql_rpl_listener_2::__State::CREATED;
}







mysql_rpl_listener_2::mysql_rpl_listener_2 (    void (*on_rpl_row)( _repl_log_x_rows*, void* ), 
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
                                                char         *master_file_name, 
                                                uint64_t     master_file_pos,
                                                TBLMAP_EVNS  *tblmaps_collection_ptr ) {

    
    this->in_transaction = 0;
    this->position_was_setted = 0;

    this->glob_description_event = NULL;
    this->glob_table_map_events.clear();
    

    this->state = mysql_rpl_listener_2::__State::CREATED;

    memset( &(this->thread_arg), 0, sizeof( _repl_thread_arg ) );
    
    memset( this->my_host, 0, 256 );
    memset( this->my_user, 0, 256 );
    memset( this->my_pass, 0, 256 );

    snprintf( this->my_host, 255, "%s", my_host );
    snprintf( this->my_user, 255, "%s", my_user );
    snprintf( this->my_pass, 255, "%s", my_pass );
    this->my_port = my_port;


    this->BLOBS_DISPLAY   = ( blobs_display > 0 ) ? 1 : 0;
    this->GEOMETRY_AS_HEX = ( geometry_as_hex > 0 ) ? 1 : 0;
    this->BLOB_AS_HEX     = ( blob_as_hex > 0 ) ? 1 : 0;


    this->my_conn           = NULL;
    this->need_stop         = false;
    this->repl_thread       = 0;

    MYSQL_RPL *my_rpl = &(this->MY_RPL_H);
    
    this->on_row = on_rpl_row;
    this->on_row_param_ptr = on_rpl_row_param_ptr;

    this->on_heartbeat = on_rpl_heartbeat;
    this->on_heartbeat_param_ptr = on_rpl_heartbeat_param_ptr;

    this->on_transaction = on_rpl_transaction;
    this->on_transaction_param_ptr = on_rpl_transaction_param_ptr;


    memset( this->m_log_nm, 0, 512 );
    memset( my_rpl, 0, sizeof( MYSQL_RPL ) );


    my_rpl[0].server_id = slave_id;
    // MYSQL_RPL_SKIP_HEARTBEAT; 
    // нам крайне нужны eventы типа MYSQL_RPL_SKIP_HEARTBEAT, 
    // чтоб достоверно определять отставание от мастера ,
    // потому ни в коем случае не ставим флаг MYSQL_RPL_SKIP_HEARTBEAT
    my_rpl[0].flags  = 0; 
    //my_rpl[0].flags |= MYSQL_RPL_GTID; 

    
    // ----- SET RPL file name and position and tablemaps -----

    if ( master_file_name != NULL ) {

        snprintf( this->m_log_nm, 512, "%s", master_file_name );
        my_rpl[0].file_name        = this->m_log_nm;
        my_rpl[0].file_name_length = strlen( my_rpl[0].file_name );
        my_rpl[0].start_position   = master_file_pos;

        if ( tblmaps_collection_ptr != NULL ) {


            int glob_description_event_CREATED;
            try {
            
                this->glob_description_event = new Format_description_event( BINLOG_VERSION, "8.0.0" );
                glob_description_event_CREATED = 1;
            }
            catch ( ... ) {

                this->glob_description_event = NULL;
                glob_description_event_CREATED = 0;
            }   

            if ( glob_description_event_CREATED == 1 ) {

                for( uint32_t i=0; i<tblmaps_collection_ptr[0][0].size(); i++ ){

                    char *buff = ( char* )( tblmaps_collection_ptr[0][0].at(i) );
                    Log_event_type event_type = (Log_event_type)buff[ EVENT_TYPE_OFFSET ];
                    Log_event *ev = NULL;
                    
                    if ( event_type == binary_log::TABLE_MAP_EVENT ) {


                        uint32_t event_len = ( ( uint32_t* )( &( buff[ EVENT_LEN_OFFSET ] ) ) )[0];
                        char     *event_buf = (char *)my_malloc( key_memory_log_event, event_len + 1, MYF(0) );
                        if ( !event_buf ) {

                            break;
                        }
                        memcpy( event_buf, buff, event_len );


                        Binlog_read_error read_error;
                        try {

                            read_error = binlog_event_deserialize(
                                                reinterpret_cast<unsigned char *>(event_buf), 
                                                event_len,
                                                ( Format_description_event* )( this->glob_description_event ), 
                                                false /* opt_verify_binlog_checksum */, // не чекаем потому, 
                                                                                        // что мы не знаем верная ли была 
                                                                                        // server_version при создании 
                                                                                        // Format_description_event
                                                &ev );
                        }
                        catch ( ... ) {

                            my_free(event_buf);
                            break;
                        }                                     
                        if ( read_error.has_error() ) {
                            
                            my_free(event_buf);
                            break;
                        }
                        ev->register_temp_buf(event_buf);

                        Table_map_log_event   *tmp_ev;
                        tmp_ev = dynamic_cast<Table_map_log_event*>(ev);

                        uint64_t tmp_key;
                        tmp_key = tmp_ev->get_table_id().id();


                        std::map<uint64_t,void*>::iterator it;
                        it = this->glob_table_map_events.find( tmp_key );
                        if ( it != this->glob_table_map_events.end() ) {

                            if ( (*it).second != NULL ) {
                            
                                delete ( Table_map_log_event* )( (*it).second );
                            }
                            this->glob_table_map_events.erase(it);                
                        }
                        this->glob_table_map_events.insert( std::pair<uint64_t,void*>( tmp_key, (void*)tmp_ev ) );

                        // printf( "tablemap %s.%s added\n", tmp_ev->get_db_name(), tmp_ev->get_table_name() );
                    }
                }

                delete (Format_description_event*)this->glob_description_event;
                this->glob_description_event = NULL;

            }

        }
    
    } 
    
    // ----- SET RPL file name and position and tablemaps -----

    return;
}


int32_t mysql_rpl_listener_2::table_add ( const char* db_name, const char* tbl_name ) {
    
    uint32_t st;
    st = __sync_fetch_and_add( &( this->state ), (uint32_t)0 );
    if ( st != mysql_rpl_listener_2::__State::CREATED ) {

        return 500;
    }

    _listening_tables new_tbl;
    memset( &( new_tbl.info ), 0, sizeof( _tbl_info ) );
    snprintf( new_tbl.db_name,  MAX_DATABASE_NAME_LEN + 1, "%s", db_name  );
    snprintf( new_tbl.tbl_name, MAX_TABLE_NAME_LEN + 1,    "%s", tbl_name );

    try {
    
        this->tables.push_back( new_tbl );
    
    } catch ( std::exception& e ) {

        return 700;
    }

    return 0;
}


/*
    метод должен быть вызван после добавления таблиц
    по которым отслеживаются изменения ( mysql_rpl_listener_2::table_add() ),
    в противном случае метод завершится ошибкой 
*/
void mysql_rpl_listener_2::prepare_replication( _repl_log_x_error *error_ptr ) {

    repl_log_x_error_clear( error_ptr );

    if ( !( __sync_bool_compare_and_swap ( &(this->state), 
                                            (uint32_t)(mysql_rpl_listener_2::__State::CREATED),
                                            (uint32_t)(mysql_rpl_listener_2::__State::POST_CREATED) ) ) ) {

        repl_log_error_set( error_ptr, 0, __LINE__, "prepare_replication() ERROR ORDER!!!" );
        return;
    }



    if ( this->tables.size() == 0 ) {

        repl_log_error_set( error_ptr, 0, __LINE__, "tables are empry!!! call method table_add() to fill list of tables!!!" );
        __sync_bool_compare_and_swap( &(this->state), 
                                        (uint32_t)(mysql_rpl_listener_2::__State::POST_CREATED),
                                        (uint32_t)(mysql_rpl_listener_2::__State::ENDED ) );
        return;
    }

    MYSQL             *my_conn_res = NULL;
    MYSQL_RES *res    = NULL;
    MYSQL_ROW  row    = NULL;


    this->my_conn = mysql_init( NULL );
    if ( this->my_conn == NULL ) {

        repl_log_error_set( error_ptr, 0, __LINE__, "mysql_init() failed!!!" );
        __sync_bool_compare_and_swap( &(this->state), 
                                        (uint32_t)(mysql_rpl_listener_2::__State::POST_CREATED),
                                        (uint32_t)(mysql_rpl_listener_2::__State::ENDED ) );
        return;
    }


    my_conn_res = mysql_real_connect(   this->my_conn,
                                        this->my_host,
                                        this->my_user,
                                        this->my_pass,
                                        NULL,
                                        (uint)this->my_port,
                                        "",
                                        0 );
    if ( my_conn_res == NULL ) {

        repl_log_error_set( error_ptr, 1, mysql_errno( this->my_conn ), mysql_error( this->my_conn ) );
        mysql_close( this->my_conn );
        this->my_conn = NULL;
        __sync_bool_compare_and_swap( &(this->state), 
                                        (uint32_t)(mysql_rpl_listener_2::__State::POST_CREATED),
                                        (uint32_t)(mysql_rpl_listener_2::__State::ENDED ) );
        return;
    }

    if ( mysql_query( this->my_conn, "SELECT VERSION()" ) ) {

        repl_log_error_set( error_ptr, 1, mysql_errno( this->my_conn ), mysql_error( this->my_conn ) );
        mysql_close( this->my_conn );
        this->my_conn = NULL;
        __sync_bool_compare_and_swap( &(this->state), 
                                        (uint32_t)(mysql_rpl_listener_2::__State::POST_CREATED),
                                        (uint32_t)(mysql_rpl_listener_2::__State::ENDED ) );
        return;
    }
    res = mysql_store_result( my_conn );
    if ( !res ) {

        repl_log_error_set( error_ptr, 1, mysql_errno( this->my_conn ), mysql_error( this->my_conn ) );
        mysql_close( this->my_conn );
        this->my_conn = NULL;
        __sync_bool_compare_and_swap( &(this->state), 
                                        (uint32_t)(mysql_rpl_listener_2::__State::POST_CREATED),
                                        (uint32_t)(mysql_rpl_listener_2::__State::ENDED ) );        
        return;  
    }
    if ( !( row = mysql_fetch_row(res) ) ) {
        
        repl_log_error_set( error_ptr, 0, __LINE__, "Could not find server version. Master returned no rows for SELECT VERSION()" );
        mysql_free_result( res );
        mysql_close( this->my_conn );
        this->my_conn = NULL;
        __sync_bool_compare_and_swap( &(this->state), 
                                        (uint32_t)(mysql_rpl_listener_2::__State::POST_CREATED),
                                        (uint32_t)(mysql_rpl_listener_2::__State::ENDED ) );
        return;
    }
    if ( row[0] == NULL ) {

        repl_log_error_set( error_ptr, 0, __LINE__, "Could not find server version. Master reported NULL for SELECT VERSION()" );
        mysql_free_result( res );
        mysql_close( this->my_conn );
        this->my_conn = NULL;
        __sync_bool_compare_and_swap( &(this->state), 
                                        (uint32_t)(mysql_rpl_listener_2::__State::POST_CREATED),
                                        (uint32_t)(mysql_rpl_listener_2::__State::ENDED ) );
        return;
    }
    snprintf( server_version , SERVER_VERSION_LENGTH, "%s", row[0] );
    mysql_free_result( res );

    if ( strcmp( server_version, "" ) == 0 ) {

        repl_log_error_set( error_ptr, 0, __LINE__, "Could not find server version. Master reported empty string for SELECT VERSION()" );
        mysql_close( this->my_conn );
        this->my_conn = NULL;
        __sync_bool_compare_and_swap( &(this->state), 
                                        (uint32_t)(mysql_rpl_listener_2::__State::POST_CREATED),
                                        (uint32_t)(mysql_rpl_listener_2::__State::ENDED ) );
        return;
    }



    //-------------

    uint8_t binlog_format_OK       = 0;
    uint8_t binlog_row_image_OK    = 0;
    uint8_t binlog_row_metadata_OK = 0;
    uint8_t gtid_mode_OK           = 0;

    if ( mysql_query( this->my_conn, "SHOW VARIABLES WHERE Variable_name IN ( 'binlog_format', 'binlog_row_image', 'binlog_row_metadata', 'gtid_mode' )" ) ) {

        repl_log_error_set( error_ptr, 1, mysql_errno( this->my_conn ), mysql_error( this->my_conn ) );
        mysql_close( this->my_conn );
        this->my_conn = NULL;
        __sync_bool_compare_and_swap( &(this->state), 
                                        (uint32_t)(mysql_rpl_listener_2::__State::POST_CREATED),
                                        (uint32_t)(mysql_rpl_listener_2::__State::ENDED ) );
        return;
    }
    res = mysql_store_result( my_conn );
    if ( !res ) {

        repl_log_error_set( error_ptr, 1, mysql_errno( this->my_conn ), mysql_error( this->my_conn ) );
        mysql_close( this->my_conn );
        this->my_conn = NULL;
        __sync_bool_compare_and_swap( &(this->state), 
                                        (uint32_t)(mysql_rpl_listener_2::__State::POST_CREATED),
                                        (uint32_t)(mysql_rpl_listener_2::__State::ENDED ) );        
        return;  
    }
    while( ( row = mysql_fetch_row(res) ) ) {
        
        if ( strcmp( row[0], "binlog_format" ) == 0 ) {

            if ( strcmp( row[1], "ROW" ) == 0 ) {

                binlog_format_OK = 1;
            }
        }

        if ( strcmp( row[0], "binlog_row_image" ) == 0 ) {

            if ( strcmp( row[1], "FULL" ) == 0 ) {

                binlog_row_image_OK = 1;
            }
        } 

        if ( strcmp( row[0], "binlog_row_metadata" ) == 0 ) {

            if ( strcmp( row[1], "FULL" ) == 0 ) {

                binlog_row_metadata_OK = 1;
            }
        } 

        if ( strcmp( row[0], "gtid_mode" ) == 0 ) {

            if ( strcmp( row[1], "OFF" ) == 0 ) {

                gtid_mode_OK = 1;
            }
        } 

    }
    mysql_free_result( res );

    if ( binlog_format_OK != 1 ) {

        repl_log_error_set( error_ptr, 0, __LINE__, "Server variable 'binlog_format' not setted to ROW" );
        mysql_close( this->my_conn );
        this->my_conn = NULL;
        __sync_bool_compare_and_swap( &(this->state), 
                                        (uint32_t)(mysql_rpl_listener_2::__State::POST_CREATED),
                                        (uint32_t)(mysql_rpl_listener_2::__State::ENDED ) );
        return;
    }

    if ( binlog_row_image_OK != 1 ) {

        repl_log_error_set( error_ptr, 0, __LINE__, "Server variable 'binlog_row_image' not setted to FULL" );
        mysql_close( this->my_conn );
        this->my_conn = NULL;
        __sync_bool_compare_and_swap( &(this->state), 
                                        (uint32_t)(mysql_rpl_listener_2::__State::POST_CREATED),
                                        (uint32_t)(mysql_rpl_listener_2::__State::ENDED ) );
        return;
    }

    if ( binlog_row_metadata_OK != 1 ) {

        repl_log_error_set( error_ptr, 0, __LINE__, "Server variable 'binlog_row_metadata' not setted to FULL" );
        mysql_close( this->my_conn );
        this->my_conn = NULL;
        __sync_bool_compare_and_swap( &(this->state), 
                                        (uint32_t)(mysql_rpl_listener_2::__State::POST_CREATED),
                                        (uint32_t)(mysql_rpl_listener_2::__State::ENDED ) );
        return;
    }

    if ( gtid_mode_OK != 1 ) {

        repl_log_error_set( error_ptr, 0, __LINE__, "Server variable 'gtid_mode' not setted to OFF" );
        mysql_close( this->my_conn );
        this->my_conn = NULL;
        __sync_bool_compare_and_swap( &(this->state), 
                                        (uint32_t)(mysql_rpl_listener_2::__State::POST_CREATED),
                                        (uint32_t)(mysql_rpl_listener_2::__State::ENDED ) );
        return;
    }

    //-------------



    if ( mysql_query( this->my_conn, "SET @master_binlog_checksum='ALL'" ) ) {

        repl_log_error_set( error_ptr, 1, mysql_errno( this->my_conn ), mysql_error( this->my_conn ) );
        mysql_close( this->my_conn );
        this->my_conn = NULL;
        __sync_bool_compare_and_swap( &(this->state), 
                                        (uint32_t)(mysql_rpl_listener_2::__State::POST_CREATED),
                                        (uint32_t)(mysql_rpl_listener_2::__State::ENDED ) );
        return;
    }
    // зададим heartbeat_period 0,333сек
    if ( mysql_query( this->my_conn, "SET @master_heartbeat_period=333000000" ) ) {

        repl_log_error_set( error_ptr, 1, mysql_errno( this->my_conn ), mysql_error( this->my_conn ) );
        mysql_close( this->my_conn );
        this->my_conn = NULL;
        __sync_bool_compare_and_swap( &(this->state), 
                                        (uint32_t)(mysql_rpl_listener_2::__State::POST_CREATED),
                                        (uint32_t)(mysql_rpl_listener_2::__State::ENDED ) );
        return;
    }


    MYSQL_RPL *my_rpl = &(this->MY_RPL_H);

    // если в конструкторе не была передана позиция,то
    if ( this->m_log_nm[0] == '\0' ) {

        this->position_was_setted = 0;

        if ( mysql_query( this->my_conn, "SHOW MASTER STATUS" ) ) {

            repl_log_error_set( error_ptr, 1, mysql_errno( this->my_conn ), mysql_error( this->my_conn ) );
            mysql_close( this->my_conn );
            this->my_conn = NULL;
            __sync_bool_compare_and_swap( &(this->state), 
                                            (uint32_t)(mysql_rpl_listener_2::__State::POST_CREATED),
                                            (uint32_t)(mysql_rpl_listener_2::__State::ENDED ) );
            return;
        }
        res = mysql_store_result( this->my_conn );
        if ( !res ) {

            repl_log_error_set( error_ptr, 1, mysql_errno( this->my_conn ), mysql_error( this->my_conn ) );
            __sync_bool_compare_and_swap( &(this->state), 
                                            (uint32_t)(mysql_rpl_listener_2::__State::POST_CREATED),
                                            (uint32_t)(mysql_rpl_listener_2::__State::ENDED ) );
            return;      
        }
        if ( !( row = mysql_fetch_row(res) ) ) {
            
            repl_log_error_set( error_ptr, 1, mysql_errno( this->my_conn ), mysql_error( this->my_conn ) );
            mysql_free_result( res );
            mysql_close( this->my_conn );
            this->my_conn = NULL;
            __sync_bool_compare_and_swap( &(this->state), 
                                            (uint32_t)(mysql_rpl_listener_2::__State::POST_CREATED),
                                            (uint32_t)(mysql_rpl_listener_2::__State::ENDED ) );
            return;
        }
        if ( ( row[0] == NULL ) || ( row[1] == NULL ) ) {

            repl_log_error_set( error_ptr, 0, __LINE__, "Could not find master log and pos. Master returned NULL!" );
            mysql_free_result( res );
            mysql_close( this->my_conn );
            this->my_conn = NULL;
            __sync_bool_compare_and_swap( &(this->state), 
                                            (uint32_t)(mysql_rpl_listener_2::__State::POST_CREATED),
                                            (uint32_t)(mysql_rpl_listener_2::__State::ENDED ) );
            return; 
        }
        
        // ----- SET RPL file name and position -----

        snprintf( this->m_log_nm, 512, "%s", row[0] );
        my_rpl[0].file_name = this->m_log_nm;
        my_rpl[0].file_name_length = strlen( my_rpl[0].file_name );
        char *strtoull_end_pos;
        my_rpl[0].start_position = (uint64_t)strtoull( row[1], &strtoull_end_pos, 10);
        
        // ----- SET RPL file name and position -----
        
        mysql_free_result( res );


    } else {

        this->position_was_setted = 1;

    }

    __sync_bool_compare_and_swap( &(this->state), 
                                    (uint32_t)(mysql_rpl_listener_2::__State::POST_CREATED),
                                    (uint32_t)(mysql_rpl_listener_2::__State::PREPARED ) );
    return;
}




void mysql_rpl_listener_2::start_replication( _repl_log_x_error *error_ptr ) {

    repl_log_x_error_clear( error_ptr );


    if ( !( __sync_bool_compare_and_swap( &(this->state), 
                                            (uint32_t)(mysql_rpl_listener_2::__State::PREPARED),
                                            (uint32_t)(mysql_rpl_listener_2::__State::POST_PREPARED ) ) ) ) {

        repl_log_error_set( error_ptr, 0, __LINE__, "start_replication() ERROR ORDER!!!" );
        return;
    }
    

    this->thread_arg.error_ptr = error_ptr;
    this->thread_arg.object    = ( void* )this;



    pthread_attr_t    attr;
    if ( pthread_attr_init( &attr ) > 0 ) {

        repl_log_error_set( error_ptr, 0, __LINE__, "Can not init attr for replication thread!!!" ); 
        __sync_bool_compare_and_swap( &(this->state), 
                                    (uint32_t)(mysql_rpl_listener_2::__State::POST_PREPARED),
                                    (uint32_t)(mysql_rpl_listener_2::__State::ENDED ) );
        return;
    }
    if ( pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE) ) {

        repl_log_error_set( error_ptr, 0, __LINE__, "Can not set PTHREAD_CREATE_JOINABLE on attr for replication thread!!!" ); 
        pthread_attr_destroy( &attr );
        __sync_bool_compare_and_swap( &(this->state), 
                                    (uint32_t)(mysql_rpl_listener_2::__State::POST_PREPARED),
                                    (uint32_t)(mysql_rpl_listener_2::__State::ENDED ) );
        return;   
    }

    this->need_stop = false;

    if ( pthread_create( &( this->repl_thread ),  &attr , this->process_replication_thread_fnc, &(this->thread_arg) ) != 0 ) {

        repl_log_error_set( error_ptr, 0, __LINE__, "Can not create replication thread!!!" ); 
        pthread_attr_destroy( &attr );
        __sync_bool_compare_and_swap( &(this->state), 
                                    (uint32_t)(mysql_rpl_listener_2::__State::POST_PREPARED),
                                    (uint32_t)(mysql_rpl_listener_2::__State::ENDED ) );
        return;       
    }
    pthread_attr_destroy( &attr );

    pthread_detach( this->repl_thread );

    if ( this->position_was_setted == 0 ) {

        __sync_bool_compare_and_swap( &(this->state), 
                                        (uint32_t)(mysql_rpl_listener_2::__State::POST_PREPARED),
                                        (uint32_t)(mysql_rpl_listener_2::__State::INPROCESS_1 ) );
    } else {

        __sync_bool_compare_and_swap( &(this->state), 
                                        (uint32_t)(mysql_rpl_listener_2::__State::POST_PREPARED),
                                        (uint32_t)(mysql_rpl_listener_2::__State::INPROCESS ) );
    }
    return;
}





int32_t mysql_rpl_listener_2::stop_replication ( void ) {

    
    if ( this->in_process_global() ) {

        usleep( 5000 );
        this->need_stop = true;

        // подождём 4 секунды и если репликационный поток не завершится,
        // прибъём его принудительно
        uint32_t i = 0;
        while ( this->in_process_global() ) {

            usleep( 100000 );
            i++;
            if ( i > 40 ) { break; }
        }
        
        if ( this->in_process_global() ) {

            int ret = pthread_cancel( this->repl_thread );
            usleep( 500000 );
            // TODO проверить успешность завершения pthread_cancel()
            // this->state может быть и не (uint32_t)(mysql_rpl_listener_2::__State::INPROCESS_1
            // this->state может быть и не (uint32_t)(mysql_rpl_listener_2::__State::INPROCESS
            __sync_bool_compare_and_swap( &(this->state), 
                                            (uint32_t)(mysql_rpl_listener_2::__State::INPROCESS_1),
                                            (uint32_t)(mysql_rpl_listener_2::__State::ENDED ) );

            __sync_bool_compare_and_swap( &(this->state), 
                                            (uint32_t)(mysql_rpl_listener_2::__State::INPROCESS),
                                            (uint32_t)(mysql_rpl_listener_2::__State::ENDED ) );                                
            
            mysql_binlog_close( this->my_conn, &(this->MY_RPL_H) );
            return ret;
        }

        pthread_join( this->repl_thread, NULL );
    }

    return 0;
}



void mysql_rpl_listener_2::get_tablemap_events( TBLMAP_EVNS *tb_m_ev_ptr, _repl_log_x_error *error_ptr ) {

    repl_log_x_error_clear( error_ptr );

    uint32_t st;
    st = __sync_fetch_and_add( &( this->state ), (uint32_t)0 );
    if ( st != mysql_rpl_listener_2::__State::ENDED ) {

        tb_m_ev_ptr[0] = NULL;
        repl_log_error_set( error_ptr, 0, __LINE__, "get_tablemap_events() ERROR ORDER!!!" );
        return;
    }

    try {
    
        tb_m_ev_ptr[0] = new std::vector< void* >;
    
    } catch ( std::exception& e ) {

        tb_m_ev_ptr[0] = NULL;
        repl_log_error_set( error_ptr, 0, __LINE__, e.what() );
        return;

    } catch ( ... ) {

        tb_m_ev_ptr[0] = NULL;
        repl_log_error_set( error_ptr, 0, __LINE__, "get_tablemap_events() new ERROR!!!" );
        return;
    }

    // EVENT_LEN_OFFSET = 9
    /*
            Binlog Event header
        --------------------------
        bytes |     value
        --------------------------
        4     |         timestamp
        1     |         event type
        4     |         server-id
        4     |         event-size

        if binlog-version > 1:

        4     |         log pos
        2     |         flags
    */

    for (auto it = this->glob_table_map_events.begin(); it != this->glob_table_map_events.end(); ++it) {

        char     *evbuff    = NULL;
        uint32_t evbuff_len = ( ( uint32_t* )( &( ( ( Table_map_log_event* )( (*it).second ) )->temp_buf[ EVENT_LEN_OFFSET ] ) ) )[0];

        evbuff = (char*)malloc( ( evbuff_len + 1 ) * sizeof(char) ); 
        if ( evbuff == NULL ) {

            mysql_rpl_listener_2::destroy_tablemap_events( tb_m_ev_ptr );

            repl_log_error_set( error_ptr, 0, __LINE__, "get_tablemap_events() can not allocate mem ERROR!!!" );
            return;
        }

        memcpy( evbuff, (( Table_map_log_event* )( (*it).second ))->temp_buf, evbuff_len );
        tb_m_ev_ptr[0][0].push_back( (void*)evbuff );
    }

    return;
}



void mysql_rpl_listener_2::destroy_tablemap_events( TBLMAP_EVNS *tb_m_ev_ptr ) {

    if ( ( tb_m_ev_ptr == NULL ) || ( tb_m_ev_ptr[0] == NULL ) ) {

        return;
    }

    for( uint32_t i=0; i<tb_m_ev_ptr[0][0].size(); i++ ){

        free( tb_m_ev_ptr[0][0].at(i) );
    }
    tb_m_ev_ptr[0][0].clear();
    delete tb_m_ev_ptr[0];
    tb_m_ev_ptr[0] = NULL;
}


void *mysql_rpl_listener_2::process_replication_thread_fnc ( void *arg_ptr ) {


    pthread_setcanceltype( PTHREAD_CANCEL_ASYNCHRONOUS, NULL );
    pthread_setcancelstate( PTHREAD_CANCEL_ENABLE, NULL );
    
    _repl_thread_arg  *arg_PTR     = ( _repl_thread_arg* )arg_ptr;
    _repl_log_x_error *error_ptr   = arg_PTR[0].error_ptr;
    mysql_rpl_listener_2 *OBJECT   = ( mysql_rpl_listener_2* )arg_PTR[0].object;

    repl_log_x_error_clear( error_ptr );

    // дождёмся пока родительский поток установит 
    // статус репликации в INPROCESS_1 или INPROCESS
    if ( OBJECT->position_was_setted == 0 ) {
    
        while ( __sync_fetch_and_add( &( OBJECT->state ), (uint32_t)0 ) != mysql_rpl_listener_2::__State::INPROCESS_1 ) {

            usleep(5000);
        }

    } else {

        while ( __sync_fetch_and_add( &( OBJECT->state ), (uint32_t)0 ) != mysql_rpl_listener_2::__State::INPROCESS ) {

            usleep(5000);
        }  
    }

    pthread_setcancelstate( PTHREAD_CANCEL_DISABLE, NULL );


    // https://dev.mysql.com/doc/refman/8.0/en/c-api-binary-log-functions.html


    char *event_buf = NULL;
    ulong event_len;
    

    try {
    
        OBJECT->glob_description_event = new Format_description_event( BINLOG_VERSION, server_version );
    
    }
    catch ( std::exception& e ) {

        OBJECT->glob_description_event = NULL;
        repl_log_error_set( error_ptr, 0, __LINE__, e.what() );
        mysql_close( OBJECT->my_conn );
        OBJECT->my_conn = NULL;
        if ( OBJECT->position_was_setted == 0 ) {
        __sync_bool_compare_and_swap( &(OBJECT->state), 
                            (uint32_t)(mysql_rpl_listener_2::__State::INPROCESS_1),
                            (uint32_t)(mysql_rpl_listener_2::__State::ENDED ) );
        } else {
        __sync_bool_compare_and_swap( &(OBJECT->state), 
                            (uint32_t)(mysql_rpl_listener_2::__State::INPROCESS),
                            (uint32_t)(mysql_rpl_listener_2::__State::ENDED ) );    
        }
        return NULL;
    } 
    catch ( ... ) {

        OBJECT->glob_description_event = nullptr;
        repl_log_error_set( error_ptr, 0, __LINE__, "exception catched!!!" );
        mysql_close( OBJECT->my_conn );
        OBJECT->my_conn = NULL;
        if ( OBJECT->position_was_setted == 0 ) {
        __sync_bool_compare_and_swap( &(OBJECT->state), 
                            (uint32_t)(mysql_rpl_listener_2::__State::INPROCESS_1),
                            (uint32_t)(mysql_rpl_listener_2::__State::ENDED ) );
        } else {
        __sync_bool_compare_and_swap( &(OBJECT->state), 
                            (uint32_t)(mysql_rpl_listener_2::__State::INPROCESS),
                            (uint32_t)(mysql_rpl_listener_2::__State::ENDED ) );    
        }
        return NULL;
    }

    /**
    To store Table_map_log_event information for decoding the following RowEvents correctly
    */
    OBJECT->glob_table_map_events.clear();


    
    if ( mysql_binlog_open( OBJECT->my_conn, &( OBJECT->MY_RPL_H ) ) ) {

        repl_log_error_set( error_ptr, 1, mysql_errno( OBJECT->my_conn ), mysql_error( OBJECT->my_conn ) );
        mysql_close( OBJECT->my_conn );
        OBJECT->my_conn = NULL;
        if ( OBJECT->position_was_setted == 0 ) {
        __sync_bool_compare_and_swap( &(OBJECT->state), 
                            (uint32_t)(mysql_rpl_listener_2::__State::INPROCESS_1),
                            (uint32_t)(mysql_rpl_listener_2::__State::ENDED ) );
        } else {
        __sync_bool_compare_and_swap( &(OBJECT->state), 
                            (uint32_t)(mysql_rpl_listener_2::__State::INPROCESS),
                            (uint32_t)(mysql_rpl_listener_2::__State::ENDED ) );    
        }
        return  NULL;
    }
    while ( 1 ) {  //read events until error or EOF
    

        pthread_setcancelstate( PTHREAD_CANCEL_ENABLE, NULL );
        
        int fetch_res = mysql_binlog_fetch( OBJECT->my_conn, &( OBJECT->MY_RPL_H ) );
        
        pthread_setcancelstate( PTHREAD_CANCEL_DISABLE, NULL );

        if ( fetch_res ) {

            repl_log_error_set( error_ptr, 1, mysql_errno( OBJECT->my_conn ), mysql_error( OBJECT->my_conn ) );
            break;
        }
        if ( OBJECT->MY_RPL_H.size == 0 ) {  // EOF

            repl_log_error_set( error_ptr, 0, __LINE__, "mysql_binlog_fetch() EOF received" );
            break;
        }

        Log_event_type type = (Log_event_type)OBJECT->MY_RPL_H.buffer[1 + EVENT_TYPE_OFFSET];
        Log_event *ev = NULL;
        
        /*
               Binlog Event header
           --------------------------
            bytes |     value
           --------------------------
            4     |         timestamp
            1     |         event type
            4     |         server-id
            4     |         event-size

            if binlog-version > 1:

            4     |         log pos
            2     |         flags
        */

        event_len = OBJECT->MY_RPL_H.size - 1;
        event_buf = (char *)my_malloc( key_memory_log_event, event_len + 1, MYF(0) );
        if ( !event_buf ) {

            repl_log_error_set( error_ptr, 0, __LINE__, "out of mem when allocate event_buf" );
            break;
        }
        memcpy( event_buf, OBJECT->MY_RPL_H.buffer + 1, event_len );


        if ( type == binary_log::HEARTBEAT_LOG_EVENT ) {

            
            // https://dev.mysql.com/doc/internals/en/binlog-event-header.html
            /*
                4              timestamp
                1              event type
                4              server-id
                4              event-size
                if binlog-version > 1:
                4              log pos
                2              flags
            */


            if ( OBJECT->glob_description_event != NULL ) {

                binary_log::Heartbeat_event *hbev;
                try {
                
                    hbev = new binary_log::Heartbeat_event( event_buf, ( Format_description_event* )( OBJECT->glob_description_event ) );
                
                }
                catch ( std::exception& e ) {

                    repl_log_error_set( error_ptr, 0, __LINE__, e.what() );
                    my_free(event_buf);
                    break;
                }
                catch ( ... ) {

                    repl_log_error_set( error_ptr, 0, __LINE__, "exception catched!!!" );
                    my_free(event_buf);
                    break;
                } 
                
                
                // если мы получили HEARTBEAT, то с большой долей вероятности все ивенты 
                // которые требуют TABLE_MAP уже прилетели к нам 
                // ( либо их не было ввобще в логе, либо были в логе ранее той позиции с которой мы начали )
                // потому можно дальнейшую послетовательность ивентов считать консистентной относительно
                // TABLE_MAP и ROW ивентов
                // потому установим сатус INPROCESS, если нужно
                __sync_bool_compare_and_swap( &(OBJECT->state), 
                            (uint32_t)(mysql_rpl_listener_2::__State::INPROCESS_1),
                            (uint32_t)(mysql_rpl_listener_2::__State::INPROCESS ) );


                if ( OBJECT->on_heartbeat != NULL ) {

                    _repl_log_x_heartbeat X_HRB_EV;
                    X_HRB_EV.when = hbev->header()->when;

                    OBJECT->on_heartbeat( &X_HRB_EV, OBJECT->on_heartbeat_param_ptr );
                }
                
                // если мы поймали HEARTBEAT_LOG_EVENT, то он нам нужен только для 
                // апдейта last_time и более ничего делать не нужно потому 
                // чистим всё и уходим на следующую итерацию
                delete hbev;

            } else {

                repl_log_error_set( error_ptr, 0, __LINE__, "HEARTBEAT_LOG_EVENT without FORMAT_DESCRIPTION_EVENT!!!" );
                my_free(event_buf);
                break;
            }

            my_free(event_buf);

            if ( OBJECT->need_stop == true ) {

                break;
            }               
            continue;
        }

        Binlog_read_error read_error;
        try {

            read_error = binlog_event_deserialize(
                                reinterpret_cast<unsigned char *>(event_buf), 
                                event_len,
                                ( Format_description_event* )( OBJECT->glob_description_event ), 
                                true /* opt_verify_binlog_checksum */, 
                                &ev );
        }
        catch ( std::exception& e ) {

            repl_log_error_set( error_ptr, 0, __LINE__, e.what() );
            my_free(event_buf);
            break;
        }
        catch ( ... ) {

            repl_log_error_set( error_ptr, 0, __LINE__, "exception catched!!!" );
            my_free(event_buf);
            break;
        }                                     
        if ( read_error.has_error() ) {
            
            repl_log_error_set( error_ptr, 0, __LINE__, read_error.get_str() );
            my_free(event_buf);
            break;
        }
        ev->register_temp_buf(event_buf);

        
        if ( type == binary_log::FORMAT_DESCRIPTION_EVENT ) {
        
            if ( OBJECT->glob_description_event != NULL ) {
            
                delete ( Format_description_event* )( OBJECT->glob_description_event );
                OBJECT->glob_description_event = NULL;
            }
            OBJECT->glob_description_event = dynamic_cast<Format_description_event*>(ev);

            /*
                This could be an fake Format_description_log_event that server
                (5.0+) automatically sends to a slave on connect, before sending
                a first event at the requested position.  If this is the case,
                don't increment old_off. Real Format_description_log_event always
                starts from BIN_LOG_HEADER_SIZE position.
            */
            if ( OBJECT->MY_RPL_H.start_position != BIN_LOG_HEADER_SIZE ) {
            
                OBJECT->MY_RPL_H.size  = 1;
                event_len = 0;
            }

        }
        if ( type == binary_log::TABLE_MAP_EVENT ) {

            Table_map_log_event   *tmp_ev;
            tmp_ev = dynamic_cast<Table_map_log_event*>(ev);

            uint64_t tmp_key;
            tmp_key = tmp_ev->get_table_id().id();


            std::map<uint64_t,void*>::iterator it;
            it = OBJECT->glob_table_map_events.find( tmp_key );
            if ( it != OBJECT->glob_table_map_events.end() ) {

                if ( (*it).second != NULL ) {
                
                    delete ( Table_map_log_event* )( (*it).second );
                }
                OBJECT->glob_table_map_events.erase(it);                
            }
            OBJECT->glob_table_map_events.insert( std::pair<uint64_t,void*>( tmp_key, (void*)tmp_ev ) );

        }
        if ( type == binary_log::ROTATE_EVENT ) {

            Rotate_log_event *rev =  (Rotate_log_event *)ev;

            //binary_log::Rotate_event *rev = ( binary_log::Rotate_event* )ev;
                
            // если это настоящий ROTATE_EVENT, то 
            // имя файла будет отличным от текущего
            // в противном же случае это фейковый ROTATE_EVENT и нам нужно ничего не делать
            if ( (rev->ident_len != OBJECT->MY_RPL_H.file_name_length) ||
                    memcmp( rev->new_log_ident, OBJECT->MY_RPL_H.file_name, OBJECT->MY_RPL_H.file_name_length ) ) {

                memset( OBJECT->m_log_nm, 0, 512 );
                snprintf( OBJECT->m_log_nm, ( ( rev->ident_len + 1 ) > 512 ) ? 512 : ( rev->ident_len + 1 ) , "%s", rev->new_log_ident );
                OBJECT->MY_RPL_H.file_name_length = strlen( OBJECT->MY_RPL_H.file_name );
                OBJECT->MY_RPL_H.start_position = 0;

                OBJECT->MY_RPL_H.size  = 5;
                event_len = 0; 

            } else {

                OBJECT->MY_RPL_H.size  = 1;
                event_len = 0;
            }

        }


        // обрабатываем EVENT 
        OBJECT->process_event( ev, error_ptr );


        // The event's deletion has been handled in process_event. To prevent
        // that Destroy_log_event_guard deletes it again, we have to set it to
        // NULL
        ev = NULL;

        if ( error_ptr[0].is_error != 0 ) {

            break;
        }


        OBJECT->MY_RPL_H.start_position += OBJECT->MY_RPL_H.size - 1;

        // printf( "%s %lu  ** %s\n\n", OBJECT->MY_RPL_H.file_name, 
        // (uint64_t)OBJECT->MY_RPL_H.start_position , Log_event::get_type_str( type ) );

        // если была команда STOP то завершаем репликационную петлю
        if ( OBJECT->need_stop == true ) {

            break;
        }  

    }

    mysql_binlog_close( OBJECT->my_conn, &(OBJECT->MY_RPL_H) );
    mysql_close( OBJECT->my_conn );
    OBJECT->my_conn = NULL;

        
    __sync_bool_compare_and_swap( &(OBJECT->state), 
                            (uint32_t)(mysql_rpl_listener_2::__State::INPROCESS_1),
                            (uint32_t)(mysql_rpl_listener_2::__State::ENDED ) );
    __sync_bool_compare_and_swap( &(OBJECT->state), 
                            (uint32_t)(mysql_rpl_listener_2::__State::INPROCESS),
                            (uint32_t)(mysql_rpl_listener_2::__State::ENDED ) );                        
    return NULL;
}



/**
*  функция обрабатывающая event
    по сути мы обрабатываем только ROWS_EVENTы различных видов
*/
void mysql_rpl_listener_2::process_event (  void *ev_x, _repl_log_x_error *error_ptr ) {
    
    repl_log_x_error_clear( error_ptr );

    Log_event *ev = ( Log_event* )( ev_x );

    // Table_map_log_event *tbl_ev = ( Table_map_log_event* )( this->glob_table_map_event );
    
    Log_event_type ev_type = ev->get_type_code();


    // если у нас ивент любой, кроме ROWS, XID_EVENT либо QUERY_EVENT, то приравниваем его к HEARTBEAT ивенту
    // QUERY_EVENT, если это не относится к началу или окончанию транзакции,
    // также приравняем к HEARTBEAT
    if ( !( 
            (  ev_type == binary_log::WRITE_ROWS_EVENT ) ||
            (  ev_type == binary_log::DELETE_ROWS_EVENT ) ||
            (  ev_type == binary_log::UPDATE_ROWS_EVENT ) ||
            (  ev_type == binary_log::WRITE_ROWS_EVENT_V1 ) ||
            (  ev_type == binary_log::DELETE_ROWS_EVENT_V1 ) ||
            (  ev_type == binary_log::UPDATE_ROWS_EVENT_V1 ) ||
            (  ev_type == binary_log::PARTIAL_UPDATE_ROWS_EVENT ) ||
            (  ev_type == binary_log::QUERY_EVENT ) ||
            (  ev_type == binary_log::XID_EVENT )
        ) ) {
            
        
        if ( __sync_fetch_and_add( &( this->state ), (uint32_t)0 ) == mysql_rpl_listener_2::__State::INPROCESS ) {
        
            if ( this->on_heartbeat != NULL ) {

                _repl_log_x_heartbeat X_HRB_EV;
                X_HRB_EV.when = ev->common_header->when;

                this->on_heartbeat( &X_HRB_EV, this->on_heartbeat_param_ptr );
            }
        }
    }


    switch ( ev_type ) {

        // QUERY_EVENT, если это не относится к началу или окончанию транзакции,
        // приравняем к HEARTBEAT
        case binary_log::QUERY_EVENT: {
            
            uint8_t convert_to_heartbeat = 1;

            Query_log_event *qle = (Query_log_event *)ev;
                
            char *query_str = NULL;
            try {
                    
                // проверим не является ли это началом транзакции ( BEGIN )
                // или окончанием тразакции ( COMMIT )
                query_str = (char*)malloc( qle->q_len + 8 );
                if ( query_str == NULL ) {
                
                    throw std::runtime_error( "out of mem!!!" );
                    return;
                }
                memset( query_str, 0, qle->q_len + 8 );
                memcpy( query_str, qle->query, qle->q_len );
                if ( 
                    ( strcmp( "BEGIN", query_str ) == 0 ) || 
                    ( strcmp( "XA START", query_str ) == 0  ) ) {
                
                    __sync_bool_compare_and_swap( &(this->state), 
                                        (uint32_t)(mysql_rpl_listener_2::__State::INPROCESS_1),
                                        (uint32_t)(mysql_rpl_listener_2::__State::INPROCESS ) );

                    ;;
                    // TRANSACTION!!!!
                    this->in_transaction = 1;

                    // здесь будет сконверчено в HEARTBEAT
                    // а если мы встретим в дальнейшем таблицы, которые мы отслеживаем внутри этой транзакции,
                    // то мы вызовем this->on_transaction со временем первого ROW_EVENTа
                }
                if ( 
                    /* ( strcmp( "COMMIT", query_str ) == 0 ) || */
                    ( strcmp( "COMMIT", query_str ) == 0 ) ) {

                    // TRANSACTION!!!!
                    // > 1 потому, что мы, изначально встретив query BEGIN ставим в 1,
                    // а потом, если внутри транзакции встречаем одну из отслеживаемых таблиц,.
                    // то ставим в 2
                    if ( this->in_transaction > 1 ) {
                    
                        if ( __sync_fetch_and_add( &( this->state ), (uint32_t)0 ) == mysql_rpl_listener_2::__State::INPROCESS ) {
                        
                            if ( this->on_transaction != NULL ) {
                        
                                _repl_log_x_transaction X_TR_EV;
                                X_TR_EV.when = qle->common_header->when;
                                X_TR_EV.transaction_event_type = (uint8_t)( TRANSACTION_EV_TYPE_COMMIT );

                                this->on_transaction( &X_TR_EV, this->on_transaction_param_ptr );

                                convert_to_heartbeat = 0;
                            }
                        }
                    }
                    this->in_transaction = 0;

                    __sync_bool_compare_and_swap( &(this->state), 
                            (uint32_t)(mysql_rpl_listener_2::__State::INPROCESS_1),
                            (uint32_t)(mysql_rpl_listener_2::__State::INPROCESS ) );
                }

                if ( convert_to_heartbeat == 0 ) {

                    _repl_log_x_heartbeat X_HRB_EV;
                    X_HRB_EV.when = qle->common_header->when;

                    this->on_heartbeat( &X_HRB_EV, this->on_heartbeat_param_ptr );
                }
            
            }
            catch ( std::exception& e ) {

                if ( query_str != NULL ) { free( query_str ); }
                repl_log_error_set( error_ptr, 0, __LINE__, e.what() );
                delete ev;
                return;
            }
            catch ( ... ) {

                if ( query_str != NULL ) { free( query_str ); }
                repl_log_error_set( error_ptr, 0, __LINE__, "exception catched!!!" );
                delete ev;
                return;
            }
            if ( query_str != NULL ) { free( query_str ); }

            break;
        }

        case binary_log::INTVAR_EVENT: {
            ;;
            break;
        }

        case binary_log::RAND_EVENT: {
            ;;
            break;
        }

        case binary_log::USER_VAR_EVENT: {
            ;;
            break;
        }
        case binary_log::APPEND_BLOCK_EVENT: {
            ;;
            break;
        }  
        case binary_log::FORMAT_DESCRIPTION_EVENT: {

            // для предотвращения удаления eventа делаем return
            // FORMAT_DESCRIPTION_EVENT мы сохраняем для дальнейшей расшифровки всех последующих
            // eventов вплоть до следующего FORMAT_DESCRIPTION_EVENT
            return;
            break;
        }
        case binary_log::BEGIN_LOAD_QUERY_EVENT: {
            ;;
            break;
        }

        case binary_log::EXECUTE_LOAD_QUERY_EVENT: {
            ;;
            break;
        }

        case binary_log::TABLE_MAP_EVENT: {

            // для предотвращения удаления eventа делаем return
            // TABLE_MAP_EVENT нам нужен для дальнейших ROW eventов 
            return;
            break;
        }


        case binary_log::ROWS_QUERY_LOG_EVENT: {
            ;;
            break;
        }
        case binary_log::WRITE_ROWS_EVENT:
        case binary_log::DELETE_ROWS_EVENT:
        case binary_log::UPDATE_ROWS_EVENT:
        case binary_log::WRITE_ROWS_EVENT_V1:
        case binary_log::UPDATE_ROWS_EVENT_V1:
        case binary_log::DELETE_ROWS_EVENT_V1:
        case binary_log::PARTIAL_UPDATE_ROWS_EVENT: {

            Rows_log_event_2 *new_ev = ( Rows_log_event_2 *)ev;

            if ( __sync_fetch_and_add( &( this->state ), (uint32_t)0 ) == mysql_rpl_listener_2::__State::INPROCESS ) {

                Table_map_log_event *tbl_ev = NULL;

                std::map<uint64_t,void*>::iterator it;
                it = this->glob_table_map_events.find( (uint64_t)( new_ev->get_table_id().id() ) );
                if ( it != this->glob_table_map_events.end() ) {

                    tbl_ev = ( Table_map_log_event* )( (*it).second );
                }

                if ( 
                        ( tbl_ev != NULL ) && 
                        ( tbl_ev->get_table_id().id() == new_ev->get_table_id().id() ) 
                ) {

                        _tbl_info  *cached_tbl_info = NULL;
                        for ( uint32_t q=0; q<this->tables.size(); q++ ) {

                            if ( ( strcmp( tbl_ev->get_db_name(),    this->tables.at( q ).db_name  ) == 0 ) && 
                                ( strcmp( tbl_ev->get_table_name(), this->tables.at( q ).tbl_name ) == 0 ) ) {

                                cached_tbl_info = &( this->tables.at( q ).info );
                            }
                        }

                        // проверим отслеживается ли нами таблица    
                        if ( cached_tbl_info != NULL ) {


                            // TRANSACTION!!!!
                            // если мы внутри транзакции, и мы встретили отслеживаемую нами таблицу,
                            // то установим индикатор транзакции в 2 и вызовем функцию обработки начала транзакции
                            if ( this->in_transaction == 1 ) {

                                this->in_transaction = 2;
                                if ( this->on_transaction != NULL ) {

                                    _repl_log_x_transaction X_TR_EV;
                                    X_TR_EV.when = new_ev->common_header->when;
                                    X_TR_EV.transaction_event_type = (uint8_t)( TRANSACTION_EV_TYPE_START );
                        
                                    this->on_transaction( &X_TR_EV, this->on_transaction_param_ptr );
                                }
                            }

                            _repl_log_x_rows X_ROWS;

                            int retval = 0;
                            try {
                            
                                if ( cached_tbl_info[0].columns == NULL ) {
                                
                                    cached_tbl_info[0].columns = ( _col_info* )malloc( tbl_ev->m_colcnt * sizeof( _col_info ) );
                                    if ( cached_tbl_info[0].columns == NULL ) {

                                        repl_log_error_set( error_ptr, 0, __LINE__, "mem allocation error when store columns info!!!" );
                                        delete ev;
                                        return; 
                                    }

                                } else {

                                    if ( cached_tbl_info[0].col_count >= tbl_ev->m_colcnt ) {

                                        ;;
                                    } else {

                                        free( cached_tbl_info[0].columns );
                                        cached_tbl_info[0].columns = ( _col_info* )malloc( tbl_ev->m_colcnt * sizeof( _col_info ) );
                                        if ( cached_tbl_info[0].columns == NULL ) {

                                            repl_log_error_set( error_ptr, 0, __LINE__, "mem allocation error when store columns info!!!" );
                                            delete ev;
                                            return; 
                                        }
                                    }
                                }
                                cached_tbl_info[0].col_count = tbl_ev->m_colcnt;
                                memset( cached_tbl_info[0].columns, 0, sizeof(_col_info) * cached_tbl_info[0].col_count );
                                
                                
                                if ( tbl_ev->m_optional_metadata_len == 0 ) {

                                    repl_log_error_set( error_ptr, 0, __LINE__, "(1) optional metadata is not valid!!!"
                                                                                " set server variable binlog_row_metadata=FULL" );
                                    delete ev;
                                    return; 
                                }


                                binary_log::Table_map_event::Optional_metadata_fields OP_MD_FIELDS( tbl_ev->m_optional_metadata, 
                                                                                                    tbl_ev->m_optional_metadata_len );

                                if ( !OP_MD_FIELDS.is_valid ) {

                                    repl_log_error_set( error_ptr, 0, __LINE__, "(2) optional metadata is not valid!!!"
                                                                                " set server variable binlog_row_metadata=FULL" );
                                    delete ev;
                                    return; 
                                }
                                
                                if ( OP_MD_FIELDS.m_column_name.size() != cached_tbl_info[0].col_count ) {       
                                    
                                    char buff[4096];
                                    snprintf( buff, 4096, "ERROR opt metadata parsing ( may be binlog_row_metadata is not set to FULL ) " );
                                    repl_log_error_set( error_ptr, 0, __LINE__, buff );
                                    delete ev;
                                    return; 
                                }
                                                                                    
                                uint32_t m_signedness_current = 0;
                                for ( uint32_t c_i=0; c_i<cached_tbl_info[0].col_count; c_i++ ) {

                                    cached_tbl_info[0].columns[ c_i ].is_unsigned = 0;
                                    if ( mysql_rpl_listener_2::is_numeric_type( tbl_ev->m_coltype[ c_i ] ) ) {

                                        if ( m_signedness_current >= OP_MD_FIELDS.m_signedness.size() ) {

                                            char buff[4096];
                                            snprintf( buff, 4096, "ERROR opt metadata SIGNEDESS parsing !!!" );
                                            repl_log_error_set( error_ptr, 0, __LINE__, buff );
                                            delete ev;
                                            return; 
                                        }
                                        if ( OP_MD_FIELDS.m_signedness.at( m_signedness_current ) ) {

                                            cached_tbl_info[0].columns[ c_i ].is_unsigned = 1;
                                        }
                                        m_signedness_current++;
                                    }

                                    snprintf( cached_tbl_info[0].columns[ c_i ].name, 128, "%s", OP_MD_FIELDS.m_column_name.at( c_i ).c_str() ); 
                                }


                                for ( uint32_t c_i=0; c_i<cached_tbl_info[0].col_count; c_i++ ) {
                                
                                    cached_tbl_info[0].columns[ c_i ].type = (uint8_t)( tbl_ev->m_coltype[ c_i ] );
                                }


                                mysql_rpl_listener_2::repl_log_x_rows_init( &X_ROWS, this, new_ev, cached_tbl_info, tbl_ev );

                                retval = new_ev->get_rows( tbl_ev, &X_ROWS, cached_tbl_info );

                            }
                            catch ( std::exception& e ) {

                                mysql_rpl_listener_2::repl_log_x_rows_clear( &X_ROWS );
                                repl_log_error_set( error_ptr, 0, __LINE__, e.what() );
                                delete ev;
                                return;
                            }
                            catch ( ... ) {

                                mysql_rpl_listener_2::repl_log_x_rows_clear( &X_ROWS );
                                repl_log_error_set( error_ptr, 0, __LINE__, "exception catched!!!" );
                                delete ev;
                                return;
                            }
                            if ( retval != OK_CONTINUE ) {

                                mysql_rpl_listener_2::repl_log_x_rows_clear( &X_ROWS );
                                repl_log_error_set( error_ptr, 0, __LINE__, new_ev->get_error_desc() );
                                delete ev;
                                return;
                            }

                            if ( this->on_row != NULL ) {

                                this->on_row( &X_ROWS, this->on_row_param_ptr );
                            }

                            mysql_rpl_listener_2::repl_log_x_rows_clear( &X_ROWS );
                        }

                } else {

                    // что-то не то с потоком репликации
                    // нам насыпался ROWS_EVENT, но перед этим обязан быть TABLE_MAP_EVENT,
                    // которого не было
                    repl_log_error_set( error_ptr, 0, __LINE__, "rows event without table_map event" );
                    delete ev;
                    return;
                }
            }

            break;
        }
        case binary_log::ANONYMOUS_GTID_LOG_EVENT:
        case binary_log::GTID_LOG_EVENT: {
        
            __sync_bool_compare_and_swap( &(this->state), 
                                        (uint32_t)(mysql_rpl_listener_2::__State::INPROCESS_1),
                                        (uint32_t)(mysql_rpl_listener_2::__State::INPROCESS ) );
            break;
        }
        case binary_log::XID_EVENT: {

            // по сути может представлять как Xid_event так и XA_prepare_event , ( хз - может это и не так!!! )
            // но для нас не важно какого именно типа транзакция подтверждается.
            ;;
            Xid_log_event *xidle = (Xid_log_event *)ev;
            // -- xidle->xid;

            // TRANSACTION!!!!
            // > 1 потому, что мы, изначально встретив query BEGIN ставим в 1,
            // а потом, если внутри транзакции встречаем одну из отслеживаемых таблиц,.
            // то ставим в 2
            if ( this->in_transaction > 1 ) {

                if ( __sync_fetch_and_add( &( this->state ), (uint32_t)0 ) == mysql_rpl_listener_2::__State::INPROCESS ) {
                
                    if ( this->on_transaction != NULL ) {

                        _repl_log_x_transaction X_TR_EV;
                        X_TR_EV.when = xidle->common_header->when;
                        X_TR_EV.transaction_event_type = (uint8_t)( TRANSACTION_EV_TYPE_COMMIT );
                        
                        this->on_transaction( &X_TR_EV, this->on_transaction_param_ptr );
                    }
                }
            }
            this->in_transaction = 0;

            __sync_bool_compare_and_swap( &(this->state), 
                                    (uint32_t)(mysql_rpl_listener_2::__State::INPROCESS_1),
                                    (uint32_t)(mysql_rpl_listener_2::__State::INPROCESS ) );

            break;
        }
        case binary_log::XA_PREPARE_LOG_EVENT: {

            // XA_prepare_log_event 
            ;;
            XA_prepare_log_event *xaple = (XA_prepare_log_event *)ev;
            // -- xaple->xid;

            // TRANSACTION!!!!
            // > 1 потому, что мы, изначально встретив query BEGIN ставим в 1,
            // а потом, если внутри транзакции встречаем одну из отслеживаемых таблиц,.
            // то ставим в 2
            if ( this->in_transaction > 1 ) {

                if ( __sync_fetch_and_add( &( this->state ), (uint32_t)0 ) == mysql_rpl_listener_2::__State::INPROCESS ) {
                
                    if ( this->on_transaction != NULL ) {

                        _repl_log_x_transaction X_TR_EV;
                        X_TR_EV.when = xaple->common_header->when;
                        X_TR_EV.transaction_event_type = (uint8_t)( TRANSACTION_EV_TYPE_COMMIT );
                        
                        this->on_transaction( &X_TR_EV, this->on_transaction_param_ptr );
                    }
                }
            }
            this->in_transaction = 0;

            __sync_bool_compare_and_swap( &(this->state), 
                                    (uint32_t)(mysql_rpl_listener_2::__State::INPROCESS_1),
                                    (uint32_t)(mysql_rpl_listener_2::__State::INPROCESS ) );

            break;
        }
        case binary_log::PREVIOUS_GTIDS_LOG_EVENT: {
            ;;
            break;
        }
        case binary_log::STOP_EVENT: {
            ;;
            break;  
        }
        default: {
            ;;
            break;
        }

    }

    // удаляем отработанный event 
    // на будущее нам нужны только eventы типов  FORMAT_DESCRIPTION_EVENT и TABLE_MAP_EVENT
    // по которым мы делаем return не доходя сюда
    delete ev;
    return;
}


void mysql_rpl_listener_2::repl_log_x_rows_init( _repl_log_x_rows     *x_rows_ptr, 
                                                 mysql_rpl_listener_2 *obj,
                                                 Rows_log_event_2     *new_ev,
                                                 _tbl_info            *info_ptr,
                                                 Table_map_log_event  *map ) {

    memset( x_rows_ptr, 0, sizeof( _repl_log_x_rows ) );

    if ( map == NULL ) {

        throw std::runtime_error( "table_map event is NULL!!!" );
        return;
    }

    x_rows_ptr[0].display_blobs.BLOBS_DISPLAY   = ( obj->BLOBS_DISPLAY > 0 )   ? 1 : 0;
    x_rows_ptr[0].display_blobs.GEOMETRY_AS_HEX = ( obj->GEOMETRY_AS_HEX > 0 ) ? 1 : 0;
    x_rows_ptr[0].display_blobs.BLOB_AS_HEX     = ( obj->BLOB_AS_HEX > 0 )     ? 1 : 0;

    x_rows_ptr[0].when      = new_ev->header()->when;
    x_rows_ptr[0].col_count = (uint32_t)map->m_colcnt;
    x_rows_ptr[0].col_names = (char**)malloc( (size_t)( sizeof( char* ) * x_rows_ptr[0].col_count ) );
    x_rows_ptr[0].col_types = ( uint8_t* )malloc( (size_t)( sizeof( uint8_t ) * x_rows_ptr[0].col_count ) );
    if ( ( x_rows_ptr[0].col_names == NULL ) || ( x_rows_ptr[0].col_types == NULL ) ) {

        if ( x_rows_ptr[0].col_names != NULL ) { free( x_rows_ptr[0].col_names ); }
        if ( x_rows_ptr[0].col_types != NULL ) { free( x_rows_ptr[0].col_types ); }

        throw std::runtime_error( "can not allocate memory for columns info!!!" );
        return;
    }
    for ( uint32_t i=0; i<x_rows_ptr[0].col_count; i++ ) {

        // здесь мы присваиваем выделенную заранее память которую не нужно потом удалять
        x_rows_ptr[0].col_names[i] = info_ptr->columns[i].name;//NULL;

        x_rows_ptr[0].col_types[i] = info_ptr->columns[i].type;
    }
    x_rows_ptr[0].rows_count = 0;
    x_rows_ptr[0].rows       = NULL;
    x_rows_ptr[0].rows_after = NULL;
    x_rows_ptr[0].tbl_id     = (uint64_t)( new_ev->get_table_id().id() );
    snprintf( x_rows_ptr[0].tbl_name, MAX_TABLE_NAME_LEN,    "%s", map->get_table_name() );
    snprintf( x_rows_ptr[0].db_name , MAX_DATABASE_NAME_LEN, "%s", map->get_db_name() );

    return; 
}



void mysql_rpl_listener_2::repl_log_x_rows_clear( _repl_log_x_rows *r_ptr ) {

    if ( r_ptr[0].col_names != NULL ) {

        free( r_ptr[0].col_names );
    }
    if ( r_ptr[0].col_types != NULL ) {

        free( r_ptr[0].col_types );
    }

    if ( r_ptr[0].rows != NULL ) {

        free( r_ptr[0].rows );
    }

    if ( r_ptr[0].rows_after != NULL ) {

        free( r_ptr[0].rows_after );
    }


    if ( r_ptr[0].rows_L2 != NULL ) {

        for ( uint32_t row_i=0; row_i<( r_ptr[0].rows_count * r_ptr[0].col_count ); row_i++ ) {

            if ( r_ptr[0].rows_L2[ row_i ].val != NULL ) {

                free( r_ptr[0].rows_L2[ row_i ].val );
            }
        }
        free( r_ptr[0].rows_L2 );
    }

    if ( r_ptr[0].rows_after_L2 != NULL ) {

        for ( uint32_t row_i=0; row_i<( r_ptr[0].rows_count * r_ptr[0].col_count ); row_i++ ) {

            if ( r_ptr[0].rows_after_L2[ row_i ].val != NULL ) {

                free( r_ptr[0].rows_after_L2[ row_i ].val );
            }
        }
        free( r_ptr[0].rows_after_L2 );
    }

    memset( r_ptr, 0, sizeof( _repl_log_x_rows ) );
}



bool mysql_rpl_listener_2::is_numeric_type(uint type) {
  switch (type) {
    case MYSQL_TYPE_TINY:
    case MYSQL_TYPE_SHORT:
    case MYSQL_TYPE_INT24:
    case MYSQL_TYPE_LONG:
    case MYSQL_TYPE_LONGLONG:
    case MYSQL_TYPE_NEWDECIMAL:
    case MYSQL_TYPE_FLOAT:
    case MYSQL_TYPE_DOUBLE:
      return true;
    default:
      return false;
  }
  return false;
}


bool mysql_rpl_listener_2::is_character_type(uint type) {
  switch (type) {
    case MYSQL_TYPE_STRING:
    case MYSQL_TYPE_VAR_STRING:
    case MYSQL_TYPE_VARCHAR:
    case MYSQL_TYPE_BLOB:
      return true;
    default:
      return false;
  }
}

bool mysql_rpl_listener_2::is_enum_or_set_type(uint type) {
  return type == MYSQL_TYPE_ENUM || type == MYSQL_TYPE_SET;
}






//-------------------------------------------------------------------------------------------------

/**
  заглушки для корректной работы кода MySQL 
*/
void error_or_warning(const char *format, va_list args, const char *msg) {

    return;
}
void error(const char *format, ...) {

    return ;
}
void sql_print_error(const char *format, ...) {

    return;
}
void warning(const char *format, ...) {

    return;
}



