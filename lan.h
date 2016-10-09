
/*
 *  a simple jit
 *  pengjianzhang
 *
 *  2016.9.28
 *
 * */


#ifndef _MY_LAN_H
#define _MY_LAN_H


typedef enum
{
    ERR_NONE,
    ERR_UNKNOWN_VAR,
    ERR_TOO_LONG_ID,
    RRR_SYNTAX_ERROR,
    ERR_TOO_LONG_STRING,
    ERR_MISSING_BRACKET,
    ERR_TYPE_MISMATCH,
    ERR_MISSING_SEMICOLON,
    ERR_UNKNOWN_FUNCTION,
    ERR_ARG_EXCEED_LIMIT,
    ERR_MEM_ALLOC,
}jit_err_t;



#ifndef LAN_CALLOC
#define LAN_CALLOC  calloc
#endif


#ifndef LAN_MALLOC  
#define LAN_MALLOC  malloc
#endif

#ifndef LAN_FREE  
#define LAN_FREE    free
#endif


#define STACK_STR_INIT_NUM  4096 
#define STACK_INIT_NUM  64

#define STATIC_STR_MAX 64 
#define ID_MAX 64
#define ARG_MAX 8


typedef enum
{
    /* the same as op code */
    TK_NONE,
	TK_ADD,	
	TK_SUB,	
	TK_MUL,	
	TK_DIV,	
	TK_REM,	
	TK_GT,     	
	TK_LT,     	
	TK_GE,     	
	TK_LE,     	
	TK_EQ,     	
	TK_NE,     	
    TK_AND,		
	TK_OR, 		
	TK_NOT,		
    TK_ASSIGN,


    TK_LBRACKET,
    TK_RBRACKET,
    TK_COMMA,
    TK_SEMICOLON,

    TK_NUM,
    TK_STR,

	    
    TK_IF,
    TK_THEN,
    TK_END,
    TK_ELSEIF,
    TK_ELSE,

    TK_RETURN,
    TK_FNAME,
    TK_ID,

    TK_ERR,
}token_t;


struct token
{
    token_t type;
    char * ptr;
    int len;
};




typedef enum
{
    TYPE_NONE,
	INTEGER,
	STRING,
} value_t;


typedef struct
{
    char * data;
    int len;
    int max;
}str_t;


/*
 * 0, fail
 * 1, continue
 * */


#define VAR_FUNCTION_RET_ID     1
#define VAR_FUNCTION_ARGC_ID    2
#define VAR_FUNCTION_ARGV_ID    3
#define VAR_FUNCTION_NUM        (ARG_MAX + 2)

#define VAR_ST_TEMP_FREE    1
#define VAR_ST_TEMP_USE     2
#define VAR_ST_CONST        3
#define VAR_ST_VAR          4

struct variable
{
    value_t type;
    int status;    
    union
    {    
        str_t str;
        long   num;
    }u;
};

typedef enum
{
    OP_NOP,
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_REM,
    OP_GT, 
    OP_LT, 
    OP_GE, 
    OP_LE, 
    OP_EQ, 
    OP_NE, 
    OP_AND,
    OP_OR,
    OP_NOT,
    OP_ASSIGN,

    OP_MOV,
    OP_JMP,
    OP_JE,  /* if false jump */

    OP_CALL,
    OP_RETURN,
}opcode_t;

struct instruction
{
    opcode_t op;
    long ret;
    long v0;
    long v1;  
};

struct jit_heap
{
    int max;
    int num;
    void *base;
};


struct jit_stack
{
        int max;
        int num;
        int element_size;
        void * base;
};

struct symbol
{
    char name[ID_MAX];
    int len;
    int id;
};




struct jit_state
{
    struct jit_stack inst;
    struct jit_stack data;
    struct jit_stack symtable;
    struct jit_stack functable;
    struct jit_stack static_str;


    struct 
    {    
        struct token lookahead;
        char * code;
        int len;
        int next;
        int line;

    }lex;
    jit_err_t err;
};

typedef int (*jit_func_t)( struct jit_state * jit);    

struct func_entry
{
    char * name;
    jit_func_t func;
    value_t ret_type;
};



struct jit_state * jit_new( );
int jit_load(struct jit_state * jit,char * code);

int jit_register(struct jit_state *jit,  struct func_entry * entry);


int jit_get_argv_int(struct jit_state * jit, int id, long * ret);

int jit_get_argv_str(struct jit_state * jit, int id, str_t * ret);

int jit_get_argc(struct jit_state * jit);

int jit_return_int(struct jit_state * jit, long num);

int jit_return_str(struct jit_state * jit, char * str, int len);


void jit_free(struct jit_state * jit);


#endif
