/*
 * db.c - database api
 *
 * Copyright (c) 2015-2022 Gemini Complex Corporation. All rights reserved.
 *
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <stdbool.h>
#include <fcntl.h>
#include <inttypes.h>

#include "sqlite3.h"
#include "common.h"
#include "lib/uthash/utstring.h"
#include "util.h"
#include "sql.h"
#include "db.h"


struct db *db_create(){
    struct db *db = NULL;

    if((db = (struct db*)malloc(sizeof(struct db))) == NULL){
        die("malloc failed");
    }

    db->path = NULL;
    db->_db = NULL;
    db->is_open = false;

    return db;
}

void db_free(struct db *db){
    if(db == NULL){
        die("pointer is null");
    }
    if(db->is_open){
        die("failed to free db because db connection is still open.");
    }
    free(db);
    return;
}

void db_open(struct db *db, const char *path){
    char *err_msg = NULL;

    if(db == NULL){
        die("pointer is null");
    }

    if(path == NULL){
        die("pointer is null");
    }

    if(db->is_open == true){
        return;
    }

    int rc = sqlite3_open_v2(path, &db->_db, SQLITE_OPEN_CREATE | SQLITE_OPEN_READWRITE, NULL);

    if(db->_db == NULL){
        bye("failed to open database");
    }

    if (rc != SQLITE_OK) {
        sqlite3_close(db->_db);
        bye("cannot open database: %s\n", sqlite3_errmsg(db->_db));
    }

    db->is_open = true;

    // execute the create db sql
    rc = sqlite3_exec(db->_db, DB_SQL, 0, 0, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "init sql error: %s\n", err_msg);
        sqlite3_free(err_msg);
        sqlite3_close(db->_db);
        exit(EXIT_FAILURE);
    }

    return;
}

void db_close(struct db *db){
    if(db == NULL){
        die("pointer is null");
    }
    if(db->is_open == false){
        return;
    }
    sqlite3_close(db->_db);
    db->is_open = false;
    return;
}

static struct db_user* db_make_user(sqlite3_stmt *res){
    struct db_user *user = NULL;
    if(res == NULL){
        die("pointer is null");
    }
    if((user = (struct db_user*)calloc(1, sizeof(struct db_user))) == NULL){
        die("malloc failed");
    }
    user->id = sqlite3_column_int64(res, 0);
    user->username = (const char *)sqlite3_column_text(res, 3);
    user->password = (const char *)sqlite3_column_text(res, 4);
    user->session = (const char *)sqlite3_column_text(res, 5);
    user->is_admin = sqlite3_column_int(res, 6);
    return user;
}

static struct db_board* db_make_board(sqlite3_stmt *res){
    struct db_board *board = NULL;
    if(res == NULL){
        die("pointer is null");
    }
    if((board = (struct db_board*)calloc(1, sizeof(struct db_board))) == NULL){
        die("malloc failed");
    }
    board->id = sqlite3_column_int64(res, 0);
    board->dna = (const char *)sqlite3_column_text(res, 1);
    board->name = (const char *)sqlite3_column_text(res, 2);
    board->ip_addr = (const char *)sqlite3_column_text(res, 3);
    board->cur_dut_board_id = sqlite3_column_int(res, 4);
    board->is_master = sqlite3_column_int(res, 5);
    return board;
}

static struct db_dut_board* db_make_dut_board(sqlite3_stmt *res){
    struct db_dut_board *dut_board = NULL;
    if(res == NULL){
        die("pointer is null");
    }
    if((dut_board = (struct db_dut_board*)calloc(1, sizeof(struct db_dut_board))) == NULL){
        die("malloc failed");
    }
    dut_board->id = sqlite3_column_int64(res, 0);
    dut_board->dna = (const char *)sqlite3_column_text(res, 1);
    dut_board->name = (const char *)sqlite3_column_text(res, 2);
    dut_board->profile_path = (const char *)sqlite3_column_text(res, 3);
    return dut_board;
}

static struct db_job* db_make_job(sqlite3_stmt *res){
    struct db_job *job = NULL;
    if(res == NULL){
        die("pointer is null");
    }
    if((job = (struct db_job*)calloc(1, sizeof(struct db_job))) == NULL){
        die("malloc failed");
    }
    job->id = sqlite3_column_int64(res, 0);
    job->board_id = sqlite3_column_int64(res, 1);
    job->dut_board_id = sqlite3_column_int64(res, 2);
    job->user_id = sqlite3_column_int64(res, 3);
    job->state = (enum db_job_states)sqlite3_column_int64(res, 6);
    return job;
}

static struct db_prgm* db_make_prgm(sqlite3_stmt *res){
    struct db_prgm *prgm = NULL;
    if(res == NULL){
        die("pointer is null");
    }
    if((prgm = (struct db_prgm*)calloc(1, sizeof(struct db_prgm))) == NULL){
        die("malloc failed");
    }
    prgm->id = sqlite3_column_int64(res, 0);
    prgm->job_id = sqlite3_column_int64(res, 1);
    prgm->path = (const char *)sqlite3_column_text(res, 4);
    prgm->body = (const char *)sqlite3_column_text(res, 5);
    prgm->date_start = util_dt_to_epoch((char *)sqlite3_column_text(res, 6));
    prgm->date_end = util_dt_to_epoch((char *)sqlite3_column_text(res, 7));
    prgm->return_code = sqlite3_column_int(res, 8);
    prgm->error_msg = (const char *)sqlite3_column_text(res, 9);
    prgm->last_stim_id = sqlite3_column_int(res, 10);
    prgm->did_fail = sqlite3_column_int(res, 11);
    prgm->failing_vec = sqlite3_column_int(res, 12);
    prgm->state = (enum db_prgm_states)sqlite3_column_int(res, 13);
    return prgm;
}

static struct db_prgm_log* db_make_prgm_log(sqlite3_stmt *res){
    struct db_prgm_log *prgm_log = NULL;
    if(res == NULL){
        die("pointer is null");
    }
    if((prgm_log = (struct db_prgm_log*)calloc(1, sizeof(struct db_prgm_log))) == NULL){
        die("malloc failed");
    }
    prgm_log->id = sqlite3_column_int64(res, 0);
    prgm_log->prgm_id = sqlite3_column_int64(res, 1);
    prgm_log->date_created = util_dt_to_epoch((char *)sqlite3_column_text(res, 2));
    prgm_log->line = (const char *)sqlite3_column_text(res, 3);
    return prgm_log;
}

static struct db_stim* db_make_stim(sqlite3_stmt *res){
    struct db_stim *stim = NULL;
    if(res == NULL){
        die("pointer is null");
    }
    if((stim = (struct db_stim*)calloc(1, sizeof(struct db_stim))) == NULL){
        die("malloc failed");
    }
    stim->id = sqlite3_column_int64(res, 0);
    stim->prgm_id = sqlite3_column_int64(res, 1);
    stim->path = (const char *)sqlite3_column_text(res, 4);
    stim->did_fail = sqlite3_column_int(res, 5);
    stim->failing_vec = sqlite3_column_int64(res, 6);
    stim->state = (enum db_stim_states)sqlite3_column_int(res, 7);
    return stim;
}

static struct db_fail_pin* db_make_fail_pin(sqlite3_stmt *res){
    struct db_fail_pin *fail_pin = NULL;
    if(res == NULL){
        die("pointer is null");
    }
    if((fail_pin = (struct db_fail_pin*)calloc(1, sizeof(struct db_fail_pin))) == NULL){
        die("malloc failed");
    }
    fail_pin->id = sqlite3_column_int64(res, 0);
    fail_pin->stim_id = sqlite3_column_int64(res, 1);
    fail_pin->dut_io_id = sqlite3_column_int64(res, 2);
    fail_pin->did_fail = sqlite3_column_int(res, 3);
    return fail_pin;
}

static struct db_mount* db_make_mount(sqlite3_stmt *res){
    struct db_mount *mount = NULL;
    if(res == NULL){
        die("pointer is null");
    }
    if((mount = (struct db_mount*)calloc(1, sizeof(struct db_mount))) == NULL){
        die("malloc failed");
    }
    mount->id = sqlite3_column_int64(res, 0);
    mount->name = (const char *)sqlite3_column_text(res, 3);
    mount->ip_addr = (const char *)sqlite3_column_text(res, 4);
    mount->path = (const char *)sqlite3_column_text(res, 5);
    mount->point = (const char *)sqlite3_column_text(res, 6);
    mount->message = (const char *)sqlite3_column_text(res, 7);
    return mount;
}

void db_free_user(struct db_user *user){
    if(user == NULL){
        die("pointer is null");
    }
    free((char *)user->username);
    free((char *)user->password);
    free((char *)user->session);
    free(user);
    return;
}

void db_free_board(struct db_board *board){
    if(board == NULL){
        die("pointer is null");
    }
    free((char *)board->dna);
    free((char *)board->name);
    free((char *)board->ip_addr);
    free(board);
    return;
}

void db_free_dut_board(struct db_dut_board *dut_board){
    if(dut_board == NULL){
        die("pointer is null");
    }
    free((char *)dut_board->dna);
    free((char *)dut_board->name);
    free((char *)dut_board->profile_path);
    free(dut_board);
    return;
}

void db_free_job(struct db_job *job){
    if(job == NULL){
        die("pointer is null");
    }
    free(job);
    return;
}

void db_free_prgm(struct db_prgm *prgm){
    if(prgm == NULL){
        die("pointer is null");
    }
    free((char *)prgm->path);
    free((char *)prgm->body);
    free((char *)prgm->error_msg);
    free(prgm);
    return;
}

void db_free_stim(struct db_stim *stim){
    if(stim == NULL){
        die("pointer is null");
    }
    free((char *)stim->path);
    free(stim);
    return;
}

void db_free_fail_pin(struct db_fail_pin *fail_pin){
    if(fail_pin == NULL){
        die("pointer is null");
    }
    free(fail_pin);
    return;
}

void db_free_mount(struct db_mount *mount){
    if(mount == NULL){
        die("pointer is null");
    }
    free((char *)mount->name);
    free((char *)mount->ip_addr);
    free((char *)mount->path);
    free((char *)mount->point);
    free((char *)mount->message);
    free(mount);
    return;
}

int64_t db_insert_board(struct db *db, 
        char *dna, char *name, char *ip_addr, int32_t cur_dut_board_id, int32_t is_master){
    sqlite3_stmt *res = NULL;
    int rc = 0;

    if(db == NULL){
        die("pointer is null");
    }
    if(dna == NULL){
        die("pointer is null");
    }

    char *sql = "INSERT INTO boards(dna, name, ip_addr, cur_dut_board_id, is_master) VALUES(?, ?, ?, ?, ?)";

    rc = sqlite3_prepare_v2(db->_db, sql, -1, &res, 0);

    if(rc == SQLITE_OK){
        sqlite3_bind_text(res, 1, dna, strlen(dna), SQLITE_STATIC);
        sqlite3_bind_text(res, 2, name, strlen(name), SQLITE_STATIC);
        sqlite3_bind_text(res, 3, ip_addr, strlen(ip_addr), SQLITE_STATIC);
        sqlite3_bind_int64(res, 4, cur_dut_board_id);
        sqlite3_bind_int(res, 5, is_master);
    }else{
        die("Failed to execute statement: %s\n", sqlite3_errmsg(db->_db));
    }

    int step = sqlite3_step(res);

    if(step != SQLITE_DONE){
        die("failed to exec sql '%s'", sql);
    }

    sqlite3_finalize(res);
    return sqlite3_last_insert_rowid(db->_db);
}

int64_t db_update_board(struct db *db, struct db_board *board){
    sqlite3_stmt *res = NULL;
    int rc = 0;

    if(db == NULL){
        die("pointer is null");
    }
    if(board == NULL){
        die("pointer is null");
    }

    char *sql = "UPDATE boards SET dna=?, name=?, ip_addr=?, cur_dut_board_id=?, is_master=? WHERE id=?";

    rc = sqlite3_prepare_v2(db->_db, sql, -1, &res, 0);

    if(rc == SQLITE_OK){
        sqlite3_bind_text(res, 1, board->dna, strlen(board->dna), SQLITE_STATIC);
        sqlite3_bind_text(res, 2, board->name, strlen(board->name), SQLITE_STATIC);
        sqlite3_bind_text(res, 3, board->ip_addr, strlen(board->ip_addr), SQLITE_STATIC);
        sqlite3_bind_int64(res, 4, board->cur_dut_board_id);
        sqlite3_bind_int(res, 5, board->is_master);
        sqlite3_bind_int64(res, 6, board->id);
    } else {
        die("Failed to execute statement: %s\n", sqlite3_errmsg(db->_db));
    }

    int step = sqlite3_step(res);

    if (step != SQLITE_DONE) {
        die("failed to exec sql '%s'", sql);
    }

    sqlite3_finalize(res);

    return board->id;
}

struct db_board* db_get_board_by_id(struct db *db, int64_t board_id){
    struct db_board *board = NULL;
    sqlite3_stmt *res = NULL;
    int rc = 0;
    int step = 0;

    if(db == NULL){
        die("pointer is null");
    }

    char *sql = "SELECT * FROM boards WHERE id=? LIMIT 1";

    rc = sqlite3_prepare_v2(db->_db, sql, -1, &res, 0);

    if(rc == SQLITE_OK){
        sqlite3_bind_int64(res, 1, board_id);
    } else {
        fprintf(stderr, "Failed to execute statement: %s\n", sqlite3_errmsg(db->_db));
    }

    step = sqlite3_step(res);

    if (step == SQLITE_ROW) {
        board = db_make_board(res);
    }

    sqlite3_finalize(res);
    return board;
}

struct db_board* db_get_board_by_dna(struct db *db, char *dna){
    struct db_board *board = NULL;
    sqlite3_stmt *res = NULL;
    int rc = 0;

    if(db == NULL){
        die("pointer is null");
    }
    if(dna == NULL){
        die("pointer is null");
    }

    char *sql = "SELECT * FROM boards WHERE dna = ?";

    rc = sqlite3_prepare_v2(db->_db, sql, -1, &res, 0);

    if(rc == SQLITE_OK){
        sqlite3_bind_text(res, 1, dna, strlen(dna), SQLITE_STATIC);
    } else {
        fprintf(stderr, "Failed to execute statement: %s\n", sqlite3_errmsg(db->_db));
    }

    int step = sqlite3_step(res);

    if (step == SQLITE_ROW) {
        board = db_make_board(res);
    }

    sqlite3_finalize(res);
    return board;
}

int64_t db_update_job(struct db *db, struct db_job *job){
    sqlite3_stmt *res = NULL;
    int rc = 0;

    if(db == NULL){
        die("pointer is null");
    }

    if(job == NULL){
        die("pointer is null");
    }

    char *sql = "UPDATE jobs SET board_id=?, dut_board_id=?, user_id=?, state=? WHERE id=?";

    rc = sqlite3_prepare_v2(db->_db, sql, -1, &res, 0);

    if(rc == SQLITE_OK){
        sqlite3_bind_int64(res, 1, job->board_id);
        sqlite3_bind_int64(res, 2, job->dut_board_id);
        sqlite3_bind_int64(res, 3, job->user_id);
        sqlite3_bind_int(res, 4, job->state);
        sqlite3_bind_int64(res, 5, job->id);
    } else {
        die("Failed to execute statement: %s\n", sqlite3_errmsg(db->_db));
    }

    int step = sqlite3_step(res);

    if (step != SQLITE_DONE) {
        die("failed to exec sql '%s'", sql);
    }

    sqlite3_finalize(res);

    return job->id;
}

uint64_t db_get_num_jobs(struct db *db, 
        int64_t board_id, int64_t dut_board_id,
        int64_t user_id, int32_t states){
    sqlite3_stmt *res = NULL;
    int rc = 0;
    int step = 0;
    int64_t num = 0;

    if(db == NULL){
        die("pointer is null");
    }

    UT_string *sql;
    utstring_new(sql);
    utstring_printf(sql, "SELECT COUNT(*) FROM jobs WHERE board_id = ? ");

    if(board_id < 0){
        die("board_id must not be < 0");
    }

    if(dut_board_id != -1){
        utstring_printf(sql, "AND dut_board_id = %lli ", dut_board_id);
    }

    if(user_id != -1){
        utstring_printf(sql, "AND user_id != %lli ", user_id);
    }

    if((states & JOB_IDLE) == JOB_IDLE){
        utstring_printf(sql, "AND state = %i ", JOB_IDLE);
    }
    if((states & JOB_PENDING) == JOB_PENDING){
        utstring_printf(sql, "AND state = %i ", JOB_PENDING);
    }
    if((states & JOB_RUNNING) == JOB_RUNNING){
        utstring_printf(sql, "AND state = %i ", JOB_RUNNING);
    }
    if((states & JOB_KILLING) == JOB_KILLING){
        utstring_printf(sql, "AND state = %i ", JOB_KILLING);
    }
    if((states & JOB_KILLED) == JOB_KILLED){
        utstring_printf(sql, "AND state = %i ", JOB_KILLED);
    }
    if((states & JOB_DONE) == JOB_DONE){
        utstring_printf(sql, "AND state = %i ", JOB_DONE);
    }

    char *sql_str = utstring_body(sql);
    rc = sqlite3_prepare_v2(db->_db, sql_str, -1, &res, 0);

    if(rc == SQLITE_OK){
        sqlite3_bind_int64(res, 1, board_id);
    } else {
        fprintf(stderr, "Failed to execute statement: %s\n", sqlite3_errmsg(db->_db));
    }

    if((step = sqlite3_step(res)) == SQLITE_ROW) {
        num = sqlite3_column_int64(res, 0);
    }

    if(num < 0){
        die("count returned negative number");
    }

    utstring_free(sql);
    sqlite3_finalize(res);
    return (uint64_t)num;
}

struct db_job** db_get_jobs(struct db *db, 
        int64_t board_id, int64_t dut_board_id, 
        int64_t user_id, int32_t states){
    struct db_job *job = NULL;
    struct db_job **jobs = NULL;
    sqlite3_stmt *res = NULL;
    int rc = 0;
    int step = 0;

    if(db == NULL){
        die("pointer is null");
    }

    UT_string *sql;
    utstring_new(sql);
    utstring_printf(sql, "SELECT * FROM jobs WHERE board_id = ? ");

    if(board_id < 0){
        die("board_id must not be < 0");
    }

    if(dut_board_id != -1){
        utstring_printf(sql, "AND dut_board_id = %lli ", dut_board_id);
    }

    if(user_id != -1){
        utstring_printf(sql, "AND user_id != %lli ", user_id);
    }

    if((states & JOB_IDLE) == JOB_IDLE){
        utstring_printf(sql, "AND state = %i ", JOB_IDLE);
    }
    if((states & JOB_PENDING) == JOB_PENDING){
        utstring_printf(sql, "AND state = %i ", JOB_PENDING);
    }
    if((states & JOB_RUNNING) == JOB_RUNNING){
        utstring_printf(sql, "AND state = %i ", JOB_RUNNING);
    }
    if((states & JOB_KILLING) == JOB_KILLING){
        utstring_printf(sql, "AND state = %i ", JOB_KILLING);
    }
    if((states & JOB_KILLED) == JOB_KILLED){
        utstring_printf(sql, "AND state = %i ", JOB_KILLED);
    }
    if((states & JOB_DONE) == JOB_DONE){
        utstring_printf(sql, "AND state = %i ", JOB_DONE);
    }

    rc = sqlite3_prepare_v2(db->_db, utstring_body(sql), -1, &res, 0);

    if(rc == SQLITE_OK){
        sqlite3_bind_int64(res, 1, board_id);
    } else {
        fprintf(stderr, "Failed to execute statement: %s\n", sqlite3_errmsg(db->_db));
    }

    uint64_t num = db_get_num_jobs(db, board_id, dut_board_id, user_id, states);

    if(num > 0){
        if((jobs = (struct db_job**)calloc(num, sizeof(struct db_job*))) == NULL){
            die("malloc failed");
        }

        uint64_t i = 0;
        while((step = sqlite3_step(res)) == SQLITE_ROW) {
            job = db_make_job(res);
            jobs[i] = job;
            i += 1;
        }
    }

    utstring_free(sql);
    sqlite3_finalize(res);
    return jobs;
}

struct db_prgm* db_get_prgm_by_id(struct db *db, int64_t prgm_id){
    struct db_prgm *prgm = NULL;
    sqlite3_stmt *res = NULL;
    int rc = 0;
    int step = 0;

    if(db == NULL){
        die("pointer is null");
    }

    char *sql = "SELECT * FROM prgms WHERE id=? LIMIT 1";

    rc = sqlite3_prepare_v2(db->_db, sql, -1, &res, 0);

    if(rc == SQLITE_OK){
        sqlite3_bind_int64(res, 1, prgm_id);
    } else {
        fprintf(stderr, "Failed to execute statement: %s\n", sqlite3_errmsg(db->_db));
    }

    step = sqlite3_step(res);

    if (step == SQLITE_ROW) {
        prgm = db_make_prgm(res);
    }

    sqlite3_finalize(res);
    return prgm;
}

int64_t db_update_prgm(struct db *db, struct db_prgm *prgm){
    sqlite3_stmt *res = NULL;
    int rc = 0;

    if(db == NULL){
        die("pointer is null");
    }

    if(prgm == NULL){
        die("pointer is null");
    }

    char *sql = "UPDATE prgms SET job_id=?, date_start=?, date_end=?, path=?, body=?, return_code=?, error_msg=?, last_stim_id=?, did_fail=?, failing_vec=?, state=? WHERE id=?";

    rc = sqlite3_prepare_v2(db->_db, sql, -1, &res, 0);

    char *date_start = util_epoch_to_dt(prgm->date_start);
    char *date_end = util_epoch_to_dt(prgm->date_end);

    if(rc == SQLITE_OK){
        sqlite3_bind_int64(res, 1, prgm->job_id);
        sqlite3_bind_text(res, 2, date_start, strlen(date_start), SQLITE_STATIC);
        sqlite3_bind_text(res, 3, date_end, strlen(date_end), SQLITE_STATIC);
        sqlite3_bind_text(res, 4, prgm->path, strlen(prgm->path), SQLITE_STATIC);
        sqlite3_bind_text(res, 5, prgm->body, strlen(prgm->body), SQLITE_STATIC);
        sqlite3_bind_int(res, 6, prgm->return_code);
        sqlite3_bind_text(res, 7, prgm->error_msg, strlen(prgm->error_msg), SQLITE_STATIC);
        sqlite3_bind_int64(res, 8, prgm->last_stim_id);
        sqlite3_bind_int(res, 9, prgm->did_fail);
        sqlite3_bind_int64(res, 10, prgm->failing_vec);
        sqlite3_bind_int(res, 11, prgm->state);
        sqlite3_bind_int64(res, 12, prgm->id);
    } else {
        die("Failed to execute statement: %s\n", sqlite3_errmsg(db->_db));
    }

    int step = sqlite3_step(res);

    if (step != SQLITE_DONE) {
        die("failed to exec sql '%s'", sql);
    }

    sqlite3_finalize(res);

    return prgm->id;
}

uint64_t db_get_num_prgms(struct db *db, 
        int64_t job_id, const char *path, const char *body, 
        int32_t return_code, const char *error_msg, int32_t last_stim_id, 
        int32_t did_fail, int32_t failing_vec, int32_t states){
    sqlite3_stmt *res = NULL;
    int rc = 0;
    int step = 0;
    int64_t num = 0;

    if(db == NULL){
        die("pointer is null");
    }

    UT_string *sql;
    utstring_new(sql);
    utstring_printf(sql, "SELECT COUNT(*) FROM prgms WHERE job_id = ? ");

    if(job_id < 0){
        die("job_id must not be < 0");
    }

    if(path != NULL){
        utstring_printf(sql, "AND path = '%s' ", path);
    }

    if(body != NULL){
        utstring_printf(sql, "AND body = '%s' ", body);
    }

    if(return_code != -1){
        utstring_printf(sql, "AND return_code = %i ", return_code);
    }

    if(error_msg != NULL){
        utstring_printf(sql, "AND error_msg = '%s' ", error_msg);
    }

    if(last_stim_id != -1){
        utstring_printf(sql, "AND last_stim_id = %i ", last_stim_id);
    }

    if(did_fail != -1){
        utstring_printf(sql, "AND did_fail = %i ", did_fail);
    }

    if(failing_vec != -1){
        utstring_printf(sql, "AND failing_vec = %i ", failing_vec);
    }

    if((states & PRGM_IDLE) == PRGM_IDLE){
        utstring_printf(sql, "AND state = %i ", PRGM_IDLE);
    }
    if((states & PRGM_PENDING) == PRGM_PENDING){
        utstring_printf(sql, "AND state = %i ", PRGM_PENDING);
    }
    if((states & PRGM_RUNNING) == PRGM_RUNNING){
        utstring_printf(sql, "AND state = %i ", PRGM_RUNNING);
    }
    if((states & PRGM_KILLING) == PRGM_KILLING){
        utstring_printf(sql, "AND state = %i ", PRGM_KILLING);
    }
    if((states & PRGM_KILLED) == PRGM_KILLED){
        utstring_printf(sql, "AND state = %i ", PRGM_KILLED);
    }
    if((states & PRGM_DONE) == PRGM_DONE){
        utstring_printf(sql, "AND state = %i ", PRGM_DONE);
    }

    rc = sqlite3_prepare_v2(db->_db, utstring_body(sql), -1, &res, 0);

    if(rc == SQLITE_OK){
        sqlite3_bind_int64(res, 1, job_id);
    } else {
        fprintf(stderr, "Failed to execute statement: %s\n", sqlite3_errmsg(db->_db));
    }

    if((step = sqlite3_step(res)) == SQLITE_ROW) {
        num = sqlite3_column_int64(res, 0);
    }

    if(num < 0){
        die("count returned negative number");
    }

    utstring_free(sql);
    sqlite3_finalize(res);
    return (uint64_t)num;
}

struct db_prgm** db_get_prgms(struct db *db, 
        int64_t job_id, const char *path, const char *body,
        int32_t return_code, const char *error_msg, int32_t last_stim_id,
        int32_t did_fail, int32_t failing_vec, int32_t states){
    struct db_prgm *prgm = NULL;
    struct db_prgm **prgms = NULL;
    sqlite3_stmt *res = NULL;
    int rc = 0;
    int step = 0;

    if(db == NULL){
        die("pointer is null");
    }

    UT_string *sql;
    utstring_new(sql);
    utstring_printf(sql, "SELECT * FROM prgms WHERE job_id = ? ");

    if(job_id < 0){
        die("job_id must not be < 0");
    }

    if(path != NULL){
        utstring_printf(sql, "AND path = '%s' ", path);
    }

    if(body != NULL){
        utstring_printf(sql, "AND body = '%s' ", body);
    }

    if(return_code != -1){
        utstring_printf(sql, "AND return_code = %i ", return_code);
    }

    if(error_msg != NULL){
        utstring_printf(sql, "AND error_msg = '%s' ", error_msg);
    }

    if(last_stim_id != -1){
        utstring_printf(sql, "AND last_stim_id = %i ", last_stim_id);
    }

    if(did_fail != -1){
        utstring_printf(sql, "AND did_fail = %i ", did_fail);
    }

    if(failing_vec != -1){
        utstring_printf(sql, "AND failing_vec = %i ", failing_vec);
    }

    if((states & PRGM_IDLE) == PRGM_IDLE){
        utstring_printf(sql, "AND state = %i ", PRGM_IDLE);
    }
    if((states & PRGM_PENDING) == PRGM_PENDING){
        utstring_printf(sql, "AND state = %i ", PRGM_PENDING);
    }
    if((states & PRGM_RUNNING) == PRGM_RUNNING){
        utstring_printf(sql, "AND state = %i ", PRGM_RUNNING);
    }
    if((states & PRGM_KILLING) == PRGM_KILLING){
        utstring_printf(sql, "AND state = %i ", PRGM_KILLING);
    }
    if((states & PRGM_KILLED) == PRGM_KILLED){
        utstring_printf(sql, "AND state = %i ", PRGM_KILLED);
    }
    if((states & PRGM_DONE) == PRGM_DONE){
        utstring_printf(sql, "AND state = %i ", PRGM_DONE);
    }

    rc = sqlite3_prepare_v2(db->_db, utstring_body(sql), -1, &res, 0);

    if(rc == SQLITE_OK){
        sqlite3_bind_int64(res, 1, job_id);
    } else {
        fprintf(stderr, "Failed to execute statement: %s\n", sqlite3_errmsg(db->_db));
    }

    uint64_t num = db_get_num_prgms(db, job_id, path, body, return_code, error_msg,
            last_stim_id, did_fail, failing_vec, states);

    if(num > 0){
        if((prgms = (struct db_prgm**)calloc(num, sizeof(struct db_prgm*))) == NULL){
            die("malloc failed");
        }

        uint64_t i = 0;
        while((step = sqlite3_step(res)) == SQLITE_ROW) {
            prgm = db_make_prgm(res);
            prgms[i] = prgm;
            i += 1;
        }
    }

    utstring_free(sql);
    sqlite3_finalize(res);
    return prgms;
}

int64_t db_insert_prgm_log(struct db *db, 
        int64_t prgm_id, char *line){
    sqlite3_stmt *res = NULL;
    int rc = 0;

    if(db == NULL){
        die("pointer is null");
    }
    if(line == NULL){
        die("pointer is null");
    }

    char *sql = "INSERT INTO prgm_logs(prgm_id, date_created, line) VALUES(?, datetime('now'), ?)";

    rc = sqlite3_prepare_v2(db->_db, sql, -1, &res, 0);

    if(rc == SQLITE_OK){
        sqlite3_bind_int64(res, 1, prgm_id);
        sqlite3_bind_text(res, 2, line, strlen(line), SQLITE_STATIC);
    }else{
        die("Failed to execute statement: %s\n", sqlite3_errmsg(db->_db));
    }

    int step = sqlite3_step(res);

    if(step != SQLITE_DONE){
        die("failed to exec sql '%s'", sql);
    }

    sqlite3_finalize(res);
    return sqlite3_last_insert_rowid(db->_db);
}

int64_t db_insert_stim(struct db *db, 
        int64_t prgm_id, char *path, int32_t did_fail, 
        int64_t failing_vec, enum db_stim_states state){
    sqlite3_stmt *res = NULL;
    int rc = 0;

    if(db == NULL){
        die("pointer is null");
    }

    if(path == NULL){
        die("pointer is null");
    }

    char *sql = "INSERT INTO stims(prgm_id, date_created, path, did_fail, failing_vec, state) VALUES(?, datetime('now'), ?, ?, ?, ?)";

    rc = sqlite3_prepare_v2(db->_db, sql, -1, &res, 0);

    if(rc == SQLITE_OK){
        sqlite3_bind_int64(res, 1, prgm_id);
        sqlite3_bind_text(res, 2, path, strlen(path), SQLITE_STATIC);
        sqlite3_bind_int(res, 3, did_fail);
        sqlite3_bind_int64(res, 4, failing_vec);
        sqlite3_bind_int(res, 5, (int32_t)state);
    }else{
        die("Failed to execute statement: %s\n", sqlite3_errmsg(db->_db));
    }

    int step = sqlite3_step(res);

    if(step != SQLITE_DONE){
        die("failed to exec sql '%s'", sql);
    }

    sqlite3_finalize(res);
    return sqlite3_last_insert_rowid(db->_db);
}

int64_t db_update_stim(struct db *db, struct db_stim *stim){
    sqlite3_stmt *res = NULL;
    int rc = 0;

    if(db == NULL){
        die("pointer is null");
    }

    if(stim == NULL){
        die("pointer is null");
    }

    char *sql = "UPDATE stims SET prgm_id=?, path=?, did_fail=?, failing_vec=?, state=? WHERE id=?";

    rc = sqlite3_prepare_v2(db->_db, sql, -1, &res, 0);

    if(rc == SQLITE_OK){
        sqlite3_bind_int64(res, 1, stim->prgm_id);
        sqlite3_bind_text(res, 2, stim->path, strlen(stim->path), SQLITE_STATIC);
        sqlite3_bind_int(res, 3, stim->did_fail);
        sqlite3_bind_int64(res, 4, stim->failing_vec);
        sqlite3_bind_int(res, 5, stim->state);
        sqlite3_bind_int64(res, 6, stim->id);
    } else {
        die("Failed to execute statement: %s\n", sqlite3_errmsg(db->_db));
    }

    int step = sqlite3_step(res);

    if (step != SQLITE_DONE) {
        die("failed to exec sql '%s'", sql);
    }

    sqlite3_finalize(res);

    return stim->id;
}

struct db_stim* db_get_stim_by_id(struct db *db, int64_t stim_id){
    struct db_stim *stim = NULL;
    sqlite3_stmt *res = NULL;
    int rc = 0;
    int step = 0;

    if(db == NULL){
        die("pointer is null");
    }

    char *sql = "SELECT * FROM stims WHERE id=? LIMIT 1";

    rc = sqlite3_prepare_v2(db->_db, sql, -1, &res, 0);

    if(rc == SQLITE_OK){
        sqlite3_bind_int64(res, 1, stim_id);
    } else {
        fprintf(stderr, "Failed to execute statement: %s\n", sqlite3_errmsg(db->_db));
    }

    step = sqlite3_step(res);

    if (step == SQLITE_ROW) {
        stim = db_make_stim(res);
    }

    sqlite3_finalize(res);
    return stim;
}



