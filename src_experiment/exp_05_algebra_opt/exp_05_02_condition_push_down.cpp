//
// Created by sumover on 2018/2/13.
// 本代码用了大量的auto, std::function的模板, 还有各种迭代器, 请保证自己的g++编译器版本为C++11以上
//

#include <relationalalgebra/optimizer.h>
#include <dongmensql/expression.h>
#include <functional>
#include <queue>
// 测试用, 如果不需要输出请删掉
#include <iostream>


#if 1 // 一些定义

class _SRA;

class _Table;

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

void
addTableNameOn(std::vector<Expression *> &exprVector, std::vector<char *> tableNameVector, TableManager *tableManager,
               Transaction *transaction);

void getAllTableNames(SRA_t *tree, std::vector<char *> &tableNames);

void expressionPrint(Expression *expr);

void SRAPrint(Expression *expr);


#endif

/**
* explain:说明:
*   整体算法基于后根DFS, 先访问所有节点, 然后访问根
*/
class _SRA {    // explain:此处使用状态模式, 并且将一些可以抽象的函数抽象起来, 以保证方法的充分利用
private:
    SRA_t *before;  //  explain: 前节点, 用于构造一个树形结构
public:
    // explain: 构造一个双向链表
    void setBefore(SRA_t *sra) { before = sra; }

    SRA_t *getBefore() { return before; }

    /**
     * 一个用于判断是否为子节点的一个函数, 总的方法抽象到父类中, 并将具体的实现下放给子类
     * @return  是否为叶子节点
     */
    virtual bool isLeaves() = 0;

    /**
     * 给当前节点添加修饰节点(Select)
     * @param expressionVector
     */
    virtual void setSRAOn(std::vector<Expression *> &expressionVector) = 0;

    /**
     * 基于当前节点向下推进, 保证整个过程是遍历了整个节点的
     * @param expressionVector
     */
    virtual void pushSRADown(std::vector<Expression *> &expressionVector) = 0;

    /**
     *  检测传入的表达式能不能与当前节点相匹配
     * @param expr
     * @return 是否匹配
     */
    virtual bool expressionMatch(Expression *expr) = 0;

    /**
     *  给当前节点添加一个修饰节点
     * @param expr
     */
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
};//complain: 其实也不是状态模式, 就是活用了C++的`virtual`和`继承`而已

/***
 *  explain: 查询树的叶子节点
 */
class _Table : public _SRA {
    SRA_t *table_sra;
    char *table_name;
public:
    /**
     * 构造函数, 初始化一个table节点的封装节点
     * @param sra
     */
    _Table(SRA_t *sra) {
        table_sra = sra;
        table_name = new_id_name();
        strcpy(table_name, sra->table.ref->table_name);
    }

    /**
     * 用于判断表名是否与当前封装的节点的表名匹配
     * @param tableName
     * @return
     */
    bool tableNameMatched(const char *tableName) {
        if (tableName == nullptr)return false;
        else return strcmp(tableName, table_name) == 0;
    }

    /**
     * 首先, 要排除那些用于修饰Join节点的表达式, 然后看看当前节点是否可以被修饰即可
     * @param expr
     * @return
     */
    bool expressionMatch(Expression *expr) override {
        // explain: 匹配方案:有且仅有一个`Expression*`的类型为TOKEN_WORD, 且值与当前匹配
        //  如果没有任何匹配值, 返回false
        if (expr == nullptr)return false;
        Expression *pointer = expr;
        int token_word_counter = 0;
        bool token_word_matched = false;
        while (pointer) {
            if (pointer->opType == TOKEN_WORD) {
                token_word_counter++;
                if ((strcmp(pointer->term->ref->tableName, table_name) == 0)) {
                    token_word_matched = true;
                }
            }
            pointer = pointer->nextexpr;
        }
        return token_word_counter == 1 && token_word_matched;
    }

    void setSRAOn(std::vector<Expression *> &expressionVector) override {
        //explain: 漫长的算法他又lei了
        std::vector<Expression *> matchedExpression;// Expression which match this leave
        for (size_t i = 0; i < expressionVector.size(); ++i) {
            if (expressionVector[i] == nullptr)continue;
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
        SRA_t *newBefore = SRASelect(table_sra, expr);
        switch (this->getBefore()->t) { // 这一块本来可以扔到某个统一的函数里的...
            case SRA_SELECT: {
                this->getBefore()->select.sra = newBefore;
                break;
            }
            case SRA_TABLE:
                break;
            case SRA_PROJECT: {
                this->getBefore()->project.sra = newBefore;
                break;
            }
            case SRA_JOIN: {
                SRA_t *originNext = this->table_sra;
                SRA_t *originBefore = this->getBefore();
                if (originBefore->join.sra1 == originNext) {
                    originBefore->join.sra1 = newBefore;
                    break;
                } else if (originBefore->join.sra2 == originNext) {
                    originBefore->join.sra2 = newBefore;
                } else break;
            }
            default:
                break;
        }

        this->setBefore(newBefore);
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
        SRA_t *newBefore = SRASelect(join_sra, expr);
        switch (this->getBefore()->t) { // 这一块本来可以扔到某个统一的函数里的...
            case SRA_SELECT: {
                this->getBefore()->select.sra = newBefore;
                break;
            }
            case SRA_TABLE:
                break;
            case SRA_PROJECT: {
                this->getBefore()->project.sra = newBefore;
                break;
            }
            case SRA_JOIN: {
                SRA_t *originNext = this->join_sra;
                SRA_t *originBefore = this->getBefore();
                if (originBefore->join.sra1 == originNext) {
                    originBefore->join.sra1 = newBefore;
                    break;
                } else if (originBefore->join.sra2 == originNext) {
                    originBefore->join.sra2 = newBefore;
                } else break;
            }
            default:
                break;
        }

        this->setBefore(newBefore);
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
            if (expressionVector[i] == nullptr)continue;
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
        // 首先解析得到expr的两个table_name
        char *left_table_name, *right_table_name;
        {
            int token_word_counter = 0;
            Expression *pointer = expr;
            while (pointer) {
                if (pointer->opType == TOKEN_WORD) {
                    if (token_word_counter == 0) {
                        left_table_name = pointer->term->ref->tableName;
                    } else if (token_word_counter == 1) {
                        right_table_name = pointer->term->ref->tableName;
                    }
                    token_word_counter++;
                }
                pointer = pointer->nextexpr;
            }
            if (token_word_counter < 2)return false;
        }
        _Table *left_table = nullptr, *right_table = nullptr; // 暂时不考虑叶节点重复的情况
        std::function<void(_SRA *)> dfsSearch = [&](_SRA *sra) { // C++11 万岁
            if (sra->isLeaves()) {
                _Table *trySRA = dynamic_cast<_Table *>(sra);
                if (trySRA->tableNameMatched(left_table_name)) {
                    left_table = trySRA;
                } else if (trySRA->tableNameMatched(right_table_name)) {
                    right_table = trySRA;
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
    if ((isOperator(from_expr->opType) && matchWord)) {
        return nullptr;
    } else if (from_expr->nextexpr == nullptr) {
        return from_expr;
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
    std::vector<Expression *> cuttedExpr;
    for (auto &expr:exprVector) {
        if (!haveTableName(expr)) {
            cuttedExpr.push_back(expr);
            expr = nullptr;
        }
    }
    return cuttedExpr;
}

/**
 * explain: to redescribe SRA_t*list
 * @param iter
 * @param sra
 * @return
 */
SRA_t *createSelectOn(SRA_t *now, SRA_t *before, std::vector<Expression *> &expr, int counter = 0) {
    if (counter == expr.size())
        return nullptr;
    SRA_t *sra = nullptr;
    switch (before->t) {
        case SRA_JOIN: {
            sra = SRASelect(now, expr[counter]);
            if (before->join.sra1 == now) {
                before->join.sra1 = sra;
            } else if (before->join.sra2 == now) {
                before->join.sra2 = sra;
            }
        }
        case SRA_PROJECT: {
            sra = SRASelect(now, expr[counter]);
            before->project.sra = sra;
        }
        case SRA_SELECT: {
            sra = SRASelect(now, expr[counter]);
            before->select.sra = sra;
        }
        case SRA_TABLE: {
            return nullptr;
        }
    }
    return createSelectOn(now, sra, expr, counter + 1);
}

/**
 * 给一堆Expr们添加上tableName
 * @param exprVector
 * @param tableNameVector
 */
void
addTableNameOn(std::vector<Expression *> &exprVector, std::vector<char *> tableNameVector, TableManager *tableManager,
               Transaction *transaction) {
    std::vector<TableInfo *> tableInfoVector;
    for (auto &tableName:tableNameVector) {
        tableInfoVector.push_back(tableManager->table_manager_get_tableinfo(tableName, transaction));
    }
    std::function<void(char *, const char *)> strAdd = [](char *str, const char *add) {
        int newLen = strlen(str) + strlen(add);
        for (int i = strlen(str), j = 0; j < strlen(add); ++i, ++j) {
            str[i] = add[j];
        }
        str[newLen] = 0;
    };
    std::function<void(Expression *)> resetExpressionTableName = [&](Expression *expr) -> void {
        if (expr == nullptr)return;
        //explain: 当且仅当 类型为`TOKEN_WORD`且`tableName=nullptr`时, 开始添加
        if (expr->opType == TOKEN_WORD) {
            if (expr->term->ref->tableName == nullptr) {
                for (auto &tableInfo:tableInfoVector) {
                    if (tableInfo->table_info_offset(expr->term->ref->columnName)) {
                        expr->term->ref->tableName = new_id_name();
                        strcpy(expr->term->ref->tableName, tableInfo->tableName);
                        strcpy(expr->term->ref->allName, tableInfo->tableName);
                        strAdd(expr->term->ref->allName, ".");
                        strAdd(expr->term->ref->allName, expr->term->ref->columnName);
                    }
                }
            }
        }
        resetExpressionTableName(expr->nextexpr);
    };
    for (auto &expr:exprVector) {
        resetExpressionTableName(expr);
    }
}

void getAllTableNames(SRA_t *tree, std::vector<char *> &tableNames) {
    switch (tree->t) {
        case SRA_SELECT: {
            getAllTableNames(tree->select.sra, tableNames);
            return;
        }
        case SRA_PROJECT: {
            getAllTableNames(tree->project.sra, tableNames);
            return;
        }
        case SRA_JOIN: {
            getAllTableNames(tree->join.sra1, tableNames);
            getAllTableNames(tree->join.sra2, tableNames);
            return;
        }
        case SRA_TABLE: {
            char *tableName = tree->table.ref->table_name;
            tableNames.push_back(tableName);
            return;
        }
        default:
            return;
    }
}


/*输入一个关系代数表达式，输出优化后的关系代数表达式
 * 要求：在查询条件符合合取范式的前提下，根据等价变换规则将查询条件移动至合适的位置。
 * complain: this function is stupid like the function Expression* Expression::expression_print(), who can tell me what the parameter `tableManager` used to?
 * */
SRA_t *
dongmengdb_algebra_optimize_condition_pushdown(SRA_t *sra, TableManager *tableManager, Transaction *transaction) {
//    SRA_print(sra);
//    return sra;
    _SRA *join_tree = SRAConstruct(sra, nullptr);
    Expression *begin_expr = cutANDOperator(sra->project.sra->select.cond);
    std::vector<Expression *> expressionVector = expressionSpread(begin_expr);
#if 0
    printf("expression spread:\n");
    for (int i = 0; i < expressionVector.size(); ++i) {
        char str[1000];
        memset(str, 0, sizeof(str));
        Expression *exp = expressionVector.at(i);
        exp->expression_print(exp, str);
        std::cout << str << std::endl;
    }
    printf("\n******************\n");
#endif
//    TableInfo *tableInfo = tableManager->table_manager_get_tableinfo(nullptr, transaction);
    std::vector<char *> tableNames;
    getAllTableNames(sra, tableNames);
    addTableNameOn(expressionVector, tableNames, tableManager, transaction);
//    return sra;
    join_tree->pushSRADown(expressionVector);
    //explain: rebuild SRA_t pointer
    sra->project.sra = sra->project.sra->select.sra;
    printf("\n**************\n");
    SRA_print(sra);
    printf("\n**************\n");
    return sra;
}
