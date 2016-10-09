#include "lan.h"
#include <stdio.h>
#include <stdlib.h>

#define BUF_LEN 4096
static int file_size(char * path)
{
    FILE * fp = NULL;
    int size = 0;
    char buf[BUF_LEN];
    int len = 0;

    if(path != NULL)
    {
        if((fp = fopen(path,"r")) != NULL)
        {
            while((len = fread(buf, 1, BUF_LEN, fp)) > 0)
            {
                size += len;
            }
         
            fclose(fp);
        }
    }
    return size;
}

char * file_read(char *path) 
{
    FILE * fp = NULL;
    char * buf = NULL;
    int len = 0;
    int size = 0;

    if(path == NULL)
    {
        return NULL;
    }

    if((size = file_size(path)) <= 0)
    {
        return NULL;
    }

    size += 10;
    if((buf = malloc(size)) == NULL)
    {
        return NULL;
    }

    if((fp = fopen(path,"r")) != NULL)
    {
        len = fread(buf, 1, size, fp);
        if(len > 0)
        {    
            buf[len] = 0;
        }

        fclose (fp);
    }

    if(len <= 0)
    {
        free(buf);
        buf = NULL;
    }    

    return buf;
}


int log_info(struct jit_state * jit)
{
    printf("hello\n");
    return 1;
}    


int log_var(struct jit_state * jit)
{
    int argc = jit_get_argc(jit);  
    long num;
    int i,j;
    str_t str;
    long sum = 0;
        
    for(i = 0; i < argc; i++)
    {
        if(jit_get_argv_int(jit, i, &num))
        {
            printf("%ld ",num);
            sum += num;
        }
        else if (jit_get_argv_str(jit, i,&str))
        {
            for(j = 0; j < str.len; j++)
            {
                printf("%c",str.data[j]);
            }
            printf(" ");
        }
    }    

    printf("\n");

    jit_return_int(jit,sum);

    return 1;
}    









struct func_entry log_table[] = {
    {"log.info",log_info, INTEGER},
    {"log.var",log_var, INTEGER},
    {NULL,NULL,INTEGER}
};


void test(int num, char * code)
{
    int i;
    struct jit_state * jit = NULL;

    for(i = 0; i < num; i++)
    {
            jit = jit_new();
            jit_register(jit, log_table);
            jit_load(jit,code);
            jit_free(jit);
    }
}



int main(int argc, char ** argv)
{
    char * code =  NULL;
    int num = 0;
    if(argc == 3)
    {    
        num = atoi(argv[1]);
        code =  file_read(argv[2]) ;

        if(code != NULL)
        {
            test(num, code);
            free(code);
        }
    }
    else 
    {
        printf("Usage:\n\t%s num path\n",argv[0]);
    }

    return 0;
}


