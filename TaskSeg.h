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

#ifndef TASKSEG_H_
#define TASKSEG_H_

/* Description:
 *
 * This is an abstract class to build task segment subclasses upon. It inherits
 * the Elem class to add tracking. */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "element_context.h"

enum {
    TSTT_calc,
    TSTT_com,

    TSTT_enumsize
};

typedef unsigned char TSTaskType;

typedef struct TaskSeg_VMT TaskSeg_VMT;
typedef struct TaskSeg TaskSeg;
typedef struct TaskSegCtx TaskSegCtx;
typedef struct TaskSegCtx_VMT TaskSegCtx_VMT;

typedef struct {
    double *reql[TSTT_enumsize];
    unsigned reql_siz[TSTT_enumsize];
} TaskSeg_reql;

/* note: task deviation is the difference between compressed and uncompressed
 * task weight. */
typedef struct {
    /* sum of the individual task deviations */
    double devi_sum[TSTT_enumsize];
    /* average task deviation */
    double devi_mean[TSTT_enumsize];
    /* dictionary size */
    unsigned dict_size[TSTT_enumsize];
    /* total requirement */
    double sum[TSTT_enumsize];
    /* average requirement */
    double avg[TSTT_enumsize];
} TaskSeg_summary;

struct TaskSeg_VMT {
    Elem_VMT _super;
    int     (*TaskSeg_compar)(TaskSeg*, TaskSeg*);
    void    (*TaskSeg_print)(TaskSeg*);
    void    (*TaskSeg_export)(TaskSeg*, FILE*, TSTaskType tt);
    TaskSeg_reql* (*TaskSeg_to_reql)(TaskSeg*, char);
    TaskSeg_summary (*TaskSeg_eval)(TaskSeg*);
};

struct TaskSeg {
    Elem _super;
};

struct TaskSegCtx {
    union {
        ElemCtx _super;
        const TaskSegCtx_VMT *vmt;
    };
    char tsclass_id;
};

struct TaskSegCtx_VMT {
    ElemCtx_VMT _super;
};

extern const TaskSeg_VMT TaskSeg_vmt;
extern const TaskSegCtx_VMT TaskSegCtx_vmt;

/* *** INHERITABLE METHODS WRAPPERS BEGIN *** */
/* Compares two TaskSeg pointers sharing the same subclass.
 * @param ts1p pointer to a TaskSeg object
 * @param ts2p pointer to a TaskSeg object sharing the same subclass as ts1p
 * @return 1 if equal, 0 otherwise */
static inline int TaskSeg_compar(TaskSeg *ts1p, TaskSeg *ts2p)
{
    assert(ts1p && ts2p);
    /* ensure they share the same class */
    assert(_obj_vmtp(ts1p) == _obj_vmtp(ts2p));

    return ((TaskSeg_VMT*)_obj_vmtp(ts1p))->TaskSeg_compar(ts1p, ts2p);
}
/* For debugging. Prints a segment on stdout.
 * @tsp pointer to a TaskSeg object */
static inline void TaskSeg_print(TaskSeg *tsp)
{
    assert(tsp);

    ((TaskSeg_VMT*)_obj_vmtp(tsp))->TaskSeg_print(tsp);
}

/* For debugging. Prints segment in a data file for gnuplot.
 * @tsp pointer to a TaskSeg object
 * @fp FILE pointer
 * @tt Task type filter. May be TSTT_calc or TSTT_com*/
static inline void TaskSeg_export(TaskSeg *tsp, FILE *fp, TSTaskType tt)
{
    assert(tsp && fp);
    assert((unsigned)tt < TSTT_enumsize);
    ((TaskSeg_VMT*)_obj_vmtp(tsp))->TaskSeg_export(tsp, fp, tt);
}

/* Get the (sorted) requirement lists of a TaskSeg object.
 * @param tsp pointer to a TaskSeg object
 * @param sort set to 1 to also sort the lists.
 * @return pointer to a TaskSeg_reql object */
static inline TaskSeg_reql* TaskSeg_to_reql(TaskSeg *tsp, char sort)
{
    assert(tsp);
    return ((TaskSeg_VMT*)_obj_vmtp(tsp))->TaskSeg_to_reql(tsp, sort);
}

/* Destroy a requirement lists object previously returned by a call to
 * TaskSeg_to_reql()
 * @param reql pointer to a TaskSeg_reql object */
static inline void TaskSeg_reql_destroy(TaskSeg_reql *reql)
{
    if (reql) {
        for (int i = 0; i < TSTT_enumsize; i++)
            free(reql->reql[i]);

        free(reql);
    }
}

/* Evaluates a TaskSeg object.
 * @param tsp pointer to a TaskSeg object
 * @return a TaskSeg_summary struct containing the summary info of the segment */
static inline TaskSeg_summary TaskSeg_eval(TaskSeg *tsp)
{
    assert(tsp);
    return ((TaskSeg_VMT*)_obj_vmtp(tsp))->TaskSeg_eval(tsp);
}
/* *** INHERITABLE METHODS WRAPPERS END *** */

/* *** NOT INHERITABLE METHODS BEGIN *** */
int TaskSeg_init(TaskSegCtx *ctx, TaskSeg *segp);
int TaskSegCtx_init(TaskSegCtx *ctx);
/* *** NOT INHERITABLE METHODS END *** */

/* *** INHERITABLE METHODS IMPLEMENTATION DECLARATION BEGIN *** */
/* All the function below are the implementations of the methods in the VMT.
 * They may/must be called ONLY in the context of the corresponding overridden
 * methods of a class DIRECTLY inheriting this class. These functions MUST NOT
 * be called otherwise. Use the inheritable wrappers defined above instead. */

void _TaskSeg_deinit(Object* objp);
void _TaskSegCtx_deinit(Object* objp);
/* *** INHERITABLE METHODS IMPLEMENTATION DECLARATION END *** */

#endif /* TASKSEG_H_ */
