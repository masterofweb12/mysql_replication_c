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


#ifndef __ROWS_LOG_EVENT_2
#define __ROWS_LOG_EVENT_2


#include <fcntl.h>
#include <inttypes.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "sql/log_event.h"


#include "json_binary.h"
#include "json_dom.h"
#include "json_diff.h"
#include "log_event.h"

#define OK_CONTINUE 0L
#define ERROR_STOP 1L
#define OK_STOP 2L

#include "../mysql_rpl_listener_2/mysql_rpl_tbl_info.h"


class Rows_log_event_2 : public Rows_log_event {

    
    private:
    char error_description[1024];

    public:
    
    char*  get_error_desc();

    void  set_error_desc( const char* desc );

    

    int get_rows( Table_map_log_event *map, _repl_log_x_rows *x_rows_ptr, _tbl_info *info_ptr );



    size_t get_one_row_values ( table_def *td, 
                                MY_BITMAP *cols_bitmap, 
                                const uchar *value, 
                                enum_row_image_type row_image_type,
                                _repl_log_x_rows *x_rows_ptr,
                                int32_t *error_code_ptr,
                                _tbl_info *info_ptr );


    size_t log_event_get_value_2(   const uchar *ptr, 
                                    uint  type,
                                    uint  meta, 
                                    bool  is_partial,
                                    _repl_log_x_value    *ROW_COL_PTR,
                                    int32_t              *error_code_ptr,
                                    _col_info            *col_info_ptr,
                                    _repl_display_blobs  display_blobs );



    void my_b_write_bit_2( const uchar *ptr, uint nbits, char* out_buff );

    size_t my_b_print_with_length_2( const uchar *ptr, uint length, char* out_buff );

    void sprint_as_hex( char *dst, const char *str, int64 len );

    void repl_log_x_rows_add_row_mem( _repl_log_x_rows *x_rows_ptr, 
                                      enum_row_image_type row_image_type,
                                      int32_t *error_code_ptr );

    void repl_log_x_rows_finish( _repl_log_x_rows *x_rows_ptr, 
                                 int32_t *error_code_ptr );                                

}; 

#endif
