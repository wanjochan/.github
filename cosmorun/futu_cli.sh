#!/bin/bash
# Futu CLI wrapper script
cd "$(dirname "$0")"
../cosmorun.exe futu_cli.c futu_utils.c "$@"
