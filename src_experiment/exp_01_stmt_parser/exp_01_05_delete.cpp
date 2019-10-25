//
// Created by Sam on 2018/2/13.
//

#include <dongmensql/sqlstatement.h>
#include <parser/StatementParser.h>

/**
 * 在现有实现基础上，实现delete from子句
 *
 *  支持的delete语法：
 *
 *  DELETE FROM <table_nbame>
 *  WHERE <logical_expr>
 *
 * 解析获得 sql_stmt_delete 结构
 */

sql_stmt_delete *DeleteParser::parse_sql_stmt_delete() {
    sql_stmt_delete *sqlStmtDelete = (sql_stmt_delete *)
            malloc(sizeof(sql_stmt_delete));
    auto &tableName = sqlStmtDelete->tableName;
    auto &where = sqlStmtDelete->where;
    if (!this->matchToken(TOKEN_RESERVED_WORD, "delete")) {
        strcpy(this->parserMessage, "invalid sql: missing delete");
        return NULL;
    }
//    if (!this->matchToken(TOKEN_RESERVED_WORD, "from")) {
//        strcpy(this->parserMessage, "invalid sql: missing from");
//        return NULL;
//    }

    Token *token = this->parseNextToken();
    SRA_t *table = NULL;
    if (token->type == TOKEN_WORD) {
        tableName = new_id_name();
        strcpy(tableName, token->text);
        TableReference_t *ref = TableReference_make(tableName, NULL);
        table = SRATable(ref);
    } else {
        strcpy(this->parserMessage, "invalid sql: missing table name.");
        return NULL;
    }
    token = this->parseEatAndNextToken();
    if (!this->matchToken(TOKEN_RESERVED_WORD, "where")) {
        strcpy(this->parserMessage, "invalid sql: missing where");
        return NULL;
    }
    token = this->parseNextToken();
    Expression *whereExpr = this->parseExpressionRD();
    where = SRASelect(table, whereExpr);
    SRA_print(where);
    return sqlStmtDelete;
};