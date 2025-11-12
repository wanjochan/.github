/* Flame graph generator (Brendan Gregg format)
 * Generates interactive SVG visualization of stack traces
 * X-axis: Alphabetical ordering (NOT time!)
 * Y-axis: Stack depth
 * Width: Proportional to sample count
 */

#ifndef COSMO_FLAMEGRAPH_H
#define COSMO_FLAMEGRAPH_H

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque flamegraph structure */
typedef struct flamegraph_t flamegraph_t;

/* Create new flamegraph with specified dimensions
 * @param width SVG width in pixels
 * @param height SVG height in pixels
 * @return New flamegraph instance or NULL on failure
 */
flamegraph_t* flamegraph_create(int width, int height);

/* Add stack trace to flamegraph
 * @param fg Flamegraph instance
 * @param stack Array of function names from bottom to top
 * @param depth Number of frames in stack
 * @param count Number of samples for this stack trace
 * @return 0 on success, -1 on failure
 */
int flamegraph_add_stack(flamegraph_t *fg, const char **stack, int depth, int count);

/* Generate SVG output
 * @param fg Flamegraph instance
 * @param output_path Output SVG file path
 * @return 0 on success, -1 on failure
 */
int flamegraph_generate_svg(flamegraph_t *fg, const char *output_path);

/* Free flamegraph resources
 * @param fg Flamegraph instance
 */
void flamegraph_destroy(flamegraph_t *fg);

#ifdef __cplusplus
}
#endif

#endif /* COSMO_FLAMEGRAPH_H */
