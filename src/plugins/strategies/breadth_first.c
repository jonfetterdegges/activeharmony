/*
 * Copyright 2003-2015 Jeffrey K. Hollingsworth
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

/**
 * \page exhaustive Exhaustive (exhaustive.so)
 *
 * This search strategy starts with the minimum-value point (i.e.,
 * using the minimum value for each tuning variable), and incrementing
 * the tuning variables like an odometer until the maximum-value point
 * is reached.  This strategy is guaranteed to visit all points within
 * a search space.
 *
 * It is mainly used as a basis of comparison for more intelligent
 * search strategies.
 */

#include "strategy.h"
#include "session-core.h"
#include "hperf.h"
#include "hutil.h"
#include "hcfg.h"
#include "queues.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>

/*
 * Configuration variables used in this plugin.
 * These will automatically be registered by session-core upon load.
 */
hcfg_info_t plugin_keyinfo[] = {
	{ CFGKEY_PASSES, "1",
	  "Number of passes through the search space before the search "
	  "is considered converged." },
	{ CFGKEY_INIT_POINT, NULL,
	  "Initial point begin testing from." },
	{ NULL }
};

typedef struct curr_node{
	hpoint_t point;
	unsigned long* idx;
}curr_n_t;


hpoint_t best;
double   best_perf;

/* Forward function definitions. */
int strategy_cfg(hsignature_t* sig);
int get_next_point(void);
int generate_all_successors(void);

/* Variables to track current search state. */
int N;
hrange_t* range;

curr_n_t curr;

int remaining_passes = 1;
int final_id = 0;
int outstanding_points = 0, final_point_received = 0;
int global_point_id = 0;

queue_node_t* queue_head = NULL;
visited_node_t *vqueue_head = NULL;

void print_curr(void){
  fprintf(stderr, "Point #%d: (", curr.point.id);
  for (int i = 0; i < curr.point.n; ++i) {
      hval_t* v = &curr.point.val[i];
      if (i > 0) fprintf(stderr, ",");

      switch (v->type) {
      case HVAL_INT:  fprintf(stderr, "%ld", v->value.i); break;
      case HVAL_REAL: fprintf(stderr, "%lf", v->value.r); break;
      case HVAL_STR:  fprintf(stderr, "\"%s\"", v->value.s); break;
      default:
          session_error("Invalid point value type");
          return;
      }
  }
  fprintf(stderr, ")\n");
}

/*
 * Invoked once on strategy load.
 */
int strategy_init(hsignature_t* sig)
{
	N = sig->range_len;
	range = sig->range;

	if (!curr.idx) {
		/* One time memory allocation and/or initialization. */
		if (hpoint_init(&curr.point, N) != 0) {
			session_error("Could not allocate memory candidate point.");
			return -1;
		}

		curr.idx = malloc(N * sizeof(unsigned long));
		if (!curr.idx) {
			session_error("Could not allocate memory for index array.");
			return -1;
		}

		/* The best point and trial counter should only be initialized once,
		 * and thus be retained across a restart.
		 */
		best = HPOINT_INITIALIZER;
		best_perf = HUGE_VAL;

		curr.point.id = 1;
		global_point_id = 1;

		queue_head = NULL;
	}

	/* Initialization for subsequent calls to strategy_init(). */
	if (strategy_cfg(sig) != 0)
		return -1;

	if (session_setcfg(CFGKEY_CONVERGED, "0") != 0) {
		session_error("Could not set " CFGKEY_CONVERGED " config variable.");
		return -1;
	}
	return 0;
}

int strategy_cfg(hsignature_t* sig)
{
	const char* cfgstr;
	int i;

	remaining_passes = hcfg_int(session_cfg, CFGKEY_PASSES);
	if (remaining_passes < 0) {
		session_error("Invalid value for " CFGKEY_PASSES ".");
		return -1;
	}

	cfgstr = hcfg_get(session_cfg, CFGKEY_INIT_POINT);
	if (cfgstr) {
		if (hpoint_parse(&curr.point, sig, cfgstr) != 0) {
			session_error("Error parsing point from " CFGKEY_INIT_POINT ".");
			return -1;
		}

		if (hpoint_align(&curr.point, sig) != 0) {
			session_error("Error aligning point to parameter space.");
			return -1;
		}

		for (i = 0; i < sig->range_len; ++i) {
			switch (sig->range[i].type) {
			case HVAL_INT:
				curr.idx[i] = hrange_int_index(&sig->range[i].bounds.i,
										  curr.point.val[i].value.i);
				break;
			case HVAL_REAL:
				curr.idx[i] = hrange_real_index(&sig->range[i].bounds.r,
										   curr.point.val[i].value.r);
				break;
			case HVAL_STR:
				curr.idx[i] = hrange_str_index(&sig->range[i].bounds.s,
										  curr.point.val[i].value.s);
				break;

			default:
				session_error("Invalid type detected in signature.");
				return -1;
			}
		}
		global_point_id = curr.point.id;
	}
	else {
		for (i = 0; i < N; ++i) {
			hval_t* v = &curr.point.val[i];

			v->type = range[i].type;
			switch (range[i].type) {
			case HVAL_INT:  v->value.i = range[i].bounds.i.min; break;
			case HVAL_REAL: v->value.r = range[i].bounds.r.min; break;
			case HVAL_STR:  v->value.s = range[i].bounds.s.set[0]; break;
			default:
				session_error("Invalid range detected during strategy init.");
				return -1;
			}
		}
		memset(curr.idx, 0, N * sizeof(unsigned long));
	}

	queue_head = queue_push_back(queue_head, &curr.point, curr.idx, N);
	vqueue_head = vqueue_push_back(vqueue_head, curr.idx, N);
	return 0;
}

/*
 * Generate a new candidate configuration.
 */
int strategy_generate(hflow_t* flow, hpoint_t* point)
{
	if (remaining_passes > 0) {

    if (get_next_point() < 0){
      return -1;
    }

		if (hpoint_copy(point, &curr.point) != 0) {
			session_error("Could not copy current point during generation.");
			return -1;
		}

	}else {
		if (hpoint_copy(point, &best) != 0) {
			session_error("Could not copy best point during generation.");
			return -1;
		}
	}

  /* every time we send out a point that's before
     the final point, increment the numebr of points
     we're waiting for results from */
  if(! final_id || curr.point.id <= final_id)
    outstanding_points++;

	flow->status = HFLOW_ACCEPT;
	return 0;
}

/*
 * Regenerate a point deemed invalid by a later plug-in.
 */
int strategy_rejected(hflow_t* flow, hpoint_t* point)
{
	hpoint_t* hint = &flow->point;
	int orig_id = point->id;

	if (hint && hint->id != -1) {
		if (hpoint_copy(point, hint) != 0) {
			session_error("Could not copy hint during reject.");
			return -1;
		}
	}
	else {
    if (get_next_point() != 0){
      return -1;
    }

		if (hpoint_copy(point, &curr.point) != 0) {
			session_error("Could not copy current point during reject.");
			return -1;
		}

	}
	point->id = orig_id;

	flow->status = HFLOW_ACCEPT;
	return 0;
}

/*
 * Analyze the observed performance for this configuration point.
 */
int strategy_analyze(htrial_t* trial)
{
	double perf = hperf_unify(trial->perf);

	if (best_perf > perf) {
		best_perf = perf;
		if (hpoint_copy(&best, &trial->point) != 0) {
			session_error("Internal error: Could not copy point.");
			return -1;
		}
	}

	if (trial->point.id == final_id) {
		if (session_setcfg(CFGKEY_CONVERGED, "1") != 0) {
			session_error("Internal error: Could not set convergence status.");
			return -1;
		}
	}

  /* decrement the number of points we're waiting for
     when we get a point back that was generated before
     the final point */
  if(! final_id || trial->point.id <= final_id)
      outstanding_points--;

  if (trial->point.id == final_id)
      final_point_received = 1;

	/* converged when the final point has been received,
	   and there are no outstanding points */
	if(outstanding_points <= 0 && final_point_received) {
		if (session_setcfg(CFGKEY_CONVERGED, "1") != 0) {
			session_error("Internal error: Could not set convergence status.");
			return -1;
		}
	}

	return 0;
}

/*
 * Return the best performing point thus far in the search.
 */
int strategy_best(hpoint_t* point)
{
	if (hpoint_copy(point, &best) != 0) {
		session_error("Could not copy best point during request for best.");
		return -1;
	}
	return 0;
}

int get_next_point(void){
	if(queue_head != NULL){
		curr.point = queue_head->point;
		memcpy(curr.idx, queue_head->idx, N*sizeof(unsigned long));
		queue_head = queue_pop(queue_head);
		if(generate_all_successors() == 0){
			//Convergence
			if(queue_head == NULL){
				final_id = curr.point.id;
				return 1;
			}else{
				return 0;
			}
		}else{
			return -1;
		}
	}
	return -1;
}

int generate_all_successors(void)
{
	double next_r, prev_r;
	if (remaining_passes <= 0)
		return 0;

	for (int i = 0; i < N; ++i) {

		switch (range[i].type) {
		case HVAL_INT:
			if (curr.point.val[i].value.i + range[i].bounds.i.step < range[i].bounds.i.max) {
				hpoint_t new_point = HPOINT_INITIALIZER;
				hpoint_copy(&new_point, &curr.point);
				new_point.id = ++global_point_id;
				new_point.val[i].value.i += range[i].bounds.i.step;
				curr.idx[i]++;
				if(vqueue_contains(vqueue_head, curr.idx, N) != 0){
					queue_head = queue_push_back(queue_head, &new_point, curr.idx, N);
					vqueue_head = vqueue_push_back(vqueue_head, curr.idx, N);
				}
				curr.idx[i]--;
			}

			if (curr.point.val[i].value.i - range[i].bounds.i.step > range[i].bounds.i.min &&
				  curr.idx[i] >= 1) {
				hpoint_t new_point = HPOINT_INITIALIZER;
				hpoint_copy(&new_point, &curr.point);
				new_point.id = ++global_point_id;
				new_point.val[i].value.i -= range[i].bounds.i.step;
				curr.idx[i]--;
				if(vqueue_contains(vqueue_head, curr.idx, N) != 0){
					queue_head = queue_push_back(queue_head, &new_point, curr.idx, N);
					vqueue_head = vqueue_push_back(vqueue_head, curr.idx, N);
				}
				curr.idx[i]++;
			}
			break;

		case HVAL_REAL:
			next_r = (range[i].bounds.r.step > 0.0)
				? range[i].bounds.r.min + ((curr.idx[i]+1) * range[i].bounds.r.step)
				: nextafter(curr.point.val[i].value.r, HUGE_VAL);

			prev_r = (range[i].bounds.r.step > 0.0)
				? range[i].bounds.r.min + ((curr.idx[i]-1) * range[i].bounds.r.step)
				: nextafter(curr.point.val[i].value.r, -HUGE_VAL);

			if (next_r <= range[i].bounds.r.max &&
				next_r != curr.point.val[i].value.r)
			{
				hpoint_t new_point = HPOINT_INITIALIZER;
				hpoint_copy(&new_point, &curr.point);
				new_point.id = ++global_point_id;
				new_point.val[i].value.r = next_r;
				curr.idx[i]++;
				if(vqueue_contains(vqueue_head, curr.idx, N) != 0){
					queue_head = queue_push_back(queue_head, &new_point, curr.idx, N);
					vqueue_head = vqueue_push_back(vqueue_head, curr.idx, N);
				}
				curr.idx[i]--;
			}

			if (prev_r >= range[i].bounds.r.min &&
				prev_r != curr.point.val[i].value.r &&
				curr.idx[i] >= 1)
			{
				hpoint_t new_point = HPOINT_INITIALIZER;
				hpoint_copy(&new_point, &curr.point);
				new_point.id = ++global_point_id;
				new_point.val[i].value.r = prev_r;
				curr.idx[i]--;
				if(vqueue_contains(vqueue_head, curr.idx, N) != 0){
					queue_head = queue_push_back(queue_head, &new_point, curr.idx, N);
					vqueue_head = vqueue_push_back(vqueue_head, curr.idx, N);
				}
				curr.idx[i]++;
			}
			break;

		case HVAL_STR:
			if (curr.idx[i] + 1 < range[i].bounds.s.set_len) {
				hpoint_t new_point = HPOINT_INITIALIZER;
				hpoint_copy(&new_point, &curr.point);
				new_point.id = ++global_point_id;
				new_point.val[i].value.s = range[i].bounds.s.set[curr.idx[i] + 1];
				curr.idx[i] += 1;
				if(vqueue_contains(vqueue_head, curr.idx, N) != 0){
					queue_head = queue_push_back(queue_head, &new_point, curr.idx, N);
					vqueue_head = vqueue_push_back(vqueue_head, curr.idx, N);
				}
				curr.idx[i] -=1;
			}

			if (curr.idx[i] >= 1) {
				hpoint_t new_point = HPOINT_INITIALIZER;
				hpoint_copy(&new_point, &curr.point);
				new_point.id = ++global_point_id;
				new_point.val[i].value.s = range[i].bounds.s.set[curr.idx[i] - 1];
				curr.idx[i] -= 1;
				if(vqueue_contains(vqueue_head, curr.idx, N) != 0){
					queue_head = queue_push_back(queue_head, &new_point, curr.idx, N);
					vqueue_head = vqueue_push_back(vqueue_head, curr.idx, N);
				}
				curr.idx[i] +=1;
			}
			break;
		default:
			session_error("Invalid range detected.");
			return -1;
		}
	}

	return 0;
}