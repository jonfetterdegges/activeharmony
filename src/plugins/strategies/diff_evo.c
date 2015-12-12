#include "strategy.h"
#include "session-core.h"
#include "hsignature.h"
#include "hperf.h"
#include "hutil.h"
#include "hcfg.h"
#include "libvertex.h"
#include "libgridpoint.h"
#include "random.h"
#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <math.h>
#include <limits.h>

#define GiNP 50 //no of population in each generations
#define GENMAX 200 //no of generations
#define F_WEIGHT 0.3 //mutation factor
#define F_CROSS 1.3//crossing over factor
#define URN_DEPTH   5 //4 + one index to avoid; contains four random population indices
#define MAXPOP 1000 //maximum population

hcfg_info_t plugin_keyinfo[] = {
    { CFGKEY_INIT_POINT, NULL, "Initial point begin testing from." },
    { NULL }
};

int n_explorations=GiNP;
int GEN=0;
int i_seed=1345; //for psedorandom number generation
int n_outstanding; 
int outstanding[GiNP]; //this to keep track of outstanding points
int all_returned; 

struct lgp_info *lgp_info = NULL;
gridpoint_t *current_pts = NULL; // current locations for each exploration
double *current_perfs = NULL; // performance of current_pts
gridpoint_t *older_pts = NULL; // current locations for each exploration
gridpoint_t *generated_pts = NULL; // points returned from generate but not yet analyzed
double *generated_perfs = NULL; 
hsignature_t* sig_a;

hpoint_t best; //for best one in curreent generation
double   best_perf;// performance for the best one

/* Function definitions. */
void permute(int ia_urn2[], int i_urn2_depth, int i_NP, int i_avoid);
void print_point(hpoint_t ppoint);
int generator(int j, hpoint_t *ppoint, hpoint_t ppoint_a, hpoint_t ppoint_b, hpoint_t ppoint_c);
int point_id(int tstep, int exploration);
int find_min();
int left_vector_wins(double x,double y);
int copier(hpoint_t *ppoint, hpoint_t ppoint_a);
void check_outstanding();
int exploration_of_id(int id);


int strategy_init(hsignature_t* sig)
{   int i;
    hpoint_t ppoint;
   
    if (libvertex_init(sig) != 0) {
        session_error("Could not initialize vertex library.");
        return -1;
    }

    lgp_info = libgridpoint_init(sig);
    if (!lgp_info) {
        session_error("Could not initialize gridpoint library.");
    }

    current_pts = (gridpoint_t *) calloc(n_explorations, sizeof(gridpoint_t));
    current_perfs = (double *) malloc(n_explorations * sizeof(double));
    generated_pts = (gridpoint_t *) calloc(n_explorations, sizeof(gridpoint_t));
    older_pts = (gridpoint_t *) calloc(n_explorations, sizeof(gridpoint_t));
    generated_perfs = (double *) malloc(n_explorations * sizeof(double));

    if (!current_pts || !current_perfs || !generated_pts || !older_pts || !generated_perfs)  
    {
        session_error("Could not set initial population.");
        return -1;

    }
   
    for (i = 0; i < n_explorations; i++) {
        if (gridpoint_init(lgp_info, current_pts + i) < 0)
        {
        session_error("Could not set initial population.");
        return -1;

        }
        
        if (gridpoint_rand(lgp_info, current_pts + i) < 0)
        {
        session_error("Could not set initial population.");
        return -1;
        }
        
        current_pts[i].point.id = point_id(0, i);
        current_perfs[i] = HUGE_VAL;          
        generated_perfs[i] = HUGE_VAL;
        
        if (gridpoint_init(lgp_info, generated_pts + i) < 0)
        {
        session_error("Could not set initial population.");
        return -1;
        }

    }

    n_outstanding=0;
    all_returned=0;

    for(i=0;i<GiNP;i++) outstanding[i]=0;
  
    best = HPOINT_INITIALIZER;
    best_perf = HUGE_VAL;
    sig_a=sig;

    if (session_setcfg(CFGKEY_CONVERGED, "0") != 0) {
        session_error("Could not set " CFGKEY_CONVERGED " config variable.");
        return -1;
    }
    return 0;
}


int strategy_generate(hflow_t* flow, hpoint_t* point)
{       
    int exploration; 
    int   i_r1, i_r2, i_r3, i_r4, j1, k1;  // placeholders for random indexes    
    int   ia_urn2[URN_DEPTH];

    hpoint_t *ppoint=calloc(1,sizeof(hpoint_t));

    sgenrand(i_seed);// for Mersenne Twister
    check_outstanding();
    
    if (all_returned) {

        for (int i=0;i<GiNP;i++){
            
            outstanding[i]=0;
            
            permute(ia_urn2,URN_DEPTH,GiNP,i); //Pick 4 random and distinct
            i_r1 = ia_urn2[1];                 //population members
            i_r2 = ia_urn2[2];
            i_r3 = ia_urn2[3];
            //---classical strategy DE/rand/1/bin-----------------------------------------
            hpoint_copy(ppoint,&((current_pts+i)->point));
            j1 = (int)(genrand()*(ppoint->n));
            k1=0;
            
            do{ 
                generator(j1,ppoint,(current_pts+i_r1)->point,(current_pts+i_r2)->point,(current_pts+i_r3)->point);
                j1 = (j1+1)%(ppoint->n);
                k1++;
            } while ((genrand() < F_CROSS) && (k1 < ppoint->n));
  
            if (gridpoint_from_hpoint(lgp_info, ppoint, generated_pts+i) < 0) {
                       if (gridpoint_copy(lgp_info,(current_pts+i), generated_pts+i) < 0) {
                        session_error("Couldn't create gridpoint from current pts in DE generate.");
                        return -1; 
                    }
            }

        }

    n_outstanding=0;
    GEN++;

    }

    if(GEN>2 && all_returned) {
        if (!(GEN%2)){
            for (int i=0;i<GiNP;i++){            
                if (left_vector_wins(generated_perfs[i],current_perfs[i])){
                    copier(ppoint,generated_pts[i].point);
                    copier(&(current_pts[i].point),*ppoint);
            }
        }
        }

    }

    exploration = n_outstanding;
    n_outstanding++;

    if (n_outstanding>=GiNP) n_outstanding=0;
    
    if (!(GEN%2)){
        if (hpoint_copy(point, &current_pts[exploration].point) < 0) {
        session_error("Failed hpoint_copy in SA strategy_generate");
        return -1;
        }
    }
    else{
        if (hpoint_copy(point, &generated_pts[exploration].point) < 0) {
        session_error("Failed hpoint_copy in SA strategy_generate");
        return -1; 
        }
    }
    
    if (all_returned)
    flow->status = HFLOW_ACCEPT;
    
    return 0;
}

/*
 * Analyze the observed performance for this configuration point.
 */
int strategy_analyze(htrial_t* trial)
{   
    int exploration = exploration_of_id(trial->point.id);
    double perf = hperf_unify(trial->perf);
    
    if (!(GEN%2)) current_perfs[exploration]=perf;
    else generated_perfs[exploration]=perf;
    outstanding[exploration]=1;
    return 0;
}


int strategy_best(hpoint_t* point)
{
    int index_of_min=find_min();
    best=current_pts[index_of_min].point;
    if (hpoint_copy(point, &best) != 0) {
        session_error("Internal error: Could not copy point.");
        return -1;
    }
    return 0;
}

int generator(int j, hpoint_t *ppoint, hpoint_t ppoint_a, hpoint_t ppoint_b, hpoint_t ppoint_c)
{
    //probelms remaining: set at bondary; rub required generations
    hval_t* v_a=&ppoint_a.val[j];
    hval_t* v_b=&ppoint_b.val[j];
    hval_t* v_c=&ppoint_c.val[j];
    hrange_t* range_a = &sig_a->range[j];

    double rand_val=genrand();

     
    switch (v_a->type) {
      case HVAL_INT:  {ppoint->val[j].value.i=v_a->value.i+F_WEIGHT*((v_b->value.i-v_c->value.i));  
                        if ((ppoint->val[j].value.i>range_a->bounds.i.max) || (ppoint->val[j].value.i<range_a->bounds.i.min)){
                            ppoint->val[j].value.i=v_a->value.i;
                        }
                        break;}
      case HVAL_REAL: {ppoint->val[j].value.r=v_a->value.r+F_WEIGHT*((v_b->value.r-v_c->value.r));  
                        if ((ppoint->val[j].value.r>range_a->bounds.r.max) || (ppoint->val[j].value.r<range_a->bounds.r.min)){
                            ppoint->val[j].value.r=v_a->value.r;
                        }
                        break;}
      case HVAL_STR:  {
            if (rand_val>0.66)       
            {strcpy((char *)ppoint->val[j].value.s,v_c->value.s); break;}
            else if (rand_val>0.33)
            {strcpy((char *)ppoint->val[j].value.s,v_b->value.s); break;}
            else break;}//need some better heuristic for string based calculations;
      default:
          session_error("Invalid point value type");
          return -1;
      }
    return 0;
}

int copier(hpoint_t *ppoint, hpoint_t ppoint_a)
{
    for (int j = 0; j < ppoint->n; ++j){
    hval_t* v_a=&ppoint_a.val[j];
     
    switch (v_a->type) {
      case HVAL_INT:  ppoint->val[j].value.i=v_a->value.i;  break;
      case HVAL_REAL: ppoint->val[j].value.r=v_a->value.r;  break;
      case HVAL_STR:  strcpy((char *)ppoint->val[j].value.s,v_a->value.s); break;
            
      default:
          session_error("Invalid point value type");
          return -1;
      }
  }
    return 0;
}

int point_id(int tstep, int exploration) {
  return tstep * n_explorations + exploration;
}

int left_vector_wins(double x,double y){
    if (x<y) return 1;
    else return 0;
}


int find_min(){
    int index_of_min=0;
    double top_perf=HUGE_VAL;

    index_of_min=0;
    
    int i;

    for (i=0;i<GiNP;i++){
        if (current_perfs[i]<top_perf) {
            top_perf=current_perfs[i];
            index_of_min=i;
        }
    }
return index_of_min;
}

void check_outstanding(){
    all_returned=outstanding[0];
    for (int i=1;i<GiNP;i++) {
        all_returned&=outstanding[i];
    }   
} 

int exploration_of_id(int id) {
  return id % n_explorations;
}

void permute(int ia_urn2[], int i_urn2_depth, int i_NP, int i_avoid)
//Written by Storn et al.
{
    int  i, k, i_urn1, i_urn2;
    int  ia_urn1[MAXPOP] = {0};      //urn holding all indices

    k= i_NP;
    i_urn1 = 0; 
    i_urn2 = 0;
    for (i=0; i<i_NP; i++) ia_urn1[i] = i; //initialize urn1

    i_urn1 = i_avoid;                  //get rid of the index to be avoided and place it in position 0.
    while (k >= i_NP-i_urn2_depth)     //i_urn2_depth is the amount of indices wanted (must be <= NP) 
    {
       ia_urn2[i_urn2] = ia_urn1[i_urn1]; //move it into urn2
       ia_urn1[i_urn1] = ia_urn1[k-1]; //move highest index to fill gap
       k = k-1;                        //reduce number of accessible indices
       i_urn2 = i_urn2 + 1;            //next position in urn2
       i_urn1 = (int)(genrand()*k);    //choose a random index
    }
}

void print_point(hpoint_t ppoint){
  fprintf(stderr, "Point #%d: (", ppoint.id);
  for (int i = 0; i < ppoint.n; ++i) {
      hval_t* v = &ppoint.val[i];

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
 * Regenerate a point deemed invalid by a later plug-in.
 */
int strategy_rejected(hflow_t* flow, hpoint_t* point)
{ 
    return 0;
}
