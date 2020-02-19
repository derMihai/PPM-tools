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

#ifndef TASKSEGBUCK_H_
#define TASKSEGBUCK_H_
/*
 * This class is NOT inheritable.
 */
#include "TaskSeg.h"
#include "task_classifier.h"
#include "element_context.h"
#include "arll.h"

#define TSB_CLASSID 0x2

typedef enum {
    TSB_ok,
    TSB_mem,
    TSB_err
} TSBRes;

typedef union {
    struct {
        uint16_t    ttype  : 1;
        uint16_t    idx    : 15;
    };
    uint16_t        as_short;
} TCLetter;

typedef struct {
    TaskSeg         _super;
    const TCDict    *dictp[TSTT_enumsize];
    arll            *seg;
    unsigned        task_cnt[TSTT_enumsize];
    /* for evaluation */
//    TaskSegRaw      *orig_seg;
    TaskSeg_summary summary;
} TaskSegBuck;

typedef struct TaskSegBuckCtx {
    TaskSegCtx _super;
} TaskSegBuckCtx;

int TSB_init(
    TaskSegBuckCtx *ctx,
    TaskSegBuck *tsbp,
    const TCDict *calc_dictp,
    const TCDict *com_dictp,
    TaskSegRaw *tsr_src);
int TaskSegBuckCtx_init(TaskSegBuckCtx *ctx);
int TaskSegBuckCtx_to_file(TaskSegBuckCtx *ctx, FILE *wfp, TCDictCtx *dictctx);

#endif /* TASKSEGBUCK_H_ */
