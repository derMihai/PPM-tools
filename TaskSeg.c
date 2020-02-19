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

#include "TaskSeg.h"

static const TaskSeg taskseg_zero = { 0 };
static const TaskSegCtx tasksegctx_zero = { 0 };

const TaskSeg_VMT TaskSeg_vmt = {
    ._super._super._object_deinit = _TaskSeg_deinit,

    .TaskSeg_compar = NULL,
    .TaskSeg_print = NULL,
    .TaskSeg_export = NULL,
    .TaskSeg_to_reql = NULL,
    .TaskSeg_eval = NULL,
};

const TaskSegCtx_VMT TaskSegCtx_vmt = {
    ._super._super._object_deinit = _TaskSegCtx_deinit,
};
/* Initiate a TaskSeg object.
 * @param segp pointer to the TaskSeg object to be initialized
 * @param ctx pointer to the TaskSegCtx context into 'segp' is to be registered
 * This function MUST be called BEFORE the object that directly inherits this
 * class performs its init routine. */
int TaskSeg_init(TaskSegCtx *ctx, TaskSeg *segp)
{
    assert(ctx && segp);
    *segp = taskseg_zero;
    assert(!Elem_init((ElemCtx*)ctx, (Elem*)segp));

    _obj_vmtp(segp) = (Object_VMT*)&TaskSeg_vmt;
    return 0;
}

/* Initiate a TaskSegCtx object.
 * @param ep pointer to the TaskSegCtx context object to be initialized
 * @return 0 for success, -1 otherwise
 * This function MUST be called BEFORE the object that directly inherits this
 * class performs its init routine. */
int TaskSegCtx_init(TaskSegCtx *ctx)
{
    assert(ctx);
    *ctx = tasksegctx_zero;
    assert(!ElemCtx_init((ElemCtx*)ctx));

    _obj_vmtp(ctx) = (Object_VMT*)&TaskSegCtx_vmt;
    return 0;
}

/* This function MUST be called AFTER the object DIRECTLY inheriting this class
 * ended its deinit routine. */
void _TaskSeg_deinit(Object* objp)
{
    _Elem_deinit(objp);
}

/* This function MUST be called AFTER the object DIRECTLY inheriting this class
 * ended its deinit routine. */
void _TaskSegCtx_deinit(Object* objp)
{
    _ElemCtx_deinit(objp);
}

