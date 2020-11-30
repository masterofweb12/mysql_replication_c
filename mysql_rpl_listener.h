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


#ifndef __REPL_LOG_LISTENER
#define __REPL_LOG_LISTENER


#include <stdio.h>
#include <string.h>

#include "mysql_rpl_listener_row.h"


// должно быть определено так же, как CREATED, INPROCESS_1 и INPROCESS
// в файле mysql_rpl_listener_2/mysql_rpl_listener_2.h
#define REPL_PROC_STATUS_OFF     0
#define REPL_PROC_STATUS_PHASE_1 80
#define REPL_PROC_STATUS_PHASE_2 90



typedef void* RPL_LISTENER_H;
typedef void* RPL_TABLE_MAPS;


#ifdef __cplusplus
extern "C" {
#endif


/**
 *    --- mysql_rpl_listener__init() ---
 *
 *     EN
 *     --
 * 
 *     This function must be called first to initialize the LISTENER. 
 *     If successful, the function returns 0, and nonzero value if an error occurs.
 *     mysql_rpl_listener__init() initializes the RPLEPLICATION LISTENER object and writes it to rpl_h.
 *     Memory allocated when calling mysql_rpl_listener__init() must be freed by 
 *     calling mysql_rpl_listener__destroy().
 * 
 *    
 *     on_rpl_row () - a function that will be called when LISTENER receives a ROW-EVENT.
 *                     ROW-EVENTS are generated by the server when modifying the contents of tables and contain
 *                     data from changed rows.
 *                     To this function will be passed a parameter of type _repl_log_x_rows* which will point to
 *                     changed rows ( you do not need to free the memory allocated for the _repl_log_x_rows structure,
 *                     it will be freed automatically at exit from on_rpl_row() ) function, and it can also
 *                     an arbitrary pointer be passed, the value of which is passed by the on_rpl_row_param_ptr parameter
 *                     of the mysql_rpl_listener__init() function.
 * 
 * 
 * 
 *     on_rpl_heartbeat () - a function that will be called when the LISTENER receives a HEARTBEAT-EVENT.
 *                           HEARTBEAT-EVENTS are generated by the server during inactivity moments so that 
 *                           client can understand that the server is still alive. These EVENTS can be used to 
 *                           determine how far we have lagged behind the server by implementing the changes that 
 *                           have occurred on it in our program.
 *                           To this function will be passed a parameter of type _repl_log_x_heartbeat* which will point
 *                           to the structure with HEARTBEAT-EVENT data ( free the memory allocated for the structure
 *                           _repl_log_x_heartbeat is not needed, it will be deallocated automatically at exit
 *                           from  on_rpl_heartbeat() function ).
 *                           An arbitrary pointer can also be passed there, the value of which is passed
 *                           with the on_rpl_heartbeat_param_ptr parameter of the mysql_rpl_listener__init() function.
 * 
 * 
 *     You can read more about EVENTS here
 *     https://dev.mysql.com/doc/dev/mysql-server/latest/page_protocol_replication_binlog_event.html
 * 
 * 
 * 
 *     my_host,
 *     my_user,
 *     my_pass
 *     and
 *     my_port - parameters for connecting to the MySQL database.
 *     User my_user must have REPLICATION SLAVE and REPLICATION CLIENT privileges.
 *     Otherwise, replication will not be possible.
 * 
 * 
 * 
 *     slave_id - the replication protocol assumes that
 *               each member has a unique identifier within the group of servers involved in replication.
 *               Because we are a slave for the master-server, this is our slave id.
 *    
 * 
 * 
 *     blobs_display {0 | 1} - a variable responsible for whether the data on changed BLOB columns will be displayed. 
 *                             0 - will not. 1 - will be.
 *                             It should be noted that BLOB should be understood as columns 
 *                             of the following types BLOB, TEXT, JSON, GEOMETRY.
 *                             If blobs_display=0, then geometry_as_hex and blob_as_hex parameters are ignored.
 * 
 * 
 * 
 *     geometry_as_hex {0 | 1} - a variable responsible for whether fields of type GEOMETRY will be displayed
 *                               in hexadecimal notation, or as raw BLOB
 *                               ( because, in essence, the GEOMETRY type is BLOB ).
 *                               This variable is not ignored only if blobs_display=1.
 * 
 * 
 * 
 *     blob_as_hex {0 | 1} - a variable responsible for whether fields of type BLOB ( except GEOMETRY type ) will be   
 *                           displayed in hexadecimal notation, or as raw BLOB. 
 *                           ( this also includes TEXT and JSON types ).
 *                           This variable is not ignored only if blobs_display=1.
 * 
 * 
 * 
 *     on_rpl_transaction () - a function that will be called when LISTENER receives events that indicate
 *                             beginning and end of each transaction. A variable will be passed to it pointint
 *                             to a structure of type _repl_log_x_transaction, which has a field transaction_event_type,
 *                             which can be set to TRANSACTION_EV_TYPE_START to start transaction
 *                             or to TRANSACTION_EV_TYPE_COMMIT to complete it
 *                             ( there is no need to free the memory allocated for the _repl_log_x_transaction structure,
 *                             it will be deallocated automatically at exit of on_rpl_transaction () function ).
 *                             An arbitrary pointer can also be passed there, the value of which is passed
 *                             with the on_rpl_transaction_param_ptr parameter of the mysql_rpl_listener__init() function.
 * 
 * 
 *     master_file_name - name of the binary log file on the server
 * 
 * 
 *     master_file_pos - position in the file (ignored if master_file_name = NULL)
 * 
 * 
 *     tbl_maps_ptr - pointer to the collection of TABLEMAP-EVENTs (ignored if master_file_name = NULL)
 * 
 * 
 *   
 * 
 * 
 * 
 *    RUS
 *    ---
 * 
 *    Эта функция должна быть вызвана первой для инициализации LISTENERа.
 *    В случае успеха функция возвращает 0, в случае же ошибки - значение отличное от 0.
 *    mysql_rpl_listener__init() инициализирует объект  RPLEPLICATION LISTENER и записывет его в rpl_h.
 *    Память выделенная при вызове mysql_rpl_listener__init() обязательно должна быть освобождена 
 *    вызовом mysql_rpl_listener__destroy().
 * 
 * 
 *    on_rpl_row() - функция, которая будет вызвана при получении LISTENERом ROW-EVENTа.
 *                   ROW-EVENTы генерируются сервером при модификации содержимого таблиц и содержат в себе 
 *                   данные из изменённых строк.
 *                   В эту функцию будет передан параметр типа _repl_log_x_rows* который будет указывать на 
 *                   изменённые строки ( освобождать память выделенную под структуру _repl_log_x_rows не нужно, 
 *                   она будет освобождена автоматически при выходе из функции on_rpl_row() ), а также туда может  
 *                   быть передан произвольный указатель, значение которого передаётся парметром on_rpl_row_param_ptr 
 *                   функции mysql_rpl_listener__init().
 * 
 * 
 *    on_rpl_heartbeat() - функция, которая будет вызвана при получении LISTENERом HEARTBEAT-EVENTа.
 *                         HEARTBEAT-EVENTы генерерируются сервером в моменты бездействия, чтоб клиент мог понимать, 
 *                         что сервер ещё жив. Эти EVENTы можно использовать для определения того, насколько далеко 
 *                         мы отстали от сервера реализуя изменения произошедшие на нём в своей программе. 
 *                         В эту функцию будет передан параметр типа _repl_log_x_heartbeat* который будет указывать 
 *                         на структуру с данными HEARTBEAT-EVENTа ( освобождать память выделенную под структуру 
 *                         _repl_log_x_heartbeat не нужно, она будет освобождена автоматически при выходе из 
 *                         функции on_rpl_heartbeat() ). 
 *                         Также туда может быть передан произвольный указатель, значение которого передаётся 
 *                         парметром on_rpl_heartbeat_param_ptr функции mysql_rpl_listener__init().
 *
 * 
 *    Детальнее о EVENTах можно прочитать здесь
 *    https://dev.mysql.com/doc/dev/mysql-server/latest/page_protocol_replication_binlog_event.html
 *
 *    
 *    my_host, 
 *    my_user, 
 *    my_pass
 *    и 
 *    my_port  - параметры подключения к БД MySQL.
 *               Пользователь my_user должен обладать привелегиями  REPLICATION SLAVE и REPLICATION CLIENT.
 *               В противном случае репликация будет невозможна.
 *  
 *
 *      
 *    slave_id - протокол репликации предполагает то, 
 *               что каждый участник имеет уникальный идентификатор в пределах группы серверов, участвующих в репликации.
 *               Поскольку мы являемся для сервера слейвом, то это наш id слейва. 
 *
 *
 *
 *    blobs_display { 0 | 1 } - переменная отвечающая за то, будут ли отображаться данные по изменённым столбцам типа BLOB.
 *                              0 - не будут. 1 - будут.
 *                              Следует заметить, что под BLOB попадают столбцы следующих типов BLOB, TEXT, JSON, GEOMETRY.
 *                              Если blobs_display=0, то параметры geometry_as_hex и blob_as_hex не учитываются. 
 *
 *
 *
 *    geometry_as_hex { 0 | 1 } - переменная отвечающая за то, буду ли поля типа GEOMETRY отображаться 
 *                                в шестнадцатиричном представлении, либо как BLOB 
 *                                ( потому, что по своей сути тип GEOMETRY и есть BLOB ).
 *                                Эта переменная имет смысл только если blobs_display=1.
 *
 *
 *
 *    blob_as_hex { 0 | 1 } - переменная отвечающая за то, буду ли поля типа BLOB ( за исключением GEOMETRY )
 *                            отображаться в шестнадцатиричном представлении, либо как BLOB в сыром 
 *                            ( сюда же относятся TEXT и JSON ).
 *                            Эта переменная имет смысл только если blobs_display=1.
 *
 * 
 *
 *    on_rpl_transaction() - функция, которая будет вызвана при получении LISTENERом 
 *                           в начале и в конце каждой транзакции. В неё будет передана переменная указывающая 
 *                           на структуру типа _repl_log_x_transaction, которая имеет поле transaction_event_type, 
 *                           могущее принимать значение TRANSACTION_EV_TYPE_START для начала транзакции 
 *                           или TRANSACTION_EV_TYPE_COMMIT для её завершения 
 *                           ( освобождать память выделенную под структуру _repl_log_x_transaction не нужно, 
 *                           она будет освобождена автоматически при выходе из функции on_rpl_transaction() ).
 *                           Также туда может  быть передан произвольный указатель, значение которого передаётся 
 *                           парметром on_rpl_transaction_param_ptr функции mysql_rpl_listener__init().
 * 
 * 
 *    master_file_name - имя файла бинарного журнала на сервере
 * 
 * 
 *    master_file_pos - позиция в файле ( игнорируется, если master_file_name=NULL )
 * 
 * 
 *    tbl_maps_ptr - указатель на колекцию TABLEMAP-EVENTов ( игнорируется, если master_file_name=NULL )
 */
int mysql_rpl_listener__init(   RPL_LISTENER_H *rpl_h,
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
                                RPL_TABLE_MAPS *tbl_maps_ptr );



/**
 *    --- mysql_rpl_listener__table_add() ---
 * 
 *    EN
 *    --
 *    
 *    This function should be called second, right after a successful call to mysql_rpl_listener__init().
 *    Calling mysql_rpl_listener__table_add() in a different order will return an error.
 *    On successful function returns 0, but if an error occurs, a value other than 0.
 *    mysql_rpl_listener__table_add() is used to add watched tables to LISTENER.
 *    If no tables are added, replication will fail.
 * 
 *    rpl_h - pointer to LISTENER's structure
 * 
 *    db_name - MySQL database name
 * 
 *    tbl_name - name of table
 * 
 * 
 * 
 *    
 *    RUS
 *    ---
 * 
 *    Эта функция должна быть вызвана второй, сразу после успешного вызова mysql_rpl_listener__init().
 *    При другом порядке вызова mysql_rpl_listener__table_add() возвратит ошибку.
 *    В случае успеха функция возвращает 0, в случае же ошибки - значение отличное от 0.
 *    mysql_rpl_listener__table_add() служит для добавления отслеживаемых таблиц в LISTENER.
 *    Если ни одной таблицы не будет добавлено, то репликация будет невозможна.
 *  
 *    rpl_h - указатель на структуру LISTENERа
 * 
 *    db_name - имя базы данных MySQL
 * 
 *    tbl_name - имя таблицы
 */
int mysql_rpl_listener__table_add( RPL_LISTENER_H *rpl_h, const char* db_name, const char* tbl_name );


/**
 *    --- mysql_rpl_listener__prepare() ---
 * 
 *    EN
 *    --
 * 
 *    This function must be called after calling mysql_rpl_listener__table_add().
 *    Calling mysql_rpl_listener__prepare() in a different order will return an error.
 *    The function is used to prepare LISTENER to start replication.
 *    If successful, the function returns 0, but if an error occurs, a value other than 0.
 * 
 *    rpl_h - pointer to LISTENER's structure
 * 
 *    error_ptr - pointer to the _repl_log_x_error structure, where in case of errors
 *                error description will be posted
 * 
 * 
 * 
 *    
 *    RUS
 *    ---
 * 
 *    Эта функция должна быть вызвана после вызова mysql_rpl_listener__table_add().
 *    При другом порядке вызова mysql_rpl_listener__prepare() возвратит ошибку.
 *    Функция служит для подготовки LISTENERа к старту репликации. 
 *    В случае успеха функция возвращает 0, в случае же ошибки - значение отличное от 0.
 * 
 *    rpl_h - указатель на структуру LISTENERа
 * 
 *    error_ptr - указатель на структуру _repl_log_x_error, куда в случае возникновения ошибок 
 *                будет помещено описание ошибки
 */
int mysql_rpl_listener__prepare( RPL_LISTENER_H *rpl_h, _repl_log_x_error *error_ptr );


/**
 *    --- mysql_rpl_listener__set_position() ---
 * 
 *    EN
 *    --
 * 
 *    This function must be called strictly after mysql_rpl_listener__prepare()
 *    The function is used to set the binary log file on the server and the position in it,
 *    If you pass NULL as the name of the log file, the position will be set automatically
 *    from now.
 *    If successful, the function returns 0, but if an error occurs, a value other than 0.
 * 
 *    rpl_h - pointer to LISTENER's structure
 * 
 *    file_name - name of the binary log file on the server
 * 
 *    pos - position in the file (ignored if file_name = NULL)
 * 
 *    error_ptr - pointer to the _repl_log_x_error structure, where in case of errors
 *                error description will be posted
 * 
 * 
 * 
 *    
 *    RUS
 *    ---
 * 
 *    Эта функция должна быть вызвана строго после mysql_rpl_listener__prepare()
 *    Функция служит для установки файла бинарного журнала на сервере и позиции в нём,
 *    Если в качестве имени файла журнала передать NULL позиция будет установлена автоматически
 *    с текущего момента.
 *    В случае успеха функция возвращает 0, в случае же ошибки - значение отличное от 0.
 * 
 *    rpl_h - указатель на структуру LISTENERа
 * 
 *    file_name - имя файла бинарного журнала на сервере
 * 
 *    pos - позиция в файле ( игнорируется, если file_name=NULL )
 * 
 *    error_ptr - указатель на структуру _repl_log_x_error, куда в случае возникновения ошибок 
 *                будет помещено описание ошибки
 *
 * int mysql_rpl_listener__set_position( RPL_LISTENER_H *rpl_h, char *file_name, uint64_t pos, _repl_log_x_error *error_ptr );
 */

/**
 *    --- mysql_rpl_listener__start() ---
 * 
 *    EN
 *    --
 * 
 *    The function is used to start replication.
 *    Should be called after mysql_rpl_listener__set_position(), or after calling
 *    mysql_rpl_listener__set_tablemaps().
 *    If successful, the function returns 0, but if an error occurs, a value other than 0.
 * 
 *    rpl_h - pointer to LISTENER's structure
 * 
 *    error_ptr - pointer to the _repl_log_x_error structure, where in case of errors
 *                error description will be posted
 * 
 * 
 * 
 *    
 *    RUS
 *    ---
 * 
 *    Функция служит для запуска репликации.
 *    Должна быть вызвана после mysql_rpl_listener__set_position(), либо после вызова 
 *    mysql_rpl_listener__set_tablemaps().
 *    В случае успеха функция возвращает 0, в случае же ошибки - значение отличное от 0.
 * 
 *    rpl_h - указатель на структуру LISTENERа
 *    
 *    error_ptr - указатель на структуру _repl_log_x_error, куда в случае возникновения ошибок 
 *                будет помещено описание ошибки  
 */
int mysql_rpl_listener__start( RPL_LISTENER_H *rpl_h, _repl_log_x_error *error_ptr );


/**
 *    --- mysql_rpl_listener__stop() ---
 * 
 *    EN
 *    --
 * 
 *    The function is used to stop replication.
 *    If successful, the function returns 0, but if an error occurs, a value other than 0.
 * 
 *    rpl_h - pointer to LISTENER's structure
 *    
 * 
 * 
 *    
 *    RUS
 *    ---
 * 
 *    Функция служит для остановки репликации.
 *    В случае успеха функция возвращает 0, в случае же ошибки - значение отличное от 0.
 * 
 *    rpl_h - указатель на структуру LISTENERа
 */
int mysql_rpl_listener__stop( RPL_LISTENER_H *rpl_h );


/**
 *    --- mysql_rpl_listener__destroy() ---
 * 
 *    EN
 *    --
 * 
 *    The function is used to free the memory allocated in the LISTENER structure.
 *    If successful, the function returns 0, but if an error occurs, a value other than 0.
 * 
 *    rpl_h - pointer to LISTENER's structure
 *    
 * 
 * 
 *    
 *    RUS
 *    ---
 * 
 *    Функция служит для очистки памяти выделенной в структуре LISTENERа.
 *    В случае успеха функция возвращает 0, в случае же ошибки - значение отличное от 0.
 *    
 *    rpl_h - указатель на структуру LISTENERа
 */
int mysql_rpl_listener__destroy( RPL_LISTENER_H *rpl_h );


/**
 *    --- mysql_rpl_listener__in_process() ---
 * 
 *    EN
 *    --
 * 
 *    This function is used to check the status of replication.
 *    If replication is running, the function returns REPL_PROC_STATUS_PHASE_1
 *    or REPL_PROC_STATUS_PHASE_2, if replication has ended for some reason
 *    or hasn't started yet, then mysql_rpl_listener__in_process() will return REPL_PROC_STATUS_OFF.
 *    If the function returned REPL_PROC_STATUS_OFF and replication was started before, then error status
 *    can be getted by analyzing the _repl_log_x_error structure that was passed
 *    when calling the mysql_rpl_listener__start() function.
 *    REPL_PROC_STATUS_PHASE_1 status means that replication has started successfully, but
 *    replication thread is waiting for the moment when it will be possible to change status to REPL_PROC_STATUS_PHASE_2.
 *    When starting replication from an arbitrary location in an arbitrary file, we cannot be sure that
 *    for every ROW-EVENT we get, we got a corresponding TABLEMAP-EVENT,
 *    therefore, the replication thread is waiting for the moment from which it will be possible to say that in the future
 *    ROW-EVENTS can be assigned to the required TABLEMAP-EVENTS.
 *    After that it change replication status to REPL_PROC_STATUS_PHASE_2.
 *    Being in the REPL_PROC_STATUS_PHASE_1 status, the replication thread ignores the beginning and end of transactions,
 *    as well as ROW-EVENTS.
 *    REPL_PROC_STATUS_PHASE_2 status means that replication has started successfully and ROW-EVENTs will be
 *    processed correctly.
 *    If mysql_rpl_listener__set_position() was called with a log file and position ( file_name!= NULL ),
 *    then the REPL_PROC_STATUS_PHASE_1 status will be skipped and the replication stream will go immediately
 *    to the REPL_PROC_STATUS_PHASE_2 status.
 *    This is done because if you are running replication from some place known to you in advance,
 *    then you probably took steps to prevent replication from crashing. For example, you put the server in READ_ONLY mode
 *    and waited enough time for all queries that modify tables to complete.
 *    Or you already had LISTENER running, but replication for some reason has 
 *    failed ( for example, network problems ), and you created a new LISTENER, saving from the old LISTENER last file and position 
 *    using mysql_rpl_listener__get_last_sucess_pos() function ,
 *    and saving the TABLEMAP-EVENT collection using the mysql_rpl_listener__get_tablemaps() function,
 *    and passed these parameters to mysql_rpl_listener__set_position() and mysql_rpl_listener__set_tablemaps().
 * 
 * 
 * 
 *    
 *    RUS
 *    ---
 * 
 *    Функция служит для того, чтоб проверять состояние репликации.
 *    Если репликация работает, функция возвращает либо REPL_PROC_STATUS_PHASE_1 
 *    либо REPL_PROC_STATUS_PHASE_2, если же репликация по каким-либо причинам завершилась 
 *    или ещё не началась, то mysql_rpl_listener__in_process() вернёт REPL_PROC_STATUS_OFF.
 *    Если функция вернула REPL_PROC_STATUS_OFF и репликация до этого была запущена, то статус 
 *    ошибки можно посмотреть проанализировав структуру _repl_log_x_error, которая была передана 
 *    при вызове функции mysql_rpl_listener__start().
 *    Статус REPL_PROC_STATUS_PHASE_1 означает, что репликация успешно запустилась, однако 
 *    поток репликации ожидает момента, когда можно будет перейти в статус REPL_PROC_STATUS_PHASE_2.
 *    При запуске репликации с произвольного места в произвольном файле мы не можем быть уверены в том, 
 *    что для каждого ROW-EVENTа, который мы получим, мы получили соответствующий TABLEMAP-EVENT, 
 *    поэтому поток репликации ждёт того, момента, начиная с которого можно будет сказать, что в  дальнейшем 
 *    ROW-EVENTам можно будет поставить в соответсвие нужные TABLEMAP-EVENTы. 
 *    После этого он переводит статус репликации в REPL_PROC_STATUS_PHASE_2.  
 *    Находясь в статусе REPL_PROC_STATUS_PHASE_1 поток репликации игнорирует начало и конец транзакций, 
 *    а также ROW-EVENTы.
 *    Статус REPL_PROC_STATUS_PHASE_2 означает, что репликация успешно запущена и ROW-EVENTы будут 
 *    корректно обработаны.
 *    Если mysql_rpl_listener__set_position() была вызвана с указанием файла журнала и позиции ( file_name!=NULL ),
 *    то статус REPL_PROC_STATUS_PHASE_1 будет пропущен и поток репликации перейдёт сразу 
 *    в статус REPL_PROC_STATUS_PHASE_2.
 *    Это сделано потому, что если вы запускаете репликацию с какого либо наперёд известного вам места, 
 *    то наверняка вы предприняли меры, чтоб репликация не упала. Например вы перевели сервер в режим READ_ONLY 
 *    и выждали достаточно времени, чтоб все запросы, модифицирующие таблицы завершились.
 *    Либо у вас ранее уже был запущен LISTENER, но репликация по каким-либо причинам потерпела 
 *    неудачу ( например проблемы с сетью ), и вы создали новый LISTENER, сохранив со старого LISTENERа при
 *    помощи функции mysql_rpl_listener__get_last_sucess_pos() последний файл и позицию,
 *    а также при помощи функции mysql_rpl_listener__get_tablemaps() сохранив колекцию TABLEMAP-EVENTов, 
 *    которые были в LISTENERе на момент падения, и передали эти параметры в mysql_rpl_listener__set_position() и 
 *    в mysql_rpl_listener__set_tablemaps().
 * 
 *    rpl_h - указатель на структуру LISTENERа
 */
int mysql_rpl_listener__in_process( RPL_LISTENER_H *rpl_h ); 


/**
 *    --- mysql_rpl_listener__get_last_sucess_pos() ---
 * 
 *    EN
 *    --
 * 
 *    The function is used to get the last successfully processed by the replication stream
 *    log file and position in it.
 *    If successful, the function returns 0, but if an error occurs, a value other than 0.
 * 
 *    rpl_h - pointer to LISTENER's structure
 * 
 *    file_n - pointer to char array, where the name of the binary log file will be saved
 * 
 *    file_maxlen - length of the array passed through file_n
 * 
 *    pos - a pointer to a variable of the uint64_t type, where the position in the log file will be saved
 *    
 * 
 * 
 * 
 *    RUS
 *    ---
 * 
 *    Функция служит для того, чтоб получить последние успешно обработанные потоком репликации 
 *    файл журнала и позицию в нём.
 *    В случае успеха функция возвращает 0, в случае же ошибки - значение отличное от 0.
 * 
 *    rpl_h - указатель на структуру LISTENERа
 * 
 *    file_n - указатель на массив char, куда будет сохранено имя файла бинарного журнала
 * 
 *    file_maxlen - длина массива переданного через file_n
 * 
 *    pos - указатель на переменную uint64_t, куда будет сохранена позиция в файле журнала
 */
int mysql_rpl_listener__get_last_sucess_pos( RPL_LISTENER_H *rpl_h, char *file_n, int file_maxlen, uint64_t *pos );




/**
 *    --- mysql_rpl_listener__get_tablemaps() ---
 * 
 *    EN
 *    --
 * 
 *    This function is used to save the collection of TABLEMAP-EVENTS from the LISTENER.
 *    This may be needed if replication fails for some reason and
 *    we want to create a new LISTENER object to start replication from the same place.
 *    Because we are not sure that for all ROW-EVENTS that will follow after the replication start
 *    we will receive the corresponding TABLEMAP-EVENTS in the replication stream, then we will save before
 *    destruction of the old LISTENER it's TABLEMAP-EVENTs and transfer them to the new LISTENER.
 *    If successful, the function returns 0, but if an error occurs, a value other than 0.
 *    Should be called only after the replication thread has finished.
 *    Any other call order will result in an error.
 * 
 *    rpl_h - pointer to LISTENER's structure
 * 
 *    dst_tbl_maps - pointer to the collection of TABLEMAP-EVENTs
 * 
 *    error_ptr - pointer to the _repl_log_x_error structure, where in case of errors
 *                error description will be posted
 * 
 * 
 * 
 * 
 *    RUS
 *    ---
 * 
 *    Функция служит для сохранения колекции  TABLEMAP-EVENTов из LISTENERа.
 *    Это может понадобиться если репликация по каким-либо причинам упадёт и мы 
 *    захотим создать новый объект LISTENERа, запустив репликацию с того же места.
 *    Поскольку у нас нет уверенности, что для всех ROW-EVENTов которые последуют после старта 
 *    репликации мы получим в потоке репликации соответсвующие TABLEMAP-EVENTы, то мы сохраним перед
 *    уничтожением старого LISTENERа TABLEMAP-EVENTы и передадим их новому LISTENERу.
 *    В случае успеха функция возвращает 0, в случае же ошибки - значение отличное от 0.
 *    Должна вызываться только после завершения работы потока репликации.
 *    Любой другой порядок вызова приведёт к ошибке.
 * 
 *    rpl_h - указатель на структуру LISTENERа
 * 
 *    dst_tbl_maps - указатель на колекцию TABLEMAP-EVENTов
 * 
 *    error_ptr - указатель на структуру _repl_log_x_error, куда в случае возникновения ошибок 
 *                будет помещено описание ошибки
 */
int mysql_rpl_listener__get_tablemaps( RPL_LISTENER_H *rpl_h, RPL_TABLE_MAPS *dst_tbl_maps, _repl_log_x_error *error_ptr );


/**
 *    --- mysql_rpl_listener__set_tablemaps() ---
 * 
 *    EN
 *    --
 * 
 *    The function is used to transfer the collection of TABLEMAP-EVENTs to LISTENER.
 *    Its action is the opposite of mysql_rpl_listener__get_tablemaps() action.
 *    The function can only be called after calling mysql_rpl_listener__set_position()
 *    and before calling mysql_rpl_listener__start(). Otherwise, it will fail.
 *    If successful, the function returns 0, but if an error occurs, a value other than 0.
 * 
 *    rpl_h - pointer to LISTENER's structure
 * 
 *    src_tbl_maps - pointer to the collection of TABLEMAP-EVENTs
 *    
 *    error_ptr - pointer to the _repl_log_x_error structure, where in case of errors
 *                error description will be posted
 * 
 * 
 * 
 * 
 *    RUS
 *    ---
 * 
 *    Функция служит для передачи колекции  TABLEMAP-EVENTов в LISTENER.
 *    Её действие обратно действию mysql_rpl_listener__get_tablemaps().
 *    Функция может быть вызвана только после вызова mysql_rpl_listener__set_position() 
 *    и до вызова mysql_rpl_listener__start(). В противном случае она завершится с ошибкой.
 *    В случае успеха функция возвращает 0, в случае же ошибки - значение отличное от 0.
 * 
 *    rpl_h - указатель на структуру LISTENERа
 * 
 *    src_tbl_maps - указатель на колекцию TABLEMAP-EVENTов
 * 
 *    error_ptr - указатель на структуру _repl_log_x_error, куда в случае возникновения ошибок 
 *                будет помещено описание ошибки 
 *
 * int mysql_rpl_listener__set_tablemaps( RPL_LISTENER_H *rpl_h, RPL_TABLE_MAPS *src_tbl_maps, _repl_log_x_error *error_ptr );
 */

/**
 *    --- mysql_rpl_listener__free_tablemaps() ---
 * 
 *    EN
 *    --
 * 
 *    Frees resources allocated in the structure pointed to by the tbl_maps parameter.
 *    If successful, the function returns 0, but if an error occurs, a value other than 0.
 * 
 * 
 *    
 * 
 *    RUS
 *    ---
 * 
 *    Освобождает ресурсы выдеенные в структуре на которую указывает параметр tbl_maps.
 *    В случае успеха функция возвращает 0, в случае же ошибки - значение отличное от 0.
 */
int mysql_rpl_listener__free_tablemaps( RPL_TABLE_MAPS *tbl_maps );




#ifdef __cplusplus
}
#endif


#endif