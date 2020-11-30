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



#include "../../mysql_rpl_listener.h"
#include <libgen.h>
#include <signal.h>
#include <pthread.h>
#include <sys/types.h>
#include <dirent.h>
#include <ctype.h>

#include <mysql.h>


ssize_t format_timeval(struct timeval *tv, char *buf, size_t sz)
{
    ssize_t written = -1;
    struct tm *gm = gmtime(&tv->tv_sec);

    if (gm) {

        written = (ssize_t)strftime(buf, sz, "%Y-%m-%dT%H:%M:%S", gm);
        if ( ( written > 0 ) && ( (size_t)written < sz) ) {

            int w = snprintf( buf+written, sz-(size_t)written, ".%06ldZ", tv->tv_usec );
            written = ( w > 0 ) ? written + w : -1;
        }
    }
    return written;
}

void on_replication_transaction ( _repl_log_x_transaction *tr_ptr, void *ptr ) {

    FILE *output_file = ( FILE* )ptr;

    if ( tr_ptr->transaction_event_type == TRANSACTION_EV_TYPE_START ) {

        fprintf( output_file,"-- replication thread -- BEGIN\n");
    }

    if ( tr_ptr->transaction_event_type == TRANSACTION_EV_TYPE_COMMIT ) {
        
        fprintf( output_file,"-- replication thread -- COMMIT\n");
    }
}


void on_replication_rows( _repl_log_x_rows *r_ptr, void *ptr ) {

    FILE *output_file = ( FILE* )ptr;

    char time_buff[128];
    format_timeval( &(r_ptr[0].when), time_buff, 128 );

    fprintf( output_file, "-- replication thread -- %s %s.%s (%lu) time='%s'\n", 
                          ( r_ptr[0].type_code == REPL_LOG_X_INSERT ) ? "INSERT" : 
                          ( ( r_ptr[0].type_code == REPL_LOG_X_UPDATE ) ? "UPDATE" : "DELETE" ), 
                          r_ptr[0].db_name, 
                          r_ptr[0].tbl_name, 
                          r_ptr[0].tbl_id,
                          time_buff );

    for ( uint32_t i=0; i<r_ptr[0].rows_count; i++ ) {

        fprintf( output_file, "row %u begin\n", i );
        fprintf( output_file, "values\n" );
        for( uint32_t j=0; j<r_ptr[0].col_count; j++ ) {

            if ( r_ptr[0].rows[i][j].len >= 0 ) {
            
                if ( r_ptr[0].rows[i][j].val != NULL ) {
                
                    fprintf( output_file, "%s(%d) %s\n", r_ptr[0].col_names[j], (int)r_ptr[0].col_types[j], r_ptr[0].rows[i][j].val );
                
                } else {

                    fprintf( output_file, "%s(%d) NULL\n", r_ptr[0].col_names[j], (int)r_ptr[0].col_types[j] );
                }
            
            } else {

                fprintf( output_file, "%s(%d) -\n", r_ptr[0].col_names[j], (int)r_ptr[0].col_types[j] );

            }
        }
        if ( r_ptr[0].type_code == REPL_LOG_X_UPDATE ) {

            fprintf( output_file, "new values\n" );
            for( uint32_t j=0; j<r_ptr[0].col_count; j++ ) {

                if ( r_ptr[0].rows_after[i][j].len >= 0 ) {
                
                    if ( r_ptr[0].rows_after[i][j].val != NULL ) {
                    
                        fprintf( output_file, "%s(%d) %s\n", r_ptr[0].col_names[j], (int)r_ptr[0].col_types[j], r_ptr[0].rows_after[i][j].val );
                    
                    } else {

                        fprintf( output_file, "%s(%d) NULL\n", r_ptr[0].col_names[j], (int)r_ptr[0].col_types[j] );
                    }
                
                } else {

                    fprintf( output_file, "%s(%d) -\n", r_ptr[0].col_names[j], (int)r_ptr[0].col_types[j] );

                }
            }
        }
        fprintf( output_file, "row %u end\n", i );
    }

}
//---------------------------------------------------------------------------------


char base_dir[1024];



pthread_t        SQL_thread_id;
uint8_t          SQL_thread_exit;
uint8_t          main_thread_exit;


RPL_LISTENER_H     rpl_2;
RPL_TABLE_MAPS     rpl_tbl_maps;
_repl_log_x_error  ERROR;


struct  sigaction sigact;
static void exit_signal_handler ( int sig ) {
    
    if ( rpl_tbl_maps != NULL ) {

        mysql_rpl_listener__free_tablemaps( &rpl_tbl_maps );
    }
    
    if ( SQL_thread_id != 0 ) {

        SQL_thread_exit = 1;
        pthread_join( SQL_thread_id, NULL );
    }
    
    if ( rpl_2 != NULL ) {

        if ( mysql_rpl_listener__in_process( &rpl_2 ) != REPL_PROC_STATUS_OFF ) {

            mysql_rpl_listener__stop( &rpl_2 );
            while ( mysql_rpl_listener__in_process( &rpl_2 ) != REPL_PROC_STATUS_OFF ) {

                usleep( 100000 );
            }
            if (  ERROR.is_error != 0 ) {

                mysql_rpl_listener__destroy( &rpl_2 );
                printf( "%ld %s ( %s %ld )", ERROR.code, ERROR.description, ERROR.file , ERROR.line );
            }
        }

        char       log_file_name[512];
        uint64_t   log_file_pos;
        mysql_rpl_listener__get_last_sucess_pos( &rpl_2, log_file_name, 512,  &log_file_pos );
        printf( "\nlast sucessfull position file=\"%s\" position=%lu\n", log_file_name, log_file_pos );

        mysql_rpl_listener__destroy( &rpl_2 );
    }

    printf("\n\nEND OF PROGRAMM\n\n");
    main_thread_exit = 1;

    return;
}


char *ltrim(char *s) {
    while(isspace(*s)) s++;
    return s;
}
char *rtrim(char *s) {
    char* back = s + strlen(s);
    while(isspace(*--back));
    *(back+1) = '\0';
    return s;
}
char *trim(char *s) {
    return rtrim(ltrim(s)); 
}

static int cmpstringp(const void *p1, const void *p2) {

    return strcmp(*(const char **) p1, *(const char **) p2);
}


/**
 *  убедитесь, что пользователь DB_USER имеет привелегии 
 *  REPLICATION SLAVE и REPLICATION CLIENT
 * 
 *  make sure user DB_USER has privileges
 *  REPLICATION SLAVE and REPLICATION CLIENT
 */
#define DB_HOST "127.0.0.1"
#define DB_USER "root"
#define DB_PASS "1111"
#define DB_PORT 3306
#define DB_NAME "test_bd"
#define SLAVE_ID 9999


/**
 *  функция потока для исполнения тестовых SQL запросов
 * 
 *  thread function for executing test SQL queries
 */ 
void *SQL_thread_loop_fnc( void *in_ptr ) {

    printf("-- SQL thread created --\n");

    MYSQL     *my_conn = NULL;
    MYSQL_RES *res     = NULL;
    MYSQL_ROW  row     = NULL;

    my_conn = mysql_init( NULL );
    if ( my_conn == NULL ) {

        printf("-- SQL thread can not init MySQL connection!!! Exit! --\n");
        SQL_thread_exit = 1;
        return NULL;
    }

    MYSQL *my_conn_res;
    my_conn_res = mysql_real_connect( my_conn,
                                      DB_HOST,
                                      DB_USER,
                                      DB_PASS,
                                      DB_NAME,
                                      DB_PORT,
                                      "",
                                      0 );
    if ( my_conn_res == NULL ) {

        printf("-- SQL thread MySQL Error!!! %d ( %s ) Exit! --\n", 
               mysql_errno( my_conn ), 
               mysql_error( my_conn ) );
        mysql_close( my_conn );
        SQL_thread_exit = 1;    
        return NULL;
    }

    char SQL_DIR_NAME[2048];
    snprintf( SQL_DIR_NAME, 2048, "%s../SQL", base_dir );
    DIR *SQL_DIR;

    SQL_DIR = opendir( SQL_DIR_NAME );
    if ( SQL_DIR == NULL ) {

        printf("-- SQL thread Can not open directory \"%s\"!!! Exit! --\n", SQL_DIR_NAME );
        mysql_close( my_conn );
        SQL_thread_exit = 1;    
        return NULL;
    }


    int sql_fnames_mem_len = 0;
    int sql_fnames_len     = 0;
    char **sql_fnames      = NULL;

    struct dirent *entry;
    while ( (entry = readdir( SQL_DIR )) != NULL) {

        if ( !( ( strcmp( entry->d_name, "." ) == 0 ) || ( strcmp( entry->d_name, ".." ) == 0 ) ) ) {

            int x_len = strlen( entry->d_name );
            if ( x_len > 3 ) {

                if ( strcmp( &(entry->d_name[ x_len - 4 ]), ".sql" ) == 0 ) {

                    if ( sql_fnames_len == sql_fnames_mem_len ) {

                        if ( sql_fnames_mem_len == 0 ) {

                            sql_fnames_mem_len = 100;
                            sql_fnames = (char**)malloc( sql_fnames_mem_len * sizeof( char* ) );
                            if ( sql_fnames == NULL ) {

                                printf("-- SQL thread Can allocate mem!!! Exit! --\n" );
                                
                                closedir( SQL_DIR );
                                mysql_close( my_conn );
                                SQL_thread_exit = 1;
                                return NULL;
                            }

                        } else {

                            char **sql_fnames_new; 
                            sql_fnames_mem_len = sql_fnames_mem_len + 100;
                            sql_fnames_new = (char**)realloc( sql_fnames, sql_fnames_mem_len * sizeof( char* ) );
                            if ( sql_fnames_new == NULL ) {

                                printf("-- SQL thread Can rellocate mem!!! Exit! --\n" );
                                
                                for( int i=0; i<sql_fnames_len; i++ ) {

                                    free( sql_fnames[i] );
                                }
                                free( sql_fnames );

                                closedir( SQL_DIR );
                                mysql_close( my_conn );
                                SQL_thread_exit = 1;
                                return NULL;
                            }
                        }
                    }
                    sql_fnames[ sql_fnames_len ] = (char*)malloc( ( strlen( entry->d_name ) + 1 ) * sizeof(char) );
                    if ( sql_fnames[ sql_fnames_len ] == NULL ) {

                        printf("-- SQL thread Can allocate mem!!! Exit! --\n" );
                        
                        for( int i=0; i<sql_fnames_len; i++ ) {

                            free( sql_fnames[i] );
                        }
                        free( sql_fnames );

                        closedir( SQL_DIR );
                        mysql_close( my_conn );
                        SQL_thread_exit = 1;
                        return NULL;  
                    }
                    snprintf( sql_fnames[ sql_fnames_len ], ( strlen( entry->d_name ) + 1 ), "%s", entry->d_name );
                    sql_fnames_len++;
                }
            }
        }
    };
    closedir( SQL_DIR );


    qsort( sql_fnames, sql_fnames_len, sizeof( char * ), cmpstringp );

    char  sql_f_name[4096];
    for ( int i=0; i<sql_fnames_len; i++ ) {

        snprintf( sql_f_name, 4096, "%s/%s", SQL_DIR_NAME, sql_fnames[i] );
        
        FILE *SQL_FD;

        SQL_FD = fopen( sql_f_name, "r" );
        if ( SQL_FD == NULL ) {

            printf( "-- SQL thread Can not open file \"%s\" for read!!! Exit! --\n", sql_f_name );

            for( int i=0; i<sql_fnames_len; i++ ) {

                free( sql_fnames[i] );
            }
            free( sql_fnames );    
            mysql_close( my_conn );
            SQL_thread_exit = 1;
            return NULL;
        }

        int SQL_QUERY_MEM_LEN = 1024;
        int SQL_QUERY_LEN     = 0;
        char *SQL_QUERY       = NULL;
        size_t fread_res;

        SQL_QUERY = (char*)malloc( SQL_QUERY_MEM_LEN * sizeof(char) );
        if ( SQL_QUERY == NULL ) {

            printf( "-- SQL thread Can not allocate mem!!! Exit! --\n" );

            fclose( SQL_FD );
            for( int i=0; i<sql_fnames_len; i++ ) {

                free( sql_fnames[i] );
            }
            free( sql_fnames );    
            mysql_close( my_conn );
            SQL_thread_exit = 1;
            return NULL;
        }
        SQL_QUERY[0] = '\0';
        SQL_QUERY_LEN = strlen( SQL_QUERY );

        while ( !feof( SQL_FD ) ) {

            fread_res = fread( &SQL_QUERY[ SQL_QUERY_LEN ], 1, 1, SQL_FD );
            if ( ( fread_res != 1 ) && ( !feof( SQL_FD ) ) ) {

                printf( "-- SQL thread Error when reading from file \"%s\"!!! Exit! --\n", sql_f_name );

                free( SQL_QUERY );
                fclose( SQL_FD );
                for( int i=0; i<sql_fnames_len; i++ ) {

                    free( sql_fnames[i] );
                }
                free( sql_fnames );    
                mysql_close( my_conn );
                SQL_thread_exit = 1;
                return NULL;
            }
            SQL_QUERY[ SQL_QUERY_LEN + 1 ] = '\0';
            SQL_QUERY_LEN++;

            if ( ( SQL_QUERY_LEN + 1 ) == SQL_QUERY_MEM_LEN ) {

                char *TMP_SQL_QUERY;

                SQL_QUERY_MEM_LEN = SQL_QUERY_MEM_LEN + 1024;
                TMP_SQL_QUERY = (char*)realloc( SQL_QUERY, SQL_QUERY_MEM_LEN * sizeof(char) );
                if ( TMP_SQL_QUERY == NULL ) {

                    printf( "-- SQL thread Can not reallocate mem!!! Exit! --\n" );

                    free( SQL_QUERY );
                    fclose( SQL_FD );
                    for( int i=0; i<sql_fnames_len; i++ ) {

                        free( sql_fnames[i] );
                    }
                    free( sql_fnames );    
                    mysql_close( my_conn );
                    SQL_thread_exit = 1;
                    return NULL;
                }
            }

            if ( SQL_QUERY[ SQL_QUERY_LEN-1 ] == ';' ) {

                SQL_QUERY[ SQL_QUERY_LEN-1 ] = '\0';
                char *TR_QUERY = trim( SQL_QUERY );

                printf( "\n-- SQL thread send query --\n---------------------------\n%s\n---------------------------\n", TR_QUERY );
                usleep( 500000 );

                if ( mysql_query( my_conn, TR_QUERY ) ) {

                    printf( "-- SQL thread MySQL ERROR %d ( %s )!!! Exit! --\n",
                            mysql_errno( my_conn ),
                            mysql_error( my_conn ) );

                    free( SQL_QUERY );
                    fclose( SQL_FD );
                    for( int i=0; i<sql_fnames_len; i++ ) {

                        free( sql_fnames[i] );
                    }
                    free( sql_fnames );    
                    mysql_close( my_conn );
                    SQL_thread_exit = 1;
                    return NULL;
                }

                usleep( 2000000 );

                SQL_QUERY[0] = '\0';
                SQL_QUERY_LEN = strlen( SQL_QUERY );
            }
        }

        free( SQL_QUERY );
        fclose( SQL_FD );

        if ( SQL_thread_exit == 1 ) { break; }
    }
    

    for( int i=0; i<sql_fnames_len; i++ ) {

        free( sql_fnames[i] );
    }
    free( sql_fnames );    
    mysql_close( my_conn );
    SQL_thread_exit = 1;
    return NULL;
}




int main ( int argc, char **argv ) {

    

    snprintf( base_dir, 1024, "%s/", dirname( argv[0] ) );

    main_thread_exit = 0;
    SQL_thread_id    = 0;
    SQL_thread_exit  = 0;
    rpl_2            = NULL;
    rpl_tbl_maps     = NULL;

    /**
     *  устанавливаем обработчик сигналов
     * 
     *  set up a signal handler
     */
    sigact.sa_handler = exit_signal_handler;
    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = 0;
    sigaction(SIGINT,  &sigact, (struct sigaction *)NULL);
    sigaction(SIGHUP,  &sigact, (struct sigaction *)NULL);
    sigaction(SIGTERM, &sigact, (struct sigaction *)NULL);
    sigaction(SIGQUIT, &sigact, (struct sigaction *)NULL);




    rpl_2 = NULL;
    int res = mysql_rpl_listener__init( &rpl_2,
                                        on_replication_rows,
                                        stdout,
                                        NULL,
                                        NULL,
                                        DB_HOST,
                                        DB_USER,
                                        DB_PASS,
                                        DB_PORT,
                                        SLAVE_ID,
                                        1,
                                        1,
                                        1,
                                        on_replication_transaction,
                                        stdout,
                                        NULL,
                                        0,
                                        NULL ); 
    if ( res != 0 ) {

        return 100;
    }                                                                                    

    mysql_rpl_listener__table_add( &rpl_2, DB_NAME, "tbl_1" );

    mysql_rpl_listener__prepare( &rpl_2, &ERROR );
    if ( ERROR.is_error != 0 ) {

        printf( "%ld %s ( %s %ld )!!! Exit!\n", ERROR.code, ERROR.description, ERROR.file , ERROR.line );
        return 100;
    }


    mysql_rpl_listener__start( &rpl_2, &ERROR );
    if (  ERROR.is_error != 0 ) {

        printf( "%ld %s ( %s %ld )!!! Exit!\n", ERROR.code, ERROR.description, ERROR.file , ERROR.line );
        return 100;
    }


    while ( mysql_rpl_listener__in_process( &rpl_2 ) == REPL_PROC_STATUS_PHASE_1 ) {

        usleep( 250000 );
    }


    if ( pthread_create( &SQL_thread_id, NULL, &SQL_thread_loop_fnc, NULL ) ) {

        printf( "Can not create SQL thread!!! Exit!\n" );
        return 100;
    }
    

    usleep( 250000 ); 
    uint8_t stopped = 0;

    time_t  t1 = time( NULL );
    time_t  t2;
    while ( mysql_rpl_listener__in_process( &rpl_2 ) == REPL_PROC_STATUS_PHASE_2 ) {

        usleep( 250000 );
        if ( SQL_thread_exit == 1 ) { break; }

        t2 = time( NULL );
        if ( ( ( t2 - t1 ) > 10 ) && ( stopped == 0 ) ) {

            printf( "stop replication\n" );
            mysql_rpl_listener__stop( &rpl_2 );

            stopped = 1;

            usleep( 500000 );

            char       log_file_name[512];
            uint64_t   log_file_pos;

            
            printf( "save replication position and tablemaps\n" );
            if ( mysql_rpl_listener__get_last_sucess_pos( &rpl_2, log_file_name, 512,  &log_file_pos ) != 0 ) {

                printf( "mysql_rpl_listener__get_last_sucess_pos() ERROR!!! Exit!\n" );
                break;
            }
            if ( mysql_rpl_listener__get_tablemaps( &rpl_2, &rpl_tbl_maps, &ERROR ) != 0 ) {

                printf( "mysql_rpl_listener__get_tablemaps() ERROR!!! Exit!\n" );
                break;
            }

            printf( "destroy replication object\n" );
            mysql_rpl_listener__destroy( &rpl_2 );

            usleep( 10000000 );

            printf( "start replication again from %s %lu\n", log_file_name, log_file_pos );
            int res = mysql_rpl_listener__init( &rpl_2,
                                                on_replication_rows,
                                                stdout,
                                                NULL,
                                                NULL,
                                                DB_HOST,
                                                DB_USER,
                                                DB_PASS,
                                                DB_PORT,
                                                SLAVE_ID,
                                                1,
                                                1,
                                                1,
                                                on_replication_transaction,
                                                stdout,
                                                log_file_name,
                                                log_file_pos,
                                                &rpl_tbl_maps ); 

            if ( res != 0 ) {

                printf( "mysql_rpl_listener__init() ERROR!!! Exit!\n" );
                return 100;
            }

            mysql_rpl_listener__table_add( &rpl_2, DB_NAME, "tbl_1" );

            mysql_rpl_listener__prepare( &rpl_2, &ERROR );
            if ( ERROR.is_error != 0 ) {

                printf( "%ld %s ( %s %ld )!!! Exit!\n", ERROR.code, ERROR.description, ERROR.file , ERROR.line );
                return 100;
            }


            mysql_rpl_listener__start( &rpl_2, &ERROR );
            if (  ERROR.is_error != 0 ) {

                printf( "%ld %s ( %s %ld )!!! Exit!\n", ERROR.code, ERROR.description, ERROR.file , ERROR.line );
                return 100;
            }

            mysql_rpl_listener__free_tablemaps( &rpl_tbl_maps );

            while ( mysql_rpl_listener__in_process( &rpl_2 ) == REPL_PROC_STATUS_PHASE_1 ) {

                usleep( 250000 );
            }
        }

    }
    if (  ERROR.is_error != 0 ) {

        printf( "%ld %s ( %s %ld )!!! Exit!\n", ERROR.code, ERROR.description, ERROR.file , ERROR.line );
        return 100;
    }

    SQL_thread_exit = 1;
    printf( "\n\npress Ctrl+C to exit\n\n" );
    while  (  main_thread_exit == 0 ) {

        usleep( 250000 );
    }

    return 0;
}

