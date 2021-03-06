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

/* File : nearest_neighbor.i */
%module nearest_neighbor
%{
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>


#include "putil.h"
#include "pmesgs.h"
#include "psockutil.h"
#include "StringTokenizer.h"
#include "pclient.h"

%}
int projection_startup(char* hostname, int sport);
void projection_end();
int is_a_valid_point(char* point);
void simplex_construction(char* request, char* filename);
char* do_projection_one_point(char *request);
char* projection_sim_construction_2(char* request, int mesg_type);
char* string_to_char_star(string str);
char* do_projection_entire_simplex(char *request);

/*
double rosen_2d(double x1, double x2);
double weird_1_2d(double x1_, double x2_);
double rosen_4d(double x1, double x2,double x3, double x4);
double sombrero_2d(double x, double y);

double rastrigin_2d(double x1_, double x2_);
double rosen_6d(double x1_, double x2_,double x3_, double x4_,double x5_,double x6_);
double sombrero_6d(double x1_, double x2_,double x3_, double x4_,double x5_,double x6_);
double rastrigin_6d(double x1_, double x2_,double x3_, double x4_,double x5_,double x6_);
double quad_6d(double x1_, double x2_,double x3_, double x4_,double x5_,double x6_);
*/ 
