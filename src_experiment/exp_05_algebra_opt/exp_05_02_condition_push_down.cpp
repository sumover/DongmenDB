//
// Created by Sam on 2018/2/13.
//

#include <relationalalgebra/optimizer.h>
#include <dongmensql/expression.h>

/**
 * 通过tableName来查找相关的Expression
 * @param tableName
 * @param expr
 * @return
 */
std::vector<Expression *> search_expression_by_tableName(const char *tableName, std::vector<Expression *> &exprs) {
    std::vector<Expression *> exprTable;
    for (auto &exp:exprs) {

    }
}

void rebuild_sra(SRA_t *sra, std::vector<Expression *> &expr) {
    switch (sra->t) {
        case SRA_SELECT: {
            /* 拆分cond中的Expression
             * 并将它们push到不是select为止
             * */
            SRA_t *pointer = sra;
            while (pointer->t != SRA_SELECT) { pointer = pointer->select.sra; }
            sra = pointer;
            rebuild_sra(sra, expr);
        }
        case SRA_TABLE: {
            /**
             * 在他们之前构造一堆SELECT, 并且最后的SELECT要指向当前的SRA
             */
            const char *tableName = sra->table.ref->table_name;

        }
        case SRA_JOIN: {

        }
        case SRA_PROJECT: {
            rebuild_sra(sra->project.sra, expr);
        }
    }
}

/**
* 使用关于选择的等价变换规则将条件下推。
*
*/

/*输入一个关系代数表达式，输出优化后的关系代数表达式
 * 要求：在查询条件符合合取范式的前提下，根据等价变换规则将查询条件移动至合适的位置。
 * TODO this function is stupid like the function Expression* Expression::expression_print(), who can tell me what the parameter `tableManager` used to?
 * */
SRA_t *dongmengdb_algebra_optimize_condition_pushdown(SRA_t *sra, TableManager *tableManager) {
//    SRA_print(sra);
//    printf("\n*************\n");
//    sra->project.sra->select.cond->expression_print(sra->project.sra->select.cond, nullptr);
//    printf("\n*************\n");
    std::vector<Expression *> condExpr;
    Expression *pointer = sra->project.sra->select.cond;


    rebuild_sra(sra->project.sra, condExpr); // 去死吧
    return sra;
}

