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


#ifndef __REPL_LOG_LISTENER_2
#define __REPL_LOG_LISTENER_2



#include <vector>
#include <map>
#include "sql_common.h"
#include "../mysql_rpl_listener_row.h"
#include "../rows_log_event_2/rows_log_event_2.h"
#include "mysql_rpl_tbl_info.h"



// typedef std::map < uint64_t , void* >* TBLMAP_EVNS;
typedef std::vector< void* >* TBLMAP_EVNS;



typedef struct __repl_thread_arg {

    _repl_log_x_error     *error_ptr;
    void                  *object;

} _repl_thread_arg;


typedef struct __listening_tables {

    char       db_name[  MAX_DATABASE_NAME_LEN + 1 ];
    char       tbl_name[ MAX_TABLE_NAME_LEN + 1 ];
    _tbl_info  info;

} _listening_tables;




class mysql_rpl_listener_2 {
    
    private:

    uint8_t in_transaction;

    uint8_t position_was_setted;

    MYSQL        *my_conn;
    MYSQL_RPL    MY_RPL_H;
    
    void (*on_row)( _repl_log_x_rows*, void* ); 
    void *on_row_param_ptr;

    void (*on_heartbeat)( _repl_log_x_heartbeat*, void* );
    void *on_heartbeat_param_ptr;

    void (*on_transaction)( _repl_log_x_transaction*, void* );
    void *on_transaction_param_ptr;
    
    std::vector< _listening_tables > tables;

    pthread_t            repl_thread;
    _repl_thread_arg     thread_arg;
    uint32_t             state;

    char m_log_nm[512]; // имя файла журнала на мастере
    bool need_stop;

    char      my_host[256];
    char      my_user[256];
    char      my_pass[256];
    uint32_t  my_port;

    uint8_t  BLOBS_DISPLAY;
    uint8_t  GEOMETRY_AS_HEX;
    uint8_t  BLOB_AS_HEX;


    /**
     For storing information of the Format_description_event of the currently
    active binlog. it will be changed each time a new Format_description_event is
    found in the binlog.
    */
    //Format_description_event *glob_description_event;
    void *glob_description_event;

    /**
    To store Table_map_log_events information for decoding the following RowEvents correctly
    */
    std::map < uint64_t , void* > glob_table_map_events;


    
    void tables_clear( void );

    public:

    // изменять значения CREATED, INPROCESS_1 и INPROCESS надо в соответствии
    // с тем как определены REPL_PROC_STATUS_OFF, REPL_PROC_STATUS_PHASE_1 и REPL_PROC_STATUS_PHASE_2
    // в файле ../mysql_rpl_listener.h
    static enum __State {

        CREATED           = 0,
        POST_CREATED      = 10,
        PREPARED          = 40,
        POST_PREPARED     = 50,
        INPROCESS_1       = 80,
        INPROCESS         = 90,
        ENDED             = 100,

    } _State;

    

    void get_last_sucessful_position( char *file, int file_maxlen, uint64_t *pos );

    int32_t in_process( void );
    
    int32_t in_process_global( void );


    ~mysql_rpl_listener_2();

    mysql_rpl_listener_2 ( void (*on_rpl_row)( _repl_log_x_rows*, void* ), 
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
                           char    *master_file_name, 
                           uint64_t master_file_pos,
                           TBLMAP_EVNS *tblmaps_collection_ptr );

    int32_t table_add ( const char* db_name, const char* tbl_name );                       

    /*
       метод должен быть вызван после добавления таблиц
       по которым отслеживаются изменения ( mysql_rpl_listener_2::table_add() ),
       в противном случае метод завершится ошибкой 
    */
    void prepare_replication( _repl_log_x_error *error_ptr );

    void start_replication( _repl_log_x_error *error_ptr );

    int32_t stop_replication ( void );
    

    void get_tablemap_events( TBLMAP_EVNS *tb_m_ev_ptr, _repl_log_x_error *error_ptr );
    static void destroy_tablemap_events( TBLMAP_EVNS *tb_m_ev_ptr );
    


    static void *process_replication_thread_fnc ( void *arg_ptr );



    /**
    *  функция обрабатывающая event
       по сути мы обрабатываем только ROWS_EVENTы различных видов
    */
    void process_event ( mysql_rpl_listener_2 *OBJECT, void *ev_x, _repl_log_x_error *error_ptr );

    static void repl_log_x_rows_init( _repl_log_x_rows     *r_ptr, 
                                      mysql_rpl_listener_2 *obj,
                                      Rows_log_event_2     *new_ev,
                                      _tbl_info            *info_ptr,
                                      Table_map_log_event  *map );

    static void repl_log_x_rows_clear( _repl_log_x_rows *r_ptr );

    static bool is_numeric_type(uint type);
    static bool is_character_type(uint type);
    static bool is_enum_or_set_type(uint type);

};



#endif




