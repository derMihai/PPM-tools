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

#ifndef ABSTRACT_UTILS_H_
#define ABSTRACT_UTILS_H_

#include <stdio.h>
#include "pm.h"
#include "task_classifier.h"
#include "TaskSegRaw.h"
#include "TaskSegBuck.h"

int au_export_model(
    TCDictCtx *dictctx,
    TaskSegCtx *segctx,
    PMContext *pmctx,
    FILE *wfp);

char *fname_update(const char *fname, const char *suffix, const char *dir);
int file_compress_gzip(const char *fname);
#endif /* ABSTRACT_UTILS_H_ */
