#include "core.h"
#include <stdlib.h>
#include <string.h>

void instance_destroy(Instance *instance) {
    free(instance->positions);
    free(instance->demands);
    free(instance->duals);
    memset(instance, 0, sizeof(*instance));
}
