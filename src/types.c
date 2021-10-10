#include "types.h"
#include <stdlib.h>
#include <string.h>

int32_t *veci32_copy(int32_t *other, int32_t len) {
    int32_t *result = malloc(len * sizeof(*result));
    memcpy(result, other, len * sizeof(*result));
    return result;
}

int32_t *mati32_copy(int32_t *other, int32_t w, int32_t h) {
    int32_t *result = malloc(w * h * sizeof(*result));
    memcpy(result, other, w * h * sizeof(*result));
    return result;
}
