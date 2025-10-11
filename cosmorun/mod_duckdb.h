#ifndef COSMORUN_DUCKDB_MINIMAL_H
#define COSMORUN_DUCKDB_MINIMAL_H

#include "cosmo_libc.h"

/* Override DUCKDB_C_API for TCC compatibility */
#ifndef DUCKDB_C_API
#define DUCKDB_C_API
#endif

/* Define DUCKDB_STATIC_BUILD to avoid __declspec on Windows */
#ifndef DUCKDB_STATIC_BUILD
#define DUCKDB_STATIC_BUILD
#endif

#include "duckdb-1.4.1.h"

#endif /* COSMORUN_DUCKDB_MINIMAL_H */
