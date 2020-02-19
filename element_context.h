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

#ifndef ELEMENT_CONTEXT_H_
#define ELEMENT_CONTEXT_H_

/* Description:
 *
 * Every class inheriting Elem has the ability to associate its objects with a
 * context, which simplifies the tracking of objects. Every time an object is
 * initialized, it is added to the context of choice and removed when
 * deinitialized. The ElemCtx class is responsible for tracking. The ElemCtx
 * class is also inheritable. */

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "_object.h"

typedef struct Elem Elem;
typedef struct ElemCtx ElemCtx;

typedef struct {
    Object_VMT _super;
} Elem_VMT;

extern const Elem_VMT elem_vmt;

struct Elem {
    Object _super;
    Elem *next_p;
    Elem **prev_next_pp;
    ElemCtx *ctxp;
    int idx;
};

typedef struct {
   Object_VMT _super;
} ElemCtx_VMT;

extern const ElemCtx_VMT elemctx_vmt;

struct ElemCtx{
    Object _super;
    Elem *elem_dll;
    unsigned size;
};

/* *** NOT INHERITABLE METHODS BEGIN *** */
int Elem_init(ElemCtx *ctx, Elem *ep);
int ElemCtx_init(ElemCtx *ctx);
int ElemCtx_assign_idx(ElemCtx *ctx);

/* @return return the index of an Elem object previously assigned by
 * ElemCtx_assign_idx().
 * @param ep pointer to an Elem object */
static inline int Elem_ptoi(Elem *ep)
{
    assert(ep);
    return ep->idx;
}
/* *** NOT INHERITABLE METHODS END *** */

/* *** INHERITABLE METHODS WRAPPERS BEGIN *** */

/* *** INHERITABLE METHODS WRAPPERS END *** */

/* *** INHERITABLE METHODS IMPLEMENTATION DECLARATION BEGIN *** */
/* All the function below are the implementations of the methods in the VMT.
 * They may/must be called ONLY in the context of the corresponding overridden
 * methods of a class DIRECTLY inheriting this class. This functions MUST NOT be
 * called otherwise. Use the inheritable wrappers defined above instead. */

void _Elem_deinit(Object *objp);
void _ElemCtx_deinit(Object *objp);
/* *** INHERITABLE METHODS IMPLEMENTATION DECLARATION END *** */

#endif /* ELEMENT_CONTEXT_H_ */
