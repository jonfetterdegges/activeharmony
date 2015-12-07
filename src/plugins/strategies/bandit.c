/*
 * Multi-Armed Bandit meta-search algorithm
 */

/*
 * **Configuration Variables**
 * Key                | Type       | Default | Description
 * ------------------ | ---------- | ------- | -----------
 * BANDIT_STRATEGIES  | String     | (none)  | Colon-separated list of strategies to invoke
 * BANDIT_WINDOW_SIZE | Integer    | 32      | Number of results to keep in the sliding window
 * BANDIT_TRADEOFF    | Double     | 1.0     | Tradeoff between exploration and exploitation; higher means switch strategies more often
 */

#undef DEBUG
//#define DEBUG 1
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

#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

typedef struct strategy {
  char *name;
  void *lib;
  strategy_generate_t generate;
  strategy_rejected_t rejected;
  strategy_analyze_t analyze;
  strategy_analyze_t hint;
  strategy_best_t best;
  hook_init_t init;
  hook_join_t join;
  hook_setcfg_t setcfg;
  hook_fini_t fini;
} strategy_t;

strategy_t *strategies = NULL;
int strat_count = 0;
int strat_cap = 0;

const char *errmsg = NULL;

hpoint_t best;
double best_perf;

int *converged = NULL;
hpoint_t *best_per_strategy = NULL;
double *best_perf_per_strategy = NULL;

#define STRATEGIES_PARAM  "BANDIT_STRATEGIES"
#define WINDOW_SIZE_PARAM "BANDIT_WINDOW_SIZE"
#define TRADEOFF_PARAM    "BANDIT_TRADEOFF"

hcfg_info_t plugin_keyinfo[] = {
    { STRATEGIES_PARAM, NULL,
      "Colon-separated list of strategies to invoke." },
    { WINDOW_SIZE_PARAM, "32",
      "Number of results to keep in the sliding window." },
    { TRADEOFF_PARAM, "1.0",
      "Tradeoff between exploration and exploitation; "
      "higher means switch strategies more often." },
    { NULL, NULL, NULL }
};

// have to keep MAX_NUM_STRATEGIES in sync with POINT_ID_MASK and POINT_ID_BITS;
// the strategy ID fits in the remaining bits (with a zero sign bit)
#define MAX_NUM_STRATEGIES 128
#define POINT_ID_MASK 0x00ffffff
#define POINT_ID_BITS 24

int window_size;  // size of sliding window in MAB
double tradeoff;  // exploration-exploitation tradeoff constant C
double explore_factor;  // tradeoff * sqrt(2.0 * log2(window_size))

/*
 * The sliding window of results per strategy. This is actually implemented
 * as a ring buffer. The total population is stored in window_population.
 * Unoccupied spaces have strategy_id == -1. The location where we'll store
 * the next result is in window_next.
 */
typedef struct subtrial {
  int strategy_id; // t (as index in strategies[]), or -1 if not used
  int point_id;    // (meta-strategy) ID of the point for this subtrial
  int complete;    // have we yet gotten the analyze call for this point?
  int new_best;    // V_{t,i} in the OpenTuner paper: 1 if this subtrial
                   // found a new global optimum, 0 if not
} subtrial_t;

subtrial_t *window = NULL;
int window_population;
int window_next;
int requests_outstanding;  // number of generate calls not yet analyzed

// forward function declarations
int init_sliding_window(void);
void free_strategies(void);
void free_strat(strategy_t *strat);
int parse_strategies(const char *list);
int translate_id_from_sub(int strategy_id, int point_id);
int translate_id_to_sub(int point_id, int *strategy_id);
int assign_scores(double *scores);
int test_convergence(void);
int clear_convergence(void);

int strategy_init(hsignature_t *sig) {
  int retval;

  dbg("Starting init\n");

  best = HPOINT_INITIALIZER;
  best_perf = INFINITY;

  // Load constants for sliding window and tradeoff constant
  window_size = hcfg_int(session_cfg, WINDOW_SIZE_PARAM);
  if (window_size <= 0) {
    session_error("Invalid " WINDOW_SIZE_PARAM);
    return -1;
  }

  tradeoff = hcfg_real(session_cfg, TRADEOFF_PARAM);
  if (tradeoff <= 0.0) {
    session_error("Invalid " TRADEOFF_PARAM);
    return -1;
  }
  explore_factor = tradeoff * sqrt(2.0 * log2((double) window_size));
  
  // Load the sub-strategies and run their init's in order
  const char *strats = hcfg_get(session_cfg, STRATEGIES_PARAM);
  if (!strats) {
    session_error("No strategy list provided in " STRATEGIES_PARAM);
    return -1;
  }

  retval = parse_strategies(strats);
  dbg("Finished parsing strategies.\n");
  if (retval < 0) {
    session_error(errmsg 
		  ? errmsg 
		  : "Error parsing strategy list (allocation filed?)");
    return -1;
  }
  dbg("strat_count is %d\n", strat_count);
  if (strat_count > MAX_NUM_STRATEGIES) {
    session_error("Too many strategies specified.");
    free_strategies();
    return -1;
  }

  best_per_strategy = (hpoint_t *) malloc(strat_count * sizeof(hpoint_t));
  best_perf_per_strategy = (double *) malloc(strat_count * sizeof(double));
  converged = (int *) calloc(strat_count, sizeof(int));
  if (best_per_strategy == NULL
      || best_perf_per_strategy == NULL
      || converged == NULL) {
    session_error("Couldn't allocate memory for control structures.");
    free_strategies();
    free(best_per_strategy);
    free(best_perf_per_strategy);
    free(converged);
    return -1;
  }

  // code above this should be run once; below this should be run on
  // every restart---unless we want to allow window size and tradeoff
  // to change on restart?

  dbg("Initializing sliding window.\n");
  if (init_sliding_window() < 0) {
    free_strategies();
    free(best_per_strategy);
    free(best_perf_per_strategy);
    return -1;
  }

  int i;
  for (i = 0; i < strat_count; i++) {
    dbg("Initializing strategy %d.\n", i);
    hook_init_t init = strategies[i].init;
    if (init) {
      dbg(" running strategy_init\n");
      retval = (strategies[i].init)(sig);
      dbg(" returned %d\n", retval);
      if (retval < 0) {
	// sub-strategy should have called session_error
	return retval;
      }
    } else {
      dbg("strategy %d, no init\n", i);
    }
  }

  if (clear_convergence() < 0)
    return -1;

  // TODO I have not handled restarts; need to understand that better.

  return retval;
}

int init_sliding_window(void) {
  int i;

  if (window != NULL) {
    // this is a restart, so we need to reset the old contents
    // we free the old buffer because the size might have changed
    free(window);
  }
  
  window = (subtrial_t *) malloc(window_size * sizeof(subtrial_t));
  if (!window) {
    session_error("Couldn't allocate memory for sliding window.");
    return -1;
  }

  for (i = 0; i < window_size; i++) {
    window[i].strategy_id = -1;
  }
  window_population = 0;
  window_next = 0;
  requests_outstanding = 0;

  return 0;
}

// stolen from session-core.c
#define dlfptr(x, y) ((void (*)(void))(long)(dlsym((x), (y))))

int parse_strategies(const char *list) {
  /* Much code stolen from session-core.c's load_layers() */
  const char *prefix, *end;
  char *path;
  int path_len, retval;
  void *lib;
  strategy_t *strat;
  char *name;

  path = NULL;
  name = NULL;
  path_len = 0;
  retval = 0;
  strat_count = 0;
  strat_cap = 0;
  while (list && *list) {
    if (strat_count == strat_cap) {
      if (array_grow(&strategies, &strat_cap, sizeof(strategy_t)) < 0) {
	retval = -1;
	goto cleanup;
      }
    }
    end = strchr(list, ':');
    if (!end)
      end = list + strlen(list);

    name = (char *) malloc((end - list + 1) * sizeof(char));
    if (!name) {
      retval = -1;
      goto cleanup;
    }
    strncpy(name, list, end - list);
    name[end-list] = '\0';

    prefix = hcfg_get(session_cfg, CFGKEY_HARMONY_HOME);
    if (snprintf_grow(&path, &path_len, "%s/libexec/%.*s",
		      prefix, end - list, list) < 0)
      {
	retval = -1;
	goto cleanup;
      }

    lib = dlopen(path, RTLD_LAZY | RTLD_LOCAL);
    if (lib == NULL) {
      errmsg = dlerror();
      retval = -1;
      goto cleanup;
    }

    strat = strategies + strat_count;

    strat->name = stralloc(name);
    strat->lib = lib;

    // load function pointers into the data structure
    strat->generate = (strategy_generate_t) dlfptr(lib, "strategy_generate");
    strat->rejected = (strategy_rejected_t) dlfptr(lib, "strategy_rejected");
    strat->analyze  =  (strategy_analyze_t) dlfptr(lib, "strategy_analyze");
    strat->hint     =  (strategy_analyze_t) dlfptr(lib, "strategy_hint");
    strat->best     =     (strategy_best_t) dlfptr(lib, "strategy_best");

    // make sure the required functions are present
    if (!(strat->generate
	  && strat->rejected
	  && strat->analyze
	  && strat->best)) {
      errmsg = "Sub-strategy does not define all required functions";
      retval = -1;
      goto cleanup;
    }

    strat->init     =         (hook_init_t) dlfptr(lib, "strategy_init");
    strat->join     =         (hook_join_t) dlfptr(lib, "strategy_join");
    strat->setcfg   =       (hook_setcfg_t) dlfptr(lib, "strategy_setcfg");
    strat->fini     =         (hook_fini_t) dlfptr(lib, "strategy_fini");

    hcfg_info_t *keyinfo = dlsym(lib, "plugin_keyinfo");
    if (keyinfo) {
      if (hcfg_reginfo((hcfg_t *) session_cfg, keyinfo) != 0) {
        errmsg = "Error registering strategy configuration keys.";
        return -1;
      }
    }

    name = NULL; // to avoid freeing it in cleanup
    lib = NULL;  // avoid closing in cleanup

    // success; prepare for the loop test and next iteration
    ++strat_count;
    if (*end)
      list = end + 1;
    else
      list = NULL;
  }

 cleanup:
  free(path);
  free(name);
  if (lib)
    dlclose(lib);
  if (retval == -1) {
    // error exit; free allocated space for names, close shared libs
    int i;
    for (i = 0; i < strat_count; i++)
      free_strat(strategies+i);
  }
  return retval;
}

int strategy_fini(void) {
  int retval = 0;
  int i;

  for (i = 0; i < strat_count; i++) {
    // finalize all strategies; if there are any errors, we'll let the
    // last session_error call govern (since there is no way to retrieve
    // its value).
    if (strategies[i].fini)
      retval |= (strategies[i].fini)();
  }
  if (retval)
    retval = -1;
  free_strategies();

  // other things to free
  free(window);
  free(best_per_strategy);
  free(best_perf_per_strategy);
  free(converged);

  return retval;
}

void free_strategies(void) {
  int i;
  for (i = 0; i < strat_count; i++) {
    free_strat(strategies + i);
  }
  free(strategies);
}

// Nothing special to be done for join, setcfg; we just pass the
// call to the sub-strategies.

int strategy_join(const char *id) {
  int i;
  int retval = 0;

  for (i = 0; i < strat_count; i++) {
    if (strategies[i].join)
      retval |= (strategies[i].join)(id);
  }
  return retval;
}

int strategy_setcfg(const char *key, const char *val) {
  int i;
  int retval = 0;

  for (i = 0; i < strat_count; i++) {
    if (strategies[i].setcfg)
      retval |= (strategies[i].setcfg)(key, val);
  }
  return retval;
}

/*
 * Point generation: depends on sliding window of recent results
 */


int strategy_generate(hflow_t *flow, hpoint_t *point) {
  double scores[MAX_NUM_STRATEGIES];

#ifdef DEBUG
  int buflen = 100;
  char *buf = (char *) malloc(buflen*sizeof(char));
  char *bbuf = buf;
#endif

  // don't let the generates get too far ahead of the analyses; otherwise
  // we might wind up chasing down a strategy that is doing poorly
  // TODO make max requests outstanding a parameter?
  if (requests_outstanding > (window_size / 4)) {
    flow->status = HFLOW_WAIT;
    return 0;
  }

  dbg("Entering assign_scores\n");
  int best_strategy = assign_scores(scores);
  dbg("best_strategy is %d\n", best_strategy);
  if (best_strategy < 0) {
    // session_error set below
    return -1;
  }
  // assign_scores tells us which strategy had the highest score. we will
  // just use that, unless that strategy wants to HFLOW_WAIT. in that case,
  // we'll go to the second best, and so forth.
  // I assume strategies won't pay attention to the "point" field in flow.
  do {
    dbg("calling strategy %d generate\n", best_strategy);
    int result = strategies[best_strategy].generate(flow, point);
    if (result < 0)
      return -1;
    if (flow->status == HFLOW_WAIT) {
      dbg("strategy %d wants to wait\n", best_strategy);
      // find the next best strategy
      scores[best_strategy] = -1.0;
      best_strategy = -1;
      double best_score = -1.0;
      int i;
      for (i = 0; i < strat_count; i++)
	if (!converged[i] && scores[i] > best_score) {
	  best_score = scores[i];
	  best_strategy = i;
	}
      dbg("new best strategy is %d\n", best_strategy);
    }
    #ifdef DEBUG
      if (flow->status != HFLOW_WAIT) {
        buf = bbuf;
        if (hpoint_serialize(&buf, &buflen, point) < 0)
          return -1;
        dbg("strategy %d generated %s\n", best_strategy, bbuf);
      }
    #endif
  } while (best_strategy >= 0 && flow->status == HFLOW_WAIT);

  if (best_strategy < 0) {
    // all strategies want to wait (or have converged), so we will wait.
    flow->status = HFLOW_WAIT;
    return 0;
  }

  // best_strategy has generated a point. we modify the id so we can remember
  // who is responsible for it.
  point->id = translate_id_from_sub(best_strategy, point->id);
  if (point->id < 0)
    return -1;

  // save the request in the window
  if (window_next < 0 || window_next >= window_size 
      || window_next > window_population) {
    session_error("Invalid window_next value.");
    return -1;
  }
  subtrial_t *subtrial = window + window_next;
  subtrial->strategy_id = best_strategy;
  subtrial->point_id = point->id;
  subtrial->complete = subtrial->new_best = 0;
  window_next++;
  if (window_next == window_size)
    window_next = 0;
  if (window_population < window_size)
    window_population++;

  requests_outstanding++;
#ifdef DEBUG
  free(bbuf);
#endif
  return 0;
}

int assign_scores(double *scores) {
  int istrat;
  int isubtrial;
  int usage_counts[MAX_NUM_STRATEGIES];
  int result_counts[MAX_NUM_STRATEGIES];
  int auc[MAX_NUM_STRATEGIES];
  subtrial_t *subtrial;
  double best_score = -1.0;
  int best_strategy = -1;

  memset(usage_counts, '\0', (MAX_NUM_STRATEGIES)*sizeof(int));
  memset(result_counts, '\0', (MAX_NUM_STRATEGIES)*sizeof(int));
  memset(auc, '\0', (MAX_NUM_STRATEGIES)*sizeof(int));
  for (isubtrial = 0; isubtrial < window_population; isubtrial++) {
    subtrial = window+isubtrial;
    if (subtrial->strategy_id < 0 
	|| subtrial->strategy_id >= MAX_NUM_STRATEGIES) {
      session_error("Internal error: unexpected invalid strategy id");
      return -1;
    }
    usage_counts[subtrial->strategy_id]++;
    if (subtrial->complete)
      result_counts[subtrial->strategy_id]++;
    if (subtrial->new_best) // implies complete
      auc[subtrial->strategy_id] += usage_counts[subtrial->strategy_id];
  }
  for (istrat = 0; istrat < strat_count; istrat++) {
    if (converged[istrat]) {
      // if it says it's converged, we'll prefer any other strategy
      dbg("Strategy %d: converged\n", istrat);
      scores[istrat] = 0.0;
    } else {
      // handle "unused" and "used but no results" separately
      if (usage_counts[istrat] == 0) {
        scores[istrat] = HUGE_VAL;
      } else {
        double nuses = (double) usage_counts[istrat];
        double nresults = (double) result_counts[istrat];
        if (nresults == 0.0)
          nresults = 1.0;  // auc will be zero so the exploitation score is 0 anyway
        scores[istrat] = 2.0 * auc[istrat] / nresults / (nresults + 1.0);
        scores[istrat] += explore_factor / sqrt(nuses);
      }
      dbg("Strategy %d: score %.3lf\n", istrat, scores[istrat]);
    }
    if (scores[istrat] > best_score) {
      best_strategy = istrat;
      best_score = scores[istrat];
    }
  }
  return best_strategy;
}

int strategy_rejected(hflow_t *flow, hpoint_t *point) {
  int strategy_id, sub_point_id, orig_point_id;
  // We always let the original sub-strategy regenerate its own point.
  orig_point_id = point->id;
  sub_point_id = translate_id_to_sub(point->id, &strategy_id);
  if (strategy_id < 0 || strategy_id >= MAX_NUM_STRATEGIES) {
    session_error("Invalid strategy ID in strategy_rejected.");
    return -1;
  }

  if (flow->point.id == orig_point_id)
    flow->point.id = sub_point_id;
  point->id = sub_point_id;
  int result = strategies[strategy_id].rejected(flow, point);
  point->id = translate_id_from_sub(strategy_id, point->id);
  return result;
}

int test_convergence() {
  char *cvg = hcfg_get(session_cfg, CFGKEY_CONVERGED);
  return (cvg && cvg[0] == '1') ? 1 : 0;  // this is what the client tests
}

int clear_convergence() {
  if (session_setcfg(CFGKEY_CONVERGED, "0") < 0) {
    session_error("failed to unset convergence");
    return -1;
  }
  return 0;
}

int strategy_analyze(htrial_t *trial) {
  int strategy_id, sub_point_id;
  int converged_before, converged_after;

  sub_point_id = translate_id_to_sub(trial->point.id, &strategy_id);
  if (strategy_id < 0 || strategy_id >= MAX_NUM_STRATEGIES) {
    session_error("Invalid strategy ID in strategy_analyze.");
    return -1;
  }
  dbg("analyze: strategy id %d, sub point id %d\n", strategy_id, sub_point_id);
  
  // find the relevant subtrial
  // for now I'm being dumb about searching the window, because it's small
  int window_entry;
  for (window_entry = 0; window_entry < window_size; window_entry++) {
    if (window[window_entry].point_id == trial->point.id)
      break;
  }
  if (window_entry == window_size) {
    dbg("didn't find subtrial, returning\n");
    // i'm not responsible for this
    return 0;
  }

  // indicate that this point is complete
  window[window_entry].complete = 1;

  // track global optimum
  // TODO use hperf_cmp to make this suitable for multi-objective search
  if (hperf_unify(trial->perf) < best_perf) {
    if (hpoint_copy(&best, &trial->point) < 0) {
      session_error("failed to copy new global optimum");
      return -1;
    }
    dbg("new global best\n");
    best_perf = hperf_unify(trial->perf);
    window[window_entry].new_best = 1;
  }

  // decrement count of outstanding generates
  requests_outstanding--;

  // now pass the result to the sub-strategies

  // We call strategy_analyze from the strategy that generated the point,
  // and strategy_hint for all other strategies. Their interfaces are
  // identical, so a new strategy can just define strategy_hint to call
  // strategy_analyze if it doesn't need to match with a point it's found.

  // call "analyze" for originating strategy
  // have to bend over backwards to respect "const" qualifier on trial->point
  // while still giving the substrategy the id it expects
  htrial_t trial_clone = {HPOINT_INITIALIZER, NULL};
  hpoint_t point_clone = HPOINT_INITIALIZER;
  if (hpoint_copy(&point_clone, &trial->point) < 0) {
    session_error("Failed to copy point for substrategy");
    return -1;
  }
  point_clone.id = sub_point_id;
  if (hpoint_copy((hpoint_t *)&trial_clone.point, &point_clone) < 0) {
    session_error("Failed second point copy for substrategy");
    return -1;
  }
  trial_clone.perf = trial->perf;  // pointer copy; any reason to clone?

  converged_before = test_convergence();
  if (converged_before) {
    if (clear_convergence() < 0)
      return -1;
  }
  dbg("calling strategy %d analyze\n", strategy_id);
  int result = strategies[strategy_id].analyze(&trial_clone);
  if (result < 0)
    return -1;
  converged_after = test_convergence();
  if (converged_after) {
    converged[strategy_id] = 1;
    // we will keep the convergence flag if (1) it was set before, or (2) all
    // the other strategies have also converged
    int keep_convergence = converged_before;
    if (!keep_convergence) {
      // clear the convergence flag, unless everyone else is converged too
      int i;
      int all_converged = 1;
      for (i = 0; i < strat_count; i++) {
        all_converged = all_converged && converged[i];
        if (!all_converged)
          break;
      }
      keep_convergence = all_converged;
    }
    if (!keep_convergence)
      if (clear_convergence() < 0)
          return -1;
  }

  // call "hint" for all others
  int i;
  for (i = 0; i < strat_count; i++) {
    if (i != strategy_id && strategies[i].hint) {
      dbg("hinting strategy %d\n", i);
      if (strategies[i].hint(trial) < 0)
	return -1;
    }
  }

  return 0;
}

int strategy_best(hpoint_t *point)
{
  if (hpoint_copy(point, &best) != 0) {
    session_error("Could not copy best point during request for best.");
    return -1;
  }
  return 0;
}

// We'll identify points using 7 bits for the strategy index and 24 for the
// ID the strategy is using internally. If the strategy uses an ID that
// doesn't fit in 24 bits, we'll throw an error and I have to use a freaking
// lookup table or something.
int translate_id_from_sub(int strategy_id, int point_id) {
  int new_id = point_id & POINT_ID_MASK;
  if (new_id != point_id) {
    session_error("substrategy point ID didn't fit in 24 bits");
    return -1;
  }
  if (strategy_id < 0 || strategy_id >= MAX_NUM_STRATEGIES) {
    session_error("internal error: strategy_id outside valid range");
    return -1;
  }
  new_id |= (strategy_id << POINT_ID_BITS);
  return new_id;
}

int translate_id_to_sub(int point_id, int *strategy_id) {
  int sub_id = point_id & POINT_ID_MASK;
  if (strategy_id) {
    *strategy_id = point_id >> POINT_ID_BITS;
  }
  return sub_id;
}

void free_strat(strategy_t *strat) {
  free(strat->name);
  if (strat->lib)
    dlclose(strat->lib);
}