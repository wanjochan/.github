/* Minimal CRT1 for TCC self-compilation */

/* External symbols that will be provided by the program */
extern int main(int argc, char **argv, char **envp);
extern void exit(int status);

/* Global variables that some programs expect */
int __argc;
char **__argv;
char **environ;

/* Minimal _start function */
void _start(void) {
    /* In a real CRT, this would:
     * 1. Set up the stack
     * 2. Parse argc/argv from the stack
     * 3. Set up environment
     * 4. Call main()
     * 5. Call exit() with main's return value
     * 
     * For TCC self-compilation, we can use a simplified version
     * since TCC doesn't rely heavily on command-line parsing during compilation
     */
    
    /* Set up minimal environment */
    __argc = 0;
    __argv = 0;
    environ = 0;
    
    /* Call main with minimal arguments */
    int result = main(0, 0, 0);
    
    /* Exit with the result */
    exit(result);
}
