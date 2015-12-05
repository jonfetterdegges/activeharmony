//#define DEBUG 1
#undef DEBUG
#ifdef DEBUG
#define dbg(x...) fprintf(stderr, x)
#else
#define dbg(x...)
#endif

#include "strategy.h"


#include "session-core.h"
#include "hperf.h"
#include "hutil.h"
#include "hcfg.h"
#include "defaults.h"
#include "libgridpoint.h"

#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>

#define TMAX_PARAM "SA_TMAX"
#define TMAX_DEFAULT "60.0"
#define TMIN_PARAM "SA_TMIN"
#define TMIN_DEFAULT "0.5"
#define TSTEPS_PARAM "SA_TSTEPS"
#define TSTEPS_DEFAULT "200"
#define NPOINT_PARAM "SA_NPOINTS"
#define NPOINT_DEFAULT "3"
#define NSTEPS_RESET_PARAM "SA_RESET_STEPS"
#define NSTEPS_RESET_DEFAULT "20"

hcfg_info_t plugin_keyinfo[] = {
    { TMAX_PARAM, TMAX_DEFAULT,
      "Starting temperature." },
    { TMIN_PARAM, TMIN_DEFAULT,
      "Temperature where we assume convergence." },
    { TSTEPS_PARAM, TSTEPS_DEFAULT,
      "Number of temperature steps between min and max." },
    { NPOINT_PARAM, NPOINT_DEFAULT,
      "Number of parallel explorations to perform. "
      "1 means operate serially." },
    { NSTEPS_RESET_PARAM, NSTEPS_RESET_DEFAULT,
      "Number of steps to take before an exploration resets to the best "
      "known point. 0 means never reset." },
    { NULL, NULL, NULL }
};

struct lgp_info *lgp_info = NULL;

double tmax, tmin, logtmax, logtmin, dlogt;
int ntemp;

gridpoint_t *current_pts = NULL; // current location for each exploration
double *current_perfs = NULL; // performance of current_pts
gridpoint_t *generated_pts = NULL; // points returned from generate but not yet analyzed
gridpoint_t best;
double best_perf = HUGE_VAL;

hpoint_t best_hint;
double best_hint_perf = HUGE_VAL;

int n_dims;
int n_explorations;
int n_outstanding;
int *outstanding = NULL;
int *step_to_generate = NULL;
int steps_between_reset;
int next_reset_step;

// forward function declarations
int point_id(int tstep, int exploration);
int exploration_of_id(int id);
int move_point(gridpoint_t *result, gridpoint_t *origin);
double transition_prob(double perf_curr, double perf_candidate, int tstep);
void print_current(void);


int strategy_init(hsignature_t *sig) {
  int i;

  // init gridpoints here so they can be safely finalized on error
  best = GRIDPOINT_INITIALIZER;
  best_hint = HPOINT_INITIALIZER;

  tmax = hcfg_real(session_cfg, TMAX_PARAM);
  if (isnan(tmax)) {
    session_error("Error retrieving parameter " TMAX_PARAM ".");
    goto error;
  }
  if (tmax <= 0) {
    session_error("Bad value for " TMAX_PARAM ".");
    goto error;
  }

  tmin = hcfg_real(session_cfg, TMIN_PARAM);
  if (isnan(tmin)) {
    session_error("Error retrieving parameter " TMIN_PARAM ".");
    goto error;
  }
  if (tmin <= 0 || tmin > tmax) {
    session_error("Bad value for " TMIN_PARAM ".");
    goto error;
  }

  ntemp = (int) hcfg_int(session_cfg, TSTEPS_PARAM);
  if (ntemp < 0) {
    session_error("Error retrieving parameter " TSTEPS_PARAM ".");
    goto error;
  }

  n_explorations = (int) hcfg_int(session_cfg, NPOINT_PARAM);
  if (n_explorations <= 0) {
    session_error("Error retrieving parameter " NPOINT_PARAM ".");
    goto error;
  }

  steps_between_reset = (int) hcfg_int(session_cfg, NSTEPS_RESET_PARAM);
  if (steps_between_reset < 0) {
    session_error("Error retrieving parameter " NSTEPS_RESET_PARAM ".");
    goto error;
  }
  if (steps_between_reset == 0)
    next_reset_step = INT_MAX;
  else
    next_reset_step = steps_between_reset;

  // initialize convergence key
  if (session_setcfg(CFGKEY_CONVERGED, "0") != 0) {
    session_error("Could not set " CFGKEY_CONVERGED " config variable.");
    return -1;
  }

  // initialize libgridpoint
  lgp_info = libgridpoint_init(sig);
  if (!lgp_info) {
    goto error;
  }

  // candidate points per exploration
  n_dims = sig->range_len;
  // calloc candidate array so that unallocated members are NULL in case of
  // failure
  current_pts = (gridpoint_t *) calloc(n_explorations, sizeof(gridpoint_t));
  current_perfs = (double *) malloc(n_explorations * sizeof(double));
  generated_pts = (gridpoint_t *) calloc(n_explorations, sizeof(gridpoint_t));
  if (!current_pts || !current_perfs || !generated_pts)
    goto error;
  for (i = 0; i < n_explorations; i++) {
    if (gridpoint_init(lgp_info, current_pts + i) < 0)
      goto error;
    if (gridpoint_rand(lgp_info, current_pts + i) < 0)
      goto error;
    current_pts[i].point.id = point_id(0, i);
    current_perfs[i] = HUGE_VAL;
    if (gridpoint_init(lgp_info, generated_pts + i) < 0)
      goto error;
    generated_pts[i].point.id = -1;
  }

  n_outstanding = 0;
  step_to_generate = (int *) calloc(n_explorations, sizeof(int));
  outstanding = (int *) calloc(n_explorations, sizeof(int));
  if (!step_to_generate || !outstanding) {
    session_error("Failed to allocate in sim_anneal strategy_init");
    goto error;
  }

  logtmax = log(tmax);
  logtmin = log(tmin);
  dlogt = (logtmax - logtmin) / ntemp;

  if (gridpoint_init(lgp_info, &best) < 0)
    goto error;

  return 0;

 error:
  gridpoint_fini(&best);
  if (current_pts) {
    for (i = 0; i < n_explorations; i++)
      gridpoint_fini(current_pts + i);
    free(current_pts);
  }
  if (generated_pts) {
    for (i = 0; i < n_explorations; i++)
      gridpoint_fini(generated_pts + i);
    free(generated_pts);
  }
  if (current_perfs)
    free(current_perfs);
  free(step_to_generate);
  free(outstanding);
  libgridpoint_fini(lgp_info);
  return -1;
}

// if number of explorations is N
// then point IDs are {0..N-1} for the first temp step, {N..2N-1} for the second,
// etc.
// given an ID i, the exploration is i % N
int point_id(int tstep, int exploration) {
  return tstep * n_explorations + exploration;
}

int exploration_of_id(int id) {
  return id % n_explorations;
}

int strategy_fini() {
  int i;
  dbg("\nentering SA strategy_fini\n");

  gridpoint_fini(&best);
  if (current_pts) {
    for (i = 0; i < n_explorations; i++)
      gridpoint_fini(current_pts + i);
    free(current_pts);
  }
  if (generated_pts) {
    for (i = 0; i < n_explorations; i++)
      gridpoint_fini(generated_pts + i);
    free(generated_pts);
  }
  free(current_perfs);
  free(step_to_generate);
  free(outstanding);

  libgridpoint_fini(lgp_info);

  dbg("leaving SA strategy_fini\n");
  return 0;
}

/*
god dammit
here is what i need to do
each exploration has a current position
and we remember the performance at that position
in generate, we take a random step from that position
but we don't save the generated point
oh wait, we probably do
because we need to know the gridpoint coordinates
(we could regenerate them but that is stupid)
so okay, we have a "current" and "generated" gridpoint for each exploration
the generated one is a trial
when we get to analyze we do the usual SA thing to decide whether to step
if we do step, we copy "generated" to "current" in strategy_analyze
but we let strategy_generate actually take the next step
if we do NOT step, we leave "current" alone in strategy analyze
on init we initialize "current"
so those first current steps never get returned
but who cares?
and that is that
 */

void print_current() {
  int i;
  dbg("%d explorations\n", n_explorations);
  for (i = 0; i < n_explorations; i++) {
    char buf[256];
    printable_gridpoint(lgp_info, buf, 256, current_pts+i);
    dbg(" expl %d, step %d (%s), curr %s\n", i, step_to_generate[i],
        outstanding[i] ? "os" : "av", buf);
  }
}

int strategy_generate(hflow_t* flow, hpoint_t* point) {
  dbg("\nentering SA strategy_generate\n");
  print_current();

  if (n_outstanding >= n_explorations) {
    dbg("waiting for results on all explorations\n");
    flow->status = HFLOW_WAIT;
    return 0;
  }

  // which exploration should we use?
  // criteria:
  // first detect a special case: all explorations have outstanding == false and
  //   step_to_generate == next_reset_step. in this case find the exploration
  //   with the worst performance, set its current to best or best_hint
  //   (whichever is better), and step from there.
  // if we are not in the special case:
  //   among explorations with outstanding==true,
  //   use the one with the minimum step_to_generate.
  int exploration = -1;
  int reset_now = 1;
  int min_step_to_generate = INT_MAX;
  double worst_current_perf = 0.0;
  int step_with_worst_perf;
  int converged = 0;
  for (int i = 0; i < n_explorations; i++) {
    // special case detection
    if (step_to_generate[i] >= ntemp) {
      converged = 1;
      break;
    }
    if (reset_now && (outstanding[i]
                         || step_to_generate[i] != next_reset_step))
      reset_now = 0;
    if (reset_now && current_perfs[i] > worst_current_perf) {
      step_with_worst_perf = i;
      worst_current_perf = current_perfs[i];
    }
    // regular assignment
    if (!outstanding[i] && step_to_generate[i] < min_step_to_generate
        && step_to_generate[i] < next_reset_step) {
      exploration = i;
      min_step_to_generate = step_to_generate[i];
    }
  }

  if (converged) {
    dbg("converged, returning best point instead of moving\n");
    if (hpoint_copy(point, &best.point) < 0) {
      session_error("failed to copy best point in SA strategy_generate");
      return -1;
    }
    flow->status = HFLOW_ACCEPT;
    return 0;
  }

  if (reset_now) {
    // reset the current location before moving
    next_reset_step += steps_between_reset;
    exploration = step_with_worst_perf;
    dbg("resetting exploration %d\n", exploration);
    if (best_hint_perf < best_perf) {
      // some other strategy has the global best; search from there
      if (gridpoint_from_hpoint(lgp_info, &best_hint, current_pts+exploration) < 0)
        return -1;
    } else {
      // we have the global best; search from it
      if (gridpoint_copy(lgp_info, current_pts+exploration, &best) < 0)
        return -1;
    }
  }

  if (exploration == -1) {
    // this happens when some explorations are waiting at the reset point, and
    // others are waiting for analysis; in this case we just wait
    dbg("waiting for all explorations to reach reset point %d\n", next_reset_step);
    flow->status = HFLOW_WAIT;
    return 0;
  }

  // now move from the current point
  dbg("generating for exploration %d\n", exploration);
  if (move_point(generated_pts+exploration, current_pts+exploration) < 0) {
    session_error("Failed to step from current point in SA strategy_generate");
    return -1;
  }
  generated_pts[exploration].point.id = point_id(step_to_generate[exploration],
                                                 exploration);
  if (hpoint_copy(point, &generated_pts[exploration].point) < 0) {
    session_error("Failed hpoint_copy in SA strategy_generate");
    return -1;
  }

  // prepare for the next generate call
  // (analyze will increment step_to_generate)
  outstanding[exploration] = 1;
  n_outstanding++;

  flow->status = HFLOW_ACCEPT;
  return 0;
}

int strategy_rejected(hflow_t* flow, hpoint_t* point) {
  dbg("\nentering SA strategy_rejected\n");
  hpoint_t* hint = &flow->point;
  int orig_id = point->id;
  int exploration = exploration_of_id(orig_id);

  if (hint && hint->id != -1) {
    // accept the hint, and update the generated_pts entry
    if (gridpoint_from_hpoint(lgp_info, hint, generated_pts+exploration) < 0) {
      session_error("Couldn't create gridpoint from hint in SA reject.");
      return -1;
    }
    if (hpoint_copy(point, hint) != 0) {
      session_error("Could not copy hint during reject.");
      return -1;
    }
  } else {
    // try another move from the exploration's current point
    if (move_point(generated_pts+exploration, current_pts+exploration) < 0) {
      session_error("Failed to step from current point in SA strategy_reject");
      return -1;
    }
    if (hpoint_copy(point, &generated_pts[exploration].point) < 0) {
      session_error("Couldn't copy new point in SA strategy_reject");
      return -1;
    }
  }

  flow->status = HFLOW_ACCEPT;
  return 0;
}

int move_point(gridpoint_t *result, gridpoint_t *origin) {
  // decide which way and how far to move
  // TODO consider tracking visited points and excluding them
  // TODO consider moving farther on earlier steps
  int steps_taken;
  do {
    // choose a random dimension and direction as follows:
    // generate a random floating-point number in [0, ndims)
    // if it's in [i, i+0.5), step +1 in direction i
    // if it's in [i+0.5, i+1) step -1 in direction i
    double rand = n_dims * drand48();
    int tgt_dim = (int) rand;
    int dir = 1;
    if (rand - (double) tgt_dim >= 0.5)
      dir = -1;
    steps_taken = gridpoint_move(lgp_info, origin, tgt_dim, dir, result);
    if (steps_taken < 0)
      return -1;
  } while (steps_taken < 1);

  return 0;
}

int strategy_analyze(htrial_t* trial) {
  dbg("\nentering SA strategy_analyze\n");

  int exploration = exploration_of_id(trial->point.id);

  // "candidate" is the point just returned from exploration
  gridpoint_t *candidate = generated_pts + exploration;
  if (trial->point.id != candidate->point.id)
    // wrong point; skip
    return 0;
  double perf_candidate = hperf_unify(trial->perf);

#ifdef DEBUG
  char gpt_buf[256];
  dbg(" expl %d\n", exploration);
  printable_gridpoint(lgp_info, gpt_buf, 256, current_pts+exploration);
  dbg(" curr perf %.4g, pt %s\n", current_perfs[exploration], gpt_buf);
  printable_gridpoint(lgp_info, gpt_buf, 256, candidate);
  dbg(" cand perf %.4g, pt %s\n", perf_candidate, gpt_buf);
#endif

  // two things to do now: (1) update "best" if needed, (2) update current
  // point according to the transition probability. these are independent:
  // neither implies the other.
  if (perf_candidate < best_perf) {
    if (gridpoint_copy(lgp_info, &best, candidate) < 0)
      return -1;
    best_perf = perf_candidate;
    dbg(" new best\n");
  }

  double prob = transition_prob(current_perfs[exploration], perf_candidate,
                                step_to_generate[exploration]);
  dbg(" temp %.4g, prob %.4g, ",
      exp(logtmax - dlogt * step_to_generate[exploration]), prob);
  if (prob > drand48()) {
    // change "current" to the candidate point
    if (gridpoint_copy(lgp_info, current_pts+exploration, candidate) < 0)
      return -1;
    current_perfs[exploration] = perf_candidate;
    dbg("moved\n");
  } else {
    dbg("stayed\n");
  }

  n_outstanding--;
  outstanding[exploration] = 0;
  step_to_generate[exploration]++;

  // finally, if step_to_generate for any exploration has reached max, indicate
  // convergence.
  if (step_to_generate[exploration] > ntemp) {
    if (session_setcfg(CFGKEY_CONVERGED, "1") != 0) {
      session_error("Couldn't set convergence flag in SA");
      return -1;
    }
  }

  return 0;
}

double transition_prob(double perf_curr, double perf_candidate, int tstep) {
  double temp = exp(logtmax - dlogt * tstep);
  double prob = exp((perf_curr - perf_candidate) / temp);
  return prob;
}

int strategy_hint(htrial_t *trial) {
  // track global optimum; will consider on reset
  double perf = hperf_unify(trial->perf);
  if (perf < best_hint_perf) {
    if (hpoint_copy(&best_hint, &trial->point) < 0)
      return -1;
    best_hint_perf = perf;
  }

  return 0;
}

int strategy_best(hpoint_t *point) {
  dbg("\nentering SA strategy_best\n");
  if (hpoint_copy(point, &best.point) < 0) {
    session_error("Failed to copy in SA strategy_best.");
    return -1;
  }
  return 0;
}
