
#ifndef __LIBGRIDPOINT_H__
#define __LIBGRIDPOINT_H__ 1

#include "hpoint.h"
#include "hsignature.h"

struct lgp_info;

typedef struct gridpoint {
  hpoint_t point;
  long *indices;
} gridpoint_t;

extern const gridpoint_t GRIDPOINT_INITIALIZER;

/*
 * Stores the information that libgridpoint will need for the rest of its
 * functions.
 */
struct lgp_info *libgridpoint_init(hsignature_t *sig);

/*
 * Cleans up when the caller is finished using libgridpoint functions;
 * frees the memory referenced by the info pointer.
 */
void libgridpoint_fini(struct lgp_info *info);

/*
 * Initializes a grid point; includes calling hpoint_init on the associated
 * hpoint.
 *
 * Returns 0 on success, -1 on failure.
 */
int gridpoint_init(struct lgp_info *info, gridpoint_t *gpt);

/*
 * Cleans up a grid point once it is no longer needed; includes a call to
 * hpoint_fini on the associated hpoint.
 */
void gridpoint_fini(gridpoint_t *gpt);

/*
 * Chooses a grid point at random over the grid, with uniform probability.
 *
 * Returns 0 on success, -1 on failure.
 */
int gridpoint_rand(struct lgp_info *info, gridpoint_t *result);

/*
 * Create a grid point from a struct hpoint. The grid point should already be
 * allocated. The hpoint is copied, so it may be finalized after this call
 * without an effect on the resulting grid point.
 *
 * Returns 0 on success, -1 on failure.
 */
int gridpoint_from_hpoint(struct lgp_info *info,
                          hpoint_t *hpoint, gridpoint_t *result);

int gridpoint_copy(struct lgp_info *info,
                   gridpoint_t *dest, gridpoint_t *src);

/*
 * Starting from a grid point, move a certain number of steps along one
 * dimension. The result point must already be allocated with gridpoint_init.
 * It will be completely overwritten.
 *
 * The number of steps may be positive or negative; the sign controls the
 * direction of movement.
 *
 * Movement will stop at the grid boundary. 
 *
 * Returns -1 on error, or the number of steps actually taken (which will be
 * non-negative: it is zero for no movement, or a positive number if there was
 * any movement).
 *
 * Example: if the origin point is one step from the minimum, and steps is -3,
 * the resulting point will be on the boundary, and gridpoint_move will return
 * +1.
 */
int gridpoint_move(struct lgp_info *info,
                   gridpoint_t *origin, int dim, int steps,
                   gridpoint_t *result);


/*
 * Starting from a grid point, move in multiple dimensions. The result point
 * must already be allocated with gridpoint_init. It will be completely
 * overwritten.
 *
 * steps is an array with one entry for each grid dimension. Its elements may
 * be positive, negative, or zero; the sign controls the direction of movement
 * in each dimension. Movement in each dimension will stop at the grid
 * boundary.
 *
 * Returns -1 on error, or the total number of steps actually taken (which
 * will be non-negative: it is zero for no movement, or a positive number if
 * there was any movement). On return, steps will be populated with the actual
 * movement in each dimension, with signs preserved.
 *
 * Example: if the origin point is at {1, 3, 2}, and steps is {-3, 1, 2}, the
 * result point will be at {0, 4, 4}, the return value will be 4, and on
 * return steps will contain {-1, 1, 2} (assuming the maximum index is >=4 in
 * the second and third dimensions).
 */
int gridpoint_move_multi(struct lgp_info *info,
                         gridpoint_t *origin, int *steps,
                         gridpoint_t *result);

int printable_gridpoint(struct lgp_info *info, char *buf, int buflen, gridpoint_t *gpt);

#endif
