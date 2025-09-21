/***************************************************************************************
* Copyright (c) 2014-2024 Zihao Yu, Nanjing University
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
#define TOKEN_LEN 256

#include <isa.h>

/* We use the POSIX regex functions to process regular expressions.
 * Type 'man regex' for more information about POSIX regex functions.
 */
#include <sdb/sdb.h>
#include <regex.h>
#include "memory/vaddr.h"


typedef enum {
  TK_NOTYPE = 256, 
  TK_EQ, TK_NEQ, TK_LE, TK_GE, TK_LEQ, TK_GEQ, 
  TK_AND, TK_OR, 
  TK_PLUS, TK_MINUS, TK_TIMES, TK_DIVIDEDBY, 
  TK_NUM, 
  TK_LPAREN, TK_RPAREN,
  TK_REG, TK_DEREF, TK_NEG,
  /* TODO: Add more token types */

}TOKEN_TYPE;

typedef enum {
  UNARY_OP = 1145,
  BINARY_OP,
  NO_OP,
}TOKEN_CLASS;

static struct rule {
  const char *regex;
  int token_type;
} rules[] = {
  /* TODO: Add more rules.
   * Pay attention to the precedence level of different rules.
  */
  {" +", TK_NOTYPE},        // spaces 0
  {"\\(", TK_LPAREN},       // left parenthesis 1
  {"\\)", TK_RPAREN},       // right parenthesis 2
  {"(0x)?[0-9]+", TK_NUM},  // num 3
  {"==", TK_EQ},            // equal 4
  {"!=", TK_NEQ},           // unequal 5
  {"<", TK_LE},             // less than 6
  {">", TK_GE},             // greater than 7
  {"<=", TK_LEQ},           // less or equal than 8
  {">=", TK_GEQ},           // greater or equal than 9
  {"\\$(0|ra|sp|gp|tp|t[0-6]|s[0-9]|s1[0-1]|a[0-7])", TK_REG},            // get reg value 10
  {"&&", TK_AND},           // and 11 
  {"\\|\\|", TK_OR},        // or 12
  {"\\+", TK_PLUS},         // plus 13
  {"-", TK_MINUS},          // minus 14
  {"\\*", TK_TIMES},        // times 15
  {"/", TK_DIVIDEDBY},      // division 16
};

#define NR_REGEX ARRLEN(rules)

static regex_t re[NR_REGEX] = {};

/* Rules are used for many times.
 * Therefore we compile them only once before any usage.
 */
void init_regex() {
  int i;
  char error_msg[128];
  int ret;

  for (i = 0; i < NR_REGEX; i ++) {
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

static Token tokens[TOKEN_LEN] __attribute__((used)) = {};
static int nr_token __attribute__((used))  = 0;
static int token_index = 0;

static int check_expr(const Token *tokens) {
  int stack[nr_token];
  int stop = 0;
  for (int i = 0; i < nr_token; i++) {
    if (tokens[i].type != TK_LPAREN && tokens[i].type != TK_RPAREN) {
      continue;
    }
    if (stop == 0) {
      stack[stop] = tokens[i].type;
      stop++;
    } else {
      if (tokens[i].type + stack[stop - 1] == 527) {
        stop--;
      } else {
        stack[stop] = tokens[i].type;
        stop++;
      }
    }
  }
  return stop == 0 ? true : false;
}

static int record_token(int type, const char* text, int len) {
  if (token_index >= TOKEN_LEN) {
    printf("Token array full!\n");
    return -1;
  }
  
  tokens[token_index].type = type;
  
  if (text != NULL && len > 0) {
    int copy_len = len;
    if (copy_len >= TOKEN_LEN) copy_len = TOKEN_LEN - 1;
    strncpy(tokens[token_index].str, text, copy_len);
    tokens[token_index].str[copy_len] = '\0';
  } else {
    tokens[token_index].str[0] = '\0';
  }
  
  token_index++;
  nr_token = token_index;
  return 0;
}

static bool make_token(char *e) {
  token_index = 0;

  int position = 0;
  int i;
  regmatch_t pmatch;

  nr_token = 0;

  while (e[position] != '\0') {
    /* Try all rules one by one. */
    for (i = 0; i < NR_REGEX; i ++) {
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
        
      case TK_EQ:
        record_token(TK_EQ, substr_start, substr_len);
        break;
        
      case TK_NEQ:
        record_token(TK_NEQ, substr_start, substr_len);
        break;
        
      case TK_LE:
        record_token(TK_LE, substr_start, substr_len);
        break;
        
      case TK_GE:
        record_token(TK_GE, substr_start, substr_len);
        break;
        
      case TK_LEQ:
        record_token(TK_LEQ, substr_start, substr_len);
        break;
        
      case TK_GEQ:
        record_token(TK_GEQ, substr_start, substr_len);
        break;
        
      case TK_AND:
        record_token(TK_AND, substr_start, substr_len);
        break;
        
      case TK_OR:
        record_token(TK_OR, substr_start, substr_len);
        break;
        
      case TK_PLUS:
        record_token(TK_PLUS, substr_start, substr_len);
        break;
        
      case TK_MINUS:
        record_token(TK_MINUS, substr_start, substr_len);
        break;
        
      case TK_TIMES:
        record_token(TK_TIMES, substr_start, substr_len);
        break;
        
      case TK_DIVIDEDBY:
        record_token(TK_DIVIDEDBY, substr_start, substr_len);
        break;
        
      case TK_NUM:
        record_token(TK_NUM, substr_start, substr_len);
        break;
        
      case TK_LPAREN:
        record_token(TK_LPAREN, substr_start, substr_len);
        break;
        
      case TK_RPAREN:
        record_token(TK_RPAREN, substr_start, substr_len);
        break;
      
      case TK_REG:
        record_token(TK_REG, substr_start + 1, substr_len - 1);
        break;

      default: 
        TODO();
    }

        break;
      }
    }

    if (i == NR_REGEX) {
      panic("no match at position %d\n%s\n%*.s^", position, e, position, "");
      return false;
    }
  }

  return true;
}

void test_make_token() {
  Log("Total tokens: %d\n", nr_token);
  for (int i = 0; i < nr_token; i++) {
    const char *type_str;
    switch (tokens[i].type) {
      case TK_NOTYPE: type_str = "NOTYPE"; break;
      case TK_EQ: type_str = "EQ"; break;
      case TK_NEQ: type_str = "NEQ"; break;
      case TK_LE: type_str = "LE"; break;
      case TK_GE: type_str = "GE"; break;
      case TK_LEQ: type_str = "LEQ"; break;
      case TK_GEQ: type_str = "GEQ"; break;
      case TK_AND: type_str = "AND"; break;
      case TK_OR: type_str = "OR"; break;
      case TK_PLUS: type_str = "PLUS"; break;
      case TK_MINUS: type_str = "MINUS"; break;
      case TK_TIMES: type_str = "TIMES"; break;
      case TK_DIVIDEDBY: type_str = "DIVIDEDBY"; break;
      case TK_NUM: type_str = "NUM"; break;
      case TK_LPAREN: type_str = "LPAREN"; break;
      case TK_RPAREN: type_str = "RPAREN"; break;
      case TK_DEREF: type_str = "DEREF"; break;
      case TK_NEG: type_str = "NEG"; break;
      case TK_REG: type_str = "REG"; break;
      default: type_str = "UNKNOWN"; break;
    }
    Log("Token[%d]: type=%s, str='%s'", i, type_str, tokens[i].str);
  }
}


int get_precedence(TOKEN_TYPE token_type) {
  switch (token_type) {
    case TK_OR:
      return 1;
    case TK_AND:
      return 2;
    case TK_EQ:
    case TK_NEQ:
    case TK_LE:
    case TK_GE:
    case TK_LEQ:
    case TK_GEQ:
      return 3;
    case TK_PLUS:
    case TK_MINUS:
      return 4;
    case TK_TIMES:
    case TK_DIVIDEDBY:
      return 5;
    case TK_LPAREN:
      return 6;
    case TK_RPAREN:
      return 7;
    case TK_NUM:
    case TK_NEG:
    case TK_REG:
      return 8;
    default:
      return 0;
  }
}


int find_main_op(int p, int q) {
  int op_index = -1;
  int min_precedence = 0x7fffffff;
  int paren_level = 0;
  
  for (int i = p; i <= q; i++) {
    if (tokens[i].type == TK_RPAREN) {
      paren_level++;
    } else if (tokens[i].type == TK_LPAREN) {
      paren_level--;
    } else if (paren_level == 0 && tokens[i].type != TK_NUM) {
      int precedence = get_precedence(tokens[i].type);
      if (precedence <= min_precedence) {
        min_precedence = precedence;
        op_index = i;
      }
    }
  }
  
  return op_index;
}

bool check_parentheses(int p, int q) {
if (tokens[p].type != TK_LPAREN || tokens[q].type != TK_RPAREN) {
    return false;
  }
  
  int paren_level = 0;
  for (int i = p; i <= q; i++) {
    if (tokens[i].type == TK_LPAREN) {
      paren_level++;
    } else if (tokens[i].type == TK_RPAREN) {
      paren_level--;
      if (paren_level < 0) {
        return false;
      }
      if (paren_level == 0 && i != q) {
        return false;
      }
    }
  }
  
  return paren_level == 0;
}

  // TK_NOTYPE = 256, 
  // TK_EQ, TK_NEQ, TK_LE, TK_GE, TK_LEQ, TK_GEQ, 
  // TK_AND, TK_OR, 
  // TK_PLUS, TK_MINUS, TK_TIMES, TK_DIVIDEDBY, 
  // TK_NUM, 
  // TK_LPAREN, TK_RPAREN,
  // TK_REG, TK_DEREF, TK_NEG,

TOKEN_CLASS which_op_type(TOKEN_TYPE token) {
  if (token == TK_NOTYPE) {
    return NO_OP;
  } else if (token == TK_DEREF || token == TK_NEG || token == TK_REG) {
    return UNARY_OP;
  } else if (token == TK_EQ || token == TK_NEQ || 
             token == TK_LE || token == TK_GE || 
             token == TK_LEQ || token == TK_GEQ ||
             token == TK_AND || token == TK_OR ||
             token == TK_PLUS || token == TK_MINUS ||
             token == TK_TIMES || token == TK_DIVIDEDBY) {
    return BINARY_OP;
  } else {
    return NO_OP;  // TK_NUM, TK_LPAREN, TK_RPAREN, TK_REG
  }
}

word_t make_value(int p, int q) {
  if (p > q) {
    /* Bad expression */
    return 0x00;
  }
  else if (p == q) {
    /* Single token.
     * This token can be a number or a register.
     */
    if (tokens[q].type == TK_NUM) {
      char *endptr;
      long num;
      if(strncmp("0x", tokens[q].str, 2) == 0) num = strtol(tokens[q].str, &endptr, 16);
      else num = strtol(tokens[q].str, &endptr, 10);
      if (*endptr != '\0') {
          panic("Invalid number: %s", tokens[q].str);
          return 0;
      }
      return (word_t)num;
    }
    else if (tokens[q].type == TK_REG) {
      bool success;
      word_t val = isa_reg_str2val(tokens[q].str, &success);
      if (!success) {
        panic("Invalid register: %s", tokens[q].str);
        return 0;
      }
      return val;
    }
    else {
      panic("Expected number or register, got type %d", tokens[q].type);
      return 0;
    }
  }
  else if (check_parentheses(p, q) == true) {
    /* The expression is surrounded by a matched pair of parentheses.
     * If that is the case, just throw away the parentheses.
     */
    return make_value(p + 1, q - 1);
  }
  else {
    int op = find_main_op(p, q);

    switch (which_op_type(tokens[op].type)) {
      case UNARY_OP: {
        word_t val = make_value(op + 1, op + 1);
        switch (tokens[op].type) {
          case TK_DEREF:
            return vaddr_read(val, 4);
          case TK_NEG:
            return (word_t)(-((sword_t)val));
          default:
            panic("Error: Unknown unary operator %d", tokens[op].type);
            assert(0);
            return 0;
        }
      }
      
      case BINARY_OP: {
        word_t val1 = make_value(p, op - 1);
        word_t val2 = make_value(op + 1, q);
        
        switch (tokens[op].type) {
          case TK_PLUS: 
            return (word_t)(val1 + val2);
          case TK_MINUS: 
            return (word_t)(val1 - val2);
          case TK_TIMES: 
            return (word_t)(val1 * val2);
          case TK_DIVIDEDBY: 
            if (val2 == 0) {
              panic("Error: Division by zero");
              return 0;
            }
            return (word_t)((sword_t)val1 / (sword_t)val2);
          case TK_EQ:
            return (word_t)(val1 == val2);
          case TK_NEQ:
            return (word_t)(val1 != val2);
          case TK_LE:
            return (word_t)(val1 < val2);
          case TK_GE:
            return (word_t)(val1 > val2);
          case TK_LEQ:
            return (word_t)(val1 <= val2);
          case TK_GEQ:
            return (word_t)(val1 >= val2);
          case TK_AND:
            return (word_t)(val1 && val2);
          case TK_OR:
            return (word_t)(val1 || val2);
          default:
            panic("Error: Unknown binary operator %d", tokens[op].type);
            assert(0);
            return 0;
        }
      }
      
      case NO_OP: {
        if (tokens[op].type == TK_NUM) {
          // actually this case will never be executed. Because function find_main_op will fliter type TK_NUM.
          char *endptr;
          long num;
          if(strncmp("0x", tokens[op].str, 2) == 0) num = strtol(tokens[op].str, &endptr, 16);
          else num = strtol(tokens[op].str, &endptr, 10);
          if (*endptr != '\0') {
              panic("Invalid number: %s", tokens[op].str);
              return 0;
          }
          return (word_t)num;
        } else {
          panic("Error: Expected number at position %d, got type %d", 
                op, tokens[op].type);
          assert(0);
          return 0;
        }
      }
      
      default: 
        panic("Error: Unknown operator type %d", tokens[op].type);
        assert(0);
        return 0;
    }
  }
}



word_t expr(char *e, bool *success) {
  if (!make_token(e)) {
    if (success) *success = false;
    return 0;
  }
  if (!check_expr(tokens)) {
    // printf("illegal expression\n");
    // if (success) *success = false;
    // return 0;
  }
  int i;
  for (i = 0; i < nr_token; i ++) {
    for (i = 0; i < nr_token; i ++) {
      if (tokens[i].type == TK_TIMES) {
        if (i == 0 || (tokens[i - 1].type != TK_NUM && tokens[i - 1].type != TK_RPAREN)) {
          tokens[i].type = TK_DEREF;
        }
      } else if (tokens[i].type == TK_MINUS) {
        if (i == 0 || (tokens[i - 1].type != TK_NUM && tokens[i - 1].type != TK_RPAREN)) {
          tokens[i].type = TK_NEG;
        }
      }
    }
  }
  test_make_token(); 
  if (success) *success = true;
  return make_value(0, nr_token - 1);
  // /* TODO: Insert codes to evaluate the expression. */
  // TODO();
  // return 0;
}

