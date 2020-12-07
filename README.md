# mysql_replication_c
The **mysql_replication_c** library is MySQL replication listener based on **MySQL 8.0** code.
Source code of **MySQL 8.0** is included in the code of this project as is
and cloned by git in **mysql_source/mysql/** directory.
**mysql_replication_c** library supports all features of MySQL 8.0 including  **Binary Log Transaction Compression** and **transaction payload events**.


# MySQL server variables

For the library to work correctly, the following server variables must have the following values

**binlog_format**=***ROW*** 

**binlog_row_image**=***FULL***

**binlog_row_metadata**=***FULL***

**gtid_mode**=***OFF***


# build

On ubuntu you need install some pakages:

    sudo apt install git gcc g++ make cmake rpl libssl-dev libncurses-dev pkg-config bison 9base
    
To build the library, you need to run the **build.sh** script included in the project.
This script will run the **cmake** utility for the **MySQL 8.0** distribution and after that, by calling the **rpl** utility, 
it will change the compilation of object files from position dependent code to position independent.
It replacing the compiler keys in the generated by **cmake** files from **-fPIE** to **-fPIC**.
After that, the compilation of object files will be launched and their subsequent linking to the shared library.

Now you can include the **mysql_rpl_listener.h** library header file in your projects and use 
the full power of **MySQL 8.0** replication in **C** or **C++**.



As an example, you can use the **example/C/rpl_test_2.c** file from the source code of the library project.
You need to run **example/C/build_test.sh** script to build example programm.

On ubuntu to build example you need install libmysqlclient-dev:

    sudo apt install libmysqlclient-dev



Building and using this library has been tested on **ubuntu 18.04** and **ubuntu 20.04**.




# library functions
```c
/**
 *    --- mysql_rpl_listener__init() ---
 * 
 *    This function must be called first to initialize the LISTENER. 
 *    If successful, the function returns 0, and nonzero value if an error occurs.
 *    mysql_rpl_listener__init() initializes the RPLEPLICATION LISTENER object and 
 *    writes it to rpl_h.
 *    Memory allocated when calling mysql_rpl_listener__init() must be freed by 
 *    calling mysql_rpl_listener__destroy().
 * 
 *    
 *    on_rpl_row () - a function that will be called when LISTENER receives a ROW-EVENT.
 *                    ROW-EVENTS are generated by the server when modifying the contents 
 *                    of tables and contain data from changed rows.
 *                    To this function will be passed a parameter of type _repl_log_x_rows* 
 *                    which will point to changed rows ( you do not need to free the memory 
 *                    allocated for the _repl_log_x_rows structure, it will be freed automatically 
 *                    at exit from on_rpl_row() ) function, and it can also an arbitrary pointer 
 *                    be passed, the value of which is passed by the on_rpl_row_param_ptr parameter
 *                    of the mysql_rpl_listener__init() function.
 * 
 * 
 * 
 *   on_rpl_heartbeat () - a function that will be called when the LISTENER receives 
 *                         a HEARTBEAT-EVENT.
 *                         HEARTBEAT-EVENTS are generated by the server during inactivity
 *                         moments so that client can understand that the server is still alive. 
 *                         These EVENTS can be used to determine how far we have lagged 
 *                         behind the server by implementing the changes that  have occurred 
 *                         on it in our program.
 *                         To this function will be passed a parameter 
 *                         of type _repl_log_x_heartbeat* which will point to the structure 
 *                         with HEARTBEAT-EVENT data ( free the memory allocated for the 
 *                         structure _repl_log_x_heartbeat is not needed, it will be 
 *                         deallocated automatically at exit from  on_rpl_heartbeat() function ).
 *                         An arbitrary pointer can also be passed there, the value of which
 *                         is passed with the on_rpl_heartbeat_param_ptr parameter of the 
 *                         mysql_rpl_listener__init() function.
 * 
 * 
 *    You can read more about EVENTS here
 *    https://dev.mysql.com/doc/dev/mysql-server/latest/page_protocol_replication_binlog_event.html
 * 
 * 
 * 
 *    my_host,
 *    my_user,
 *    my_pass
 *    and
 *    my_port - parameters for connecting to the MySQL database.
 *    User my_user must have REPLICATION SLAVE and REPLICATION CLIENT privileges.
 *    Otherwise, replication will not be possible.
 * 
 * 
 * 
 *    slave_id - the replication protocol assumes that
 *               each member has a unique identifier within the group of servers involved 
 *               in replication.
 *               Because we are a slave for the master-server, this is our slave id.
 *    
 * 
 * 
 *    blobs_display {0 | 1} - a variable responsible for whether the data on changed BLOB 
 *                            columns will be displayed. 
 *                            0 - will not. 1 - will be.
 *                            It should be noted that BLOB should be understood as columns 
 *                            of the following types BLOB, TEXT, JSON, GEOMETRY.
 *                            If blobs_display=0, then geometry_as_hex and 
 *                            blob_as_hex parameters are ignored.
 * 
 * 
 * 
 *   geometry_as_hex {0 | 1} - a variable responsible for whether fields of type 
 *                             GEOMETRY will be displayed in hexadecimal notation, 
 *                             or as raw BLOB ( because, in essence, the GEOMETRY type is BLOB ).
 *                             This variable is not ignored only if blobs_display=1.
 * 
 * 
 * 
 *   blob_as_hex {0 | 1} - a variable responsible for whether fields of type BLOB 
 *                         ( except GEOMETRY type ) will be displayed in hexadecimal 
 *                         notation, or as raw BLOB. 
 *                         ( this also includes TEXT and JSON types ).
 *                         This variable is not ignored only if blobs_display=1.
 * 
 * 
 * 
 * on_rpl_transaction () - a function that will be called when LISTENER receives events 
 *                         that indicate beginning and end of each transaction. A variable will
 *                         be passed to it pointin to a structure of type _repl_log_x_transaction, 
 *                         which has a field transaction_event_type, which can be set to 
 *                         TRANSACTION_EV_TYPE_START to start transaction
 *                         or to TRANSACTION_EV_TYPE_COMMIT to complete it
 *                         ( there is no need to free the memory allocated for the 
 *                         _repl_log_x_transaction structure, it will be deallocated automatically 
 *                         at exit of on_rpl_transaction () function ).
 *                         An arbitrary pointer can also be passed there, the value of which 
 *                         is passed with the on_rpl_transaction_param_ptr parameter of the
 *                         mysql_rpl_listener__init() function.
 * 
 * master_file_name - name of the binary log file on the server
 * 
 * 
 * master_file_pos - position in the file (ignored if master_file_name = NULL)
 * 
 * 
 * tbl_maps_ptr - pointer to the collection of TABLEMAP-EVENTs (ignored if master_file_name = NULL)
 * 
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
 *    This function should be called second, right after a successful call to
 *    mysql_rpl_listener__init().
 *    Calling mysql_rpl_listener__table_add() in a different order will return 
 *    an error. On successful function returns 0, but if an error occurs, a value 
 *    other than 0.
 *    mysql_rpl_listener__table_add() is used to add watched tables to LISTENER.
 *    If no tables are added, replication will fail.
 * 
 *    rpl_h - pointer to LISTENER's structure
 * 
 *    db_name - MySQL database name
 * 
 *    tbl_name - name of table
 */
int mysql_rpl_listener__table_add( RPL_LISTENER_H *rpl_h, const char* db_name, 
                                   const char* tbl_name );


/**
 *    --- mysql_rpl_listener__prepare() ---
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
 */
int mysql_rpl_listener__prepare( RPL_LISTENER_H *rpl_h, 
                                 _repl_log_x_error *error_ptr );




/**
 *    --- mysql_rpl_listener__start() ---
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
 */
int mysql_rpl_listener__start( RPL_LISTENER_H *rpl_h, 
                               _repl_log_x_error *error_ptr );


/**
 *    --- mysql_rpl_listener__stop() ---
 * 
 *    The function is used to stop replication.
 *    If successful, the function returns 0, but if an error occurs, a value other than 0.
 * 
 *    rpl_h - pointer to LISTENER's structure
 */
int mysql_rpl_listener__stop( RPL_LISTENER_H *rpl_h );


/**
 *    --- mysql_rpl_listener__destroy() ---
 * 
 *    The function is used to free the memory allocated in the LISTENER structure.
 *    If successful, the function returns 0, but if an error occurs, a value other than 0.
 * 
 *    rpl_h - pointer to LISTENER's structure
 */
int mysql_rpl_listener__destroy( RPL_LISTENER_H *rpl_h );


/**
 *    --- mysql_rpl_listener__in_process() ---
 * 
 *    This function is used to check the status of replication.
 *    If replication is running, the function returns REPL_PROC_STATUS_PHASE_1
 *    or REPL_PROC_STATUS_PHASE_2, if replication has ended for some reason
 *    or hasn't started yet, then mysql_rpl_listener__in_process() will 
 *    return REPL_PROC_STATUS_OFF. If the function returned REPL_PROC_STATUS_OFF 
 *    and replication was started before, then error status can be getted by analyzing 
 *    the _repl_log_x_error structure that was passed when calling 
 *     mysql_rpl_listener__start() function.
 *    REPL_PROC_STATUS_PHASE_1 status means that replication has started successfully, but
 *    replication thread is waiting for the moment when it will be possible to 
 *    change status to REPL_PROC_STATUS_PHASE_2.
 *    When starting replication from an arbitrary location in an arbitrary file, we cannot 
 *    be sure that for every ROW-EVENT we get, we got a corresponding TABLEMAP-EVENT,
 *    therefore, the replication thread is waiting for the moment from which it will 
 *    be possible to say that in the future ROW-EVENTS can be assigned to 
 *    the required TABLEMAP-EVENTS.
 *    After that it change replication status to REPL_PROC_STATUS_PHASE_2.
 *    Being in the REPL_PROC_STATUS_PHASE_1 status, the replication thread ignores the 
 *    beginning and end of transactions, as well as ROW-EVENTS.
 *    REPL_PROC_STATUS_PHASE_2 status means that replication has started successfully
 *    and ROW-EVENTs will be processed correctly.
 *    If mysql_rpl_listener__set_position() was called with a log file 
 *    and position ( file_name!= NULL ), then the REPL_PROC_STATUS_PHASE_1 status will
 *    be skipped and the replication stream will go immediately to the 
 *    REPL_PROC_STATUS_PHASE_2 status.
 *    This is done because if you are running replication from some place 
 *    known to you in advance, then you probably took steps to prevent replication 
 *    from crashing. For example, you put the server in READ_ONLY mode
 *    and waited enough time for all queries that modify tables to complete.
 *    Or you already had LISTENER running, but replication for some reason has 
 *    failed ( for example, network problems ), and you created a new LISTENER, 
 *    saving from the old LISTENER last file and position 
 *    using mysql_rpl_listener__get_last_sucess_pos() function ,
 *    and saving the TABLEMAP-EVENT collection 
 *    using the mysql_rpl_listener__get_tablemaps() function,
 *    and passed these parameters to mysql_rpl_listener__set_position()
 *    and mysql_rpl_listener__set_tablemaps().
 */
int mysql_rpl_listener__in_process( RPL_LISTENER_H *rpl_h ); 


/**
 *    --- mysql_rpl_listener__get_last_sucess_pos() ---
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
 *    pos - a pointer to a variable of the uint64_t type, where the position in the
 *          log file will be saved
 */
int mysql_rpl_listener__get_last_sucess_pos( RPL_LISTENER_H *rpl_h, 
                                             char *file_n, 
                                             int file_maxlen, 
                                             uint64_t *pos );




/**
 *    --- mysql_rpl_listener__get_tablemaps() ---
 * 
 *    This function is used to save the collection of TABLEMAP-EVENTS from the LISTENER.
 *    This may be needed if replication fails for some reason and
 *    we want to create a new LISTENER object to start replication from the same place.
 *    Because we are not sure that for all ROW-EVENTS that will follow after the 
 *    replication start we will receive the corresponding TABLEMAP-EVENTS in the 
 *    replication stream, then we will save before destruction of the old LISTENER it's 
 *    TABLEMAP-EVENTs and transfer them to the new LISTENER.
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
 */
int mysql_rpl_listener__get_tablemaps( RPL_LISTENER_H *rpl_h, 
                                       RPL_TABLE_MAPS *dst_tbl_maps );



/**
 *    --- mysql_rpl_listener__free_tablemaps() ---
 * 
 *    Frees resources allocated in the structure pointed to by the tbl_maps parameter.
 *    If successful, the function returns 0, but if an error occurs, a value other than 0.
 */
int mysql_rpl_listener__free_tablemaps( RPL_TABLE_MAPS *tbl_maps );
```


