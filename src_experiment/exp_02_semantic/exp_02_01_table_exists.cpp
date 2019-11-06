//
// Created by Sam on 2018/2/13.
//

#include <dongmendb/DongmenDB.h>

/*
 * 检查是否存在数据表
 * 此函数在以下情况会被调用：
 * 1. 执行select，update，insert，delete时调用，检查表是否存在
 * 2 执行create table语句时检查表是否已经存在
 * 3 执行create index时检查表是否存在
 *
 * */


int TableManager::semantic_check_table_exists(char *tableName, Transaction *tx) {

    /*
     * 1. 调用 table_manager_get_tableinfo 获得 FieldInfo
     * 2. 若 FieldInfo 为NULL则不存在，否则存在.
     * */
    return DONGMENDB_OK;
}