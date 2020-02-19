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

#include <stdint.h>
#include "element_context.h"

static const Elem elem_zero = { 0 };
static const ElemCtx elemctx_zero = { 0 };

/* This function MUST be called AFTER the object DIRECTLY inheriting this class
 * ended its deinit routine.
 * The object reference is removed from the context. */
void _Elem_deinit(Object *objp)
{
    Elem *ep = (Elem*)objp;
    assert(ep->ctxp->size);
    ep->ctxp->size--;

    if (ep->next_p) ep->next_p->prev_next_pp = ep->prev_next_pp;
    *(ep->prev_next_pp) = ep->next_p;

    _object_deinit((Object*)ep);
}

/* This function MUST be called AFTER the object DIRECTLY inheriting this class
 * ended its deinit routine.
 * WARNING: all the elements in the context will be deinited and deallocated.
 * Therefore it is important that all the objects in the context were allocated
 * with _obj_alloc(). */
void _ElemCtx_deinit(Object *objp)
{
    ElemCtx *ctx = (ElemCtx*)objp;

    while (ctx->elem_dll) {
        Object *tmp = (Object*)ctx->elem_dll;
        Object_deinit(tmp);
        _obj_free(tmp);
    }

    _object_deinit((Object*)ctx);
}

const Elem_VMT elem_vmt = {
    ._super._object_deinit = _Elem_deinit,
};

const ElemCtx_VMT elemctx_vmt = {
    ._super._object_deinit = _ElemCtx_deinit,
};

/* Initiate an Elem object.
 * @param ep pointer to the Elem object to be initialized
 * @param ctx pointer to the ElemCtx context into object 'ep' is to be
 *  registered
 * This function MUST be called BEFORE the object that directly inherits this
 * class performs its init routine. */
int Elem_init(ElemCtx *ctx, Elem *ep)
{
    assert(ctx && ep);
    *ep = elem_zero;
    assert(!Object_init((Object*)ep));

    _obj_vmtp(ep) = (Object_VMT*)&elem_vmt;

    if (ctx->elem_dll) {
        ctx->elem_dll->prev_next_pp = &ep->next_p;
    }

    ep->next_p = ctx->elem_dll;
    ep->prev_next_pp = &ctx->elem_dll;
    ctx->elem_dll = ep;

    ep->ctxp = ctx;
    ep->idx = -1;
    ctx->size++;

    return 0;
}

/* Initiate an ElemCtx object.
 * @param ep pointer to the ElemCtx context object to be initialized
 * @return 0 for success, -1 otherwise
 * This function MUST be called BEFORE the object that directly inherits this
 * class performs its init routine. */
int ElemCtx_init(ElemCtx *ctx)
{
    assert(ctx);
    *ctx = elemctx_zero;
    Object_init((Object*)ctx);

    _obj_vmtp(ctx) = (Object_VMT*)&elemctx_vmt;
    return 0;
}

/* Assigns unique indexes to all the Elem objects tracked by the context pointed
 * by 'ctx', in the order they appear in the doubly linked list.
 * @param ctx pointer to the ElemCtx context into object
 * @return 0 for success, -1 otherwise
 * WARNING: After a call to Elem_init() on a specific context, already assigned
 * indices in the same context are not guaranteed to be unique. */
int ElemCtx_assign_idx(ElemCtx *ctx)
{
    Elem *ep = ctx->elem_dll;
    int idx = 0;
    while (ep) {
        ep->idx = idx++;
        ep = ep->next_p;
    }
    return 0;
}

