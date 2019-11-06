//
// Created by Sam on 2018/2/13.
//

#include <dongmendb/DongmenDB.h>

/**
 * 检查字段是否存在
 * 此函数在以下情况会被调用：
 * 1. 执行select，update，insert，delete时调用，检查相关字段是否存在
 * 2  执行create index时检查相关字段是否存在
 *
 * 1. 调用 table_manager_get_tableinfo 获得 FieldInfo
 * 2. 若 FieldInfo 为NULL则不存在，否则存在.
 * 3  通过table_info查看fieldsName是否存在指定的fieldName
 * @param tableName
 * @param fieldName
 * @param tx
 * @return
 */
int TableManager::semantic_check_field_exists(char *tableName, char *fieldName, Transaction *tx) {

    return DONGMENDB_OK;
}