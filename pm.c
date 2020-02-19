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

#include "pm.h"
#include <gsl/gsl_statistics_double.h>

/* Status bits for all vertex types*/
#define PMV_SBIT_evaluated             0x0001
#define PMV_SBIT_commonstem_start_1    0x0002
#define PMV_SBIT_commonstem_start_2    0x0004

/* Status bits for inosculation vertices */
#define PMV_SBIT_INSC_is_sym           0x0200

static const PMContext pmcontext_zero = { 0 };
//static const CPMVContext cpmvcontext_zero = { 0 };
static const PMV pmv_zero = { 0 };

#define BIG_C_RAD (2.0)
#define SMOL_C_RAD (0.5)
#define Y_SPACE (BIG_C_RAD * 2)
#define X_SPACE (BIG_C_RAD * 2)
#define VERY_BIG_NUM (10e6)

typedef enum {
    Cstr_ralign,
    Cstr_lalign
} CstrType;

typedef struct {
    CstrType cstr;
    union {
        double xmin;
        double xmax;
    };
    double y;
} Constraint;

typedef struct {
    double xmin;
    double xmax;
    double ymin;

    struct {
        double nx;
        double ny;
    };
} Bounds;

enum {
    PF_edges,
    PF_vertex,

    PF_enumsize
};
static const char * const plot_file[PF_enumsize] = {
    "pvm_plots/edges.dat",
    "pvm_plots/vertex.dat"
};

enum {
    PP_graph,

    PP_enumsize
};
static const char * const plot_pipes[PP_enumsize] = {
    "gnuplot"
};

static int PMVGCtx_init(PMVGCtx *gctx);
static int PMVG_init(PMContext *ctx, PMVG *gp, PMVType type);
static void PMVG_deinit(Object *objp);
static PMV *PMV_create(PMContext *ctx, PMVType type, PMV **prevnpp);
static int PMVG_addv(PMVG *gp, PMV *vp);
static void PMV_eval_r(PMV *vp, char force);
static int PMVG_merge(PMVG *to, PMVG *from);
static PMV* _build_graph(
    MParser *parsctx,
    PMContext *pmctx,
    PMV **prevnpp,
    TaskSegRawCtx *tsrctx);


const PMVG_VMT PMVG_vmt = {
    ._super._object_deinit = PMVG_deinit
};

/* Allocate and init a context for the PPM.
 * @return pointer to newly created context */
PMContext *PMContext_create(void)
{
    PMContext *pmctx = malloc(sizeof(*pmctx));
    assert(pmctx);
    *pmctx = pmcontext_zero;

    assert(PMVGCtx_init(&pmctx->gctx) == 0);
    pmctx->segcontl = arll_construct(sizeof(Segcont), 64);
    assert(pmctx->segcontl);

    return pmctx;
}

/* For debugging. Inits the plotting enviroment for the context.
 * @param pmctx pointer to the PM context
 * @return 0 if success, -1 otherwise */
int PMContext_init_gplot(PMContext *pmctx)
{
    assert(pmctx);

    pmctx->gplot = gplot_create(plot_pipes, plot_file, PP_enumsize, PF_enumsize);
    if (!pmctx->gplot) return -1;
    return 0;
}

static int PMVGCtx_init(PMVGCtx *gctx)
{
    assert(gctx);
    assert (!ElemCtx_init((ElemCtx*)gctx));
    return 0;
}

static int PMVG_init(PMContext *ctx, PMVG *gp, PMVType type)
{
    assert(ctx && gp);
    assert(!Elem_init((ElemCtx*)&ctx->gctx, (Elem*)gp));
    assert((unsigned)type < PMV_enumsize);

    assert(gp);
    _obj_vmtp(gp) = (Object_VMT*)&PMVG_vmt;
    gp->vpl = arll_construct(sizeof(PMV*), 1);
    assert(gp->vpl);
    gp->id = ctx->gctx.gid_curr++;
    gp->cpmv.type = type;

    return 0;
}
static void PMVG_deinit(Object *objp)
{
    if (!objp) return;
    PMVG *gp = (PMVG*)objp;
    arll_destroy(gp->vpl);

    _Elem_deinit(objp);
}

static int PMVG_addv(PMVG *gp, PMV *vp)
{
    assert(gp && vp);
    assert(vp->type == gp->cpmv.type);
    return arll_push(gp->vpl, &vp) == -1 ? -1 : 0;
}

static void PMV_eval_r(PMV *vp, char force) {
    /* biggest prime leq (2^31 - 1) / 2 */
    static const uint32_t hp = 0x7FffFFff;

    assert(vp);
    assert((unsigned)vp->type < PMV_enumsize);

    unsigned max_depth = 0;
    switch(vp->type) {
    case PMV_wrap:
        /* wrapper doesn't add */
        vp->hash = 0x0;
        vp->depth = 0;
        vp->vcnt = 0;

        if (vp->wp) {
            if (force || !(vp->wp->flags & PMV_SBIT_evaluated)) {
                PMV_eval_r(vp->wp, force);
            }
            vp->depth += vp->wp->depth;
            vp->vcnt += vp->wp->vcnt;
            vp->hash += vp->wp->hash;
            vp->hash %= hp;
        }
        break;

    case PMV_insc:
        vp->hash = 0x1 << 15;
        vp->depth = 1;
        vp->vcnt = 1;

        if (vp->pp) {
            if (force || !(vp->pp->flags & PMV_SBIT_evaluated)) {
                PMV_eval_r(vp->pp, force);
            }
            max_depth = vp->pp->depth;
            vp->vcnt += vp->pp->vcnt;
            vp->hash += vp->pp->hash;
            vp->hash %= hp;
        }

        if (vp->cp) {
            if (force || !(vp->cp->flags & PMV_SBIT_evaluated)) {
                PMV_eval_r(vp->cp, force);
            }
            if (vp->cp->depth > max_depth)
                max_depth = vp->cp->depth;
            
            vp->vcnt += vp->cp->vcnt;
            vp->hash += vp->cp->hash;
            vp->hash %= hp;
        }
        vp->depth += max_depth;

        if (PMV_is_similar(vp->pp, vp->cp, 1))
            vp->flags |= PMV_SBIT_INSC_is_sym;

        break;

    case PMV_seg:
        vp->hash = 0x1;
        vp->depth = 1;
        vp->vcnt = 1;

        break;

    default:
        assert(0);
    }

    if (vp->np) {
        if (force || !(vp->np->flags & PMV_SBIT_evaluated)) {
            PMV_eval_r(vp->np, force);
        }
        vp->depth += vp->np->depth;
        vp->vcnt += vp->np->vcnt;
        vp->hash += vp->np->hash;
        vp->hash %= hp;
    }
    
    vp->flags |= PMV_SBIT_evaluated;
}

static void _PMV_find_common_stem(
    PMV *v1p,
    PMV *v2p,
    PMV **v1endpp,
    PMV **v2endpp,
    char check_summary)
{
    assert(v1endpp && v2endpp);
    if (v1p) assert(v1p != v2p); /* reflexive comparison should not occur */

    if ((!v1p != !v2p) || (v1p == v2p)) {
        /* one terminates before the other or both null */
        goto _PMV_eq_until_prev;
    }

    /* check overlap */
    if ((v1p->flags & PMV_SBIT_commonstem_start_2) ||
        (v2p->flags & PMV_SBIT_commonstem_start_1))
        goto _PMV_eq_until_prev;

    if (v1p->type != v2p->type) {
        /* Vertex types are different */
        PMV *vwp;
        PMV *vop;
        PMV **vwendpp;
        PMV **voendpp;

        if (v1p->type == PMV_wrap) {
            vwp = v1p;
            vop = v2p;
            vwendpp = v1endpp;
            voendpp = v2endpp;
        } else if (v2p->type == PMV_wrap) {
            vwp = v2p;
            vop = v1p;
            vwendpp = v2endpp;
            voendpp = v1endpp;
        } else {
            /* none is wrapper, equal until previous */
            goto _PMV_eq_until_prev;
        }

        /* One is wrapper, compare its subgraphs against the other.
         * 'check_summary' overridden off, because the wrapped branch does not
         * sit on top of anything */
        PMV_find_similar_stem(vwp->wp, vop, vwendpp, voendpp, 0);
        /* check no similarities */
        if (!*vwendpp)              goto _PMV_eq_until_prev;   
        /* check wrapped not equal until end. */
        if ((*vwendpp)->np != NULL) goto _PMV_eq_until_prev;   
  
        /* wrapped equal until end, we can check further. */
        PMV_find_similar_stem(vwp->np, (*voendpp)->np, vwendpp, voendpp, check_summary);
        /* check no further similarities */
        if (*vwendpp == NULL)       goto _PMV_eq_until_here;
        
        return;
    }
    
    /* vertex types are the same */
    if (check_summary) {
        if (v1p->hash != v2p->hash)     goto _PMV_eq_until_prev;
        if (v1p->depth != v2p->depth)   goto _PMV_eq_until_prev;
        if (v1p->vcnt != v2p->vcnt)     goto _PMV_eq_until_prev;
    }

    switch (v1p->type) {
        /* For inosculation and wrapper: if branches are not equal, then equal 
         * until previous */
    case PMV_seg:
        /* nothing to do */
        break;

    case PMV_insc:
        if (PMV_insc_is_symm(v1p) != PMV_insc_is_symm(v2p))
            goto _PMV_eq_until_prev;

        PMV_find_similar_stem(v1p->pp, v2p->pp, v1endpp, v2endpp, 1);
        if (*v1endpp == NULL)                   goto _PMV_eq_until_prev;
        if ((*v1endpp)->np != (*v2endpp)->np)   goto _PMV_eq_until_prev;

        PMV_find_similar_stem(v1p->cp, v2p->cp, v1endpp, v2endpp, 1);
        if (*v1endpp == NULL)                   goto _PMV_eq_until_prev;
        if ((*v1endpp)->np != (*v2endpp)->np)   goto _PMV_eq_until_prev;
        break;

    case PMV_wrap:
        PMV_find_similar_stem(v1p->wp, v2p->wp, v1endpp, v2endpp, 1);
        if (*v1endpp == NULL)                   goto _PMV_eq_until_prev;
        if ((*v1endpp)->np != (*v2endpp)->np)   goto _PMV_eq_until_prev; 
        break;

    default:
        assert(0);
    }

    PMV_find_similar_stem(v1p->np, v2p->np, v1endpp, v2endpp, check_summary);
    if (*v1endpp == NULL)
        goto _PMV_eq_until_here;
    
    return;

_PMV_eq_until_prev:
    v1p = NULL;
    v2p = NULL;
_PMV_eq_until_here:
    *v1endpp = v1p;
    *v2endpp = v2p;
    return;
}


/* A stem is a chain of vertices linked exclusively by the 'next' pointer.
 * Two stems are similar iff they form similar PPM trees in a detached state.
 *
 * If values pointed by 'v1end' and 'v2end' are NULL, 'v1' and 'v2' have no
 * similar stem. If 'v1end' and 'v2end' are non-null, 'v1end' and 'v2end' are the
 * last vertices of the similar stems.
 *
 * Option 'check_summary' only affects the comparison between 'v1' and 'v2' and
 * is recursively transmitted only to next-type references, i.e. only for the
 * similar stem. All other references are checked again against summary. Turning
 * it off is used to compare a wrapped subgraph with a non-wrapped subgraph,
 * since their summary in the next-type reference chain will most probably not
 * coincide (the wrapped subgraph is detached from its successor). During the
 * recursion, this rule is automatically turned off in event of another such
 * comparison.
 *
 * Note that wrappers are not part of the comparison: If a wrapper is
 * encountered, its next segment is temporary concatenated to the wrapped
 * segment, so that for the duration of comparison they form a stem. However,
 * if the comparison ends before the wrapped section ends, the returned similar
 * stems end with the vertex preceding the wrapped vertex.
 *
 * A check for overlapping is also performed, so it is safe to use on parts of
 * the tree that sit on the same stem.
 *
 * @param v1p pointer to a vertex
 * @param v2p pointer to another vertex
 * @param v1endpp pointer to a vertex pointer that will hold the end of the stem
 *  starting at 'v1p', in case a common stem is found, otherwise it will be set
 *  to NULL
 * @param v2endpp pointer to a vertex pointer that will hold the end of the stem
 *  starting at 'v2p', in case a similar stem is found, otherwise it will be set
 *  to NULL
 * @param check_summary set to 1 to turn on summary check on the stem, 0
 *  otherwise*/

void PMV_find_similar_stem(
    PMV *v1p,
    PMV *v2p,
    PMV **v1endpp,
    PMV **v2endpp,
    char check_summary)
{
    assert(v1endpp && v2endpp);
    if (v1p) assert(v1p != v2p); /* reflexive comparison should not occur */

    if ((!v1p != !v2p) || (v1p == v2p)) {
        /* one terminates before the other or both null */
        *v1endpp = NULL;
        *v2endpp = NULL;
        return;
    }

    v1p->flags |= PMV_SBIT_commonstem_start_1;
    v2p->flags |= PMV_SBIT_commonstem_start_2;

    _PMV_find_common_stem(v1p, v2p, v1endpp, v2endpp, check_summary);

    v1p->flags &= !PMV_SBIT_commonstem_start_1;
    v2p->flags &= !PMV_SBIT_commonstem_start_2;
}

/* Check if the PPMs starting at 'v1' and 'v2' are similar.
 * @param v1 pointer to a vertex
 * @param v2 pointer to another vertex
 * @param check_summary see PMV_find_common_stem()
 * @return 1 if similar, 0 ohterwise */
int PMV_is_similar(PMV *v1, PMV *v2, char check_summary)
{
    if (!v1) return v1 == v2;
    
    PMV *v1end, *v2end;
    PMV_find_similar_stem(v1, v2, &v1end, &v2end, check_summary);
    if (v1end == NULL) return 0;
    if (v1end->np != v2end->np) return 0;
    return 1;
}

/* Merge two PMV groups. All the members of 'from' are transferred to 'to' and
 * their membership is updated, then 'from' is destroyed.
 * @param to merge destination
 * @param from merge source
 * @return 0 on success, -1 otherwise */
static int PMVG_merge(PMVG *to, PMVG *from)
{
    assert(to && from);
    if (to == from) return 0;
    assert(to->cpmv.type == from->cpmv.type);
    PMV **vpp;
    while ((vpp = arll_next(from->vpl))) {
        PMV *vp = *vpp;
        vp->gp = to;
        assert (PMVG_addv(to, vp) == 0);
    }

    Object_deinit((Object*)from);
    _obj_free(from);

    return 0;
}

/* Merge two similar subtrees starting at 'v1p' and 'v2p'. The merging process
 * merges the groups of corresponding vertices.
 * @param v1p a non-NULL vertex pointer
 * @param v2p another non-NULL vertex pointer
 * @pre the subtrees are similar, otherwise this function will fail with a
 * false assertion */
void PMV_merge_r(PMV *v1p, PMV *v2p) {
    if (!v1p) {
        assert(v1p == v2p);
        return;
    }

    assert(v1p->ctxp == v2p->ctxp);

    if (v1p->type != v2p->type) {
        PMV *vwrap, *vother;
        PMV *vwrapend, *votherend;

        if (v2p->type == PMV_wrap) {
            vwrap = v2p;
            vother = v1p;

        } else if (v1p->type == PMV_wrap) {
            vwrap = v1p;
            vother = v2p;

        } else {
            /* none is wrapper */
            assert(0);
        }

        PMV_find_similar_stem(vwrap->wp, vother, &vwrapend, &votherend, 0);
        assert(vwrapend);
        assert(vwrapend->np == NULL);

        assert(PMV_wrap_section(vother, votherend) == 0);

        PMV_merge_r(v1p, v2p);
        return;
    }

    switch (v1p->type) {
    case PMV_seg:
        break;

    case PMV_insc:
        PMV_merge_r(v1p->pp, v2p->pp);
        PMV_merge_r(v1p->cp, v2p->cp);
        break;

    case PMV_wrap:
        PMV_merge_r(v1p->wp, v2p->wp);
        break;

    default:
        assert(0);
    }

    PMV_merge_r(v1p->np, v2p->np);
    PMVG_merge(v1p->gp, v2p->gp);
}

static PMV *PMV_create(PMContext *ctx, PMVType type, PMV **prevnpp)
{
    assert((unsigned)type < PMV_enumsize);
    assert(ctx);

    PMV *nvp = malloc(sizeof(*nvp));
    assert(nvp);
    *nvp = pmv_zero;

    nvp->type = type;
    nvp->prevnpp = prevnpp;
    nvp->gp = _obj_alloc(sizeof(PMVG));
    assert(!PMVG_init(ctx, nvp->gp, type));
    nvp->ctxp = ctx;
    assert(nvp->gp);

    assert (PMVG_addv(nvp->gp, nvp) == 0);
    ctx->pmvcnt[type]++;

    return nvp;
}

/* Check if an inosculation vertex in symmetric.
 * @param vp pointer to an inosculation vertex
 * @return > 0 if symmetric, 0 otherwise */
int PMV_insc_is_symm(PMV *vp)
{
    assert(vp);
    assert(vp->type == PMV_insc);

    if (!(vp->flags & PMV_SBIT_evaluated)) {
        PMV_eval_r(vp, 0);
    }
    return vp->flags & PMV_SBIT_INSC_is_sym;
}

static void plot_edge(double x1, double y1, double x2, double y2, gnuplot *gplp)
{
    fprintf(
        gplot_getf(gplp, PF_edges),
        "%f %f\n%f %f\n\n",
        x1, y1,
        x2, y2);
}

static void plot_node(const PMV *vp, double x, double y, gnuplot *gplp)
{
    assert(vp);
    assert((unsigned)vp->type < PMV_enumsize);

    static const char *_lab[PMV_enumsize] = {"S", "I", "W"};
    fprintf(
        gplot_getf(gplp, PF_vertex),
        "%f %f %s%03u %f\n",
        x,
        y,
        _lab[vp->type],
        vp->gp->id,
        BIG_C_RAD);
}

static Bounds graph_plot_r(const PMV *vp, Constraint cstr, gnuplot *gplp)
{
    Constraint passcstr;
    Bounds retb;
    Bounds currb;

    currb.ymin = cstr.y;
    currb.ny = cstr.y;

    switch (cstr.cstr) {
    case Cstr_lalign:
        currb.nx = cstr.xmax;
        break;

    case Cstr_ralign:
        currb.nx = cstr.xmin;
        break;
    }

    if (!vp) {
        switch (cstr.cstr) {
        case Cstr_lalign:
            currb.xmin = cstr.xmax - X_SPACE;
            break;

        case Cstr_ralign:
            currb.xmax = cstr.xmin + X_SPACE;
            break;
        }

        return currb;
    }

    double ymin;
    switch (vp->type) {
    case PMV_wrap:
        retb.ymin = 0; /* Shut up gcc.. */
        switch (cstr.cstr) {
        case Cstr_lalign:
            passcstr = cstr;
            passcstr.xmax -= X_SPACE;

            retb = graph_plot_r(vp->wp, passcstr, gplp);
            plot_edge(currb.nx, currb.ny, retb.nx, retb.ny, gplp);

            currb.xmin = retb.xmin;

            break;
        case Cstr_ralign:
            passcstr = cstr;
            passcstr.xmin += X_SPACE;

            retb = graph_plot_r(vp->wp, passcstr, gplp);
            plot_edge(currb.nx, currb.ny, retb.nx, retb.ny, gplp);

            currb.xmax = retb.xmax;
            break;
        }

        passcstr = cstr;
        passcstr.y = retb.ymin - Y_SPACE;
        ymin = retb.ymin;

        break;
    case PMV_insc:
        ymin = 0; /* shut up gcc... */
        switch (cstr.cstr) {
        case Cstr_lalign:
            passcstr = cstr;
            passcstr.y -= Y_SPACE;
            passcstr.xmax -= X_SPACE;

            retb = graph_plot_r(vp->cp, passcstr, gplp);
            plot_edge(currb.nx, currb.ny, retb.nx, retb.ny, gplp);

            passcstr.xmax = retb.xmin - X_SPACE;
            ymin = retb.ymin;

            retb = graph_plot_r(vp->pp, passcstr, gplp);
            plot_edge(currb.nx, currb.ny, retb.nx, retb.ny, gplp);

            currb.xmin = retb.xmin;
            if (retb.ymin < ymin) ymin = retb.ymin;

            break;
        case Cstr_ralign:
            passcstr = cstr;
            passcstr.y -= Y_SPACE;
            passcstr.xmin += X_SPACE;

            retb = graph_plot_r(vp->pp, passcstr, gplp);
            plot_edge(currb.nx, currb.ny, retb.nx, retb.ny, gplp);

            passcstr.xmin = retb.xmax + X_SPACE;
            ymin = retb.ymin;

            retb = graph_plot_r(vp->cp, passcstr, gplp);
            plot_edge(currb.nx, currb.ny, retb.nx, retb.ny, gplp);

            currb.xmax = retb.xmax;
            if (retb.ymin < ymin) ymin = retb.ymin;

            break;

        }

        passcstr = cstr;
        passcstr.y = ymin - Y_SPACE;
        currb.ymin = ymin;

        break;
    case PMV_seg:
        switch (cstr.cstr) {
        case Cstr_lalign:
            break;
        case Cstr_ralign:
            break;
        }

        passcstr = cstr;
        passcstr.y -= Y_SPACE;

        break;
    default:
        assert(0);
    }

    retb = graph_plot_r(vp->np, passcstr, gplp);
    plot_edge(currb.nx, currb.ny, retb.nx, retb.ny, gplp);

    if (retb.xmin < currb.xmin) currb.xmin = retb.xmin;
    if (retb.xmax > currb.xmax) currb.xmax = retb.xmax;
    if (retb.ymin < currb.ymin) currb.ymin = retb.ymin;

    switch (cstr.cstr) {
    case Cstr_lalign:
        plot_node(vp, cstr.xmax, cstr.y, gplp);
        break;
    case Cstr_ralign:
        plot_node(vp, cstr.xmin, cstr.y, gplp);
        break;
    }

    return currb;
}

/* Wrap a PPM section. A new wrapper node in inserted in the place of 'fromp'.
 * The section starting with 'fromp' and ending with 'untilp' is removed and
 * attached as the wrapped section of the newly created wrapper. The section
 * following 'untilp' (the next pointer) is attached as the next section of the
 * newly created wrapper.
 * @pre 'fromp' and 'untilp' are non-NULL pointers and untilp is reachable by
 *  recursively dereferencing exclusively the next pointer, starting at 'fromp',
 *  i.e. they are on the same stem.
 * @param fromp pointer to the vertex where the wrapped section will start
 * @param untilp pointer to the vertex where the wrapped section will end
 * @return pointer to the newly created wrapper */
PMV *PMV_wrap_section(PMV *fromp, PMV *untilp)
{
    assert(fromp && untilp);
    assert(fromp->ctxp == untilp->ctxp);
    PMContext *ctx = fromp->ctxp;
    PMV *nv = PMV_create(ctx, PMV_wrap, fromp->prevnpp);
    assert(nv);

    PMV *cv = fromp;
    while (cv) {
        if (cv == untilp) break;
        cv = cv->np;
    }
    assert(cv);

    nv->wp = fromp;
    *nv->prevnpp = nv;
    fromp->prevnpp = &nv->wp;

    if (untilp) {
        nv->np = untilp->np;
        untilp->np = NULL;
    } else {
        printf(
            "[Warning][PMV][PMV_wrap_section]: superfluous wrapper around vertex in group %u\n",
            nv->wp->gp->id);
    }

    if (nv->np)
        nv->np->prevnpp = &nv->np;

    PMV_eval_r(nv->wp, 1);
    PMV_eval_r(nv, 0);

    return nv;
}

/* For debugging. Visualize the PPM tree of a context using gnuplot.
 * @param ctx pointer to a PM context
 * @pre the plotting enviroment for the context was initialized with
 *  PMContext_init_gplot() */
void PMV_plot(PMContext *ctx)
{
    assert(ctx);
    if (!ctx->gplot) return;

    Constraint constr;
    constr.cstr = Cstr_ralign;
    constr.xmin = 0.0;
    constr.y = 0.0;

    gplot_reset_all(ctx->gplot);
    graph_plot_r(ctx->headp, constr, ctx->gplot);
    assert (fflush(gplot_getf(ctx->gplot, PF_edges)) == 0);
    assert (fflush(gplot_getf(ctx->gplot, PF_vertex)) == 0);

    fprintf(
        gplot_getp(ctx->gplot, PP_graph),
        "plot '%s' u 1:2 with lines lc rgb \"black\" lw 1 notitle, "
//        "'%s' u 1:2:4 with circles linecolor rgb \"white\" lw 2 fill solid border lc lt 0 notitle, "
        "'%s' using 1:2:3 with labels offset (0,0) font 'Arial Bold, 10' notitle \n",
        plot_file[PF_edges],
//        plot_file[PF_vertex],
        plot_file[PF_vertex]);
    fflush(gplot_getp(ctx->gplot, PP_graph));
}

static PMV* _create_seg(
    MParser *parsctx,
    PMContext *pmctx,
    PMV **prevnpp,
    TaskSegRawCtx *tsrctx)
{
    MPTask ct = parsctx->task_l[parsctx->cti];
    PMV *nv = PMV_create(pmctx, PMV_seg, prevnpp);
    assert(nv);

    Segcont nsegcont;
//    nsegcont.segp = (TaskSeg*)TSR_create(tsrctx);
    nsegcont.segp = _obj_alloc(sizeof(TaskSegRaw));
    assert(!TaskSegRaw_init(tsrctx, (TaskSegRaw*)nsegcont.segp));
    nsegcont.pid = ct.pno;
    assert(nsegcont.segp);

    TSRTask csegt;
    while(ct.ttype == MPTT_calc || ct.ttype == MPTT_com) {
        if (ct.ttype == MPTT_calc) {
            csegt.type = TSTT_calc;
            csegt.req = ct.req < parsctx->cap_val_cal ? ct.req : parsctx->cap_val_cal;
        } else {
            csegt.type = TSTT_com;
            csegt.req = ct.req < parsctx->cap_val_com ? ct.req : parsctx->cap_val_com;
        }

        if (ct.pno != nsegcont.pid) {
            printf(
                "[Fatal][PMV][create_seg]: task %lu has pid=%d"
                "but the segment has pid=%d\n",
                parsctx->cti,
                ct.pno,
                nsegcont.pid);
            assert(0);
        }
        if(TSR_put((TaskSegRaw*)nsegcont.segp, csegt) != TSR_ok) {
            printf(
                "[Fatal][PMV][create_seg]: on task %lu, TSeg_put failed.\n",
                parsctx->cti);
            assert(0);
        }

        parsctx->cti = ct.next[0];
        ct = parsctx->task_l[parsctx->cti];
    }

//    nsegcont.segp = nsegp;
    TSR_eval((TaskSegRaw*)nsegcont.segp);

    nv->segconti = arll_push(pmctx->segcontl, &nsegcont);
    assert(nv->segconti != -1);

    nv->np = _build_graph(parsctx, pmctx, &nv->np, tsrctx);
    assert(parsctx->cti);
    return nv;
}

static PMV* _create_insc(
    MParser *parsctx,
    PMContext *pmctx,
    PMV **prevnpp,
    TaskSegRawCtx *tsrctx)
{
    MPTask ct = parsctx->task_l[parsctx->cti];

    PMV *nv;
    if (ct.next[1] == 0) {
        int pno = ct.pno;
        /* empty fork, ignore */
        parsctx->cti = ct.next[0];
        ct = parsctx->task_l[parsctx->cti];
        assert(ct.ttype == MPTT_forkend);
        assert(ct.pno == pno);
        parsctx->cti = ct.next[0];
        nv =  _build_graph(parsctx, pmctx, prevnpp, tsrctx);
        parsctx->cti = parsctx->task_l[parsctx->cti].next[0];
        nv->np = _build_graph(parsctx, pmctx, &nv->np, tsrctx);
        assert(parsctx->cti);
        return nv;
    }

    TaskNo ret_ti, fork_ti;
    fork_ti = parsctx->cti;

    nv = PMV_create(pmctx, PMV_insc, prevnpp);
    assert(nv);

    parsctx->cti = ct.next[0];
    nv->pp = _build_graph(parsctx, pmctx, &nv->pp, tsrctx);
    assert(parsctx->cti);
    if (!nv->pp) {
        printf(
            "[Fatal][PMV][create_insc]: on fork %lu, parent branch empty.\n",
            fork_ti);
        assert(0);
    }
    ret_ti = parsctx->cti;

    parsctx->cti = ct.next[1];
    nv->cp = _build_graph(parsctx, pmctx, &nv->cp, tsrctx);
    assert(parsctx->cti);
    if (!nv->cp) {
        printf(
            "[Fatal][PMV][create_insc]: on fork %lu, child branch empty.\n",
            fork_ti);
        assert(0);
    }

    if (ret_ti != parsctx->cti) {
        printf(
            "[Fatal][PMV][create_insc]: on fork %lu(lno=%lu), branches don't meet:"
            "parent join=%lu(lno=%lu), child join=%lu(lno=%lu)\n",
            fork_ti, parsctx->task_l[fork_ti].lno,
            ret_ti, parsctx->task_l[ret_ti].lno,
            parsctx->cti, parsctx->task_l[parsctx->cti].lno);
        assert(0);
    }
    parsctx->cti = parsctx->task_l[ret_ti].next[0];

    nv->np = _build_graph(parsctx, pmctx, &nv->np, tsrctx);
    assert(parsctx->cti);

    return nv;
}


static PMV* _build_graph(
    MParser *parsctx,
    PMContext *pmctx,
    PMV **prevnpp,
    TaskSegRawCtx *tsrctx)
{
    MPTask ct = parsctx->task_l[parsctx->cti];
    PMV *nv = NULL;

    switch(ct.ttype) {
    case MPTT_fork:
        nv = _create_insc(parsctx, pmctx, prevnpp, tsrctx);
        assert(parsctx->cti);
        break;
    case MPTT_calc:
    case MPTT_com:
        nv = _create_seg(parsctx, pmctx, prevnpp, tsrctx);
        assert(parsctx->cti);
        break;

    case MPTT_forkend:
        parsctx->cti = parsctx->task_l[parsctx->cti].next[0];
        return _build_graph(parsctx, pmctx, prevnpp, tsrctx);

    case MPTT_join:
    case MPTT_end:
        break;
    case MPTT_start:
        printf(
            "[Fatal][PMV][_build_graph]: task %lu, is of type 'start'.\n",
            parsctx->cti);
        assert(0);
        break;
    default:
        /* Should never occur */
        assert(0);
    }

   return nv;
}

/* Build a PPM graph from a parser context.
 * @param parsctx pointer to a parser context that contains the parsed model
 * @param pm_ctx PM context where the graph will be created
 * @param tsrctx TaskSegRaw context pointer for storing the segments */
int PMV_build_graph(MParser *parsctx, PMContext *pm_ctx, TaskSegRawCtx *tsrctx)
{
    assert(parsctx && pm_ctx);
    MPTask ct = parsctx->task_l[parsctx->head];

    if (ct.ttype != MPTT_start) return MP_err;
    if (!ct.next[0]) return MP_err;
    parsctx->cti = ct.next[0];
    pm_ctx->headp = _build_graph(parsctx, pm_ctx, &pm_ctx->headp, tsrctx);
    assert(pm_ctx->headp);
    PMV_eval_r(pm_ctx->headp, 1);

    return 0;
}

static void _PMV_destroy_r(PMV *pmv)
{
    if (!pmv) return;

    switch (pmv->type) {
    case PMV_seg:
        break;
    case PMV_insc:
        _PMV_destroy_r(pmv->pp);
        _PMV_destroy_r(pmv->cp);
        break;
    case PMV_wrap:
        _PMV_destroy_r(pmv->wp);
        break;
    default:
        assert(0);
    }
    _PMV_destroy_r(pmv->np);
    free(pmv);
}

/* Deinit and deallocate a context previously allocated by PMContext_create()
 * @param ctx pointer to a context*/
void PMContext_destroy(PMContext *ctx)
{
    if (!ctx) return;

    Object_deinit((Object*)&ctx->gctx);
    _PMV_destroy_r(ctx->headp);
    arll_destroy(ctx->segcontl);
    gplot_destroy(ctx->gplot);
    free(ctx);
}

static const PM_seg_summary pmsegsummary_zero = { 0 };

static TaskSeg_summary *_get_seg_summary(PMContext *ctx)
{
    assert(ctx);
    unsigned vcntl[PMV_enumsize];
    PMContext_get_vcnt(ctx, vcntl);
    unsigned segcnt = vcntl[PMV_seg];

    TaskSeg_summary *tss = malloc(sizeof(*tss) * segcnt);
    assert(tss);

    unsigned ci = 0;
    PMVG *currgp = (PMVG*)ctx->gctx._super.elem_dll;
    while (currgp) {
        if (currgp->cpmv.type == PMV_seg) {
            PMV **vpp;
            arll_rewind(currgp->vpl);
            while ((vpp = arll_next(currgp->vpl))) {
                assert(ci < segcnt);
                tss[ci++] = TaskSeg_eval(PMV_getseg(*vpp).segp);
            }
        }
        currgp = (PMVG*)currgp->_super.next_p;
    }
    assert(ci == segcnt);

    return tss;
}

/* Evaluate statistics of a PM context.
 * @param ctx pointer to a PM context
 * @return PM_seg_summary structure containing the statistics */
PM_seg_summary PMContext_eval(PMContext *ctx)
{
    assert(ctx);
    PM_seg_summary retval = pmsegsummary_zero;

    unsigned vcntl[PMV_enumsize];
    PMContext_get_vcnt(ctx, vcntl);
    unsigned segcnt = vcntl[PMV_seg];
    double *fl = malloc(sizeof(*fl) * segcnt);
    assert(fl);

    TaskSeg_summary *tssl = _get_seg_summary(ctx);
    assert(tssl);

    for (unsigned i = 0; i < TSTT_enumsize; i++) {
        for (unsigned si = 0; si < segcnt; si++) {
            fl[si] = tssl[si].devi_sum[i];
            retval.devi_sum_total[i] += fl[si];
        }
        retval.devi_sum_mean[i] = gsl_stats_mean(fl, 1, segcnt);
        retval.devi_sum_stddev[i] = gsl_stats_sd_m(fl, 1, segcnt, retval.devi_sum_mean[i]);

        for (unsigned si = 0; si < segcnt; si++) {
            fl[si] = tssl[si].devi_mean[i];
        }
        retval.devi_mean[i] = gsl_stats_mean(fl, 1, segcnt);
        retval.devi_mean_stddev[i] = gsl_stats_sd_m(fl, 1, segcnt, retval.devi_mean[i]);
        for (unsigned si = 0; si < segcnt; si++) {
            fl[si] = tssl[si].dict_size[i];
            retval.dict_size_total[i] += fl[si];
        }
        retval.dict_size_mean[i] = gsl_stats_mean(fl, 1, segcnt);

        for (unsigned si = 0; si < segcnt; si++) {
            if (tssl[si].sum[i]) {
                fl[si] = tssl[si].devi_sum[i] / tssl[si].sum[i];
            } else {
                fl[si] = 0;
            }
        }
        retval.seg_badness_mean[i] = gsl_stats_mean(fl, 1, segcnt);

        for (unsigned si = 0; si < segcnt; si++) {
            if (tssl[si].avg[i]) {
                fl[si] = tssl[si].devi_mean[i] / tssl[si].avg[i];
            } else {
                fl[si] = 0;
            }
        }
        retval.task_badness_mean[i] = gsl_stats_mean(fl, 1, segcnt);
    }

    free(tssl);
    free(fl);

    return retval;
}

//void _check_tree(PMV *head) {
//    if (!head) return;
//
//    switch(head->type) {
//    case PMV_seg:
//        break;
//    case PMV_insc:
//        _check_tree(head->pp);
//        _check_tree(head->cp);
//        break;
//    case PMV_wrap:
//        _check_tree(head->wp);
//        break;
//    default:
//        assert(0);
//    }
//}
//
//void PMContext_check(PMContext *ctx) {
//    assert(ctx);
//    _check_tree(ctx->headp);
//}

static PMVG* _link_groups(PMV *vp)
{
    if (!vp) return NULL;
    /* if pointers not null, it means the node was already visited, so descends
     * can be spared */
    PMVG *gp = vp->gp;
    switch (vp->type) {
    case PMV_seg:
        break;

    case PMV_insc:
        if (!gp->cpmv.pp) gp->cpmv.pp = _link_groups(vp->pp);
        if (!gp->cpmv.cp) gp->cpmv.cp = _link_groups(vp->cp);
        break;

    case PMV_wrap:
        if (!gp->cpmv.wp) gp->cpmv.wp = _link_groups(vp->wp);
        break;

    default:
        assert(0);
        break;
    }

    if (!gp->cpmv.np) gp->cpmv.np = _link_groups(vp->np);
    return gp;
}

/* Link the PMV groups of a PPM graph. The resulting graph represents the
 * compressed graph.
 * @param ctx pointer to a PM context*/
void PM_link_groups(PMContext *ctx)
{
    assert(ctx);
    _link_groups(ctx->headp);
}

typedef struct __attribute__((__packed__)) {
    uint32_t size;
    /* here start 'size' packed segconts */
} SegcontList_pckd;

typedef struct __attribute__((__packed__)) {
    uint32_t segid;
    uint32_t pid;
} Segcont_pckd;

static unsigned _segcont_l_pack(
    PMV *vp,
    Segcont_pckd *contl_pck,
    unsigned i)
{
    if (!vp) return i;

    Segcont cont;
    switch (vp->type) {
    case PMV_seg:
        cont = PMV_getseg(vp);
        contl_pck[i].pid = cont.pid;
        contl_pck[i].segid = ((Elem*)cont.segp)->idx;
        i++;
        break;

    case PMV_insc:
        i = _segcont_l_pack(vp->pp, contl_pck, i);
        i = _segcont_l_pack(vp->cp, contl_pck, i);
        break;

    case PMV_wrap:
        i = _segcont_l_pack(vp->wp, contl_pck, i);
        break;

    default:
        assert(0);
    }

    return _segcont_l_pack(vp->np, contl_pck, i);
}

/*
 * File structure: [ Segment container data ]
 *
 * [ Segment container context ][ Segcont 1 ] ... [ Segcont c ]
 * [ 4 B                       ][ 8 B       ] ... [ 8 B       ]
 *
 * [ Segment container context ] = [ Number of containers c]
 * [ 4 B                       ]   [ 4 B                   ]
 *                                 [ unsigned              ]
 *
 * [ Segcont i ] = [ Segment index ][ Process ID ]
 * [ 8 B       ]   [ 4 B           ][ 4 B        ]
 *                 [ unsigned      ][ unsigned   ] */
static int _segcont_l_to_file(PMContext *ctx, FILE *wfp, TaskSegCtx *segctx)
{
    SegcontList_pckd lpckd;
    lpckd.size = arll_len(ctx->segcontl);

    int tot_len = 0;

    assert(fwrite(&lpckd, sizeof(lpckd), 1, wfp) == 1);
    tot_len += sizeof(lpckd);


    Segcont_pckd *contl = malloc(sizeof(*contl) * lpckd.size);
    assert(contl);

    ElemCtx_assign_idx((ElemCtx*)segctx);
    assert(_segcont_l_pack(ctx->headp, contl, 0) == lpckd.size);

    assert(fwrite(contl, sizeof(*contl), lpckd.size, wfp) == lpckd.size);
    tot_len += sizeof(*contl) * lpckd.size;

    free(contl);
    return tot_len;
}

typedef struct __attribute__((__packed__)) {
    uint8_t type;
    int32_t ni;
    union {
        struct {
            int32_t pi;
            int32_t ci;
        };
        int32_t wi;
    };
} PMVG_pckd;

typedef struct __attribute__((__packed__)) {
    uint32_t size;
} PMVGCtx_pckd;

/*
 * File structure: [ PPM tree data ]
 *
 * [ PPM context ][Vertex 1] ... [Vertex v]
 * [ 4 B         ][ 13 B   ] ... [ 13 B   ]
 *
 * [ PPM context ] = [ Number of vertices v ]
 * [ 4 B         ]   [ 4 B                  ]
 *                   [ unsigned             ]
 *
 * [ Vertex i    ] = [ Type ][ Next vertex index ][ Vertex type-specific data ]
 * [ 13 B        ]   [ 1 B  ][ 4 B               ][ 8 B                       ]
 *
 * Vertex type-specific data:
 *
 * [ Segment vertex specific data ] = [ <NONE> ]
 * [ 8 B                          ]   [ 8 B    ]
 *                                    [ <NONE> ]
 *
 * [ Inosculation vertex specific data ] = [ Parent vertex index ][ Child vertex index ]
 * [ 8 B                               ]   [ 4 B                 ][ 4 B                ]
 *                                         [ unsigned            ][ unsigned           ]
 *
 * [ Wrapper vertex specific data ] = [ Wrapped vertex index ][ <NONE> ]
 * [ 8 B                          ]   [ 4 B                  ][ 4 B    ]
 *                                    [ unsigned             ][ <NONE> ] */
static int _pmvg_ctx_to_file(PMContext *ctx, FILE *wfp)
{
    assert(ctx);
    ElemCtx_assign_idx((ElemCtx*)&ctx->gctx);
    PM_link_groups(ctx);

    int tot_len = 0;

    PMVGCtx_pckd ctxpckd;
    ctxpckd.size = ((ElemCtx*)&ctx->gctx)->size;

    assert(fwrite(&ctxpckd, sizeof(ctxpckd), 1, wfp) == 1);
    tot_len += sizeof(ctxpckd);

    PMVG_pckd *pmvgl = malloc(sizeof(*pmvgl) * ctxpckd.size);
    assert(pmvgl);

    Elem *ep = ((ElemCtx*)&ctx->gctx)->elem_dll;
    unsigned i = 0;
    while (ep) {
        PMVG *gp = (PMVG*)ep;
        pmvgl[i].type = gp->cpmv.type;
        pmvgl[i].ni = gp->cpmv.np ? ((Elem*)gp->cpmv.np)->idx : -1;

        switch (gp->cpmv.type) {
        case PMV_seg:
            break;
        case PMV_insc:
            pmvgl[i].pi = gp->cpmv.pp ? ((Elem*)gp->cpmv.pp)->idx : -1;
            pmvgl[i].ci = gp->cpmv.cp ? ((Elem*)gp->cpmv.cp)->idx : -1;
            break;
        case PMV_wrap:
            pmvgl[i].wi = gp->cpmv.wp ? ((Elem*)gp->cpmv.wp)->idx : -1;
            break;
        default:
            assert(0);
        }

        ep = ep->next_p;
        i++;
    }

    assert(fwrite(pmvgl, sizeof(*pmvgl), ctxpckd.size, wfp) == ctxpckd.size);
    tot_len += sizeof(*pmvgl) * ctxpckd.size;

    free(pmvgl);
    return tot_len;
}

/*
 * File structure: [PPM data]
 *
 * [ Segment container data ][ PPM tree data ] */
int PMContext_to_file(PMContext *ctx, FILE *wfp, TaskSegCtx *segctx)
{
    assert(ctx && wfp && segctx);
    int tot_len = 0;
    tot_len += _segcont_l_to_file(ctx, wfp, segctx);
    tot_len += _pmvg_ctx_to_file(ctx, wfp);

    return tot_len;
}

