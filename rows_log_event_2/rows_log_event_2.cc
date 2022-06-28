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


#include "rows_log_event_2.h"



char*  Rows_log_event_2::get_error_desc() {

    return this->error_description;
}

void  Rows_log_event_2::set_error_desc( const char* desc ) {

    snprintf( this->error_description, 1024, "***Rows_log_event_2 Error: %s", desc );
}



int Rows_log_event_2::get_rows( Table_map_log_event *map, _repl_log_x_rows *x_rows_ptr, _tbl_info *info_ptr ) {

    table_def  *td = nullptr;

    Log_event_type general_type_code = get_general_type_code();
    switch ( general_type_code ) {

        case binary_log::WRITE_ROWS_EVENT:
        ;;
        x_rows_ptr[0].type_code = REPL_LOG_X_INSERT;
        break;
        case binary_log::DELETE_ROWS_EVENT:
        ;;
        x_rows_ptr[0].type_code = REPL_LOG_X_DELETE;
        break;
        case binary_log::UPDATE_ROWS_EVENT:
        case binary_log::PARTIAL_UPDATE_ROWS_EVENT:
        ;;
        x_rows_ptr[0].type_code = REPL_LOG_X_UPDATE;
        break;
        default: {

            set_error_desc( "unknown general_type_code" );
            return ERROR_STOP;
        }
    }

    // enum_row_image_type { WRITE_AI, UPDATE_BI, UPDATE_AI, DELETE_BI };
    enum_row_image_type row_image_type =
        general_type_code == binary_log::WRITE_ROWS_EVENT
            ? enum_row_image_type::WRITE_AI
            : general_type_code == binary_log::DELETE_ROWS_EVENT
                    ? enum_row_image_type::DELETE_BI
                    : enum_row_image_type::UPDATE_BI;


    if ( m_extra_row_info.have_ndb_info() ) {
        
        if ( m_extra_row_info.get_ndb_length() < EXTRA_ROW_INFO_HEADER_LENGTH ) {
            
            set_error_desc( "the number of extra_row_ndb_info is smaller than the minimum acceptable value" );
            return ERROR_STOP;
        }
    }

    /*
    if ( m_extra_row_info.have_part() ) {
        ;;
    }
    */

    if ( !( map ) || !( td = map->create_table_def() ) ) {
        
        if ( td != nullptr ) {

            delete td;
        }
        char err_str[900];
        snprintf( err_str, 900, "row event for unknown table #%llu", this->get_table_id().id() );
        set_error_desc( err_str );
        return ERROR_STOP;
    }


    if( info_ptr->col_count != (uint32_t)map->m_colcnt ) {

        delete td;
        char err_str[900];
        snprintf( err_str, 900, "cached col_count != col_count in table_map event (`%s`.`%s` tbl_id=%lu)!!!",
                                map->get_db_name(),
                                map->get_table_name(),
                                (uint64_t)this->get_table_id().id() );
        set_error_desc( err_str );
        return ERROR_STOP;
    }

    /* If the write rows event contained no values for the After Image */
    if ( ( general_type_code == binary_log::WRITE_ROWS_EVENT ) && ( m_rows_buf == m_rows_end ) ) {
    
        goto end;
    }

    
    for ( const uchar *value = m_rows_buf; value < m_rows_end; ) {
        
        int32_t error_code = 0;
        size_t  length;
        
        // * Print the first image * /
        length = get_one_row_values (   td,
                                        &( this->m_cols ),
                                        value,
                                        row_image_type,
                                        x_rows_ptr,
                                        &error_code,
                                        info_ptr );
        if ( error_code != 0 ) {

            delete td;
            // set_error_desc() вызвана ранее в print_verbose_one_row_2
            return ERROR_STOP;
        }                                                              
        if ( !( length ) ) {

            goto end;
        }
        value += length;

        // * Print the second image (for UPDATE only) * /
        if ( ( general_type_code == binary_log::UPDATE_ROWS_EVENT ) || ( general_type_code == binary_log::PARTIAL_UPDATE_ROWS_EVENT ) ) {

            length = get_one_row_values (   td,
                                            &( this->m_cols_ai ),
                                            value,
                                            enum_row_image_type::UPDATE_AI,
                                            x_rows_ptr,
                                            &error_code,
                                            info_ptr );
            if ( error_code != 0 ) {

                delete td;
                // set_error_desc() вызвана ранее в print_verbose_one_row_2
                return ERROR_STOP;
            }  
            if ( !( length ) ) {

                goto end;
            }
            value += length;
        }
    }


    end:

    ;;
    int32_t error_code = 0;
    this->repl_log_x_rows_finish( x_rows_ptr, &error_code );
    if ( error_code != 0 ) {

        set_error_desc( "can not allocate mem for finish rows!!!" );
        delete td;
        return ERROR_STOP;
    }

    delete td;
    return OK_CONTINUE;
}




/*
    выделяем память под очередную строку, 
    но только если мы не в enum_row_image_type::UPDATE_AI
    ибо под него мы выделяем память заходя в эту функцию под enum_row_image_type::UPDATE_BI
*/
void Rows_log_event_2::repl_log_x_rows_add_row_mem( _repl_log_x_rows *x_rows_ptr, 
                                                   enum_row_image_type row_image_type,
                                                   int32_t *error_code_ptr ) {
    error_code_ptr[0] = 0;

    if ( row_image_type != enum_row_image_type::UPDATE_AI ) {

        if ( x_rows_ptr[0].rows_count == 0 ) {

            x_rows_ptr[0].rows_L2 = ( _repl_log_x_value* )malloc( sizeof( _repl_log_x_value ) * ( x_rows_ptr[0].rows_count + 1 ) * x_rows_ptr[0].col_count );
            if ( x_rows_ptr[0].rows_L2 == NULL ) {

                error_code_ptr[0] = (int32_t)__LINE__;
                return;
            }
            memset( &x_rows_ptr[0].rows_L2[ x_rows_ptr[0].rows_count * x_rows_ptr[0].col_count ], 0, ( sizeof( _repl_log_x_value ) * x_rows_ptr[0].col_count ) );
            for ( uint32_t i=0; i<x_rows_ptr[0].col_count; i++ ) {

                x_rows_ptr[0].rows_L2[ x_rows_ptr[0].rows_count * x_rows_ptr[0].col_count + i ].len = -1;
            }

            // если мы сейчас выделяем память под строку ДО, то сразу выделим и для ПОСЛЕ
            if ( row_image_type == enum_row_image_type::UPDATE_BI ) {

                x_rows_ptr[0].rows_after_L2 = ( _repl_log_x_value* )malloc( sizeof( _repl_log_x_value ) * ( x_rows_ptr[0].rows_count + 1 ) * x_rows_ptr[0].col_count );
                if ( x_rows_ptr[0].rows_after_L2 == NULL ) {

                    error_code_ptr[0] = (int32_t)__LINE__;
                    return;
                }
                memset( &x_rows_ptr[0].rows_after_L2[ x_rows_ptr[0].rows_count * x_rows_ptr[0].col_count ], 0, ( sizeof( _repl_log_x_value ) * x_rows_ptr[0].col_count ) );
                for ( uint32_t i=0; i<x_rows_ptr[0].col_count; i++ ) {

                    x_rows_ptr[0].rows_after_L2[ x_rows_ptr[0].rows_count * x_rows_ptr[0].col_count + i ].len = -1;
                }
            }

        } else {

            _repl_log_x_value* tmp_ptr;
            
            tmp_ptr = ( _repl_log_x_value* )realloc( x_rows_ptr[0].rows_L2, 
                                                    sizeof( _repl_log_x_value ) * ( x_rows_ptr[0].rows_count + 1 ) * x_rows_ptr[0].col_count );
            if ( tmp_ptr == NULL ) {

                error_code_ptr[0] = (int32_t)__LINE__;
                return;
            }
            x_rows_ptr[0].rows_L2 = tmp_ptr;
            memset( &x_rows_ptr[0].rows_L2[ x_rows_ptr[0].rows_count * x_rows_ptr[0].col_count ], 0, ( sizeof( _repl_log_x_value ) * x_rows_ptr[0].col_count ) );
            for ( uint32_t i=0; i<x_rows_ptr[0].col_count; i++ ) {

                x_rows_ptr[0].rows_L2[ x_rows_ptr[0].rows_count * x_rows_ptr[0].col_count + i ].len = -1;
            }


            // если мы сейчас выделяем память под строку ДО, то сразу выделим и для ПОСЛЕ
            if ( row_image_type == enum_row_image_type::UPDATE_BI ) {

                tmp_ptr = ( _repl_log_x_value* )realloc( x_rows_ptr[0].rows_after_L2, 
                                                        sizeof( _repl_log_x_value ) * ( x_rows_ptr[0].rows_count + 1 ) * x_rows_ptr[0].col_count );
                if ( tmp_ptr == NULL ) {

                    error_code_ptr[0] = (int32_t)__LINE__;
                    return;
                }
                x_rows_ptr[0].rows_after_L2 = tmp_ptr;
                memset( &x_rows_ptr[0].rows_after_L2[ x_rows_ptr[0].rows_count * x_rows_ptr[0].col_count ], 0, ( sizeof( _repl_log_x_value ) * x_rows_ptr[0].col_count ) );
                for ( uint32_t i=0; i<x_rows_ptr[0].col_count; i++ ) {

                    x_rows_ptr[0].rows_after_L2[ x_rows_ptr[0].rows_count * x_rows_ptr[0].col_count + i ].len = -1;;
                }
            }
        }

        x_rows_ptr[0].rows_count++;
    }

    return;
}


void Rows_log_event_2::repl_log_x_rows_finish( _repl_log_x_rows *x_rows_ptr, 
                                               int32_t *error_code_ptr ) {

    error_code_ptr[0] = 0;

    //x_rows_ptr[0].type_code

    x_rows_ptr[0].rows = ( _repl_log_x_value** )malloc( sizeof( _repl_log_x_value* ) * x_rows_ptr[0].rows_count );
    if ( x_rows_ptr[0].rows == NULL ) {

        error_code_ptr[0] = __LINE__;
        return;        
    }
    for ( uint32_t i=0; i<x_rows_ptr[0].rows_count; i++ ) {

        x_rows_ptr[0].rows[ i ] = &( x_rows_ptr[0].rows_L2[ i * x_rows_ptr[0].col_count ] );
    }

    if ( x_rows_ptr[0].type_code == REPL_LOG_X_UPDATE ) {

        x_rows_ptr[0].rows_after = ( _repl_log_x_value** )malloc( sizeof( _repl_log_x_value* ) * x_rows_ptr[0].rows_count );
        if ( x_rows_ptr[0].rows_after == NULL ) {

            error_code_ptr[0] = __LINE__;
            return;        
        }
        for ( uint32_t i=0; i<x_rows_ptr[0].rows_count; i++ ) {

            x_rows_ptr[0].rows_after[ i ] = &( x_rows_ptr[0].rows_after_L2[ i * x_rows_ptr[0].col_count ] );
        }

    }


    return;
}






size_t Rows_log_event_2::get_one_row_values ( table_def *td, 
                                              MY_BITMAP *cols_bitmap, 
                                              const uchar *value, 
                                              enum_row_image_type row_image_type,
                                              _repl_log_x_rows *x_rows_ptr,
                                              int32_t *error_code_ptr,
                                              _tbl_info *info_ptr ) {

    error_code_ptr[0] = 0;
    const uchar *value0 = value;

    // Read value_options if this is AI for PARTIAL_UPDATE_ROWS_EVENT
    ulonglong value_options = 0;
    Bit_reader partial_bits;

    if ( ( get_type_code() == binary_log::PARTIAL_UPDATE_ROWS_EVENT )
        &&
        ( row_image_type == enum_row_image_type::UPDATE_AI )
    ) {
        size_t length = m_rows_end - value;
        if ( net_field_length_checked<ulonglong>(&value, &length, &value_options )) {
        
            error_code_ptr[0] = (int32_t)__LINE__;
            this->set_error_desc( "error reading binlog_row_value_options from Partial_update_rows_log_event" );
            return 0;
        }
        if ( (value_options & PARTIAL_JSON_UPDATES ) != 0 ) {
        
            partial_bits.set_ptr(value);
            value += (td->json_column_count() + 7) / 8;
        }
    }




    this->repl_log_x_rows_add_row_mem( x_rows_ptr, 
                                      row_image_type,
                                      error_code_ptr );
    if ( error_code_ptr[0] != 0 ) {

        error_code_ptr[0] = (int32_t)__LINE__;
        this->set_error_desc( "can not allocate mem for row!!!" );
        return 0;
    }                                  


    _repl_log_x_value* X_ROW;
    if ( row_image_type == enum_row_image_type::UPDATE_AI ) {

        X_ROW = &( x_rows_ptr[0].rows_after_L2[ ( x_rows_ptr[0].rows_count - 1 ) * x_rows_ptr[0].col_count ] );

    } else {

        X_ROW = &( x_rows_ptr[0].rows_L2[ ( x_rows_ptr[0].rows_count - 1 ) * x_rows_ptr[0].col_count ] );
    }

    /*
        Metadata bytes which gives the information about nullabity of
        master columns. Master writes one bit for each column in the
        image.
    */
    Bit_reader null_bits(value);
    value += ( bitmap_bits_set(cols_bitmap) + 7 ) / 8;


    for ( size_t i = 0; i < td->size(); i++ ) {
        
        /*
        Note: need to read partial bit before reading cols_bitmap, since
        the partial_bits bitmap has a bit for every JSON column
        regardless of whether it is included in the bitmap or not.
        */
        bool is_partial = ( ( value_options & PARTIAL_JSON_UPDATES ) != 0 ) &&
                            ( row_image_type == enum_row_image_type::UPDATE_AI ) &&
                            ( td->type(i) == MYSQL_TYPE_JSON && partial_bits.get() );

        // если колонка не участвует в представлении то пропускаем 
        // итерацию цикла по ней оставив len равным -1, что по сути и есть индикатор незадействованности колонки 
        if ( bitmap_is_set(cols_bitmap, i) == 0 ) {

            continue;
        }

        // если колонка участвует в представлении,
        // то сменим длину c -1 на 0 ( но это может поменяться внутри log_event_get_value_2() для блобов, 
        // если блобы не надо отображать, то мы установим X_ROW[ i ].len в -1 для блоба  )
        X_ROW[ i ].len = 0;


        bool is_null = null_bits.get();

        if ( !is_null ) {
        
            size_t fsize = td->calc_field_size( (uint)i, pointer_cast<const uchar *>(value) );
            if ( fsize > (size_t)(m_rows_end - value) ) {
                
                error_code_ptr[0] = (int32_t)__LINE__;
                char err_str[900];
                snprintf( err_str, 900, "corrupted replication event was detected: "
                                        "field size is set to %u, but there are only %u bytes "
                                        "left of the event. Not printing the value", (uint)fsize, (uint)(m_rows_end - value) );
                set_error_desc( err_str );
                return 0;
            }
        }
        
        // _tbl_info *info_ptr
        // _repl_log_x_rows *x_rows_ptr
        size_t size = log_event_get_value_2(  is_null ? nullptr : value, 
                                                td->type(i), 
                                                td->field_metadata(i),
                                                is_partial,
                                                &X_ROW[ i ],
                                                error_code_ptr,
                                                &( info_ptr[0].columns[ i ] ),
                                                x_rows_ptr[0].display_blobs );
        if ( error_code_ptr[0] != 0 ) {

            // 
            return 0;
        }                                        
        
        if ( !is_null ) {

            value += size;
        } 

    }
    return value - value0;

} //--




size_t Rows_log_event_2::log_event_get_value_2( const uchar *ptr, 
                                uint  type,
                                uint  meta, 
                                bool  is_partial,
                                _repl_log_x_value    *ROW_COL_PTR,
                                int32_t              *error_code_ptr,
                                _col_info            *col_info_ptr,
                                _repl_display_blobs  display_blobs ) {
error_code_ptr[0] = 0;

uint32 length = 0;
if ( type == MYSQL_TYPE_STRING ) {

    if (meta >= 256) {

        uint byte0 = meta >> 8;
        uint byte1 = meta & 0xFF;

        if ((byte0 & 0x30) != 0x30) {
            /* a long CHAR() field: see #37426 */
            length = byte1 | (((byte0 & 0x30) ^ 0x30) << 4);
            type = byte0 | 0x30;
        } else {
            length = meta & 0xFF;
        }

    } else {

        length = meta;
    }
}

switch (type) {

    case MYSQL_TYPE_LONG: {

        if (!ptr) {
        
            return 0;
        }
        
        ROW_COL_PTR[0].val = (char*)malloc( 16 );
        if ( ROW_COL_PTR[0].val == NULL ) {

            set_error_desc( "can not allocate mem for value MYSQL_TYPE_LONG" );
            error_code_ptr[0] = (uint32_t)__LINE__;
            return 0;
        }
        if ( col_info_ptr->is_unsigned ) {
        
            uint32 ui = uint4korr(ptr);
            snprintf( ROW_COL_PTR[0].val, 16, "%u", ui );
        
        } else {

            int32 si = sint4korr(ptr);
            snprintf( ROW_COL_PTR[0].val, 16, "%d", si );
        }
        ROW_COL_PTR[0].len = strlen( ROW_COL_PTR[0].val );

        return 4;
    }

    case MYSQL_TYPE_TINY: {

        if (!ptr) {
            
            return 0;
        }

        ROW_COL_PTR[0].val = (char*)malloc( 16 );
        if ( ROW_COL_PTR[0].val == NULL ) {

            set_error_desc( "can not allocate mem for value MYSQL_TYPE_TINY" );
            error_code_ptr[0] = (uint32_t)__LINE__;
            return 0;
        }
        if ( col_info_ptr->is_unsigned ) {
        
            uint32 ui = (uint32)( (unsigned char)*ptr );
            snprintf( ROW_COL_PTR[0].val, 16, "%u", ui );
        
        } else {

            int32 si = (int32)( (signed char)*ptr );
            snprintf( ROW_COL_PTR[0].val, 16, "%d", si );
        }
        ROW_COL_PTR[0].len = strlen( ROW_COL_PTR[0].val );

        return 1;
    }

    case MYSQL_TYPE_SHORT: {

        if (!ptr) {
        
            return 0;
        }

        ROW_COL_PTR[0].val = (char*)malloc( 16 );
        if ( ROW_COL_PTR[0].val == NULL ) {

            set_error_desc( "can not allocate mem for value MYSQL_TYPE_SHORT" );
            error_code_ptr[0] = (uint32_t)__LINE__;
            return 0;
        }
        if ( col_info_ptr->is_unsigned ) {
        
            uint32 ui = (uint32)uint2korr(ptr);
            snprintf( ROW_COL_PTR[0].val, 16, "%u", ui );
        
        } else {

            int32 si = (int32)sint2korr(ptr);
            snprintf( ROW_COL_PTR[0].val, 16, "%d", si );

        }
        ROW_COL_PTR[0].len = strlen( ROW_COL_PTR[0].val );

        return 2;
    }

    case MYSQL_TYPE_INT24: {

        if (!ptr) {
            
            return 0;
        }
        

        ROW_COL_PTR[0].val = (char*)malloc( 16 );
        if ( ROW_COL_PTR[0].val == NULL ) {

            set_error_desc( "can not allocate mem for value MYSQL_TYPE_INT24" );
            error_code_ptr[0] = (uint32_t)__LINE__;
            return 0;
        }
        if ( col_info_ptr->is_unsigned ) {
        
            uint32 ui = uint3korr(ptr);
            snprintf( ROW_COL_PTR[0].val, 16, "%u", ui );
        
        } else {

            int32 si = sint3korr(ptr);
            snprintf( ROW_COL_PTR[0].val, 16, "%d", si );
        }
        ROW_COL_PTR[0].len = strlen( ROW_COL_PTR[0].val );

        return 3;
    }

    case MYSQL_TYPE_LONGLONG: {

        if (!ptr) {
            
            return 0;
        }
        

        ROW_COL_PTR[0].val = (char*)malloc( 32 );
        if ( ROW_COL_PTR[0].val == NULL ) {

            set_error_desc( "can not allocate mem for value MYSQL_TYPE_LONGLONG" );
            error_code_ptr[0] = (uint32_t)__LINE__;
            return 0;
        }
        if ( col_info_ptr->is_unsigned ) {
        
            ulonglong ui = uint8korr(ptr);
            snprintf( ROW_COL_PTR[0].val, 32, "%llu", ui );
        
        } else {

            longlong si = sint8korr(ptr);
            snprintf( ROW_COL_PTR[0].val, 32, "%lld", si );

        }
        ROW_COL_PTR[0].len = strlen( ROW_COL_PTR[0].val );

        return 8;
    }

    case MYSQL_TYPE_NEWDECIMAL: {

        uint precision = meta >> 8;
        uint decimals = meta & 0xFF;
        

        if (!ptr) {
        
            return 0;
        }
        uint bin_size = my_decimal_get_binary_size(precision, decimals);
        my_decimal dec;
        binary2my_decimal(E_DEC_FATAL_ERROR, pointer_cast<const uchar *>(ptr),
                            &dec, precision, decimals);
        char buff[DECIMAL_MAX_STR_LENGTH + 1];
        int len = sizeof(buff);
        decimal2string(&dec, buff, &len);
        
        ROW_COL_PTR[0].val = (char*)malloc( strlen( buff ) + 1 );
        if ( ROW_COL_PTR[0].val == NULL ) {

            set_error_desc( "can not allocate mem for value MYSQL_TYPE_NEWDECIMAL" );
            error_code_ptr[0] = (uint32_t)__LINE__;
            return 0;
        }
        snprintf( ROW_COL_PTR[0].val, strlen( buff ) + 1, "%s", buff );
        ROW_COL_PTR[0].len = strlen( ROW_COL_PTR[0].val );

        return bin_size;
    }

    case MYSQL_TYPE_FLOAT: {

        if (!ptr) {
        
            return 0;
        }
        float fl = float4get(ptr);
        char tmp[320];
        sprintf( tmp, "%-20g", (double)fl );

        ROW_COL_PTR[0].val = (char*)malloc( strlen( tmp ) + 1 );
        if ( ROW_COL_PTR[0].val == NULL ) {

            set_error_desc( "can not allocate mem for value MYSQL_TYPE_FLOAT" );
            error_code_ptr[0] = (uint32_t)__LINE__;
            return 0;
        }
        snprintf( ROW_COL_PTR[0].val, strlen( tmp ) + 1, "%s", tmp );
        ROW_COL_PTR[0].len = strlen( ROW_COL_PTR[0].val );

        return 4;
    }

    case MYSQL_TYPE_DOUBLE: {

        if (!ptr) {
            
            return 0;
        }
        double dbl = float8get(ptr);
        char tmp[320];
        sprintf( tmp, "%-.20g", dbl ); /* my_b_printf doesn't support %-20g */
        
        ROW_COL_PTR[0].val = (char*)malloc( strlen( tmp ) + 1 );
        if ( ROW_COL_PTR[0].val == NULL ) {

            set_error_desc( "can not allocate mem for value MYSQL_TYPE_DOUBLE" );
            error_code_ptr[0] = (uint32_t)__LINE__;
            return 0;
        }
        snprintf( ROW_COL_PTR[0].val, strlen( tmp ) + 1, "%s", tmp );
        ROW_COL_PTR[0].len = strlen( ROW_COL_PTR[0].val );           
        
        return 8;
    }

    case MYSQL_TYPE_BIT: {

        /* Meta-data: bit_len, bytes_in_rec, 2 bytes */
        uint nbits = ((meta >> 8) * 8) + (meta & 0xFF);
        
        
        if (!ptr) {
        
            return 0;
        }
        length = (nbits + 7) / 8;

        ROW_COL_PTR[0].val = (char*)malloc( nbits + 4 );
        if ( ROW_COL_PTR[0].val == NULL ) {

            set_error_desc( "can not allocate mem for value MYSQL_TYPE_BIT" );
            error_code_ptr[0] = (uint32_t)__LINE__;
            return 0;
        }
        my_b_write_bit_2( ptr, nbits, ROW_COL_PTR[0].val );
        ROW_COL_PTR[0].len = strlen( ROW_COL_PTR[0].val );  
        
        return length;
    }

    case MYSQL_TYPE_TIMESTAMP: {

        if (!ptr) {
        
            return 0;
        }
        uint32 i32 = uint4korr(ptr);

        ROW_COL_PTR[0].val = (char*)malloc( 16 );
        if ( ROW_COL_PTR[0].val == NULL ) {

            set_error_desc( "can not allocate mem for value MYSQL_TYPE_TIMESTAMP" );
            error_code_ptr[0] = (uint32_t)__LINE__;
            return 0;
        }
        snprintf( ROW_COL_PTR[0].val, 16, "%u", i32 );
        ROW_COL_PTR[0].len = strlen( ROW_COL_PTR[0].val );

        return 4;
    }

    case MYSQL_TYPE_TIMESTAMP2: {
        
        if (!ptr) {
        
            return 0;
        }
        char buf[MAX_DATE_STRING_REP_LENGTH+1];
        memset( buf, 0, MAX_DATE_STRING_REP_LENGTH+1 );
        struct my_timeval tm;
        my_timestamp_from_binary(&tm, ptr, meta);

        my_timeval_to_str(&tm, buf, meta);
        
        ROW_COL_PTR[0].val = (char*)malloc( strlen( buf ) + 1 );
        if ( ROW_COL_PTR[0].val == NULL ) {

            set_error_desc( "can not allocate mem for value MYSQL_TYPE_TIMESTAMP2" );
            error_code_ptr[0] = (uint32_t)__LINE__;
            return 0;
        }
        snprintf( ROW_COL_PTR[0].val, strlen( buf ) + 1, "%s", buf );
        ROW_COL_PTR[0].len = strlen( ROW_COL_PTR[0].val );

        return my_timestamp_binary_length(meta);
    }

    case MYSQL_TYPE_DATETIME: {

        if (!ptr) {
            
            return 0;
        }
        size_t d, t;
        uint64 i64 = uint8korr(ptr); /* YYYYMMDDhhmmss */
        d = static_cast<size_t>(i64 / 1000000);
        t = i64 % 1000000;

        ROW_COL_PTR[0].val = (char*)malloc( 32 );
        if ( ROW_COL_PTR[0].val == NULL ) {

            set_error_desc( "can not allocate mem for value MYSQL_TYPE_DATETIME" );
            error_code_ptr[0] = (uint32_t)__LINE__;
            return 0;
        }
        snprintf( ROW_COL_PTR[0].val,
                    32,
                    "%04d-%02d-%02d %02d:%02d:%02d",
                    static_cast<int>(d / 10000),
                    static_cast<int>(d % 10000) / 100, static_cast<int>(d % 100),
                    static_cast<int>(t / 10000),
                    static_cast<int>(t % 10000) / 100, static_cast<int>(t % 100));
        ROW_COL_PTR[0].len = strlen( ROW_COL_PTR[0].val );

        return 8;
    }

    case MYSQL_TYPE_DATETIME2: {
        
        if (!ptr) {
            
            return 0;
        }
        char buf[MAX_DATE_STRING_REP_LENGTH+1];
        memset( buf, 0, MAX_DATE_STRING_REP_LENGTH+1 );
        MYSQL_TIME ltime;
        longlong packed = my_datetime_packed_from_binary(ptr, meta);
        TIME_from_longlong_datetime_packed(&ltime, packed);

        my_datetime_to_str(ltime, buf, meta);

        ROW_COL_PTR[0].val = (char*)malloc( strlen( buf ) + 1 );
        if ( ROW_COL_PTR[0].val == NULL ) {

            set_error_desc( "can not allocate mem for value MYSQL_TYPE_DATETIME2" );
            error_code_ptr[0] = (uint32_t)__LINE__;
            return 0;
        }
        snprintf( ROW_COL_PTR[0].val, strlen( buf ) + 1, "%s", buf );
        ROW_COL_PTR[0].len = strlen( ROW_COL_PTR[0].val );

        return my_datetime_binary_length(meta);
    }

    case MYSQL_TYPE_TIME: {

        if (!ptr) {
        
            return 0;
        }
        uint32 i32 = uint3korr(ptr);

        ROW_COL_PTR[0].val = (char*)malloc( 16 );
        if ( ROW_COL_PTR[0].val == NULL ) {

            set_error_desc( "can not allocate mem for value MYSQL_TYPE_TIME" );
            error_code_ptr[0] = (uint32_t)__LINE__;
            return 0;
        }
        snprintf( ROW_COL_PTR[0].val, 16, "%02d:%02d:%02d", i32 / 10000, (i32 % 10000) / 100, i32 % 100 );
        ROW_COL_PTR[0].len = strlen( ROW_COL_PTR[0].val );

        return 3;
    }

    case MYSQL_TYPE_TIME2: {
        
        if (!ptr) {
        
            return 0;
        }
        char buf[MAX_DATE_STRING_REP_LENGTH+1];
        memset( buf, 0, MAX_DATE_STRING_REP_LENGTH+1 );
        MYSQL_TIME ltime;
        longlong packed = my_time_packed_from_binary(ptr, meta);
        TIME_from_longlong_time_packed(&ltime, packed);

        my_time_to_str( ltime, buf, meta );

        ROW_COL_PTR[0].val = (char*)malloc( strlen( buf ) + 1 );
        if ( ROW_COL_PTR[0].val == NULL ) {

            set_error_desc( "can not allocate mem for value MYSQL_TYPE_TIME2" );
            error_code_ptr[0] = (uint32_t)__LINE__;
            return 0;
        }
        snprintf( ROW_COL_PTR[0].val, strlen( buf ) + 1, "%s", buf );
        ROW_COL_PTR[0].len = strlen( ROW_COL_PTR[0].val );

        return my_time_binary_length(meta);
    }

    case MYSQL_TYPE_NEWDATE: {

        if (!ptr) {
        
            return 0;
        }
        uint32 tmp = uint3korr(ptr);
        int part;
        char buf[11];
        char *pos = &buf[10];  // start from '\0' to the beginning

        /* Copied from field.cc */
        *pos-- = 0;  // End NULL
        part = (int)(tmp & 31);
        *pos-- = (char)('0' + part % 10);
        *pos-- = (char)('0' + part / 10);
        *pos-- = '-';
        part = (int)(tmp >> 5 & 15);
        *pos-- = (char)('0' + part % 10);
        *pos-- = (char)('0' + part / 10);
        *pos-- = '-';
        part = (int)(tmp >> 9);
        *pos-- = (char)('0' + part % 10);
        part /= 10;
        *pos-- = (char)('0' + part % 10);
        part /= 10;
        *pos-- = (char)('0' + part % 10);
        part /= 10;
        *pos = (char)('0' + part);

        ROW_COL_PTR[0].val = (char*)malloc( strlen( buf ) + 1 );
        if ( ROW_COL_PTR[0].val == NULL ) {

            set_error_desc( "can not allocate mem for value MYSQL_TYPE_NEWDATE" );
            error_code_ptr[0] = (uint32_t)__LINE__;
            return 0;
        }
        snprintf( ROW_COL_PTR[0].val, strlen( buf ) + 1, "%s", buf );
        ROW_COL_PTR[0].len = strlen( ROW_COL_PTR[0].val );

        return 3;
    }

    case MYSQL_TYPE_YEAR: {

        if (!ptr) {
        
            return 0;
        }
        int32 i32 = *ptr;

        ROW_COL_PTR[0].val = (char*)malloc( 16 );
        if ( ROW_COL_PTR[0].val == NULL ) {

            set_error_desc( "can not allocate mem for value MYSQL_TYPE_YEAR" );
            error_code_ptr[0] = (uint32_t)__LINE__;
            return 0;
        }
        snprintf( ROW_COL_PTR[0].val, 16, "%04d", i32 + 1900 );
        ROW_COL_PTR[0].len = strlen( ROW_COL_PTR[0].val );

        return 1;
    }

    case MYSQL_TYPE_ENUM: {

        switch (meta & 0xFF) {

            case 1: {
                
                if (!ptr) {
                    
                    return 0;
                }
                int32 i32 = *ptr;

                ROW_COL_PTR[0].val = (char*)malloc( 16 );
                if ( ROW_COL_PTR[0].val == NULL ) {

                    set_error_desc( "can not allocate mem for value MYSQL_TYPE_ENUM" );
                    error_code_ptr[0] = (uint32_t)__LINE__;
                    return 0;
                }
                snprintf( ROW_COL_PTR[0].val, 16, "%d", (int)i32 );
                ROW_COL_PTR[0].len = strlen( ROW_COL_PTR[0].val );

                return 1;
            }
            case 2: {
                
                if (!ptr) {
                
                    return 0;
                }
                int32 i32 = uint2korr(ptr);

                ROW_COL_PTR[0].val = (char*)malloc( 16 );
                if ( ROW_COL_PTR[0].val == NULL ) {

                    set_error_desc( "can not allocate mem for value MYSQL_TYPE_ENUM" );
                    error_code_ptr[0] = (uint32_t)__LINE__;
                    return 0;
                }
                snprintf( ROW_COL_PTR[0].val, 16, "%d", (int)i32 );
                ROW_COL_PTR[0].len = strlen( ROW_COL_PTR[0].val );

                return 2;
            }
            default: {
                
                char err_str[900];
                snprintf( err_str, 900, "!! Unknown ENUM packlen=%d",  meta & 0xFF );
                error_code_ptr[0] = (uint32_t)__LINE__;
                set_error_desc( err_str );
                return 0;
            }
        }
    }
    break;

    case MYSQL_TYPE_SET: {
        
        if (!ptr) {
        
            return 0;
        }

        ROW_COL_PTR[0].val = (char*)malloc( ( (meta & 0xFF) * 8 ) + 4 );
        if ( ROW_COL_PTR[0].val == NULL ) {

            set_error_desc( "can not allocate mem for value MYSQL_TYPE_SET" );
            error_code_ptr[0] = (uint32_t)__LINE__;
            return 0;
        }
        my_b_write_bit_2( ptr, (meta & 0xFF) * 8, ROW_COL_PTR[0].val );
        ROW_COL_PTR[0].len = strlen( ROW_COL_PTR[0].val ); 

        return meta & 0xFF;
    }

    case MYSQL_TYPE_BLOB: {
    
        if (!ptr) {
        
            if ( display_blobs.BLOBS_DISPLAY == 0 ) {

                ROW_COL_PTR[0].len    = -1;
            } else {
                ROW_COL_PTR[0].len    = 0;
            }
            ROW_COL_PTR[0].val    = NULL;
            ROW_COL_PTR[0].is_hex = display_blobs.BLOB_AS_HEX;
            return 0;
        }

        size_t blob_retval = 0; 
        switch (meta) {
            case 1: {

                length = *ptr;
                blob_retval = length + 1;
                ptr += 1;
                break;
            }
            case 2: {

                length = uint2korr(ptr);
                blob_retval = length + 2;
                ptr += 2;
                break;
            }
            case 3: {

                length = uint3korr(ptr);
                blob_retval = length + 3;
                ptr += 3;
                break;
            }
            case 4: {

                length = uint4korr(ptr);
                blob_retval = length + 4;
                ptr += 4;
                break;
            }
            default: {
                
                char err_str[900];
                snprintf( err_str, 900, "!!Unknown BLOB packlen=%d",  length );
                error_code_ptr[0] = (uint32_t)__LINE__;
                set_error_desc( err_str );
                return 0;
            }
        }

        
        if ( display_blobs.BLOBS_DISPLAY == 0 ) {

            ROW_COL_PTR[0].len    = -1;
            ROW_COL_PTR[0].val    = NULL;
            ROW_COL_PTR[0].is_hex = 0;

        } else {

            if ( display_blobs.BLOB_AS_HEX == 0 ) {

                ROW_COL_PTR[0].val = (char*)malloc( ( length ) + 16 );
                if ( ROW_COL_PTR[0].val == NULL ) {

                    set_error_desc( "can not allocate mem for value MYSQL_TYPE_BLOB" );
                    error_code_ptr[0] = (uint32_t)__LINE__;
                    return 0;
                }
                memset( ROW_COL_PTR[0].val, 0, ( length ) + 16 );
                memcpy( ROW_COL_PTR[0].val, ptr, length );
                ROW_COL_PTR[0].len = length;
                ROW_COL_PTR[0].is_hex = 0;

            } else {

                ROW_COL_PTR[0].val = (char*)malloc( ( length * 2 ) + 16 );
                if ( ROW_COL_PTR[0].val == NULL ) {

                    set_error_desc( "can not allocate mem for value MYSQL_TYPE_BLOB" );
                    error_code_ptr[0] = (uint32_t)__LINE__;
                    return 0;
                }
                this->sprint_as_hex( ROW_COL_PTR[0].val, ( const char* )ptr, length );
                ROW_COL_PTR[0].len = strlen( ROW_COL_PTR[0].val );
                ROW_COL_PTR[0].is_hex = 1;
            }
        }

        return blob_retval;
    }

    case MYSQL_TYPE_VARCHAR:
    case MYSQL_TYPE_VAR_STRING: {
    
        length = meta;
        
        if (!ptr) {
        
            return 0;
        }

        uint32_t  real_len = 0;
        if ( length < 256 ) {
        
            real_len = *ptr;

        } else {
        
            real_len = uint2korr(ptr);
        }

        ROW_COL_PTR[0].val = (char*)malloc( real_len + 16 );
        if ( ROW_COL_PTR[0].val == NULL ) {

            set_error_desc( "can not allocate mem for value MYSQL_TYPE_VARCHAR-MYSQL_TYPE_VAR_STRING" );
            error_code_ptr[0] = (uint32_t)__LINE__;
            return 0;
        }
        memset( ROW_COL_PTR[0].val, 0, real_len + 16 );
        size_t r = my_b_print_with_length_2( ptr, length, ROW_COL_PTR[0].val );
        ROW_COL_PTR[0].len = real_len;
        return r;
    }

    case MYSQL_TYPE_STRING: {
        
        if (!ptr) {
        
            return 0;
        }

        uint32_t  real_len = 0;
        if ( length < 256 ) {
        
            real_len = *ptr;

        } else {
        
            real_len = uint2korr(ptr);
        }

        ROW_COL_PTR[0].val = (char*)malloc( real_len + 16 );
        if ( ROW_COL_PTR[0].val == NULL ) {

            set_error_desc( "can not allocate mem for value MYSQL_TYPE_STRING" );
            error_code_ptr[0] = (uint32_t)__LINE__;
            return 0;
        }
        memset( ROW_COL_PTR[0].val, 0, real_len + 16 );
        size_t r = my_b_print_with_length_2( ptr, length, ROW_COL_PTR[0].val );
        ROW_COL_PTR[0].len = real_len;
        return r;
    }


    case MYSQL_TYPE_JSON: {

        if ( meta != 4 ) {

            char err_str[900];
            snprintf( err_str, 900, "!!Unknown JSON len of len=%d",  meta );
            error_code_ptr[0] = (uint32_t)__LINE__;
            set_error_desc( err_str );
            return 0;
        }

        if (!ptr) {
        
            if ( display_blobs.BLOBS_DISPLAY == 0 ) {

                ROW_COL_PTR[0].len    = -1;
            } else {
                ROW_COL_PTR[0].len    = 0;
            }
            ROW_COL_PTR[0].val    = NULL;
            ROW_COL_PTR[0].is_hex = 0;
            return 0;
        }
        length = uint4korr(ptr);
        ptr += 4;
        

        if ( display_blobs.BLOBS_DISPLAY == 0 ) {

            ROW_COL_PTR[0].len    = -1;
            ROW_COL_PTR[0].val    = NULL;
            ROW_COL_PTR[0].is_hex = 0;

        } else {

            if ( is_partial ) {
  
                try { //++*++


                    size_t  length_x = length;
                    const   uchar *p = ptr;
                    const   uchar *start_p = p;
                    size_t  start_length = length_x;


                    // Print paths and values.
                    p = start_p;
                    length_x = start_length;
                    
                    StringBuffer<STRING_BUFFER_USUAL_SIZE> buf;
                    buf.set_charset(&my_charset_utf8mb4_bin);
                    
                    while ( length_x ) {
                        // Read operation
                        enum_json_diff_operation operation = (enum_json_diff_operation)*p;

                        if ( (int)operation >= JSON_DIFF_OPERATION_COUNT ) {

                            error_code_ptr[0] = (uint32_t)__LINE__;
                            set_error_desc( "Invalid PARTIAL_JSON operation type!!!" );
                            return 0;
                        }

                        if ( operation == enum_json_diff_operation::REPLACE ) {

                            if ( buf.append( "REPLACE", strlen("REPLACE") ) ) {

                                error_code_ptr[0] = (uint32_t)__LINE__;
                                set_error_desc( "Invalid PARTIAL_JSON string append!!!" );
                                return 0;
                            }
                        }
                        if ( operation == enum_json_diff_operation::INSERT ) {

                            if ( buf.append( "INSERT", strlen("INSERT") ) ) {

                                error_code_ptr[0] = (uint32_t)__LINE__;
                                set_error_desc( "Invalid PARTIAL_JSON string append!!!" );
                                return 0;
                            }
                        }
                        if ( operation == enum_json_diff_operation::REMOVE ) {

                            if ( buf.append( "REMOVE", strlen("REMOVE") ) ) {

                                error_code_ptr[0] = (uint32_t)__LINE__;
                                set_error_desc( "Invalid PARTIAL_JSON string append!!!" );
                                return 0;
                            }
                        }
                        p++;
                        length_x--;

                        // Read path length
                        size_t path_length;
                        if ( net_field_length_checked<size_t>(&p, &length_x, &path_length) ) {

                            error_code_ptr[0] = (uint32_t)__LINE__;
                            set_error_desc( "Invalid PARTIAL_JSON field length!!!" );
                            return 0;
                        }

                        // Print path
                        if ( buf.append( ( const char* )p, path_length ) ) {

                            error_code_ptr[0] = (uint32_t)__LINE__;
                            set_error_desc( "Invalid PARTIAL_JSON string append!!!" );
                            return 0;
                        }
                        p += path_length;
                        length_x -= path_length;

                        if ( buf.append( ":", strlen(":") ) ) {

                            error_code_ptr[0] = (uint32_t)__LINE__;
                            set_error_desc( "Invalid PARTIAL_JSON string append!!!" );
                            return 0;
                        }


                        if ( operation != enum_json_diff_operation::REMOVE ) {

                            // Read value length
                            size_t value_length;
                            if ( net_field_length_checked<size_t>(&p, &length_x, &value_length) ) {

                                error_code_ptr[0] = (uint32_t)__LINE__;
                                set_error_desc( "Invalid PARTIAL_JSON field length!!!" );
                                return 0;
                            }

                            // Read value
                            json_binary::Value value = json_binary::parse_binary((const char *)p, value_length);
                            p += value_length;
                            length_x -= value_length;
                            if ( value.type() == json_binary::Value::ERROR ) {

                                error_code_ptr[0] = (uint32_t)__LINE__;
                                set_error_desc( "Invalid PARTIAL_JSON JSON part!!!" );
                                return 0;
                            }

                            StringBuffer<STRING_BUFFER_USUAL_SIZE> sp;
                            Json_wrapper wrapper(value); 

                            wrapper.to_string( &sp, false, "Rows_log_event_2::log_event_get_value_2" );
                            
                            if ( buf.append( sp.c_ptr(), sp.length() ) ) {

                                error_code_ptr[0] = (uint32_t)__LINE__;
                                set_error_desc( "Invalid PARTIAL_JSON string append!!!" );
                                return 0;
                            }

                        }

                        if ( length_x == 0 ) {

                            ;;;
                            // total end
                        }

                        if ( length_x != 0 ) {

                            ;;;
                            // end one operation
                            if ( buf.append( "\n", strlen("\n") ) ) {

                                    error_code_ptr[0] = (uint32_t)__LINE__;
                                    set_error_desc( "Invalid PARTIAL_JSON string append!!!" );
                                    return 0;
                            }
                        }
                    }

                    size_t json_str_len = strlen( buf.c_ptr() );

                    ROW_COL_PTR[0].val = (char*)malloc( json_str_len + 16 );
                    if ( ROW_COL_PTR[0].val == NULL ) {

                        set_error_desc( "can not allocate mem for value MYSQL_TYPE_JSON PARTIAL_JSON UPDATE" );
                        error_code_ptr[0] = (uint32_t)__LINE__;
                        return 0;
                    }
                    memset( ROW_COL_PTR[0].val, 0, json_str_len + 16 );
                    snprintf( ROW_COL_PTR[0].val, json_str_len+1, "%s",  buf.c_ptr() );
                    ROW_COL_PTR[0].len = strlen( ROW_COL_PTR[0].val );
                    ROW_COL_PTR[0].is_hex = 0;

                    
                } //++*++
                catch ( const std::exception& e ) {

                    error_code_ptr[0] = (uint32_t)__LINE__;
                    set_error_desc( e.what() );
                    return 0;
                }
                catch ( ... ) {

                    error_code_ptr[0] = (uint32_t)__LINE__;
                    set_error_desc( "expection catched!!!" );
                    return 0;
                }  

            } else {

                try {
                
                    json_binary::Value value = json_binary::parse_binary((const char *)ptr, length);
                    if ( value.type() == json_binary::Value::ERROR ) {
            
                        error_code_ptr[0] = (uint32_t)__LINE__;
                        set_error_desc( "Invalid JSON" );
                        return 0;
                    }

                    StringBuffer<STRING_BUFFER_USUAL_SIZE> s;

                    Json_wrapper wrapper(value);
                    wrapper.to_string( &s, false, "Rows_log_event_2::log_event_get_value_2" );

                    size_t json_str_len = strlen( s.c_ptr() );

                    ROW_COL_PTR[0].val = (char*)malloc( json_str_len + 16 );
                    if ( ROW_COL_PTR[0].val == NULL ) {

                        set_error_desc( "can not allocate mem for value MYSQL_TYPE_JSON" );
                        error_code_ptr[0] = (uint32_t)__LINE__;
                        return 0;
                    }
                    memset( ROW_COL_PTR[0].val, 0, json_str_len + 16 );
                    snprintf( ROW_COL_PTR[0].val, json_str_len+1, "%s",  s.c_ptr() );
                    ROW_COL_PTR[0].len = strlen( ROW_COL_PTR[0].val );
                    ROW_COL_PTR[0].is_hex = 0;

                }
                catch ( const std::exception& e ) {

                    error_code_ptr[0] = (uint32_t)__LINE__;
                    set_error_desc( e.what() );
                    return 0;
                }
                catch ( ... ) {

                    error_code_ptr[0] = (uint32_t)__LINE__;
                    set_error_desc( "expection catched!!!" );
                    return 0;
                }
            }
        }

        return length + meta;
    }


    case MYSQL_TYPE_GEOMETRY: {

        if ( meta != 4 ) {

            char err_str[900];
            snprintf( err_str, 900, "!!Unknown GEOMETRY len of len=%d",  meta );
            error_code_ptr[0] = (uint32_t)__LINE__;
            set_error_desc( err_str );
            return 0;
        }

        if (!ptr) {
        
            if ( display_blobs.BLOBS_DISPLAY == 0 ) {

                ROW_COL_PTR[0].len    = -1;
            } else {
                ROW_COL_PTR[0].len    = 0;
            }
            ROW_COL_PTR[0].val    = NULL;
            ROW_COL_PTR[0].is_hex = display_blobs.GEOMETRY_AS_HEX;
            return 0;
        }
        length = uint4korr(ptr);
        ptr += 4;


        if ( display_blobs.BLOBS_DISPLAY == 0 ) {

           ROW_COL_PTR[0].len    = -1;
           ROW_COL_PTR[0].val    = NULL;
           ROW_COL_PTR[0].is_hex = 0;

        } else {

           if ( display_blobs.GEOMETRY_AS_HEX == 0 ) {

                ROW_COL_PTR[0].val = (char*)malloc( ( length ) + 16 );
                if ( ROW_COL_PTR[0].val == NULL ) {

                    set_error_desc( "can not allocate mem for value MYSQL_TYPE_GEOMETRY" );
                    error_code_ptr[0] = (uint32_t)__LINE__;
                    return 0;
                }
                memset( ROW_COL_PTR[0].val, 0, ( length ) + 16 );
                memcpy( ROW_COL_PTR[0].val, ptr, length );
                ROW_COL_PTR[0].len = length;
                ROW_COL_PTR[0].is_hex = 0;

           } else {

                ROW_COL_PTR[0].val = (char*)malloc( ( length * 2 ) + 16 );
                if ( ROW_COL_PTR[0].val == NULL ) {

                    set_error_desc( "can not allocate mem for value MYSQL_TYPE_GEOMETRY" );
                    error_code_ptr[0] = (uint32_t)__LINE__;
                    return 0;
                }
                
                this->sprint_as_hex( ROW_COL_PTR[0].val, ( const char* )ptr, length );

                ROW_COL_PTR[0].len = strlen( ROW_COL_PTR[0].val );
                ROW_COL_PTR[0].is_hex = 1;
            }
        }

        return length + meta;
    }
    default: {

        char tmp[5];
        snprintf(tmp, sizeof(tmp), "%04x", meta);

        char err_str[900];
        snprintf( err_str, 900, "!! Don't know how to handle column type=%d meta=%d (%s)", type, meta, tmp  );
        error_code_ptr[0] = (uint32_t)__LINE__;
        set_error_desc( err_str );
        return 0;
    }
    break;
}

return 0;
}



void Rows_log_event_2::my_b_write_bit_2( const uchar *ptr, uint nbits, char* out_buff ) {

    uint bitnum, nbits8 = ((nbits + 7) / 8) * 8, skip_bits = nbits8 - nbits;

    snprintf( out_buff, 2, "b" );
    for (bitnum = skip_bits; bitnum < nbits8; bitnum++) {

        int is_set = (ptr[(bitnum) / 8] >> (7 - bitnum % 8)) & 0x01;
        snprintf( &out_buff[ strlen( out_buff ) ], 2, (is_set ? "1" : "0") );
    }
}

size_t Rows_log_event_2::my_b_print_with_length_2( const uchar *ptr, uint length, char* out_buff ) {

    snprintf( out_buff, 2, "%s", "" );

    if ( length < 256 ) {

        length = *ptr;
        for ( uint i=0; i<length; i++ ) {

            snprintf( &out_buff[ strlen( out_buff ) ], 2, "%c", ( int )((ptr + 1 + i)[0]) );
        }
        return length + 1;
    
    } else {
    
        length = uint2korr(ptr);
        for ( uint i=0; i<length; i++ ) {

            snprintf( &out_buff[ strlen( out_buff ) ], 2, "%c", ( int )((ptr + 2 + i)[0]) );
        }
        return length + 2;
    }

    return 0;
}


/* Print binary value as hex literal (0x ...) */
void Rows_log_event_2::sprint_as_hex( char *dst, const char *str, int64 len ) {


  const char *ptr = str, *end = ptr + len;
  //int64 i;
  snprintf( dst, 3, "%s" , "0x" );
  for ( ; ptr < end; ptr++ ) {

      snprintf( &dst[ strlen( dst ) ], 3, "%02x", *(pointer_cast<const uchar *>(ptr)));
  } 

}






