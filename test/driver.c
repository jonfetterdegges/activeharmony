#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

#include <hclient.h>

/*
 * Provides a measure of wall-clock time in seconds. The integer part of the
 * measurement starts from zero on the first call, to avoid floating-point
 * roundoff error.
 */
float currTime() {
  static time_t firstTime = 0;
  static struct timeval tv;
  if (gettimeofday(&tv, NULL) < 0) {
    perror("Error calling gettimeofday()");
    exit(1);
  }
  if (firstTime == 0)
    firstTime = (float) tv.tv_sec;
  float result = tv.tv_sec - firstTime;
  result += ((float) tv.tv_usec) / 1.0e6;
  return result;
}

/*
 * Run a child process and return the wall-clock time it took.
 * Arguments:
 *  path: the full path to the child binary
 *  argv: the child's full argv vector; the zero element should be the name
 *        of the child executable, and the last element must be NULL
 *
 * XXX is there a more accurate way to get wall time? in principle we'd like
 *     to time until the child terminates, not until the parent wakes up.
 *     This is possible with CPU time using getrusage().
 *
 * XXX is it too annoying to provide the full path?
 */
#define BUFLEN 32
int runChild(char *path, char **argv, float *time, int *precision, int do_read) {
  int pipefd[2];
  if (pipe(pipefd) < 0) {
    perror("Error opening pipe");
    return -1;
  }
  /* Only OSX
  if (fcntl(pipefd[1], F_SETNOSIGPIPE, 1) < 0) {
    perror("disabling SIGPIPE for child");
    return -1;
  }*/

  float starttime = currTime();
  pid_t childPid = fork();

  if (childPid) {
    // first error check
    if (childPid < 0) {
      perror("Error forking child");
      return -1;
    }

    // fork succeeded and we're in the parent process

    // read precision from child stdout if do_read is true
    int read_ret = 0;
    char buf[BUFLEN];
    if (do_read) {
      read_ret = read(pipefd[0], buf, BUFLEN-1);
      if (read_ret < 0) {
        perror("Error reading precision from child");
        return -1;
      }
    }

    // now wait for the child to terminate
    waitpid(childPid, NULL, 0);

    // timing
    float endtime = currTime();
    *time = endtime - starttime;

    // interpret precision we read earlier
    if (read_ret == 0)
      // this covers the case where do_read is false
      *precision = 0;
    else {
      // assume the child wrote precision on the pipe
      // no error checking since I'm running this with lots of different children
      buf[read_ret] = '\0';
      sscanf(buf, "%d", precision);
    }

    close(pipefd[0]);
    close(pipefd[1]);

    return 0;

  } else {
    // fork succeeded, this is the child process
    int ret;

    if (do_read) {
      // redirect stdout to the write end of the pipe
      ret = dup2(pipefd[1], 1);
      if (ret < 0) {
        perror("Error redirecting stdout");
        return -1;
      }
    }

    // exec the child
    ret = execv(path, argv);
    if (ret < 0) {
      perror("Error starting child");
    }
    return -1;  // this implies we had ret < 0
  }
}

#define SOURCE "linpackc.c"
#define TARGET "linpackc"

float compile(char *compiler, char *optflag, char *rollflag) {
  char *args[9];
  args[0] = compiler;
  args[1] = "-o";
  args[2] = TARGET;
  args[3] = SOURCE;
  args[4] = "-DDP";
  args[5] = "-lm";
  args[6] = rollflag;
  // pass in the optimization flag unless it's empty
  if (*optflag) {
    args[7] = optflag;
    args[8] = NULL;
  } else {
    args[7] = NULL;
  }

  int precision;
  float time;
  int ret = runChild(compiler, args, &time, &precision, 0);
  if (ret < 0)
    return -1.0;
  return time;
}

float run() {
  char *args[2];
  args[0] = TARGET;
  args[1] = NULL;

  int precision;
  float time;
  int ret;

  ret = runChild(TARGET, args, &time, &precision, 0);
  if (ret < 0)
    exit(2);
  return time;
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

#define COMPILER_OPT "compiler"
#define COMPILER1 "/usr/bin/gcc"
#define COMPILER2 "/usr/bin/clang"

#define OPTIM_OPT "optimization"
//#define OPT1 "-O1"
//#define OPT2 "-O2"
//#define OPT3 "-O3"
//#define OPT4 "-g"

#define ROLL_OPT "loopRoll"
#define ROLL1 "-DROLL"
#define ROLL2 "-DUNROLL"

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
    snprintf(name, sizeof(name), "gcc_ex.%d", getpid());

  // add compiler parameters
  if (ah_enum(desc, COMPILER_OPT, COMPILER1) ||
      ah_enum(desc, COMPILER_OPT, COMPILER2))
    ah_die("ah_enum returned nonzero for compiler");

  // add optimization flag parameters
  if (ah_int(desc, OPTIM_OPT, 1, 3, 1))
    ah_die("ah_int returned nonzero for optimization flag");

  // add loop unroll parameters
  if (ah_enum(desc, ROLL_OPT, ROLL1) ||
      ah_enum(desc, ROLL_OPT, ROLL2))
    ah_die("ah_enum returned nonzero for loop unroll flags");

  // define other options here

  // set strategy; would also connect to other plugins here
  // note when reading AH examples: harmony_strategy and harmony_layer_list
  // are just front ends to harmony_setcfg with particular config keys;
  // the examples in the AH repo call harmony_setcfg instead
  if (ah_strategy(desc, "bandit.so"))
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
  oldval = ah_set_cfg(desc, "LOG_FILE", "gcc-ex.log");
  if (oldval == NULL)
    ah_die_detail("ah_setcfg LOG_FILE failed", desc);
  oldval = ah_set_cfg(desc, "BANDIT_STRATEGIES", "exhaustive.so:random.so:nm.so");
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

  // associate local variables with AH parameters set up above
  // the const char ** casts silence compiler warnings, not sure whether
  // I should worry about them
  char *compiler, *rollflag;
  long optlevel;
  char optflag[10];
  if (ah_bind_enum(desc, COMPILER_OPT, (const char **) &compiler)
      || ah_bind_int(desc, OPTIM_OPT, &optlevel)
      || ah_bind_enum(desc, ROLL_OPT, (const char **) &rollflag))
    ah_die("ah_bind_enum failed");

  // join the tuning session
  if (ah_join(desc, NULL, 0, name))
    ah_die_detail("ah_join failed", desc);

  // dummy run because the first time always seems fastest
  // compile(COMPILER1, "-O1", ROLL1);
  // run();

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
    snprintf(optflag, sizeof(optflag), "-O%ld", optlevel);
    float ctime = compile(compiler, optflag, rollflag);
    float rtime = run();
    double result = (10. * (double) ctime) + (double) rtime;
    if (hresult > 0)
      fprintf(stderr, "\n\n%s %s %s -> %.3f + %.3f -> %.3f\n\n",
	      compiler, optflag, rollflag, ctime, rtime, result);
    if (ah_report(desc, &result))
      ah_die("ah_report failed");
  }

  fprintf(stderr, "convergence %s in %d iteration%s\n",
	  ah_converged(desc) ? "success" : "failure",
	  iters, iters == 1 ? "" : "s");

  if (ah_best(desc) >= 0)
    fprintf(stderr, "best result: %s -O%ld %s\n", compiler, optlevel, rollflag);
  else
    fprintf(stderr, "no best point: %s\n", ah_error_string(desc));

  // Leave the tuning session
  if (ah_leave(desc))
    ah_die("you can ah_checkout any time you want "
	   "but you can never ah_leave");

  // Terminate Harmony connection.
  ah_fini(desc);

  return 0;
}
