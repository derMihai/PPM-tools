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

#ifndef ARLL_H_
#define ARLL_H_

#include <stdint.h>

/* Array Linked List. Iterable. Can only be appended, no deletion supported.
 * Only for objects of fixed size.*/

typedef struct {
    void        *blk_l;
    unsigned    blk_cnt;
    unsigned    blk_lsiz;
    unsigned    blk_curi;
    uint16_t    blk_siz;
} arll;

arll        *arll_construct(uint16_t blk_siz, unsigned init_lsiz);
void        *arll_next(arll *arllp);
void        arll_rewind(arll *arllp);
void        *arll_geti(arll *arllp, unsigned i);
int         arll_push(arll *arllp, const void *blkp);
void        arll_destroy(arll *arllp);
int         arll_get_nexti(const arll *arllp);
unsigned    arll_len(const arll *arllp);

#endif /* ARLL_H_ */
