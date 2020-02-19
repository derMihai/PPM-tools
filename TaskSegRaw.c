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

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <gsl/gsl_statistics_double.h>
#include "TaskSegRaw.h"

static const TaskSegRaw   tasksegraw_zero     = { 0 };
static const TaskSegRawCtx tasksegrawctx_zero = { 0 };
static const TaskSeg_reql tasksegreql_zero = { 0 };

static int TaskSegRaw_compar(TaskSeg *ts1p, TaskSeg *ts2p);
static void TaskSegRaw_print(TaskSeg *tsp);
static void TaskSegRaw_deinit(Object *objp);
static void TaskSegRaw_export(TaskSeg *tsp, FILE *fp, TSTaskType tt);
static TaskSeg_reql* TaskSegRaw_to_reql(TaskSeg *tsp, char sort);
static TaskSeg_summary TaskSegRaw_eval(TaskSeg *tsp);

const TaskSeg_VMT TaskSegRaw_vmt = {
    ._super._super._object_deinit = TaskSegRaw_deinit,

    .TaskSeg_compar = TaskSegRaw_compar,
    .TaskSeg_print = TaskSegRaw_print,
    .TaskSeg_export = TaskSegRaw_export,
    .TaskSeg_to_reql = TaskSegRaw_to_reql,
    .TaskSeg_eval = TaskSegRaw_eval,
};

const TaskSegCtx_VMT TaskSegRawCtx_vmt = {
    ._super._super._object_deinit = _TaskSegCtx_deinit,
};

typedef struct __attribute__((__packed__)) {
    uint8_t type;
    double weight;
} Task_pckd;

typedef struct __attribute__((__packed__)) {
    uint8_t class_id;
    uint32_t size;
} TaskSegRawCtx_pckd;

typedef struct __attribute__((__packed__)) {
    uint32_t size;
} TaskSegRaw_pckd;

static inline unsigned task_cnt_tot(const TaskSegRaw *ctx)
{
    unsigned ac = 0;
    for (unsigned i = 0; i < TSTT_enumsize; i++) ac += ctx->treq_l[i].task_cnt;
    return ac;
}

static inline unsigned task_curr_tot(const TaskSegRaw *ctx)
{
    unsigned ac = 0;
    for (unsigned i = 0; i < TSTT_enumsize; i++) ac += ctx->treq_l[i].task_curr;
    return ac;
}

static inline unsigned task_lsiz_tot(const TaskSegRaw *ctx)
{
    unsigned ac = 0;
    for (unsigned i = 0; i < TSTT_enumsize; i++) ac += ctx->treq_l[i].req_l_siz;
    return ac;
}


static int sort_cf(const void *a, const void *b)
{
    double diff = *(double*)a - *(double*)b;
    if      (diff > 0.0) return 1;
    else if (diff < 0.0) return -1;
    return 0;
}

static inline TSR_compopt _get_compopt(TaskSegRaw *tsrp)
{
    return ((TaskSegRawCtx*)((Elem*)tsrp)->ctxp)->compopt;
}

static int TaskSegRaw_compar(TaskSeg *ts1p, TaskSeg *ts2p)
{
    assert(_obj_vmtp(ts1p) == (Object_VMT*)&TaskSegRaw_vmt);
    TaskSegRaw *tsr1p = (TaskSegRaw*)ts1p;
    TaskSegRaw *tsr2p = (TaskSegRaw*)ts2p;

    TSR_compopt tsropt = _get_compopt(tsr1p);

    for (int i = 0; i < TSTT_enumsize; i++) {
        double fmin, fmax;
        if (tsr1p->treq_l[i].task_cnt != tsr2p->treq_l[i].task_cnt) return 0;
        if (tsr1p->treq_l[i].task_cnt == 0) continue; /* empty */

        if (tsr1p->treq_l[i].avg > tsr2p->treq_l[i].avg) {
            fmax = tsr1p->treq_l[i].avg;
            fmin = tsr2p->treq_l[i].avg;
        } else {
            fmax = tsr2p->treq_l[i].avg;
            fmin = tsr1p->treq_l[i].avg;
        }
        if (fmin == 0.0f) return fmin == fmax;
        if (fmax / fmin > tsropt.mu_max) return 0;

        if (tsr1p->treq_l[i].stddev > tsr2p->treq_l[i].stddev) {
            fmax = tsr1p->treq_l[i].stddev;
            fmin = tsr2p->treq_l[i].stddev;
        } else {
            fmax = tsr2p->treq_l[i].stddev;
            fmin = tsr1p->treq_l[i].stddev;
        }
        if (fmin == 0.0f) return fmin == fmax;
        if (fmax / fmin > tsropt.sigma_max) return 0;
    }

    if (memcmp(tsr1p->task_type_l, tsr2p->task_type_l, task_cnt_tot(tsr1p)))
        return 0;

    return 1;
}

static void TaskSegRaw_print(TaskSeg *tsp) {
    static const char *ttype_c[TSTT_enumsize] = {"cl", "cm"};

    assert(_obj_vmtp(tsp) == (Object_VMT*)&TaskSegRaw_vmt);
    TaskSegRaw *tsrp = (TaskSegRaw*)tsp;

    TSR_rewind(tsrp);

    printf("len=%u\t{",
        TSR_size(tsrp, TSTT_calc) + TSR_size(tsrp, TSTT_com));

    const TSRTask *ctp;
    while((ctp = TSR_next(tsrp))) {
        printf(",%s=%f", ttype_c[ctp->type], ctp->req);
    }
    printf("}\n");

    TSR_rewind(tsrp);
}

static void TaskSegRaw_deinit(Object *objp)
{
    TaskSegRaw *tsrp = (TaskSegRaw*)objp;

    if (tsrp) {
        for (int i = 0; i < TSTT_enumsize; i++) {
            free(tsrp->treq_l[i].req_l);
            tsrp->treq_l[i].req_l = NULL;
        }

        free(tsrp->task_type_l);
        tsrp->task_type_l = NULL;
    }

    _TaskSeg_deinit(objp);
}

static void TaskSegRaw_export(TaskSeg *tsp, FILE *fp, TSTaskType tt)
{
    assert(_obj_vmtp(tsp) == (Object_VMT*)&TaskSegRaw_vmt);
    TaskSegRaw *tsrp = (TaskSegRaw*)tsp;

    TSR_rewind(tsrp);
    const TSRTask *ctp;
    while((ctp = TSR_next(tsrp))) {
        if (ctp->type != tt) continue;
        assert (fprintf(fp, "%f\n", ctp->req) > 0);
    }
    fflush(fp);
}

static TaskSeg_reql* TaskSegRaw_to_reql(TaskSeg *tsp, char sort)
{
    assert(tsp);
    TaskSegRaw *tsrp = (TaskSegRaw*)tsp;

    TReql *creqp;
    TaskSeg_reql *reqp = malloc(sizeof(*reqp));
    if (!reqp) goto tsr_to_reql_err;
    *reqp = tasksegreql_zero;

    for (unsigned  i = 0; i < TSTT_enumsize; i++) {
        creqp = &tsrp->treq_l[i];

        reqp->reql[i] = malloc(creqp->task_cnt * sizeof(*creqp->req_l));
        if (!reqp->reql[i]) goto tsr_to_reql_err;
        reqp->reql_siz[i] = creqp->task_cnt;
        memcpy(reqp->reql[i], creqp->req_l,
            sizeof(*creqp->req_l) * reqp->reql_siz[i]);

        if (sort)
            qsort(reqp->reql[i], reqp->reql_siz[i], sizeof(*reqp->reql[i]), sort_cf);
    }
    return reqp;

tsr_to_reql_err:
    TaskSeg_reql_destroy(reqp);
    return NULL;
}

static TaskSeg_summary TaskSegRaw_eval(TaskSeg *tsp)
{
    TaskSegRaw *tsrp = (TaskSegRaw*)tsp;
    TaskSeg_summary tss = { 0 };
    for (unsigned i = 0; i < TSTT_enumsize; i++) {
        tss.sum[i] = tsrp->treq_l[i].sum;
        tss.avg[i] = tsrp->treq_l[i].avg;
    }
    return tss;
}

int TaskSegRaw_init(TaskSegRawCtx *ctx, TaskSegRaw *nseg)
{
    assert(ctx && nseg);
    *nseg = tasksegraw_zero;
    assert(!TaskSeg_init((TaskSegCtx*)ctx, (TaskSeg*)nseg));

    _obj_vmtp(nseg) = (Object_VMT*)&TaskSegRaw_vmt;

    for (int i = 0 ; i < TSTT_enumsize; i++) {
        nseg->treq_l[i].req_l =
            malloc(TSR_MALLOC_CNT * sizeof(*nseg->treq_l[i].req_l));
        nseg->treq_l[i].req_l_siz = TSR_MALLOC_CNT;

        if (!nseg->treq_l[i].req_l) goto tsr_init_fail;
    }

    nseg->task_type_l = malloc(sizeof(*nseg->task_type_l) * TSR_MALLOC_CNT * 2);
    assert(nseg->task_type_l);
    return 0;

tsr_init_fail:
    assert(0);
    return -1;
}

int TaskSegRawCtx_init(TaskSegRawCtx *ctx, double mu_max, double sigma_max)
{
    assert(ctx);
    *ctx = tasksegrawctx_zero;
    assert(!TaskSegCtx_init((TaskSegCtx*)ctx));
    ctx->_super.tsclass_id = TSR_CLASSID;
    ctx->compopt.mu_max = mu_max;
    ctx->compopt.sigma_max = sigma_max;

    return 0;
}

/* Append a task to the segment.
 * @param tsrp pointer to the segment
 * @param task task data to be pushed
 * @return TSR_ok on success, TSR_mem on memory allocation failure
 */
TSRRes TSR_put(TaskSegRaw *tsrp, TSRTask task)
{
    assert(tsrp);
    assert((unsigned)task.type < TSTT_enumsize);
    TReql *rqlp = &tsrp->treq_l[task.type];

    if (rqlp->task_cnt == rqlp->req_l_siz) {
        double *nfp = realloc(
            rqlp->req_l,
            rqlp->req_l_siz * sizeof(*nfp) * 2);

        if (!nfp) return TSR_mem;
        rqlp->req_l         = nfp;
        rqlp->req_l_siz     *= 2;

        TSTaskType *nttp = realloc(
            tsrp->task_type_l,
            task_lsiz_tot(tsrp) * sizeof(*nttp));

        if (!nttp) {
            /* The newly allocated list may stay as it is, although advertised
             * to be half the size.*/
            rqlp->req_l_siz /= 2;
            return TSR_mem;
        }
        tsrp->task_type_l = nttp;
    }

    tsrp->task_type_l[task_cnt_tot(tsrp)]   = task.type;
    rqlp->req_l[rqlp->task_cnt++]           = task.req;

    return TSR_ok;
}

/* Get the next task in the segment.
 * @param tsrp Pointer to the task segment
 * @return pointer to task data.
 * WARNING: subsequent calls to TSR_next() will override the task data. */
const TSRTask* TSR_next(TaskSegRaw *tsrp)
{
    assert(tsrp);

    unsigned tcsum = task_curr_tot(tsrp);
    if (tcsum == task_cnt_tot(tsrp))
        return NULL;

    TSTaskType ttype = tsrp->task_type_l[tcsum];
    TReql *reqlp = &tsrp->treq_l[ttype];

    tsrp->ct.type   = ttype;
    tsrp->ct.req    = reqlp->req_l[reqlp->task_curr++];

    return &tsrp->ct;
}

/* Reset the iterator of a segment.
 * @param tsrp pointer to the task segment */
void TSR_rewind(TaskSegRaw *tsrp)
{
    assert(tsrp);
    for (int i = 0; i < TSTT_enumsize; i++) tsrp->treq_l[i].task_curr = 0;
}

/* Get the task count of a segment, filtered by task type.
 * @param tsrp pointer to the task segment
 * @param filter task type filter
 * @return task count*/
unsigned TSR_size(TaskSegRaw *tsrp, TSTaskType filter)
{
    assert(tsrp);
    assert((unsigned)filter < TSTT_enumsize);

    return tsrp->treq_l[filter].task_cnt;
}

/* Evaluate the segment summary. If the segment is subsequently changed (e.g. by
 * a call to TSR_put(), the summary has to be reevaluated.
 * @param tsrp pointer to the task segment */
void TSR_eval(TaskSegRaw *tsrp)
{
    assert(tsrp);
    TReql *creqp;
    for (int i = 0; i < TSTT_enumsize; i++) {
        creqp = &tsrp->treq_l[i];
        creqp->avg = gsl_stats_mean(creqp->req_l, 1, creqp->task_cnt);
        creqp->stddev = creqp->task_cnt > 1 ?
            gsl_stats_sd_m(creqp->req_l, 1, creqp->task_cnt, creqp->avg) : 0.0;
        creqp->sum = 0;
        for (unsigned j = 0; j < creqp->req_l_siz; j++)
            creqp->sum += creqp->req_l[j];
    }
}

/* Merge two segments by concatenation.
 * @param tsegp1 pointer to the destination segment to be extended
 * @param tsegp1 pointer to the source segment
 * @return TSR_ok on success, TSR_mem on memory allocation failure */
TSRRes TSR_merge(TaskSegRaw *restrict tsegp1, TaskSegRaw *restrict tsegp2)
{
    TSRRes res = TSR_ok;
    assert(tsegp1 && tsegp2);

    const TSRTask *ctp;
    TSR_rewind(tsegp2);
    while ((ctp = TSR_next(tsegp2))) {
        res = TSR_put(tsegp1, *ctp);
        if (res != TSR_ok) break;
    }

    return res;
}

/* Get the average length of the segments in the context.
 * @param pointer to a segment context
 * @return average length of the segments in the context */
double TaskSegRaw_ctx_seg_meanlen(TaskSegRawCtx *ctx)
{
    assert(ctx);
    double *dl = malloc(sizeof(*dl) * ctx->_super._super.size);
    assert(dl);

    Elem *ep = ctx->_super._super.elem_dll;
    unsigned i = 0;
    while (ep) {
        TaskSegRaw *tsrp = (TaskSegRaw*)ep;
        dl[i] = tsrp->treq_l[0].task_cnt + tsrp->treq_l[1].task_cnt;

        ep = ep->next_p;
        i++;
    }
    assert(i == ctx->_super._super.size);

    return ctx->_super._super.size ?
        gsl_stats_mean(dl, 1, ctx->_super._super.size) :
        0.0;
}

/* File structure: [ Segment i ]
 *
 * [ Segment metadata ][ Task 1 ] ... [ Task k ]
 * [ 4 B              ][ 9 B    ] ... [ 9 B    ]
 *
 * [ Segment metadata ] = [ Size=k   ]
 * [ 4 B              ]   [ 4 B      ]
 *                        [ unsigned ]
 *
 * [ Task i ] = [ Task type ][ Task weight ]
 * [ 9 B    ]   [ 1 B       ][ 8 B         ]
 *              [ unsigned  ][ double      ]*/

static int _TaskSegRaw_to_file(TaskSegRaw *tsrp, FILE *wfp)
{
    assert(tsrp);
    uint32_t task_cnt = tsrp->treq_l[TSTT_calc].task_cnt + tsrp->treq_l[TSTT_com].task_cnt;
    Task_pckd *tlp = malloc(sizeof(*tlp) * task_cnt);
    assert(tlp);
    unsigned total_len = 0;

    TaskSegRaw_pckd tsrpckd;
    tsrpckd.size = task_cnt;

    assert(fwrite(&tsrpckd, sizeof(tsrpckd), 1, wfp) == 1);
    total_len += sizeof(tsrpckd);

    TSR_rewind(tsrp);
    for (uint32_t i = 0; i < task_cnt; i++) {
        const TSRTask *tp = TSR_next(tsrp);
        assert(tp);

        tlp[i].type = tp->type;
        tlp[i].weight = tp->req;
    }
    assert(!TSR_next(tsrp));

    assert(fwrite(tlp, sizeof(*tlp), task_cnt, wfp) == task_cnt);
    total_len += sizeof(*tlp) * task_cnt;

    free(tlp);
    return total_len;

}

/* Flushes a TaskSegRaw context and its segments a file.
 *
 * File structure: [ Segment data ]
 *
 * [ Segment context ][ Segment 1 ] ... [ Segment n ]
 * [ 5 B             ][ ? B       ] ... [ ? B       ]
 *
 * [ Segment context ] = [ Type=TSR_CLASSID ][ Number of segments n ]
 * [ 5 B             ]   [ 1 B              ][ 4 B                  ]
 *                       [ unsigned         ][ unsigned             ]
 *
 * @param ctx segment context
 * @param wfp file pointer for writing
 * @return returns the number bytes written, -1 on error */
int TaskSegRawCtx_to_file(TaskSegRawCtx *ctx, FILE *wfp)
{
    assert(ctx && wfp);
    int total_size = 0;

    TaskSegRawCtx_pckd ctxpckd;
    ctxpckd.size = ((ElemCtx*)ctx)->size;
    ctxpckd.class_id = TSR_CLASSID;

    assert(fwrite(&ctxpckd, sizeof(ctxpckd), 1, wfp) == 1);
    total_size += sizeof(ctxpckd);

    Elem *ep = ((ElemCtx*)ctx)->elem_dll;
    while (ep) {
        TaskSegRaw *tsrp = (TaskSegRaw*)ep;
        total_size += _TaskSegRaw_to_file(tsrp, wfp);

        ep = ep->next_p;
    }

    return total_size;
}
