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
#include "hclient.h"
//#include "hsockutil.h"
#include <mpi.h>
#include <sys/time.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>


#define PERF_MAX 2147483647
#define SEARCH_MAX 200
#define EXT_SUCCESS 1
#define EXT_FAILURE -1
#define TAG 1234


// performance function
int application(int p1, int p2, int p3, int p4, int p5, int p6)
{

    // Each parameter can take a value between 1 and 50 inclusive.
    // global best perf: -4112
    //   for: param_1: 27, param_2: 36, param_3: 46, param_4: 1, param_5:
    // 29, param_6: 50
    int perf =
        (p1-45)*(p1-9) + (p2-65)*(p2-8) +
        (p3-85)*(p3-7) - (p4-75)*(p4-6) +
        (p5-55)*(p5-4) -
        (p6-45)*(p6-3) - 200;

    return perf;
}

// timer function
double now()
{
  struct timeval tim;
  gettimeofday(&tim,NULL);
  double t1=tim.tv_sec + (tim.tv_usec/1000000.0);
  return t1;
}


// driver
int  main(int argc, char *argv[])
{
    int    rank, simplex_size, i;
    char   *host_name;
    double time_initial, time_current, time;
    char  app_desc[128];
    int buffer[1];int code_complete=0;

    MPI_Status recv_status;
    double t1, t2;
    
    /* MPI Initialization */
    MPI_Init(&argc,&argv);
    time_initial  = MPI_Wtime();
    /* get the rank */
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    /* get the simplex size */
    MPI_Comm_size(MPI_COMM_WORLD, &simplex_size);

    /* allocate space for the host name */
    host_name    = (char *)calloc(80,sizeof(char));

    gethostname(host_name,80);

    printf("sanity check, %s \n", host_name);

    /* Insert Harmony API calls here */

    /* initialize the harmony server */
    harmony_startup(0);

    /* send in the application description to the server */
    printf("sending the app description \n");
    sprintf(app_desc, "offline.tcl");
    harmony_application_setup_file("offline.tcl");

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
    printf("Adding harmony variables ... \n");
    /* register the tunable varibles */
    param_1=(int *)harmony_add_variable("OfflineT","param_1",VAR_INT);
    param_2=(int *)harmony_add_variable("OfflineT","param_2",VAR_INT);
    param_3=(int *)harmony_add_variable("OfflineT","param_3",VAR_INT);
    param_4=(int *)harmony_add_variable("OfflineT","param_4",VAR_INT);
    param_5=(int *)harmony_add_variable("OfflineT","param_5",VAR_INT);
    param_6=(int *)harmony_add_variable("OfflineT","param_6",VAR_INT);

    /* barrier: let everyone get to this point */
    MPI_Barrier(MPI_COMM_WORLD);

    /* send in the default performance number to get things started at the
     * server end */
    // This can be the default performance, for example.
    printf("sending the dummy perf \n");
    harmony_performance_update(PERF_MAX);
    harmony_request_all();
    int j;
    double perf_f;
    int perf;
    int search_iter=1;

    int harmony_ended=0;

    while(!harmony_ended && search_iter < SEARCH_MAX)
    {
        /* Run one full run of the application with default parameter settings.
           Definition of performance can be user-defined. Depending on
           application, it can be MFlops/sec, time to complete the entire
           run of the application, cache hits vs. misses and so on.
        */

        // get the new set of parameters from the server
      // new: int evaluate=harmony_request_all();

      harmony_request_all();
        search_iter++;
        // evaluate the parameters
        printf("Evaluating the parameters: \n");
        perf=application(*param_1, *param_2, *param_3, *param_4, *param_5, *param_6);

        printf("rank :%d, param_1: %d, param_2: %d, param_3: %d, param_4: %d, param_5: %d, param_6: %d, perf: %d\n", 
               rank, *param_1, *param_2, *param_3, *param_4, *param_5, *param_6, perf);

	    MPI_Barrier(MPI_COMM_WORLD);
	
        harmony_performance_update(perf);

        MPI_Barrier(MPI_COMM_WORLD);
        // check if the search is done
        printf("harmony_ended call \n");
        //harmony_ended=harmony_check_convergence();
        //harmony_ended=(int)(atof((char*)harmony_database_lookup()));
        //printf("harmony_ended %d \n", harmony_ended);
        //int code_complete=harmony_code_generation_complete();

        printf("rank: %d code_status: %d \n", rank, code_complete);

        if(harmony_ended)
        {
            harmony_request_all();
            perf=application(*param_1, *param_2, *param_3, *param_4, *param_5, *param_6);

            printf("rank :%d, param_1: %d, param_2: %d, param_3: %d, param_4: %d, param_5: %d, param_6: %d, perf: %d\n", 
                   rank, *param_1, *param_2, *param_3, *param_4, *param_5, *param_6, perf);
        }

    } //while(search_iter < SEARCH_MAX);

    harmony_end();

    time_current  = MPI_Wtime();
    time  = time_current - time_initial;
    printf("%.3f tid=%i : machine=%s [# CPUs=%i]\n",
           time, rank, host_name, simplex_size);
    MPI_Finalize();

    return 0;
}
