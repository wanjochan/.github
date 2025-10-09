/* Minimal CRTI for TCC self-compilation */

/* This file provides the initialization part of the C runtime */

/* Constructor initialization - called before main() */
void _init(void) {
    /* Minimal initialization - nothing needed for TCC self-compilation */
}

/* Destructor initialization - called after main() */
void _fini(void) {
    /* Minimal finalization - nothing needed for TCC self-compilation */
}
