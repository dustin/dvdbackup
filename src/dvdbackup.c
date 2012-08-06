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

#include "dvdbackup.h"

void usage()
{
    fprintf(stderr,
            "\nUsage: dvdbackup [options]\n"
            "\t-i device\twhere device is your dvd device\n"
            "\t-v X\t\twhere X is the amount of verbosity\n"
            "\t-I\t\tfor information about the DVD\n"
            "\t-o directory\twhere directory is your backup target\n"
            "\t-n dvd_title\tDVD title "
            "(in case it is not automatically determined)\n"
            "\t-M\t\tbackup the whole DVD\n"
            "\t-F\t\tbackup the main feature of the DVD\n"
            "\t-T X\t\tbackup title set X\n"
            "\t-t X\t\tbackup title X\n"
            "\t-s X\t\tbackup from chapter X\n"
            "\t-e X\t\tbackup to chapter X\n"
            "\t-a 0\t\tto get aspect ratio 4:3 "
            "instead of 16:9 if both are present\n"
            "\t-h\t\tprint a brief usage message\n"
            "\t-?\t\tprint a brief usage message\n\n"
            "\t-i is mandatory\n"
            "\t-o is mandatory except if you use -I\n"
            "\t-a is option to the -F switch and "
            "has no effect on other options\n"
            "\t-s and -e should preferably be used together with -t\n\n");
    exit(EX_USAGE);
}

int
CheckSizeArray (const int size_array[], int reference, int target)
{
    /* If true:
       We have a dual DVD with two feature films - now lets see if
       they have the same amount of chapters */
    return ((size_array[target] != 0)
            && ((size_array[reference]/size_array[target]) == 1)
            && ((size_array[reference] * 2 - size_array[target])
                / (size_array[target]) == 1)
            && ((size_array[reference] % size_array[target] * 3)
                < size_array[reference]));
}

int
CheckAudioSubChannels (int audio_audio_array[], int title_set_audio_array[],
                       int subpicture_sub_array[], int title_set_sub_array[],
                       int channels_channel_array[],
                       int title_set_channel_array[],
                       int reference, int candidate, int title_sets)
{
    int temp, i, found_audio, found_sub, found_channels;

    found_audio=0;
    temp = audio_audio_array[reference];
    for (i=0 ; i < title_sets ; i++ ) {
        if ( audio_audio_array[i] < temp ) {
            break;
        }
        if ( candidate == title_set_audio_array[i] ) {
            found_audio=1;
            break;
        }

    }

    found_sub=0;
    temp = subpicture_sub_array[reference];
    for (i=0 ; i < title_sets ; i++ ) {
        if ( subpicture_sub_array[i] < temp ) {
            break;
        }
        if ( candidate == title_set_sub_array[i] ) {
            found_sub=1;
            break;
        }

    }

    found_channels=0;
    temp = channels_channel_array[reference];
    for (i=0 ; i < title_sets ; i++ ) {
        if ( channels_channel_array[i] < temp ) {
            break;
        }
        if ( candidate == title_set_channel_array[i] ) {
            found_channels=1;
            break;
        }

    }

    return(found_audio + found_sub + found_channels);
}

int
DVDWriteCells (dvd_reader_t *dvd,
               int cell_start_sector[], int cell_end_sector[],
               int length, int titles,
               title_set_info_t *title_set_info,
               titles_info_t *titles_info,
               char *targetdir, char *title_name)
{
    /* Loop variables */
    int i, f;

    /* Vob control */
    int vob;

    /* Temp filename,dirname */
    char targetname[PATH_MAX];

    /* Write buffer */

    unsigned char *buffer = NULL;

    /* File Handler */
    int streamout;

    int size;
    int left;
    int leftover;

    /* Buffer size in DVD sectors */
    int buff = READ_BUF_SIZE_IN_BLOCKS;
    int tsize;

    /* Offsets */
    int soffset;
    int offset;

    /* DVD handler */
    dvd_file_t *dvd_file = NULL;

    int title_set;
    int number_of_vob_files;

#ifdef DEBUG
    fprintf(stderr,"DVDWriteCells: length is %d\n", length);
#endif

    title_set = titles_info->titles[titles - 1].title_set;
    number_of_vob_files = title_set_info->title_set[title_set].number_of_vob_files;
#ifdef DEBUG
    fprintf(stderr,"DVDWriteCells: title set is %d\n", title_set);
    fprintf(stderr,"DVDWriteCells: vob files are %d\n", number_of_vob_files);
#endif

    /* Remove all old files silently if they exists */

    for ( i = 0 ; i < 10 ; i++ ) {
        sprintf(targetname,"%s/%s/VIDEO_TS/VTS_%02i_%i.VOB",targetdir,
                title_name, title_set, i + 1);
#ifdef DEBUG
        fprintf(stderr,"DVDWriteCells: file is %s\n", targetname);
#endif
        unlink( targetname);

    }

    /* Loop through all sectors and find the right vob */
    for (f = 0; f < length ; f++) {

        soffset=0;
        offset=0;

        /* Now figure which vob we will use and write to that vob and
           if there is a left over write to the next vob*/

        for ( i = 0; i < number_of_vob_files ; i++ ) {

            tsize = title_set_info->title_set[title_set].size_vob[i];

            if (tsize%2048 != 0) {
                fprintf(stderr, "The Title VOB number %d of title set %d "
                        "doesn't have a valid DVD size\n", i + 1, title_set);
                return(1);
            } else {
                soffset = offset;
                offset = offset + tsize/2048;
            }
#ifdef DEBUG
            fprintf(stderr,"DVDWriteCells: soffset is %d\n", soffset);
            fprintf(stderr,"DVDWriteCells: offset is %d\n", offset);
#endif
            /* Find out if this is the right vob */
            if( soffset <= cell_start_sector[f]  && offset > cell_start_sector[f] ) {
#ifdef DEBUG
                fprintf(stderr,"DVDWriteCells: got it \n");
#endif
                leftover=0;
                soffset = cell_start_sector[f];
                if ( cell_end_sector[f] > offset ) {
                    leftover = cell_end_sector[f] - offset + 1;
                } else {
                    size = cell_end_sector[f] - cell_start_sector[f] + 1;
                }
#ifdef DEBUG
                fprintf(stderr,"DVDWriteCells: size is %d\n", size);
#endif
                vob = i + 1;
                break;
            }

        }
#ifdef DEBUG
        fprintf(stderr,
                "DVDWriteCells: Writing soffset is %d and leftover is %d\n",
                soffset, leftover);
#endif

        /* Create VTS_XX_X.VOB */
        if (title_set == 0) {
            fprintf(stderr,
                    "Don't try to copy chapters from the VMG domain there aren't any\n");
            return(1);
        } else {
            sprintf(targetname,"%s/%s/VIDEO_TS/VTS_%02i_%i.VOB",
                    targetdir, title_name, title_set, vob);
        }

#ifdef DEBUG
        fprintf(stderr,"DVDWriteCells: 1\n");
#endif

        if ((buffer = (unsigned char *)malloc(buff * 2048 * sizeof(unsigned char))) == NULL) {
            fprintf(stderr, "Out of memory coping %s\n", targetname);
            return(1);
        }

#ifdef DEBUG
        fprintf(stderr,"DVDWriteCells: 2\n");
#endif

        if ((streamout = open(targetname, O_WRONLY | O_CREAT | O_APPEND, 0644)) == -1) {
            fprintf(stderr, "Error creating %s\n", targetname);
            perror("");
            return(1);
        }

#ifdef DEBUG
        fprintf(stderr,"DVDWriteCells: 3\n");
#endif

        if ((dvd_file = DVDOpenFile(dvd, title_set, DVD_READ_TITLE_VOBS))== 0) {
            fprintf(stderr, "Failed opending TITLE VOB\n");
            free(buffer);
            close(streamout);
            return(1);
        }

        left = size;

#ifdef DEBUG
        fprintf(stderr,"DVDWriteCells: left is %d\n", left);
#endif

        while( left > 0 ) {

            if (buff > left) {
                buff = left;
            }
            if ( DVDReadBlocks(dvd_file,soffset,buff, buffer) != buff) {
                fprintf(stderr, "Error reading MENU VOB\n");
                free(buffer);
                DVDCloseFile(dvd_file);
                close(streamout);
                return(1);
            }

            if (write(streamout,buffer, buff * 2048) != buff * 2048) {
                fprintf(stderr, "Error writing TITLE VOB\n");
                free(buffer);
                close(streamout);
                return(1);
            }

            soffset = soffset + buff;
            left = left - buff;

        }

        DVDCloseFile(dvd_file);
        free(buffer);
        close(streamout);

        if ( leftover != 0 ) {

            vob = vob + 1;

            if (title_set == 0) {
                fprintf(stderr,
                        "Don't try to copy chapters from the VMG domain there aren't any\n");
                return(1);
            } else {
                sprintf(targetname,"%s/%s/VIDEO_TS/VTS_%02i_%i.VOB",
                        targetdir, title_name, title_set, vob);
            }

            if ((buffer = (unsigned char *)malloc(buff * 2048
                                                  * sizeof(unsigned char))) == NULL) {
                fprintf(stderr, "Out of memory coping %s\n", targetname);
                close(streamout);
                return(1);
            }

            if ((streamout = open(targetname, O_WRONLY | O_CREAT | O_APPEND, 0644)) == -1) {
                fprintf(stderr, "Error creating %s\n", targetname);
                perror("");
                return(1);
            }

            if ((dvd_file = DVDOpenFile(dvd, title_set, DVD_READ_TITLE_VOBS))== 0) {
                fprintf(stderr, "Failed opending TITLE VOB\n");
                free(buffer);
                close(streamout);
                return(1);
            }

            left = leftover;

            while( left > 0 ) {

                if (buff > left) {
                    buff = left;
                }
                if ( DVDReadBlocks(dvd_file,offset,buff, buffer) != buff) {
                    fprintf(stderr, "Error reading MENU VOB\n");
                    free(buffer);
                    DVDCloseFile(dvd_file);
                    close(streamout);
                    return(1);
                }

                if (write(streamout,buffer, buff * 2048) != buff * 2048) {
                    fprintf(stderr, "Error writing TITLE VOB\n");
                    free(buffer);
                    close(streamout);
                    return(1);
                }

                offset = offset + buff;
                left = left - buff;

            }

            DVDCloseFile(dvd_file);
            free(buffer);
            close(streamout);
        }

    }
    return(0);
}

void
FreeSortArrays (int chapter_chapter_array[], int title_set_chapter_array[],
                int angle_angle_array[], int title_set_angle_array[],
                int subpicture_sub_array[], int title_set_sub_array[],
                int audio_audio_array[], int title_set_audio_array[],
                int size_size_array[], int title_set_size_array[],
                int channels_channel_array[], int title_set_channel_array[])
{
    free(chapter_chapter_array);
    free(title_set_chapter_array);

    free(angle_angle_array);
    free(title_set_angle_array);

    free(subpicture_sub_array);
    free(title_set_sub_array);

    free(audio_audio_array);
    free(title_set_audio_array);

    free(size_size_array);
    free(title_set_size_array);

    free(channels_channel_array);
    free(title_set_channel_array);
}

titles_info_t*
DVDGetInfo (dvd_reader_t *_dvd)
{
    /* title interation */
    int counter, i, f;

    /* Our guess */
    int candidate;
    int multi = 0;
    int dual  = 0;

    int titles;
    int title_sets;

    /* Arrays for chapter, angle, subpicture, audio, size, aspect,
       channels - file_set relationship */

    /* Size == number_of_titles */
    int *chapter_chapter_array;
    int *title_set_chapter_array;

    int *angle_angle_array;
    int *title_set_angle_array;

    /* Size == number_of_title_sets */

    int *subpicture_sub_array;
    int *title_set_sub_array;

    int *audio_audio_array;
    int *title_set_audio_array;

    int *size_size_array;
    int *title_set_size_array;

    int *channels_channel_array;
    int *title_set_channel_array;

    /* Temp helpers */
    int channels;
    int found;
    int chapters_1;
    int chapters_2;
    int found_chapter;
    int number_of_multi;

    /*DVD handlers*/
    ifo_handle_t  *vmg_ifo        = NULL;
    dvd_file_t    *vts_title_file = NULL;
    titles_info_t *titles_info    = NULL;

    /*  Open main info file */
    vmg_ifo = ifoOpen( _dvd, 0 );
    if( !vmg_ifo ) {
        fprintf( stderr, "Can't open VMG info.\n" );
        return (0);
    }

    titles = vmg_ifo->tt_srpt->nr_of_srpts;
    title_sets = vmg_ifo->vmgi_mat->vmg_nr_of_title_sets;

    if ((vmg_ifo->tt_srpt == 0) || (vmg_ifo->vts_atrt == 0)) {
        ifoClose(vmg_ifo);
        return(0);
    }

    /* Todo fix malloc check */
    titles_info = ( titles_info_t *)malloc(sizeof(titles_info_t));
    titles_info->titles = (titles_t *)malloc((titles)* sizeof(titles_t));

    titles_info->number_of_titles = titles;

    chapter_chapter_array = malloc(titles * sizeof(int));
    title_set_chapter_array = malloc(titles * sizeof(int));

    /*currently not used in the guessing */
    angle_angle_array = malloc(titles * sizeof(int));
    title_set_angle_array = malloc(titles * sizeof(int));

    subpicture_sub_array = malloc(title_sets * sizeof(int));
    title_set_sub_array = malloc(title_sets * sizeof(int));

    audio_audio_array = malloc(title_sets * sizeof(int));
    title_set_audio_array = malloc(title_sets * sizeof(int));

    size_size_array = malloc(title_sets * sizeof(int));
    title_set_size_array = malloc(title_sets * sizeof(int));

    channels_channel_array = malloc(title_sets * sizeof(int));
    title_set_channel_array = malloc(title_sets * sizeof(int));

    /* Interate over the titles nr_of_srpts */

    for (counter=0; counter < titles; counter++ )  {
        /* For titles_info */
        titles_info->titles[counter].title = counter + 1;
        titles_info->titles[counter].title_set = vmg_ifo->tt_srpt->title[counter].title_set_nr;
        titles_info->titles[counter].vts_title = vmg_ifo->tt_srpt->title[counter].vts_ttn;
        titles_info->titles[counter].chapters = vmg_ifo->tt_srpt->title[counter].nr_of_ptts;
        titles_info->titles[counter].angles = vmg_ifo->tt_srpt->title[counter].nr_of_angles;

        /* For main title*/
        chapter_chapter_array[counter] = vmg_ifo->tt_srpt->title[counter].nr_of_ptts;
        title_set_chapter_array[counter] = vmg_ifo->tt_srpt->title[counter].title_set_nr;
        angle_angle_array[counter] = vmg_ifo->tt_srpt->title[counter].nr_of_angles;
        title_set_angle_array[counter] = vmg_ifo->tt_srpt->title[counter].title_set_nr;
    }

    /* Interate over vmg_nr_of_title_sets */

    for (counter=0; counter < title_sets ; counter++ )  {

        /* Picture*/
        subpicture_sub_array[counter] =
            vmg_ifo->vts_atrt->vts[counter].nr_of_vtstt_subp_streams;
        title_set_sub_array[counter] = counter + 1;

        /* Audio */
        audio_audio_array[counter] = vmg_ifo->vts_atrt->vts[counter].nr_of_vtstt_audio_streams;
        title_set_audio_array[counter] = counter + 1;

        channels=0;
        for  (i=0; i < audio_audio_array[counter]; i++) {
            if ( channels < vmg_ifo->vts_atrt->vts[counter].vtstt_audio_attr[i].channels + 1) {
                channels = vmg_ifo->vts_atrt->vts[counter].vtstt_audio_attr[i].channels + 1;
            }

        }
        channels_channel_array[counter] = channels;
        title_set_channel_array[counter] = counter + 1;

        /* For tiles_info */
        for (f=0; f < titles_info->number_of_titles ; f++ ) {
            if ( titles_info->titles[f].title_set == counter + 1 ) {
                titles_info->titles[f].aspect_ratio =
                    vmg_ifo->vts_atrt->vts[counter].vtstt_vobs_video_attr.display_aspect_ratio;
                titles_info->titles[f].sub_pictures =
                    vmg_ifo->vts_atrt->vts[counter].nr_of_vtstt_subp_streams;
                titles_info->titles[f].audio_tracks =
                    vmg_ifo->vts_atrt->vts[counter].nr_of_vtstt_audio_streams;
                titles_info->titles[f].audio_channels = channels;
            }
        }

    }

    for (counter=0; counter < title_sets; counter++ ) {

        vts_title_file = DVDOpenFile(_dvd, counter + 1, DVD_READ_TITLE_VOBS);

        if (vts_title_file  != 0) {
            size_size_array[counter] = DVDFileSize(vts_title_file);
            DVDCloseFile(vts_title_file);
        } else {
            size_size_array[counter] = 0;
        }

        title_set_size_array[counter] = counter + 1;
    }

    /* Sort all arrays max to min */

    bsort_max_to_min(chapter_chapter_array, title_set_chapter_array, titles);
    bsort_max_to_min(angle_angle_array, title_set_angle_array, titles);
    bsort_max_to_min(subpicture_sub_array, title_set_sub_array, title_sets);
    bsort_max_to_min(audio_audio_array, title_set_audio_array, title_sets);
    bsort_max_to_min(size_size_array, title_set_size_array, title_sets);
    bsort_max_to_min(channels_channel_array, title_set_channel_array, title_sets);

    /* Check if the second biggest one actually can be a feature title
       Here we will take do biggest/second and if that is bigger than
       one it's not a feature title.

       Now this is simply not enough since we have to check that the
       diff between the two of them is small enough to consider the
       second one a feature title we are doing two checks (biggest +
       biggest - second) /second == 1 and biggest%second * 3 <
       biggest */

    if (CheckSizeArray(size_size_array, 0, 1)) {
        /* We have a dual DVD with two feature films - now lets see if
           they have the same amount of chapters*/

        chapters_1 = 0;
        for (i=0 ; i < titles ; i++ ) {
            if (titles_info->titles[i].title_set == title_set_size_array[0] ) {
                if ( chapters_1 < titles_info->titles[i].chapters){
                    chapters_1 = titles_info->titles[i].chapters;
                }
            }
        }

        chapters_2 = 0;
        for (i=0 ; i < titles ; i++ ) {
            if (titles_info->titles[i].title_set == title_set_size_array[1] ) {
                if ( chapters_2 < titles_info->titles[i].chapters){
                    chapters_2 = titles_info->titles[i].chapters;
                }
            }
        }

        if (  vmg_ifo->vts_atrt->vts[title_set_size_array[0] - 1].vtstt_vobs_video_attr.display_aspect_ratio ==
              vmg_ifo->vts_atrt->vts[title_set_size_array[1] - 1].vtstt_vobs_video_attr.display_aspect_ratio) {
            /* In this case it's most likely so that we have a dual
               film but with different context.  They are with in the
               same size range and have the same aspect ratio I would
               guess that such a case is e.g. a DVD containing several
               episodes of a TV series */
            candidate = title_set_size_array[0];
            multi = 1;
        } else if ( chapters_1 == chapters_2  && vmg_ifo->vts_atrt->vts[title_set_size_array[0] - 1].vtstt_vobs_video_attr.display_aspect_ratio !=
                    vmg_ifo->vts_atrt->vts[title_set_size_array[1] - 1].vtstt_vobs_video_attr.display_aspect_ratio){
            /* In this case we have (guess only) the same context -
               they have the same number of chapters but different
               aspect ratio and are in the same size range*/
            if ( vmg_ifo->vts_atrt->vts[title_set_size_array[0] - 1].vtstt_vobs_video_attr.display_aspect_ratio == aspect) {
                candidate = title_set_size_array[0];
            } else if ( vmg_ifo->vts_atrt->vts[title_set_size_array[1] - 1].vtstt_vobs_video_attr.display_aspect_ratio == aspect) {
                candidate = title_set_size_array[1];
            } else {
                /* Okay we didn't have the prefered aspect ratio -
                   just make the biggest one a candidate */
                /* please send  report if this happens*/
                fprintf(stderr, "You have encountered a very special DVD, "
                        "please send a bug report along with all IFO files from this title\n");
                candidate = title_set_size_array[0];
            }
            dual = 1;
        }
    } else {
        candidate = title_set_size_array[0];
    }

    /* Lets start checking audio,sub pictures and channels my guess is
       namly that a special suburb will put titles with a lot of
       chapters just to make our backup hard */

    found = CheckAudioSubChannels(audio_audio_array, title_set_audio_array,
                                  subpicture_sub_array, title_set_sub_array,
                                  channels_channel_array, title_set_channel_array,
                                  0 , candidate, title_sets);

    /* Now lets see if we can find our candidate among the top most chapters */
    found_chapter=6;
    for (i=0 ; (i < titles) && (i < 4) ; i++ ) {
        if ( candidate == title_set_chapter_array[i] ) {
            found_chapter=i+1;
            break;
        }
    }

    /* Close the VMG ifo file we got all the info we need */
    ifoClose(vmg_ifo);

    if (((found == 3) && (found_chapter == 1) && (dual == 0) && (multi == 0))
        || ((found == 3) && (found_chapter < 3 ) && (dual == 1))) {

        FreeSortArrays( chapter_chapter_array, title_set_chapter_array,
                        angle_angle_array, title_set_angle_array,
                        subpicture_sub_array, title_set_sub_array,
                        audio_audio_array, title_set_audio_array,
                        size_size_array, title_set_size_array,
                        channels_channel_array, title_set_channel_array);
        titles_info->main_title_set = candidate;
        return(titles_info);

    }

    if (multi == 1) {
        for (i=0 ; i < title_sets ; ++i) {
            if (!CheckSizeArray(size_size_array, 0, i + 1)) {
                break;
            }
        }
        number_of_multi = i;
        for (i = 0; i < number_of_multi; i++ ) {
            if (title_set_chapter_array[0] == i + 1) {
                candidate = title_set_chapter_array[0];
            }
        }

        found = CheckAudioSubChannels(audio_audio_array, title_set_audio_array,
                                      subpicture_sub_array, title_set_sub_array,
                                      channels_channel_array, title_set_channel_array,
                                      0 , candidate, title_sets);

        if (found == 3) {
            FreeSortArrays( chapter_chapter_array, title_set_chapter_array,
                            angle_angle_array, title_set_angle_array,
                            subpicture_sub_array, title_set_sub_array,
                            audio_audio_array, title_set_audio_array,
                            size_size_array, title_set_size_array,
                            channels_channel_array, title_set_channel_array);
            titles_info->main_title_set = candidate;
            return(titles_info);
        }
    }

    /* We have now come to that state that we more or less have given
       up :( giving you a good guess of the main feature film No
       matter what we will more or less only return the biggest VOB
       Lets see if we can find our biggest one - then we return that
       one */
    candidate = title_set_size_array[0];

    found = CheckAudioSubChannels(audio_audio_array, title_set_audio_array,
                                  subpicture_sub_array, title_set_sub_array,
                                  channels_channel_array, title_set_channel_array,
                                  0 , candidate, title_sets);

    /* Now lets see if we can find our candidate among the top most chapters */

    found_chapter=5;
    for (i=0 ; (i < titles) && (i < 4) ; i++ ) {
        if ( candidate == title_set_chapter_array[i] ) {
            found_chapter=i+1;
            break;
        }

    }

    /* Here we take chapters in to consideration*/
    if (found == 3) {
        FreeSortArrays( chapter_chapter_array, title_set_chapter_array,
                        angle_angle_array, title_set_angle_array,
                        subpicture_sub_array, title_set_sub_array,
                        audio_audio_array, title_set_audio_array,
                        size_size_array, title_set_size_array,
                        channels_channel_array, title_set_channel_array);
        titles_info->main_title_set = candidate;
        return(titles_info);
    }

    /* Here we do but we lower the treshold for audio, sub and channels */

    if ((found > 1 ) && (found_chapter <= 4)) {
        FreeSortArrays( chapter_chapter_array, title_set_chapter_array,
                        angle_angle_array, title_set_angle_array,
                        subpicture_sub_array, title_set_sub_array,
                        audio_audio_array, title_set_audio_array,
                        size_size_array, title_set_size_array,
                        channels_channel_array, title_set_channel_array);
        titles_info->main_title_set = candidate;
        return(titles_info);

        /* return it */
    } else {
        /* Here we give up and just return the biggest one :(*/
        /* Just return the biggest badest one*/
        FreeSortArrays( chapter_chapter_array, title_set_chapter_array,
                        angle_angle_array, title_set_angle_array,
                        subpicture_sub_array, title_set_sub_array,
                        audio_audio_array, title_set_audio_array,
                        size_size_array, title_set_size_array,
                        channels_channel_array, title_set_channel_array);
        titles_info->main_title_set = candidate;
        return(titles_info);
    }

    /* Some radom thoughts about DVD guessing */
    /* We will now gather as much data about the DVD-Video as we can and
       then make a educated guess which one is the main feature film of it*/

    /* Make a tripple array with chapters, angles and title sets
       - sort out dual title sets with a low number of chapters. Tradtionaly
       the title set with most chapters is the main film. Number of angles is
       keept as a reference point of low value*/

    /* Make a dual array with number of audio streams, sub picture streams
       and title sets. Tradtionaly the main film has many audio streams
       since it's supposed be synconised e.g. a English film syncronised/dubbed
       in German. We are also keeping track of sub titles since it's also indication
       of the main film*/

    /* Which title set is the biggest one - dual array with title sets and size
       The biggest one is usally the main film*/

    /* Which title set is belonging to title 1 and how many chapters has it. Once
       again tradtionaly title one is belonging to the main film*/

    /* Yes a lot of rant - but it helps me think - some sketch on
       paper or in the mind I sketch in the comments - beside it will
       help you understand the code*/

    /* Okay lets see if the biggest one has most chapters, it also has
       more subtitles and audio tracks than the second one and its
       title one.  Done it must be the main film

       Hmm the biggest one doesn't have the most chapters?

       See if the second one has the same amount of chapters and is
       the biggest one If so we probably have a 4:3 and 16:9 versions
       of film on the same disk

       Now we fetch the 16:9 by default unless the forced to do 4:3
       First check which one is which.  If the 16:9 is the biggest one
       and has the same or more subtile, audio streams then we are
       happy unless we are in force 4:3 mode :( The same goes in
       reverse if we are in force 4:3 mode

       Hmm, in force 4:3 mode - now we check how much smaller than the
       biggest one it is (or the reverse if we are in 16:9 mode)

       Generally a reverse division should render in 1 and with a
       small modulo - like wise a normal modulo should give us a high
       modulo

       If we get more than one its of cource a fake however if we get
       just one we still need to check if we subtract the smaller one
       from the bigger one we should end up with a small number -
       hence we need to multiply it more than 4 times to get it bigger
       than the biggest one. Now we know that the two biggest once are
       really big and possibly carry the same film in differnet
       formats.

       We will now return the prefered one either 16:9 or 4:3 but we
       will first check that the one we return at lest has two or more
       audio tracks. We don't want it if the other one has a lot more
       sound (we may end up with a film that only has 2ch Dolby
       Digital so we want to check for 6ch DTS or Dolby Digital. If
       the prefered one doesn't have those features but the other once
       has we will return the other one.
    */
}

int
DVDCopyTileVobX (dvd_reader_t *dvd,
                 title_set_info_t *title_set_info,
                 int title_set, int vob,
                 char *targetdir, char *title_name)
{
    /* Loop variable */
    int i;

    /* Temp filename,dirname */
    char        targetname[PATH_MAX];
    struct stat fileinfo;

    /* Write buffer */

    unsigned char *buffer = NULL;
    unsigned char  buffy;       /* :-) */

    /* File Handler */
    int streamout;

    int size;
    int left;

    /* Buffer size in DVD sectors */
    int buff   = READ_BUF_SIZE_IN_BLOCKS;
    int offset = 0;
    int tsize;

    /* DVD handler */
    dvd_file_t *dvd_file = NULL;

    if (title_set_info->number_of_title_sets + 1 < title_set) {
        fprintf(stderr,"Failed num title test\n");
        return(1);
    }

    if (title_set_info->title_set[title_set].number_of_vob_files < vob ) {
        fprintf(stderr,"Failed vob test\n");
        return(1);
    }

    if (title_set_info->title_set[title_set].size_vob[0] == 0 ) {
        fprintf(stderr,"Failed vob 1 size test\n");
        return(0);
    } else if (title_set_info->title_set[title_set].size_vob[vob - 1] == 0 ) {
        fprintf(stderr,"Failed vob %d test\n", vob);
        return(0);
    } else {
        size = title_set_info->title_set[title_set].size_vob[vob - 1]/2048;
        if (title_set_info->title_set[title_set].size_vob[vob - 1]%2048 != 0) {
            fprintf(stderr, "The Title VOB number %d of title set %d "
                    "doesn't have a valid DVD size\n", vob, title_set);
            return(1);
        }
    }
#ifdef DEBUG
    fprintf(stderr,"After we check the vob it self %d\n", vob);
#endif

    /* Create VTS_XX_X.VOB */
    if (title_set == 0) {
        fprintf(stderr,"Don't try to copy a Title VOB from "
                "the VMG domain there aren't any\n");
        return(1);
    } else {
        sprintf(targetname,"%s/%s/VIDEO_TS/VTS_%02i_%i.VOB",
                targetdir, title_name, title_set, vob);
    }

    /* Now figure out the offset we will start at also check that the
       previus files are of valid DVD size */
    for ( i = 0; i < vob - 1; i++ ) {
        tsize = title_set_info->title_set[title_set].size_vob[i];
        if (tsize%2048 != 0) {
            fprintf(stderr, "The Title VOB number %d of title set %d "
                    "doesn't have a valid DVD size\n", i + 1, title_set);
            return(1);
        } else {
            offset = offset + tsize/2048;
        }
    }
#ifdef DEBUG
    fprintf(stderr,"The offset for vob %d is %d\n", vob, offset);
#endif

    if (stat(targetname, &fileinfo) == 0) {
        fprintf(stderr, "The Title file %s exists will try to over write it.\n",
                targetname);
        if (! S_ISREG(fileinfo.st_mode)) {
            fprintf(stderr,"The Title %s file is not valid, it may be a directory\n",
                    targetname);
            return(1);
        } else {
            if ((streamout = open(targetname, O_WRONLY | O_TRUNC, 0644)) == -1) {
                fprintf(stderr, "Error opening %s\n", targetname);
                perror("");
                return(1);
            }
        }
    } else {
        if ((streamout = open(targetname, O_WRONLY | O_CREAT, 0644)) == -1) {
            fprintf(stderr, "Error creating %s\n", targetname);
            perror("");
            return(1);
        }
    }

    left = size;

    if ((buffer = (unsigned char *)malloc(buff * 2048 * sizeof(buffy))) == NULL) {
        fprintf(stderr, "Out of memory coping %s\n", targetname);
        close(streamout);
        return(1);
    }

    if ((dvd_file = DVDOpenFile(dvd, title_set, DVD_READ_TITLE_VOBS))== 0) {
        fprintf(stderr, "Failed opending TITLE VOB\n");
        free(buffer);
        close(streamout);
        return(1);
    }

    while( left > 0 ) {

        if (buff > left) {
            buff = left;
        }
        if ( DVDReadBlocks(dvd_file,offset,buff, buffer) != buff) {
            fprintf(stderr, "Error reading MENU VOB\n");
            free(buffer);
            DVDCloseFile(dvd_file);
            close(streamout);
            return(1);
        }

        if (write(streamout,buffer,buff * 2048) != buff * 2048) {
            fprintf(stderr, "Error writing TITLE VOB\n");
            free(buffer);
            close(streamout);
            return(1);
        }

        offset = offset + buff;
        left = left - buff;

    }

    DVDCloseFile(dvd_file);
    free(buffer);
    close(streamout);
    return(0);
}

int
DVDCopyMenu (dvd_reader_t *dvd,
             title_set_info_t *title_set_info,
             int title_set,
             char *targetdir, char *title_name)
{

    /* Temp filename,dirname */
    char        targetname[PATH_MAX];
    struct stat fileinfo;

    /* Write buffer */

    unsigned char *buffer = NULL;
    unsigned char  buffy;       /* :-) */

    /* File Handler */
    int streamout;

    int size;
    int left;

    /* Buffer size in DVD sectors */
    int buff   = READ_BUF_SIZE_IN_BLOCKS;
    int offset = 0;

    /* DVD handler */
    dvd_file_t *dvd_file = NULL;

    if (title_set_info->number_of_title_sets + 1 < title_set) {
        return(1);
    }

    if (title_set_info->title_set[title_set].size_menu == 0 ) {
        return(0);
    } else {
        size = title_set_info->title_set[title_set].size_menu/2048;
        if (title_set_info->title_set[title_set].size_menu%2048 != 0) {
            fprintf(stderr, "The Menu VOB of title set %d doesn't "
                    "have a valid DVD size\n", title_set);
            return(1);
        }
    }

    /* Create VIDEO_TS.VOB or VTS_XX_0.VOB */
    if (title_set == 0) {
        sprintf(targetname,"%s/%s/VIDEO_TS/VIDEO_TS.VOB", targetdir, title_name);
    } else {
        sprintf(targetname,"%s/%s/VIDEO_TS/VTS_%02i_0.VOB",
                targetdir, title_name, title_set);
    }

    if (stat(targetname, &fileinfo) == 0) {
        fprintf(stderr, "The Menu file %s exists will try to over write it.\n",
                targetname);
        if (! S_ISREG(fileinfo.st_mode)) {
            fprintf(stderr,"The Menu %s file is not valid, it may be a directory\n",
                    targetname);
            return(1);
        } else {
            if ((streamout = open(targetname, O_WRONLY | O_TRUNC, 0644)) == -1) {
                fprintf(stderr, "Error opening %s\n", targetname);
                perror("");
                return(1);
            }
        }
    } else {
        if ((streamout = open(targetname, O_WRONLY | O_CREAT, 0644)) == -1) {
            fprintf(stderr, "Error creating %s\n", targetname);
            perror("");
            return(1);
        }
    }

    left = size;

    if ((buffer = (unsigned char *)malloc(buff * 2048 * sizeof(buffy))) == NULL) {
        fprintf(stderr, "Out of memory coping %s\n", targetname);
        close(streamout);
        return(1);
    }

    if ((dvd_file = DVDOpenFile(dvd, title_set, DVD_READ_MENU_VOBS))== 0) {
        fprintf(stderr, "Failed opending MENU VOB\n");
        free(buffer);
        close(streamout);
        return(1);
    }

    while( left > 0 ) {

        if (buff > left) {
            buff = left;
        }
        if ( DVDReadBlocks(dvd_file,offset,buff, buffer) != buff) {
            fprintf(stderr, "Error reading MENU VOB\n");
            free(buffer);
            DVDCloseFile(dvd_file);
            close(streamout);
            return(1);
        }

        if (write(streamout,buffer,buff * 2048) != buff * 2048) {
            fprintf(stderr, "Error writing MENU VOB\n");
            free(buffer);
            close(streamout);
            return(1);
        }

        offset = offset + buff;
        left = left - buff;

    }

    DVDCloseFile(dvd_file);
    free(buffer);
    close(streamout);
    return(0);
}

int
DVDCopyIfoBup (dvd_reader_t *dvd,
               title_set_info_t *title_set_info,
               int title_set,
               char *targetdir, char *title_name)
{
    /* Temp filename,dirname */
    char        targetname[PATH_MAX];
    struct stat fileinfo;

    /* Write buffer */

    unsigned char *buffer = NULL;
    unsigned char  buffy;       /* :-) */

    /* File Handler */
    int streamout;

    int size;

    /* DVD handler */
    dvd_file_t *dvd_file = NULL;

    if (title_set_info->number_of_title_sets + 1 < title_set) {
        return(1);
    }

    if (title_set_info->title_set[title_set].size_ifo == 0 ) {
        return(0);
    } else {
        size = title_set_info->title_set[title_set].size_ifo;
        if (title_set_info->title_set[title_set].size_ifo%2048 != 0) {
            fprintf(stderr, "The IFO of title set %d doesn't have a valid DVD size\n",
                    title_set);
            return(1);
        }
    }

    /* Create VIDEO_TS.IFO or VTS_XX_0.IFO */

    if (title_set == 0) {
        sprintf(targetname,"%s/%s/VIDEO_TS/VIDEO_TS.IFO",targetdir, title_name);
    } else {
        sprintf(targetname,"%s/%s/VIDEO_TS/VTS_%02i_0.IFO",targetdir, title_name, title_set);
    }

    if (stat(targetname, &fileinfo) == 0) {
        fprintf(stderr, "The IFO file %s exists will try to over write it.\n",
                targetname);
        if (! S_ISREG(fileinfo.st_mode)) {
            fprintf(stderr,"The IFO %s file is not valid, it may be a directory\n",
                    targetname);
            return(1);
        } else {
            if ((streamout = open(targetname, O_WRONLY | O_TRUNC, 0644)) == -1) {
                fprintf(stderr, "Error opening %s\n", targetname);
                perror("");
                return(1);
            }
        }
    } else {
        if ((streamout = open(targetname, O_WRONLY | O_CREAT, 0644)) == -1) {
            fprintf(stderr, "Error creating %s\n", targetname);
            perror("");
            return(1);
        }
    }

    /* Copy VIDEO_TS.IFO, since it's a small file try to copy it in one shot */

    if ((buffer = (unsigned char *)malloc(size * sizeof(buffy))) == NULL) {
        fprintf(stderr, "Out of memory coping %s\n", targetname);
        close(streamout);
        return(1);
    }

    if ((dvd_file = DVDOpenFile(dvd, title_set, DVD_READ_INFO_FILE))== 0) {
        fprintf(stderr, "Failed opending IFO for tile set %d\n", title_set);
        free(buffer);
        close(streamout);
        return(1);
    }

    if ( DVDReadBytes(dvd_file,buffer,size) != size) {
        fprintf(stderr, "Error reading IFO for title set %d\n", title_set);
        free(buffer);
        DVDCloseFile(dvd_file);
        close(streamout);
        return(1);
    }

    DVDCloseFile(dvd_file);

    if (write(streamout,buffer,size) != size) {
        fprintf(stderr, "Error writing %s\n",targetname);
        free(buffer);
        close(streamout);
        return(1);
    }

    free(buffer);
    close(streamout);

    /* Create VIDEO_TS.BUP or VTS_XX_0.BUP */

    if (title_set == 0) {
        sprintf(targetname,"%s/%s/VIDEO_TS/VIDEO_TS.BUP",targetdir, title_name);
    } else {
        sprintf(targetname,"%s/%s/VIDEO_TS/VTS_%02i_0.BUP",targetdir, title_name, title_set);
    }

    if (title_set_info->title_set[title_set].size_bup == 0 ) {
        return(0);
    } else {
        size = title_set_info->title_set[title_set].size_bup;
        if (title_set_info->title_set[title_set].size_bup%2048 != 0) {
            fprintf(stderr, "The BUP of title set %d doesn't "
                    "have a valid DVD size\n", title_set);
            return(1);
        }
    }

    if (stat(targetname, &fileinfo) == 0) {
        fprintf(stderr, "The BUP file %s exists will try to over write it.\n",
                targetname);
        if (! S_ISREG(fileinfo.st_mode)) {
            fprintf(stderr,"The BUP %s file is not valid, it may be a directory\n",
                    targetname);
            return(1);
        } else {
            if ((streamout = open(targetname, O_WRONLY | O_TRUNC, 0644)) == -1) {
                fprintf(stderr, "Error opening %s\n", targetname);
                perror("");
                return(1);
            }
        }
    } else {
        if ((streamout = open(targetname, O_WRONLY | O_CREAT, 0644)) == -1) {
            fprintf(stderr, "Error creating %s\n", targetname);
            perror("");
            return(1);
        }
    }

    /* Copy VIDEO_TS.BUP or VTS_XX_0.BUP, since it's a small file try
       to copy it in one shot */

    if ((buffer = (unsigned char *)malloc(size * sizeof(buffy))) == NULL) {
        fprintf(stderr, "Out of memory coping %s\n", targetname);
        close(streamout);
        return(1);
    }

    if ((dvd_file = DVDOpenFile(dvd, title_set, DVD_READ_INFO_BACKUP_FILE))== 0) {
        fprintf(stderr, "Failed opending BUP for title set %d\n", title_set);
        free(buffer);
        close(streamout);
        return(1);
    }

    if ( DVDReadBytes(dvd_file,buffer,size) != size) {
        fprintf(stderr, "Error reading BUP for title set %d\n", title_set);
        free(buffer);
        DVDCloseFile(dvd_file);
        close(streamout);
        return(1);
    }

    DVDCloseFile(dvd_file);

    if (write(streamout,buffer,size) != size) {
        fprintf(stderr, "Error writing %s\n", targetname);
        free(buffer);
        close(streamout);
        return(1);
    }

    free(buffer);
    close(streamout);

    return(0);
}

int
DVDMirrorVMG (dvd_reader_t *dvd,
              title_set_info_t *title_set_info,
              char *targetdir, char *title_name)
{
    if ( DVDCopyIfoBup(dvd, title_set_info, 0, targetdir, title_name) != 0 ) {
        return(1);
    }

    if ( DVDCopyMenu(dvd, title_set_info, 0, targetdir, title_name) != 0 ) {
        return(1);
    }
    return(0);
}

int
DVDMirrorTitleX (dvd_reader_t *dvd,
                 title_set_info_t *title_set_info,
                 int title_set, char *targetdir,char *title_name)
{
    /* Loop through the vobs */
    int i;

    if ( DVDCopyIfoBup(dvd, title_set_info, title_set, targetdir, title_name) != 0 ) {
        return(1);
    }

    if ( DVDCopyMenu(dvd, title_set_info, title_set, targetdir, title_name) != 0 ) {
        return(1);
    }

    for (i = 0; i < title_set_info->title_set[title_set].number_of_vob_files ; i++) {
#ifdef DEBUG
        fprintf(stderr,"In the VOB copy loop for %d\n", i);
#endif
        if ( DVDCopyTileVobX(dvd, title_set_info, title_set,
                             i + 1, targetdir, title_name) != 0 ) {
            return(1);
        }
    }

    return(0);
}

int
DVDGetTitleName (const char *device, char *title)
{
    /* Variables for filehandel and title string interaction */

    int  filehandle, i, last;

    /* Open DVD device */

    if ( !(filehandle = open(device, O_RDONLY)) ) {
        fprintf(stderr, "Can't open secified device %s - check your DVD device\n",
                device);
        return(1);
    }

    /* Seek to title of first track, which is at (track_no * 32768) + 40 */

    if ( 32808 != lseek(filehandle, 32808, SEEK_SET) ) {
        close(filehandle);
        fprintf(stderr, "Can't seek DVD device %s - check your DVD device\n",
                device);
        return(1);
    }

    /* Read the DVD-Video title */

    if ( 32 != read(filehandle, title, 32)) {
        close(filehandle);
        fprintf(stderr, "Can't read title from DVD device %s\n", device);
        return(1);
    }

    /* Terminate the title string */

    title[32] = '\0';

    /* Remove trailing white space */

    last = 32;
    for ( i = 0; i < 32; i++ ) {
        if ( title[i] != ' ' ) { last = i; }
    }

    title[last + 1] = '\0';

    return(0);
}

void
bsort_min_to_max (int sector[], int title[], int size)
{
    int temp_title, temp_sector, i, j;

    for ( i=0; i < size ; i++ ) {
        for ( j=0; j < size ; j++ ) {
            if (sector[i] < sector[j]) {
                temp_sector = sector[i];
                temp_title = title[i];
                sector[i] = sector[j];
                title[i] = title[j];
                sector[j] = temp_sector;
                title[j] = temp_title;
            }
        }
    }
}

void
bsort_max_to_min (int sector[], int title[], int size)
{
    int temp_title, temp_sector, i, j;

    for ( i=0; i < size ; i++ ) {
        for ( j=0; j < size ; j++ ) {
            if (sector[i] > sector[j]) {
                temp_sector = sector[i];
                temp_title = title[i];
                sector[i] = sector[j];
                title[i] = title[j];
                sector[j] = temp_sector;
                title[j] = temp_title;
            }
        }
    }
}

void
uniq (int sector[], int title[], int title_sets_array[],
      int sector_sets_array[], int titles)
{
    int  i, j;

    for ( i=0, j=0; j < titles;) {
        if (sector[j] != sector[j+1]) {
            title_sets_array[i]  = title[j];
            sector_sets_array[i] = sector[j];
            i++ ;
            j++ ;
        } else {
            do {
                if (j < titles) {
                    j++ ;
                }
            } while ( sector[j] == sector[j+1] );

        }
    }

}

void
align_end_sector (int cell_start_sector[], int cell_end_sector[], int size)
{
    int i;

    for (i = 0; i < size  - 1 ; i++) {
        if ( cell_end_sector[i] >= cell_start_sector[i + 1] ) {
            cell_end_sector[i] = cell_start_sector[i + 1] - 1;
        }
    }
}

void
DVDFreeTitleSetInfo (title_set_info_t *title_set_info)
{
    free(title_set_info->title_set);
    free(title_set_info);
}

void
DVDFreeTitlesInfo (titles_info_t *titles_info)
{
    free(titles_info->titles);
    free(titles_info);
}

title_set_info_t*
DVDGetFileSet (dvd_reader_t *_dvd)
{
    /* title interation */
    int title_sets, counter, i;

    /* DVD Video files */
    char     filename[MAXNAME];
    uint32_t size;

    /*DVD ifo handler*/
    ifo_handle_t *vmg_ifo = NULL;

    /* The Title Set Info struct*/
    title_set_info_t *title_set_info;

    /*  Open main info file */
    vmg_ifo = ifoOpen( _dvd, 0 );
    if( !vmg_ifo ) {
        fprintf( stderr, "Can't open VMG info.\n" );
        return (0);
    }

    title_sets = vmg_ifo->vmgi_mat->vmg_nr_of_title_sets;

    /* Close the VMG ifo file we got all the info we need */
    ifoClose(vmg_ifo);

    /* Todo fix malloc check */
    title_set_info = (title_set_info_t *)malloc(sizeof(title_set_info_t));
    title_set_info->title_set = (title_set_t *)malloc((title_sets + 1)* sizeof(title_set_t));

    title_set_info->number_of_title_sets = title_sets;

    /* Find VIDEO_TS.IFO is present - must be present since we did a ifo open 0*/

    sprintf(filename,"/VIDEO_TS/VIDEO_TS.IFO");

    if ( UDFFindFile(_dvd, filename, &size) != 0 ) {
        title_set_info->title_set[0].size_ifo = size;
    } else {
        DVDFreeTitleSetInfo(title_set_info);
        return(0);
    }

    /* Find VIDEO_TS.VOB if present*/

    sprintf(filename,"/VIDEO_TS/VIDEO_TS.VOB");

    if ( UDFFindFile(_dvd, filename, &size) != 0 ) {
        title_set_info->title_set[0].size_menu = size;
    } else {
        title_set_info->title_set[0].size_menu = 0 ;
    }

    /* Find VIDEO_TS.BUP if present */

    sprintf(filename,"/VIDEO_TS/VIDEO_TS.BUP");

    if ( UDFFindFile(_dvd, filename, &size) != 0 ) {
        title_set_info->title_set[0].size_bup = size;
    } else {
        DVDFreeTitleSetInfo(title_set_info);
        return(0);
    }

    if (title_set_info->title_set[0].size_ifo != title_set_info->title_set[0].size_bup) {
        fprintf(stderr,"BUP and IFO size not the same be warened!\n");
    }

    /* Take care of the titles which we don't have in VMG */

    title_set_info->title_set[0].number_of_vob_files = 0;
    title_set_info->title_set[0].size_vob[0] = 0;

    if ( verbose > 0 ){
        fprintf(stderr,"\n\n\nFile sizes for Title set 0 VIDEO_TS.XXX\n");
        fprintf(stderr,"IFO = %d, MENU_VOB = %d, BUP = %d\n",
                title_set_info->title_set[0].size_ifo,
                title_set_info->title_set[0].size_menu,
                title_set_info->title_set[0].size_bup );
    }

    if ( title_sets >= 1 ) {
        for (counter=0; counter < title_sets; counter++ ){

            if ( verbose > 1 ){
                fprintf(stderr,"At top of loop\n");
            }

            sprintf(filename,"/VIDEO_TS/VTS_%02i_0.IFO",counter + 1);

            if ( UDFFindFile(_dvd, filename, &size) != 0 ) {
                title_set_info->title_set[counter + 1].size_ifo = size;
            } else {
                DVDFreeTitleSetInfo(title_set_info);
                return(0);
            }

            if ( verbose > 1 ){
                fprintf(stderr,"After opening files\n");
            }

            /* Find VTS_XX_0.VOB if present*/

            sprintf(filename,"/VIDEO_TS/VTS_%02i_0.VOB", counter + 1);

            if ( UDFFindFile(_dvd, filename, &size) != 0 ) {
                title_set_info->title_set[counter + 1].size_menu = size;
            } else {
                title_set_info->title_set[counter + 1].size_menu = 0 ;
            }

            if ( verbose > 1 ){
                fprintf(stderr,"After Menu VOB check\n");
            }

            /* Find all VTS_XX_[1 to 9].VOB files if they are present*/

            for( i = 0; i < 9; ++i ) {
                sprintf(filename,"/VIDEO_TS/VTS_%02i_%i.VOB", counter + 1, i + 1 );
                if(UDFFindFile(_dvd, filename, &size) == 0 ) {
                    break;
                }
                title_set_info->title_set[counter + 1].size_vob[i] = size;
            }
            title_set_info->title_set[counter + 1].number_of_vob_files = i;

            if ( verbose > 1 ){
                fprintf(stderr,"After Menu Title VOB check\n");
            }

            sprintf(filename,"/VIDEO_TS/VTS_%02i_0.BUP", counter + 1);

            if ( UDFFindFile(_dvd, filename, &size) != 0 ) {
                title_set_info->title_set[counter +1].size_bup = size;
            } else {
                DVDFreeTitleSetInfo(title_set_info);
                return(0);
            }

            if (title_set_info->title_set[counter +1].size_ifo !=
                title_set_info->title_set[counter + 1].size_bup) {
                fprintf(stderr,"BUP and IFO size for fileset %d is "
                        "not the same be warened!\n", counter + 1);
            }

            if ( verbose > 1 ){
                fprintf(stderr,"After Menu Title BUP check\n");
            }

            if ( verbose > 0 ) {
                fprintf(stderr,"\n\n\nFile sizes for Title set %d "
                        "i.e.VTS_%02d_X.XXX\n", counter + 1, counter + 1);
                fprintf(stderr,"IFO: %d, MENU: %d\n",
                        title_set_info->title_set[counter +1].size_ifo,
                        title_set_info->title_set[counter +1].size_menu);
                for (i = 0;
                     i < title_set_info->title_set[counter + 1].number_of_vob_files;
                     i++) {
                    fprintf(stderr, "VOB %d is %d\n", i + 1,
                            title_set_info->title_set[counter + 1].size_vob[i]);
                }
                fprintf(stderr, "BUP: %d\n",
                        title_set_info->title_set[counter +1].size_bup);
            }

            if ( verbose > 1 ){
                fprintf(stderr, "Bottom of loop \n");
            }
        }

    }

    /* Return the info */
    return(title_set_info);
}

int
DVDMirror (dvd_reader_t *_dvd, char *targetdir, char *title_name)
{
    int i;
    title_set_info_t *title_set_info=NULL;

    title_set_info = DVDGetFileSet(_dvd);
    if (!title_set_info) {
        DVDClose(_dvd);
        return(1);
    }

    if ( DVDMirrorVMG(_dvd, title_set_info, targetdir, title_name) != 0 ) {
        fprintf(stderr,"Mirror of VMG failed\n");
        DVDFreeTitleSetInfo(title_set_info);
        return(1);
    }

    for ( i=0; i < title_set_info->number_of_title_sets; i++) {
        if ( DVDMirrorTitleX(_dvd, title_set_info, i + 1, targetdir, title_name) != 0 ) {
            fprintf(stderr,"Mirror of Title set %d failed\n", i + 1);
            DVDFreeTitleSetInfo(title_set_info);
            return(1);
        }
    }
    return(0);
}

int
DVDMirrorTitleSet (dvd_reader_t *_dvd,
                   char *targetdir,
                   char *title_name,
                   int title_set)
{
    title_set_info_t *title_set_info=NULL;

#ifdef DEBUG
    fprintf(stderr,"In DVDMirrorTitleSet\n");
#endif

    title_set_info = DVDGetFileSet(_dvd);

    if (!title_set_info) {
        DVDClose(_dvd);
        return(1);
    }

    if ( title_set > title_set_info->number_of_title_sets ) {
        fprintf(stderr, "Can't copy title_set %d there is only %d "
                "title_sets present on this DVD\n", title_set,
                title_set_info->number_of_title_sets);
        DVDFreeTitleSetInfo(title_set_info);
        return(1);
    }

    if ( title_set == 0 ) {
        if ( DVDMirrorVMG(_dvd, title_set_info, targetdir, title_name) != 0 ) {
            fprintf(stderr,"Mirror of Title set 0 (VMG) failed\n");
            DVDFreeTitleSetInfo(title_set_info);
            return(1);
        }
    } else {
        if ( DVDMirrorTitleX(_dvd, title_set_info, title_set,
                             targetdir, title_name) != 0 ) {
            fprintf(stderr,"Mirror of Title set %d failed\n", title_set);
            DVDFreeTitleSetInfo(title_set_info);
            return(1);
        }
    }
    DVDFreeTitleSetInfo(title_set_info);
    return(0);
}

int
DVDMirrorMainFeature (dvd_reader_t *_dvd,
                      char *targetdir,
                      char *title_name)
{
    title_set_info_t *title_set_info=NULL;
    titles_info_t *titles_info=NULL;

    titles_info = DVDGetInfo(_dvd);
    if (!titles_info) {
        fprintf(stderr, "Guess work of main feature film failed\n");
        return(1);
    }

    title_set_info = DVDGetFileSet(_dvd);
    if (!title_set_info) {
        DVDFreeTitlesInfo(titles_info);
        return(1);
    }

    if ( DVDMirrorTitleX(_dvd, title_set_info, titles_info->main_title_set,
                         targetdir, title_name) != 0 ) {
        fprintf(stderr,"Mirror of main featur file which is title set %d failed\n",
                titles_info->main_title_set);
        DVDFreeTitleSetInfo(title_set_info);
        return(1);
    }

    DVDFreeTitlesInfo(titles_info);
    DVDFreeTitleSetInfo(title_set_info);
    return(0);
}

int
DVDMirrorChapters (dvd_reader_t *_dvd,
                   char *targetdir,
                   char *title_name, 
                   int start_chapter,
                   int end_chapter,
                   int titles)
{
    int result;
    int chapters = 0;
    int i, s;
    int spg, epg;
    int pgc;
    int start_cell, end_cell;
    int vts_title;

    title_set_info_t *title_set_info    = NULL;
    titles_info_t    *titles_info       = NULL;
    ifo_handle_t     *vts_ifo_info      = NULL;
    int              *cell_start_sector = NULL;
    int              *cell_end_sector   = NULL;

    /* To do free memory*/

    titles_info = DVDGetInfo(_dvd);
    if (!titles_info) {
        fprintf(stderr, "Failed to obtain titles information\n");
        return(1);
    }

    title_set_info = DVDGetFileSet(_dvd);
    if (!title_set_info) {
        DVDFreeTitlesInfo(titles_info);
        return(1);
    }

    if(titles == 0) {
        fprintf(stderr, "No title specified for chapter extraction, "
                "will try to figure out main feature title\n");
        for (i=0; i < titles_info->number_of_titles ; i++ ) {
            if ( titles_info->titles[i].title_set == titles_info->main_title_set ) {
                if(chapters < titles_info->titles[i].chapters) {
                    chapters = titles_info->titles[i].chapters;
                    titles = i + 1;
                }
            }
        }
    }

    vts_ifo_info = ifoOpen(_dvd, titles_info->titles[titles - 1].title_set);
    if(!vts_ifo_info) {
        fprintf(stderr, "Coundn't open tile_set %d IFO file\n",
                titles_info->titles[titles - 1].title_set);
        DVDFreeTitlesInfo(titles_info);
        DVDFreeTitleSetInfo(title_set_info);
        return(1);
    }

    vts_title = titles_info->titles[titles - 1].vts_title;

    if (end_chapter > titles_info->titles[titles - 1].chapters) {
        end_chapter = titles_info->titles[titles - 1].chapters;
        fprintf(stderr, "Turncated the end_chapter only %d chapters in %d title\n",
                end_chapter,titles);
    }

    if (start_chapter > titles_info->titles[titles - 1].chapters) {
        start_chapter = titles_info->titles[titles - 1].chapters;
        fprintf(stderr, "Turncated the end_chapter only %d chapters in %d title\n",
                end_chapter,titles);
    }

    /* We assume the same PGC for the whole title - this is not true
       and need to be fixed later on */

    pgc = vts_ifo_info->vts_ptt_srpt->title[vts_title - 1].ptt[start_chapter - 1].pgcn;

    /* Lookup  PG for start chapter */

    spg = vts_ifo_info->vts_ptt_srpt->title[vts_title - 1].ptt[start_chapter - 1].pgn;

    /* Look up start cell for this pgc/pg */

    start_cell = vts_ifo_info->vts_pgcit->pgci_srp[pgc - 1].pgc->program_map[spg - 1];

    /* Lookup end cell*/

    if ( end_chapter < titles_info->titles[titles - 1].chapters ) {
        epg = vts_ifo_info->vts_ptt_srpt->title[vts_title - 1].ptt[end_chapter].pgn;
#ifdef DEBUG
        fprintf(stderr,"DVDMirrorChapter: epg %d\n", epg);
#endif

        end_cell = vts_ifo_info->vts_pgcit->pgci_srp[pgc - 1].pgc->program_map[epg -1] - 1;
#ifdef DEBUG
        fprintf(stderr,"DVDMirrorChapter: end cell adjusted %d\n", end_cell);
#endif

    } else {

        end_cell = vts_ifo_info->vts_pgcit->pgci_srp[pgc - 1].pgc->nr_of_cells;
#ifdef DEBUG
        fprintf(stderr,"DVDMirrorChapter: end cell adjusted 2 %d\n",end_cell);
#endif
    }

#ifdef DEBUG
    fprintf(stderr,"DVDMirrorChapter: star cell %d\n", start_cell);
#endif

    /* Put all the cells start and end sector in a dual array */

    cell_start_sector = (int *)malloc( (end_cell - start_cell + 1) * sizeof(int));
    if(!cell_start_sector) {
        fprintf(stderr,"Memory allocation error 1\n");
        DVDFreeTitlesInfo(titles_info);
        DVDFreeTitleSetInfo(title_set_info);
        ifoClose(vts_ifo_info);
        return(1);
    }
    cell_end_sector = (int *)malloc( (end_cell - start_cell + 1) * sizeof(int));
    if(!cell_end_sector) {
        fprintf(stderr,"Memory allocation error\n");
        DVDFreeTitlesInfo(titles_info);
        DVDFreeTitleSetInfo(title_set_info);
        ifoClose(vts_ifo_info);
        free(cell_start_sector);
        return(1);
    }
#ifdef DEBUG
    fprintf(stderr,"DVDMirrorChapter: start cell is %d\n", start_cell);
    fprintf(stderr,"DVDMirrorChapter: end cell is %d\n", end_cell);
    fprintf(stderr,"DVDMirrorChapter: pgc is %d\n", pgc);
#endif

    for (i=0, s=start_cell; s < end_cell +1 ; i++, s++) {

        cell_start_sector[i] =
            vts_ifo_info->vts_pgcit->pgci_srp[pgc - 1].pgc->cell_playback[s - 1].first_sector;
        cell_end_sector[i] =
            vts_ifo_info->vts_pgcit->pgci_srp[pgc - 1].pgc->cell_playback[s - 1].last_sector;
#ifdef DEBUG
        fprintf(stderr,"DVDMirrorChapter: S is %d\n", s);
        fprintf(stderr, "DVDMirrorChapter: start sector %d\n",
                vts_ifo_info->vts_pgcit->pgci_srp[pgc - 1].pgc->cell_playback[s - 1].first_sector);
        fprintf(stderr,"DVDMirrorChapter: end sector %d\n",
                vts_ifo_info->vts_pgcit->pgci_srp[pgc - 1].pgc->cell_playback[s - 1].last_sector);
#endif
    }

    bsort_min_to_max(cell_start_sector, cell_end_sector, end_cell - start_cell + 1);

    align_end_sector(cell_start_sector, cell_end_sector,end_cell - start_cell + 1);

#ifdef DEBUG
    for (i=0 ; i < end_cell - start_cell + 1; i++) {
        fprintf(stderr, "DVDMirrorChapter: Start sector is %d end sector is %d\n",
                cell_start_sector[i], cell_end_sector[i]);
    }
#endif

    result = DVDWriteCells(_dvd, cell_start_sector,
                           cell_end_sector , end_cell - start_cell + 1,
                           titles, title_set_info, titles_info, targetdir, title_name);

    DVDFreeTitlesInfo(titles_info);
    DVDFreeTitleSetInfo(title_set_info);
    ifoClose(vts_ifo_info);
    free(cell_start_sector);
    free(cell_end_sector);

    if( result != 0) {
        return(1);
    } else {
        return(0);
    }
}

int
DVDMirrorTitles (dvd_reader_t *_dvd,
                 char *targetdir,
                 char *title_name,
                 int titles)
{
    int end_chapter;

    titles_info_t *titles_info=NULL;

#ifdef DEBUG
    fprintf(stderr,"In DVDMirrorTitles\n");
#endif

    titles_info = DVDGetInfo(_dvd);
    if (!titles_info) {
        fprintf(stderr, "Failed to obtain titles information\n");
        return(1);
    }

    end_chapter = titles_info->titles[titles - 1].chapters;
#ifdef DEBUG
    fprintf(stderr,"DVDMirrorTitles: end_chapter %d\n", end_chapter);
#endif

    if (DVDMirrorChapters( _dvd, targetdir, title_name, 1, end_chapter, titles) != 0 ) {
        DVDFreeTitlesInfo(titles_info);
        return(1);
    }

    DVDFreeTitlesInfo(titles_info);

    return(0);
}

int
DVDDisplayInfo (dvd_reader_t *_dvd, char *dvd)
{
    int               i, f;
    int               chapters;
    int               channels;
    char              title_name[33] = "";
    title_set_info_t *title_set_info = NULL;
    titles_info_t    *titles_info    = NULL;

    titles_info = DVDGetInfo(_dvd);
    if (!titles_info) {
        fprintf(stderr, "Guess work of main feature film failed\n");
        return(1);
    }

    title_set_info = DVDGetFileSet(_dvd);
    if (!title_set_info) {
        DVDFreeTitlesInfo(titles_info);
        return(1);
    }

    DVDGetTitleName(dvd,title_name);

    fprintf(stdout,"\n\n\nDVD-Video information of the DVD with tile %s\n\n", title_name);

    /* Print file structure */

    fprintf(stdout,"File Structure DVD\n");
    fprintf(stdout,"VIDEO_TS/\n");
    fprintf(stdout,"\tVIDEO_TS.IFO\t%i\n", title_set_info->title_set[0].size_ifo);

    if (title_set_info->title_set[0].size_menu != 0 ) {
        fprintf(stdout,"\tVIDEO_TS.VOB\t%i\n", title_set_info->title_set[0].size_menu);
    }

    fprintf(stdout,"\tVIDEO_TS.BUP\t%i\n", title_set_info->title_set[0].size_bup);

    for( i = 0 ; i < title_set_info->number_of_title_sets ; i++) {
        fprintf(stdout,"\tVTS_%02i_0.IFO\t%i\n", i + 1,
                title_set_info->title_set[i + 1].size_ifo);
        if (title_set_info->title_set[i + 1].size_menu != 0 ) {
            fprintf(stdout,"\tVTS_%02i_0.VOB\t%i\n", i + 1,
                    title_set_info->title_set[i + 1].size_menu);
        }
        if (title_set_info->title_set[i + 1].number_of_vob_files != 0) {
            for( f = 0; f < title_set_info->title_set[i + 1].number_of_vob_files ; f++ ) {
                fprintf(stdout,"\tVTS_%02i_%i.VOB\t%i\n",
                        i + 1, f + 1, title_set_info->title_set[i + 1].size_vob[f]);
            }
        }
        fprintf(stdout,"\tVTS_%02i_0.BUP\t%i\n", i + 1,
                title_set_info->title_set[i + 1].size_bup);
    }

    fprintf(stdout,"\n\nMain feature:\n");
    fprintf(stdout,"\tTitle set containing the main feature is  %d\n",
            titles_info->main_title_set);
    for (i=0; i < titles_info->number_of_titles ; i++ ) {
        if (titles_info->titles[i].title_set == titles_info->main_title_set) {
            if(titles_info->titles[i].aspect_ratio == 3) {
                fprintf(stdout,"\tThe aspect ratio of the main feature is 16:9\n");
            } else if (titles_info->titles[i].aspect_ratio == 0) {
                fprintf(stdout,"\tThe aspect ratio of the main feature is 4:3\n");
            } else {
                fprintf(stdout,"\tThe aspect ratio of the main feature is unknown\n");
            }
            fprintf(stdout,"\tThe main feature has %d angle(s)\n",
                    titles_info->titles[i].angles);
            fprintf(stdout,"\tThe main feature has %d audio_track(s)\n",
                    titles_info->titles[i].angles);
            fprintf(stdout,"\tThe main feature has %d subpicture channel(s)\n",
                    titles_info->titles[i].sub_pictures);
            chapters=0;
            channels=0;

            for (f=0; f < titles_info->number_of_titles ; f++ ) {
                if ( titles_info->titles[i].title_set == titles_info->main_title_set ) {
                    if(chapters < titles_info->titles[f].chapters) {
                        chapters = titles_info->titles[f].chapters;
                    }
                    if(channels < titles_info->titles[f].audio_channels) {
                        channels = titles_info->titles[f].audio_channels;
                    }
                }
            }
            fprintf(stdout, "\tThe main feature has a maximum of %d "
                    "chapter(s) in on of its titles\n", chapters);
            fprintf(stdout, "\tThe main feature has a maximum of %d "
                    "audio channel(s) in on of its titles\n", channels);
            break;
        }

    }

    fprintf(stdout,"\n\nTitle Sets:");
    for (f=0; f < title_set_info->number_of_title_sets ; f++ ) {
        fprintf(stdout,"\n\n\tTitle set %d\n", f + 1);
        for (i=0; i < titles_info->number_of_titles ; i++ ) {
            if (titles_info->titles[i].title_set == f + 1) {
                if(titles_info->titles[i].aspect_ratio == 3) {
                    fprintf(stdout,"\t\tThe aspect ratio of title set %d is 16:9\n", f + 1);
                } else if (titles_info->titles[i].aspect_ratio == 0) {
                    fprintf(stdout,"\t\tThe aspect ratio of title set %d is 4:3\n", f + 1);
                } else {
                    fprintf(stdout,"\t\tThe aspect ratio of title set %d is unknown\n", f + 1);
                }
                fprintf(stdout,"\t\tTitle set %d has %d angle(s)\n",
                        f + 1, titles_info->titles[i].angles);
                fprintf(stdout,"\t\tTitle set %d has %d audio_track(s)\n",
                        f + 1, titles_info->titles[i].angles);
                fprintf(stdout,"\t\tTitle set %d has %d subpicture channel(s)\n",
                        f + 1, titles_info->titles[i].sub_pictures);
                break;
            }
        }
        fprintf(stdout,"\n\t\tTitles included in title set %d is/are\n", f + 1);
        for (i=0; i < titles_info->number_of_titles ; i++ ) {
            if (titles_info->titles[i].title_set == f + 1) {
                fprintf(stdout,"\t\t\tTitle %d:\n", i + 1);
                fprintf(stdout,"\t\t\t\tTitle %d has %d chapter(s)\n",
                        i + 1, titles_info->titles[i].chapters);
                fprintf(stdout,"\t\t\t\tTitle %d has %d audio channle(s)\n",
                        i + 1, titles_info->titles[i].audio_channels);
            }
        }
    }
    DVDFreeTitlesInfo(titles_info);
    DVDFreeTitleSetInfo(title_set_info);

    return(0);
}

int
main (int argc, char *argv[])
{
    /* Args */
    int flags;

    /* Switches */
    int title_set = 0;
    int titles;
    int start_chapter;
    int end_chapter;

    int do_mirror    = 0;
    int do_title_set = 0;
    int do_chapter   = 0;
    int do_titles    = 0;
    int do_feature   = 0;
    int do_info      = 0;

    int return_code;

    /* DVD Video device */
    char *dvd = NULL;

    /* Temp switch helpers */
    char *verbose_temp       = NULL;
    char *aspect_temp        = NULL;
    char *start_chapter_temp = NULL;
    char *end_chapter_temp   = NULL;
    char *titles_temp        = NULL;
    char *title_set_temp     = NULL;

    /* Title of the DVD */
    char title_name[33]       = "";
    char *provided_title_name = NULL;

    /* Targer dir */
    char *targetdir = NULL;

    /* Temp filename,dirname */
    char        targetname[PATH_MAX];
    struct stat fileinfo;

    /* The DVD main structure */
    dvd_reader_t *_dvd = NULL;

    /*Todo do isdigit check */

    while ((flags = getopt(argc, argv, "MFI?hi:v:a:o:n:s:e:t:T:")) != -1) {
        switch (flags) {
        case 'i':
            if(optarg[0]=='-') usage();
            dvd = optarg;
            break;
        case 'v':
            if(optarg[0]=='-') usage();
            verbose_temp = optarg;
            break;
        case 'o':
            if(optarg[0]=='-') usage();
            targetdir = optarg;
            break;
        case 'n':
            if(optarg[0]=='-') usage();
            provided_title_name = optarg;
            break;
        case 'a':
            if(optarg[0]=='-') usage();
            aspect_temp = optarg;
            break;
        case 's':
            if(optarg[0]=='-') usage();
            start_chapter_temp = optarg;
            break;
        case 'e':
            if(optarg[0]=='-') usage();
            end_chapter_temp = optarg;
            break;
        case 't':
            if(optarg[0]=='-') usage();
            titles_temp = optarg;
            break;
        case 'T':
            if(optarg[0]=='-') usage();
            title_set_temp = optarg;
            break;
        case 'M':
            do_mirror = 1;
            break;
        case 'F':
            do_feature = 1;
            break;
        case 'I':
            do_info = 1;
            break;

        case '?':
            usage();
            break;
        case 'h':
            usage();
            break;
        default:
            usage();
        }
    }

    if (dvd == NULL) {
        usage();
    }

    if (targetdir == NULL && do_info == 0) {
        usage();
    }

    if(verbose_temp == NULL) {
        verbose = 0;
    } else {
        verbose = atoi(verbose_temp);
    }

    if (aspect_temp == NULL) {
        /* Deafult to 16:9 aspect ratio */
        aspect = 3;
    } else {
        aspect = atoi(aspect_temp);
    }

    if((aspect != 0) && (aspect != 3) && (do_info == 0)){
        usage();
    }

    if ( titles_temp != NULL) {
        titles = atoi(titles_temp);
        if ( titles < 1 ) {
            usage();
        }
    }

    if ( start_chapter_temp !=NULL) {
        start_chapter = atoi(start_chapter_temp);
        if ( start_chapter < 1 || start_chapter > 99 ) {
            usage();
        }
    }

    if (end_chapter_temp != NULL) {
        end_chapter = atoi(end_chapter_temp);
        if ( end_chapter < 1 || end_chapter > 99 ) {
            usage();
        }
    }

    if ( end_chapter_temp != NULL || start_chapter_temp != NULL) {
        if( end_chapter_temp == NULL) {
            end_chapter = 99;
        } else if ( start_chapter_temp == NULL) {
            start_chapter = 1;
        }
        if ( end_chapter < start_chapter ) {
            usage();
        }
    }

    if ( titles_temp != NULL
         && ((end_chapter_temp != NULL) || (start_chapter_temp != NULL))) {
        do_chapter = 1;
    } else if ((titles_temp != NULL)
               && ((end_chapter_temp == NULL) && (start_chapter_temp == NULL))) {
        do_titles=1;
    }
    if (do_chapter && (titles_temp == NULL)) {
        titles = 0;
    }

    if ( title_set_temp != NULL ) {
        title_set = atoi(title_set_temp);
        if ( title_set > 99 || title_set < 0 ) {
            usage();
        }
        do_title_set = 1;
    }

    if (do_info + do_titles + do_chapter + do_feature + do_title_set + do_mirror > 1 ) {
        usage();
    } else if (do_info + do_titles + do_chapter + do_feature + do_title_set + do_mirror == 0) {
        usage();
    }
#ifdef DEBUG
    fprintf(stderr,"After args\n");
#endif

    _dvd = DVDOpen(dvd);
    if(!_dvd) exit(-1);

    if (do_info) {
        DVDDisplayInfo(_dvd, dvd);
        DVDClose(_dvd);
        exit(EXIT_SUCCESS);
    }

    if(provided_title_name == NULL) {
        if (DVDGetTitleName(dvd, title_name) != 0) {
            fprintf(stderr,"You must provide a title name when you "
                    "read your DVD-Video structure direct from the HD\n");
            DVDClose(_dvd);
            exit(EXIT_FAILURE);
        }
        if (strstr(title_name, "DVD_VIDEO") != NULL) {
            fprintf(stderr,"The DVD-Video title on the disk is "
                    "DVD_VIDEO which is to generic please "
                    "provide a title with the -n switch\n");
            DVDClose(_dvd);
            exit(2);
        }

    } else {
        if (strlen(provided_title_name) > 32) {
            fprintf(stderr,"The title name specified is "
                    "longer than 32 charachters, truncating the title name\n");
            strncpy(title_name,provided_title_name, 32);
            title_name[32]='\0';
        } else {
            strcpy(title_name,provided_title_name);
        }
    }

    sprintf(targetname,"%s",targetdir);

    if (stat(targetname, &fileinfo) == 0) {
        if (! S_ISDIR(fileinfo.st_mode)) {
            fprintf(stderr,"The target directory is not valid, "
                    "it may be a ordinary file\n");
        }
    } else {
        if (mkdir(targetname, S_IRUSR | S_IWUSR | S_IXUSR) != 0) {
            fprintf(stderr,"Failed creating target directory\n");
            perror("");
            DVDClose(_dvd);
            exit(-1);
        }
    }

    sprintf(targetname,"%s/%s",targetdir, title_name);

    if (stat(targetname, &fileinfo) == 0) {
        if (! S_ISDIR(fileinfo.st_mode)) {
            fprintf(stderr,"The title directory is not valid, "
                    "it may be a ordinary file\n");
        }
    } else {
        if (mkdir(targetname, S_IRUSR | S_IWUSR | S_IXUSR) != 0) {
            fprintf(stderr,"Failed creating title directory\n");
            perror("");
            DVDClose(_dvd);
            exit(-1);
        }
    }

    sprintf(targetname,"%s/%s/VIDEO_TS",targetdir, title_name);

    if (stat(targetname, &fileinfo) == 0) {
        if (! S_ISDIR(fileinfo.st_mode)) {
            fprintf(stderr,"The VIDEO_TS directory is not valid, "
                    "it may be a ordinary file\n");
        }
    } else {
        if (mkdir(targetname, S_IRUSR | S_IWUSR | S_IXUSR) != 0) {
            fprintf(stderr,"Failed creating VIDEO_TS directory\n");
            perror("");
            DVDClose(_dvd);
            exit(-1);
        }
    }

#ifdef DEBUG
    fprintf(stderr,"After dirs\n");
#endif

    if(do_mirror) {
        if ( DVDMirror(_dvd, targetdir, title_name)  != 0 ) {
            fprintf(stderr, "Mirror of DVD failed\n");
            return_code = EXIT_FAILURE;
        } else {
            return_code = EXIT_SUCCESS;
        }
    }
#ifdef DEBUG
    fprintf(stderr,"After Mirror\n");
#endif

    if (do_title_set) {
        if (DVDMirrorTitleSet(_dvd, targetdir, title_name, title_set) != 0) {
            fprintf(stderr, "Mirror of title set %d failed\n", title_set);
            return_code = EXIT_FAILURE;
        } else {
            return_code = EXIT_SUCCESS;
        }

    }
#ifdef DEBUG
    fprintf(stderr,"After Title Set\n");
#endif

    if(do_feature) {
        if ( DVDMirrorMainFeature(_dvd, targetdir, title_name)  != 0 ) {
            fprintf(stderr, "Mirror of main feature film of DVD failed\n");
            return_code = EXIT_FAILURE;
        } else {
            return_code = EXIT_SUCCESS;
        }
    }

    if(do_titles) {
        if (DVDMirrorTitles(_dvd, targetdir, title_name, titles) != 0) {
            fprintf(stderr, "Mirror of title  %d failed\n", titles);
            return_code = EXIT_FAILURE;
        } else {
            return_code = EXIT_SUCCESS;
        }
    }

    if(do_chapter) {
        if (DVDMirrorChapters(_dvd, targetdir, title_name,
                              start_chapter, end_chapter, titles) != 0) {
            fprintf(stderr, "Mirror of chapters %d to %d in title %d failed\n",
                    start_chapter, end_chapter, titles);
            return_code = EXIT_FAILURE;
        } else {
            return_code = EXIT_SUCCESS;
        }
    }

    DVDClose(_dvd);
    exit(return_code);
}
