#include <stdlib.h>
#include <string.h>

#include "libgridpoint.h"
#include "session-core.h"

struct lgp_info {
  hsignature_t sig;
  long *max_indices;
};

// forward declarations
int point_from_indices(struct lgp_info *info, gridpoint_t *gpt);

struct lgp_info *libgridpoint_init(hsignature_t *sig) {
  struct lgp_info *info;
  if (!sig) {
    session_error("Null signature passed to libgridpoint_init");
    return NULL;
  }

  info = (struct lgp_info *) malloc(sizeof(struct lgp_info)
                                    + sig->range_len * sizeof(long));
  if (!info)
    goto error;
  info->sig = HSIGNATURE_INITIALIZER;
  if (hsignature_copy(&info->sig, sig) < 0) {
    session_error("Error copying signature in libgrindpoint_init");
    goto error;
  }

  void *vinfo = (void *) info;
  info->max_indices = (long *) (vinfo + sizeof(struct lgp_info));

  int i;
  for (i = 0; i < info->sig.range_len; i++) {
    info->max_indices[i] = (int) hrange_max_idx(sig->range + i);
  }


  return info;

 error:
  if (info) {
    if (info->sig.name)
      hsignature_fini(&info->sig);
    free(info);
  }
  return NULL;
}

void libgridpoint_fini(struct lgp_info *info) {
  if (info) {
    if (info->sig.name)
      hsignature_fini(&info->sig);
    free(info);
  }
}

int gridpoint_init(struct lgp_info *info, gridpoint_t *gpt) {
  int result;

  gpt->indices = (long *) calloc(info->sig.range_len, sizeof(long));
  if (!gpt->indices)
    return -1;

  gpt->point = HPOINT_INITIALIZER;
  result = hpoint_init(&gpt->point, info->sig.range_len);
  if (result < 0) {
    session_error("Error in hpoint_init called from gridpoint_init");
    goto error;
  }

  return 0;

 error:
  hpoint_fini(&gpt->point);
  free(gpt->indices);
  return -1;
}

void gridpoint_fini(gridpoint_t *gpt) {
  hpoint_fini(&gpt->point);
  free(gpt->indices);
}

int gridpoint_rand(struct lgp_info *info, gridpoint_t *gpt) {
  int i;
  for (i = 0; i < info->sig.range_len; i++) {
    gpt->indices[i] = (long) (drand48() * info->max_indices[i]);
  }
  if (point_from_indices(info, gpt) < 0)
    return -1;
  
  return 0;
}

int gridpoint_from_hpoint(struct lgp_info *info,
                          hpoint_t *hpoint, gridpoint_t *result) {
  // copy the hpoint
  hpoint_fini(&result->point);
  if (hpoint_copy(&result->point, hpoint) < 0)
    goto error;

  // infer indices
  int i;
  for (i = 0; i < info->sig.range_len; i++) {
    hrange_t *rng = info->sig.range + i;
    switch (rng->type) {
    case HVAL_INT:
      result->indices[i] = hrange_int_index(&rng->bounds.i, hpoint->val->value.i);
      break;
    case HVAL_REAL:
      result->indices[i] = hrange_real_index(&rng->bounds.r, hpoint->val->value.r);
      break;
    case HVAL_STR:
      result->indices[i] = hrange_str_index(&rng->bounds.s, hpoint->val->value.s);
      break;
    default:
      session_error("bad range type in gridpoint_from_hpoint");
      return -1;
    }
  }

  return 0;

 error:
  hpoint_fini(&result->point);
  return -1;
}

int gridpoint_move(struct lgp_info *info,
                   gridpoint_t *origin, int dim, int steps,
                   gridpoint_t *result) {
  if (dim < 0 || dim >= info->sig.range_len) {
    session_error("Bad dimension passed to gridpoint_move.");
    return -1;
  }

  long new_idx = origin->indices[dim] + steps;
  if (new_idx < 0)
    new_idx = 0;
  if (new_idx >= info->max_indices[dim])
    new_idx = info->max_indices[dim] - 1;
  
  int steps_taken = (int) (new_idx - origin->indices[dim]);
  if (steps_taken < 0)
    steps_taken = -steps_taken;

  memcpy(result->indices, origin->indices, info->sig.range_len * sizeof(long));
  result->indices[dim] = new_idx;
  if (hpoint_copy(&result->point, &origin->point) < 0)
    return -1;
  if (point_from_indices(info, result) < 0)
    return -1;

  return steps_taken;
}

int gridpoint_move_multi(struct lgp_info *info,
                         gridpoint_t *origin, int *steps,
                         gridpoint_t *result) {
  if (!steps) {
    session_error("Bad step array passed to gridpoint_move_multi.");
    return -1;
  }

  int idim;
  int steps_taken = 0;
  for (idim = 0; idim < info->sig.range_len; idim++) {
    long new_idx = origin->indices[idim] + steps[idim];
    if (new_idx < 0)
      new_idx = 0;
    if (new_idx >= info->max_indices[idim])
      new_idx = info->max_indices[idim] - 1;
    result->indices[idim] = new_idx;
    
    int dsteps_taken = (int) (new_idx - origin->indices[idim]);
    if (dsteps_taken > 0)
      steps_taken += dsteps_taken;
    else
      steps_taken -= dsteps_taken;
  }

  if (hpoint_copy(&result->point, &origin->point) < 0)
    return -1;
  if (point_from_indices(info, result) < 0)
    return -1;

  return steps_taken;
}

int point_from_indices(struct lgp_info *info, gridpoint_t *gpt) {
  int i;
  for (i = 0; i < info->sig.range_len; i++) {
    hrange_t *rng = info->sig.range + i;
    switch (rng->type) {
    case HVAL_INT:
      gpt->point.val->value.i = hrange_int_value(&rng->bounds.i, gpt->indices[i]);
      break;
    case HVAL_REAL:
      gpt->point.val->value.r = hrange_real_value(&rng->bounds.r, gpt->indices[i]);
      break;
    case HVAL_STR:
      if (gpt->point.memlevel)
        free((char *)gpt->point.val->value.s); // cast silences const warning
      gpt->point.val->value.s = hrange_str_value(&rng->bounds.s, gpt->indices[i]);
      break;
    default:
      session_error("bad range type in point_from_indices");
      return -1;
    }
  }
  gpt->point.memlevel = 0;
  return 0;
}

