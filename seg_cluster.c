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

#include "seg_cluster.h"
#include "TaskSegBuck.h"
#include "TaskSegRaw.h"
#include "stdio.h"
#include "gplot.h"

static const SegClusterCtx segclusterctx_zero = { 0 };
static const SegCluster segcluster_zero = { 0 };

enum {
    PF_reqcom_raweval,
    PF_reqcom_raw,
    PF_reqcom_comp,
    PF_dictcom,
    PF_reqcal_raweval,
    PF_reqcal_raw,
    PF_reqcal_comp,
    PF_dictcal,

    PF_enumsize
};
static const char * const plot_files[PF_enumsize] = {
    "seg_plots/reqcom_raw_eval.dat",
    "seg_plots/reqcom_raw.dat",
    "seg_plots/reqcom_comp.dat",
    "seg_plots/dictcom.dat",
    "seg_plots/reqcal_raw_eval.dat",
    "seg_plots/reqcal_raw.dat",
    "seg_plots/reqcal_comp.dat",
    "seg_plots/dictcal.dat"
};

enum {
    PP_com,
    PP_cal,

    PP_enumsize
};
static const char * const plot_pipes[PP_enumsize] = {
    "gnuplot",
    "gnuplot"
};

static int SegCluster_init(SegCluster *clp)
{
    assert(clp);
    *clp = segcluster_zero;
    clp->segv_arll = arll_construct(sizeof(PMV*), 1);
    assert(clp->segv_arll);

    return 0;
}

static int SegClusterCtx_add(SegClusterCtx *ctx, PMV *segvp)
{
    SegCluster *clp;
    assert(segvp);

    arll_rewind(ctx->cluster_arll);
    while ((clp = (SegCluster*)arll_next(ctx->cluster_arll))) {
        arll_rewind(clp->segv_arll);

        PMV **repvpp = arll_next(clp->segv_arll);
        TaskSeg *repseg = PMV_getseg(*repvpp).segp;
        assert(repseg);
        TaskSeg *cseg = PMV_getseg(segvp).segp;
        assert(cseg);

        if (TaskSeg_compar(repseg, cseg)) {
            arll_push(clp->segv_arll, &segvp);
            return 0;
        }
    }

    SegCluster ncl;
    assert (SegCluster_init(&ncl) == 0);
    assert (arll_push(ncl.segv_arll, &segvp) != -1);
    assert (arll_push(ctx->cluster_arll, &ncl) != -1);

    return 0;
}

/* Create and init a segment cluster context. The segments are further grouped
 * into clusters. Segments that are considered equal by TaskSeg_compar() are
 * part of the same cluster. Note that the definition for equality depends on
 * the class of segments.
 * @param segv_grp pointer to the segment group
 * @param k bucketing threshold k
 * @param plot for debugging. Set to 1 to plot clusters using gnuplot, 0
 *  otherwise.
 * @return pointer to the new context, NULL otherwise */
SegClusterCtx *SegClusterCtx_create(
    PMVG *segv_grp,
    double k,
    char plot)
{
    assert(segv_grp);

    SegClusterCtx *nctx = malloc(sizeof(*nctx));
    assert(nctx);
    *nctx = segclusterctx_zero;

    nctx->k = k;
    nctx->segv_grp = segv_grp;
    nctx->cluster_arll = arll_construct(sizeof(SegCluster), 1);
    assert(nctx->cluster_arll);

    if (plot) {
        nctx->gplp = gplot_create(plot_pipes, plot_files, PP_enumsize, PF_enumsize);
        assert(nctx->gplp);
    }

    PMV **vpp;
    arll_rewind(segv_grp->vpl);
    while ((vpp = arll_next(segv_grp->vpl))) {
        assert(!SegClusterCtx_add(nctx, *vpp));
    }

    return nctx;
}

static void _export_plot(
    SegClusterCtx *ctx,
    TaskSeg *tsraw,
    TaskSeg *tsraw_eval,
    TaskSeg *tsbuck,
    TCDict *calc_dict,
    TCDict *com_dict,
    unsigned cluster_size)
{
    gplot_reset_all(ctx->gplp);

    TaskSeg_export(tsraw_eval, gplot_getf(ctx->gplp, PF_reqcal_raweval), TSTT_calc);
    TaskSeg_export(tsraw_eval, gplot_getf(ctx->gplp, PF_reqcom_raweval), TSTT_com);
    TaskSeg_export(tsraw, gplot_getf(ctx->gplp, PF_reqcal_raw), TSTT_calc);
    TaskSeg_export(tsraw, gplot_getf(ctx->gplp, PF_reqcom_raw), TSTT_com);
    TaskSeg_export(tsbuck, gplot_getf(ctx->gplp, PF_reqcal_comp), TSTT_calc);
    TaskSeg_export(tsbuck, gplot_getf(ctx->gplp, PF_reqcom_comp), TSTT_com);
    TCDict_export(calc_dict, gplot_getf(ctx->gplp, PF_dictcal));
    TCDict_export(com_dict, gplot_getf(ctx->gplp, PF_dictcom));

    fprintf(
        gplot_getp(ctx->gplp, PP_cal),
        "set title \"Calculation k=%f cluster size=%u\"\n", ctx->k, cluster_size);
    fprintf(
        gplot_getp(ctx->gplp, PP_cal),
        "plot '%s' using 1:(0) title \"raw weights eval\" with points pointtype 1 ps 2 lc rgb \"#E0FF0000\", "
        "'%s' using 1:(1) title \"raw weights\" with points pointtype 1 ps 2 lc rgb \"#E00000FF\", "
        "'%s' using 1:(2) title \"comp weights\" with points pointtype 1 ps 2 lc rgb \"#E000FF00\", "
        "'%s' using 1:(3) title \"bucket supremum\" with points pointtype 10 ps 3 lc rgb \"magenta\","
        "'%s' using 2:(4) title \"bucket mean\" with points pointtype 10 ps 3 lc rgb \"cyan\"\n",
        plot_files[PF_reqcal_raweval],
        plot_files[PF_reqcal_raw],
        plot_files[PF_reqcal_comp],
        plot_files[PF_dictcal],
        plot_files[PF_dictcal]);
    fprintf(
        gplot_getp(ctx->gplp, PP_cal),
        "set title\n");
    fflush(gplot_getp(ctx->gplp, PP_cal));

    fprintf(
        gplot_getp(ctx->gplp, PP_com),
        "set title \"Communication k=%f cluster size=%u\"\n", ctx->k, cluster_size);
    fprintf(
        gplot_getp(ctx->gplp, PP_com),
        "plot '%s' using 1:(0) title \"raw weights eval\" with points pointtype 1 ps 2 lc rgb \"#E0FF0000\", "
        "'%s' using 1:(1) title \"raw weights\" with points pointtype 1 ps 2 lc rgb \"#E00000FF\", "
        "'%s' using 1:(2) title \"comp weights\" with points pointtype 1 ps 2 lc rgb \"#E000FF00\", "
        "'%s' using 1:(3) title \"bucket supremum\" with points pointtype 10 ps 3 lc rgb \"magenta\","
        "'%s' using 2:(4) title \"bucket mean\" with points pointtype 10 ps 3 lc rgb \"cyan\"\n",
        plot_files[PF_reqcom_raweval],
        plot_files[PF_reqcom_raw],
        plot_files[PF_reqcom_comp],
        plot_files[PF_dictcom],
        plot_files[PF_dictcom]);
    fprintf(
        gplot_getp(ctx->gplp, PP_com),
        "set title\n");
    fflush(gplot_getp(ctx->gplp, PP_com));
}

/* Converts the clusters of TaskSegRaw segments into clusters of TaskSegBuck
 * segments. One dictionary is created for each cluster.The raw segments are not 
 * removed from their segment context.
 * @param clctx pointer to the segment cluster context
 * @param tsrctx pointer to the TaskSegRaw context
 * @param tsbctx pointer to the TaskSegBuck context
 * @param dctx pointer to the dictionary context */
void SegClusterCtx_compress(
    SegClusterCtx *clctx,
    TaskSegRawCtx *tsrctx,
    TaskSegBuckCtx *tsbctx,
    TCDictCtx *dctx)
{
    assert(clctx);
    SegCluster *clp;

    arll_rewind(clctx->cluster_arll);
    while((clp = arll_next(clctx->cluster_arll))) {
        TaskSegRaw *eval_seg = _obj_alloc(sizeof(TaskSegRaw));
        assert(!TaskSegRaw_init(tsrctx, eval_seg));

        PMV **vpp;
        arll_rewind(clp->segv_arll);
        while((vpp = arll_next(clp->segv_arll))) {
            assert(_obj_vmtp(PMV_getseg(*vpp).segp) == (Object_VMT*)&TaskSegRaw_vmt);
            assert (TSR_merge(eval_seg, (TaskSegRaw*)PMV_getseg(*vpp).segp) == TSR_ok);
        }

        TaskSeg_reql *reql = TaskSeg_to_reql((TaskSeg*)eval_seg, 1);
        assert(reql);

        TCDict *calc_dictp = _obj_alloc(sizeof(TCDict));
        TCDict *com_dictp = _obj_alloc(sizeof(TCDict));
        assert(!TCDict_init(
            dctx,
            calc_dictp,
            reql->reql[TSTT_calc],
            reql->reql_siz[TSTT_calc],
            clctx->k,
            1 << 15));

        assert(!TCDict_init(
            dctx,
            com_dictp,
            reql->reql[TSTT_com],
            reql->reql_siz[TSTT_com],
            clctx->k,
            1 << 15));

        TaskSeg_reql_destroy(reql);

        arll_rewind(clp->segv_arll);
        while((vpp = arll_next(clp->segv_arll))) {
            TaskSegBuck *ntsb = _obj_alloc(sizeof(TaskSegBuck));

            assert(!TSB_init(
                tsbctx,
                ntsb,
                calc_dictp,
                com_dictp,
                (TaskSegRaw*)PMV_getseg(*vpp).segp));

            if (clctx->gplp)
                _export_plot(
                    clctx,
                    PMV_getseg(*vpp).segp,
                    (TaskSeg*)eval_seg,
                    (TaskSeg*)ntsb,
                    calc_dictp,
                    com_dictp,
                    arll_len(clp->segv_arll));

            PMV_setseg(*vpp, (TaskSeg*)ntsb);
        }

        Object_deinit((Object*)eval_seg);
        free(eval_seg);
    }
}

/* For all the clusters in the context, replaces the segments of the vertices in
 * a cluster with a single representative from the cluster. The replaced
 * segments are also removed from their segment context.
 * @param ctx pointer to segment cluster context*/
void SegClusterCtx_remdupl(SegClusterCtx *ctx)
{
    assert(ctx);
    arll_rewind(ctx->cluster_arll);
    SegCluster *clp;
    while ((clp = arll_next(ctx->cluster_arll))) {
        arll_rewind(clp->segv_arll);
        PMV **repvpp = arll_next(clp->segv_arll);
        assert(repvpp);
        PMV **vpp;

        while ((vpp = arll_next(clp->segv_arll))) {
            Object_deinit((Object*)PMV_getseg(*vpp).segp);
            _obj_free(PMV_getseg(*vpp).segp);
            PMV_setseg(*vpp, PMV_getseg(*repvpp).segp);
            assert(PMV_getseg(*vpp).segp);
        }
    }
}

/* For debugging. Print a cluster context.
 * @param ctx pointer to segmenct cluster context */
void SegClusterCtx_print(SegClusterCtx *ctx)
{
    assert(ctx);
    printf("Cluster ctx: Group ID=%03u\n", ctx->segv_grp->id);

    arll_rewind(ctx->cluster_arll);
    SegCluster *clp;
    while ((clp = (SegCluster*)arll_next(ctx->cluster_arll))) {
        printf("Cluster %03u:\n", arll_get_nexti(ctx->cluster_arll) - 1);

        PMV **vpp;
        arll_rewind(clp->segv_arll);
        while ((vpp = arll_next(clp->segv_arll))) {
            printf("\t\tSeg PID=%03d", PMV_getseg(*vpp).pid);
            TaskSeg_print(PMV_getseg(*vpp).segp);
        }
    }
}

/* Deinit and deallocate a segment cluster context.
 * @param ctx pointer to a segmenct cluster context */
void SegClusterCtx_destroy(SegClusterCtx *ctx)
{
    if (!ctx) return;

    arll_rewind(ctx->cluster_arll);
    SegCluster *clp;
    while ((clp = arll_next(ctx->cluster_arll))) {
        arll_destroy(clp->segv_arll);
    }
    arll_destroy(ctx->cluster_arll);
}

/* @param pointer to a segment cluster context
 * @return number of clusters*/
unsigned SegClusterCtx_size(SegClusterCtx *ctx)
{
    assert(ctx);
    return arll_len(ctx->cluster_arll);
}
