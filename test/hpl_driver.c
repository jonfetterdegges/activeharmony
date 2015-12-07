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
int runChild(char *path, char **argv) {
  pid_t childPid = fork();

  if (childPid) {
    // first error check
    if (childPid < 0) {
      perror("Error forking child");
      return -1;
    }

    // fork succeeded and we're in the parent process
    // wait for the child to terminate
    waitpid(childPid, NULL, 0);

    return 0;

  } else {
    // fork succeeded, this is the child process
    int ret;

    // exec the child
    ret = execv(path, argv);
    if (ret < 0) {
      perror("Error starting child");
    }
    return -1;  // this implies we had ret < 0
  }
}

char hpl_input[] = \
  "HPLinpack benchmark input file\n" \
  "Innovative Computing Laboratory, University of Tennessee\n" \
  "x                output file name (if any)\n" \
  "0                device out (6=stdout,7=stderr,file)\n" \
  "1                # of problems sizes (N)\n" \
  "4096             # Ns\n" \
  "1                # of NBs\n" \
  "16               NBs\n" \
  "0                PMAP process mapping (0=Row-,1=Column-major)\n" \
  "1                # of process grids (P x Q)\n" \
  "2                Ps\n" \
  "2                Qs\n" \
  "16.0             threshold\n" \
  "1                # of panel fact\n" \
  "0                PFACTs (0=left, 1=Crout, 2=Right)\n" \
  "1                # of recursive stopping criterium\n" \
  "2                NBMINs (>= 1)\n" \
  "1                # of panels in recursion\n" \
  "2                NDIVs\n" \
  "1                # of recursive panel fact.\n" \
  "0                RFACTs (0=left, 1=Crout, 2=Right)\n" \
  "1                # of broadcast\n" \
  "0                BCASTs (0=1rg,1=1rM,2=2rg,3=2rM,4=Lng,5=LnM)\n" \
  "1                # of lookahead depth\n" \
  "2                DEPTHs (>=0)\n" \
  "2                SWAP (0=bin-exch,1=long,2=mix)\n" \
  "64               swapping threshold\n" \
  "0                L1 in (0=transposed,1=no-transposed) form\n" \
  "0                U  in (0=transposed,1=no-transposed) form\n" \
  "1                Equilibration (0=no,1=yes)\n" \
  "8                memory alignment in double (> 0)\n";

void set_input(char *buf, int line, char *val) {
  //  fprintf(stderr, "Setting line %d to %s\n", line, val);
  //  fprintf(stderr, "Initial input:\n%s\n\n", buf);
  // scan to the appropriate line
  // first line is line 1
  int curr_line = 1;
  char *pos = buf;
  while (curr_line < line) {
    while (*pos != '\0' && *pos != '\n')
      pos++;
    if (*pos == '\0') {
      fprintf(stderr, "Ran past end of input in set_input\n");
      exit(3);
    }
    pos++;
    curr_line++;
  }
  char *endpos = stpcpy(pos, val);
  *endpos = ' ';

  //  fprintf(stderr, "Resulting input:\n%s\n\n", buf);
}

#define VAL_BUFLEN 16
void set_input_int(char *buf, int line, int val) {
  // get a printable representation of the value
  char cval[VAL_BUFLEN];
  snprintf(cval, VAL_BUFLEN, "%d", val);
  set_input(buf, line, cval);
}

enum grid {
  GRID_4BY1,
  GRID_2BY2,
  GRID_1BY4
};

enum pfact {
  PFACT_LEFT,
  PFACT_CROUT,
  PFACT_RIGHT
};

#define HPL_OUTBUF_SIZE 4096

float run_hpl(int id, int size, int nb, enum grid grid, enum pfact pfact,
              enum pfact rfact, int nbmin, int ndiv, int depth, int xpose) {
  char *input;
  int input_len;
  input_len = strlen(hpl_input);
  input = (char *) malloc(input_len + 1);
  strcpy(input, hpl_input);

  // line 3: output file name (not used right now)
  char outname[VAL_BUFLEN];
  snprintf(outname, VAL_BUFLEN, "r%d", id);
  set_input(input, 3, outname);

  // line 6: problem size
  set_input_int(input, 6, size);

  // line 8: block size
  set_input_int(input, 8, nb);

  // lines 11 and 12: grid size
  int prows, pcols;
  switch(grid) {
  case GRID_4BY1:
    prows = 4;
    pcols = 1;
    break;
  case GRID_1BY4:
    prows = 1;
    pcols = 4;
    break;
  default:
  case GRID_2BY2:
    prows = 2;
    pcols = 2;
    break;
  }
  set_input_int(input, 11, prows);
  set_input_int(input, 12, pcols);

  // line 15: pfact
  set_input_int(input, 15, (int) pfact);

  // line 21: rfact
  set_input_int(input, 21, (int) rfact);

  // line 17: nbmin
  set_input_int(input, 17, nbmin);

  // line 19: ndiv
  set_input_int(input, 19, ndiv);

  // line 25: lookahead depth
  set_input_int(input, 25, depth);

  // lines 28 and 29: transpose L1 and U
  set_input_int(input, 28, xpose);
  set_input_int(input, 28, xpose);

  // okay, inputs are set; write HPL.dat
  // we work in the current working directory
  FILE *hpldat = fopen("HPL.dat", "w");
  fwrite(input, sizeof(char), input_len, hpldat);
  fclose(hpldat);

  // fprintf(stderr, "%s\n\n", input);

  free(input);

  // now run HPL
  char *argv[] = {"mpirun", "-np", "4", "xhpl", NULL};
  if (runChild("/opt/local/bin/mpirun", argv) < 0)
    return -1.0;

  // HPL has run, read its output file
  FILE *hplout = fopen(outname, "r");
  // scan until we see the top of the result table
  char inbuf[128];
  char tbl_hdr[] = "T/V                N    NB     P     Q";  //copied from hpl source
  int tbl_hdr_len = strlen(tbl_hdr);
  do {
    if (fgets(inbuf, 128, hplout) == NULL)
      return -1.0;
  } while (strncmp(inbuf, tbl_hdr, tbl_hdr_len) != 0);
  fgets(inbuf, 128, hplout);  // skip separator line
  fgets(inbuf, 128, hplout);  // this is what we want
  fclose(hplout);

  // read the result line
  int testsize, testnb, testrows, testcols;
  float gflops;
  int nread;
  nread = sscanf(inbuf, "%*s %d %d %d %d %*g %g",
                 &testsize, &testnb, &testrows, &testcols, &gflops);
  if (nread != 5
      || testsize != size
      || testnb != nb
      || testrows != prows
      || testcols != pcols) {
    fprintf(stderr, "Input mismatch from file %s:\n  %s\n", outname, inbuf);
    return -1.0;
  }

  return 1000.0/gflops;  // time to run 10^12 FP operations

  return -1.0;
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


#define MAX_LOOP 4000

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
    snprintf(name, sizeof(name), "hpl.%d", getpid());

  // tuning parameters:
  // problem size: 2048, 4096, 6144
  // nb: 16, 32, 64, 128, 256, 512, 1024
  // grid: 0, 1, 2 for 4x1, 2x2, 1x4
  // pfact: 0, 1, 2 for left, Crout, right
  // rfact: 0, 1, 2 for left, Crout, right
  // nbmin: 1, 2, 4, 8
  // ndiv: 2, 3, 4
  // depth: 0, 1
  // xpose: 0, 1
  // this gives 3*5*3*3*3*4*3*2*2 = 81*240 = 19,440 combinations

  if (ah_int(desc, "SIZE", 2048, 2048*3, 2048) < 0
      || ah_int(desc, "NB_EXP", 4, 10, 1) < 0
      || ah_int(desc, "GRID", 0, 2, 1) < 0
      || ah_int(desc, "PFACT", 0, 2, 1) < 0
      || ah_int(desc, "RFACT", 0, 2, 1) < 0
      || ah_int(desc, "NBMIN_EXP", 0, 3, 1) < 0
      || ah_int(desc, "NDIV", 2, 4, 1) < 0
      || ah_int(desc, "DEPTH", 0, 1, 1) < 0
      || ah_int(desc, "XPOSE", 0, 1, 1) < 0)
    ah_die("ah_int failed");

  // set strategy; would also connect to other plugins here
  // note when reading AH examples: harmony_strategy and harmony_layer_list
  // are just front ends to harmony_setcfg with particular config keys;
  // the examples in the AH repo call harmony_setcfg instead
  if (ah_strategy(desc, "bandit.so"))
    ah_die("ah_strategy failed");
  if (ah_layers(desc, "agg.so:log.so"))
    ah_die("ah_layers failed");

  char *oldval;
  oldval = ah_set_cfg(desc, "AGG_FUNC", "min");
  if (oldval == NULL)
    ah_die_detail("ah_setcfg AGG_FUNC failed", desc);
  oldval = ah_set_cfg(desc, "AGG_TIMES", "3");
  if (oldval == NULL)
    ah_die_detail("ah_setcfg AGG_TIMES failed", desc);
  oldval = ah_set_cfg(desc, "LOG_FILE", "hpl-driver.log");
  if (oldval == NULL)
    ah_die_detail("ah_setcfg LOG_FILE failed", desc);
  oldval = ah_set_cfg(desc, "BANDIT_STRATEGIES", "random.so:sim_anneal.so:beam.so");
  if (oldval == NULL)
    ah_die_detail("ah_setcfg BANDIT_STRATEGIES failed", desc);
  if (ah_set_cfg(desc, "SA_TMAX", "8") == NULL)
    ah_die_detail("ah_set_cfg SA_TMAX failed", desc);
  if (ah_set_cfg(desc, "SA_TMIN", "0.1") == NULL)
    ah_die_detail("ah_set_cfg SA_TMIN failed", desc);
  if (ah_set_cfg(desc, "SA_NPOINTS", "3") == NULL)
    ah_die_detail("ah_set_cfg SA_NPOINTS failed", desc);
  if (ah_set_cfg(desc, "SA_TSTEPS", "200") == NULL)
    ah_die_detail("ah_set_cfg SA_TSTEPS failed", desc);
  if (ah_set_cfg(desc, "SA_RESET_STEPS", "20") == NULL)
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
  long size;
  long nb_exp;
  long grid = GRID_2BY2;
  long pfact = PFACT_LEFT;
  long rfact = PFACT_LEFT;
  long nbmin_exp = 4;
  long ndiv = 3;
  long depth = 0;
  long xpose = 0;
  if (ah_bind_int(desc, "SIZE", &size)
      || ah_bind_int(desc, "NB_EXP", &nb_exp)
      || ah_bind_int(desc, "GRID", &grid)
      || ah_bind_int(desc, "PFACT", &pfact)
      || ah_bind_int(desc, "RFACT", &rfact)
      || ah_bind_int(desc, "NBMIN_EXP", &nbmin_exp)
      || ah_bind_int(desc, "NDIV", &ndiv)
      || ah_bind_int(desc, "DEPTH", &depth)
      || ah_bind_int(desc, "XPOSE", &xpose))
    ah_die_detail("ah_bind_* failed", desc);

  int nb;
  int nbmin;

  // join the tuning session
  if (ah_join(desc, NULL, 0, name))
    ah_die_detail("ah_join failed", desc);

  printf("Tuning start\n");
  printf("%3s %4s %3s %4s %2s %2s %5s %4s %2s %2s %8s\n",
         "id", "size", "nb", "grid", "pf", "rf", "nbmin", "ndiv", "dp", "xp", "result");
  char *grid_printable[] = {"4x1", "2x2", "1x4"};
  char *pfact_printable[] = {"l", "c", "r"};
  int iters = 0;
  while (iters < MAX_LOOP && !ah_converged(desc)) {
    iters++;
    int hresult = ah_fetch(desc);
    if (hresult < 0)
      // indicates error
      ah_die_detail("ah_fetch failed", desc);
    if (hresult == 0)
      fprintf(stderr, "no parameter change :(\n");
    // may also test hresult > 0; if that is true then AH updated a parameter,
    // and we can do any systemic change required

    // run with current parameters
    nb = 1 << nb_exp;
    nbmin = 1 << nbmin_exp;
    double result = run_hpl(iters, size, nb, grid, pfact,
                            rfact, nbmin, ndiv, depth, xpose);
    printf("%3d %4ld %3d %4s %2s %2s %5d %4ld %2ld %2ld %8.2f\n",
           iters, size, nb, grid_printable[grid], pfact_printable[pfact],
           pfact_printable[rfact], nbmin, ndiv, depth, xpose, result);
    if (result < 0.0) {
      fprintf(stderr, "***Bad result, quitting\n");
    }
    if (ah_report(desc, &result))
      ah_die("ah_report failed");
  }

  fprintf(stderr, "convergence %s in %d iteration%s\n",
	  ah_converged(desc) ? "success" : "failure",
	  iters, iters == 1 ? "" : "s");

  if (ah_best(desc) >= 0) {
    nb = 1 << nb_exp;
    nbmin = 1 << nbmin_exp;
    printf("\nbest result:\n");
    printf("%3s %4ld %3d %4s %2s %2s %5d %4ld %2ld %2ld\n",
           "", size, nb, grid_printable[grid], pfact_printable[pfact],
           pfact_printable[rfact], nbmin, ndiv, depth, xpose);
  } else {
    fprintf(stderr, "no best point: %s\n", ah_error_string(desc));
  }

  // Leave the tuning session
  if (ah_leave(desc))
    ah_die("you can ah_checkout any time you want "
	   "but you can never ah_leave");

  // Terminate Harmony connection.
  ah_fini(desc);

  return 0;
}
