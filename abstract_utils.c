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

#include <assert.h>
#include <string.h>
#include "abstract_utils.h"

/* Wrapper function to export the whole model to a binary file. It behaves
 * differently depending on what types of TaskSegment subclass is being used in
 * the model.
 *
 * File structure:
 * Uncompressed model:
 * [ PPM data ][ Segment data ]
 *
 * Compressed model:
 * [ PPM data ][ Segment data ][ Dictionary data ]
 *
 * @param dictctx pointer to the dictionary context the segment context operates
 *  with, in case TaskSegBuck segments are used in the model, otherwise NULL.
 * @param segctx pointer to the segment context the model is using. May be of
 *  type TaskSegRawCtx or TaskSegBuckCtx
 * @param pmctx pointer to the PM context
 * @param wfp pointer to the file the model will be exported into
 * @return 0 on success, -1 otherwise*/
int au_export_model(
    TCDictCtx *dictctx,
    TaskSegCtx *segctx,
    PMContext *pmctx,
    FILE *wfp)
{
    assert(segctx && pmctx && wfp);

    assert(fseek(wfp, 0, SEEK_END) == 0);
    assert(ftell(wfp) == 0);

    unsigned total_size = 0;
    unsigned fts;

    total_size += PMContext_to_file(pmctx, wfp, segctx);

    assert(fseek(wfp, 0, SEEK_END) == 0);
    fts = ftell(wfp);
    assert(fts == total_size);

    if (segctx->tsclass_id == TSB_CLASSID) {
        assert(dictctx);

        total_size += TaskSegBuckCtx_to_file((TaskSegBuckCtx*)segctx, wfp, dictctx);
        assert(fseek(wfp, 0, SEEK_END) == 0);
        fts = ftell(wfp);
        assert(fts == total_size);

        total_size += TCDictCtx_to_file(dictctx, wfp);
        assert(fseek(wfp, 0, SEEK_END) == 0);
        fts = ftell(wfp);
        assert(fts == total_size);
    } else if (segctx->tsclass_id == TSR_CLASSID){
        total_size += TaskSegRawCtx_to_file((TaskSegRawCtx*)segctx, wfp);
        assert(fseek(wfp, 0, SEEK_END) == 0);
        fts = ftell(wfp);
        assert(fts == total_size);
    } else {
        assert(0);
    }

    assert(fseek(wfp, 0, SEEK_END) == 0);
    fts = ftell(wfp);
    assert(fts == total_size);

    return total_size;
}

/* Removes the last extension and the path from a filename.
 * @param fname pointer to null-terminated array representing the file name
 * @return pointer to a newly null-terminated array containing the extracted
 *  file name, or NULL on error. The user should free the returned pointer with
 *  free() after it is not used anymore. */
char *fname_extract_name(const char *fname)
{
    assert(fname);
    char *fname_new = strdup(fname);
    assert(fname);

    char *cp = strrchr(fname_new , '.');

    if (cp)
        *cp = 0;

    cp = strrchr(fname_new , '/');
    if (cp) {
        cp++;
    } else {
        cp = fname_new;
    }

    char *fname_extracted = strdup(cp);
    free(fname_new);

    return fname_extracted;
}

/* Updates a filename with a new path, a suffix and a .dat extension.
 * @param fname pointer to null-terminated array representing the file name
 * @param suffix pointer to a null-terminated array representing the suffix
 * @param dir pointer to a null-terminated array representing the folder path
 * @return pointer to a newly null-terminated array containing the new file path
    or NULL on error. The user should free the returned pointer with
 *  free() after it is not used anymore. */
char *fname_update(const char *fname, const char *suffix, const char *dir)
{
    assert(dir && fname && suffix);

    char *fname_new = fname_extract_name(fname);

    char buf[1024];

    snprintf(buf, 1024, "%s/%s%s.dat", dir, fname_new, suffix);
    free(fname_new);

    return strdup(buf);
}

/* Compress a file with tar/gzip using the enviroment variable GZIP=-9. The
 * newly created file is appended with the extension .tar.gz.
 * @param fname pointer to a null-terminated array containing the file name
 * @return the size of the compressed file, in bytes*/
int file_compress_gzip(const char *fname)
{
    char cbuf[2048];
    char nfname[1024];
    snprintf(nfname, 1024, "%s.tar.gz", fname);
    snprintf(cbuf, 2048, "env GZIP=-9 tar cvzf %s %s", nfname, fname);
    FILE *pipe = popen(cbuf, "r");
    assert(pipe);

    while (fgets(cbuf, 2048, pipe)) {
      printf("%s\n",cbuf);
    }
    fclose(pipe);

    FILE *compf = fopen(nfname, "r");
    assert(compf);
    assert(fseek(compf, 0, SEEK_END) == 0);
    unsigned size = ftell(compf);
    fclose(compf);

    return size;
}
