/***************************************************************************************
* Copyright (c) 2014-2022 Zihao Yu, Nanjing University
*
* NEMU is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*          http://license.coscl.org.cn/MulanPSL2
*
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
*
* See the Mulan PSL v2 for more details.
***************************************************************************************/

#include <isa.h>
#include <memory/vaddr.h>

/* We use the POSIX regex functions to process regular expressions.
 * Type 'man regex' for more information about POSIX regex functions.
 */
#include <regex.h>

enum {
    TK_NOTYPE = 256,
    TK_EQ, TK_NEQ, TK_OR, TK_AND, TK_NUM, TK_REG, TK_REF, TK_NEG
    /* TODO: Add more token types */
};

static struct rule {
    const char *regex;
    int token_type;
} rules[] = {

    /* TODO: Add more rules.
     * 此处增加正则表达式规则
     * Pay attention to the precedence level of different rules.
     */

    {" +", TK_NOTYPE},                  // 空格串
    {"0x[0-9a-fA-F]{1,16}", TK_NUM},    // 十六进制数
    {"[0-9]{1,10}", TK_NUM},            // 十进制数
    {"\\$[a-z0-9]{1,31}", TK_REG},      // register names
    {"\\+", '+'},
    {"-", '-'},
    {"\\*", '*'},
    {"/", '/'},
    {"%", '%'},
    {"==", TK_EQ},
    {"!=", TK_NEQ},
    {"&&", TK_AND},
    {"\\|\\|", TK_OR},
    {"!", '!'},
    {"\\(", '('},
    {"\\)", ')'}
};

#define NR_REGEX ARRLEN(rules)

// 存放编译后的正则表达式规则
static regex_t re[NR_REGEX] = {};

/* Rules are used for many times.
 * Therefore we compile them only once before any usage.
 * 初始化正则表达式识别规则
 * C项目处理正则表达式分三步：编译regcomp()、匹配regexec()、释放regfree()
 */
void init_regex() {
    int i;
    char error_msg[128];
    int ret;

    for (i = 0; i < NR_REGEX; i++) {
        ret = regcomp(&re[i], rules[i].regex, REG_EXTENDED);
        if (ret != 0) {
            regerror(ret, &re[i], error_msg, 128);
            panic("regex compilation failed: %s\n%s", error_msg, rules[i].regex);
        }
    }
}

typedef struct token {
    int type;
    char str[32];
} Token;

static Token tokens[32] __attribute__((used)) = {};
static int nr_token __attribute__((used)) = 0;

// 识别表达式中的token，即词法分析
static bool make_token(char *e) {
    int position = 0;
    int i;
    // regex.h中的结构体，成员rm_so 存放匹配文本串在目标串中的开始位置，rm_eo 存放结束位置
    regmatch_t pmatch;

    nr_token = 0;

    while (e[position] != '\0') {
        for (i = 0; i < NR_REGEX; i++) {
            if (regexec(&re[i], e + position, 1, &pmatch, 0) == 0 && pmatch.rm_so == 0) {
                char *substr_start = e + position;
                int substr_len = pmatch.rm_eo;

                Log("match rules[%d] = \"%s\" at position %d with len %d: %.*s",
                    i, rules[i].regex, position, substr_len, substr_len, substr_start);

                position += substr_len;

                /* TODO: Now a new token is recognized with rules[i]. Add codes
                 * to record the token in the array `tokens'. For certain types
                 * of tokens, some extra actions should be performed.
                 */

                switch (rules[i].token_type) {
                case TK_NOTYPE:
                    break;
                case TK_NUM:
                case TK_REG:
                    sprintf(tokens[nr_token].str, "%.*s", substr_len, substr_start);
                default:
                    tokens[nr_token].type = rules[i].token_type;
                    nr_token++;
                }
                break;
            }
        }

        if (i == NR_REGEX) {
            printf("no match at position %d\n%s\n%*.s^\n", position, e, position, "");
            return false;
        }
    }
    return true;
}

// 判断括号匹配情况
// static bool check_parentheses(int p, int q) { 
//     int i, tag = 0;
//     if (tokens[p].type != '(' || tokens[q].type != ')') return false; //首尾没有()则为false 
//     for (i = p; i <= q; i++) {
//         if (tokens[i].type == '(') tag++;
//         else if (tokens[i].type == ')') tag--;
//         if (tag == 0 && i < q) return false;  //(3+4)*(5+3) 返回false
//     }
//     if (tag != 0) return false;
//     return true;
// }

// 计算运算符优先级，优先级越高，数字越小
static int op_prec(int t) {
    switch (t) {
    case '!':
    case TK_NEG:
    case TK_REF: return 0;
    case '*':
    case '/':
    case '%': return 1;
    case '+':
    case '-': return 2;
    case TK_EQ:
    case TK_NEQ: return 4;
    case TK_AND: return 8;
    case TK_OR: return 9;
    default: assert(0);
    }
}

// 比较运算符优先级
static int op_prec_cmp(int t1, int t2) {
    return op_prec(t1) - op_prec(t2);
}

// 寻找主运算符（最后做计算的运算符）
static int dominant_operator(int p, int q) {
    int i;
    int bracket_level = 0;
    int op = -1;
    for (i = p; i <= q; i++) {
        switch (tokens[i].type) {
        case TK_NUM:
        case TK_REG: break;
        case '(':
            bracket_level++;
            break;
        case ')':
            bracket_level--;
            if (bracket_level < 0) {
                return -1;
            }
            break;
        default:
            if (bracket_level == 0) {
                if (op == -1) {
                    op = i;
                }
                else if (op_prec_cmp(tokens[op].type, tokens[i].type) < 0) {
                    // op位置优先级高于i位置
                    op = i;
                }
                else if (op_prec_cmp(tokens[op].type, tokens[i].type) == 0 &&
                    tokens[i].type != '!' && tokens[i].type != '~' &&
                    tokens[i].type != TK_NEG && tokens[i].type != TK_REF) {
                    // 优先级相同，除了部分运算符之外，运算符合从左至右规则
                    op = i;
                }
            }
            break;
        }
    }
    return op;
}

static word_t eval(int p, int q) {
    if (p > q) {
        /* Bad expression */
        assert(0);
    }
    else if (p == q) {
        /* Single token.
         * For now this token should be a number.
         * Return the value of the number.
         */
        word_t val;
        switch (tokens[p].type) {
        case TK_REG:
            // 寄存器类型，加一用于去除$符号
            val = isa_reg_str2val(tokens[p].str + 1, NULL);
            break;
        case TK_NUM:
            // 数据类型
            val = (word_t)strtoul(tokens[p].str, NULL, 0);
            break;
        default:
            assert(0);
        }
        return val;
    }
    else if (tokens[p].type == '(' && tokens[q].type == ')') {
        return eval(p + 1, q - 1);
    }
    else {
        // op = the position of 主运算符 in the token expression;
        int op = dominant_operator(p, q);
        // 表达式错误，无主运算符
        if (op < 0) {
            assert(0);
        }
        int op_type = tokens[op].type;
        // 主运算符为单目运算符
        if (op_type == '!' || op_type == TK_NEG || op_type == TK_REF) {
            word_t val = eval(op + 1, q);
            switch (op_type) {
            case '!':return !val;
            case TK_NEG:return -val;
            case TK_REF:return vaddr_read(val, 4);
            default:
                assert(0);
            }
        }

        // 主运算符为双目运算符
        word_t val1 = eval(p, op - 1);
        word_t val2 = eval(op + 1, q);

        switch (tokens[op].type) {
        case '+': return val1 + val2;
        case '-': return val1 - val2;
        case '*': return val1 * val2;
        case '/': return val1 / val2;
        case '%': return val1 % val2;
        case TK_EQ: return val1 == val2;
        case TK_NEQ: return val1 != val2;
        case TK_AND: return val1 && val2;
        case TK_OR: return val1 || val2;
        default: assert(0);
        }
    }
    return -1;
}

word_t expr(char *e, bool *success) {
    if (!make_token(e)) {
        *success = false;
        return 0;
    }

    int i;
    int prev_type;
    for (i = 0; i < nr_token; i++) {
        // 负号和间接引用符处理
        if (tokens[i].type == '-' || tokens[i].type == '*') {
            if (i == 0) {
                tokens[i].type = tokens[i].type == '-' ? TK_NEG : TK_REF;
                continue;
            }
            prev_type = tokens[i - 1].type;
            if (!(prev_type == ')' || prev_type == TK_NUM || prev_type == TK_REG)) {
                tokens[i].type = tokens[i].type == '-' ? TK_NEG : TK_REF;
            }
        }
    }
    *success = true;
    /* TODO: Insert codes to evaluate the expression. */
    return eval(0, nr_token - 1);
}
