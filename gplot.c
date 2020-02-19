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
#include <unistd.h>
#include "gplot.h"

static const gnuplot gnuplot_zero = { 0 };

// TODO: take a folder argument and create it if needed
gnuplot *gplot_create(
    const char *const *pipe_run_l,
    const char *const *file_name_l,
    unsigned pipe_cnt,
    unsigned file_cnt)
{
    assert(pipe_run_l && file_name_l);
    gnuplot *ngp = malloc(sizeof(*ngp));
    assert(ngp);
    *ngp = gnuplot_zero;

    ngp->pipe_lsiz = pipe_cnt;
    ngp->file_lsiz = file_cnt;
    assert((ngp->pipe_l = malloc(sizeof(*ngp->pipe_l) * pipe_cnt)));
    assert((ngp->file_l = malloc(sizeof(*ngp->file_l) * file_cnt)));

    for (unsigned i = 0; i < pipe_cnt; i++) {
        assert(pipe_run_l[i]);

        ngp->pipe_l[i] = popen(pipe_run_l[i], "w");
        assert(ngp->pipe_l[i]);
    }

    for (unsigned i = 0; i < file_cnt; i++) {
        assert(file_name_l[i]);

        ngp->file_l[i] = fopen(file_name_l[i], "w");
        assert(ngp->file_l[i]);
    }

    return ngp;
}
//void gplot_plot(gnuplot *ctx, unsigned pipe_i, const char *plot_cmd);
int gplot_reset(gnuplot *ctx, unsigned file_i)
{
    assert(ctx);
    assert(file_i < ctx->file_lsiz);

    assert(ftruncate(fileno(ctx->file_l[file_i]), 0) == 0);
    rewind(ctx->file_l[file_i]);

    return 0;
}

int gplot_reset_all(gnuplot *ctx)
{
    assert(ctx);
    for (unsigned i = 0; i < ctx->file_lsiz; i++)
        assert(gplot_reset(ctx, i) == 0);

    return 0;
}

void gplot_destroy(gnuplot *ctx)
{
    if (!ctx) return;
    for (unsigned i = 0; i < ctx->pipe_lsiz; i++) {
        pclose(ctx->pipe_l[i]);
    }
    free(ctx->pipe_l);

    for (unsigned i = 0; i < ctx->file_lsiz; i++) {
        fclose(ctx->file_l[i]);
    }
    free(ctx->file_l);
}

FILE *gplot_getf(gnuplot *ctx, unsigned file_i)
{
    assert(ctx && file_i < ctx->file_lsiz);
    return ctx->file_l[file_i];
}

FILE *gplot_getp(gnuplot *ctx, unsigned pipe_i)
{
    assert(ctx && pipe_i < ctx->pipe_lsiz);
    return ctx->pipe_l[pipe_i];
}
