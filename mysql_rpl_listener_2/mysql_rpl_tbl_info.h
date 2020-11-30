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


#ifndef __REPL_LOG_TBL_INFO
#define __REPL_LOG_TBL_INFO



typedef struct __col_info {

    uint8_t  type;
    char     name[128];
    uint8_t  is_unsigned;

} _col_info;

typedef struct __tbl_info {

    uint32_t    col_count; 
    _col_info   *columns;

} _tbl_info;


#endif
