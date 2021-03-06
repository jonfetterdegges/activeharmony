/*
 * This file was created automatically from SUIF
 *   on Thu Oct 14 18:02:34 2010.
 *
 * Created by:
 * s2c 1.3.0.1 (plus local changes) compiled Tue Oct 12 14:23:42 EDT 2010 by tiwari on spoon
 *     Based on SUIF distribution 1.3.0.1
 *     Linked with:
 *   libsuif1 1.3.0.1 (plus local changes) compiled Tue Oct 12 14:23:10 EDT 2010 by tiwari on spoon
 *   libuseful 1.3.0.1 (plus local changes) compiled Tue Oct 12 14:23:17 EDT 2010 by tiwari on spoon
 */


#define __suif_min(x,y) ((x)<(y)?(x):(y))
#define __suif_max(x,y) ((x)>(y)?(x):(y))

struct hypre_Box_struct { int imin[3];
                          int imax[3]; };
struct hypre_BoxArray_struct { struct hypre_Box_struct *boxes;
                               int size;
                               int alloc_size; };
struct hypre_RankLink_struct { int rank;
                               struct hypre_RankLink_struct *next; };
struct hypre_BoxNeighbors_struct { struct hypre_BoxArray_struct *boxes;
                                   int *procs;
                                   int *ids;
                                   int first_local;
                                   int num_local;
                                   int num_periodic;
                                   struct hypre_RankLink_struct *(((*rank_links)[3])[3])[3]; };
struct hypre_StructGrid_struct { int comm;
                                 int dim;
                                 struct hypre_BoxArray_struct *boxes;
                                 int *ids;
                                 struct hypre_BoxNeighbors_struct *neighbors;
                                 int max_distance;
                                 struct hypre_Box_struct *bounding_box;
                                 int local_size;
                                 int global_size;
                                 int periodic[3];
                                 int ref_count; };
struct hypre_StructStencil_struct { int (*shape)[3];
                                    int size;
                                    int max_offset;
                                    int dim;
                                    int ref_count; };
struct hypre_CommTypeEntry_struct { int imin[3];
                                    int imax[3];
                                    int offset;
                                    int dim;
                                    int length_array[4];
                                    int stride_array[4]; };
struct hypre_CommType_struct { struct hypre_CommTypeEntry_struct **comm_entries;
                               int num_entries; };
struct hypre_CommPkg_struct { int num_values;
                              int comm;
                              int num_sends;
                              int num_recvs;
                              int *send_procs;
                              int *recv_procs;
                              struct hypre_CommType_struct **send_types;
                              struct hypre_CommType_struct **recv_types;
                              int *send_mpi_types;
                              int *recv_mpi_types;
                              struct hypre_CommType_struct *copy_from_type;
                              struct hypre_CommType_struct *copy_to_type; };
struct hypre_StructMatrix_struct { int comm;
                                   struct hypre_StructGrid_struct *grid;
                                   struct hypre_StructStencil_struct *user_stencil;
                                   struct hypre_StructStencil_struct *stencil;
                                   int num_values;
                                   struct hypre_BoxArray_struct *data_space;
                                   double *data;
                                   int data_alloced;
                                   int data_size;
                                   int **data_indices;
                                   int symmetric;
                                   int *symm_elements;
                                   int num_ghost[6];
                                   int global_size;
                                   struct hypre_CommPkg_struct *comm_pkg;
                                   int ref_count; };

extern void OUT__1__6119__(void **);

extern void OUT__1__6119__(void **__out_argv)
  {
    struct hypre_StructMatrix_struct *A;
    int ri;
    double *rp;
    int stencil_size;
    int i;
    int (*dxp_s)[15];
    int hypre__sy1;
    int hypre__sz1;
    int hypre__sy2;
    int hypre__sz2;
    int hypre__sy3;
    int hypre__sz3;
    int hypre__mx;
    int hypre__my;
    int hypre__mz;
    int si;
    int ii;
    int jj;
    int kk;
    const double *Ap_0;
    const double *xp_0;
    double *suif_tmp;
    int over1;
    int _t12;
    int _t10;
    int _t11;
    int _t13;
    int _t14;
    int _t15;
    int _t16;
    int _t17;
    int _t18;
    int _t19;
    int _t20;
    int _t21;
    int _t22;
    int _t23;
    int _t24;
    int _t25;
    int _t26;
    int _t27;
    int _t30;
    int over2;
    int _t35;
    int _t36;
    int _t37;
    int _t38;
    int _t39;
    int _t40;
    int _t41;
    int _t42;
    int _t43;
    int _t44;
    int _t45;
    int _t46;
    int _t47;
    int _t48;
    int _t49;
    int _t50;
    int _t51;
    int _t52;
    int _t53;
    int _t54;
    int _t55;
    int _t56;
    int _t57;
    int _t58;
    int _t59;
    int _t60;
    int _t61;
    int _t62;
    int t2;
    int t4;
    int t6;
    int t8;
    int t10;
    int t12;
    int t14;

    A = *(struct hypre_StructMatrix_struct **)__out_argv[20];
    ri = *(int *)__out_argv[19];
    rp = *(double **)__out_argv[18];
    stencil_size = *(int *)__out_argv[17];
    i = *(int *)__out_argv[16];
    dxp_s = __out_argv[15];
    hypre__sy1 = *(int *)__out_argv[14];
    hypre__sz1 = *(int *)__out_argv[13];
    hypre__sy2 = *(int *)__out_argv[12];
    hypre__sz2 = *(int *)__out_argv[11];
    hypre__sy3 = *(int *)__out_argv[10];
    hypre__sz3 = *(int *)__out_argv[9];
    hypre__mx = *(int *)__out_argv[8];
    hypre__my = *(int *)__out_argv[7];
    hypre__mz = *(int *)__out_argv[6];
    si = *(int *)__out_argv[5];
    ii = *(int *)__out_argv[4];
    jj = *(int *)__out_argv[3];
    kk = *(int *)__out_argv[2];
    Ap_0 = *(const double **)__out_argv[1];
    xp_0 = *(const double **)*__out_argv;
    over1 = 0;
    over2 = 0;
    for (t2 = 0; t2 <= hypre__mz - 1; t2 += 49)
      {
        for (t4 = 0; t4 <= hypre__my - 1; t4 += 22)
          {
            for (t6 = 0; t6 <= hypre__mx - 1; t6 += 43)
              {
                for (t8 = t2; t8 <= __suif_min(t2 + 48, hypre__mz - 1); t8++)
                  {
                    for (t10 = t4; t10 <= __suif_min(t4 + 21, hypre__my - 1); t10++)
                      {
                        over1 = stencil_size % 5;
                        for (t12 = 0; t12 <= -over1 + stencil_size - 1; t12 += 5)
                          {
                            over2 = (13 * t6 + hypre__mx) % 14;
                            for (t14 = t6; t14 <= __suif_min(t6 + 28, hypre__mx - over2 - 1); t14 += 14)
                              {
                                suif_tmp = &rp[ri + t14 + t10 * hypre__sy3 + t8 * hypre__sz3];
                                *suif_tmp = *suif_tmp - Ap_0[t14 + t10 * hypre__sy1 + t8 * hypre__sz1 + A->data_indices[i][t12]] * xp_0[t14 + t10 * hypre__sy2 + t8 * hypre__sz2 + ((int *)dxp_s)[t12]];
                                suif_tmp = &rp[ri + t14 + t10 * hypre__sy3 + t8 * hypre__sz3];
                                *suif_tmp = *suif_tmp - Ap_0[t14 + t10 * hypre__sy1 + t8 * hypre__sz1 + A->data_indices[i][t12 + 1]] * xp_0[t14 + t10 * hypre__sy2 + t8 * hypre__sz2 + ((int *)dxp_s)[t12 + 1]];
                                suif_tmp = &rp[ri + t14 + t10 * hypre__sy3 + t8 * hypre__sz3];
                                *suif_tmp = *suif_tmp - Ap_0[t14 + t10 * hypre__sy1 + t8 * hypre__sz1 + A->data_indices[i][t12 + 2]] * xp_0[t14 + t10 * hypre__sy2 + t8 * hypre__sz2 + ((int *)dxp_s)[t12 + 2]];
                                suif_tmp = &rp[ri + t14 + t10 * hypre__sy3 + t8 * hypre__sz3];
                                *suif_tmp = *suif_tmp - Ap_0[t14 + t10 * hypre__sy1 + t8 * hypre__sz1 + A->data_indices[i][t12 + 3]] * xp_0[t14 + t10 * hypre__sy2 + t8 * hypre__sz2 + ((int *)dxp_s)[t12 + 3]];
                                suif_tmp = &rp[ri + t14 + t10 * hypre__sy3 + t8 * hypre__sz3];
                                *suif_tmp = *suif_tmp - Ap_0[t14 + t10 * hypre__sy1 + t8 * hypre__sz1 + A->data_indices[i][t12 + 4]] * xp_0[t14 + t10 * hypre__sy2 + t8 * hypre__sz2 + ((int *)dxp_s)[t12 + 4]];
                                suif_tmp = &rp[ri + t14 + 1 + t10 * hypre__sy3 + t8 * hypre__sz3];
                                *suif_tmp = *suif_tmp - Ap_0[t14 + 1 + t10 * hypre__sy1 + t8 * hypre__sz1 + A->data_indices[i][t12]] * xp_0[t14 + 1 + t10 * hypre__sy2 + t8 * hypre__sz2 + ((int *)dxp_s)[t12]];
                                suif_tmp = &rp[ri + t14 + 1 + t10 * hypre__sy3 + t8 * hypre__sz3];
                                *suif_tmp = *suif_tmp - Ap_0[t14 + 1 + t10 * hypre__sy1 + t8 * hypre__sz1 + A->data_indices[i][t12 + 1]] * xp_0[t14 + 1 + t10 * hypre__sy2 + t8 * hypre__sz2 + ((int *)dxp_s)[t12 + 1]];
                                suif_tmp = &rp[ri + t14 + 1 + t10 * hypre__sy3 + t8 * hypre__sz3];
                                *suif_tmp = *suif_tmp - Ap_0[t14 + 1 + t10 * hypre__sy1 + t8 * hypre__sz1 + A->data_indices[i][t12 + 2]] * xp_0[t14 + 1 + t10 * hypre__sy2 + t8 * hypre__sz2 + ((int *)dxp_s)[t12 + 2]];
                                suif_tmp = &rp[ri + t14 + 1 + t10 * hypre__sy3 + t8 * hypre__sz3];
                                *suif_tmp = *suif_tmp - Ap_0[t14 + 1 + t10 * hypre__sy1 + t8 * hypre__sz1 + A->data_indices[i][t12 + 3]] * xp_0[t14 + 1 + t10 * hypre__sy2 + t8 * hypre__sz2 + ((int *)dxp_s)[t12 + 3]];
                                suif_tmp = &rp[ri + t14 + 1 + t10 * hypre__sy3 + t8 * hypre__sz3];
                                *suif_tmp = *suif_tmp - Ap_0[t14 + 1 + t10 * hypre__sy1 + t8 * hypre__sz1 + A->data_indices[i][t12 + 4]] * xp_0[t14 + 1 + t10 * hypre__sy2 + t8 * hypre__sz2 + ((int *)dxp_s)[t12 + 4]];
                                suif_tmp = &rp[ri + t14 + 2 + t10 * hypre__sy3 + t8 * hypre__sz3];
                                *suif_tmp = *suif_tmp - Ap_0[t14 + 2 + t10 * hypre__sy1 + t8 * hypre__sz1 + A->data_indices[i][t12]] * xp_0[t14 + 2 + t10 * hypre__sy2 + t8 * hypre__sz2 + ((int *)dxp_s)[t12]];
                                suif_tmp = &rp[ri + t14 + 2 + t10 * hypre__sy3 + t8 * hypre__sz3];
                                *suif_tmp = *suif_tmp - Ap_0[t14 + 2 + t10 * hypre__sy1 + t8 * hypre__sz1 + A->data_indices[i][t12 + 1]] * xp_0[t14 + 2 + t10 * hypre__sy2 + t8 * hypre__sz2 + ((int *)dxp_s)[t12 + 1]];
                                suif_tmp = &rp[ri + t14 + 2 + t10 * hypre__sy3 + t8 * hypre__sz3];
                                *suif_tmp = *suif_tmp - Ap_0[t14 + 2 + t10 * hypre__sy1 + t8 * hypre__sz1 + A->data_indices[i][t12 + 2]] * xp_0[t14 + 2 + t10 * hypre__sy2 + t8 * hypre__sz2 + ((int *)dxp_s)[t12 + 2]];
                                suif_tmp = &rp[ri + t14 + 2 + t10 * hypre__sy3 + t8 * hypre__sz3];
                                *suif_tmp = *suif_tmp - Ap_0[t14 + 2 + t10 * hypre__sy1 + t8 * hypre__sz1 + A->data_indices[i][t12 + 3]] * xp_0[t14 + 2 + t10 * hypre__sy2 + t8 * hypre__sz2 + ((int *)dxp_s)[t12 + 3]];
                                suif_tmp = &rp[ri + t14 + 2 + t10 * hypre__sy3 + t8 * hypre__sz3];
                                *suif_tmp = *suif_tmp - Ap_0[t14 + 2 + t10 * hypre__sy1 + t8 * hypre__sz1 + A->data_indices[i][t12 + 4]] * xp_0[t14 + 2 + t10 * hypre__sy2 + t8 * hypre__sz2 + ((int *)dxp_s)[t12 + 4]];
                                suif_tmp = &rp[ri + t14 + 3 + t10 * hypre__sy3 + t8 * hypre__sz3];
                                *suif_tmp = *suif_tmp - Ap_0[t14 + 3 + t10 * hypre__sy1 + t8 * hypre__sz1 + A->data_indices[i][t12]] * xp_0[t14 + 3 + t10 * hypre__sy2 + t8 * hypre__sz2 + ((int *)dxp_s)[t12]];
                                suif_tmp = &rp[ri + t14 + 3 + t10 * hypre__sy3 + t8 * hypre__sz3];
                                *suif_tmp = *suif_tmp - Ap_0[t14 + 3 + t10 * hypre__sy1 + t8 * hypre__sz1 + A->data_indices[i][t12 + 1]] * xp_0[t14 + 3 + t10 * hypre__sy2 + t8 * hypre__sz2 + ((int *)dxp_s)[t12 + 1]];
                                suif_tmp = &rp[ri + t14 + 3 + t10 * hypre__sy3 + t8 * hypre__sz3];
                                *suif_tmp = *suif_tmp - Ap_0[t14 + 3 + t10 * hypre__sy1 + t8 * hypre__sz1 + A->data_indices[i][t12 + 2]] * xp_0[t14 + 3 + t10 * hypre__sy2 + t8 * hypre__sz2 + ((int *)dxp_s)[t12 + 2]];
                                suif_tmp = &rp[ri + t14 + 3 + t10 * hypre__sy3 + t8 * hypre__sz3];
                                *suif_tmp = *suif_tmp - Ap_0[t14 + 3 + t10 * hypre__sy1 + t8 * hypre__sz1 + A->data_indices[i][t12 + 3]] * xp_0[t14 + 3 + t10 * hypre__sy2 + t8 * hypre__sz2 + ((int *)dxp_s)[t12 + 3]];
                                suif_tmp = &rp[ri + t14 + 3 + t10 * hypre__sy3 + t8 * hypre__sz3];
                                *suif_tmp = *suif_tmp - Ap_0[t14 + 3 + t10 * hypre__sy1 + t8 * hypre__sz1 + A->data_indices[i][t12 + 4]] * xp_0[t14 + 3 + t10 * hypre__sy2 + t8 * hypre__sz2 + ((int *)dxp_s)[t12 + 4]];
                                suif_tmp = &rp[ri + t14 + 4 + t10 * hypre__sy3 + t8 * hypre__sz3];
                                *suif_tmp = *suif_tmp - Ap_0[t14 + 4 + t10 * hypre__sy1 + t8 * hypre__sz1 + A->data_indices[i][t12]] * xp_0[t14 + 4 + t10 * hypre__sy2 + t8 * hypre__sz2 + ((int *)dxp_s)[t12]];
                                suif_tmp = &rp[ri + t14 + 4 + t10 * hypre__sy3 + t8 * hypre__sz3];
                                *suif_tmp = *suif_tmp - Ap_0[t14 + 4 + t10 * hypre__sy1 + t8 * hypre__sz1 + A->data_indices[i][t12 + 1]] * xp_0[t14 + 4 + t10 * hypre__sy2 + t8 * hypre__sz2 + ((int *)dxp_s)[t12 + 1]];
                                suif_tmp = &rp[ri + t14 + 4 + t10 * hypre__sy3 + t8 * hypre__sz3];
                                *suif_tmp = *suif_tmp - Ap_0[t14 + 4 + t10 * hypre__sy1 + t8 * hypre__sz1 + A->data_indices[i][t12 + 2]] * xp_0[t14 + 4 + t10 * hypre__sy2 + t8 * hypre__sz2 + ((int *)dxp_s)[t12 + 2]];
                                suif_tmp = &rp[ri + t14 + 4 + t10 * hypre__sy3 + t8 * hypre__sz3];
                                *suif_tmp = *suif_tmp - Ap_0[t14 + 4 + t10 * hypre__sy1 + t8 * hypre__sz1 + A->data_indices[i][t12 + 3]] * xp_0[t14 + 4 + t10 * hypre__sy2 + t8 * hypre__sz2 + ((int *)dxp_s)[t12 + 3]];
                                suif_tmp = &rp[ri + t14 + 4 + t10 * hypre__sy3 + t8 * hypre__sz3];
                                *suif_tmp = *suif_tmp - Ap_0[t14 + 4 + t10 * hypre__sy1 + t8 * hypre__sz1 + A->data_indices[i][t12 + 4]] * xp_0[t14 + 4 + t10 * hypre__sy2 + t8 * hypre__sz2 + ((int *)dxp_s)[t12 + 4]];
                                suif_tmp = &rp[ri + t14 + 5 + t10 * hypre__sy3 + t8 * hypre__sz3];
                                *suif_tmp = *suif_tmp - Ap_0[t14 + 5 + t10 * hypre__sy1 + t8 * hypre__sz1 + A->data_indices[i][t12]] * xp_0[t14 + 5 + t10 * hypre__sy2 + t8 * hypre__sz2 + ((int *)dxp_s)[t12]];
                                suif_tmp = &rp[ri + t14 + 5 + t10 * hypre__sy3 + t8 * hypre__sz3];
                                *suif_tmp = *suif_tmp - Ap_0[t14 + 5 + t10 * hypre__sy1 + t8 * hypre__sz1 + A->data_indices[i][t12 + 1]] * xp_0[t14 + 5 + t10 * hypre__sy2 + t8 * hypre__sz2 + ((int *)dxp_s)[t12 + 1]];
                                suif_tmp = &rp[ri + t14 + 5 + t10 * hypre__sy3 + t8 * hypre__sz3];
                                *suif_tmp = *suif_tmp - Ap_0[t14 + 5 + t10 * hypre__sy1 + t8 * hypre__sz1 + A->data_indices[i][t12 + 2]] * xp_0[t14 + 5 + t10 * hypre__sy2 + t8 * hypre__sz2 + ((int *)dxp_s)[t12 + 2]];
                                suif_tmp = &rp[ri + t14 + 5 + t10 * hypre__sy3 + t8 * hypre__sz3];
                                *suif_tmp = *suif_tmp - Ap_0[t14 + 5 + t10 * hypre__sy1 + t8 * hypre__sz1 + A->data_indices[i][t12 + 3]] * xp_0[t14 + 5 + t10 * hypre__sy2 + t8 * hypre__sz2 + ((int *)dxp_s)[t12 + 3]];
                                suif_tmp = &rp[ri + t14 + 5 + t10 * hypre__sy3 + t8 * hypre__sz3];
                                *suif_tmp = *suif_tmp - Ap_0[t14 + 5 + t10 * hypre__sy1 + t8 * hypre__sz1 + A->data_indices[i][t12 + 4]] * xp_0[t14 + 5 + t10 * hypre__sy2 + t8 * hypre__sz2 + ((int *)dxp_s)[t12 + 4]];
                                suif_tmp = &rp[ri + t14 + 6 + t10 * hypre__sy3 + t8 * hypre__sz3];
                                *suif_tmp = *suif_tmp - Ap_0[t14 + 6 + t10 * hypre__sy1 + t8 * hypre__sz1 + A->data_indices[i][t12]] * xp_0[t14 + 6 + t10 * hypre__sy2 + t8 * hypre__sz2 + ((int *)dxp_s)[t12]];
                                suif_tmp = &rp[ri + t14 + 6 + t10 * hypre__sy3 + t8 * hypre__sz3];
                                *suif_tmp = *suif_tmp - Ap_0[t14 + 6 + t10 * hypre__sy1 + t8 * hypre__sz1 + A->data_indices[i][t12 + 1]] * xp_0[t14 + 6 + t10 * hypre__sy2 + t8 * hypre__sz2 + ((int *)dxp_s)[t12 + 1]];
                                suif_tmp = &rp[ri + t14 + 6 + t10 * hypre__sy3 + t8 * hypre__sz3];
                                *suif_tmp = *suif_tmp - Ap_0[t14 + 6 + t10 * hypre__sy1 + t8 * hypre__sz1 + A->data_indices[i][t12 + 2]] * xp_0[t14 + 6 + t10 * hypre__sy2 + t8 * hypre__sz2 + ((int *)dxp_s)[t12 + 2]];
                                suif_tmp = &rp[ri + t14 + 6 + t10 * hypre__sy3 + t8 * hypre__sz3];
                                *suif_tmp = *suif_tmp - Ap_0[t14 + 6 + t10 * hypre__sy1 + t8 * hypre__sz1 + A->data_indices[i][t12 + 3]] * xp_0[t14 + 6 + t10 * hypre__sy2 + t8 * hypre__sz2 + ((int *)dxp_s)[t12 + 3]];
                                suif_tmp = &rp[ri + t14 + 6 + t10 * hypre__sy3 + t8 * hypre__sz3];
                                *suif_tmp = *suif_tmp - Ap_0[t14 + 6 + t10 * hypre__sy1 + t8 * hypre__sz1 + A->data_indices[i][t12 + 4]] * xp_0[t14 + 6 + t10 * hypre__sy2 + t8 * hypre__sz2 + ((int *)dxp_s)[t12 + 4]];
                                suif_tmp = &rp[ri + t14 + 7 + t10 * hypre__sy3 + t8 * hypre__sz3];
                                *suif_tmp = *suif_tmp - Ap_0[t14 + 7 + t10 * hypre__sy1 + t8 * hypre__sz1 + A->data_indices[i][t12]] * xp_0[t14 + 7 + t10 * hypre__sy2 + t8 * hypre__sz2 + ((int *)dxp_s)[t12]];
                                suif_tmp = &rp[ri + t14 + 7 + t10 * hypre__sy3 + t8 * hypre__sz3];
                                *suif_tmp = *suif_tmp - Ap_0[t14 + 7 + t10 * hypre__sy1 + t8 * hypre__sz1 + A->data_indices[i][t12 + 1]] * xp_0[t14 + 7 + t10 * hypre__sy2 + t8 * hypre__sz2 + ((int *)dxp_s)[t12 + 1]];
                                suif_tmp = &rp[ri + t14 + 7 + t10 * hypre__sy3 + t8 * hypre__sz3];
                                *suif_tmp = *suif_tmp - Ap_0[t14 + 7 + t10 * hypre__sy1 + t8 * hypre__sz1 + A->data_indices[i][t12 + 2]] * xp_0[t14 + 7 + t10 * hypre__sy2 + t8 * hypre__sz2 + ((int *)dxp_s)[t12 + 2]];
                                suif_tmp = &rp[ri + t14 + 7 + t10 * hypre__sy3 + t8 * hypre__sz3];
                                *suif_tmp = *suif_tmp - Ap_0[t14 + 7 + t10 * hypre__sy1 + t8 * hypre__sz1 + A->data_indices[i][t12 + 3]] * xp_0[t14 + 7 + t10 * hypre__sy2 + t8 * hypre__sz2 + ((int *)dxp_s)[t12 + 3]];
                                suif_tmp = &rp[ri + t14 + 7 + t10 * hypre__sy3 + t8 * hypre__sz3];
                                *suif_tmp = *suif_tmp - Ap_0[t14 + 7 + t10 * hypre__sy1 + t8 * hypre__sz1 + A->data_indices[i][t12 + 4]] * xp_0[t14 + 7 + t10 * hypre__sy2 + t8 * hypre__sz2 + ((int *)dxp_s)[t12 + 4]];
                                suif_tmp = &rp[ri + t14 + 8 + t10 * hypre__sy3 + t8 * hypre__sz3];
                                *suif_tmp = *suif_tmp - Ap_0[t14 + 8 + t10 * hypre__sy1 + t8 * hypre__sz1 + A->data_indices[i][t12]] * xp_0[t14 + 8 + t10 * hypre__sy2 + t8 * hypre__sz2 + ((int *)dxp_s)[t12]];
                                suif_tmp = &rp[ri + t14 + 8 + t10 * hypre__sy3 + t8 * hypre__sz3];
                                *suif_tmp = *suif_tmp - Ap_0[t14 + 8 + t10 * hypre__sy1 + t8 * hypre__sz1 + A->data_indices[i][t12 + 1]] * xp_0[t14 + 8 + t10 * hypre__sy2 + t8 * hypre__sz2 + ((int *)dxp_s)[t12 + 1]];
                                suif_tmp = &rp[ri + t14 + 8 + t10 * hypre__sy3 + t8 * hypre__sz3];
                                *suif_tmp = *suif_tmp - Ap_0[t14 + 8 + t10 * hypre__sy1 + t8 * hypre__sz1 + A->data_indices[i][t12 + 2]] * xp_0[t14 + 8 + t10 * hypre__sy2 + t8 * hypre__sz2 + ((int *)dxp_s)[t12 + 2]];
                                suif_tmp = &rp[ri + t14 + 8 + t10 * hypre__sy3 + t8 * hypre__sz3];
                                *suif_tmp = *suif_tmp - Ap_0[t14 + 8 + t10 * hypre__sy1 + t8 * hypre__sz1 + A->data_indices[i][t12 + 3]] * xp_0[t14 + 8 + t10 * hypre__sy2 + t8 * hypre__sz2 + ((int *)dxp_s)[t12 + 3]];
                                suif_tmp = &rp[ri + t14 + 8 + t10 * hypre__sy3 + t8 * hypre__sz3];
                                *suif_tmp = *suif_tmp - Ap_0[t14 + 8 + t10 * hypre__sy1 + t8 * hypre__sz1 + A->data_indices[i][t12 + 4]] * xp_0[t14 + 8 + t10 * hypre__sy2 + t8 * hypre__sz2 + ((int *)dxp_s)[t12 + 4]];
                                suif_tmp = &rp[ri + t14 + 9 + t10 * hypre__sy3 + t8 * hypre__sz3];
                                *suif_tmp = *suif_tmp - Ap_0[t14 + 9 + t10 * hypre__sy1 + t8 * hypre__sz1 + A->data_indices[i][t12]] * xp_0[t14 + 9 + t10 * hypre__sy2 + t8 * hypre__sz2 + ((int *)dxp_s)[t12]];
                                suif_tmp = &rp[ri + t14 + 9 + t10 * hypre__sy3 + t8 * hypre__sz3];
                                *suif_tmp = *suif_tmp - Ap_0[t14 + 9 + t10 * hypre__sy1 + t8 * hypre__sz1 + A->data_indices[i][t12 + 1]] * xp_0[t14 + 9 + t10 * hypre__sy2 + t8 * hypre__sz2 + ((int *)dxp_s)[t12 + 1]];
                                suif_tmp = &rp[ri + t14 + 9 + t10 * hypre__sy3 + t8 * hypre__sz3];
                                *suif_tmp = *suif_tmp - Ap_0[t14 + 9 + t10 * hypre__sy1 + t8 * hypre__sz1 + A->data_indices[i][t12 + 2]] * xp_0[t14 + 9 + t10 * hypre__sy2 + t8 * hypre__sz2 + ((int *)dxp_s)[t12 + 2]];
                                suif_tmp = &rp[ri + t14 + 9 + t10 * hypre__sy3 + t8 * hypre__sz3];
                                *suif_tmp = *suif_tmp - Ap_0[t14 + 9 + t10 * hypre__sy1 + t8 * hypre__sz1 + A->data_indices[i][t12 + 3]] * xp_0[t14 + 9 + t10 * hypre__sy2 + t8 * hypre__sz2 + ((int *)dxp_s)[t12 + 3]];
                                suif_tmp = &rp[ri + t14 + 9 + t10 * hypre__sy3 + t8 * hypre__sz3];
                                *suif_tmp = *suif_tmp - Ap_0[t14 + 9 + t10 * hypre__sy1 + t8 * hypre__sz1 + A->data_indices[i][t12 + 4]] * xp_0[t14 + 9 + t10 * hypre__sy2 + t8 * hypre__sz2 + ((int *)dxp_s)[t12 + 4]];
                                suif_tmp = &rp[ri + t14 + 10 + t10 * hypre__sy3 + t8 * hypre__sz3];
                                *suif_tmp = *suif_tmp - Ap_0[t14 + 10 + t10 * hypre__sy1 + t8 * hypre__sz1 + A->data_indices[i][t12]] * xp_0[t14 + 10 + t10 * hypre__sy2 + t8 * hypre__sz2 + ((int *)dxp_s)[t12]];
                                suif_tmp = &rp[ri + t14 + 10 + t10 * hypre__sy3 + t8 * hypre__sz3];
                                *suif_tmp = *suif_tmp - Ap_0[t14 + 10 + t10 * hypre__sy1 + t8 * hypre__sz1 + A->data_indices[i][t12 + 1]] * xp_0[t14 + 10 + t10 * hypre__sy2 + t8 * hypre__sz2 + ((int *)dxp_s)[t12 + 1]];
                                suif_tmp = &rp[ri + t14 + 10 + t10 * hypre__sy3 + t8 * hypre__sz3];
                                *suif_tmp = *suif_tmp - Ap_0[t14 + 10 + t10 * hypre__sy1 + t8 * hypre__sz1 + A->data_indices[i][t12 + 2]] * xp_0[t14 + 10 + t10 * hypre__sy2 + t8 * hypre__sz2 + ((int *)dxp_s)[t12 + 2]];
                                suif_tmp = &rp[ri + t14 + 10 + t10 * hypre__sy3 + t8 * hypre__sz3];
                                *suif_tmp = *suif_tmp - Ap_0[t14 + 10 + t10 * hypre__sy1 + t8 * hypre__sz1 + A->data_indices[i][t12 + 3]] * xp_0[t14 + 10 + t10 * hypre__sy2 + t8 * hypre__sz2 + ((int *)dxp_s)[t12 + 3]];
                                suif_tmp = &rp[ri + t14 + 10 + t10 * hypre__sy3 + t8 * hypre__sz3];
                                *suif_tmp = *suif_tmp - Ap_0[t14 + 10 + t10 * hypre__sy1 + t8 * hypre__sz1 + A->data_indices[i][t12 + 4]] * xp_0[t14 + 10 + t10 * hypre__sy2 + t8 * hypre__sz2 + ((int *)dxp_s)[t12 + 4]];
                                suif_tmp = &rp[ri + t14 + 11 + t10 * hypre__sy3 + t8 * hypre__sz3];
                                *suif_tmp = *suif_tmp - Ap_0[t14 + 11 + t10 * hypre__sy1 + t8 * hypre__sz1 + A->data_indices[i][t12]] * xp_0[t14 + 11 + t10 * hypre__sy2 + t8 * hypre__sz2 + ((int *)dxp_s)[t12]];
                                suif_tmp = &rp[ri + t14 + 11 + t10 * hypre__sy3 + t8 * hypre__sz3];
                                *suif_tmp = *suif_tmp - Ap_0[t14 + 11 + t10 * hypre__sy1 + t8 * hypre__sz1 + A->data_indices[i][t12 + 1]] * xp_0[t14 + 11 + t10 * hypre__sy2 + t8 * hypre__sz2 + ((int *)dxp_s)[t12 + 1]];
                                suif_tmp = &rp[ri + t14 + 11 + t10 * hypre__sy3 + t8 * hypre__sz3];
                                *suif_tmp = *suif_tmp - Ap_0[t14 + 11 + t10 * hypre__sy1 + t8 * hypre__sz1 + A->data_indices[i][t12 + 2]] * xp_0[t14 + 11 + t10 * hypre__sy2 + t8 * hypre__sz2 + ((int *)dxp_s)[t12 + 2]];
                                suif_tmp = &rp[ri + t14 + 11 + t10 * hypre__sy3 + t8 * hypre__sz3];
                                *suif_tmp = *suif_tmp - Ap_0[t14 + 11 + t10 * hypre__sy1 + t8 * hypre__sz1 + A->data_indices[i][t12 + 3]] * xp_0[t14 + 11 + t10 * hypre__sy2 + t8 * hypre__sz2 + ((int *)dxp_s)[t12 + 3]];
                                suif_tmp = &rp[ri + t14 + 11 + t10 * hypre__sy3 + t8 * hypre__sz3];
                                *suif_tmp = *suif_tmp - Ap_0[t14 + 11 + t10 * hypre__sy1 + t8 * hypre__sz1 + A->data_indices[i][t12 + 4]] * xp_0[t14 + 11 + t10 * hypre__sy2 + t8 * hypre__sz2 + ((int *)dxp_s)[t12 + 4]];
                                suif_tmp = &rp[ri + t14 + 12 + t10 * hypre__sy3 + t8 * hypre__sz3];
                                *suif_tmp = *suif_tmp - Ap_0[t14 + 12 + t10 * hypre__sy1 + t8 * hypre__sz1 + A->data_indices[i][t12]] * xp_0[t14 + 12 + t10 * hypre__sy2 + t8 * hypre__sz2 + ((int *)dxp_s)[t12]];
                                suif_tmp = &rp[ri + t14 + 12 + t10 * hypre__sy3 + t8 * hypre__sz3];
                                *suif_tmp = *suif_tmp - Ap_0[t14 + 12 + t10 * hypre__sy1 + t8 * hypre__sz1 + A->data_indices[i][t12 + 1]] * xp_0[t14 + 12 + t10 * hypre__sy2 + t8 * hypre__sz2 + ((int *)dxp_s)[t12 + 1]];
                                suif_tmp = &rp[ri + t14 + 12 + t10 * hypre__sy3 + t8 * hypre__sz3];
                                *suif_tmp = *suif_tmp - Ap_0[t14 + 12 + t10 * hypre__sy1 + t8 * hypre__sz1 + A->data_indices[i][t12 + 2]] * xp_0[t14 + 12 + t10 * hypre__sy2 + t8 * hypre__sz2 + ((int *)dxp_s)[t12 + 2]];
                                suif_tmp = &rp[ri + t14 + 12 + t10 * hypre__sy3 + t8 * hypre__sz3];
                                *suif_tmp = *suif_tmp - Ap_0[t14 + 12 + t10 * hypre__sy1 + t8 * hypre__sz1 + A->data_indices[i][t12 + 3]] * xp_0[t14 + 12 + t10 * hypre__sy2 + t8 * hypre__sz2 + ((int *)dxp_s)[t12 + 3]];
                                suif_tmp = &rp[ri + t14 + 12 + t10 * hypre__sy3 + t8 * hypre__sz3];
                                *suif_tmp = *suif_tmp - Ap_0[t14 + 12 + t10 * hypre__sy1 + t8 * hypre__sz1 + A->data_indices[i][t12 + 4]] * xp_0[t14 + 12 + t10 * hypre__sy2 + t8 * hypre__sz2 + ((int *)dxp_s)[t12 + 4]];
                                suif_tmp = &rp[ri + t14 + 13 + t10 * hypre__sy3 + t8 * hypre__sz3];
                                *suif_tmp = *suif_tmp - Ap_0[t14 + 13 + t10 * hypre__sy1 + t8 * hypre__sz1 + A->data_indices[i][t12]] * xp_0[t14 + 13 + t10 * hypre__sy2 + t8 * hypre__sz2 + ((int *)dxp_s)[t12]];
                                suif_tmp = &rp[ri + t14 + 13 + t10 * hypre__sy3 + t8 * hypre__sz3];
                                *suif_tmp = *suif_tmp - Ap_0[t14 + 13 + t10 * hypre__sy1 + t8 * hypre__sz1 + A->data_indices[i][t12 + 1]] * xp_0[t14 + 13 + t10 * hypre__sy2 + t8 * hypre__sz2 + ((int *)dxp_s)[t12 + 1]];
                                suif_tmp = &rp[ri + t14 + 13 + t10 * hypre__sy3 + t8 * hypre__sz3];
                                *suif_tmp = *suif_tmp - Ap_0[t14 + 13 + t10 * hypre__sy1 + t8 * hypre__sz1 + A->data_indices[i][t12 + 2]] * xp_0[t14 + 13 + t10 * hypre__sy2 + t8 * hypre__sz2 + ((int *)dxp_s)[t12 + 2]];
                                suif_tmp = &rp[ri + t14 + 13 + t10 * hypre__sy3 + t8 * hypre__sz3];
                                *suif_tmp = *suif_tmp - Ap_0[t14 + 13 + t10 * hypre__sy1 + t8 * hypre__sz1 + A->data_indices[i][t12 + 3]] * xp_0[t14 + 13 + t10 * hypre__sy2 + t8 * hypre__sz2 + ((int *)dxp_s)[t12 + 3]];
                                suif_tmp = &rp[ri + t14 + 13 + t10 * hypre__sy3 + t8 * hypre__sz3];
                                *suif_tmp = *suif_tmp - Ap_0[t14 + 13 + t10 * hypre__sy1 + t8 * hypre__sz1 + A->data_indices[i][t12 + 4]] * xp_0[t14 + 13 + t10 * hypre__sy2 + t8 * hypre__sz2 + ((int *)dxp_s)[t12 + 4]];
                              }
                            for (t14 = __suif_max(hypre__mx - over2, t6); t14 <= __suif_min(hypre__mx - 1, t6 + 42); t14++)
                              {
                                suif_tmp = &rp[ri + t14 + t10 * hypre__sy3 + t8 * hypre__sz3];
                                *suif_tmp = *suif_tmp - Ap_0[t14 + t10 * hypre__sy1 + t8 * hypre__sz1 + A->data_indices[i][t12]] * xp_0[t14 + t10 * hypre__sy2 + t8 * hypre__sz2 + ((int *)dxp_s)[t12]];
                                suif_tmp = &rp[ri + t14 + t10 * hypre__sy3 + t8 * hypre__sz3];
                                *suif_tmp = *suif_tmp - Ap_0[t14 + t10 * hypre__sy1 + t8 * hypre__sz1 + A->data_indices[i][t12 + 1]] * xp_0[t14 + t10 * hypre__sy2 + t8 * hypre__sz2 + ((int *)dxp_s)[t12 + 1]];
                                suif_tmp = &rp[ri + t14 + t10 * hypre__sy3 + t8 * hypre__sz3];
                                *suif_tmp = *suif_tmp - Ap_0[t14 + t10 * hypre__sy1 + t8 * hypre__sz1 + A->data_indices[i][t12 + 2]] * xp_0[t14 + t10 * hypre__sy2 + t8 * hypre__sz2 + ((int *)dxp_s)[t12 + 2]];
                                suif_tmp = &rp[ri + t14 + t10 * hypre__sy3 + t8 * hypre__sz3];
                                *suif_tmp = *suif_tmp - Ap_0[t14 + t10 * hypre__sy1 + t8 * hypre__sz1 + A->data_indices[i][t12 + 3]] * xp_0[t14 + t10 * hypre__sy2 + t8 * hypre__sz2 + ((int *)dxp_s)[t12 + 3]];
                                suif_tmp = &rp[ri + t14 + t10 * hypre__sy3 + t8 * hypre__sz3];
                                *suif_tmp = *suif_tmp - Ap_0[t14 + t10 * hypre__sy1 + t8 * hypre__sz1 + A->data_indices[i][t12 + 4]] * xp_0[t14 + t10 * hypre__sy2 + t8 * hypre__sz2 + ((int *)dxp_s)[t12 + 4]];
                              }
                            if (over2 + t14 + 1 <= hypre__mx)
                              {
                                suif_tmp = &rp[ri + t6 + 42 + t10 * hypre__sy3 + t8 * hypre__sz3];
                                *suif_tmp = *suif_tmp - Ap_0[t6 + 42 + t10 * hypre__sy1 + t8 * hypre__sz1 + A->data_indices[i][t12]] * xp_0[t6 + 42 + t10 * hypre__sy2 + t8 * hypre__sz2 + ((int *)dxp_s)[t12]];
                                suif_tmp = &rp[ri + t6 + 42 + t10 * hypre__sy3 + t8 * hypre__sz3];
                                *suif_tmp = *suif_tmp - Ap_0[t6 + 42 + t10 * hypre__sy1 + t8 * hypre__sz1 + A->data_indices[i][t12 + 1]] * xp_0[t6 + 42 + t10 * hypre__sy2 + t8 * hypre__sz2 + ((int *)dxp_s)[t12 + 1]];
                                suif_tmp = &rp[ri + t6 + 42 + t10 * hypre__sy3 + t8 * hypre__sz3];
                                *suif_tmp = *suif_tmp - Ap_0[t6 + 42 + t10 * hypre__sy1 + t8 * hypre__sz1 + A->data_indices[i][t12 + 2]] * xp_0[t6 + 42 + t10 * hypre__sy2 + t8 * hypre__sz2 + ((int *)dxp_s)[t12 + 2]];
                                suif_tmp = &rp[ri + t6 + 42 + t10 * hypre__sy3 + t8 * hypre__sz3];
                                *suif_tmp = *suif_tmp - Ap_0[t6 + 42 + t10 * hypre__sy1 + t8 * hypre__sz1 + A->data_indices[i][t12 + 3]] * xp_0[t6 + 42 + t10 * hypre__sy2 + t8 * hypre__sz2 + ((int *)dxp_s)[t12 + 3]];
                              }
                            else
                              {
                                suif_tmp = &rp[ri + t6 + 42 + t10 * hypre__sy3 + t8 * hypre__sz3];
                                *suif_tmp = *suif_tmp - Ap_0[t6 + 42 + t10 * hypre__sy1 + t8 * hypre__sz1 + A->data_indices[i][t12 + 4]] * xp_0[t6 + 42 + t10 * hypre__sy2 + t8 * hypre__sz2 + ((int *)dxp_s)[t12 + 4]];
                              }
                          }
                        for (t12 = __suif_max(stencil_size - over1, 0); t12 <= stencil_size - 1; t12++)
                          {
                            for (t14 = t6; t14 <= __suif_min(t6 + 42, hypre__mx - 1); t14++)
                              {
                                suif_tmp = &rp[ri + t14 + t10 * hypre__sy3 + t8 * hypre__sz3];
                                *suif_tmp = *suif_tmp - Ap_0[t14 + t10 * hypre__sy1 + t8 * hypre__sz1 + A->data_indices[i][t12]] * xp_0[t14 + t10 * hypre__sy2 + t8 * hypre__sz2 + ((int *)dxp_s)[t12]];
                              }
                          }
                      }
                  }
              }
          }
      }
    *(int *)__out_argv[2] = kk;
    *(int *)__out_argv[3] = jj;
    *(int *)__out_argv[4] = ii;
    *(int *)__out_argv[5] = si;
    *(double **)__out_argv[18] = rp;
    *(struct hypre_StructMatrix_struct **)__out_argv[20] = A;
    return;
  }
