//
// Created by Sam on 2018/2/13.
//

#include <relationalalgebra/optimizer.h>
#include <dongmensql/expression.h>

#if 1

class _Table;

class _SRA;

class _Join;

class Select;

class JoinSelect;

_SRA *SRAConstruct(SRA_t *sra, SRA_t *before);

bool isOperator(TokenType type);

Expression *constructSingleExpression(Expression *from_expr, bool matchWord);

Expression *nextBeginExpressionNode(Expression *node);

std::vector<Expression *> expressionSpread(Expression *begin_expr);

Expression *cutANDOperator(Expression *expr);

bool isJoinExpression(Expression *expr);

#endif

class _SRA {    // explain:此处使用状态模式
private:
    SRA_t *before;
public:
    void setBefore(SRA_t *sra) {
        before = sra;
    }

    SRA_t *getBefore(SRA_t *sra) {
        return before;
    }

    virtual void setSRAOn(std::vector<Expression *> &expressionVector) = 0;

    virtual void pushSRADown(std::vector<Expression *> &expressionVector) = 0;

    virtual bool expressionMatch(Expression *expr) = 0;
};//explain: 其实也不是状态模式, 就是活用了C++的继承而已

class _Table : public _SRA {
    SRA_t *table_sra;
    char *table_name;
public:
    _Table(SRA_t *sra) {
        table_sra = sra;
        table_name = new_id_name();
        strcpy(table_name, sra->table.ref->table_name);
    }

    bool expressionMatch(Expression *expr) override {
        if (expr == nullptr)return false;
        Expression *pointer = expr;
        while (pointer) {
            if (pointer->opType == TOKEN_WORD) {
                if (strcmp(pointer->term->ref->tableName, table_name) == 0) {
                    return true;
                }
            }
            pointer = pointer->nextexpr;
        }
        return false;
    }

    void setSRAOn(std::vector<Expression *> &expressionVector) override {
        //explain: 漫长的算法他又lei了
        std::vector<Expression *> matchedExpression;
        for (int i = 0; i < expressionVector.size(); ++i) {
            auto expr = expressionVector[i];
            if (expressionMatch(expr)) {
                matchedExpression.push_back(expr);
                expressionVector[i] = nullptr;
            }
        }
        // TODO: 构造一个SRA_Select串, 并添加到该节点的父节点去
    }

    void pushSRADown(std::vector<Expression *> &expressionVector) override {

    }
};

class _Join : public _SRA {
    SRA_t *join_sra;
    _SRA *left, *right;
public:
    _Join(SRA_t *sra) {
        join_sra = sra;
        right = left = nullptr;
    }

    void setLeft(_SRA *sra_left) {
        left = sra_left;
    }

    void setRight(_SRA *sra_right) {
        right = sra_right;
    }

    void setSRAOn(std::vector<Expression *> &expressionVector) override {
        std::vector<Expression *> expressionOn;

    }

    void pushSRADown(std::vector<Expression *> &expressionVector) override {

    }

    bool expressionMatch(Expression *expr) override {
        return false;
    }
};

class Select {
public:
    explicit Select(Expression *e) : expr(e) {}

protected:
    Expression *expr;
};

class JoinSelect : public Select {
    char *left_field_name, *left_table_name;
    char *right_field_name, *right_table_name;
public:
    explicit JoinSelect(Expression *e) : Select(e) {
        left_field_name = new_id_name();
        right_field_name = new_id_name();
        left_table_name = new_id_name();
        right_table_name = new_id_name();
//        strcpy(left_table_name,)
    }
};

/**
 * construct a _SRA object, to parse and fix the struct of SRA_t which is parse from where expression
 * As we know, a SRA_t always be make by Project->Select->Join,
 * so, a connect tree must separate by SRA_Join and the leaves node must be SRA_Table.
 * @param sra
 * @return
 */
_SRA *SRAConstruct(SRA_t *sra, SRA_t *before) {
    _SRA *pointer;
    switch (sra->t) {
        case SRA_TABLE: {
            pointer = new _Table(sra);
            pointer->setBefore(before);
        }
        case SRA_SELECT: {
            return SRAConstruct(sra->select.sra, sra);
        }
        case SRA_JOIN: {
            _Join *_join = new _Join(sra);
            _join->setLeft(SRAConstruct(sra->join.sra1, sra));
            _join->setRight(SRAConstruct(sra->join.sra2, sra));
            pointer = _join;
            pointer->setBefore(before);
        }
        case SRA_PROJECT: {
            return SRAConstruct(sra->project.sra, sra);
        }
        default:
            return nullptr;
    }
}

bool isOperator(TokenType type) {
    TokenType types[21] = {TOKEN_OPEN_PAREN,
                           TOKEN_CLOSE_PAREN,
                           TOKEN_POWER,
                           TOKEN_PLUS,
                           TOKEN_MINUS,
                           TOKEN_DIVIDE,
                           TOKEN_MULTIPLY,
                           TOKEN_LT,              //less-than operator
                           TOKEN_GT,
                           TOKEN_EQ,
                           TOKEN_NOT_EQUAL,
                           TOKEN_LE,               //less-than-or-equal-to operator"
                           TOKEN_GE,
                           TOKEN_IN,
                           TOKEN_LIKE,
                           TOKEN_AND,
                           TOKEN_OR,
                           TOKEN_NOT,
                           TOKEN_ASSIGNMENT,
                           TOKEN_FUN,
                           TOKEN_COMMA};
    for (int i = 0; i < 21; ++i)if (type == types[i])return true;
    return false;
}

/**
 * 从expression串中截取一段单独的Expression
 * @param from_expr
 * @return
 */
Expression *constructSingleExpression(Expression *from_expr, bool matchWord = false) {
    if (!isOperator(from_expr->opType))matchWord = true;
    if ((isOperator(from_expr->opType) && matchWord) || (from_expr->nextexpr == nullptr)) {
        return nullptr;
    } else {
        Expression *expr = new Expression(from_expr->opType,
                                          constructSingleExpression(from_expr->nextexpr, matchWord));
        expr->term = from_expr->term;
        expr->alias = from_expr->alias;
    }
}

/**
 * explain: to fund next begin expression node
 * @param node
 * @return
 */
Expression *nextBeginExpressionNode(Expression *node) {
    if (node == nullptr)return nullptr;
    if (!isOperator(node->opType)) {             //explain: begin not from a operator
        Expression *pointer = node;
        while (pointer) {
            if (isOperator(pointer->opType))    //explain: end while find a operator
                return pointer;
            pointer = pointer->nextexpr;        //explain: else, go on.
        }
    } else {                                    //explain: begin from a operator
        Expression *pointer = node;             //explain: to find next no-operator node
        while (pointer && isOperator(pointer->opType)) {
            pointer = pointer->nextexpr;
        }
        return nextBeginExpressionNode(pointer->nextexpr);//explain: repeat precess before
        //complain: such a stupid process~
    }
}

std::vector<Expression *> expressionSpread(Expression *begin_expr) {
    std::vector<Expression *> expressionVector;
    bool matchWord = false;
    for (Expression *pointer = begin_expr;
         pointer != nullptr;
         pointer = nextBeginExpressionNode(pointer)) {  //explain: iterator the expr link
        expressionVector.push_back(constructSingleExpression(cutANDOperator(pointer)));
    }
    return expressionVector;
}

/**
 * clear all `AND` node of expression linklist.
 * which will returned
 * @param expr
 * @return
 */
Expression *cutANDOperator(Expression *expr) {
    Expression *pointer;
    if (expr != nullptr)pointer = expr;
    else return expr;
    while (pointer->nextexpr != nullptr) {
        if (pointer->opType != TOKEN_AND)return pointer;
        else {
            Expression *e = pointer->nextexpr;
            free(pointer);
            pointer = e;
            continue;
        }
        pointer = pointer->nextexpr;
    }
    return pointer;
}

bool isJoinExpression(Expression *expr) {
    int num_TOKEN_WORD = 0;
    Expression *pointer = expr;
    while (pointer) {
        if (pointer->opType == TOKEN_WORD)num_TOKEN_WORD++;
        pointer = pointer->nextexpr;
    }
    return num_TOKEN_WORD == 2;
}

/*输入一个关系代数表达式，输出优化后的关系代数表达式
 * 要求：在查询条件符合合取范式的前提下，根据等价变换规则将查询条件移动至合适的位置。
 * complain: this function is stupid like the function Expression* Expression::expression_print(), who can tell me what the parameter `tableManager` used to?
 * */
SRA_t *dongmengdb_algebra_optimize_condition_pushdown(SRA_t *sra, TableManager *tableManager) {
    _SRA *join_tree = SRAConstruct(sra, nullptr);
    Expression *begin_expr = cutANDOperator(sra->project.sra->select.cond);
    std::vector<Expression *> expressionVector = expressionSpread(begin_expr);

    return sra;
}

