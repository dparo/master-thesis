#include <argtable3.h>
#include "core.h"
#include "parser.h"

static void print_usage(FILE *fh, char *progname) {
    fprintf(fh, "%s [INPUT-TEST-INSTANCE] [OUTPUT-TEST-INSTANCE]\n", progname);
    exit(EXIT_FAILURE);
}

int main(int argc, char **argv) {
    if (argc != 2) {
        print_usage(stderr, argv[0]);
        exit(EXIT_FAILURE);
    }

    char *input = argv[1];
    char *output = argv[2];

    Instance instance = parse_test_instance(input);
    return EXIT_SUCCESS;
}
