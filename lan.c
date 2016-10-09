#include "lan.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>


static void jit_print_token(struct token * tk)
{
    int i;

    if(tk->ptr)
    {
        for(i = 0; i < tk->len; i++)
        {
            printf("%c",tk->ptr[i]);
        }
    }
}


static void jit_error(struct jit_state * jit,char * msg)
{
    if(msg)
    {    
        printf("Error:%d:%s",jit->lex.line,msg);
    }
    else
    {
        printf("Error:%d",jit->lex.line);
    }    
    
    if(jit->lex.lookahead.len > 0)
    {
        printf(" : ");
    }
    jit_print_token(&(jit->lex.lookahead));
    printf("\n");
}    

/*******************************************
 *          jit stack
 * *****************************************/
/* don't use index 0*/

int  jit_stack_init(struct jit_stack * st, int esize, int num)
{
    void * base = NULL;

    if((base = LAN_CALLOC(num,esize)) == NULL)
    {
        return 0;
    }

    st->base = base;
    st->max = num;
    st->num = 1;
    st->element_size = esize;

    return num;
}




int  __jit_stack_alloc2(struct jit_stack * st, int num)
{
    void * base = NULL;
    int esize = st->element_size;
    int max;
    int id;

    if(num <= 0)
    {
        return 0;
    }

    if((st->num + num) >= st->max)
    {
        max = st->max * 2;
        if((base = LAN_CALLOC(max,esize)) == NULL)    
        {
            return 0;
        }

        memcpy(base,st->base, esize*st->max);
        LAN_FREE(st->base);
        st->base = base;
        st->max = max;
    }

    id = st->num;
    st->num += num;

    return id;
}

int  __jit_stack_alloc(struct jit_stack * st)
{
    return __jit_stack_alloc2(st, 1);
}


void * jit_stack_get(struct jit_stack * st, int id)
{
    if((id > 0) && (id < st->num))
    {
        return   (void*)(((char*)(st->base)) + id * st->element_size);
    }    

    return NULL;
}


/*******************************************
 *              variable
 *******************************************/
/*
 *      
 *  return:
 *      0   fail
 *      >0  ok
 *
 * */


struct symbol * jit_get_symbol(struct jit_state * jit, int id)
{
    return (struct symbol*)jit_stack_get(&(jit->symtable),id);
}

int  jit_alloc_symbol(struct jit_state * jit,char * name,int len, int vid)
{
    struct symbol * sym = NULL;
    int id = 0;

    if((name == NULL) || (len <= 0) || (len >= ID_MAX))
    {
        return 0;
    }

    if((id = __jit_stack_alloc(&(jit->symtable))) > 0)
    {
        sym = jit_get_symbol(jit, id);
        strncpy(sym->name,name,len);
        sym->id = vid;
        sym->len = len;
    }

    return id;
}


struct variable * jit_get_var(struct jit_state * jit, int id)
{
    return (struct variable*)jit_stack_get(&(jit->data),id);
}


int  __jit_alloc_var(struct jit_state * jit)
{
    return __jit_stack_alloc(&(jit->data));
}


int jit_lookup_var(struct jit_state * jit, char * name, int len)
{
    int i;
    struct symbol * sym = NULL;

    if((name == NULL) || (name[0] == 0) || (len <= 0))
    {
        return 0;
    }

    for(i = 1; i < jit->symtable.num; i++)
    {
        sym = jit_get_symbol(jit, i);
        if((sym->len == len) && (strncmp(sym->name,name, len) == 0))
        {
            return sym->id;
        }
    }

    return 0;
}


int jit_alloc_tmp(struct jit_state * jit, value_t type)
{
    struct variable * var = NULL;
    int id = 0;
    int i;

    for(i = 1; i < jit->data.num; i++)
    {
        var = jit_get_var(jit, i);

        if((var->type == type) && (var->status == VAR_ST_TEMP_FREE))
        {
            id = i;
            break;
        }
    }

    id = __jit_alloc_var(jit);
    
    if(id > 0)
    {
        var = jit_get_var(jit, id);
        var->status = VAR_ST_TEMP_USE;
        var->type = type;
    }

    return id;
}


int jit_alloc_const(struct jit_state * jit, value_t type)
{
    struct variable * var = NULL;
    int id = 0;
    
    id = __jit_alloc_var(jit);
    
    if(id > 0)
    {
        var = jit_get_var(jit, id);
        var->status = VAR_ST_CONST;
        var->type = type;
    }
   
    return id;
}

int jit_alloc_const_integer(struct jit_state * jit, long value)
{
    int id = jit_alloc_const(jit, INTEGER);
    struct variable * var = NULL;
    
    if(id > 0)
    {
        var = jit_get_var(jit, id);
        var->u.num = value;
    }    

    return id;
}

int jit_free_tmp(struct jit_state * jit)
{
    struct variable * var = NULL;
    int num = 0;
    int i;

    for(i = 1; i < jit->data.num; i++)
    {
        var = jit_get_var(jit, i);
        if(var->status == VAR_ST_TEMP_USE)
        {
            var->status = VAR_ST_TEMP_FREE;
            num++;
        }
    }

    return num;
}


int jit_alloc_var(struct jit_state * jit, char * name, int len)
{
    int id = 0;

    if(name == NULL)
    {
        return 0;
    }

    if((id = jit_lookup_var(jit, name, len)) > 0)
    {
        return id;        
    }
   
    if((id = __jit_alloc_var(jit)) > 0)
    {
        if(jit_alloc_symbol(jit,name,len,id) > 0)
        {
            return id;  
        }
    }

    return 0;
}


int jit_gen_assign(struct jit_state * jit,int left, int right);
int jit_gen_arithmetic(struct jit_state * jit,opcode_t op, int v0, int v1);

int jit_clear_argc(struct jit_state * jit)
{
    struct variable * var = jit_get_var(jit, VAR_FUNCTION_ARGC_ID);
    var->u.num = 0;
    var->type = INTEGER;
//    jit_gen_assign(jit,VAR_FUNCTION_ARGC_ID,VAR_ZERO_ID);
    return 1;
}

int jit_get_argc(struct jit_state * jit)
{
    struct variable * var = jit_get_var(jit, VAR_FUNCTION_ARGC_ID);
    return var->u.num;
}


int jit_push_argv(struct jit_state * jit, int vid)
{
    struct variable * argc = jit_get_var(jit, VAR_FUNCTION_ARGC_ID);
    int id = argc->u.num + VAR_FUNCTION_ARGV_ID;
    jit_gen_assign(jit, id, vid);
    argc->u.num++;
    argc->type = INTEGER;
    return 1;
}


struct variable * __jit_get_argv(struct jit_state * jit, int id, value_t type)
{
    int argc = jit_get_argc(jit);
    struct variable * var;

    if(id < argc)
    {
        var = jit_get_var(jit, VAR_FUNCTION_ARGV_ID + id);
        if((var != NULL) && (var->type == type))
        {
            return var;
        }    
    }

    return NULL;
}



int jit_get_argv_int(struct jit_state * jit, int id, long * ret)
{
    struct variable * var = __jit_get_argv(jit,id, INTEGER);

    if(var != NULL)
    {
        if(ret)
        {    
            *ret = var->u.num;
        }
        return 1;
    }    

    return 0;
}


int jit_get_argv_str(struct jit_state * jit, int id, str_t * ret)
{
    struct variable * var = __jit_get_argv(jit,id, STRING);

    if(var != NULL)
    {
        if(ret)
        {    
            *ret = var->u.str;
        }

        return 1;
    }    

    return 0;
}


value_t jit_var_type_get(struct jit_state * jit, int id)
{
    struct variable * var = jit_get_var(jit, id);

    if(var)
    {
        return var->type;
    }

    return TYPE_NONE;
}    


int jit_var_type_set(struct jit_state * jit, int id, value_t type)
{
    struct variable * var = jit_get_var(jit, id);

    if(var)
    {
        var->type = type;
        return 1;
    }

    return 0;
}

int jit_var_type_check(struct jit_state * jit, int id, value_t type)
{
    struct variable * var = jit_get_var(jit, id);

    if(var)
    {
        if(var->type == type)
        {
            return 1;
        }    
    }

    return 0;
}

int jit_return_int(struct jit_state * jit, long num)
{
    struct variable * var = jit_get_var(jit, VAR_FUNCTION_RET_ID);
    var->type = INTEGER;
    var->u.num = num;

    return 1;
}


int jit_return_str(struct jit_state * jit, char * str, int len)
{
    struct variable * var = jit_get_var(jit, VAR_FUNCTION_RET_ID);
    var->type = STRING;
    var->u.str.data = str;
    var->u.str.len = len;
    var->u.str.max = len;
    return 1;
}

/* end variable */


/*********************************************
 *  static string 
 *********************************************/


char * jit_static_string_addr(struct jit_state *jit, int id)
{
    struct jit_stack * st = &(jit->static_str);
    return (char*)jit_stack_get(st, id);
}

/* "dsd" -> dsd
 *  \"  -> "
 *  \X  -> X
 * */
int static_string_filter(char * str, int len)
{
    int i,c,n,size;
 
    if(len == 2)
    {
        str[0] = 0;
        return 0;
    }

    size = len - 2;
    /* remove " " */    
    memmove(str,str + 1,size);
    
    for(i = 0; i < size - 1; i++)
    {
        c = str[i];
        n = str[i+1];
        
        if( c != '\\')
        {
            continue;
        }
        switch(n)
        {
        case 'b': 
            str[i] = '\b';
            break;
        case  'f':
            str[i] = '\f';
            break;
        case  'n':
            str[i] = '\n';
            break;
        case 'r' :
            str[i] = '\r';
            break;
        case 't' :
            str[i] = '\t';
            break;
        case  'v':
            str[i] = '\v';
            break;
        case '\\':
            str[i] = '\\';
            break;
        case '\'':
            str[i] = '\'';
            break;
        case  '\"':
            str[i] = '\"';
            break;
        case  '\?':
            str[i] = '\?';
            break;
/*
        case '0':
                str[i] = 0;
*/
        default:
        continue;
        }
        memmove(str + i + 1 ,str + i + 2,size - i  - 2);
        size--;
    }

    str[size] = 0;
    return size;
}

int jit_static_string(struct jit_state *jit, char * str, int len)
{
    int vid = 0;
    int sid = 0; 
    int max = len + 1;
    char * dst;
    struct variable * var;
    struct jit_stack * st = &(jit->static_str);
    
    if((str == NULL) || (len < 0))
    {
        return 0;
    }

    if((vid = jit_alloc_const(jit, STRING)) == 0)
    {
        return 0;
    }
    
    if((sid = __jit_stack_alloc2(st,len + 1)) == 0)    
    {
        return 0;
    }    

    dst = jit_static_string_addr(jit, sid);
    memcpy(dst,str,len);
    len = static_string_filter(dst, len);
    var = jit_get_var(jit, vid);
    var->u.str.len = len;
    var->u.str.data = ((char*)((long)sid));  /*coreect latter */
    var->u.str.max = max;
    
    return vid;    
}

int jit_static_string_refill(struct jit_state *jit)
{
    int num = 0;
    int i = 1;
    struct variable * var;
    int id;

    while((var = jit_get_var(jit,i)) != NULL)
    {
        if((var->type == STRING) && (var->status == VAR_ST_CONST))
        {
            id = (long)(var->u.str.data);
            var->u.str.data = jit_static_string_addr(jit,id); 
        }
        i++;
    }

    return num;
}


/* end static string */


/**********************************************
 *          function 
 **********************************************/

struct func_entry * jit_get_func(struct jit_state *jit, int id)
{
    return (struct func_entry *)jit_stack_get(&(jit->functable), id);
}


int jit_func_lookup(struct jit_state * jit, char * name, int len)
{
    int i;
    struct func_entry * entry = NULL;

    for(i = 1; i < jit->functable.num; i++)
    {
        entry = (struct func_entry *)jit_stack_get(&(jit->functable), i);
        if((strlen(entry->name) == len) && (strncmp(entry->name,name, len) == 0))
        {
            return i;
        }    
    }   

    return 0;
}


int jit_func_add(struct jit_state * jit, char * name,jit_func_t func, value_t ret_type)
{
    int id = 0;
    struct func_entry * entry = NULL;
    
    if((name != NULL) && (strlen(name) > 0) && (strlen(name) < ID_MAX) && 
            ((ret_type == INTEGER) || (ret_type == STRING)))
    {
        if((id = jit_func_lookup(jit, name, strlen(name))) == 0)
        {
            id = __jit_stack_alloc(&(jit->functable));
            if(id > 0)
            {
                entry = jit_get_func(jit, id);
                entry->name = name;
                entry->func = func;
                entry->ret_type = ret_type; 
            }
        }
    }

    return id;
}


int jit_register(struct jit_state *jit, struct func_entry * entry)
{
    int i = 0;
    int num;

    if((jit == NULL) || (entry == NULL))
    {
        return 0;
    }

    while(entry[i].func != NULL)
    {
        if(jit_func_add(jit,entry[i].name,entry[i].func, entry[i].ret_type) > 0)
        {
            num++;
        }
        i++;
    }

    return num;
}


/************end function ********************/




/******************************************************
 *              code gen
 ******************************************************/


int jit_next_inst(struct jit_state * jit)
{
    return jit->inst.num;
}

struct instruction * jit_get_inst(struct jit_state * jit, int id)
{
    return (struct instruction*)jit_stack_get(&(jit->inst),id);
}


int jit_alloc_inst(struct jit_state * jit)
{
    return __jit_stack_alloc(&(jit->inst));
}


int jit_gen_call(struct jit_state * jit, int fid)
{
    int iid = jit_alloc_inst(jit);
    struct instruction * inst = NULL;
    struct func_entry * fe = jit_get_func(jit, fid);
    int vid = 0;

    if((iid > 0) && (fe != NULL))
    {
        if((vid = jit_alloc_tmp(jit, fe->ret_type)) > 0)
        {    
            inst = jit_get_inst(jit, iid);
            inst->op = OP_CALL;
            inst->ret = vid;
            inst->v0 = fid; 
            inst->v1 = VAR_FUNCTION_RET_ID;
        }
    }

    return vid;
}


int jit_gen_je(struct jit_state * jit, int vid, int offset)
{
    struct instruction * inst = NULL;
    int iid = 0;

    if((iid = jit_alloc_inst(jit)) == 0)
    {
        return 0;
    }
    inst = jit_get_inst(jit, iid);        
    inst->op = OP_JE;
    inst->ret = 0;
    inst->v0 = vid;
    inst->v1 = offset;
    return iid;

}


int jit_jump_here(struct jit_state * jit, int iid)
{
    struct instruction * inst = jit_get_inst(jit, iid);        
    int next_id = jit_next_inst(jit); 
    int offset = next_id - iid;

    if(inst)
    {
        if(inst->op == OP_JE)
        {    
            inst->v1 = offset;
        }
        else 
        {
            inst->op = OP_JMP;
            inst->v0 = offset;
        }

        return 1;
    }
    
    return 0;
}


int jit_gen_jmp(struct jit_state * jit, int offset)
{
    struct instruction * inst = NULL;
    int iid = 0;

    if((iid = jit_alloc_inst(jit)) == 0)
    {
        return 0;
    }
    inst = jit_get_inst(jit, iid);        
    inst->op = OP_JMP;
    inst->ret = 0;
    inst->v0 = offset;
    inst->v1 = 0;

    return iid;
}


int jit_gen_nop(struct jit_state * jit)
{
    struct instruction * inst = NULL;
    int iid = 0;

    if((iid = jit_alloc_inst(jit)) == 0)
    {
        return 0;
    }
    inst = jit_get_inst(jit, iid);        
    inst->op = OP_NOP;
    inst->ret = 0;
    inst->v0 = 0;
    inst->v1 = 0;
 
    return iid;
}


int jit_gen_return(struct jit_state * jit)
{
    struct instruction * inst = NULL;
    int iid = 0;

    if((iid = jit_alloc_inst(jit)) == 0)
    {
        return 0;
    }
    inst = jit_get_inst(jit, iid);        
    inst->op = OP_RETURN;
    inst->ret = 0;
    inst->v0 = 0;
    inst->v1 = 0;
 
    return iid;
}



int jit_gen_arithmetic(struct jit_state * jit,opcode_t op, int v0, int v1)
{
    int iid = 0;
    int vid = 0;
    struct instruction * inst = NULL;
    switch(op)
    {
    case OP_ADD:
    case OP_SUB:
    case OP_MUL:
    case OP_DIV:
    case OP_REM:         
    case OP_GT:
    case OP_LT:
    case OP_GE:
    case OP_LE:
    case OP_EQ:
    case OP_NE:
    case OP_AND:
    case OP_OR:
        if(jit_var_type_check(jit, v1, INTEGER) == 0)
        {
            jit_error(jit,"type error");
            return 0;
        }    

    case OP_NOT:
        if(jit_var_type_check(jit, v0, INTEGER) == 0)
        {
            jit_error(jit,"type error");
            return 0;
        }  

        if((vid = jit_alloc_tmp(jit, INTEGER)) == 0)        
        {
            return 0;
        }

        if((iid = jit_alloc_inst(jit)) == 0)
        {
            return 0;
        }
        inst = jit_get_inst(jit, iid);        
        inst->op = op;
        inst->ret = vid;
        inst->v0 = v0;
        inst->v1 = v1;
        
        return vid;
    default:
        return 0;
    }

    return 0;
}    


int jit_gen_mov(struct jit_state * jit,int left, int num)
{
    struct instruction * inst = NULL;
    int iid;

    if((iid = jit_alloc_inst(jit)) == 0)
    {
        return 0;
    }
    inst = jit_get_inst(jit, iid);        
    inst->op = OP_MOV;
    inst->ret = left;
    inst->v0 = num;
    inst->v1 = 0;
 
    return iid;
}


int jit_gen_assign(struct jit_state * jit,int left, int right)
{
    struct instruction * inst = NULL;
    int iid;

    if((iid = jit_alloc_inst(jit)) == 0)
    {
        return 0;
    }
    inst = jit_get_inst(jit, iid);        
    inst->op = OP_ASSIGN;
    inst->ret = left;
    inst->v0 = right;
    inst->v1 = 0;
 
    return left;
}

static inline void jit_var_assign(struct variable * left, struct variable * right)
{
    left->type = right->type;
    if(left->type == INTEGER)
    {
        left->u.num = right->u.num;
    }    
    else
    {
        left->u.str = right->u.str;
    } 
}

int jit_run(struct jit_state * jit)
{
    int i = 1;
    struct instruction * inst;
    struct variable * v0 = NULL;
    struct variable * v1 = NULL;
    struct variable * ret = NULL;
    int next = 0;
    struct func_entry * fe;

    
    while(i < jit->inst.num)
    {
        next = 0;
        inst = jit_get_inst(jit, i);

        v0 = jit_get_var(jit, inst->v0);
        v1 = jit_get_var(jit, inst->v1);
        ret = jit_get_var(jit,inst->ret);

        
        switch(inst->op)
        {
        case OP_NOP:
                break;
        case OP_ADD:
            ret->type = INTEGER;
            ret->u.num = v0->u.num + v1->u.num;
            break;
        case OP_SUB:
            ret->u.num = v0->u.num - v1->u.num;
            break;
        case OP_MUL:
            ret->u.num = v0->u.num * v1->u.num;
            break;
        case OP_DIV:
            if(v1->u.num != 0)
            {    
                ret->u.num = v0->u.num / v1->u.num;
            }
            else
            {
                return 0;
            }
            break;
        case OP_REM:         
            if(v1->u.num != 0)
            {    
                ret->u.num = v0->u.num % v1->u.num;
            }
            else
            {
                return 0;
            }
            break;
        case OP_GT:
            ret->u.num = v0->u.num > v1->u.num;
            break;
        case OP_LT:
            ret->u.num = v0->u.num < v1->u.num;
            break;
        case OP_GE:
            ret->u.num = v0->u.num >= v1->u.num;
            break;
        case OP_LE:
            ret->u.num = v0->u.num <= v1->u.num;
            break;
        case OP_EQ:
            ret->u.num = v0->u.num == v1->u.num;
            break;
        case OP_NE:
            ret->u.num = v0->u.num != v1->u.num;
            break;
        case OP_AND:
            ret->u.num = v0->u.num && v1->u.num;
            break;
        case OP_OR:
            ret->u.num = v0->u.num || v1->u.num;
            break;
        case OP_NOT:
            ret->u.num = v0->u.num == 0;
            break;
        case OP_ASSIGN:
            jit_var_assign(ret,v0);
            break;
        case OP_MOV:
             ret->u.num = inst->v0;  
            break;
        case    OP_JMP:
            next = i + inst->v0;            
            break; 
        case    OP_JE:
            if(v0->u.num == 0)
            {
                next = i + inst->v1;        
            }
            break;
        case OP_CALL:
            fe = jit_get_func(jit, inst->v0);
            (fe->func)(jit);
            jit_var_assign(ret,v1);
            break;
        default:
            return 1;
        }
        /*
        if(ret != NULL)
        {    
            if(v1)
            {
                printf("%d %ld(%ld) %ld(%ld) %ld(%ld)\n",inst->op,inst->ret,ret->u.num, inst->v0, v0->u.num, inst->v1, v1->u.num);
            }
            else
            {
                printf("%d %ld(%ld) %ld(%ld)\n",inst->op,inst->ret,ret->u.num, inst->v0, v0->u.num);
            }    
        }
        */
        if(next)
        {
            i = next;
        }    
        else
        {
            i++;
        }    

    }


    return 1;
}






/*************end variable**********************/

/*
 *
 * expr ->  expr op term
 *          term
 * 
  * term->  num
 *          id
 *          str
 *          function
 *          (expr)
 *
 *
 * function: fname( arg_list )
 *           fname(  )
 *      
 * arg_list: expr
 *           arg_list, expr   
 *
 *
 * branch -> if expr then stmts elseif-part else-part  end
 *          
 *
 * */



static inline int _jit_char(struct jit_state * jit, int id)
{
    if((jit->lex.code != NULL) && (id < jit->lex.len))
    {
        return jit->lex.code[id];
    }

    return 0;
}


static inline int jit_char(struct jit_state * jit)
{
    return _jit_char(jit, jit->lex.next);
}    

static inline int jit_next_char(struct jit_state * jit)
{
    return _jit_char(jit, jit->lex.next + 1);
}


static inline void jit_skip_char(struct jit_state * jit,int num)
{
    if(num > 0)
    {
        jit->lex.next += num;
    }
}


static token_t jit_set_token(struct token * tk, token_t type, char * str, int len)
{
    if(tk)
    {    
        tk->type = type;
        tk->ptr = str;
        tk->len = len;

        return type;
    }

    return TK_NONE;
}


static token_t jit_get_token(struct jit_state * jit, struct token * tk, token_t type, int len)
{
    jit_set_token(tk, type, jit->lex.code + jit->lex.next,len);
    jit_skip_char(jit,len);
    return type;
}




static inline void jit_skip_space(struct jit_state * jit)
{
    int c;

    while(1)
    {
        c = jit_char(jit);
 
        if(c > 0)
        {
            if((c == ' ') || (c == '\t') || (c == '\r'))
            {
                jit_skip_char(jit,1);
                continue;    
            }
            else if(c == '\n')
            {
                jit->lex.line++;
                jit_skip_char(jit,1);
                continue;
            }
            else
            {
                break;
            }    
        }
        else
        {
            break;
        }
    }
}


static inline void jit_skip_comment(struct jit_state * jit)
{
    int c = jit_char(jit);
    int n = jit_next_char(jit);

    if((c == '-') && (n == '-'))
    {
        while(c > 0)
        {
            c = jit_char(jit);
            jit_skip_char(jit,1);
            if(c == '\n')
            {
                jit->lex.line++;
                return;    
            }
        }
    }    
}

static inline token_t jit_get_str(struct jit_state * jit,struct token * tk)
{
    int i;
    int len = 0;
    int flag = 0;
    int c, n;
    token_t type = TK_ERR;

    for(i = jit->lex.next; i < jit->lex.len; i++)
    {
        c = jit->lex.code[i];
        n = jit->lex.code[i + 1];
        len++;
        if(c <= 0)
        {
            break;
        }    
        if(c == '\"')
        {
            flag++;
            if(flag == 2)
            {
                break;
            }
        }    
        else if(c == '\n' || (c == '\r'))
        {
            break;
        }
        else if(c == '\\')
        {
            i++;
            len++;
        }    

        if(len >= STATIC_STR_MAX)
        {
            jit_error(jit,"static string exceeds limit");
            //jit_get_token(jit, tk, type, len);
            return TK_STR; 
        }
    }

    if(flag == 2)
    {
        type = TK_STR; 
    }

    jit_get_token(jit, tk, type, len);

    if(type == TK_ERR)
    {
        jit_error(jit,"missing \" ");
    }    


    return type;
}



static inline int jit_get_num(struct jit_state * jit,struct token * tk)
{
    int i;
    int len = 0;
    int c;
    token_t type = TK_ERR;

    for(i = jit->lex.next; i < jit->lex.len; i++)
    {
        c = jit->lex.code[i];

        if(c > 0)
        {
            if(isdigit(c))
            {    
                len++;
            }
            else if((isalpha(c)) || (c == '.'))
            {
                len++;
                goto out;
            }    
            else
            {
                break;
            }    
        }    
        else 
        {
            break;
        }    
    }

    if(len > 0)
    {
        type = TK_NUM;
    }
out:
    jit_get_token(jit, tk, type, len);

    if(type == TK_ERR)
    {
        jit_error(jit,"wrong number");
    }

    return type;
}


static inline int jit_get_id(struct jit_state * jit,struct token * tk)
{
    int i;
    int len = 0;
    int c;
    int dot = 0;
    token_t type =  TK_ERR;
    char * str = jit->lex.code + jit->lex.next;

    for(i = jit->lex.next; i < jit->lex.len; i++)
    {
        c = jit->lex.code[i];
            
        if(isalnum(c))
        {
            len++; 
        }
        else if(c == '.')
        {
            len++;
            dot++; 
        }
        else 
        {
            break;
        }    

        if(len >= ID_MAX)
        {
            jit_error(jit,"id length exceeds limit");
            return TK_ERR;
        }
    }

    if(len > 0)
    {
        if(dot == 0)
        {
            type = TK_ID;
            if(len == 2)
            {
                if(strncmp(str,"or",2) == 0)
                {
                    type = TK_OR;
                }
                else if(strncmp(str,"if",2) == 0)
                {
                    type = TK_IF;
                }
            }    
            else if(len == 3)
            {
                if(strncmp(str,"and",3) == 0)
                {
                    type = TK_AND;
                }
                else if(strncmp(str,"not",3) == 0)
                {
                    type = TK_NOT;
                }
                else if(strncmp(str,"end",3) == 0)
                {
                    type = TK_END;
                }
            }
            else if(len == 4)
            {
                if(strncmp(str,"then",4) == 0)
                {
                    type = TK_THEN;
                }
                else if(strncmp(str,"else",4) == 0)
                {
                    type = TK_ELSE;
                }
            }
            else if(len == 6)
            {
                if(strncmp(str,"elseif",6) == 0)
                {
                    type = TK_ELSEIF;
                }
                else if(strncmp(str,"return",6) == 0)
                {
                    type = TK_RETURN;
                }
            }
        }
        else if(dot == 1)
        {
            type = TK_FNAME; 
        }
    }

    jit_get_token(jit, tk, type, len);

    if(type == TK_ERR)
    {
        jit_error(jit,"syntax error, wrong id");
    }
    
    return type;
}


static token_t __jit_parse_token(struct jit_state * jit)
{
    int c,n; 
    struct token * tk = &(jit->lex.lookahead);

    jit_set_token(tk, TK_NONE,NULL, 0);
    do
    {    
        jit_skip_space(jit);
        jit_skip_comment(jit);
        c = jit_char(jit);
        n = jit_next_char(jit);
    }while(isspace(c) || ((c == '-') && (n == '-')));

    n = jit_next_char(jit);
    if(c <= 0)
    {
        return TK_NONE;
    }

    if(c == '+')
    {    
        return jit_get_token(jit, tk, TK_ADD, 1);
    }
    else if(c == '-')
    {
        return jit_get_token(jit, tk, TK_SUB, 1);
    }
    else if(c == '*')
    {
        return jit_get_token(jit, tk, TK_MUL,1);
    } 
    else if(c == '/')
    {    
        return jit_get_token(jit, tk, TK_DIV,1);
    }
    else if(c == '%')
    {
        return jit_get_token(jit, tk, TK_REM,1);
    }
    else if(c == '>')
    {
        if(n == '=')
        {
            return jit_get_token(jit, tk, TK_GE,2);
        }
        else
        {
            return jit_get_token(jit, tk, TK_GT,1);
        }    
    }
    else if(c == '<')
    {
        if(n == '=')
        {
            return jit_get_token(jit, tk, TK_LE,2);
        }    
        else
        {
            return jit_get_token(jit, tk, TK_LT,1);
        }    
    }
    else if(c == '!')
    {
        if(n == '=')
        {
            return jit_get_token(jit, tk, TK_NE,2);
        }
        else
        {
            jit_get_token(jit, tk, TK_ERR,2);
            jit_error(jit,"unknow token");
            return TK_ERR;
        }    
    }
    else if(c == '=')
    {
        if(n == '=')
        {
            return jit_get_token(jit, tk, TK_EQ,2);
        }
        else
        {
            return jit_get_token(jit, tk, TK_ASSIGN,1);
        }    
    }
    else if(c == '(')
    {
        return jit_get_token(jit, tk, TK_LBRACKET,1);
    }
    else if(c == ')')
    {
        return jit_get_token(jit, tk, TK_RBRACKET,1);
    }
    else if(c == ';')
    {
        return jit_get_token(jit, tk, TK_SEMICOLON,1);
    }
    else if(c == ',')
    {
        return jit_get_token(jit, tk, TK_COMMA,1);
    }
    else if(c == '\"')
    {
        return jit_get_str(jit,tk);
    } 
    else if(isdigit(c))
    {
        return jit_get_num(jit,tk);
    }
    else if(isalpha(c))
    {
        return jit_get_id(jit,tk);
    }
    else
    {
        jit_get_token(jit, tk, TK_ERR,1);
        jit_error(jit,"unknow token");
        return TK_ERR;
    }    
}



struct token * jit_current_token(struct jit_state * jit)
{
    return &(jit->lex.lookahead);
}

struct token * jit_parse_token(struct jit_state * jit)
{
    token_t tk = __jit_parse_token(jit);

    if(tk == TK_ERR)
    {
        jit_error(jit,"syntax error, unknow token ");
    }    


    return jit_current_token(jit);
}

static int jit_match(struct jit_state * jit, token_t type)
{
    struct token * tk = NULL;

    tk = jit_current_token(jit);

    if(tk->type == type)
    {
        jit_parse_token(jit);
        return 1;
    }    
    else
    {
        return 0;
    } 
}



/*
 * return 
 *  0   fail
 *  1   ok
 * */


int jit_return(struct jit_state * jit)
{
    jit_match(jit,TK_RETURN);

    if(jit_gen_return(jit) > 0)
    {
        if(jit_match(jit,TK_SEMICOLON) == 0)
        {
            jit_error(jit,"syntax error, expecting ';'");
            return 0;
        }

        return 1;
    }
    return 0;
}



int jit_function(struct jit_state * jit);
static int jit_expr(struct jit_state * jit);


long str_to_num(char * str, int len)
{
    long num = 0;
    int i;
    for(i = 0; i < len; i++)
    {
        num = num * 10 + str[i] - '0';
    }

    return num;
}


static int jit_term(struct jit_state * jit)
{
    struct token * tk = jit_current_token(jit);
    token_t type = tk->type;
    long num = 0;
    int id;

    switch(type)
    {
    case    TK_LBRACKET:
                jit_match(jit,TK_LBRACKET);
                if( (id = jit_expr(jit)) == 0)
                {
                    return 0;
                }    
                if(jit_match(jit,TK_RBRACKET) == 0)
                {
                    jit_error(jit,"syntax error, expecting ')'");
                    return 0;
                }

                return id;
                break;
    case    TK_STR:
                id = jit_static_string(jit, tk->ptr, tk->len);
                jit_match(jit,TK_STR);
                return id;
                break;    
    case    TK_NUM:
                num = str_to_num(tk->ptr, tk->len);
                jit_match(jit,TK_NUM);
                return jit_alloc_const_integer(jit, num);
                break;
    case    TK_ID:    
                if((id = jit_lookup_var(jit,tk->ptr, tk->len)) == 0)
                {
                    jit_error(jit,"unknown variable ");
                    return 0;
                }
                jit_match(jit,TK_ID);
                return id;
                break;
    case    TK_FNAME:
                if((id = jit_function(jit)) == 0)
                {
                    return 0;
                }
                return id;
                break;
    case    TK_NOT:
                jit_match(jit,TK_NOT);
                if((id = jit_expr(jit)) == 0)
                {
                    return 0;
                }

                return jit_gen_arithmetic(jit,OP_NOT, id, 0);

                break;
    default:
            jit_error(jit,"syntax error");            
            return 0;
    }


    return 1;
}

static int jit_expr(struct jit_state * jit)
{
    struct token * tk = NULL;
    token_t type = TK_NONE;
    int v0;
    int v1;
    int ret;
    int term_num = 0;

    if((v0 = jit_term(jit)) == 0)
    {
        return 0;
    }

    term_num++;

    ret = v0;

    while(1)
    {    
        tk = jit_current_token(jit);
        type = tk->type;

        switch(type)
        {
        case    TK_ADD:
        case    TK_SUB:
        case    TK_MUL:
        case    TK_DIV:
        case    TK_REM:
              
        case    TK_GT:
        case    TK_LT:
        case    TK_GE:
        case    TK_LE:
        case    TK_EQ:
        case    TK_NE:

        case    TK_AND:
        case    TK_OR:
                jit_match(jit,type);
                if((v1 = jit_term(jit)) == 0)
                {
                    return 0;
                }
                term_num++;
                ret = jit_gen_arithmetic(jit,(opcode_t)type, v0, v1);
                v0 = ret;
                break;
        default :
            if(term_num > 2)
            {
                jit_error(jit,"use ( )");
                return 0;
            }

            return ret;
        }
    }

    return ret;
}


int jit_assign(struct jit_state * jit)
{
    int left;
    int right;
    struct token * tk  = jit_current_token(jit);
    struct variable * vl;
    struct variable * vr;    


    if((left = jit_alloc_var(jit,tk->ptr, tk->len)) == 0)
    {
        return  0;
    }

    if(jit_match(jit,TK_ID) == 0)
    {
        return 0;
    }

    if(jit_match(jit,TK_ASSIGN) == 0)
    {
        return 0;
    }    

    if((right = jit_expr(jit)) == 0)
    {
        return 0;
    }

    if(jit_match(jit,TK_SEMICOLON) == 0)
    {
        return 0;
    }

    vl = jit_get_var(jit, left);
    vr = jit_get_var(jit, right);
    
    if(vl->type == 0)
    {
        vl->type = vr->type;
    }

    if(vl->type != vr->type)
    {
        jit_error(jit,"type unmatch");
    }

    return jit_gen_assign(jit,left, right);
}


int jit_arglist(struct jit_state * jit)
{
    token_t type;
    struct token * tk;
    int num = 0;
    int vid;

    jit_clear_argc(jit);

    while(1)
    {    
        tk = jit_current_token(jit);
        type = tk->type;

        if(type == TK_RBRACKET)
        {
            return 1;
        }

        if((vid = jit_expr(jit)) == 0)
        {
            jit_error(jit,"syntax error, expecting argument or ')' ");
            return 0;
        }
        
        jit_push_argv(jit,vid);

        num++;
        if(num > ARG_MAX)
        {
            jit_error(jit,"arguments  exceed limit");
            return 0; 
        }

        tk = jit_current_token(jit);
        type = tk->type;

        if(type == TK_RBRACKET)
        {
            jit_gen_mov(jit,VAR_FUNCTION_ARGC_ID,num);
            return 1;
        }

        if(jit_match(jit,TK_COMMA) == 0)
        {
            jit_error(jit,"syntax error, expecting ',' or ')' ");
            return 0;
        }
    }

    return 1;
}


int jit_function(struct jit_state * jit)
{
    struct token * tk = jit_current_token(jit);
    int fid = jit_func_lookup(jit, tk->ptr, tk->len);

    if(fid == 0)
    {
        jit_error(jit,"unknown function");
        return 0;
    }
    jit_match(jit,TK_FNAME);
    
    if(jit_match(jit,TK_LBRACKET) == 0)
    {
        jit_error(jit,"syntax error, expecting '('");
        return 0;
    }

    if(jit_arglist(jit) == 0)
    {
        return 0;
    }

    if(jit_match(jit,TK_RBRACKET) == 0)
    {
        jit_error(jit,"syntax error, expecting ')'");
        return 0;
    }

    return jit_gen_call(jit, fid);
}


int jit_stmt_list(struct jit_state * jit);


int jit_branch_part(struct jit_state * jit)
{
    int vid = 0;
    int je_id = 0;
    int nop_id = 0;

    if((vid = jit_expr(jit)) == 0)
    {
        return 0;
    }

    if(jit_var_type_check(jit, vid, INTEGER) == 0)
    {
        jit_error(jit,"type error");
        return 0; 
    }


    je_id = jit_gen_je(jit, vid, 0);

    if(jit_match(jit,TK_THEN) == 0)
    {
        return 0;
    }

    if(jit_stmt_list(jit) == 0)
    {
        return 0;
    }

    nop_id = jit_gen_nop(jit);
    jit_jump_here(jit, je_id);
    return nop_id;
}

/*  
 * jump instruction linked by <ret>
 *  */

void inst_link_add(struct jit_state * jit, struct instruction * head, int iid)
{
    struct instruction * inst =  jit_get_inst(jit, iid);
    inst->ret = head->ret;
    head->ret = iid;
}

int inst_link_del(struct jit_state * jit, struct instruction * head)
{
    int iid = head->ret;
    int next = 0;
    struct instruction * inst;
    if(iid > 0)
    {
        inst = jit_get_inst(jit, iid);
        next = inst->ret;
        head->ret = next;
    }

    return iid;
}

int jit_branch(struct jit_state * jit)
{
    struct token * tk = jit_current_token(jit);
    token_t type = tk->type;
    int nop_id = 0;
    struct instruction head ={0,0,0,0};
    int fillback = 0;

    if(jit_match(jit,TK_IF) == 0)
    {
        return 0;
    }

loop:
    if((nop_id = jit_branch_part(jit)) == 0)
    {
        return 0;
    }

    inst_link_add(jit,&head,nop_id);

    tk = jit_current_token(jit);
    type = tk->type;

    if(type == TK_END)
    {
        jit_match(jit,TK_END);
    }
    else if(type == TK_ELSE)
    {
        fillback++;
        jit_match(jit,TK_ELSE);

        if(jit_stmt_list(jit) == 0)
        {
            return 0;
        }

        if(jit_match(jit,TK_END) == 0)
        {
            return 0;
        }
    }
    else if(type == TK_ELSEIF)
    {
        fillback++;
        jit_match(jit,TK_ELSEIF);
        goto loop;
    }

    if(fillback)
    {    
        while((nop_id = inst_link_del(jit,&head)) >0)
        {
            jit_jump_here(jit, nop_id);
        }
    }
    return 1;
}




/*
 * return 
 *  0   fail
 *  1   ok
 * */
int jit_stmt_list(struct jit_state * jit)
{
    token_t type;
    struct token * tk;

    while(1)
    {    
        tk = jit_current_token(jit);
        type = tk->type;

        if (type == TK_NONE)
        {
            return 1;
        }
        else if(type == TK_FNAME)
        {
            if(jit_function(jit) == 0)
            {
                goto err;
            }

            if(jit_match(jit,TK_SEMICOLON) == 0)
            {
                jit_error(jit,"syntax error, expecting ';'");
                goto err;
            }
        }
        else if(type == TK_RETURN)
        {
            if(jit_return(jit) == 0)
            {
                goto err;
            }
        }    
        else if(type == TK_ID)
        {
            if(jit_assign(jit) == 0)
            {
                goto err; 
            }
        }
        else if(type == TK_IF)
        {
            if(jit_branch(jit) ==0)
            {
                goto err;
            }
        }
        else
        {
            return 1;
        }    

        jit_free_tmp(jit);
    }
    return 1;
err:
    return 0;
}


int jit_compile(struct jit_state * jit)
{
    struct token * tk;
    token_t type;
    

    tk = jit_parse_token(jit);
    type = tk->type;

    if((type == TK_ERR))
    {
        return 0;
    }
    else if (type == TK_NONE)
    {
        return 1;
    }

    if(jit_stmt_list(jit) != 0)
    {    
        jit_static_string_refill(jit);
        return 1;
    }

    return 0;
}





int jit_init_data(struct jit_state *jit)
{
    struct jit_stack * st = &(jit->data);
    int esize = sizeof(struct variable);
    int num = jit_stack_init(st, esize, STACK_INIT_NUM);
    struct variable * var = (struct variable*)(st->base);

    if(num > 0)
    {
        var->type = INTEGER;
        var->status = VAR_ST_CONST;
        st->num += VAR_FUNCTION_NUM;
    }

    return num;
}


int jit_init_inst(struct jit_state *jit)
{
    struct jit_stack * st = &(jit->inst);
    int esize = sizeof(struct instruction);
    int num = jit_stack_init(st, esize, STACK_INIT_NUM);
         
    /* fist inst is none */

    return num;
}


int jit_init_symtable(struct jit_state *jit)
{
    struct jit_stack * st = &(jit->symtable);
    int esize = sizeof(struct symbol);
    int num = jit_stack_init(st, esize, STACK_INIT_NUM);

    return num;
}

int jit_init_functable(struct jit_state *jit)
{
    struct jit_stack * st = &(jit->functable);
    int esize = sizeof(struct func_entry);
    int num = jit_stack_init(st, esize,STACK_INIT_NUM);

    return num;
}

int jit_init_static_str(struct jit_state *jit)
{
    return jit_stack_init(&(jit->static_str),1,STACK_STR_INIT_NUM);
}



void jit_free(struct jit_state * jit)
{
    if(jit)
    {
        if(jit->data.base)
        {
            LAN_FREE(jit->data.base);
        }

        if(jit->inst.base)
        {
            LAN_FREE(jit->inst.base);
        }

        if(jit->symtable.base)
        {
            LAN_FREE(jit->symtable.base);
        }

        if(jit->functable.base)
        {
            LAN_FREE(jit->functable.base);
        }

        if(jit->static_str.base)
        {
            LAN_FREE(jit->static_str.base);
        }

        LAN_FREE(jit);
    }
}

struct jit_state * jit_new( )
{
    struct jit_state * jit = NULL;
    
    jit = (struct jit_state*)LAN_CALLOC(1,sizeof(struct jit_state));

    if(jit_init_data(jit) == 0)
    {
        goto err;
    }

    if(jit_init_inst(jit) == 0)
    {
        goto err;
    }

    if(jit_init_symtable(jit) == 0)
    {
        goto err;
    }

    if(jit_init_functable(jit) == 0)
    {
        goto err;
    }

    if(jit_init_static_str(jit) == 0)
    {
        goto err;
    }

    return jit;

err:
    jit_free(jit);
    return NULL;
}


/*
 * return
 *  0   fail
 *  1   ok
 *
 * */
int jit_load(struct jit_state * jit,char * code)
{
    int ret = 0;
    jit->lex.code = code;
    jit->lex.len = strlen(code);
    jit->lex.line = 1;
    if((ret = jit_compile(jit)) > 0)
    {
        jit_run(jit);
    }

    jit->lex.code = NULL;
    return ret;
}


