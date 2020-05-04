#ifndef PRGM_H
#define PRGM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lib/fe/fe.h"

// 32 MB fe data scratch pad
#define FE_DATA_SIZE (1024*1024*3)

struct prgm {
    
    /*
     * public
     */
    char *path;
    bool is_path_open;

    /* 
     * private
     */

    // prgm path
    int _path_fd;
    FILE *_path_fp;
    off_t _path_size;

    // fe context
    uint32_t _fe_data_size;
    void *_fe_data;
    fe_Context *_fe_ctx;
};


struct prgm *prgm_create();
void prgm_open(struct prgm *prgm, char *path);
void prgm_close(struct prgm *prgm);
void prgm_free(struct prgm *prgm);
int prgm_run(struct prgm *prgm);
int prgm_repl(struct prgm *prgm, FILE *fp_in, FILE *fp_out, FILE *fp_err);


#ifdef __cplusplus
}
#endif
#endif
