/*
 * db.h - database api
 *
 * Copyright (c) 2015-2022 Gemini Complex Corporation. All rights reserved.
 *
 */
#ifndef DB_H
#define DB_H

#ifdef __cplusplus
extern "C" {
#endif

#include "sqlite3.h"

#define PASS_SALT ("1DE4CFC74A5F9AC2CC834E029E5D95D1")

enum db_user_states {
    USER_NONE        = (1 << 0), 
    USER_ACTIVE      = (1 << 1), 
    USER_UNSUPPORTED = (1 << 1), 
    USER_BANNED      = (1 << 2), 
};

enum db_job_states {
    JOB_NONE     = (1 << 0), 
    JOB_IDLE     = (1 << 1), 
    JOB_PENDING  = (1 << 2), 
    JOB_RUNNING  = (1 << 3), 
    JOB_KILLING  = (1 << 4), 
    JOB_KILLED   = (1 << 5), 
    JOB_DONE     = (1 << 6)
};

enum db_prgm_states {
    PRGM_NONE     = (1 << 0), 
    PRGM_IDLE     = (1 << 1), 
    PRGM_PENDING  = (1 << 2), 
    PRGM_RUNNING  = (1 << 3), 
    PRGM_KILLING  = (1 << 4), 
    PRGM_KILLED   = (1 << 5), 
    PRGM_DONE     = (1 << 6)
};

enum db_stim_states {
    STIM_NONE     = (1 << 0), 
    STIM_IDLE     = (1 << 1), 
    STIM_PENDING  = (1 << 2), 
    STIM_RUNNING  = (1 << 3), 
    STIM_KILLING  = (1 << 4), 
    STIM_KILLED   = (1 << 5), 
    STIM_DONE     = (1 << 6)
};

enum db_mount_states {
    MOUNT_NONE        = (1 << 0), 
    MOUNT_UNMOUNTED   = (1 << 1), 
    MOUNT_MOUNTING    = (1 << 2), 
    MOUNT_MOUNTED     = (1 << 3), 
    MOUNT_UNMOUNTING  = (1 << 4), 
    MOUNT_FAILED      = (1 << 5), 
};

struct db_user {
    int64_t id;
    const char *username;
    const char *password;
    const char *email;
    const char *session;
    int32_t is_admin;
};

struct db_board {
    int64_t id;
    const char *dna;
    const char *name;
    const char *ip_addr;
    int32_t cur_dut_board_id;
    int32_t is_master;
};

struct db_dut_board {
    int64_t id;
    const char *dna;
    const char *name;
    const char *profile_path;
};

struct db_job {
    int64_t id;
    int64_t board_id;
    int64_t dut_board_id;
    int64_t user_id;
    enum db_job_states state;
};

struct db_prgm {
    int64_t id;
    int64_t job_id;
    int64_t mount_id;
    const char *path;
    const char *body;
    time_t date_start;
    time_t date_end;
    int32_t return_code;
    const char *error_msg;
    int64_t last_stim_id;
    int32_t did_fail;
    int64_t failing_vec;
    enum db_prgm_states state;
};

struct db_prgm_log {
    int64_t id;
    int64_t prgm_id;
    time_t date_created;
    const char *line;
};

struct db_stim {
    int64_t id;
    int64_t prgm_id;
    const char *path;
    int32_t did_fail;
    int64_t failing_vec;
    enum db_stim_states state;
};

struct db_fail_pin {
    int64_t id;
    int64_t stim_id;
    int64_t dut_io_id;
    int32_t did_fail;
};

struct db_mount {
    int64_t id;
    const char *name;
    const char *ip_addr;
    const char *remote_path;
    const char *local_point;
    const char *message;
    enum db_mount_states state;
};

struct db {

    /*
     * public
     */
    char *path;
    bool is_open;

    /*
     * private
     */
    sqlite3 *_db;

};


/*
 * db management funcs
 */
struct db *db_create();
void db_open(struct db *db, const char *path);
void db_close(struct db *db);
void db_free(struct db *db);

/*
 * Use these to free structs returned from
 * funcs below.
 *
 */
void db_free_user(struct db_user *user);
void db_free_board(struct db_board *board);
void db_free_dut_board(struct db_dut_board *dut_board);
void db_free_job(struct db_job *job);
void db_free_prgm(struct db_prgm *prgm);
void db_free_prgm_log(struct db_prgm_log *prgm_log);
void db_free_stim(struct db_stim *stim);
void db_free_fail_pin(struct db_fail_pin *fail_pin);
void db_free_mount(struct db_mount *mount);

/*
 * use these to insert, update or get rows from db
 */
struct db_user* db_get_user_by_id(struct db *db, int64_t user_id);
int64_t db_insert_user(struct db *db, 
        const char *username, const char *password, const char *email, const char *session, int32_t is_admin);
struct db_board* db_get_board_by_id(struct db *db, int64_t board_id);
struct db_board* db_get_board_by_dna(struct db *db, const char *dna);
int64_t db_insert_board(struct db *db, 
        const char *dna, const char *name, const char *ip_addr, int32_t cur_dut_board_id, int32_t is_master);
int64_t db_update_board(struct db *db, struct db_board *board);
struct db_dut_board* db_get_dut_board_by_id(struct db *db, int64_t dut_board_id);
int64_t db_insert_dut_board(struct db *db, 
        const char *dna, const char *name, const char *profile_path);
struct db_job* db_get_job_by_id(struct db *db, int64_t job_id);
int64_t db_insert_job(struct db *db, 
        int64_t board_id, int64_t dut_board_id, int64_t user_id, enum db_job_states state);
int64_t db_update_job(struct db *db, struct db_job *job);
uint64_t db_get_num_jobs(struct db *db, 
        int64_t board_id, int64_t dut_board_id,
        int64_t user_id, int32_t states);
struct db_job** db_get_jobs(struct db *db, 
        int64_t board_id, int64_t dut_board_id, 
        int64_t user_id, int32_t states);
struct db_prgm* db_get_prgm_by_id(struct db *db, int64_t prgm_id);
int64_t db_insert_prgm(struct db *db, 
        int64_t job_id, int64_t mount_id, const char *path, const char *body, 
        int32_t return_code, const char *error_msg, int32_t last_stim_id, 
        int32_t did_fail, int32_t failing_vec, enum db_job_states state);
int64_t db_update_prgm(struct db *db, struct db_prgm *prgm);
uint64_t db_get_num_prgms(struct db *db, 
        int64_t job_id, int64_t mount_id, const char *path, const char *body, 
        int32_t return_code, const char *error_msg, int32_t last_stim_id, 
        int32_t did_fail, int32_t failing_vec, int32_t states);
struct db_prgm** db_get_prgms(struct db *db, 
        int64_t job_id, int64_t mount_id, const char *path, const char *body,
        int32_t return_code, const char *error_msg, int32_t last_stim_id,
        int32_t did_fail, int32_t failing_vec, int32_t states);
struct db_prgm_log* db_get_prgm_log_by_id(struct db *db, int64_t prgm_log_id);
int64_t db_insert_prgm_log(struct db *db, 
        int64_t prgm_id, const char *line);
struct db_stim* db_get_stim_by_id(struct db *db, int64_t stim_id);
int64_t db_insert_stim(struct db *db, 
        int64_t prgm_id, const char *path, int32_t did_fail, 
        int64_t failing_vec, enum db_stim_states state);
int64_t db_update_stim(struct db *db, struct db_stim *stim);
struct db_fail_pin* db_get_fail_pin_by_id(struct db *db, int64_t fail_pin_id);
int64_t db_insert_fail_pin(struct db *db, 
    int64_t stim_id, int64_t dut_io_id, int32_t did_fail);
struct db_mount* db_get_mount_by_id(struct db *db, int64_t mount_id);
int64_t db_insert_mount(struct db *db, 
        const char *name, const char *ip_addr, const char *path, 
        const char *point, const char *message, enum db_mount_states state);
int64_t db_update_mount(struct db *db, struct db_mount *mount);
uint64_t db_get_num_mounts(struct db *db, 
        const char *name, const char *ip_addr, const char *remote_path,
        const char *local_point, const char *message, int32_t states);
struct db_mount** db_get_mounts(struct db *db, 
        const char *name, const char *ip_addr, const char *remote_path,
        const char *local_point, const char *message, int32_t states);

#ifdef __cplusplus
}
#endif
#endif
