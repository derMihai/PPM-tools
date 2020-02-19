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

#ifndef SEG_CLUSTER_H_
#define SEG_CLUSTER_H_

#include "arll.h"
#include "pm.h"
#include "gplot.h"
#include "TaskSegBuck.h"

typedef struct {
    arll *segv_arll;
} SegCluster;

typedef struct {
    arll *cluster_arll;
    PMVG *segv_grp;
    double k;
//    double mu_max;
//    double sigma_max;
    /* for debugging */
    gnuplot *gplp;
} SegClusterCtx;

SegClusterCtx *SegClusterCtx_create(
    PMVG *seg_grp,
    double k,
    char plot);
void SegClusterCtx_compress(
    SegClusterCtx *clctx,
    TaskSegRawCtx *tsrctx,
    TaskSegBuckCtx *tsbctx,
    TCDictCtx *dctx);
void SegClusterCtx_remdupl(SegClusterCtx *ctx);
unsigned SegClusterCtx_size(SegClusterCtx *ctx);
void SegClusterCtx_destroy(SegClusterCtx *ctx);

#endif /* SEG_CLUSTER_H_ */
