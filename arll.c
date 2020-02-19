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

#include "arll.h"
#include "assert.h"
#include <stdlib.h>
#include <string.h>

static const arll arll_zero = { 0 };

static inline int arll_realloc(arll *arllp, unsigned newsiz) {
    void *np = realloc(arllp->blk_l, newsiz * arllp->blk_siz);
    if (np) {
        arllp->blk_l = np;
        arllp->blk_lsiz = newsiz;
        return 0;
    }

    return -1;
}

/* Allocates memory for an array linked list object and initializes it.
 * @param blk_siz   size of the objects stored in the list
 * @param init_lszi size of the initial list
 * @return     pointer to the arll object on success, NULL on failure */
arll *arll_construct(uint16_t blk_siz, unsigned init_lsiz)
{
    assert(init_lsiz && blk_siz);
    arll *arllnp = malloc(sizeof(*arllnp));
    if (!arllnp) goto arll_construct_err;
    *arllnp = arll_zero;

    arllnp->blk_siz = blk_siz;
    if (arll_realloc(arllnp, init_lsiz)) goto arll_construct_err;
    return arllnp;

arll_construct_err:
    arll_destroy(arllnp);
    return NULL;
}

/* @param arllp pointer to the arll object
 * @return Returns a pointer to the next object in the list or NULL if end of list
 * is reached. WARNING: returned pointer not guaranteed to be valid after a call
 * to arll_push() on the same arll object. */
void *arll_next(arll *arllp)
{
    assert(arllp);
    if (arllp->blk_curi == arllp->blk_cnt) return NULL;
    return ((char*)arllp->blk_l) + (arllp->blk_curi++ * arllp->blk_siz);
}

/* @param arllp pointer to the arll object
 * @param i index of the element to be retreieved
 * @return Returns a pointer to the object in the list with index 'i' or NULL if
 * 'i' is out of bounds. WARNING: returned pointer not guaranteed to be valid
 * after a call to arll_push() on the same arll object. */
void *arll_geti(arll *arllp, unsigned i)
{
    assert(arllp);
    if (i > arllp->blk_cnt) {
        assert(0); // TODO: delegate
        return NULL;
    }
    return ((char*)arllp->blk_l) + (i * arllp->blk_siz);
}

/* Reset the iterator of an arll object.
 * @param arllp pointer to the arll object*/
void arll_rewind(arll *arllp)
{
    assert(arllp);
    arllp->blk_curi = 0;
}

/* Push the contents of the object pointed by 'blkp' at the end of the list.
 * @param arllp pointer to the arll object
 * @param blkp pointer to the object contents to be pushed in the list. The size
 * of the object pointed by blkp must be the size passed on arll object creation.
 * @return On succress, the index of the pushed object, -1 otherwise. */
int arll_push(arll *arllp, const void *blkp)
{
    assert(arllp && blkp);

    if (arllp->blk_cnt == arllp->blk_lsiz)
        if (arll_realloc(arllp, arllp->blk_lsiz * 2)) return -1;

    char *blk_l_p = ((char*)arllp->blk_l) + arllp->blk_cnt * arllp->blk_siz;
    memcpy(blk_l_p, blkp, arllp->blk_siz);
    return arllp->blk_cnt++;
}

/* Destroys an arll object and frees the memory associated with it.
 * @param arllp pointer to the arllp object to be destroyed */
void arll_destroy(arll *arllp)
{
    if (arllp) {
        free(arllp->blk_l);
        free(arllp);
    }
}

/* @return the index of an object in the list that would be returned by a call to
 * arll_next().
 * @param arllp pointer to the arll object */
int arll_get_nexti(const arll *arllp)
{
    assert(arllp);
    return arllp->blk_curi;
}

/* @return number of object stored in the arll object
 * @param arllp pointer to the arll object */
unsigned arll_len(const arll *arllp) {
    assert(arllp);
    return arllp->blk_cnt;
}
