/*
 * Copyright (c) 2021 Davide Paro
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "render.h"
#include "core-utils.h"
#include <stdio.h>

static void compute_plotting_region(const Instance *instance, double *llx,
                                    double *lly, double *w, double *h) {

    int32_t n = instance->num_customers + 1;

    double min_x = 9999999, max_x = -9999999;
    double min_y = 99999999, max_y = -9999999;

    for (int32_t i = 0; i < n; i++) {
        const Vec2d *pos = &instance->positions[i];

        min_x = MIN(pos->x, min_x);
        max_x = MAX(pos->x, max_x);

        min_y = MIN(pos->x, min_y);
        max_y = MAX(pos->x, max_y);
    }

    *llx = min_x;
    *lly = min_y;
    *w = max_x - min_x;
    *h = max_y - min_y;
}

const char *guess_plot_filext(const char *filepath, const char *filext) {
    if (filext == NULL) {
        for (int32_t i = strlen(filepath) - 1; i >= 0; i--) {
            if (filepath[i] == '/' || filepath[i] == '\\') {
                break;
            } else if (filepath[i] == '.') {
                filext = filepath + i + 1;
                break;
            }
        }
    }

    // Default the file extension to pdf if none is specified and if we cannot
    // autodetect
    filext = filext != NULL ? filext : "pdf";
    return filext;
}

bool render_tour_image(const char *filepath, const Instance *instance,
                       Tour *tour, const char *filext) {

    // Try to autodetect the filepath
    filext = guess_plot_filext(filepath, filext);

    int32_t n = instance->num_customers + 1;
    bool result = true;

    double min_dist = 9999999999;
    double smaller_coord = 9999999999;
    double llx, lly, w, h;

    compute_plotting_region(instance, &llx, &lly, &w, &h);

    for (int32_t i = 0; i < n; i++) {
        for (int32_t j = 0; j < n; j++) {
            if (i != j) {
                const Vec2d *ipos = &instance->positions[i];
                const Vec2d *jpos = &instance->positions[j];

                smaller_coord = MIN(smaller_coord, ipos->x);
                smaller_coord = MIN(smaller_coord, ipos->y);
                smaller_coord = MIN(smaller_coord, jpos->x);
                smaller_coord = MIN(smaller_coord, jpos->y);

                double d = vec2d_dist(ipos, jpos);
                min_dist = MIN(min_dist, d);
            }
        }
    }

    min_dist = sqrt(min_dist) / MIN(w, h);

    // min_dist = exp(min_dist);

    char command[8192] = "";

    snprintf(command, ARRAY_LEN(command),
             "neato '-T%s' -o '%s' 1> /dev/null 2>/dev/null", filext, filepath);
    command[ARRAY_LEN(command) - 1] = '\0';

    log_info("%s :: Running popen(%s)\n", __func__, command);

    FILE *gp = popen(command, "w");
    if (!gp) {
        result = false;
        goto terminate;
    }

    fprintf(gp, "strict digraph {\n");
    fprintf(gp, "    labelloc=\"t\"\n");
    fprintf(gp, "    graph [pad=\"0.212, 0.055\", bgcolor=white, fontname = "
                "\"monospace\"]\n");
    fprintf(gp, "    node [style=filled]\n");

    //
    // Declare the nodes and style them. Apply some size normalization for the
    // output render target.
    //
    for (int32_t i = 0; i < n; i++) {
        const Vec2d *pos = &instance->positions[i];

        fprintf(gp,
                "    %d [fillcolor=\"#dddddd\" pos=\"%f,%f!\" pin=\"true\" "
                "shape=\"circle\" label=\"%d\"]\n",
                i, ((pos->x - llx) / w / min_dist),
                ((pos->y - lly) / h / min_dist), i);
    }

    assert(tour->num_comps == 1);

    for (int32_t comp = 0; comp < tour->num_comps; comp++) {
        int32_t first_node_in_comp = -1;
        for (int32_t i = 0; i < n; i++) {
            if (*tcomp(tour, i) == comp) {
                first_node_in_comp = i;
                break;
            }
        }

        assert(first_node_in_comp == 0);

        if (first_node_in_comp != -1) {

            int32_t curr_vertex = first_node_in_comp;
            int32_t next_vertex = curr_vertex;

            while ((next_vertex = *tsucc(tour, curr_vertex)) !=
                   first_node_in_comp) {
                fprintf(gp, "   %d -> %d [fontsize=\"8\"]\n", curr_vertex,
                        next_vertex);
                curr_vertex = next_vertex;
            }
            assert(next_vertex == first_node_in_comp);
            fprintf(gp, "   %d -> %d [fontsize=\"8\"]\n", curr_vertex,
                    first_node_in_comp);
        }
    }

    fprintf(gp, "}\n");

terminate:
    if (gp) {
        int exit_code = pclose(gp);
        log_info("%s :: graphviz dump creation process terminated with "
                 "exit code %d",
                 __func__, exit_code);
        result = exit_code == 0;
    }

    return result;
}

void render_instance_into_vrplib_file(FILE *fh, const Instance *instance,
                                      bool dump_profit_section) {
    const int32_t n = instance->num_customers + 1;

    bool has_name = instance->name && strlen(instance->name) > 0;
    fprintf(fh, "NAME : %s\n",
            has_name ? instance->name : "VRP unnamed instance");

    if (instance->comment && strlen(instance->comment) > 0) {
        fprintf(fh, "COMMENT : %s\n",
                instance->comment ? instance->comment : "");
    }

    fprintf(fh, "TYPE : %s\n", "CVRP");
    fprintf(fh, "DIMENSION : %d\n", instance->num_customers + 1);
    fprintf(fh, "VEHICLES : %d\n", instance->num_vehicles);
    fprintf(fh, "CAPACITY : %f\n", instance->vehicle_cap);

    if (!instance->edge_weight) {
        fprintf(fh, "EDGE_WEIGHT_FORMAT : %s\n", "FUNCTION");
        fprintf(fh, "EDGE_WEIGHT_TYPE : %s\n", "EUC_2D");
    } else {
        fprintf(fh, "EDGE_WEIGHT_FORMAT : %s\n", "UPPER_ROW");
        fprintf(fh, "EDGE_WEIGHT_TYPE : %s\n", "EXPLICIT");
    }

    // Generate node coordinate section
    fprintf(fh, "NODE_COORD_SECTION\n");

    for (int32_t i = 0; i < instance->num_customers + 1; i++) {
        fprintf(fh, "%d %g %g\n", i + 1, instance->positions[i].x,
                instance->positions[i].y);
    }

    // Generate demand section
    fprintf(fh, "DEMAND_SECTION\n");
    for (int32_t i = 0; i < instance->num_customers + 1; i++) {
        fprintf(fh, "%d %f\n", i + 1, instance->demands[i]);
    }

    if (instance->edge_weight) {
        // Generate edge weight section
        for (int32_t i = 0; i < n; i++) {
            for (int32_t j = i + 1; j < n; j++) {
                fprintf(fh, "%d %d %.17f\n", i, j,
                        instance->edge_weight[sxpos(n, i, j)]);
            }
        }
    }

    if (dump_profit_section) {
        // Generate profit section
        fprintf(fh, "PROFIT_SECTION\n");
        for (int32_t i = 0; i < instance->num_customers + 1; i++) {
            fprintf(fh, "%d %.17g\n", i + 1, instance->profits[i]);
        }
    }

    // Generate depot section
    fprintf(fh, "DEPOT_SECTION\n");
    fprintf(fh, "%d\n", 1);
    fprintf(fh, "%d\n", -1);
    fprintf(fh, "EOF");
}
