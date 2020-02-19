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

#ifndef TASK_CLASSIFIER_H_
#define TASK_CLASSIFIER_H_

/*
 * The TCDdict and TCDictCtx classes are NOT inheritable.
 */

#include <stdint.h>
#include <stdio.h>
#include "TaskSegRaw.h"
#include "element_context.h"
//#include "ROObj.h"

typedef int TCKey;
typedef double TCVal;
#define TCKey_INVALID (-1)
#define TCKey_is_valid(__tck)((__tck) != TCKey_INVALID)
#define TCVal_INVALID (-1.0f)
#define TCVal_is_valid(__tcv)((__tcv) >= 0)

typedef enum {
    TCR_ok,
    TCR_dict_too_big,
    TCR_err,
    TCR_mem
} TCRes;

/* NOT INERITABLE */
typedef struct {
    Elem        _super;
    TCVal       *supremum_l;
    TCVal       *mean_l;
    unsigned    size;
} TCDict;

/* NOT INERITABLE */
typedef struct {
    ElemCtx _super;
} TCDictCtx;

typedef Elem_VMT TCDict_VMT;
//extern const TCDict_VMT TCDict_vmt;

//typedef struct {
//    ROObj roobj;
//    TCVal *supremum_l;
//    TCVal *mean_l;
//    unsigned size;
//} TCDict_RO;



typedef struct __attribute__((__packed__)) {
    uint32_t size;
    /* here start 'size' dictionaries */
} TCDictCtx_pckd;

typedef struct __attribute__((__packed__)) {
    uint32_t size;
    /* here start 'size' times 'TCVal' supremum_l */
    /* here start 'size' times 'TCVal' mean_l */
} TCDict_pckd;

//extern const ROObj_VMT tcdict_roobj_vmt;


int TCDict_init(
    TCDictCtx *ctx,
    TCDict *tcdictp,
    const double *reql,
    unsigned reql_siz,
    double k,
    unsigned max_dict_siz);
int TCDictCtx_init(TCDictCtx *ctx);

void TCDict_print(const TCDict *tcdictp);
TCKey TCDict_key_from_val(const TCDict *ctx, TCVal val);
TCVal TCDict_val_from_key(const TCDict *ctx, TCKey key);
void TCDict_export(const TCDict *tcdictp, FILE *fp);
unsigned _TCDict_disksize(Elem *ep);

int TCDictCtx_to_file(TCDictCtx *ctx, FILE *wfp);

#endif /* TASK_CLASSIFIER_H_ */
