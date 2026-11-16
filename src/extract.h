/* The engine reads only loose files in data\. This writes any that are missing, from the original
 * files in game\ (never overwrites a file that is there: edited files stay as they are).
 * The output is byte-identical to tools\loose.py (checked by check.exe). */
#pragma once
#include "base.h"

/* data_dir: where the loose files go (normally <root>\data); game_dir: the original files.
 * Returns how many files were written, or -1 when an original file needed is missing or unreadable. */
int extract_missing(const char *data_dir, const char *game_dir);
/* the part checked against tools\loose.py: textures and levels only */
int extract_core(const char *data_dir, const char *game_dir);

/* the loose files the engine needs (relative to data\); for the checks */
int extract_list(char names[][48], int cap);
