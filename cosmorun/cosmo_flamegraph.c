/* Flame graph generator implementation
 * Generates Brendan Gregg style flame graph SVG
 */

#include "cosmo_flamegraph.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_STACK_DEPTH 128
#define MAX_NODES 4096
#define FRAME_HEIGHT 16
#define FONT_SIZE 12
#define TEXT_PADDING 4

/* Tree node representing a stack frame */
typedef struct node_t {
    char *name;
    int samples;
    struct node_t **children;
    int child_count;
    int child_capacity;
} node_t;

/* Flamegraph structure */
struct flamegraph_t {
    node_t *root;
    int width;
    int height;
    int total_samples;
};

/* Hash function for consistent color generation */
static unsigned int hash_string(const char *str) {
    unsigned int hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + (unsigned int)c; /* hash * 33 + c */
    }
    return hash;
}

/* Generate color from function name (consistent hashing) */
static void generate_color(const char *name, char *color_buf) {
    unsigned int hash = hash_string(name);

    /* Generate warm colors (yellows, oranges, reds) like Brendan's */
    int r = 200 + (int)(hash % 56);        /* 200-255 */
    int g = 100 + (int)((hash >> 8) % 156); /* 100-255 */
    int b = 0 + (int)((hash >> 16) % 100);  /* 0-99 */

    sprintf(color_buf, "rgb(%d,%d,%d)", r, g, b);
}

/* Create new tree node */
static node_t* node_create(const char *name) {
    node_t *node = (node_t*)calloc(1, sizeof(node_t));
    if (!node) return NULL;

    node->name = strdup(name);
    if (!node->name) {
        free(node);
        return NULL;
    }

    node->samples = 0;
    node->children = NULL;
    node->child_count = 0;
    node->child_capacity = 0;

    return node;
}

/* Find or create child node */
static node_t* node_find_or_create_child(node_t *parent, const char *name) {
    /* Search for existing child */
    for (int i = 0; i < parent->child_count; i++) {
        if (strcmp(parent->children[i]->name, name) == 0) {
            return parent->children[i];
        }
    }

    /* Create new child */
    if (parent->child_count >= parent->child_capacity) {
        int new_capacity = parent->child_capacity == 0 ? 4 : parent->child_capacity * 2;
        node_t **new_children = (node_t**)realloc(parent->children,
                                                   (size_t)new_capacity * sizeof(node_t*));
        if (!new_children) return NULL;

        parent->children = new_children;
        parent->child_capacity = new_capacity;
    }

    node_t *child = node_create(name);
    if (!child) return NULL;

    parent->children[parent->child_count++] = child;
    return child;
}

/* Free tree recursively */
static void node_destroy(node_t *node) {
    if (!node) return;

    for (int i = 0; i < node->child_count; i++) {
        node_destroy(node->children[i]);
    }

    free(node->children);
    free(node->name);
    free(node);
}

/* Comparison function for qsort (alphabetical order) */
static int compare_nodes(const void *a, const void *b) {
    const node_t *na = *(const node_t**)a;
    const node_t *nb = *(const node_t**)b;
    return strcmp(na->name, nb->name);
}

/* Sort children alphabetically */
static void node_sort_children(node_t *node) {
    if (node->child_count > 1) {
        qsort(node->children, (size_t)node->child_count, sizeof(node_t*), compare_nodes);
    }

    /* Recursively sort children's children */
    for (int i = 0; i < node->child_count; i++) {
        node_sort_children(node->children[i]);
    }
}

/* Escape XML special characters */
static void xml_escape(const char *input, char *output, size_t output_size) {
    size_t j = 0;
    for (size_t i = 0; input[i] && j < output_size - 6; i++) {
        switch (input[i]) {
            case '&':
                output[j++] = '&'; output[j++] = 'a'; output[j++] = 'm';
                output[j++] = 'p'; output[j++] = ';';
                break;
            case '<':
                output[j++] = '&'; output[j++] = 'l'; output[j++] = 't';
                output[j++] = ';';
                break;
            case '>':
                output[j++] = '&'; output[j++] = 'g'; output[j++] = 't';
                output[j++] = ';';
                break;
            case '"':
                output[j++] = '&'; output[j++] = 'q'; output[j++] = 'u';
                output[j++] = 'o'; output[j++] = 't'; output[j++] = ';';
                break;
            case '\'':
                output[j++] = '&'; output[j++] = 'a'; output[j++] = 'p';
                output[j++] = 'o'; output[j++] = 's'; output[j++] = ';';
                break;
            default:
                output[j++] = input[i];
                break;
        }
    }
    output[j] = '\0';
}

/* Render node recursively to SVG */
static void render_node(FILE *fp, node_t *node, double x, double width, int depth,
                       int total_samples, int svg_width, int svg_height) {
    if (width < 1.0) return; /* Too small to render */

    double y = (double)((depth + 1) * FRAME_HEIGHT);
    if (y > (double)svg_height) return; /* Beyond visible area */

    /* Generate color */
    char color[32];
    generate_color(node->name, color);

    /* Calculate percentage */
    double pct = (double)node->samples / (double)total_samples * 100.0;

    /* Prepare escaped text */
    char escaped_name[512];
    xml_escape(node->name, escaped_name, sizeof(escaped_name));

    char tooltip[1024];
    snprintf(tooltip, sizeof(tooltip), "%s (%d samples, %.2f%%)",
             escaped_name, node->samples, pct);

    char tooltip_escaped[1024];
    xml_escape(tooltip, tooltip_escaped, sizeof(tooltip_escaped));

    /* Write rectangle */
    fprintf(fp, "  <rect x=\"%.1f\" y=\"%.1f\" width=\"%.1f\" height=\"%d\" "
                "fill=\"%s\" stroke=\"white\" stroke-width=\"0.5\" "
                "onmouseover=\"s(this,'%s')\" onmouseout=\"c(this)\" "
                "onclick=\"z(this)\"/>\n",
            x, y, width, FRAME_HEIGHT, color, tooltip_escaped);

    /* Write text if there's enough space */
    if (width > 20.0) {
        char display_text[256];
        int max_chars = (int)(width / 7); /* Approximate characters that fit */

        if (max_chars > (int)strlen(node->name)) {
            snprintf(display_text, sizeof(display_text), "%s", node->name);
        } else if (max_chars > 3) {
            snprintf(display_text, sizeof(display_text), "%.*s..", max_chars - 2, node->name);
        } else {
            display_text[0] = '\0';
        }

        if (display_text[0]) {
            char display_escaped[256];
            xml_escape(display_text, display_escaped, sizeof(display_escaped));

            fprintf(fp, "  <text x=\"%.1f\" y=\"%.1f\" font-size=\"%d\" "
                        "fill=\"black\" pointer-events=\"none\">%s</text>\n",
                    x + TEXT_PADDING, y + FONT_SIZE + 2, FONT_SIZE, display_escaped);
        }
    }

    /* Render children */
    if (node->child_count > 0) {
        double child_x = x;
        for (int i = 0; i < node->child_count; i++) {
            node_t *child = node->children[i];
            double child_width = width * (double)child->samples / (double)node->samples;

            render_node(fp, child, child_x, child_width, depth + 1,
                       total_samples, svg_width, svg_height);

            child_x += child_width;
        }
    }
}

/* Create flamegraph instance */
flamegraph_t* flamegraph_create(int width, int height) {
    if (width <= 0 || height <= 0) return NULL;

    flamegraph_t *fg = (flamegraph_t*)calloc(1, sizeof(flamegraph_t));
    if (!fg) return NULL;

    fg->root = node_create("all");
    if (!fg->root) {
        free(fg);
        return NULL;
    }

    fg->width = width;
    fg->height = height;
    fg->total_samples = 0;

    return fg;
}

/* Add stack trace */
int flamegraph_add_stack(flamegraph_t *fg, const char **stack, int depth, int count) {
    if (!fg || !stack || depth < 0 || count <= 0) return -1;
    if (depth > MAX_STACK_DEPTH) return -1;

    /* Traverse/build tree from root down through stack */
    node_t *current = fg->root;
    current->samples += count;

    for (int i = 0; i < depth; i++) {
        if (!stack[i]) return -1;

        current = node_find_or_create_child(current, stack[i]);
        if (!current) return -1;

        current->samples += count;
    }

    fg->total_samples += count;
    return 0;
}

/* Generate SVG file */
int flamegraph_generate_svg(flamegraph_t *fg, const char *output_path) {
    if (!fg || !output_path) return -1;
    if (fg->total_samples == 0) return -1;

    /* Sort tree alphabetically */
    node_sort_children(fg->root);

    FILE *fp = fopen(output_path, "w");
    if (!fp) return -1;

    /* Write SVG header */
    fprintf(fp, "<?xml version=\"1.0\" standalone=\"no\"?>\n");
    fprintf(fp, "<!DOCTYPE svg PUBLIC \"-//W3C//DTD SVG 1.1//EN\" "
                "\"http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd\">\n");
    fprintf(fp, "<svg version=\"1.1\" width=\"%d\" height=\"%d\" "
                "xmlns=\"http://www.w3.org/2000/svg\">\n", fg->width, fg->height);

    /* Embed JavaScript for interactivity */
    fprintf(fp, "<defs>\n");
    fprintf(fp, "  <linearGradient id=\"background\" y1=\"0\" y2=\"1\" x1=\"0\" x2=\"0\">\n");
    fprintf(fp, "    <stop stop-color=\"#eeeeee\" offset=\"5%%\"/>\n");
    fprintf(fp, "    <stop stop-color=\"#eeeeb0\" offset=\"95%%\"/>\n");
    fprintf(fp, "  </linearGradient>\n");
    fprintf(fp, "</defs>\n");

    fprintf(fp, "<style type=\"text/css\">\n");
    fprintf(fp, "  text { font-family: Verdana, sans-serif; font-size: %dpx; }\n", FONT_SIZE);
    fprintf(fp, "  rect:hover { stroke: black; stroke-width: 1; }\n");
    fprintf(fp, "</style>\n");

    fprintf(fp, "<script type=\"text/ecmascript\"><![CDATA[\n");
    fprintf(fp, "  var details, svg;\n");
    fprintf(fp, "  function init(evt) { details = document.getElementById('details'); "
                "svg = document.getElementsByTagName('svg')[0]; }\n");
    fprintf(fp, "  function s(node, t) { details.nodeValue = t; }\n");
    fprintf(fp, "  function c(node) { details.nodeValue = ' '; }\n");
    fprintf(fp, "  function z(node) { console.log('Zoom: ' + node); }\n");
    fprintf(fp, "]]></script>\n");

    /* Background */
    fprintf(fp, "<rect x=\"0\" y=\"0\" width=\"%d\" height=\"%d\" fill=\"url(#background)\"/>\n",
            fg->width, fg->height);

    /* Title */
    fprintf(fp, "<text x=\"%d\" y=\"24\" font-size=\"17\" fill=\"black\" font-weight=\"bold\">"
                "Flame Graph</text>\n", fg->width / 2 - 50);

    /* Details text (updated on hover) */
    fprintf(fp, "<text x=\"10\" y=\"%d\" font-size=\"12\" fill=\"black\" id=\"details\"> </text>\n",
            fg->height - 10);

    /* Render flame graph */
    if (fg->root->child_count > 0) {
        for (int i = 0; i < fg->root->child_count; i++) {
            node_t *child = fg->root->children[i];
            double child_width = (double)fg->width * (double)child->samples / (double)fg->total_samples;
            double child_x = 0;

            /* Calculate x position based on previous siblings */
            for (int j = 0; j < i; j++) {
                child_x += (double)fg->width * (double)fg->root->children[j]->samples /
                          (double)fg->total_samples;
            }

            render_node(fp, child, child_x, child_width, 0,
                       fg->total_samples, fg->width, fg->height);
        }
    }

    /* Close SVG */
    fprintf(fp, "</svg>\n");

    fclose(fp);
    return 0;
}

/* Destroy flamegraph */
void flamegraph_destroy(flamegraph_t *fg) {
    if (!fg) return;

    node_destroy(fg->root);
    free(fg);
}
