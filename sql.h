/*
 * SQL
 *
 * Copyright (c) 2015-2022 Gemini Complex Corporation. All rights reserved.
 *
 */
#ifndef SQL_H
#define SQL_H

#ifdef __cplusplus
extern "C" {
#endif


const char *DB_SQL = ""
    "    PRAGMA journal_mode=WAL;"
    "    CREATE TABLE IF NOT EXISTS users (\n"
    "        id INTEGER PRIMARY KEY,\n"
    "        date_created DATETIME CURRENT_TIMESTAMP,\n"
    "        date_updated DATETIME,\n"
    "        username TEXT,\n"
    "        password TEXT,\n"
    "        session TEXT,\n"
    "        is_admin INTEGER\n"
    "    );\n"
    "    -- daemon will probe network for boards and update\n"
    "    -- properties every 5 seconds\n"
    "    --\n"
    "    -- dna : unique board identifier\n"
    "    CREATE TABLE IF NOT EXISTS boards (\n"
    "        id INTEGER PRIMARY KEY,\n"
    "        dna TEXT,\n"
    "        name TEXT,\n"
    "        ip_addr TEXT,\n"
    "        cur_dut_board_id INTEGER,\n"
    "        is_master INTEGER\n"
    "    );\n"
    "    -- dna : unique board identifier\n"
    "    CREATE TABLE IF NOT EXISTS dut_boards (\n"
    "        id INTEGER PRIMARY KEY,\n"
    "        dna TEXT,\n"
    "        name TEXT,\n"
    "        profile_path TEXT\n"
    "    );\n"
    "    -- states : IDLE, PENDING, RUNNING, KILLING, KILLED, DONE\n"
    "    CREATE TABLE IF NOT EXISTS jobs (\n"
    "        id INTEGER PRIMARY KEY,\n"
    "        board_id INTEGER,\n"
    "        dut_board_id INTEGER,\n"
    "        user_id INTEGER,\n"
    "        date_created DATETIME CURRENT_TIMESTAMP,\n"
    "        date_updated DATETIME,\n"
    "        state INTEGER\n"
    "    );\n"
    "    -- state: IDLE, PENDING, RUNNING, KILLING, KILLED, DONE\n"
    "    CREATE TABLE IF NOT EXISTS prgms (\n"
    "        id INTEGER PRIMARY KEY,\n"
    "        job_id INTEGER,\n"
    "        date_created DATETIME CURRENT_TIMESTAMP,\n"
    "        date_updated DATETIME,\n"
    "        date_start DATETIME,\n"
    "        date_end DATETIME,\n"
    "        path TEXT,\n"
    "        body TEXT,\n"
    "        return_code INTEGER,\n"
    "        error_msg TEXT,\n"
    "        last_stim_id INTEGER,\n"
    "        did_fail INTEGER,\n"
    "        failing_vec INTEGER,\n"
    "        state INTEGER\n"
    "    );\n"
    "   CREATE TABLE IF NOT EXISTS prgm_logs (\n"
    "        id INTEGER PRIMARY KEY,\n"
    "        prgm_id INTEGER,\n"
    "        date_created DATETIME CURRENT_TIMESTAMP,\n"
    "        line TEXT\n"
    "    );\n"
    "    -- state: IDLE, PENDING, RUNNING, KILLED, DONE\n"
    "    CREATE TABLE IF NOT EXISTS stims (\n"
    "        id INTEGER PRIMARY KEY,\n"
    "        prgm_id INTEGER,\n"
    "        date_created DATETIME CURRENT_TIMESTAMP,\n"
    "        date_updated DATETIME,\n"
    "        path TEXT,\n"
    "        did_fail INTEGER,\n"
    "        failing_vec INTEGER,\n"
    "        state INTEGER\n"
    "    );\n"
    "    -- if a stim fails, these are the fail pins at given failing_vec\n"
    "    CREATE TABLE IF NOT EXISTS fail_pins (\n"
    "        id INTEGER PRIMARY KEY,\n"
    "        stim_id INTEGER,\n"
    "        dut_io_id INTEGER,\n"
    "        did_fail INTEGER\n"
    "    );\n"
    "    CREATE TABLE IF NOT EXISTS mounts (\n"
    "        id INTEGER PRIMARY KEY,\n"
    "        date_created DATETIME CURRENT_TIMESTAMP,\n"
    "        date_updated DATETIME,\n"
    "        name TEXT,\n"
    "        ip_addr TEXT,\n"
    "        path TEXT,\n"
    "        point TEXT,\n"
    "        message TEXT\n"
    "    );\n";



#ifdef __cplusplus
}
#endif
#endif
