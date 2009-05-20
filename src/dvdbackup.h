/* -*- Mode: C; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Copyright (C) 2002  Olaf Beck <olaf_sc@yahoo.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/* dvdbackup version 0.1 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/errno.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <sysexits.h>
#include <dvdread/dvd_reader.h>
#include <dvdread/ifo_read.h>
#include <dvdread/ifo_print.h>
#include <dvdread/dvd_udf.h>

#define MAXNAME 256

/* Buffer size for reading DVD stuff */
#define READ_BUF_SIZE (1024 * 1024 * 4)
#define READ_BUF_SIZE_IN_BLOCKS (READ_BUF_SIZE / DVD_VIDEO_LB_LEN)

/* Flag for verbose mode */
int verbose;
int aspect;

/* Structs to keep title set information in */

typedef struct {
    int size_ifo;
    int size_menu;
    int size_bup;
    int number_of_vob_files;
    int size_vob[10];
} title_set_t;

typedef struct {
    int          number_of_title_sets;
    title_set_t *title_set;
} title_set_info_t;

typedef struct {
    int title;
    int title_set;
    int vts_title;
    int chapters;
    int aspect_ratio;
    int angles;
    int audio_tracks;
    int audio_channels;
    int sub_pictures;
} titles_t;

typedef struct {
    int       main_title_set;
    int       number_of_titles;
    titles_t *titles;
} titles_info_t;

void bsort_max_to_min(int sector[], int title[], int size);

void usage() __attribute__ ((noreturn));
