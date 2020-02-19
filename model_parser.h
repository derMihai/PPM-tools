/* This file is part of the 'PPM tools' PPM compression library.
 * Copyright (C) 2020  Mihai Renea
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 * */

#ifndef MODEL_PARSER_H_
#define MODEL_PARSER_H_

#include <stdio.h>
#include "TaskSegRaw.h"

typedef enum {
    MP_ok,
    MP_mem,
    MP_err
} MPRes;

typedef unsigned long TaskNo;

typedef enum {
    MPTT_start,
    MPTT_end,
    MPTT_fork,
    MPTT_join,
    MPTT_calc,
    MPTT_com,
    MPTT_forkend  = 10,

    MPTT__enumsize
} MPTaskType;

typedef struct {
    struct {
        int             pno;
        MPTaskType      ttype;
        unsigned long   mem;
    };                          /* common */
    double          req;        /* calc, com */
    unsigned long   dest;       /* com */
    TaskNo          next[2];    /* [0] common except end, [1] fork */
    /* for debugging */
    unsigned long lno;
} MPTask;

typedef struct {
    FILE            *src;
    MPTask          *task_l;
    unsigned long   task_llen;
    TaskNo          head;
    TaskNo          cti;        /*  */
    double          cap_val_com;
    double          cap_val_cal;
} MParser;


MPRes MParser_init(
    MParser *ctx,
    FILE *src,
    double cap_val_com,
    double cap_val_cal);
MPRes   MParser_parse(MParser *ctx);
MPRes   MParser_deinit(MParser *ctx);
#endif
