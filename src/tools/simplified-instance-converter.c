#include <argtable3.h>
#include "core.h"
#include "parser.h"

static void print_usage(FILE *fh, char *progname) {
    fprintf(fh, "%s [INPUT-TEST-INSTANCE] [OUTPUT-TEST-INSTANCE]\n", progname);
    exit(EXIT_FAILURE);
}

int main(int argc, char **argv) {
    if (argc != 3) {
        print_usage(stderr, argv[0]);
        exit(EXIT_FAILURE);
    }

    char *input = argv[1];
    char *output = argv[2];

    Instance instance = parse(input);
    if (!is_valid_instance(&instance)) {
        fprintf(stderr, "%s: failed to parse\n", input);
        exit(EXIT_FAILURE);
    }

    FILE *fh = fopen(output, "w");
    if (!fh) {
        fprintf(stderr, "%s: failed to open file for writing\n", output);
        instance_destroy(&instance);
        exit(EXIT_FAILURE);
    }

    fprintf(fh, "NAME : %s\n", instance.name);
    fprintf(fh, "COMMENT : %s\n", instance.comment ? instance.comment : "");
    fprintf(fh, "TYPE : %s\n", "CVRP");
    fprintf(fh, "DIMENSION : %d\n", instance.num_customers + 1);
    fprintf(fh, "VEHICLES : %d\n", instance.num_vehicles);
    fprintf(fh, "CAPACITY : %g\n", instance.vehicle_cap);
    fprintf(fh, "EDGE_WEIGHT_TYPE : %s\n", "EUC_2D");

    // Generate node coordinate section
    fprintf(fh, "NODE_COORD_SECTION\n");

    for (int32_t i = 0; i < instance.num_customers + 1; i++) {
        fprintf(fh, "%d %g %g\n", i + 1, instance.positions[i].x,
                instance.positions[i].y);
    }

    // Generate demand section
    fprintf(fh, "DEMAND_SECTION\n");
    for (int32_t i = 0; i < instance.num_customers + 1; i++) {
        fprintf(fh, "%d %g\n", i + 1, instance.demands[i]);
    }

    // Generate depot section
    fprintf(fh, "DEPOT_SECTION\n");
    fprintf(fh, "%d\n", 1);
    fprintf(fh, "%d\n", -1);

    // Generate profit section
    fprintf(fh, "PROFIT_SECTION\n");
    for (int32_t i = 0; i < instance.num_customers + 1; i++) {
        fprintf(fh, "%d %.17g\n", i + 1, instance.duals[i]);
    }

    instance_destroy(&instance);
    return EXIT_SUCCESS;
}
