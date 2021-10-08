
/*
 * Prgm File
 *
 */

#include "common.h"
#include "util.h"
#include "prgm.h"

#include "lib/fe/fe.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>
#include <fcntl.h>
#include <inttypes.h>
#include <ctype.h>
#include <setjmp.h>


void _prgm_create_fe_ctx(struct prgm *prgm){
    if(prgm == NULL){
        die("pointer is NULL");
    }

    if(!(prgm->_fe_data == NULL && prgm->_fe_ctx == NULL)){
        die("fe ctx already created");
    }
    
    prgm->_fe_data_size = FE_DATA_SIZE;

    if((prgm->_fe_data = malloc(prgm->_fe_data_size)) == NULL){
        die("failed to malloc fe data");
    }

    if((prgm->_fe_ctx = fe_open(prgm->_fe_data, prgm->_fe_data_size)) == NULL){
        die("failed to create fe context");
    }

    return;
}

void _prgm_free_fe_ctx(struct prgm *prgm){
    if(prgm == NULL){
        die("pointer is NULL");
    }

    if(prgm->_fe_data == NULL && prgm->_fe_ctx == NULL){
        die("fe ctx already freed");
    }

    if(prgm->_fe_ctx != NULL){
        fe_close(prgm->_fe_ctx);
        prgm->_fe_ctx = NULL;
    }

    if(prgm->_fe_data != NULL){
        free(prgm->_fe_data);
        prgm->_fe_data = NULL;
    }

    prgm->_fe_data_size = 0;

    return;
}


static fe_Object* f_loadstims(fe_Context *_fe_ctx, fe_Object *arg){
    char s[100];
    fe_tostring(_fe_ctx, fe_nextarg(_fe_ctx, &arg), s, sizeof(s));
    printf("'%s'\n", s);
    return fe_string(_fe_ctx, strdup(s)); 
}

/*
 * Adds the gemini cfuncs to fe global funcion list.
 *
 */
void _add_fe_gemini_funcs(fe_Context *_fe_ctx){
    if(_fe_ctx == NULL){
        die("ptr is NULL");
    }


    fe_set(_fe_ctx, fe_symbol(_fe_ctx, "loadstims"), fe_cfunc(_fe_ctx, f_loadstims)); 
}


struct prgm *prgm_create(){
    struct prgm *prgm = NULL;

    if((prgm = (struct prgm*)malloc(sizeof(struct prgm))) == NULL){
        die("failed to malloc prgm");
    }

    prgm->_fe_data_size = 0;
    prgm->_fe_data = NULL;
    prgm->_fe_ctx = NULL;

    _prgm_create_fe_ctx(prgm);
    _add_fe_gemini_funcs(prgm->_fe_ctx);

    return prgm;
}

void prgm_open(struct prgm *prgm, char *path){
    char *real_path = NULL;

    if(prgm == NULL){
        die("pointer is NULL");
    }

    if(path == NULL){
        die("pointer is NULL");
    }

    if(prgm->is_path_open){
        die("prgm path '%s' already open", path);
    }

    if((real_path = realpath(path, NULL)) == NULL){
        die("invalid prgm path '%s'", path);
    }

    prgm->path = strdup(real_path);
    free(real_path);

    if(util_fopen(prgm->path, &prgm->_path_fd, &prgm->_path_fp, &prgm->_path_size)){
        die("failed to open prgm file '%s'", path);
    }

    prgm->is_path_open = true;

    return;
}

void prgm_close(struct prgm *prgm){
    if(prgm->is_path_open){
        fclose(prgm->_path_fp);
        close(prgm->_path_fd);
        prgm->_path_size = 0;
        prgm->is_path_open = false;
    }
    return;
}

void prgm_free(struct prgm *prgm){
    if(prgm == NULL){
        die("pointer is NULL");
    }

    if(prgm->path != NULL){
        free(prgm->path);
    }

    prgm_close(prgm);

    _prgm_free_fe_ctx(prgm);

    free(prgm);
    return;
}


int prgm_run(struct prgm *prgm){
    if(prgm == NULL){
        die("pointer is NULL");
    }

    if(prgm->is_path_open == false){
        die("prgm is not open");
    }

    int ret = prgm_repl(prgm, prgm->_path_fp, stdout, stderr);

    // rewind prgm path if we want to re-run
    fseek(prgm->_path_fp, 0, SEEK_SET);

    return ret;
}

static jmp_buf fe_jmp_buf;
static FILE *fe_fp_err = NULL;

static void _fe_onerror(fe_Context *ctx, const char *msg, fe_Object *cl) {
    //unused(ctx), unused(cl);
    FILE *fp_err = stderr;
    if(fe_fp_err != NULL){
        fp_err = fe_fp_err;
    }
    fprintf(fp_err, "error: %s\n", msg);
    longjmp(fe_jmp_buf, -1);
}

int prgm_repl(struct prgm *prgm, FILE *fp_in, FILE *fp_out, FILE *fp_err){
    int gc;
    fe_Object *obj;

    if(prgm == NULL){
        die("pointer is NULL");
    }

    FILE *volatile _fp_in = stdin;
    FILE  *_fp_out = stdout;

    if(fp_in != NULL){
        _fp_in = fp_in;
    }

    if(fp_out != NULL){
        _fp_out = fp_out;
    }

    if(fp_err != NULL){
        fe_fp_err = fp_err;
    }else{
        fe_fp_err = stderr;
    }

    if(_fp_in == stdin){ 
        fe_handlers(prgm->_fe_ctx)->error = _fe_onerror;
    }

    gc = fe_savegc(prgm->_fe_ctx);
    int jmp_ret = setjmp(fe_jmp_buf);

    if(jmp_ret == -1){
        return EXIT_FAILURE;
    }

    // bust out the repl
    for(;;){
        fe_restoregc(prgm->_fe_ctx, gc);
        if(_fp_in == stdin){ fprintf(_fp_out, "> "); }

        obj = fe_readfp(prgm->_fe_ctx, _fp_in);

        // nothing left to read
        if(!obj){ break; }

        // evaluate read object
        obj = fe_eval(prgm->_fe_ctx, obj);

        if(_fp_in == stdin) {
            fe_writefp(prgm->_fe_ctx, obj, _fp_out);
            fprintf(_fp_out, "\n");
        }

    }

    return EXIT_SUCCESS;
}




