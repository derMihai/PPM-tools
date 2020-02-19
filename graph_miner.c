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

#include "graph_miner.h"
#include "pm.h"

static void _mine_for_symm(PMV *vp)
{
    if (!vp) return;
    switch (vp->type) {
    case PMV_seg:
        break;
    case PMV_insc:
        _mine_for_symm(vp->pp);
        _mine_for_symm(vp->cp);

        if (PMV_insc_is_symm(vp))
            PMV_merge_r(vp->pp, vp->cp);

        break;
    case PMV_wrap:
        _mine_for_symm(vp->wp);
        break;
    default:
        assert(0);
    }

    _mine_for_symm(vp->np);
}

/* Mine for similar subtrees in the form of symmetrical branches.
 * @param ctx pointer to the PM context */
void GM_mine_for_symm(PMContext *ctx)
{
    assert(ctx);
    _mine_for_symm(ctx->headp);
}

static void _mine_for_asymm(PMV *vp)
{
    if (!vp) return;

    switch (vp->type) {
    case PMV_seg:
        break;

    case PMV_wrap:
        _mine_for_asymm(vp->wp);
        break;

    case PMV_insc:
        _mine_for_asymm(vp->pp);
        _mine_for_asymm(vp->cp);

        if (!PMV_insc_is_symm(vp)) {
            PMV *haystackp, *needlep;

            arll *simp_arll = arll_construct(sizeof(PMV*), 1);
            assert(simp_arll);

            haystackp = vp->pp;
            needlep = vp->cp;

            GM_find_terminating(haystackp, needlep, simp_arll);
            if (arll_len(simp_arll) == 0) {
                haystackp = vp->cp;
                needlep = vp->pp;
                GM_find_terminating(haystackp, needlep, simp_arll);
            }

            PMV **cvpp;
            arll_rewind(simp_arll);
            while ((cvpp = arll_next(simp_arll))) {
                PMV_merge_r(needlep, *cvpp);
            }
            arll_destroy(simp_arll);
        }
        break;

    default:
        assert(0);
    }

    _mine_for_asymm(vp->np);
}

/* Mine for similar subtrees in asymmetrical branches.
 * @param ctx pointer to the PM context */
void GM_mine_for_asymm(PMContext *ctx)
{
    assert(ctx);
    _mine_for_asymm(ctx->headp);
}

#define GM_RECURRING_ADDED 0x1

static void _mine_recurrence(PMV *vp, PMContext *ctx)
{
    if (!vp) return;
    if (vp->type == PMV_insc) {
        _mine_recurrence(vp->pp, ctx);
        _mine_recurrence(vp->cp, ctx);
    }

    PMV *vendp, *nendp;
    PMV *np = vp->np;
    PMV *np_recursion = vp->np;

    if (vp->external.as_uint & GM_RECURRING_ADDED)
        goto _mine_recurrence_continue;

    char first_recurrence = 1;
    char first_not_matching = 1;

    PMV *vp_wrap_end;
    while (np) {
        PMV_find_similar_stem(vp, np, &vendp, &nendp, 0);
        if (vendp) {
            PMV *wp;
            if (first_recurrence) {
                wp = PMV_wrap_section(vp, vendp);
                wp->external.as_uint |= GM_RECURRING_ADDED;
                first_recurrence = 0;
                vp_wrap_end = vendp;

            } else {
                if (vendp != vp_wrap_end) {
                    if (first_not_matching) {
                        np_recursion = np;
                        first_not_matching = 0;
                    }
                    np = np->np;
                    continue;
                }
            }
            wp = PMV_wrap_section(np, nendp);
            wp->external.as_uint |= GM_RECURRING_ADDED;

            PMV_merge_r(vp, np);
            np = wp->np;
        } else {
            if (first_not_matching) {
                np_recursion = np;
                first_not_matching = 0;
            }
            np = np->np;
        }
    }

_mine_recurrence_continue:
    _mine_recurrence(np_recursion, ctx);
}

/* Mine the PPM tree for recurring patterns. Recurring patterns are parts of the
 * tree that sit on top of each other (share the same stem) and can be made
 * by the addition of wrappers to similar PPMs.
 * @param ctx pointer to the PM context */
void GM_mine_recurrence(PMContext *ctx)
{
    assert(ctx);
    _mine_recurrence(ctx->headp, ctx);
}

/* Searches for needle sub-tree in haystack sub-tree  and returns a list of
 * matches.
 * @param haystack pointer to haystack vertex
 * @param needle pointer to needle vertex
 * @param similarl pointer to array where the matches will be added. The
 *  contents of the arll list are pointers to the matching vertices.*/
void GM_find_terminating(PMV *haystack, PMV *needle, arll *similarl)
{
    assert(similarl);
    if (!haystack) return;

    if (PMV_is_similar(haystack, needle, 1)) {
        arll_push(similarl, &haystack);
        return;
    }

    if (haystack->depth < needle->depth) return;
    if (haystack->vcnt < needle->vcnt) return;

    switch (haystack->type) {
    case PMV_seg:
        break;

    case PMV_insc:
        GM_find_terminating(haystack->pp, needle, similarl);
        GM_find_terminating(haystack->cp, needle, similarl);
        break;

    case PMV_wrap:
        GM_find_terminating(haystack->wp, needle, similarl);
        break;

    default:
        assert(0);
    }

    GM_find_terminating(haystack->np, needle, similarl);
}
