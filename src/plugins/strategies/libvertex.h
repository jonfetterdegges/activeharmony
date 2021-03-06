/*
 * Copyright 2003-2013 Jeffrey K. Hollingsworth
 *
 * This file is part of Active Harmony.
 *
 * Active Harmony is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Active Harmony is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with Active Harmony.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __LIBVERTEX_H__
#define __LIBVERTEX_H__

#include "hsignature.h"
#include "hpoint.h"
#include "hperf.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct vertex {
    int id;
    hperf_t *perf;
    double term[];
} vertex_t;

typedef struct simplex {
    int len;
    vertex_t *vertex[];
} simplex_t;

int libvertex_init(hsignature_t *sig);
const vertex_t *vertex_min(void);
const vertex_t *vertex_max(void);

vertex_t * vertex_alloc();
void       vertex_reset(vertex_t *v);
int        vertex_copy(vertex_t *dst, const vertex_t *src);
void       vertex_free(vertex_t *v);
int        vertex_center(vertex_t *v);
int        vertex_rand(vertex_t *v);
int        vertex_rand_trim(vertex_t *v, double trim);
double     vertex_dist(const vertex_t *v1, const vertex_t *v2);
void       vertex_transform(const vertex_t *src, const vertex_t *wrt,
                            double coefficient, vertex_t *result);
int        vertex_outofbounds(const vertex_t *v);
int        vertex_regrid(vertex_t *v);
int        vertex_to_hpoint(const vertex_t *v, hpoint_t *result);
int        vertex_from_hpoint(const hpoint_t *pt, vertex_t *result);
int        vertex_from_string(const char *str, hsignature_t *sig,
                              vertex_t *result);

simplex_t *simplex_alloc(int m);
void       simplex_reset(simplex_t *s);
int        simplex_copy(simplex_t *dst, const simplex_t *src);
void       simplex_free(simplex_t *s);
void       simplex_centroid(const simplex_t *s, vertex_t *v);
void       simplex_transform(const simplex_t *src, const vertex_t *wrt,
                             double coefficient, simplex_t *result);
int        simplex_outofbounds(const simplex_t *s);
int        simplex_regrid(simplex_t *s);
int        simplex_from_vertex(const vertex_t *v, double percent,
                               simplex_t *s);
int        simplex_collapsed(const simplex_t *s);
int simplex_set_vertex(simplex_t *s, const vertex_t *v, int n);

#ifdef __cplusplus
}
#endif

#endif
