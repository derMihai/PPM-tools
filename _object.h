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

#ifndef OBJECT_H_
#define OBJECT_H_

/* The superclass of all the classes using an OOP approach. */
#include <stdlib.h>
#include <assert.h>

typedef struct Object Object;
typedef struct Object_VMT Object_VMT;

/* Retrieve a pointer to the VMT of an object inheriting Object */
#define _obj_vmtp(__objp) (((Object*)(__objp))->vmt)

/* allocate and zero out memory for an object.
 * @param __size size of the object, in bytes
 * @return pointer to newly alocated memory, NULL in case of failure */
#define _obj_alloc(__size_b)(calloc(1, __size_b))

/* deallocate memory for an object previously allocated with _obj_free()
 * @param __objp pointer to an object */
#define _obj_free(__objp)(free(__objp))

/* Virtual Method Table of an object. Each inheriting object must contain this
 * structure as the first entry in it's own VMT. */
struct Object_VMT {
    void (*_object_deinit)(Object*);
};

/* Superclass object. Each object inheriting Object must contain this structure
 * as the first entry. */
struct Object {
    const Object_VMT *vmt;
};

static void _object_deinit(Object *objp);
static int Object_init(Object *objp);

static const Object digital_black = { 0 };

static const Object_VMT _object_vmt = {
    ._object_deinit = _object_deinit
};

/* *** NOT INHERITABLE METHODS BEGIN *** */
/* Initiate an Object object.
 * @param objp pointer to an Object object
 * @return 0 for success, -1 otherwise
 * This function MUST be called BEFORE the object that directly inherits this
 * class performs its init routine. */
static int Object_init(Object *objp)
{
    assert(objp);
    *objp = digital_black;
    objp->vmt = &_object_vmt;
    return 0;
}
/* *** NOT INHERITABLE METHODS END *** */

/* *** INHERITABLE METHODS BEGIN *** */
/* Deinit an Object object.
 * @param objp pointer to an Object object */
static void Object_deinit(Object *objp)
{
    if (!objp) return;
    objp->vmt->_object_deinit(objp);
}
/* *** INHERITABLE METHODS END *** */

/* *** INHERITABLE METHODS IMPLEMENTATION DECLARATION BEGIN *** */
/* All the function below are the implementations of the methods in the VMT.
 * They may/must be called ONLY in the context of the corresponding overridden
 * methods of a class DIRECTLY inheriting this class. These functions MUST NOT
 * be called otherwise. Use the inheritable wrappers defined above instead. */

/* This function MUST be called AFTER the object DIRECTLY inheriting this class
 * ended its deinit routine. */
static void _object_deinit(Object *objp)
{
    (void)objp;
}
/* *** INHERITABLE METHODS IMPLEMENTATION DECLARATION END *** */

#endif /* OBJECT_H_ */
