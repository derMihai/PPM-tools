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

#ifndef GRAPH_MINER_H_
#define GRAPH_MINER_H_

#include "pm.h"

void GM_mine_for_symm(PMContext *ctx);
void GM_mine_for_asymm(PMContext *ctx);
void GM_find_terminating(PMV *haystack, PMV *needle, arll *similarl);
void GM_mine_recurrence(PMContext *ctx);


#endif /* GRAPH_MINER_H_ */
