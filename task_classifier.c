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

#include "task_classifier.h"
#include <stdlib.h>
#include <gsl/gsl_statistics_double.h>
#include <assert.h>
#include <fenv.h>
#include <math.h>
#include <stdio.h>

typedef struct Bucket {
    double           mean;
    double           supremum;
    double           stddev;
    unsigned        size;
    struct Bucket   *lowbp;
    struct Bucket   *highbp;
} Bucket;

static const Bucket bucket_empty = { 0 };
//static const TCDict tcdict_empty = { 0 };
static void TCDict_deinit(Object *objp);


const TCDict_VMT TCDict_vmt = {
    ._super._object_deinit = TCDict_deinit,
};

/* Returns the index of the first element i > 'n'. If no such element is found,
 * 'size' is returned. */
static unsigned bsearch_upbound(
    double n,
    const double *list,
    unsigned size)
{
    int ci;
    if (size == 1) {
        if (list[0] > n) return 0;
        return 1;
    }

    ci = size / 2;
    if (list[ci] > n) {
        ci = bsearch_upbound(n, list, ci);
    } else {
        ci += bsearch_upbound(n, list + ci, size - ci);
    }
    return ci;
}

/* Returns the index of the first element i >= 'n'. If no such element is found,
 * 'size' is returned. */
static unsigned bsearch_supremum(
    double n,
    const double *list,
    unsigned size)
{
    int ci;
    if (size == 1) {
        if (list[0] >= n) return 0;
        return 1;
    }

    ci = size / 2;
    if (list[ci] >= n) {
        ci = bsearch_supremum(n, list, ci);
    } else {
        ci += bsearch_supremum(n, list + ci, size - ci);
    }
    return ci;
}

static void Buck_destroy(Bucket *buckp)
{
    if (!buckp) return;
    Buck_destroy(buckp->highbp);
    Buck_destroy(buckp->lowbp);
    free(buckp);
}

static Bucket* Buck_create(
    const double *reql,
    unsigned size,
    double k)
{
    assert(k > 0.0);
    assert(size);

    double mean, stddev;
    Bucket *nbuckp = malloc(sizeof(Bucket));
    if (!nbuckp) goto buck_create_err;
    *nbuckp = bucket_empty;

    nbuckp->supremum    = reql[size - 1];

    mean = gsl_stats_mean(reql, 1, size);
    nbuckp->mean        = mean;
    if (mean == 0.0) goto buck_create_err;

    /* sd would divide by 0 if 'size' == 1 */
    stddev = size > 1 ? gsl_stats_sd_m(reql, 1, size, mean) : 0.0;
    nbuckp->stddev      = stddev;

    double bad = stddev / mean;
    if (bad > k) {
        assert(size > 1);
        unsigned upbound = bsearch_upbound(nbuckp->mean, reql, size);
        assert(upbound < size);

        nbuckp->lowbp   = Buck_create(reql, upbound, k);
        if (!nbuckp->lowbp) goto buck_create_err;

        nbuckp->highbp  = Buck_create(reql + upbound, size - upbound, k);
        if (!nbuckp->lowbp) goto buck_create_err;

        nbuckp->size = nbuckp->lowbp->size + nbuckp->highbp->size;

    } else {
        nbuckp->size = 1;
    }

    return nbuckp;

buck_create_err:
    Buck_destroy(nbuckp);
    return NULL;
}

static void Buck_to_dict(const Bucket *buckp, TCDict dict)
{
    if (buckp->size > 1) {
        dict.size           = buckp->lowbp->size;
        Buck_to_dict(buckp->lowbp, dict);

        dict.size           =  buckp->highbp->size;
        dict.mean_l         += buckp->lowbp->size;
        dict.supremum_l     += buckp->lowbp->size;
        Buck_to_dict(buckp->highbp, dict);

    } else {
        dict.mean_l[0]      = buckp->mean;
        dict.supremum_l[0]  = buckp->supremum;
    }
}

static void TCDict_deinit(Object *objp)
{
    TCDict *tcdictp = (TCDict*)objp;
    if (!tcdictp) return;
    free(tcdictp->mean_l);
    free(tcdictp->supremum_l);

    _Elem_deinit(objp);
}

int TCDict_init(
    TCDictCtx *ctx,
    TCDict *tcdictp,
    const double *reql,
    unsigned reql_siz,
    double k,
    unsigned max_dict_siz)
{
    assert(ctx);
    assert(reql);
    assert(tcdictp);

    assert(!Elem_init((ElemCtx*)ctx, (Elem*)tcdictp));

    Bucket *buckp = NULL;
    _obj_vmtp(tcdictp) = (Object_VMT*)&TCDict_vmt;

    if (reql_siz == 0) {
        /* an empty dict */
        return 0;
    }

    buckp = Buck_create(reql, reql_siz, k);
    if (!buckp) {
        goto tcdict_gen_err;
    }

    if (buckp->size > max_dict_siz) {
        goto tcdict_gen_err;
    }

    tcdictp->size       = buckp->size;
    tcdictp->mean_l     = malloc(sizeof(*tcdictp->mean_l) * tcdictp->size);
    tcdictp->supremum_l = malloc(sizeof(*tcdictp->supremum_l) * tcdictp->size);
    if (!tcdictp->mean_l || !tcdictp->supremum_l) goto tcdict_gen_err;

    Buck_to_dict(buckp, *tcdictp);
    Buck_destroy(buckp); buckp = NULL;

    return 0;

tcdict_gen_err:
    assert(0);
    return -1;
}

int TCDictCtx_init(TCDictCtx *ctx)
{
    assert(ctx);
    assert(!ElemCtx_init((ElemCtx*)ctx));
    return 0;
}

void TCDict_print(const TCDict *tcdictp)
{
    assert(tcdictp);

    for (unsigned i = 0; i < tcdictp->size; i++) {
        printf("%03u\t\t", i);
    }
    printf("\n\t\t");

    for (unsigned i = 0; i < tcdictp->size; i++) {
        printf("%012.00f\t", tcdictp->mean_l[i]);
    }
    printf("\n\t\t");

    for (unsigned i = 0; i < tcdictp->size; i++) {
        printf("%012.00f\t", tcdictp->supremum_l[i]);
    }
    printf("\n");
}

TCKey TCDict_key_from_val(const TCDict *ctx, TCVal val)
{
    assert(ctx);
    if (ctx->size == 0) return TCKey_INVALID;
    unsigned key = bsearch_supremum(val, ctx->supremum_l, ctx->size);
    if (key == ctx->size) return TCKey_INVALID;
    return (TCKey)key;
}

TCVal TCDict_val_from_key(const TCDict *ctx, TCKey key)
{
    assert(ctx);
    if (!TCKey_is_valid(key)) return TCVal_INVALID;
    if ((unsigned)key >= ctx->size) return TCVal_INVALID;
    return ctx->mean_l[key];
}

void TCDict_export(const TCDict *tcdictp, FILE *fp)
{
    assert(tcdictp && fp);

    fprintf(fp, "# supremum, mean\n");
    for (unsigned i = 0; i < tcdictp->size; i++) {
        fprintf(fp, "%f, %f\n", tcdictp->supremum_l[i], tcdictp->mean_l[i]);
    }
    fflush(fp);
}

/*
 * File structure: [ Dict i]
 *
 * [ Dict metadata ][ Supremum list ][ Average list ]
 * [ 4 B           ][ m * 8 B       ][ m * B        ]
 *
 * [ Dict metadata ] = [ Dict size ]
 * [ 4 B           ]   [ 4 B       ]
 *                     [ unsigned  ]
 *
 * [ Supremum list ] = [ Sup 1  ] ... [ Sup m  ]
 * [ m * 8 B       ] = [ 8 B    ]     [ 8 B    ]
 *                     [ double ]     [ double ]
 *
 * [ Average list ] = [ Avg 1  ] ... [ Avg m  ]
 * [ m * 8 B      ] = [ 8 B    ]     [ 8 B    ]
 *                    [ double ]     [ double ] */
static int _TCDict_to_file(TCDict *dictp, FILE *wfp)
{
    assert(dictp);

    int total_size = 0;
    TCDict_pckd dictpckd;
    dictpckd.size = dictp->size;

    assert(fwrite(&dictpckd, sizeof(dictpckd), 1, wfp) == 1);
    total_size += sizeof(dictpckd);

    assert(fwrite(
        dictp->supremum_l,
        sizeof(*dictp->supremum_l),
        dictp->size,
        wfp) == dictp->size);

    total_size += sizeof(*dictp->supremum_l) * dictp->size;
    assert(fwrite(
        dictp->mean_l,
        sizeof(*dictp->mean_l),
        dictp->size,
        wfp) == dictp->size);
    total_size += sizeof(*dictp->mean_l) * dictp->size;

    return total_size;
}

/*
 * File structure: [ Dictionary data ]
 *
 * [ Dict context ][ Dict 1] ... [Dict d]
 * [ 4 B          ][ ? B   ]     [ ? B  ]
 *
 * [ Dict context ] = [ Number of dictionaries d ]
 * [ 4 B          ]   [ 4 B                      ]
 *                    [ unsigned                 ] */
int TCDictCtx_to_file(TCDictCtx *ctx, FILE *wfp) {
    assert(ctx && wfp);
    int total_size = 0;

    TCDictCtx_pckd ctxpckd;
    ctxpckd.size = ((ElemCtx*)ctx)->size;

    assert(fwrite(&ctxpckd, sizeof(ctxpckd), 1, wfp) == 1);
    total_size += sizeof(ctxpckd);

    Elem *ep = ((ElemCtx*)ctx)->elem_dll;
    while (ep) {
        TCDict *dictp = (TCDict*)ep;
        total_size += _TCDict_to_file(dictp, wfp);

        ep = ep->next_p;
    }

    return total_size;
}
