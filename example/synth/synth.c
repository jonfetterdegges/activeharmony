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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <getopt.h>
#include <fcntl.h>
#include <time.h>
#include <math.h>

#include "testfunc.h"
#include "hclient.h"

#define MAX_ACCURACY 17
#define MAX_IN       16
#define MAX_OUT      16

/* Function Prototypes */
void   parse_opts(int argc, char *argv[]);
void   parse_params(int idx, int argc, char *argv[]);
void   parse_dim(char *dim);
void   parse_funcs(char *list);
int    parse_fopts(int idx, char **opts);
int    start_harmony(hdesc_t *hdesc);
void   eval_func(void);
double quantize_value(double perf);
double random_value(double min, double max);
void   fprint_darr(FILE *fp, const char *head,
                   double *arr, int len, const char *tail);
void   use_resources(void);

/* Option Variables (and their associated defaults). */
int accuracy = 6;
long seed;
int i_cnt, o_cnt;
unsigned int iterations;
int verbose;
int quantize_exp;
double quantize;
double perturb;
char tuna_mode;

/* Global Variables */
finfo_t *finfo[MAX_OUT];
double *fopts[MAX_OUT];
double bound_min, bound_max;
double point[MAX_IN];
double perf[MAX_OUT];
double best[MAX_OUT];
int single_eval;

void usage(const char *prog)
{
    fprintf(stdout,
"Usage: %s [OPTIONS] <N> <fname1>[,<fname2>,...] [x1..xN] [KEY=VAL ..]\n"
"\n"
"Test Active Harmony search strategies using multi-variate continuous\n"
"functions to simulate empirical test results.  Two parameters are\n"
"required: the name of at least one test function (fname1), and the\n"
"dimensionality of the search space (N).\n"
"\n"
"If N real values follow, they are considered to be a single point in\n"
"the search space to test.  The function is evaluated once, and the\n"
"program exits.  Otherwise, a tuning session is launched and the\n"
"function is tested repeatedly until the search session converges.\n"
"\n"
"Finally, any non-option parameter that contains an equals sign (=)\n"
"will be passed to the Active Harmony configuration system.\n"
"\n"
"OPTION SUMMARY\n"
"(Mandatory arguments to long options are also mandatory for short options.)\n"
"\n"
"  -a, --accuracy=(inf|INT) Allow INT decimal digits of accuracy to the\n"
"                            right of the decimal point for function input\n"
"                            variables.  If 'inf' is provided instead of an\n"
"                            integer, the full accuracy of an IEEE double\n"
"                            will be used. [Default: %d]\n"
"  -l, --list               List available test functions.  Full\n"
"                            descriptions will be printed when used with -v.\n"
"  -h, --help               Display this help message.\n"
"  -i, --iterations=INT     Perform at most INT search loop iterations.\n"
"  -o, --options=LIST        Pass a comma (,) separated list of real numbers\n"
"                            to the test function as optional parameters.\n"
"  -p, --perturb=REAL       Perturb the test function's output by a\n"
"                            uniformly random value in the range [0, REAL].\n"
"  -q, --quantize=INT       Quantize the underlying function by rounding\n"
"                            output values after INT decimal digits to the\n"
"                            right of the decimal point.\n"
"  -s, --seed=INT           Seed the random value generator with the\n"
"                            provided INT before perturbing output values.\n"
"                            Otherwise, the value will be taken from\n"
"                            time(NULL).\n"
"  -t, --tuna=TYPE          Run as a child of Tuna, where only a single\n"
"                            point is tested and the output is used to\n"
"                            consume a proportional amount of resources as\n"
"                            determined by TYPE:\n"
"                              w = Wall time.\n"
"                              u = User CPU time.\n"
"                              s = System CPU time.\n"
"                              o = Print value to stdout. (default)\n"
"  -v, --verbose            Verbose output.\n"
"\n", prog, accuracy);
}

int main(int argc, char *argv[])
{
    int i, j, hresult, report, retval = 0;
    int width, maxwidth;
    double best_val = INFINITY;
    hdesc_t *hdesc;

    hdesc = harmony_init(&argc, &argv);
    if (!hdesc) {
        perror("Error allocating/initializing a Harmony descriptor");
        return -1;
    }

    parse_opts(argc, argv);

    if (perturb)
        fprintf(stdout, "Seed value: %ld\n", seed);
    srand((int) seed);
    srand48(seed);

    if (single_eval) {
        eval_func();
        if (!tuna_mode)
            tuna_mode = 'o';

        use_resources();
        return 0;
    }

    if (start_harmony(hdesc) != 0) {
        retval = -1;
        goto cleanup;
    }

    report = 8;
    for (i = 0; !harmony_converged(hdesc); i++) {
        double cmp = 0.0;

        if (iterations && i >= iterations)
            break;

        hresult = harmony_fetch(hdesc);
        if (hresult < 0) {
            fprintf(stderr, "Error fetching values from session: %s\n",
                    harmony_error_string(hdesc));
            retval = -1;
            goto cleanup;
        }
        else if (hresult == 0) {
            continue;
        }

        eval_func();
        for (j = 0; j < o_cnt; ++j)
            cmp += perf[j];
        if (best_val > cmp) {
            best_val = cmp;
            memcpy(best, perf, sizeof(best));
        }

        if (harmony_report(hdesc, perf) < 0) {
            fprintf(stderr, "Error reporting performance to session: %s\n",
                    harmony_error_string(hdesc));
            retval = -1;
            goto cleanup;
        }

        if (i == report) {
            fprintf(stdout, "%6d evals, best value: ", i);
            fprint_darr(stdout, "(", best, o_cnt, ")\n");
            report <<= 1;
        }
    }

    hresult = harmony_best(hdesc);
    if (hresult < 0) {
        fprintf(stderr, "Error setting best input values: %s\n",
                harmony_error_string(hdesc));
        goto cleanup;
    }
    else if (hresult == 1) {
        double cmp = 0.0;

        /* A new point was retrieved.  Evaluate it. */
        eval_func();
        for (j = 0; j < o_cnt; ++j)
            cmp += perf[j];
        if (best_val > cmp) {
            best_val = cmp;
            memcpy(best, perf, sizeof(best));
        }
        ++i;
    }

    fprintf(stdout, "%6d eval%s, best value: ", i, i == 1 ? "" : "s");
    fprint_darr(stdout, "(", best, o_cnt, ")");

    if (o_cnt == 1 && finfo[0]->optimal != -INFINITY)
        printf(" [Global optimal: %lf]\n", finfo[0]->optimal);
    else
        printf(" [Global optimal: <Unknown>]\n");

    /* Initial pass through the point array to find maximum field width. */
    maxwidth = 0;
    for (i = 0; i < i_cnt; ++i) {
        width = snprintf(NULL, 0, "%lf", point[i]);
        if (maxwidth < width)
            maxwidth = width;
    }

    fprintf(stdout, "\nBest point found:\n");
    for (i = 0; i < i_cnt; ++i)
        fprintf(stdout, "x[%d] = %*lf\n", i, maxwidth, point[i]);

  cleanup:
    harmony_fini(hdesc);
    return retval;
}

void parse_opts(int argc, char *argv[])
{
    int c, list_funcs = 0;
    char *end;

    static struct option longopts[] = {
        {"accuracy",   required_argument, NULL, 'a'},
        {"help",       no_argument,       NULL, 'h'},
        {"list",       no_argument,       NULL, 'l'},
        {"iterations", required_argument, NULL, 'i'},
        {"perturb",    required_argument, NULL, 'p'},
        {"quantize",   required_argument, NULL, 'q'},
        {"seed",       required_argument, NULL, 's'},
        {"tuna",       required_argument, NULL, 't'},
        {"verbose",    no_argument,       NULL, 'v'},
        {NULL, 0, NULL, 0}
    };

    seed = time(NULL) + getpid();
    while (1) {
        c = getopt_long(argc, argv, "a:hli:o:p:q:s:t:v", longopts, NULL);
        if (c == -1)
            break;

        switch (c) {
        case 'a':
            if (strcmp(optarg, "inf") == 0) {
                accuracy = MAX_ACCURACY + 1;
            }
            else {
                accuracy = strtol(optarg, &end, 0);
                if (*end != '\0') {
                    fprintf(stderr, "Invalid accuracy value '%s'.\n", optarg);
                    exit(-1);
                }
            }
            break;

        case 'h':
            usage(argv[0]);
            exit(-1);

        case 'l':
            list_funcs = 1;
            break;

        case 'i':
            iterations = strtoul(optarg, &end, 0);
            if (*end != '\0') {
                fprintf(stderr, "Invalid iteration value '%s'.\n", optarg);
                exit(-1);
            }
            break;

        case 'p':
            perturb = strtod(optarg, &end);
            if (*end != '\0' || perturb < 0) {
                fprintf(stderr, "Invalid perturbation value '%s'.\n", optarg);
                exit(-1);
            }
            break;

        case 'q':
            quantize_exp = strtol(optarg, &end, 0);
            if (*end != '\0') {
                fprintf(stderr, "Invalid quantization value '%s'.\n", optarg);
                exit(-1);
            }
            quantize = pow(10.0, quantize_exp);
            break;

        case 's':
            seed = strtol(optarg, &end, 0);
            if (*end != '\0') {
                fprintf(stderr, "Invalid seed value '%s'.\n", optarg);
                exit(-1);
            }
            break;

        case 't':
            if (optarg[1] == '\0')
                tuna_mode = optarg[0];

            if (tuna_mode != 'w' && tuna_mode != 'u' &&
                tuna_mode != 's' && tuna_mode != 'o')
            {
                fprintf(stderr, "Invalid Tuna mode '%s'.\n", optarg);
                exit(-1);
            }
            break;

        case 'v':
            verbose = 1;
            break;

        default:
            exit(-1);
        }
    }

    if (list_funcs) {
        flist_print(stdout, verbose);
        exit(0);
    }

    parse_params(optind, argc, argv);
}

void parse_params(int idx, int argc, char *argv[])
{
    int i;
    char *end;

    parse_dim(argv[idx++]);
    parse_funcs(argv[idx++]);

    i = argc - idx;
    if (tuna_mode || i > 0) {
        if (i != i_cnt) {
            fprintf(stderr, "Expected %d values for test point. Got %d.\n",
                    i_cnt, i);
            exit(-1);
        }

        for (i = 0; i < i_cnt; ++i) {
            point[i] = strtod(argv[idx], &end);
            if (!end) {
                fprintf(stderr, "Invalid test point value '%s'.\n", argv[idx]);
                exit(-1);
            }
            ++idx;
        }

        single_eval = 1;
    }
}

void parse_dim(char *dim)
{
    char *end;

    if (!dim || *dim == '\0') {
        fprintf(stderr, "Search space dimensionality not specified.\n"
                "  Use `--help` for usage information.\n");
        exit(-1);
    }

    i_cnt = strtol(dim, &end, 0);
    if (*end != '\0') {
        fprintf(stderr, "Invalid search space dimensionality '%s'.\n", dim);
        exit(-1);
    }

    if (i_cnt < 2 || i_cnt > MAX_IN) {
        fprintf(stderr, "Search space dimensionality must be >= 2.\n");
        exit(-1);
    }

    if (i_cnt > MAX_IN) {
        fprintf(stderr, "Maximum search space dimensionality"
                " (%d) exceeded.\n", MAX_IN);
        exit(-1);
    }
}

void parse_funcs(char *list)
{
    char *end;

    bound_min =  INFINITY;
    bound_max = -INFINITY;
    while (list && *list) {
        char stash = '\0';
        while (isspace(*list))
            ++list;

        end = strpbrk(list, " ,:;(");
        if (end) {
            stash = *end;
            *(end++) = '\0';
        }

        if (o_cnt >= MAX_OUT) {
            fprintf(stderr, "Maximum number of output functions"
                    " (%d) exceeded.\n", MAX_OUT);
            exit(-1);
        }

        finfo[o_cnt] = flist_find(list);
        if (!finfo[o_cnt]) {
            fprintf(stderr, "'%s' is not a valid test function name.\n"
                    "  Use `--list` to show available functions.\n", list);
            exit(-1);
        }

        if (finfo[o_cnt]->n_max && i_cnt > finfo[o_cnt]->n_max) {
            fprintf(stderr, "Test function '%s' may have at most %d input"
                    " dimensions.\n", finfo[o_cnt]->name, finfo[o_cnt]->n_max);
            exit(-1);
        }

        if (bound_min > finfo[o_cnt]->b_min)
            bound_min = finfo[o_cnt]->b_min;
        if (bound_max < finfo[o_cnt]->b_max)
            bound_max = finfo[o_cnt]->b_max;

        if (stash == '(') {
            if (parse_fopts(o_cnt, &end) != 0) {
                fprintf(stderr, "Error parsing %s function options.\n", list);
                exit(-1);
            }
        }

        ++o_cnt;
        list = end;
    }

    if (o_cnt == 0) {
        fprintf(stderr, "No test functions specified.\n"
                "  Use --list to show available functions.\n");
        exit(-1);
    }
}

int parse_fopts(int idx, char **opts)
{
    int i, cnt;
    char *list = *opts;

    cnt = 1;
    for (i = 0; list[i]; ++i) {
        if (list[i] == ',')
            ++cnt;
        if (list[i] == ')')
            break;
    }
    if (list[i] != ')') {
        fprintf(stderr, "Missing end parenthesis.\n");
        return -1;
    }

    fopts[idx] = (double *) malloc(cnt * sizeof(double));
    if (!fopts[idx]) {
        fprintf(stderr, "Error allocating memory for options array.\n");
        return -1;
    }

    for (i = 0; i < cnt; ++i) {
        int len = 0;
        if (sscanf(list, " %lf %n", &fopts[idx][i], &len) > 0) {
            if (list[len] == ',') {
                list += len + 1;
                continue;
            }
            if (list[len] == ')') {
                list += len + 1;
                break;
            }
        }
        len = strcspn(list, ",)");
        fprintf(stderr, "Invalid option value '%*s'.\n", len, list);
        return -1;
    }

    while (isspace(*list)) ++list;
    if    (*list == ',')   ++list;

    *opts = list;
    return 0;
}

int start_harmony(hdesc_t *hdesc)
{
    char session_name[64], intbuf[16];
    double step;
    int i;

    /* Build the session name. */
    snprintf(session_name, sizeof(session_name),
             "test_in%d_out%d.pid%d", i_cnt, o_cnt, getpid());

    if (harmony_session_name(hdesc, session_name) != 0) {
        fprintf(stderr, "Could not set session name: %s\n",
                harmony_error_string(hdesc));
        return -1;
    }

    if (accuracy <= MAX_ACCURACY)
        step = pow(10.0, -accuracy);
    else
        step = 0;

    for (i = 0; i < i_cnt; ++i) {
        snprintf(intbuf, sizeof(intbuf), "x%d", i + 1);
        if (harmony_real(hdesc, intbuf, bound_min, bound_max, step) != 0) {
            fprintf(stderr, "Error defining '%s' tuning variable: %s\n",
                    intbuf, harmony_error_string(hdesc));
            return -1;
        }

        if (harmony_bind_real(hdesc, intbuf, &point[i]) != 0) {
            fprintf(stderr, "Failed to register variable %s: %s\n",
                    intbuf, harmony_error_string(hdesc));
            return -1;
        }
    }

    snprintf(intbuf, sizeof(intbuf), "%d", o_cnt);
    harmony_setcfg(hdesc, "PERF_COUNT", intbuf);

    snprintf(intbuf, sizeof(intbuf), "%ld", seed);
    harmony_setcfg(hdesc, "RANDOM_SEED", intbuf);

    if (harmony_launch(hdesc, NULL, 0) != 0) {
        fprintf(stderr, "Error launching tuning session: %s\n",
                harmony_error_string(hdesc));
        return -1;
    }

    if (harmony_join(hdesc, NULL, 0, session_name) != 0) {
        fprintf(stderr, "Error joining tuning session: %s\n",
                harmony_error_string(hdesc));
        return -1;
    }
    return 0;
}

void eval_func(void)
{
    int i;

    if (verbose) {
        fprintf(stdout, "(%lf", point[0]);
        for (i = 1; i < i_cnt; ++i)
            fprintf(stdout, ",%lf", point[i]);
        fprintf(stdout, ") = ");
    }

    for (i = 0; i < o_cnt; ++i)
        perf[i] = finfo[i]->f(i_cnt, point, fopts[i]);

    if (verbose) {
        fprintf(stdout, "[%lf", perf[0]);
        for (i = 1; i < o_cnt; ++i)
            fprintf(stdout, ",%lf", perf[i]);
        fprintf(stdout, "]");
    }

    if (quantize) {
        for (i = 0; i < o_cnt; ++i)
            perf[i] = quantize_value(perf[i]);

        if (verbose) {
            fprintf(stdout, " => quantized(%.*lf", quantize_exp, perf[0]);
            for (i = 1; i < o_cnt; ++i)
                fprintf(stdout, ",%.*lf", quantize_exp, perf[i]);
            fprintf(stdout, ")");
        }
    }

    if (perturb) {
        for (i = 0; i < o_cnt; ++i)
            perf[i] += random_value(0.0, perturb);

        if (verbose) {
            fprintf(stdout, " => perturbed(%lf", perf[0]);
            for (i = 1; i < o_cnt; ++i)
                fprintf(stdout, ",%lf", perf[i]);
            fprintf(stdout, ")");
        }
    }

    if (verbose && !single_eval)
        fprintf(stdout, "\n");
}

double quantize_value(double perf)
{
    return round(perf * quantize) / quantize;
}

double random_value(double min, double max)
{
    return min + ((double)rand()/(double)RAND_MAX) * (max - min);
}

void fprint_darr(FILE *fp, const char *head,
                 double *arr, int len, const char *tail)
{
    int i;

    if (head)
        fprintf(fp, "%s", head);

    if (len > 0) {
        for (i = 0; i < len; ++i) {
            if (i > 0) fprintf(fp, ",");
            fprintf(fp, "%lf", arr[i]);
        }
    }

    if (tail)
        fprintf(fp, "%s", tail);
}

void use_resources(void)
{
    unsigned int i, stall;
    double sum = 0.0;

    for (i = 0; i < o_cnt; ++i)
        sum += perf[i];

    stall = sum * 1000;

    switch (tuna_mode) {
    case 'w': /* Wall time */
        if (verbose) {
            fprintf(stdout, " ==> usleep(%d)\n", stall);
            fflush(stdout);
        }
        usleep(stall);
        break;

    case 'u': /* User time */
        if (verbose) {
            fprintf(stdout, " ==> perform %d flops\n", stall * 2);
            fflush(stdout);
        }
        for (i = 0; i < stall; ++i)
            sum = sum * (17.0/sum);
        break;

    case 's': /* System time */
        if (verbose) {
            fprintf(stdout, " ==> perform %d syscalls\n", stall * 2);
            fflush(stdout);
        }
        for (i = 0; i < stall; ++i)
            close(open("/dev/null", O_RDONLY));
        break;

    case 'o': /* Output method */
        fprint_darr(stdout, "(", perf, o_cnt, ")\n");
        break;
    }
}
