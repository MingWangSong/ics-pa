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

/* We use the POSIX regex functions to process regular expressions.
 * Type 'man regex' for more information about POSIX regex functions.
 */
#include <regex.h>

enum {
    TK_NOTYPE = 256,
    TK_HEXNUM = 254,
    TK_NUM = 253,
    TK_REGNAME = 252,
    TK_EQUAL = 251,
    TK_NOTEQUAL = 250,
    TK_AND = 249,
    TK_OR = 248,
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
    {"0x[0-9a-fA-F]+", TK_HEXNUM},      // 十六进制数
    {"[0-9]+", TK_NUM},                 // 十进制整数
    {"\\[$a-z]{0-9}", TK_REGNAME},      // 寄存器名称
    {"\\(", '('},
    {"\\)", ')'},
    {"\\*", '*'},
    {"\\/", '/'},
    {"\\+", '+'},
    {"\\-", '-'},
    {"==", TK_EQUAL},
    {"!=", TK_NOTEQUAL},
    {"&&", TK_AND},
    {"\\|\\|", TK_OR},
    {"!", '!'},
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

static bool is_special_token(int token_type) {
    return token_type > 247 ? true : false;
}

// 识别表达式中的token，即词法分析
static bool make_token(char *e) {
    int position = 0;
    int i;
    // regex.h中的结构体，成员rm_so 存放匹配文本串在目标串中的开始位置，rm_eo 存放结束位置
    regmatch_t pmatch;

    nr_token = 0;

    while (e[position] != '\0') {
        /* Try all rules one by one. */
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

                 // 记录识别出来的token
                tokens[nr_token].type = rules[i].token_type;
                if (is_special_token(rules[i].token_type)) {
                    if (rules[i].token_type == TK_NOTYPE) {
                        break;
                    }
                    else {
                        strncpy(tokens[nr_token].str, substr_start, substr_len);
                    }
                }
                nr_token++;

                // switch (rules[i].token_type) {
                // case TK_NOTYPE:
                //     break;
                // case TK_HEXNUM:
                //     tokens[nr_token++].type = TK_HEXNUM;
                //     strncpy(&tokens[nr_token++].str, substr_start, substr_len);

                // default: TODO();
                // }
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


word_t expr(char *e, bool *success) {
    if (!make_token(e)) {
        *success = false;
        return 0;
    }

    /* TODO: Insert codes to evaluate the expression. */
    double ans = eval(0, nr_token);
    TODO();

    return 0;
}

static bool check_parentheses(int p, int q) {
    // printf("--------------\n");  
    int i, tag = 0;
    if (tokens[p].type != '(' || tokens[q].type != ')') return false; //首尾没有()则为false 
    for (i = p; i <= q; i++) {
        if (tokens[i].type == '(') tag++;
        else if (tokens[i].type == ')') tag--;
        if (tag == 0 && i < q) return false;  //(3+4)*(5+3) 返回false
    }
    if (tag != 0) return false;
    return true;
}

int dominant_operator(int p, int q) {
    int i, dom = p, left_n = 0;
    int pr = -1;
    for (i = p; i <= q; i++) {
        if (tokens[i].type == '(') {
            left_n += 1;
            i++;
            while (1) {
                if (tokens[i].type == '(') left_n += 1;
                else if (tokens[i].type == ')') left_n--;
                i++;
                if (left_n == 0)
                    break;
            }
            if (i > q)break;
        }
        else if (tokens[i].type == TK_NUM) continue;
        else if (pir(tokens[i].type) > pr) {
            pr = pir(tokens[i].type);
            dom = i;
        }
    }
    // printf("%d\n",left_n);
    return dom;
}

static double eval(p, q) {
    if (p > q) {
        /* Bad expression */
        assert(0);
    }
    else if (p == q) {
        /* Single token.
         * For now this token should be a number.
         * Return the value of the number.
         */
        double res;
        if (tokens[p].type == TK_NUM) {
            res = atof(tokens[p].str);
        }
        else if (tokens[p].type == TK_HEXNUM) {
            unsigned int hex_num;
            sscanf(hex_str, "%x", &hex_num);
            res = (double)hex_num;
        }
        else if (tokens[p].type == TK_REGNAME) {
            res = (double)*(tokens->str);
        }
        else {
            assert(0);
        }
        return res;
    }
    else if (check_parentheses(p, q) == true) {
        /* The expression is surrounded by a matched pair of parentheses.
         * If that is the case, just throw away the parentheses.
         */
        return eval(p + 1, q - 1);
    }
    else {
        // op = the position of 主运算符 in the token expression;
        int op = dominant_operator(p, q);
        int val1 = eval(p, op - 1);
        int val2 = eval(op + 1, q);

        switch (tokens[op].type) {
        case '+': return val1 + val2;
        case '-': return val1 - val2;
        case '*': return val1 * val2;
        case '/': return val1 / val2;
        default: assert(0);
        }
    }
}
