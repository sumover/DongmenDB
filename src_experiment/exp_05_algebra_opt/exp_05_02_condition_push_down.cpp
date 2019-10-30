//
// Created by Sam on 2018/2/13.
//

#include <relationalalgebra/optimizer.h>
#include <dongmensql/expression.h>

void rebuild_sra(SRA_t *sra, std::vector<Expression *> &expr) {
    switch (sra->t) {
        case SRA_SELECT: { // 拆分cond中的Expression

        }
        case SRA_TABLE: {

        }
        case SRA_JOIN: {

        }
        case SRA_PROJECT: {

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
    SRA_Select_t *select = reinterpret_cast<SRA_Select_t *>(sra->project.sra);
    Expression *pointer = select->cond;
    while (pointer) {
        if (pointer->opType != TOKEN_AND)condExpr.push_back(pointer);
        pointer = pointer->nextexpr;
    }
    rebuild_sra(sra->project.sra, condExpr); // 去死吧
    return sra;
}

