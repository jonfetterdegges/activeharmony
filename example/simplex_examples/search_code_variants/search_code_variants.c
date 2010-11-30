/* This example illustrates the use of Active harmony to search a parameter
 * space for a parameterized compiler transformation framework. 
 * The underlying optimization algorithm is modified version of Nelder-Mead
 * Simplex algorithm. A more effective search algorithm (Parallel Rank
 * Ordering) is in the development phase. Please refer to our SC'05 paper 
 * (can be accessed through Active Harmony's Webpage)
 * to get preliminary idea on this algorithm.

 */

#include <math.h>
#include <stdio.h>
#include "hclient.h"
#include "hsockutil.h"

/* For illustration purposes, the performance here is defined by following
 * simple definition:
 *   perf = (p1-9)*(p1-9) + (p2-8)*(p2-8) + 
 *          (p3-7)*(p3-7) + (p4-6)*(p4-6) + 
 *          (p4-5)*(p4-5) + (p5-4)*(p5-4) +
 *          (p6-3)*(p6-3) + 200
 * All parameters are in [2-100] range
 * 
 */
int application(int p1, int p2, int p3, int p4, int p5, int p6) 
{
    int perf = 
        (p1-45)*(p1-9) + (p2-65)*(p2-8) + 
        (p3-85)*(p3-7) - (p4-75)*(p4-6) +
        (p5-55)*(p5-4) -
        (p6-45)*(p6-3) - 200;
    
    return perf;
}

int main(int argc, char **argv) {
    int i;
    int loop=200;
    int perf = -1000;
    

  printf("Starting Harmony...%d\n", argc);
  
  /* initialize the harmony server */
  harmony_startup(0);
  
  printf("Sending setup file!\n");
  
  /* send the parameter space description file to the server */
  harmony_application_setup_file("parameter_space.tcl");
  
  /* declare application's runtime tunable parameters. for example, these
   * could be blocking factor and unrolling factor for matrix
   * multiplication program.
   */
  int *param_1=NULL;
  int *param_2=NULL;
  int *param_3=NULL;
  int *param_4=NULL;
  int *param_5=NULL;
  int *param_6=NULL;
  
  /* register the tunable varibles */
  param_1=(int *)harmony_add_variable("SearchT","param_1",VAR_INT);
  param_2=(int *)harmony_add_variable("SearchT","param_2",VAR_INT);
  param_3=(int *)harmony_add_variable("SearchT","param_3",VAR_INT);
  param_4=(int *)harmony_add_variable("SearchT","param_4",VAR_INT);
  param_5=(int *)harmony_add_variable("SearchT","param_5",VAR_INT);
  param_6=(int *)harmony_add_variable("SearchT","param_6",VAR_INT);

  /* main loop */
  for (i=0;i<loop;i++) {
    printf(" %d, %d, %d, %d, %d, %d, ", *param_1, *param_2, *param_3, 
           *param_4, *param_5, *param_6);

    /* Run one full run of the application (or code variant)  with default 
       parameter settings.
       Here our application is rather simple. Definition of performance can
       be user-defined. Depending on application, it can be MFlops/sec,
       time to complete the entire run of the application, cache hits vs.
       misses and so on.

       For searching the parameter space in a Transformation framework,
       just run different parameterized code variants here. A simple
       mapping between the parameters and the code-variants is needed to
       call the appropriate code variant.
       
    */

    perf = application(*param_1, *param_2, *param_3, *param_4, *param_5, *param_6);
    printf("%d \n", perf);
    
    /* update the performance result */
    harmony_performance_update(perf);
    
    /* get the new values of params from the server. */
    harmony_request_all();
  }
  
  /* close the session */
  harmony_end();
}

