// Copyright (c) 2021 Davide Paro
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include <log.h>
#include "misc.h"
#include "version.h"
#include <argtable3.h>

int mymain(int l, int R, int k, const char **defines, int ndefines,
           const char *outfile, int v, const char **infiles, int ninfiles) {
    int i;

    if (l > 0)
        printf("list files (-l)\n");
    if (R > 0)
        printf("recurse through directories (-R)\n");
    if (v > 0)
        printf("verbose is enabled (-v)\n");
    printf("scalar k=%d\n", k);
    printf("output is \"%s\"\n", outfile);

    for (i = 0; i < ndefines; i++)
        printf("user defined macro \"%s\"\n", defines[i]);

    for (i = 0; i < ninfiles; i++)
        printf("infile[%d]=\"%s\"\n", i, infiles[i]);

    return 0;
}

int main(int argc, char **argv) {
    const char *progname = argv[0];

    struct arg_lit *list = arg_lit0("lL", NULL, "list files");
    struct arg_lit *recurse =
        arg_lit0("R", NULL, "recurse through subdirectories");
    struct arg_int *repeat =
        arg_int0("k", "scalar", NULL, "define scalar value k (default is 3)");
    struct arg_str *defines =
        arg_strn("D", "define", "MACRO", 0, argc + 2, "macro definitions");
    struct arg_file *outfile =
        arg_file0("o", NULL, "<output>", "output file (default is \"-\")");
    struct arg_lit *verbose =
        arg_lit0("v", "verbose,debug", "verbose messages");
    struct arg_lit *help = arg_lit0(NULL, "help", "print this help and exit");
    struct arg_lit *version =
        arg_lit0(NULL, "version", "print version information and exit");
    struct arg_file *infiles =
        arg_filen(NULL, NULL, NULL, 1, argc + 2, "input file(s)");
    struct arg_end *end = arg_end(256);

    void *argtable[] = {list,    recurse, repeat,  defines, outfile,
                        verbose, help,    version, infiles, end};

    int nerrors;
    int exitcode = 0;

    /* verify the argtable[] entries were allocated sucessfully */
    if (arg_nullcheck(argtable) != 0) {
        /* NULL entries were detected, some allocations must have failed */
        printf("%s: insufficient memory\n", progname);
        exitcode = 1;
        goto exit;
    }

    /* set any command line default values prior to parsing */
    repeat->ival[0] = 3;
    outfile->filename[0] = "-";

    /* Parse the command line as defined by argtable[] */
    nerrors = arg_parse(argc, argv, argtable);

    /* special case: '--help' takes precedence over error reporting */
    if (help->count > 0) {
        printf("Usage: %s", progname);
        arg_print_syntax(stdout, argtable, "\n");
        printf("This program demonstrates the use of the argtable2 library\n");
        printf("for parsing command line arguments.\n");
        arg_print_glossary(stdout, argtable, "  %-25s %s\n");
        exitcode = 0;
        goto exit;
    }

    /* special case: '--version' takes precedence error reporting */
    if (version->count > 0) {
        printf("'%s' example program for the \"argtable\" command line "
               "argument parser.\n",
               progname);

        printf("%s (GIT SHA: %s)\n", GIT_DATE, GIT_SHA1);
        exitcode = 0;
        goto exit;
    }

    /* If the parser returned any errors then display them and exit */
    if (nerrors > 0) {
        /* Display the error details contained in the arg_end struct.*/
        arg_print_errors(stdout, end, progname);
        printf("Try '%s --help' for more information.\n", progname);
        exitcode = 1;
        goto exit;
    }

    /* special case: uname with no command line options induces brief help */
    if (argc == 1) {
        printf("Try '%s --help' for more information.\n", progname);
        exitcode = 0;
        goto exit;
    }

    /* normal case: take the command line options at face value */
    exitcode = mymain(list->count, recurse->count, repeat->ival[0],
                      defines->sval, defines->count, outfile->filename[0],
                      verbose->count, infiles->filename, infiles->count);

exit:
    /* deallocate each non-null entry in argtable[] */
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));

    return exitcode;
}
