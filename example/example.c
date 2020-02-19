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

#include <stdlib.h>
#include <stdio.h>
/* Wrapper header for all the methods available to the user */
#include "../inc/PPM_tools.h"

/* Bucket badness threshold k */
#define PARAM_K         0.04
/* Parameters for the segment similarity function */
#define PARAM_MU_MAX    1.25
#define PARAM_SIGMA_MAX 1.25

/* Input model file in human-readable format */
static const char model_input_fname[] = "model_test.txt";
/* Output binary model file (uncompressed) */
static const char model_out_fname_raw[] = "model_test_raw.dat";
/* Output binary model file (compressed) */
static const char model_out_fname_comp[] = "model_test_comp.dat";

int main(void)
{
    int retval;
    FILE *mod_inputf = fopen(model_input_fname, "r");

    if (!mod_inputf) {
        printf("Error opening input file %s\n", model_input_fname);
        return -1;
    }

    /* Declare and initialize the parsing context. */
    MParser parser_ctx;
    retval = MParser_init(&parser_ctx, mod_inputf, -1, -1);
    if (retval != MP_ok) {
        printf("Model parser: init failed with code %d.\n", retval);
        return -1;
    }

    /* Parse the model. After completion, the context will hold the PPM graph
     * as a DAG, with the tasks still as vertices. */
    retval = MParser_parse(&parser_ctx);
    if (retval != MP_ok) {
        printf("Model parser: parsing failed with code %d.\n", retval);
        return -1;
    }
    printf("Model parser: model input file %s parsed.\n", model_input_fname);

    fclose(mod_inputf);

    /* Create a PPM context. */
    PMContext *pm_ctx = PMContext_create();
    if (!pm_ctx) {
        printf("PM: Cannot create prediction model context.\n");
        return -1;
    }

    /* Allocate and initialize a context for the raw task segments. */
    TaskSegRawCtx *tsr_ctx = _obj_alloc(sizeof(TaskSegRawCtx));
    retval = TaskSegRawCtx_init(tsr_ctx, PARAM_MU_MAX, PARAM_SIGMA_MAX);
    if (retval) {
        printf("TaskSeg: cannot init TaskSegRaw context.\n");
        return -1;
    }

    /* Build the PPM tree from the DAG contained in the parser context. The
     * tasks, available as vertices, are encapsulated in raw segments. */
    retval = PMV_build_graph(&parser_ctx, pm_ctx, tsr_ctx);
    if (retval)  {
        printf("PM: cannot build prediction model tree.\n");
        return -1;
    }

    /* We don't need the parser context anymore, as all the operations are from
     * now on done on the PPM tree contained by the PM context ('pm_ctx'). */
    MParser_deinit(&parser_ctx);

    /* Gather some statistics about the uncompressed PPM. All the raw segments
     * in the model are tracked by their context ('tsr_ctx'). */
    double seglen_avg = TaskSegRaw_ctx_seg_meanlen(tsr_ctx);
    unsigned vcnt[PMV_enumsize];
    PMContext_get_vcnt(pm_ctx, vcnt);
    printf("PM: prediction model tree built:\n"
            "\ttotal vertex count=%u\n"
            "\tsegment count=%u\n"
            "\tsegment length (average)=%.02f\n",
            vcnt[PMV_seg] + vcnt[PMV_insc] + vcnt[PMV_wrap],
            vcnt[PMV_seg],
            seglen_avg);

    FILE *mod_outf_raw = fopen(model_out_fname_raw, "w");
    if (!mod_outf_raw) {
        printf("Error opening uncompressed destination file %s.\n",
                model_out_fname_raw);
        return -1;
    }

    /* Export the binary uncompressed model. */
    int fsize_raw =
            au_export_model(NULL, (TaskSegCtx*)tsr_ctx, pm_ctx, mod_outf_raw);
    if (fsize_raw < 0) {
        printf("Error exporting uncompressed model.\n");
        return -1;
    }
    printf("Uncompressed model exported to %s:\n"
            "\tsize=%.02f KB\n",
            model_out_fname_raw, fsize_raw / 1024.0);
    fclose(mod_outf_raw);

    /* Aimed minining. This has the scope of finding similar segments
     * candidates. All the vertices found to be roots of similar PPMs are
     * grouped together. */
    GM_mine_for_symm(pm_ctx);
    GM_mine_for_asymm(pm_ctx);
    GM_mine_recurrence(pm_ctx);

    /* The vertex groups also form the compressed PPM tree. */
    printf("Graph miner: pattern mining (aimed) complete:\n"
            "\tcompressed vertex count=%u\n",
            ((ElemCtx*)&pm_ctx->gctx)->size);

    /* Allocate and initialize the context for bucketized segments. */
    TaskSegBuckCtx *tsb_ctx = _obj_alloc(sizeof(TaskSegBuckCtx));
    retval = TaskSegBuckCtx_init(tsb_ctx);
    if (retval) {
        printf("TaskSeg: cannot init TaskSegBuck context.\n");
        return -1;
    }

    /* Allocate and initialize the context for the dictionaries used by the
     * bucketed segments. */
    TCDictCtx *dict_ctx = _obj_alloc(sizeof(TCDictCtx));
    retval = TCDictCtx_init(dict_ctx);
    if (retval) {
        printf("TCDict: cannot init TCDict context.\n");
        return -1;
    }

    /* The segments in segment vertex groups are forming candidates for similar
     * segments. */
    PMVG *gp = PMContext_get_grouplist(pm_ctx);
    while (gp) {
        if (gp->cpmv.type == PMV_seg) {
            /* For each segment vertex group, create clusters of similar
             * segments. */
            SegClusterCtx *cluster_ctx = SegClusterCtx_create(gp, PARAM_K, 0);
            if (!cluster_ctx) {
                printf("Seg Cluster: cannot create cluster.\n");
                return -1;
            }
            /* For each cluster, a dictionary is created. All the segments in
             * the cluster are then bucketized using this dictionary. The raw
             * segments in the PPM tree are then replaces with their bucketized
             * versions.*/
            SegClusterCtx_compress(cluster_ctx, tsr_ctx, tsb_ctx, dict_ctx);
            SegClusterCtx_destroy(cluster_ctx);
        }
        gp = (PMVG*)((Elem*)gp)->next_p;
    }

    /* We no longer need the raw segments, as the tree contains bucketized
     * segments now. */
    Object_deinit((Object*)tsr_ctx);
    _obj_free(tsr_ctx);

    /* Print some statistics about the bucketized segments. */
    PM_seg_summary segsumm = PMContext_eval(pm_ctx);
    printf("Seg cluster: segments bucketized:\n"
            "\tcalc: average task badness=%.03f, average dictionary size=%.02f\n"
            "\tcom: average task badness=%.03f, average dictionary size=%.02f\n",
            segsumm.task_badness_mean[TSTT_calc], segsumm.dict_size_mean[TSTT_calc],
            segsumm.task_badness_mean[TSTT_com], segsumm.dict_size_mean[TSTT_com]);

    /* The segments in a segment vertex group are now forming candidates for
     * equivalent segments. */
    gp = PMContext_get_grouplist(pm_ctx);
    while (gp) {
        if (gp->cpmv.type == PMV_seg) {
            /* For each segment vertex group, create clusters of equivalent
             * segments. */
            SegClusterCtx *cluster_ctx = SegClusterCtx_create(gp, PARAM_K, 0);
            if (!cluster_ctx) {
                printf("Seg Cluster: cannot create cluster.\n");
                return -1;
            }
            /* Since all the segments in a cluster are forming an equivalence
             * class, only one representative is required to be preserved. This
             * step removes all the duplicates and updates the segment vertices
             * in the PPM tree accordingly. */
            SegClusterCtx_remdupl(cluster_ctx);
            SegClusterCtx_destroy(cluster_ctx);
        }
        gp = (PMVG*)((Elem*)gp)->next_p;
    }

    /* The representatives preserved above are available trough their context. */
    unsigned segcnt_comp = ((ElemCtx*)tsb_ctx)->size;
    printf("Seg cluster: duplicate segments removed:\n"
            "\tsegment count=%u\n", segcnt_comp);

    FILE *mod_outf_comp = fopen(model_out_fname_comp, "w");
    if (!mod_outf_comp) {
        printf("Error opening compressed destination file %s.\n",
                model_out_fname_comp);
        return -1;
    }

    /* Export the binary uncompressed model.*/
    int fsize_comp =
            au_export_model(dict_ctx, (TaskSegCtx*)tsb_ctx, pm_ctx, mod_outf_comp);
    if (fsize_comp < 0) {
        printf("Error exporting uncompressed model.\n");
        return -1;
    }

    printf("Compressed model exported to %s:\n"
            "\tsize=%.02f KB\n"
            "\tcompression rate=%.02f\n",
            model_out_fname_comp,
            fsize_comp / 1024.0,
            (float)fsize_raw / fsize_comp);

    fclose(mod_outf_comp);

    Object_deinit((Object*)tsb_ctx);
    _obj_free(tsb_ctx);
    Object_deinit((Object*)dict_ctx);
    _obj_free(dict_ctx);
    PMContext_destroy(pm_ctx);

    return 0;
}
