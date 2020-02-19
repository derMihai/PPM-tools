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

#include "TaskSegBuck.h"
#include "stdio.h"
#include <math.h>
#include <gsl/gsl_statistics_double.h>

static const TaskSegBuck tasksegbuck_zero = { 0 };
static const TaskSegBuckCtx tasksegbuckctx_zero = { 0 };
static const TaskSeg_reql tasksegreql_zero = { 0 };

static int TaskSegBuck_compar(TaskSeg *seg1p, TaskSeg *seg2p);
static void TaskSegBuck_print(TaskSeg *segp);
static void TaskSegBuck_deinit(Object *objp);
static void TaskSegBuck_export(TaskSeg *segp, FILE *fp, TSTaskType tt);
static TaskSeg_reql* TaskSegBuck_to_reql(TaskSeg *tsp, char sort);
static TaskSeg_summary TaskSegBuck_eval(TaskSeg *tsp);

static const TaskSeg_VMT TaskSegBuck_vmt = {
    ._super._super._object_deinit = TaskSegBuck_deinit,

    .TaskSeg_compar = TaskSegBuck_compar,
    .TaskSeg_print = TaskSegBuck_print,
    .TaskSeg_export = TaskSegBuck_export,
    .TaskSeg_to_reql = TaskSegBuck_to_reql,
    .TaskSeg_eval = TaskSegBuck_eval
};

const TaskSegCtx_VMT TaskSegBuckCtx_vmt = {
    ._super._super._object_deinit = _TaskSegCtx_deinit,
};

typedef struct __attribute__((__packed__)) {
    uint8_t class_id;
    uint32_t size;
} TaskSegBuckCtx_pckd;

typedef struct __attribute__((__packed__)) {
    uint32_t size;
    uint32_t dicti[TSTT_enumsize];
} TaskSegBuck_pckd;

typedef union __attribute__((__packed__)) {
    struct {
        uint16_t    ttype  : 1;
        uint16_t    idx    : 15;
    };
    uint16_t        as_short;
} TCLetter_pckd;

static void TSB_eval(TaskSegBuck *tsbp, TaskSegRaw *orig_tsrp);

static int TaskSegBuck_compar(
    TaskSeg *seg1p,
    TaskSeg *seg2p)
{
    TaskSegBuck *bseg1p = (TaskSegBuck*)seg1p;
    TaskSegBuck *bseg2p = (TaskSegBuck*)seg2p;

    if (bseg1p == bseg2p) return 1;
    for (int i = 0; i < TSTT_enumsize; i++)
        if (bseg1p->dictp[i] != bseg2p->dictp[i]) return 0;

    if (arll_len(bseg1p->seg) != arll_len(bseg2p->seg)) return 0;

    arll_rewind(bseg1p->seg);
    arll_rewind(bseg2p->seg);
    TCLetter *lp1, *lp2;
    while ((lp1 = arll_next(bseg1p->seg))) {
        assert((lp2 = arll_next(bseg2p->seg)));
        if (lp1->as_short != lp2->as_short) return 0;
    }

    return 1;
}

static void TaskSegBuck_print(TaskSeg *segp)
{
    TaskSegBuck *bsegp = (TaskSegBuck*)segp;

    static const char * const seg_label[TSTT_enumsize] = {
        "com", "cal"
    };
    TCLetter *clp;
    arll_rewind(bsegp->seg);
    while ((clp = (TCLetter*)arll_next(bsegp->seg))) {
        TCVal cval;
        cval = TCDict_val_from_key(bsegp->dictp[clp->ttype], clp->idx);
        assert (TCVal_is_valid(cval));

        printf("%s=%f, ", seg_label[clp->ttype], cval);
    }

    printf("\n");
    for (unsigned i = 0; i < TSTT_enumsize; i++) {
        printf("\t%s dict ", seg_label[clp->ttype]);
        TCDict_print(bsegp->dictp[clp->ttype]);
    }
}

static void TaskSegBuck_deinit(Object *ep)
{
    if (!ep) return;
    TaskSegBuck *tsbp = (TaskSegBuck*)ep;
    arll_destroy(tsbp->seg);

    _TaskSeg_deinit((Object*)ep);
}

static void TaskSegBuck_export(TaskSeg *segp, FILE *fp, TSTaskType tt)
{
    TaskSegBuck *bsegp = (TaskSegBuck*)segp;

    TCLetter *clp;
    arll_rewind(bsegp->seg);
    while ((clp = (TCLetter*)arll_next(bsegp->seg))) {
        if (tt != clp->ttype) continue;

        TCVal cval = TCDict_val_from_key(bsegp->dictp[clp->ttype], clp->idx);
        assert (TCVal_is_valid(cval));

        assert (fprintf(fp, "%f\n", cval) > 0);
    }
    fflush(fp);
}

static int sort_cf(const void *a, const void *b)
{
    double diff = *(double*)a - *(double*)b;
    if      (diff > 0.0) return 1;
    else if (diff < 0.0) return -1;
    return 0;
}

static TaskSeg_reql* TaskSegBuck_to_reql(TaskSeg *tsp, char sort)
{
    TaskSegBuck *bsegp = (TaskSegBuck*)tsp;

    TaskSeg_reql *nreql = malloc(sizeof(*nreql));
    assert(nreql);
    *nreql = tasksegreql_zero;

    for (int i = 0; i < TSTT_enumsize; i++) {
        nreql->reql_siz[i] = bsegp->task_cnt[i];
        nreql->reql[i] = malloc(sizeof(**(nreql->reql)) * nreql->reql_siz[i]);
        assert(nreql->reql[i]);
    }

    TCLetter *clp;
    unsigned idx[TSTT_enumsize] = { 0 };
    arll_rewind(bsegp->seg);
    while ((clp = (TCLetter*)arll_next(bsegp->seg))) {
        assert(idx[clp->ttype] < nreql->reql_siz[clp->ttype]);

        TCVal cval = TCDict_val_from_key(bsegp->dictp[clp->ttype], clp->idx);
        assert (TCVal_is_valid(cval));

        nreql->reql[clp->ttype][idx[clp->ttype]] = cval;
        idx[clp->ttype]++;
    }

    for (unsigned i = 0; i < TSTT_enumsize; i++)
        assert(idx[i] == nreql->reql_siz[i]);

    if (sort)
        for (int i = 0; i < TSTT_enumsize; i++)
            qsort(
                nreql->reql[i],
                nreql->reql_siz[i],
                sizeof(**(nreql->reql)),
                sort_cf);

    return nreql;
}

static TaskSeg_summary TaskSegBuck_eval(TaskSeg *tsp)
{
    TaskSegBuck *tsbp = (TaskSegBuck*)tsp;
    return tsbp->summary;
}

/* Init a bucketed segment from a raw segment.
 * @param ctx pointer to the segment context
 * @param tsbp pointer to the segment
 * @param calc_dictp pointer to the calculation bucketing dictionary
 * @param com_dictp pointer to the communication bucketing dictionary
 * @param tsr_src pointer to the source raw segment
   @return 0 on success, -1 otherwise */
int TSB_init(
    TaskSegBuckCtx *ctx,
    TaskSegBuck *tsbp,
    const TCDict *calc_dictp,
    const TCDict *com_dictp,
    TaskSegRaw *tsr_src)
{
    assert(ctx && tsbp && calc_dictp && com_dictp && tsr_src);
    *tsbp = tasksegbuck_zero;

    assert(!TaskSeg_init((TaskSegCtx*)ctx, (TaskSeg*)tsbp));

    _obj_vmtp(tsbp) = (Object_VMT*)&TaskSegBuck_vmt;

    tsbp->dictp[TSTT_calc] = calc_dictp;
    tsbp->dictp[TSTT_com] = com_dictp;
//    tsbp->orig_seg = tsr_src;
    tsbp->seg = arll_construct(
        sizeof(TCLetter),
        TSR_size(tsr_src, TSTT_calc) + TSR_size(tsr_src, TSTT_com));
    assert(tsbp->seg);

    const TSRTask *ctp;
    TCLetter clt;
    TSR_rewind(tsr_src);
    TCKey ckey;

    while ((ctp = TSR_next(tsr_src))) {
        assert((unsigned)ctp->type < TSTT_enumsize);

        ckey = TCDict_key_from_val(tsbp->dictp[ctp->type], ctp->req);
        assert(TCKey_is_valid(ckey));

        clt.ttype = ctp->type;
        clt.idx = ckey;

        tsbp->task_cnt[ctp->type]++;

        assert (arll_push(tsbp->seg, &clt) != -1);
    }

    TSB_eval(tsbp, tsr_src);
    return 0;
}
/* Init a bucketed segment context.
 * @param ctx pointer to the context
 * @return 0 on success, -1 otherwise */
int TaskSegBuckCtx_init(TaskSegBuckCtx *ctx)
{
    assert(ctx);
    *ctx = tasksegbuckctx_zero;
    assert(!TaskSegCtx_init((TaskSegCtx*)ctx));
    ctx->_super.tsclass_id = TSB_CLASSID;
    return 0;
}

static void TSB_eval(TaskSegBuck *tsbp, TaskSegRaw *orig_tsrp)
{
    assert(tsbp);
    TaskSeg_reql *buck_reql = TaskSeg_to_reql((TaskSeg*)tsbp, 0);
    assert(buck_reql);

    TaskSeg_reql *raw_reql = TaskSeg_to_reql((TaskSeg*)orig_tsrp, 0);
    assert(raw_reql);

    double *deltal;

    for (unsigned i = 0; i < TSTT_enumsize; i++) {
        assert(buck_reql->reql_siz[i] == raw_reql->reql_siz[i]);
        if (buck_reql->reql_siz[i] == 0) continue;

        deltal = malloc(sizeof(*deltal) * buck_reql->reql_siz[i]);
        assert(deltal);

        tsbp->summary.devi_sum[i] = 0;
        tsbp->summary.sum[i] = 0;
        tsbp->summary.avg[i] = 0;
        for (unsigned ti = 0; ti < buck_reql->reql_siz[i]; ti++) {
            deltal[ti] = buck_reql->reql[i][ti];
            tsbp->summary.sum[i] += deltal[ti];
        }
        tsbp->summary.avg[i] = buck_reql->reql_siz[i] ?
            gsl_stats_mean(deltal, 1, buck_reql->reql_siz[i]) : 0;

        for (unsigned ti = 0; ti < buck_reql->reql_siz[i]; ti++) {
            deltal[ti] -= raw_reql->reql[i][ti];
            tsbp->summary.devi_sum[i] += deltal[ti];
            deltal[ti] = fabs(deltal[ti]);
        }

        tsbp->summary.devi_mean[i] = buck_reql->reql_siz[i] ?
                gsl_stats_mean(deltal, 1, buck_reql->reql_siz[i]) : 0;
        free(deltal);

        tsbp->summary.dict_size[i] = tsbp->dictp[i]->size;
    }

    TaskSeg_reql_destroy(buck_reql);
    TaskSeg_reql_destroy(raw_reql);
}

/* File structure: [ Segment i ]
 *
 * [ Segment metadata ][ Key 1    ] ... [ Key k    ]
 * [ 12 B             ][ 2 B      ]     [ 2 B      ]
 *                     [ unsigned ]     [ unsigned ]
 *
 * [ Segment metadata ] = [ Size=k   ][ Calc dict index ][ Com dict index ]
 * [ 12 B             ]   [ 4 B      ][ 4 B             ][ 4 B            ]
 *                        [ unsigned ][ unsigned        ][ unsigned       ]
 *
 * [ Key i ] = [ Task type ][ Index ]
 * [ 2 B   ]   [ 1 b       ][ 15 b  ]
 *             [ unsigned           ]*/
static int _TaskSegBuck_to_file(TaskSegBuck *tsbp, FILE *wfp)
{
    assert(tsbp);
    int tot_len = 0;

    TaskSegBuck_pckd tsbpckd;
    unsigned task_cnt = tsbp->task_cnt[TSTT_calc] + tsbp->task_cnt[TSTT_com];
    tsbpckd.size = task_cnt;
    tsbpckd.dicti[TSTT_calc] = ((Elem*)tsbp->dictp[TSTT_calc])->idx;
    tsbpckd.dicti[TSTT_com] = ((Elem*)tsbp->dictp[TSTT_com])->idx;

    assert(fwrite(&tsbpckd, sizeof(tsbpckd), 1, wfp) == 1);
    tot_len += sizeof(tsbpckd);

    TCLetter_pckd *letl = malloc(sizeof(*letl) * task_cnt);
    assert(letl);

    arll_rewind(tsbp->seg);
    for (unsigned i = 0; i < task_cnt; i++) {
        TCLetter *tclp = arll_next(tsbp->seg);
        assert(tclp);
        letl[i].as_short = tclp->as_short;
    }
    assert(!arll_next(tsbp->seg));

    assert(fwrite(letl, sizeof(*letl), task_cnt, wfp) == task_cnt);
    tot_len += sizeof(*letl) * task_cnt;

    free(letl);
    return tot_len;
}
/* Flushes a TaskSegBuck context and its segments a file.
 *
 * File structure: [ Segment data ]
 * [ Segment context ][ Segment 1 ] ... [ Segment n ]
 * [ 5 B             ][ ? B       ] ... [ ? B       ]
 *
 * [ Segment context ] = [ Type=TSB_CLASSID ][ Number of segments n ]
 * [ 5 B             ]   [ 1 B              ][ 4 B                  ]
 *                       [ unsigned         ][ unsigned             ]
 *
 * @param ctx segment context
 * @param wfp file pointer for writing
 * @param dictctx the context for the dictionaries used by this segment context.
 * @return returns the number bytes written, -1 on error */
int TaskSegBuckCtx_to_file(TaskSegBuckCtx *ctx, FILE *wfp, TCDictCtx *dictctx)
{
    assert(ctx && wfp && dictctx);
    unsigned total_size = 0;

    ElemCtx_assign_idx((ElemCtx*)dictctx);

    TaskSegBuckCtx_pckd ctxpckd;
    ctxpckd.size = ((ElemCtx*)ctx)->size;
    ctxpckd.class_id = TSB_CLASSID;

    assert(fwrite(&ctxpckd, sizeof(ctxpckd), 1, wfp) == 1);
    total_size += sizeof(ctxpckd);

    Elem *ep = ((ElemCtx*)ctx)->elem_dll;
    while (ep) {
        TaskSegBuck *tsrp = (TaskSegBuck*)ep;
        total_size += _TaskSegBuck_to_file(tsrp, wfp);

        ep = ep->next_p;
    }

    return total_size;
}

