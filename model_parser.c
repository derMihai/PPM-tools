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

#include "model_parser.h"
#include <assert.h>
#include <limits.h>
#include <stdlib.h>
#include <float.h>

static const MPTask     task_empty  = { 0 };
static const MParser    mparser_empty = { 0 };

/* Init a context for parsing a PPM model file. This also allocates memory for
 * the vertex list.
 * @param ctx pointer to parsing context
 * @param src source file pointer
 * @param cap_val_com cap the weights of the communication tasks. Set to -1 for
 *  no capping
 * @param cap_val_cal cap the weights of the calculation tasks. Set to -1 for no
 *  capping
 * @return MP_ok on success, MP_err on IO error or file corruption, MP_mem on
 *  memory allocation issues */
MPRes MParser_init(
    MParser *ctx,
    FILE *src,
    double cap_val_com,
    double cap_val_cal)
{
    assert(ctx);
    char linebuf[2048];
    *ctx = mparser_empty;

    ctx->src = src;
    ctx->cap_val_cal = cap_val_cal < 0 ? DBL_MAX : cap_val_cal;
    ctx->cap_val_com = cap_val_com < 0 ? DBL_MAX : cap_val_com;

    unsigned long tno_cur;
    unsigned long tno_max = 0;
    unsigned long tno_min = ULONG_MAX;

    rewind(ctx->src);
    while (1) {
        if (!fgets(linebuf, 2048, ctx->src)) break;
        if (linebuf[0] < '0' || linebuf[0] > '9') continue;
        tno_cur = atol(linebuf);
        if (tno_cur > tno_max) tno_max = tno_cur;
        if (tno_cur < tno_min) tno_min = tno_cur;
    }

    /* IO error or invalid file */
    if (tno_max < tno_min) return MP_err;

    ctx->head = tno_min;
    ctx->task_llen = tno_max + 1;
    ctx->task_l = malloc(ctx->task_llen  * sizeof(MPTask));
    if (!ctx->task_l) return MP_mem;

    return MP_ok;
}

/* Start the parsing of the model.
 * @param ctx the context to be parsed
 * @return MP_ok on success, MP_err on file corruption or IO error */
MPRes MParser_parse(MParser *ctx)
{
    assert(ctx);

    char linebuf[2048];
    MPTask ct;
    TaskNo tno;
    int lp, lpt, conv_cnt;
    int lno = 1;

    rewind(ctx->src);

    while (fgets(linebuf, 2048, ctx->src)) {
//        printf("lno: %d\n", lno++);
        lp = 0;
        if (linebuf[0] < '0' || linebuf[0] > '9') continue;

        ct = task_empty;
        ct.lno = lno;

        conv_cnt = sscanf(linebuf, "%lu %d %d %lu%n",
            &tno, &ct.pno, (int*)&ct.ttype, &ct.mem, &lpt);
        if (conv_cnt != 4)
            return MP_err;

        lp += lpt;

        switch (ct.ttype) {
        case MPTT_start:
        case MPTT_forkend:
        case MPTT_join:
            conv_cnt = sscanf(linebuf + lp, " -> %lu", &ct.next[0]);
            if (conv_cnt != 1)
                return MP_err;
            break;

        case MPTT_end:
            //printf("end\n");
            break;

        case MPTT_fork:
            conv_cnt = sscanf(linebuf + lp, " -> %lu %*u %*u -> %lu",
                &ct.next[0], &ct.next[1]);
            if (conv_cnt != 2) {
                if (conv_cnt < 1) {
                    return MP_err;
                }
                /* empty fork */
                ct.next[1] = 0;
            }
            break;

        case MPTT_calc:
            conv_cnt = sscanf(linebuf + lp, " %lf -> %lu",
                &ct.req, &ct.next[0]);
            if (conv_cnt != 2)
                return MP_err;
            break;

        case MPTT_com:
            conv_cnt = sscanf(linebuf + lp, " %lf -- %lu%n",
                &ct.req, &ct.dest, &lpt);
            if (conv_cnt != 2)
                return MP_err;
            lp += lpt;

            /* dest == 0 => broadcast */
            conv_cnt = sscanf(linebuf + lp,
                ct.dest == 0 ? " -> %lu" : "  %*u %*u -> %lu" , &ct.next[0]);
            if (conv_cnt != 1)
                return MP_err;
            break;

        default:
            return MP_err;

        }

        if (tno >= ctx->task_llen)
            return MP_err;
        if (ct.next[0] >= ctx->task_llen)
            return MP_err;
        if (ct.next[1] >= ctx->task_llen)
            return MP_err;
        ctx->task_l[tno] = ct;
    }

    return MP_ok;
}

/* Deinit a parsing context
 * @param ctx pointer to parsing context
 * @return MP_ok is always returned*/
MPRes MParser_deinit(MParser *ctx)
{
    assert(ctx);
    free(ctx->task_l);
    return MP_ok;
}

