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

#ifndef GPLOT_H_
#define GPLOT_H_

#include <stdio.h>

typedef struct {
    FILE **pipe_l;
    FILE **file_l;
    unsigned pipe_lsiz;
    unsigned file_lsiz;
} gnuplot;

gnuplot *gplot_create(
    const char *const *pipe_run_l,
    const char *const *file_name_l,
    unsigned pipe_cnt,
    unsigned file_cnt);
//void gplot_plot(gnuplot *ctx, unsigned pipe_i, const char *plot_cmt);
int gplot_reset(gnuplot *ctx, unsigned file_i);
int gplot_reset_all(gnuplot *ctx);
void gplot_destroy(gnuplot *ctx);
FILE *gplot_getf(gnuplot *ctx, unsigned file_i);
FILE *gplot_getp(gnuplot *ctx, unsigned pipe_i);

#endif /* GPLOT_H_ */
