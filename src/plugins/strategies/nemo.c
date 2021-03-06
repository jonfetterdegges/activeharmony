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

#include "strategy.h"
#include "session-core.h"
#include "hsignature.h"
#include "hperf.h"
#include "hutil.h"
#include "hcfg.h"
#include "defaults.h"
#include "libvertex.h"

#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>
#include <math.h>
#include <assert.h>

hpoint_t best;
hperf_t *best_perf;

hpoint_t curr;

typedef struct value_range {
    double min;
    double max;
} value_range_t;

typedef enum simplex_init {
    SIMPLEX_INIT_UNKNOWN = 0,
    SIMPLEX_INIT_CENTER,
    SIMPLEX_INIT_RANDOM,
    SIMPLEX_INIT_POINT,

    SIMPLEX_INIT_MAX
} simplex_init_t;

typedef enum reject_method {
    REJECT_METHOD_UNKNOWN = 0,
    REJECT_METHOD_PENALTY,
    REJECT_METHOD_RANDOM,

    REJECT_METHOD_MAX
} reject_method_t;

typedef enum simplex_state {
    SIMPLEX_STATE_UNKNONW = 0,
    SIMPLEX_STATE_INIT,
    SIMPLEX_STATE_REFLECT,
    SIMPLEX_STATE_EXPAND,
    SIMPLEX_STATE_CONTRACT,
    SIMPLEX_STATE_SHRINK,
    SIMPLEX_STATE_CONVERGED,

    SIMPLEX_STATE_MAX
} simplex_state_t;

/* Forward function definitions. */
int nemo_init_simplex(void);
int strategy_cfg(hsignature_t *sig);
int nemo_phase_incr(void);
void simplex_update_index(void);
void simplex_update_centroid(void);
int  nm_algorithm(void);
int  nm_state_transition(void);
int  nm_next_vertex(void);
void check_convergence(void);
double calc_perf(double *perf);

/* Variables to control search properties. */
simplex_init_t  init_method  = SIMPLEX_INIT_CENTER;
vertex_t *      init_point;
double          init_percent = 0.35;
reject_method_t reject_type  = REJECT_METHOD_PENALTY;

double reflect  = 1.0;
double expand   = 2.0;
double contract = 0.5;
double shrink   = 0.5;
double fval_tol = 1e-8;
double size_tol;
int simplex_size;

/* Variables to track current search state. */
simplex_state_t state;
vertex_t *centroid;
vertex_t *test;
simplex_t *base;
simplex_t *init;
vertex_t *next;

int phase = -1;
vertex_t *simplex_best;
vertex_t *simplex_worst;
int curr_idx; /* for INIT or SHRINK */
int next_id;

int perf_n;
double *thresh;
value_range_t *range;

/* Option variables */
double *leeway;
double mult;
int anchor;
int loose;
int samesimplex;

int nemo_init_simplex()
{
    switch (init_method) {
    case SIMPLEX_INIT_CENTER: vertex_center(init_point); break;
    case SIMPLEX_INIT_RANDOM: vertex_rand_trim(init_point,
                                               init_percent); break;
    case SIMPLEX_INIT_POINT:  assert(0 && "Not yet implemented."); break;
    default:
        session_error("Invalid initial search method.");
        return -1;
    }
    return simplex_from_vertex(init_point, init_percent, init);
}

int nemo_phase_incr(void)
{
    int i;
    double min_dist, curr_dist;
    char intbuf[16];

    if (phase >= 0) {
        /* Calculate the threshold for the previous phase. */
        thresh[phase] = range[phase].min + (range[phase].max -
                                            range[phase].min) * leeway[phase];
    }

    ++phase;
    snprintf(intbuf, sizeof(intbuf), "%d", phase);
    session_setcfg("NEMO_PHASE", intbuf);

    if (!samesimplex) {
        /* Re-initialize the initial simplex, if needed. */
        if (nemo_init_simplex() != 0) {
            session_error("Could not reinitialize the initial simplex.");
            return -1;
        }
    }
    simplex_copy(base, init);

    if (best.id > 0) {
        if (anchor) {
            int idx;
            min_dist = INFINITY;
            for (i = 0; i < simplex_size; ++i) {
                vertex_from_hpoint(&best, test);
                curr_dist = vertex_dist(test, base->vertex[i]);
                if (min_dist > curr_dist) {
                    min_dist = curr_dist;
                    idx = i;
                }
            }
            vertex_copy(base->vertex[idx], test);
        }

        hperf_reset(best_perf);
        best.id = 0;
    }

    state = SIMPLEX_STATE_INIT;
    curr_idx = 0;
    return 0;
}

/*
 * Invoked once on strategy load.
 */
int strategy_init(hsignature_t *sig)
{
    if (libvertex_init(sig) != 0) {
        session_error("Could not initialize vertex library.");
        return -1;
    }

    init_point = vertex_alloc();
    if (!init_point) {
        session_error("Could not allocate memory for initial point.");
        return -1;
    }

    if (strategy_cfg(sig) != 0)
        return -1;

    best = HPOINT_INITIALIZER;
    best_perf = hperf_alloc(perf_n);

    if (hpoint_init(&curr, simplex_size) != 0) {
        session_error("Could not initialize point structure.");
        return -1;
    }

    centroid = vertex_alloc();
    if (!centroid) {
        session_error("Could not allocate memory for centroid vertex.");
        return -1;
    }

    test = vertex_alloc();
    if (!test) {
        session_error("Could not allocate memory for testing vertex.");
        return -1;
    }

    /* Default stopping criteria: 0.5% of dist(vertex_min, vertex_max). */
    if (size_tol == 0)
        size_tol = vertex_dist(vertex_min(), vertex_max()) * 0.001;

    init = simplex_alloc(simplex_size);
    if (!init) {
        session_error("Could not allocate memory for initial simplex.");
        return -1;
    }

    if (nemo_init_simplex() != 0) {
        session_error("Could not initialize initial simplex.");
        return -1;
    }

    base = simplex_alloc(simplex_size);
    if (!base) {
        session_error("Could not allocate memory for base simplex.");
        return -1;
    }

    if (session_setcfg(CFGKEY_STRATEGY_CONVERGED, "0") != 0) {
        session_error("Could not set "
                      CFGKEY_STRATEGY_CONVERGED " config variable.");
        return -1;
    }

    next_id = 1;
    if (nemo_phase_incr() != 0)
        return -1;

    if (nm_next_vertex() != 0) {
        session_error("Could not initiate test vertex.");
        return -1;
    }

    return 0;
}

int strategy_cfg(hsignature_t *sig)
{
    int i;
    const char *cfgval;
    char *endp;

    cfgval = session_getcfg("NEMO_LOOSE");
    if (cfgval) {
        loose = (*cfgval == '1' || *cfgval == 'y' || *cfgval == 'Y');
    }
    else {
        loose = 0;
    }

    cfgval = session_getcfg("NEMO_MULT");
    if (cfgval) {
        mult = strtod(cfgval, &endp);
        if (*endp != '\0') {
            session_error("Invalid value for NEMO_MULT configuration key.");
            return -1;
        }
    }
    else {
        mult = 1.0;
    }

    cfgval = session_getcfg("NEMO_ANCHOR");
    if (cfgval) {
        anchor = (*cfgval == '1' || *cfgval == 'y' || *cfgval == 'Y');
    }
    else {
        anchor = 1;
    }

    cfgval = session_getcfg("NEMO_SAMESIMPLEX");
    if (cfgval) {
        samesimplex = (*cfgval == '1' || *cfgval == 'y' || *cfgval == 'Y');
    }
    else {
        samesimplex = 1;
    }

    /* Make sure the simplex size is N+1 or greater. */
    cfgval = session_getcfg(CFGKEY_SIMPLEX_SIZE);
    if (cfgval)
        simplex_size = atoi(cfgval);

    if (simplex_size < sig->range_len + 1)
        simplex_size = sig->range_len + 1;

    cfgval = session_getcfg(CFGKEY_INIT_METHOD);
    if (cfgval) {
        if (strcasecmp(cfgval, "center") == 0) {
            init_method = SIMPLEX_INIT_CENTER;
        }
        else if (strcasecmp(cfgval, "random") == 0) {
            init_method = SIMPLEX_INIT_RANDOM;
        }
        else if (strcasecmp(cfgval, "point") == 0) {
            init_method = SIMPLEX_INIT_POINT;
        }
        else {
            session_error("Invalid value for "
                          CFGKEY_INIT_METHOD " configuration key.");
            return -1;
        }
    }

    cfgval = session_getcfg(CFGKEY_INIT_PERCENT);
    if (cfgval) {
        init_percent = strtod(cfgval, &endp);
        if (*endp != '\0') {
            session_error("Invalid value for " CFGKEY_INIT_PERCENT
                " configuration key.");
            return -1;
        }
        if (init_percent <= 0 || init_percent > 1) {
            session_error("Configuration key " CFGKEY_INIT_PERCENT
                " must be between 0.0 and 1.0 (exclusive).");
            return -1;
        }
    }

    cfgval = session_getcfg(CFGKEY_REJECT_METHOD);
    if (cfgval) {
        if (strcasecmp(cfgval, "penalty") == 0) {
            reject_type = REJECT_METHOD_PENALTY;
        }
        else if (strcasecmp(cfgval, "random") == 0) {
            reject_type = REJECT_METHOD_RANDOM;
        }
        else {
            session_error("Invalid value for "
                          CFGKEY_REJECT_METHOD " configuration key.");
            return -1;
        }
    }

    cfgval = session_getcfg(CFGKEY_REFLECT);
    if (cfgval) {
        reflect = strtod(cfgval, &endp);
        if (*endp != '\0') {
            session_error("Invalid value for " CFGKEY_REFLECT
                          " configuration key.");
            return -1;
        }
        if (reflect <= 0.0) {
            session_error("Configuration key " CFGKEY_REFLECT
                          " must be positive.");
            return -1;
        }
    }

    cfgval = session_getcfg(CFGKEY_EXPAND);
    if (cfgval) {
        expand = strtod(cfgval, &endp);
        if (*endp != '\0') {
            session_error("Invalid value for " CFGKEY_EXPAND
                          " configuration key.");
            return -1;
        }
        if (expand <= reflect) {
            session_error("Configuration key " CFGKEY_EXPAND
                          " must be greater than the reflect coefficient.");
            return -1;
        }
    }

    cfgval = session_getcfg(CFGKEY_CONTRACT);
    if (cfgval) {
        contract = strtod(cfgval, &endp);
        if (*endp != '\0') {
            session_error("Invalid value for " CFGKEY_CONTRACT
                          " configuration key.");
            return -1;
        }
        if (contract <= 0.0 || contract >= 1.0) {
            session_error("Configuration key " CFGKEY_CONTRACT
                          " must be between 0.0 and 1.0 (exclusive).");
            return -1;
        }
    }

    cfgval = session_getcfg(CFGKEY_SHRINK);
    if (cfgval) {
        shrink = strtod(cfgval, &endp);
        if (*endp != '\0') {
            session_error("Invalid value for " CFGKEY_SHRINK
                          " configuration key.");
            return -1;
        }
        if (shrink <= 0.0 || shrink >= 1.0) {
            session_error("Configuration key " CFGKEY_SHRINK
                          " must be between 0.0 and 1.0 (exclusive).");
            return -1;
        }
    }

    cfgval = session_getcfg(CFGKEY_FVAL_TOL);
    if (cfgval) {
        fval_tol = strtod(cfgval, &endp);
        if (*endp != '\0') {
            session_error("Invalid value for " CFGKEY_FVAL_TOL
                          " configuration key.");
            return -1;
        }
    }

    cfgval = session_getcfg(CFGKEY_SIZE_TOL);
    if (cfgval) {
        size_tol = strtod(cfgval, &endp);
        if (*endp != '\0') {
            session_error("Invalid value for " CFGKEY_SIZE_TOL
                          " configuration key.");
            return -1;
        }
    }

    cfgval = session_getcfg(CFGKEY_PERF_COUNT);
    if (cfgval) {
        perf_n = atoi(cfgval);
        if (perf_n < 1) {
            session_error("Invalid value for " CFGKEY_PERF_COUNT
                          " configuration key.");
            return -1;
        }
    }
    else {
        perf_n = DEFAULT_PERF_COUNT;
    }

    leeway = (double *) malloc(sizeof(double) * (perf_n - 1));
    if (!leeway) {
        session_error("Could not allocate memory for leeway vector.");
        return -1;
    }
    cfgval = session_getcfg("NEMO_LEEWAY");
    if (cfgval) {
        endp = (char *)cfgval;
        for (i = 0; *endp && i < perf_n - 1; ++i) {
            leeway[i] = strtod(cfgval, &endp);
            if (*endp && !isspace(*endp) && *endp != ',') {
                session_error("Invalid value for NEMO_LEEWAY"
                              " configuration key.");
                return -1;
            }

            cfgval = endp;
            while (cfgval && (isspace(*cfgval) || *cfgval == ','))
                ++cfgval;
        }
        if (i < perf_n - 1) {
            session_error("Insufficient leeway values provided.");
            return -1;
        }
    }
    else {
        /* Default to a 10% leeway for all objectives. */
        for (i = 0; i < perf_n - 1; ++i)
            leeway[i] = 0.10;
    }

    range = (value_range_t *) malloc(sizeof(value_range_t) * perf_n);
    if (!range) {
        session_error("Could not allocate memory for range container.");
        return -1;
    }
    for (i = 0; i < perf_n; ++i) {
        range[i].min =  INFINITY;
        range[i].max = -INFINITY;
    }

    thresh = (double *) malloc(sizeof(double) * (perf_n - 1));
    if (!thresh) {
        session_error("Could not allocate memory for threshold container.");
        return -1;
    }

    return 0;
}

/*
 * Generate a new candidate configuration point.
 */
int strategy_generate(hflow_t *flow, hpoint_t *point)
{
    if (next->id == next_id) {
        flow->status = HFLOW_WAIT;
        return 0;
    }
    else {
        next->id = next_id;
        if (vertex_to_hpoint(next, point) != 0) {
            session_error("Internal error: Could not make point from vertex.");
            return -1;
        }
    }

    flow->status = HFLOW_ACCEPT;
    return 0;
}

/*
 * Regenerate a point deemed invalid by a later plug-in.
 */
int strategy_rejected(hflow_t *flow, hpoint_t *point)
{
    hpoint_t *hint = &flow->point;

    if (hint && hint->id != -1) {
        int orig_id = point->id;

        /* Update our state to include the hint point. */
        if (vertex_from_hpoint(hint, next) != 0) {
            session_error("Internal error: Could not make vertex from point.");
            return -1;
        }

        if (hpoint_copy(point, hint) != 0) {
            session_error("Internal error: Could not copy point.");
            return -1;
        }
        point->id = orig_id;
    }
    else {
        if (reject_type == REJECT_METHOD_PENALTY) {
            int i;
            /* Apply an infinite penalty to the invalid point and
             * allow the algorithm to determine the next point to try.
             */

            for (i = 0; i < perf_n; ++i)
                next->perf->p[i] = INFINITY;

            if (nm_algorithm() != 0) {
                session_error("Internal error: Nelder-Mead"
                              " algorithm failure.");
                return -1;
            }

            next->id = next_id;
            if (vertex_to_hpoint(next, point) != 0) {
                session_error("Internal error: Could not make"
                              " point from vertex.");
                return -1;
            }
        }
        else if (reject_type == REJECT_METHOD_RANDOM) {
            /* Replace the rejected point with a random point. */
            vertex_rand(next);
            if (vertex_to_hpoint(next, point) != 0) {
                session_error("Internal error: Could not make point"
                              " from vertex.");
                return -1;
            }
        }
    }
    flow->status = HFLOW_ACCEPT;
    return 0;
}

/*
 * Analyze the observed performance for this configuration point.
 */
int strategy_analyze(htrial_t *trial)
{
    int i;
    double penalty, base;

    if (trial->point.id != next->id) {
        session_error("Rouge points not supported.");
        return -1;
    }
    hperf_copy(next->perf, trial->perf);

    /* Update the observed value ranges. */
    for (i = 0; i < perf_n; ++i) {
        if (range[i].min > next->perf->p[i])
            range[i].min = next->perf->p[i];

        if (range[i].max < next->perf->p[i] && next->perf->p[i] < INFINITY)
            range[i].max = next->perf->p[i];
    }

    penalty = 0.0;
    base = 1.0;
    for (i = phase-1; i >= 0; --i) {
        if (next->perf->p[i] > thresh[i]) {
            if (!loose) {
                penalty += base;
            }
            penalty += (next->perf->p[i] - range[i].min) / (range[i].max -
                                                            range[i].min);
        }
        base *= 2;
    }

    if (penalty > 0.0) {
        if (loose) {
            penalty += 1.0;
        }
        next->perf->p[phase] += penalty * (range[phase].max -
                                           range[phase].min) * mult;
    }

    /* Update the best performing point, if necessary. */
    if (best_perf->p[phase] > next->perf->p[phase]) {
        hperf_copy(best_perf, next->perf);
        if (hpoint_copy(&best, &trial->point) != 0) {
            session_error( strerror(errno) );
            return -1;
        }
    }

    if (nm_algorithm() != 0) {
        session_error("Internal error: Nelder-Mead algorithm failure.");
        return -1;
    }

    if (state != SIMPLEX_STATE_CONVERGED)
        ++next_id;

    return 0;
}

/*
 * Return the best performing point thus far in the search.
 */
int strategy_best(hpoint_t *point)
{
    if (hpoint_copy(point, &best) != 0) {
        session_error("Internal error: Could not copy point.");
        return -1;
    }
    return 0;
}

int nm_algorithm(void)
{
    do {
        if (state == SIMPLEX_STATE_CONVERGED)
            break;

        if (nm_state_transition() != 0)
            return -1;

        if (state == SIMPLEX_STATE_REFLECT)
            check_convergence();

        if (nm_next_vertex() != 0)
            return -1;

    } while (vertex_outofbounds(next));

    return 0;
}

int nm_state_transition(void)
{
    double perf = next->perf->p[phase];

    switch (state) {
    case SIMPLEX_STATE_INIT:
    case SIMPLEX_STATE_SHRINK:
        /* Simplex vertex performance value. */
        if (++curr_idx == simplex_size) {
            simplex_update_centroid();
            state = SIMPLEX_STATE_REFLECT;
        }
        break;

    case SIMPLEX_STATE_REFLECT:
        if (perf < simplex_best->perf->p[phase]) {
            /* Reflected test vertex has best known performance.
             * Replace the worst performing simplex vertex with the
             * reflected test vertex, and attempt expansion.
             */
            vertex_copy(simplex_worst, test);
            state = SIMPLEX_STATE_EXPAND;
        }
        else if (perf < simplex_worst->perf->p[phase]) {
            /* Reflected test vertex performs better than the worst
             * known simplex vertex.  Replace the worst performing
             * simplex vertex with the reflected test vertex.
             */
            vertex_t *prev_worst;
            vertex_copy(simplex_worst, test);

            prev_worst = simplex_worst;
            simplex_update_centroid();

            /* If the test vertex has become the new worst performing
             * point, attempt contraction.  Otherwise, try reflection.
             */
            if (prev_worst == simplex_worst)
                state = SIMPLEX_STATE_CONTRACT;
            else
                state = SIMPLEX_STATE_REFLECT;
        }
        else {
            /* Reflected test vertex has worst known performance.
             * Ignore it and attempt contraction.
             */
            simplex_update_centroid();
            state = SIMPLEX_STATE_CONTRACT;
        }
        break;

    case SIMPLEX_STATE_EXPAND:
        if (perf < simplex_best->perf->p[phase]) {
            /* Expanded test vertex has best known performance.
             * Replace the worst performing simplex vertex with the
             * test vertex, and attempt expansion.
             */
            vertex_copy(simplex_worst, test);
        }

        simplex_update_centroid();
        state = SIMPLEX_STATE_REFLECT;
        break;

    case SIMPLEX_STATE_CONTRACT:
        if (perf < simplex_worst->perf->p[phase]) {
            /* Contracted test vertex performs better than the worst
             * known simplex vertex.  Replace the worst performing
             * simplex vertex with the test vertex.
             */
            vertex_copy(simplex_worst, test);
            simplex_update_centroid();
            state = SIMPLEX_STATE_REFLECT;
        }
        else {
            /* Contracted test vertex has worst known performance.
             * Shrink the entire simplex towards the best point.
             */
            curr_idx = -1; /* to notify the beginning of SHRINK */
            state = SIMPLEX_STATE_SHRINK;
        }
        break;

    default:
        return -1;
    }
    return 0;
}

int nm_next_vertex(void)
{
    switch (state) {
    case SIMPLEX_STATE_INIT:
        /* Test individual vertices of the initial simplex. */
        next = base->vertex[curr_idx];
        break;

    case SIMPLEX_STATE_REFLECT:
        /* Test a vertex reflected from the worst performing vertex
         * through the centroid point. */
        vertex_transform(simplex_worst, centroid, -reflect, test);
        next = test;
        break;

    case SIMPLEX_STATE_EXPAND:
        /* Test a vertex that expands the reflected vertex even
         * further from the the centroid point. */
        vertex_transform(simplex_worst, centroid, expand, test);
        next = test;
        break;

    case SIMPLEX_STATE_CONTRACT:
        /* Test a vertex contracted from the worst performing vertex
         * towards the centroid point. */
        vertex_transform(simplex_worst, centroid, contract, test);
        next = test;
        break;

    case SIMPLEX_STATE_SHRINK:
        if (curr_idx == -1) {
          /* Shrink the entire simplex towards the best known vertex
           * thus far. */
          simplex_transform(base, simplex_best, shrink, base);
          curr_idx = 0;
          next = base->vertex[curr_idx];
        } else {
          /* Test individual vertices of the initial simplex. */
          next = base->vertex[curr_idx];
        }
        break;

    case SIMPLEX_STATE_CONVERGED:
        /* Simplex has converged.  Nothing to do.
         * In the future, we may consider new search at this point. */
        next = simplex_best;
        break;

    default:
        return -1;
    }
    return 0;
}

void simplex_update_index(void)
{
    int i;

    simplex_best = base->vertex[0];
    simplex_worst = base->vertex[0];
    for (i = 1; i < simplex_size; ++i) {
        if (base->vertex[i]->perf->p[phase] < simplex_best->perf->p[phase])
            simplex_best = base->vertex[i];

        if (base->vertex[i]->perf->p[phase] > simplex_worst->perf->p[phase])
            simplex_worst = base->vertex[i];
    }
}

void simplex_update_centroid(void)
{
    int stash;

    simplex_update_index();
    stash = simplex_worst->id;
    simplex_worst->id = -1;

    simplex_centroid(base, centroid);

    simplex_worst->id = stash;
}

void check_convergence(void)
{
    int i;
    double fval_err, size_max, dist, base_val;

    if (simplex_collapsed(base) == 1)
        goto converged;

    fval_err = 0;
    base_val = centroid->perf->p[phase];
    for (i = 0; i < simplex_size; ++i) {
        fval_err += ((base->vertex[i]->perf->p[phase] - base_val) *
                     (base->vertex[i]->perf->p[phase] - base_val));
    }
    fval_err /= simplex_size;

    size_max = 0;
    for (i = 0; i < simplex_size; ++i) {
        dist = vertex_dist(base->vertex[i], centroid);
        if (size_max < dist)
            size_max = dist;
    }

    if (fval_err < fval_tol && size_max < size_tol)
        goto converged;

    return;

  converged:
    if (phase == perf_n - 1) {
        state = SIMPLEX_STATE_CONVERGED;
        session_setcfg(CFGKEY_STRATEGY_CONVERGED, "1");
    }
    else {
        nemo_phase_incr();
    }
}
