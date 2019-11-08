//
// Created by Sam on 2018/2/13.
//

#include <relationalalgebra/optimizer.h>
#include <dongmensql/expression.h>
#include <functional>
#include <queue>


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

bool haveTableName(Expression *expr);

std::vector<Expression *> cutNoTableNameExpresionFrom(std::vector<Expression *> &exprVector);

SRA_t *createSelectOn(SRA_t *now, SRA_t *before, std::vector<Expression *> &expr, int counter);

#endif

/**
* explain:说明:
* 整体算法基于后根DFS, 先访问所有节点, 然后访问根
*/
class _SRA {    // explain:此处使用状态模式
private:
    SRA_t *before;
public:
    // explain: 构造一个双向链表
    void setBefore(SRA_t *sra) { before = sra; }

    SRA_t *getBefore() { return before; }

    virtual bool isLeaves() = 0;

    virtual void setSRAOn(std::vector<Expression *> &expressionVector) = 0;

    virtual void pushSRADown(std::vector<Expression *> &expressionVector) = 0;

    virtual bool expressionMatch(Expression *expr) = 0;

    virtual void pushSelectOn(Expression *expr) = 0;

    /**
     * set before SRA for a node. For it have a new SRA_Select node to decorate
     * @param sra
     */
    void resetBefore(SRA_t *sra) {
        if (before == nullptr)return;
        switch (before->t) {
            case SRA_PROJECT: {
                before->project.sra = sra;
                break;
            }
            case SRA_SELECT: {
                before->select.sra = sra;
                break;
            }
            case SRA_JOIN: {
                if (sra->select.sra == before->join.sra1) {
                    before->join.sra1 = sra;
                } else {
                    before->join.sra2 = sra;
                }
                break;
            }
            default: // ???why there exist a default?
                return;
        }
        before = sra;
    }
};//explain: 其实也不是状态模式, 就是活用了C++的`virtual`和`继承`而已

/***
 *  explain: 查询树的叶子节点
 */
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
        // explain: 匹配方案: 如果有任何一个表名与当前Table的名字相同, 则返回true,
        //  如果没有任何匹配值, 返回false
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
        std::vector<Expression *> matchedExpression;// Expression which match this leave
        for (size_t i = 0; i < expressionVector.size(); ++i) {
            auto expr = expressionVector[i];
            if (this->expressionMatch(expr)) {
                matchedExpression.push_back(expr);
                expressionVector[i] = nullptr; // while using this Expression, just set it null
            }
        }
        // TODO: 构造一个SRA_Select串, 并添加到该节点的父节点去
        SRA_t *before = this->getBefore();
        for (auto &expr:matchedExpression) {
            this->pushSelectOn(expr);
        }
    }

    void pushSRADown(std::vector<Expression *> &expressionVector) override {
        this->setSRAOn(expressionVector);
        // explain: 这里, 由于到达叶子节点了, 所以说也就没有继续下推一说了
    }

    /**
     * set a new SRA_Select on now node
     * @param expr
     */
    void pushSelectOn(Expression *expr) override {
        this->setBefore(SRASelect(table_sra, expr));
    }

    bool isLeaves() override {
        return true;
    }
};

class _Join : public _SRA {
    SRA_t *join_sra;
    _SRA *left, *right;
public:
    explicit _Join(SRA_t *sra) {
        join_sra = sra;
        right = left = nullptr;
    }

    bool isLeaves() override {
        return false;
    }

    void pushSelectOn(Expression *expr) override {
        this->setBefore(SRASelect(join_sra, expr));
    }

    void setLeft(_SRA *sra_left) {
        left = sra_left;
    }

    void setRight(_SRA *sra_right) {
        right = sra_right;
    }

    void setSRAOn(std::vector<Expression *> &expressionVector) override {
        // complain: 美妙的算法他又lei了
        std::vector<Expression *> matchedExpression;// Expression which match this leave
        for (size_t i = 0; i < expressionVector.size(); ++i) {
            auto expr = expressionVector[i];
            if (this->expressionMatch(expr)) {
                matchedExpression.push_back(expr);
                expressionVector[i] = nullptr; // while using this Expression, just set it null
            }
        }
        // TODO: 构造一个SRA_Select串, 并添加到该节点的父节点去
        SRA_t *before = this->getBefore();
        for (auto &expr:matchedExpression) {
            this->pushSelectOn(expr);
        }
    }

    void pushSRADown(std::vector<Expression *> &expressionVector) override {
        left->pushSRADown(expressionVector);
        right->pushSRADown(expressionVector);
        this->setSRAOn(expressionVector);
    }
    /**
     * 对于一个用于链接操作的expression,
     *  找到这个expression的左右Table节点位置, 然后检测本节点是否为这俩节点的LCA~
     *  想不到吧~ LCA!!!
     * @param expr
     * @return
     */
    bool expressionMatch(Expression *expr) override {
        if (expr == nullptr)return false;
        _Table *left_table = nullptr, *right_table = nullptr; // 暂时不考虑叶节点重复的情况
        std::function<void(_SRA *)> dfsSearch = [&](_SRA *sra) { // C++11 万岁
            if (sra->isLeaves()) {
                if (left_table == nullptr) {
                    left_table = dynamic_cast<_Table *>(sra);
                } else if (right_table == nullptr) {
                    right_table = dynamic_cast<_Table *>(sra);
                }
                return;
            } else {
                _Join *join = dynamic_cast<_Join *>(sra);
                dfsSearch(join->left);
                dfsSearch(join->right);
            }
        };
        dfsSearch(this);
        if (left_table == nullptr || right_table == nullptr) {
            // 如果该节点下找不到子节点, 说明这个expression不属于当前节点
            return false;
        }
        std::queue<_SRA *> left_table_father, right_table_father;
        std::function<bool(std::queue<_SRA *> &, _SRA *, const _SRA *)> getFathers = [&getFathers](
                std::queue<_SRA *> &queue,
                _SRA *sra,
                const _SRA *judged
        ) -> bool {
            if (sra->isLeaves()) {
                return judged == sra;
            } else {
                _Join *join = dynamic_cast<_Join *>(sra);
                if (getFathers(queue, join->left, judged)) {
                    queue.push(sra);
                    return true;
                } else if (getFathers(queue, join->right, judged)) {
                    queue.push(sra);
                    return true;
                } else return false;
            }
        };
        getFathers(left_table_father, this, left_table);
        getFathers(right_table_father, this, right_table);
        return [&](std::queue<_SRA *> &queue) -> bool { // 左右两个节点的父亲节点中必须都要包含自身才行
            int queue_size = queue.size();
            while (queue_size--) {
                if (queue.front() == this) return true;
                _SRA *sra = queue.front();
                queue.pop();
                queue.push(sra);
            }
            return false;
        }(left_table_father) && [&](std::queue<_SRA *> &queue) -> bool {
            int queue_size = queue.size();
            while (queue_size--) {
                if (queue.front() == this) return true;
                _SRA *sra = queue.front();
                queue.pop();
                queue.push(sra);
            }
            return false;
        }(right_table_father);
    }

    /**
     *  to judge a expression whether it was used to decorate a SRA_join type node or not
     * @param expr
     * @return
     */
    static bool isJoinExpression(Expression *expr) {
        int num_TOKEN_WORD = 0;
        Expression *pointer = expr;
        while (pointer) {
            if (pointer->opType == TOKEN_WORD)num_TOKEN_WORD++;
            pointer = pointer->nextexpr;
        }
        return num_TOKEN_WORD == 2;
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

class TableSelect : public Select {
    char *field_name, *table_name;

    explicit TableSelect(Expression *e) : Select(e) {
        table_name = new_id_name();
        field_name = new_id_name();
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
            break;
        }
        case SRA_SELECT: {
            return SRAConstruct(sra->select.sra, sra);
            break;
        }
        case SRA_JOIN: {
            _Join *_join = new _Join(sra);
            _join->setLeft(SRAConstruct(sra->join.sra1, sra));
            _join->setRight(SRAConstruct(sra->join.sra2, sra));
            pointer = _join;
            pointer->setBefore(before);
            break;
        }
        case SRA_PROJECT: {
            return SRAConstruct(sra->project.sra, sra);
            break;
        }
        default:
            return nullptr;
    }
    return pointer;
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
        if (pointer == nullptr)return nullptr;
        return nextBeginExpressionNode(pointer->nextexpr);//explain: repeat precess before
        //complain: such a stupid process~
    }
}

std::vector<Expression *> expressionSpread(Expression *begin_expr) {
    std::vector<Expression *> expressionVector;
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

bool haveTableName(Expression *expr) {
    if (expr == nullptr)return true;
    else if (expr->opType != TOKEN_WORD) {
        return haveTableName(expr->nextexpr);
    } else {
        return expr->term->ref->tableName != nullptr && haveTableName(expr->nextexpr);
    }
}

std::vector<Expression *> cutNoTableNameExpresionFrom(std::vector<Expression *> &exprVector) {
    std::vector<Expression *> cutedExpr;
    for (auto &expr:exprVector) {
        if (!haveTableName(expr)) {
            cutedExpr.push_back(expr);
            expr = nullptr;
        }
    }
    return cutedExpr;
}

/**
 * explain: to redescribe SRA_t*list
 * @param iter
 * @param sra
 * @return
 */
SRA_t *createSelectOn(SRA_t *now, SRA_t *before, std::vector<Expression *> &expr, int counter = 0) {

    switch (before->t) {
        case SRA_JOIN: {

        }
        case SRA_PROJECT: {

        }
        case SRA_SELECT: {

        }
        case SRA_TABLE: {

        }
    }
}


/*输入一个关系代数表达式，输出优化后的关系代数表达式
 * 要求：在查询条件符合合取范式的前提下，根据等价变换规则将查询条件移动至合适的位置。
 * complain: this function is stupid like the function Expression* Expression::expression_print(), who can tell me what the parameter `tableManager` used to?
 * */
SRA_t *dongmengdb_algebra_optimize_condition_pushdown(SRA_t *sra, TableManager *tableManager) {
    _SRA *join_tree = SRAConstruct(sra, nullptr);
    Expression *begin_expr = cutANDOperator(sra->project.sra->select.cond);
    std::vector<Expression *> expressionVector = expressionSpread(begin_expr);
    std::vector<Expression *> noTableNameExpr = cutNoTableNameExpresionFrom(expressionVector);
    join_tree->pushSRADown(expressionVector);
    //explain: rebuild SRA_t pointer
    sra->project.sra = sra->project.sra->select.sra;
    createSelectOn(sra->project.sra, sra, noTableNameExpr);
    return sra;
}
