#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <math.h>

#include <hclient.h>

/*
 * Provides a measure of wall-clock time in seconds. The integer part of the
 * measurement starts from zero on the first call, to avoid floating-point
 * roundoff error.
 */
double currTime() {
  static time_t firstTime = 0;
  static struct timeval tv;
  if (gettimeofday(&tv, NULL) < 0) {
    perror("Error calling gettimeofday()");
    exit(1);
  }
  if (firstTime == 0)
    firstTime = (double) tv.tv_sec;
  double result = tv.tv_sec - firstTime;
  result += ((double) tv.tv_usec) / 1.0e6;
  return result;
}


int runChild(char *cmd, double *time, double *speedup) {
  double starttime = currTime();
  FILE *pcmd = popen(cmd, "r");
  if (!pcmd)
    exit(1);
  char buf[512];
  size_t much = fread(buf, 1, sizeof(buf) - 1, pcmd);
  buf[much] = 0;
  printf("%s\n", buf);
  if (ferror(pcmd)) {
    printf("Error while running test:\n%s\n", cmd);
  }
  int ret = WEXITSTATUS(pclose(pcmd));

  double endtime = currTime();
  *time = endtime - starttime;
  *speedup = atof(buf);

  return ret;
}

/*

minor_heap_sizes: 0 .. 163840 (step 32768 for 5)
major_heap_increment: 15 .. 105 (step 10 for 10)
space_overhead: 50 .. 100 (step 10 for 6)
max_overhead: 500 .. 5000 (step 500 for 10)

 */

#define SHBIN "/usr/bin/sh"
#define INTERP "/data/code/plum/ic-aam/cli"
#define TEST "/data/code/plum/ic-aam/test/mutations/array-max.sch"
#define MUTS "/data/code/plum/ic-aam/test/mutations/array-max-mut-ans-and-ind.mut"
#define ENV "/data/code/plum/ic-aam/env"

double interp(char* name,
              long mhs,      // 0 .. 163840 (step 32768 for 5)
              long mo,       // 500 .. 5000 (step 500 for 10)
              long mhi,      // 15 .. 105 (step 10 for 10)
              long so,       // 50 .. 100 (step 10 for 6)
              long k,        // -1 .. 4 (step 1 for 6)
              long md,       // 1 .. 6 (step 1 for 6)
              long noa,      // 0/1
              long del,      // 0/1
              long clo,      // 0/1
              long mem,      // 0/1
              double aif,    // 1.0 .. 3.0 (step 0.2 for 11)
              char *artlib,  // fromscratch, nominal, structural
              char *heap,    // trie-map, trie-rel, map, rel
              char *namer,   // dyn, con, gensym, abs
              char *alloc,   // ktime, gensym, state
              char *tick     // dyn, lex
  ) {

  char cmd[1024];
  sprintf(cmd,
          "sh -c "
          "\""
          "ulimit -t 10 -v `free | grep Mem | awk -F' ' '{ print $7 }'` ; "
          INTERP
          " -interp abs "
          " -env "
          ENV
          " -test "
          TEST
          " -mutations "
          MUTS
          " -test-name %s "
          " -gc-minor-heap-size %ld "
          " -gc-max-overhead %ld "
          " -gc-major-heap-increment %ld "
          " -gc-space-overhead %ld "
          " -k %ld "
          " -min-depth %ld "
          " -art-ifreq %f "
          " -heap %s "
          " -namer %s "
          " -alloc %s " 
          " -artlib %s "
          " -tick %s "
          " -quiet -activeharmony "
          "\"",
          name,
          mhs,
          mo,
          mhi,
          so,
          k,
          md,
          aif,
          heap,
          namer,
          alloc,
          artlib,
          tick);

  printf("%s\n", cmd);

  double speedup = -1; // really, inverse speedup because we're minimizing
  double time = -1;
  int rett = runChild(cmd, &time, &speedup);
  double resultt = speedup == 0. ? time : speedup * time;

  printf("Got speedup: %f and time %f, so result %f\n", speedup, time, resultt);
  if (rett == 0 || time > 0) {
    return resultt;
  } else {
    return -1.0;
  }
}

void ah_die(char *message) {
  fprintf(stderr, "AH error: %s\n", message);
  exit(2);
}

void ah_die_detail(char *message, hdesc_t *desc) {
  fprintf(stderr, "AH error: %s\n", message);
  fprintf(stderr, "Error message: %s\n", ah_error_string(desc));
  exit(2);
}

// Art inverse frequencey (11)
#define AIF_ARG "art-ifreq"
#define AIF_MIN 1.0
#define AIF_MAX 3.0
#define AIF_STEP 0.2

// Min trie depth (6)
#define MD_ARG "min-depth"
#define MD_MIN 1
#define MD_MAX 6
#define MD_STEP 1

// Minor heap size (8)
#define MHS_ARG "minor-heap-size"
#define MHS_MIN 0
#define MHS_MAX 163840
#define MHS_STEP 32768

// Max overhead (10)
#define MO_ARG "max-overhead"
#define MO_MIN 500
#define MO_MAX 5000
#define MO_STEP 500

// Major heap increment (10)
#define MHI_ARG "major-heap-increment"
#define MHI_MIN 15
#define MHI_MAX 105
#define MHI_STEP 10

// Space overhead (6)
#define SO_ARG "space-overhead"
#define SO_MIN 50
#define SO_MAX 100
#define SO_STEP 10

// k (context) (6)
#define K_ARG "k"
#define K_MIN -1
#define K_MAX 4
#define K_STEP 1

// tick (2)
#define TICK_ARG "tick"
#define TICK1 "dyn"
#define TICK2 "lex"

// artlib (2)
#define ARTLIB_ARG "artlib"
#define ARTLIB1 "structural"
#define ARTLIB2 "nominal"
#define ARTLIB3 "fromscratch"

// alloc (3)
#define ALLOC_ARG "alloc"
#define ALLOC1 "ktime"
#define ALLOC2 "state"
#define ALLOC3 "gensym"

// namer (4)
#define NAMER_ARG "namer"
#define NAMER1 "dyn"
#define NAMER2 "con"
#define NAMER3 "abs"
#define NAMER4 "gensym"

// heap (2)
#define HEAP_ARG "heap"
#define HEAP1 "trie-rel"
#define HEAP2 "rel"

// bool flags
#define NA_ARG "no-arts"
#define DEL_ARG "delay"
#define CLOSFV_ARG "clos-fv"
#define MEMOFV_ARG "memo-fv"

#define STRAT1 "bandit.so"
#define STRAT2 "beam.so"
#define STRAT3 "breadth_first.so"
#define STRAT4 "sim_anneal.so"
#define STRAT5 "nm.so"
#define STRAT6 "angel.so"

#define MAX_LOOP 1000

int main(int argc, char **argv) {
  // Connect to Harmony.
  hdesc_t *desc = ah_init();
  if (!desc)
    ah_die("ah_init returned NULL");

  if (ah_args(desc, &argc, argv) < 0)
    ah_die("ah_args returned negative");

  char name[1024];
  if (argc > 1)
    strncpy(name, argv[1], sizeof(name));
  else
    snprintf(name, sizeof(name), "adi.%d", getpid());

  // int flags

  if (ah_int(desc, MHS_ARG, MHS_MIN, MHS_MAX, MHS_STEP))
    ah_die("ah_int returned nonzero for MHS flag") ;

  if (ah_int(desc, MO_ARG, MO_MIN, MO_MAX, MO_STEP))
    ah_die("ah_int returned nonzero for MO flag") ;

  if (ah_int(desc, MHI_ARG, MHI_MIN, MHI_MAX, MHI_STEP))
    ah_die("ah_int returned nonzero for MHI flag") ;

  if (ah_int(desc, SO_ARG, SO_MIN, SO_MAX, SO_STEP))
    ah_die("ah_int returned nonzero for SO flag") ;

  if (ah_int(desc, MD_ARG, MD_MIN, MD_MAX, MD_STEP))
    ah_die("ah_int returned nonzero for MD flag") ;

  if (ah_int(desc, K_ARG, K_MIN, K_MAX, K_STEP))
    ah_die("ah_int returned nonzero for K flag") ;

  // real flags

  if (ah_real(desc, AIF_ARG, AIF_MIN, AIF_MAX, AIF_STEP))
    ah_die("ah_int returned nonzero for AIF flag") ;

  // enum flags

  if (ah_enum(desc, TICK_ARG, TICK1) ||
      ah_enum(desc, TICK_ARG, TICK2))
    ah_die("ah_enum returned nonzero for TICK") ;

  if (ah_enum(desc, ARTLIB_ARG, ARTLIB1) ||
      ah_enum(desc, ARTLIB_ARG, ARTLIB2) ||
      ah_enum(desc, ARTLIB_ARG, ARTLIB3))
    ah_die("ah_enum returned nonzero for ARTLIB") ;

  if (ah_enum(desc, ALLOC_ARG, ALLOC1) ||
      ah_enum(desc, ALLOC_ARG, ALLOC2) ||
      ah_enum(desc, ALLOC_ARG, ALLOC3))
    ah_die("ah_enum returned nonzero for ALLOC") ;

  if (ah_enum(desc, NAMER_ARG, NAMER1) ||
      ah_enum(desc, NAMER_ARG, NAMER2) ||
      ah_enum(desc, NAMER_ARG, NAMER3) ||
      ah_enum(desc, NAMER_ARG, NAMER4))
    ah_die("ah_enum returned nonzero for NAMER") ;

  if (ah_enum(desc, HEAP_ARG, HEAP1) ||
      ah_enum(desc, HEAP_ARG, HEAP2))
    ah_die("ah_enum returned nonzero for HEAP") ;

  // bool flags

  if (ah_int(desc, NA_ARG, 0, 1, 1))
    ah_die("ah_int returned nonzero for arts bool") ;

  if (ah_int(desc, DEL_ARG, 0, 1, 1))
    ah_die("ah_int returned nonzero for delay bool") ;

  if (ah_int(desc, CLOSFV_ARG, 0, 1, 1))
    ah_die("ah_int returned nonzero for closfv bool") ;

  if (ah_int(desc, MEMOFV_ARG, 0, 1, 1))
    ah_die("ah_int returned nonzero for memofv bool") ;


  // AH config

  if (ah_strategy(desc, STRAT1))    // 2 4 5
    ah_die("ah_strategy failed");

  if (ah_layers(desc, "log.so"))
    ah_die("ah_layers failed");

  char *oldval;
  oldval = ah_set_cfg(desc, "AGG_FUNC", "min");
  if (oldval == NULL)
    ah_die_detail("ah_setcfg AGG_FUNC failed", desc);
  oldval = ah_set_cfg(desc, "AGG_TIMES", "3");
  if (oldval == NULL)
    ah_die_detail("ah_setcfg AGG_TIMES failed", desc);
  oldval = ah_set_cfg(desc, "LOG_FILE", "adi.log");
  if (oldval == NULL)
    ah_die_detail("ah_setcfg LOG_FILE failed", desc);
  oldval = ah_set_cfg(desc, "BANDIT_STRATEGIES", "sim_anneal.so:beam.so:nm.so");
  if (oldval == NULL)
    ah_die_detail("ah_setcfg BANDIT_STRATEGIES failed", desc);
  if (ah_set_cfg(desc, "SA_TMAX", "3") == NULL)
    ah_die_detail("ah_set_cfg SA_TMAX failed", desc);
  if (ah_set_cfg(desc, "SA_TMIN", "0.02") == NULL)
    ah_die_detail("ah_set_cfg SA_TMIN failed", desc);
  if (ah_set_cfg(desc, "SA_NPOINTS", "2") == NULL)
    ah_die_detail("ah_set_cfg SA_NPOINTS failed", desc);
  if (ah_set_cfg(desc, "SA_TSTEPS", "100") == NULL)
    ah_die_detail("ah_set_cfg SA_TSTEPS failed", desc);
  if (ah_set_cfg(desc, "SA_RESET_STEPS", "10") == NULL)
    ah_die_detail("ah_set_cfg SA_RESET_STEPS failed", desc);


  // start a Harmony session
  // the second and third args are host and port; when NULL/0, they tell the
  // client to use env vars HARMONY_S_HOST and HARMONY_S_PORT (or to start a
  // local session in HARMONY_HOME if those are undefined)
  if (ah_launch(desc, NULL, 0, name))
    ah_die_detail("ah_launch failed", desc);

  double arg_art_ifreq;
  long arg_min_depth, arg_minor_heap_size, arg_max_overhead,
    arg_major_heap_increment, arg_space_overhead, arg_k,
    arg_no_arts, arg_delay, arg_closfv, arg_memofv;
  char *arg_tick, *arg_alloc, *arg_namer, *arg_heap, *arg_artlib;

  if (ah_bind_real(desc, AIF_ARG, &arg_art_ifreq)
  ||  ah_bind_int(desc, MD_ARG, &arg_min_depth)
  ||  ah_bind_int(desc, MHS_ARG, &arg_minor_heap_size)
  ||  ah_bind_int(desc, MO_ARG, &arg_max_overhead)
  ||  ah_bind_int(desc, MHI_ARG, &arg_major_heap_increment)
  ||  ah_bind_int(desc, SO_ARG, &arg_k)
  ||  ah_bind_int(desc, SO_ARG, &arg_space_overhead)
  ||  ah_bind_int(desc, NA_ARG, &arg_no_arts)
  ||  ah_bind_int(desc, DEL_ARG, &arg_delay)
  ||  ah_bind_int(desc, CLOSFV_ARG, &arg_closfv)
  ||  ah_bind_int(desc, MEMOFV_ARG, &arg_memofv)
  || ah_bind_enum(desc, ARTLIB_ARG, (const char **) &arg_artlib)
  || ah_bind_enum(desc, TICK_ARG, (const char **) &arg_tick)
  || ah_bind_enum(desc, ALLOC_ARG, (const char **) &arg_alloc)
  || ah_bind_enum(desc, NAMER_ARG, (const char **) &arg_namer)
  || ah_bind_enum(desc, HEAP_ARG, (const char **) &arg_heap))
    ah_die("ah_bind_* failed");

  // join the tuning session
  if (ah_join(desc, NULL, 0, name))
    ah_die_detail("ah_join failed", desc);

  int iters = 0;
  while (iters < MAX_LOOP && !ah_converged(desc)) {
    iters++;
    int hresult = ah_fetch(desc);
    if (hresult < 0)
      // indicates error
      ah_die_detail("ah_fetch failed", desc);
    // may also test hresult > 0; if that is true then AH updated a parameter,
    // and we can do any systemic change required

    // run with current parameters
    double result =
      interp(name,
             arg_minor_heap_size,
             arg_max_overhead,
             arg_major_heap_increment,
             arg_space_overhead,
             arg_k,
             arg_min_depth,
             arg_no_arts,
             arg_delay,
             arg_closfv,
             arg_memofv,
             arg_art_ifreq,
             arg_artlib,
             arg_heap,
             arg_namer,
             arg_alloc,
             arg_tick);


    if (ah_report(desc, &result))
      ah_die("ah_report failed");
  }

  fprintf(stderr, "convergence %s in %d iteration%s\n",
	  ah_converged(desc) ? "success" : "failure",
	  iters, iters == 1 ? "" : "s");

  /*
  if (ah_best(desc) >= 0)
    fprintf(stderr, "best result: %s -O%ld %s\n", compiler, optlevel, rollflag);
  else
    fprintf(stderr, "no best point: %s\n", ah_error_string(desc));
  */

  // Leave the tuning session
  if (ah_leave(desc))
    ah_die("you can ah_checkout any time you want "
	   "but you can never ah_leave");

  // Terminate Harmony connection.
  ah_fini(desc);

  return 0;
}
