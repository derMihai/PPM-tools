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

#ifndef TASKSEGRAW_H_
#define TASKSEGRAW_H_
/*
 * This class is NOT inheritable.
 */
#include <stdint.h>
#include "TaskSeg.h"

#define TSR_MALLOC_CNT     16
#define TSR_CLASSID 0x1

typedef enum {
    TSR_ok,
    TSR_mem,
    TSR_err
} TSRRes;

typedef struct {
    double       req;
    TSTaskType type;
} TSRTask;

typedef struct TReql {
    double      *req_l;
    unsigned    req_l_siz;
    unsigned    task_cnt;
    unsigned    task_curr;
    double      avg;
    double      stddev;
    double      sum;
} TReql;

typedef struct {
    TaskSeg             _super;
    TSTaskType          *task_type_l;
    TReql               treq_l[TSTT_enumsize];
    TSRTask             ct;
} TaskSegRaw;

typedef struct {
    double mu_max;
    double sigma_max;
} TSR_compopt;

typedef struct {
    TaskSegCtx  _super;
    TSR_compopt compopt;
} TaskSegRawCtx;

extern const TaskSeg_VMT TaskSegRaw_vmt;

int             TaskSegRaw_init(TaskSegRawCtx *ctx, TaskSegRaw *tsrp);
int             TaskSegRawCtx_init(TaskSegRawCtx *ctx, double mu_max, double sigma_max);
TSRRes          TSR_put(TaskSegRaw *tsrp, TSRTask taskp);
const TSRTask*  TSR_next(TaskSegRaw *tsrp);
void            TSR_rewind(TaskSegRaw *tsrp);
unsigned        TSR_size(TaskSegRaw *tsrp, TSTaskType filter);
void            TSR_eval(TaskSegRaw *tsrp);
TSRRes          TSR_merge(TaskSegRaw *restrict tsegp1, TaskSegRaw *restrict tsegp2);
double          TaskSegRaw_ctx_seg_meanlen(TaskSegRawCtx *ctx);
int             TaskSegRawCtx_to_file(TaskSegRawCtx *ctx, FILE *wfp);

#endif /* TASKSEGRAW_H_ */
