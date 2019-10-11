//
// Created by Sam on 2018/2/13.
//

#include <dongmensql/sqlstatement.h>
#include <parser/StatementParser.h>

/**
 * 在现有实现基础上，实现update from子句
 *
 * 支持的update语法：
 *
 * UPDATE <table_name> SET <field1> = <expr1>[, <field2 = <expr2>, ..]
 * WHERE <logical_expr>
 * exp:
 *      update SPJ set sno = 1, jno = 2
 *      where pno = 1
 *
 * 解析获得 sql_stmt_update 结构
 */

/*TODO: parse_sql_stmt_update， update语句解析*/
sql_stmt_update *UpdateParser::parse_sql_stmt_update() {
//    fprintf(stderr, "TODO: update is not implemented yet. in parse_sql_stmt_update \n");
    sql_stmt_update *sqlStmtUpdate = (sql_stmt_update *) malloc(sizeof(sql_stmt_update));
    // 为sql_stmt_update 中的值创造引用或者指针
    auto &tableName = sqlStmtUpdate->tableName;
    vector<char *> &fields = sqlStmtUpdate->fields;
    vector<Expression *> &fieldExpr = sqlStmtUpdate->fieldsExpr;
    auto &where = sqlStmtUpdate->where;
    if (!this->matchToken(TOKEN_RESERVED_WORD, "update")) {
        return NULL;
    }
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
    if (!this->matchToken(TOKEN_RESERVED_WORD, "set")) {
        strcpy(this->parserMessage, "invalid sql: missing set.");
        return NULL;
    }
    token = this->parseNextToken();
    if (token->type == TOKEN_WORD) {
        while (token->type == TOKEN_WORD) {
            char *fieldName = new_id_name();
            strcpy(fieldName, token->text);
            fields.push_back(fieldName);
            token = this->parseEatAndNextToken();
            if (token->type == TOKEN_EQ) {
                token = this->parseEatAndNextToken();
            }
            Expression *expr = this->parseExpressionRD();
//            expr->printTermExpression()
            fieldExpr.push_back(expr);
            token = this->parseNextToken();
            if (token && token->type == TOKEN_COMMA) {
                token = this->parseEatAndNextToken();
            } else {
                break;
            }
        }
    }

    token = this->parseNextToken();
    if (!this->matchToken(TOKEN_RESERVED_WORD, "where")) {
        strcpy(this->parserMessage, "invalid sql: missing where.");
    }
    token = this->parseNextToken();
    Expression *whereExpr = this->parseExpressionRD();
    if (this->parserStateType != PARSER_WRONG) {
        where = SRASelect(table, whereExpr);
    } else {
        where = SRASelect(table, whereExpr);
    }
    return sqlStmtUpdate;
};