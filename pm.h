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

#ifndef PM_H_
#define PM_H_
/*
 * The PMVG and PMVGCtx classes are NOT inheritable.
 */
#include <stdint.h>
#include "TaskSeg.h"
#include "element_context.h"
#include "arll.h"
#include "gplot.h"
#include "model_parser.h"

typedef enum {
    PMV_seg,
    PMV_insc,
    PMV_wrap,

    PMV_enumsize
} PMVType;

typedef struct PMV PMV;
typedef struct PMVG PMVG;
//typedef struct CPMV CPMV;
typedef struct Segcont Segcont;
typedef struct PMContext PMContext;
typedef struct CPMVContext CPMVContext;

typedef struct CPMV CPMV;
struct CPMV {
    PMVType type;
    PMVG *np;
    union {
        struct {
            PMVG *pp;
            PMVG *cp;
        };
        PMVG *wp;
    };
};

/* NOT INHERITABLE */
struct PMVG {
    Elem _super;
    arll *vpl;
    CPMV cpmv;
    /* for debugging */
    unsigned id;
};

struct Segcont {
    union {
        TaskSeg *segp;
        int segi;
    };
    int pid;
};

struct PMV {
    /* Vertex type: segment, inosculation or wrapper */
    PMVType type;
    /* next vertex */
    PMV *np;
    /* pointer to the pointer to this vertex */
    PMV **prevnpp;
    /* pointer to the vertex group */
    PMVG *gp;
    /* pointer to the PM context */
    PMContext *ctxp;
    /* Depth of the tree. Wrappers do not add. */
    unsigned    depth;
    /* Number of vertices in the tree. Wrappers do not add. */
    unsigned    vcnt;
    /* Hash value. Wrappers do not change the hash. */
    uint32_t    hash;
    /* interal flags. !!!! DO NOT USE EXTERNALLY !!!! */
    uint32_t    flags;
    /* container for external use. As pointer or as unsigned integer. It is
     * initialized to zero and is internally untouched throughout the life of
     * the PM context.*/
    union {
        void *as_ptr;
        uint32_t as_uint;
    }           external;
    union {
        /* for segment vertex: index of the segment container */
        int segconti;
        /* for inosculation vertex: */
        struct {
            /* pointer to parent branch vertex */
            PMV *pp;
            /* pointer to child branch vertex */
            PMV *cp;
        };
        /* for wrapper vertex: pointer to the wrapped section */
        PMV *wp;
    };
};

/* NOT INHERITABLE */
typedef struct {
    ElemCtx _super;
    unsigned gid_curr;
} PMVGCtx;

typedef Elem_VMT PMVG_VMT;

struct PMContext {
    PMV *headp;
    PMVGCtx gctx;
    arll *segcontl;
    unsigned pmvcnt[PMV_enumsize];
    /* for debugging */
    gnuplot *gplot;
};

/* note: task deviation is the difference between compressed and uncompressed
 * task weight. */
typedef struct {
    /* task deviation sum across the whole model */
    double devi_sum_total[TSTT_enumsize];
    /* average of task deviation sum across each segment */
    double devi_sum_mean[TSTT_enumsize];
    /* standard deviation of task deviation sum across each segment */
    double devi_sum_stddev[TSTT_enumsize];
    /* average of task deviation average across each segment */
    double devi_mean[TSTT_enumsize];
    /* average of task deviation standard deviation across each segment */
    double devi_mean_stddev[TSTT_enumsize];
    /* average dictionary size */
    double dict_size_mean[TSTT_enumsize];
    /* total dictionary size */
    double dict_size_total[TSTT_enumsize];
    /* average task badness */
    double task_badness_mean[TSTT_enumsize];
    /* average segment badness */
    double seg_badness_mean[TSTT_enumsize];
} PM_seg_summary;


PMContext *PMContext_create(void);
void PMContext_destroy(PMContext *ctx);
int PMV_insc_is_symm(PMV *vp);
PMV *PMV_wrap_section(PMV *fromp, PMV *untilp);
void PMV_find_similar_stem(
    PMV *v1,
    PMV *v2,
    PMV **v1end,
    PMV **v2end,
    char check_summary);
int PMV_is_similar(PMV *v1, PMV *v2, char check_summary);
void PMV_merge_r(PMV *v1p, PMV *v2p) ;
//int CPMVContext_init(CPMVContext *ctx);
void PMV_plot(PMContext *ctx);
int PMV_build_graph(MParser *parsctx, PMContext *pm_ctx, TaskSegRawCtx *tsrctx);
static Segcont PMV_getseg(PMV* vp) {
    assert(vp);
    assert(vp->type == PMV_seg);
    Segcont *segcontp = (Segcont*)arll_geti(vp->ctxp->segcontl, vp->segconti);
    assert(segcontp);
    return *segcontp;
}
static int PMV_setseg(PMV* vp, TaskSeg *segp)
{
    assert(vp);
    Segcont *segcontp = (Segcont*)arll_geti(vp->ctxp->segcontl, vp->segconti);
    assert(segcontp);
    segcontp->segp = segp;
    return 0;
}

static void PMContext_get_vcnt(PMContext *ctx, unsigned vcnt[PMV_enumsize]) {
    assert(ctx);
    memcpy(vcnt, ctx->pmvcnt, sizeof(*vcnt) * PMV_enumsize);
}
PM_seg_summary PMContext_eval(PMContext *ctx);
//void PMContext_check(PMContext *ctx);
int PMContext_to_file(PMContext *ctx, FILE *wfp, TaskSegCtx *segctx);
int PMContext_init_gplot(PMContext *pmctx);
static inline PMVG *PMContext_get_grouplist(PMContext *pmctx)
{
    assert(pmctx);
    return (PMVG*)(pmctx->gctx._super).elem_dll;
}
#endif /* PM_H_ */
