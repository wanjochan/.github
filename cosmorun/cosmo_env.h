/* cosmo_env.h - Environment Variable Support for GCC/Clang Compatibility
 *
 * Provides environment variable support for drop-in gcc/clang replacement:
 * - C_INCLUDE_PATH: Additional include directories
 * - CPLUS_INCLUDE_PATH: C++ include directories
 * - LIBRARY_PATH: Additional library directories
 * - LD_LIBRARY_PATH: Runtime library search path
 * - PKG_CONFIG_PATH: pkg-config search path
 * - CFLAGS: Additional compiler flags
 * - LDFLAGS: Additional linker flags
 */

#ifndef COSMO_ENV_H
#define COSMO_ENV_H

#include "libtcc.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Apply individual environment variables */
void cosmo_env_apply_c_include_path(TCCState *s);
void cosmo_env_apply_cplus_include_path(TCCState *s);
void cosmo_env_apply_library_path(TCCState *s);
void cosmo_env_apply_ld_library_path(TCCState *s);
void cosmo_env_apply_pkg_config_path(TCCState *s);
void cosmo_env_apply_cflags(TCCState *s);
void cosmo_env_apply_ldflags(TCCState *s);

/* Apply all environment variables at once */
void cosmo_env_apply_all(TCCState *s);

#ifdef __cplusplus
}
#endif

#endif // COSMO_ENV_H
