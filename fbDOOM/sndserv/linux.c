// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id: linux.c,v 1.3 1997/01/26 07:45:01 b1 Exp $
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
//
// $Log: linux.c,v $
// Revision 1.3  1997/01/26 07:45:01  b1
// 2nd formatting run, fixed a few warnings as well.
//
// Revision 1.2  1997/01/21 19:00:01  b1
// First formatting run:
//  using Emacs cc-mode.el indentation for C++ now.
//
// Revision 1.1  1997/01/19 17:22:45  b1
// Initial check in DOOM sources as of Jan. 10th, 1997
//
//
// DESCRIPTION:
//	UNIX, soundserver for Linux i386.
//
//-----------------------------------------------------------------------------

static const char rcsid[] = "$Id: linux.c,v 1.3 1997/01/26 07:45:01 b1 Exp $";


#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

#include <linux/soundcard.h>
#include <sys/ioctl.h>

#include <errno.h>

#include "soundsrv.h"

int	audio_fd;

void
myioctl
( int	fd,
  int	command,
  int*	arg )
{   
    int		rc;
    
    rc = ioctl(fd, command, arg);  
    if (rc < 0)
    {
	fprintf(stderr, "ioctl(dsp,%d,arg) failed\n", command);
	fprintf(stderr, "errno=%d\n", errno);
	exit(-1);
    }
}

void I_InitMusic(void)
{
}


#define PCM_DEVICE "default"


snd_pcm_t *pcm_handle;
snd_pcm_hw_params_t *params;
snd_pcm_sw_params_t *sparams;
unsigned int err;
snd_pcm_uframes_t buffer_size;
snd_pcm_uframes_t period_size;

void
I_InitSound
( int	samplerate,
  int	samplesize )
{
    unsigned int tmp;
    unsigned int reqrate = SPEED;
    unsigned int rrate = SPEED;
    int dir;
    // unsigned int buffer_time = 160000;
    // unsigned int period_time = 40000;
    period_size = 512;
    buffer_size = period_size*4;
    int period_event = 0;

        /* Open the PCM device in playback mode */
        if (err = snd_pcm_open(&pcm_handle, PCM_DEVICE,
                        SND_PCM_STREAM_PLAYBACK, 0) < 0) 
            printf("ERROR: Can't open \"%s\" PCM device. %s\n",
                        PCM_DEVICE, snd_strerror(err));

    snd_pcm_hw_params_alloca(&params);
    snd_pcm_sw_params_alloca(&sparams);
            
    /* choose all parameters */
    err = snd_pcm_hw_params_any(pcm_handle, params);
    if (err < 0) {
        printf("Broken configuration for playback: no configurations available: %s\n", snd_strerror(err));
        return;
    }
    /* set resampling */
    err = snd_pcm_hw_params_set_rate_resample(pcm_handle, params, 1);
    if (err < 0) {
        printf("Resampling setup failed for playback: %s\n", snd_strerror(err));
        return;
    }
    /* set the interleaved read/write format */
    err = snd_pcm_hw_params_set_access(pcm_handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    if (err < 0) {
        printf("Access type not available for playback: %s\n", snd_strerror(err));
        return;
    }
    /* set the sample format */
    err = snd_pcm_hw_params_set_format(pcm_handle, params, FMT);
    if (err < 0) {
        printf("Sample format not available for playback: %s\n", snd_strerror(err));
        return;
    }
    /* set the count of channels */
    err = snd_pcm_hw_params_set_channels(pcm_handle, params, CHNUM);
    if (err < 0) {
        printf("Channels count (%u) not available for playbacks: %s\n", CHNUM, snd_strerror(err));
        return;
    }
    /* set the stream rate */
    err = snd_pcm_hw_params_set_rate_near(pcm_handle, params, &reqrate, 0);
    if (err < 0) {
        printf("Rate %uHz not available for playback: %s\n", reqrate, snd_strerror(err));
        return;
    }
    if (rrate != reqrate) {
        printf("Rate doesn't match (requested %uHz, get %iHz)\n", reqrate, rrate);
        return;
    }
    /* set the buffer time */
    // err = snd_pcm_hw_params_set_buffer_time_near(pcm_handle, params, &buffer_time, &dir);
    // if (err < 0) {
    //     printf("Unable to set buffer time %u for playback: %s\n", buffer_time, snd_strerror(err));
    //     return;
    // }
    err = snd_pcm_hw_params_set_buffer_size_near(pcm_handle, params, &buffer_size);
    if (err < 0) {
        printf("Unable to set buffer size %lu for playback: %s\n", buffer_size, snd_strerror(err));
        return;
    }
    err = snd_pcm_hw_params_get_buffer_size(params, &buffer_size);
    if (err < 0) {
        printf("Unable to get buffer size for playback: %s\n", snd_strerror(err));
        return;
    }
    /* set the period time */
    // err = snd_pcm_hw_params_set_period_time_near(pcm_handle, params, &period_time, &dir);
    // if (err < 0) {
        // printf("Unable to set period time %u for playback: %s\n", period_time, snd_strerror(err));
        // return;
    // }
    err = snd_pcm_hw_params_set_period_size_near(pcm_handle, params, &period_size, &dir);
    if (err < 0) {
        printf("Unable to set period size %lu for playback: %s\n", period_size, snd_strerror(err));
        return;
    }
    err = snd_pcm_hw_params_get_period_size(params, &period_size, &dir);
    if (err < 0) {
        printf("Unable to get period size for playback: %s\n", snd_strerror(err));
        return;
    }
    /* write the parameters to device */
    err = snd_pcm_hw_params(pcm_handle, params);
    if (err < 0) {
        printf("Unable to set hw params for playback: %s\n", snd_strerror(err));
        return;
    }
    /* get the current swparams */
    err = snd_pcm_sw_params_current(pcm_handle, sparams);
    if (err < 0) {
        printf("Unable to determine current swparams for playback: %s\n", snd_strerror(err));
        return;
    }
    /* start the transfer when the buffer is almost full: */
    /* (buffer_size / avail_min) * avail_min */
    err = snd_pcm_sw_params_set_start_threshold(pcm_handle, sparams, (buffer_size / period_size) * period_size);
    if (err < 0) {
        printf("Unable to set start threshold mode for playback: %s\n", snd_strerror(err));
        return;
    }
    /* allow the transfer when at least period_size samples can be processed */
    /* or disable this mechanism when period event is enabled (aka interrupt like style processing) */
    err = snd_pcm_sw_params_set_avail_min(pcm_handle, sparams, period_event ? buffer_size : period_size);
    if (err < 0) {
        printf("Unable to set avail min for playback: %s\n", snd_strerror(err));
        return;
    }
    /* enable period events when requested */
    if (period_event) {
        err = snd_pcm_sw_params_set_period_event(pcm_handle, sparams, 1);
        if (err < 0) {
            printf("Unable to set period event: %s\n", snd_strerror(err));
            return;
        }
    }
    /* write the parameters to the playback device */
    err = snd_pcm_sw_params(pcm_handle, sparams);
    if (err < 0) {
        printf("Unable to set sw params for playback: %s\n", snd_strerror(err));
        return;
    }
 
            
        // if ((err = snd_pcm_set_params(pcm_handle,
        //                 SND_PCM_FORMAT_S16_LE,
        //                 SND_PCM_ACCESS_RW_INTERLEAVED,
        //                 1,
        //                 SPEED,
        //                 1,
        //                 200000)) < 0) {
        //     printf("Playback setparm error: %s\n", snd_strerror(err));
        //     exit(EXIT_FAILURE);
        // }
 
 
//         snd_pcm_hw_params_alloca(&params);
//         snd_pcm_hw_params_current(pcm_handle, params);
//  
//         snd_pcm_hw_params_get_period_size(params, &seconds, 0);
//         snd_pcm_hw_params_get_buffer_size(params, &buffer_size);

        // printf("bt %u pt %u pers %lu buffs %lu\n", buffer_time, period_time, period_size, buffer_size);
        printf("pers %lu buffs %lu\n", period_size, buffer_size);
        
        // uint16_t samples[buffer_size/2];
        
        // for(int i = 0; i < buffer_size/2; i++) {
            // samples[i] = ((i/50)%2==0) ? -0x7fff : 0x7fff;
        // }

        // while(1)
            // snd_pcm_writei(pcm_handle, samples, buffer_size/2);

        
//     int i;
//                 
//     audio_fd = open("/dev/dsp", O_WRONLY);
//     if (audio_fd<0)
//         fprintf(stderr, "Could not open /dev/dsp\n");
//          
//                      
//     i = 11 | (2<<16);                                           
//     myioctl(audio_fd, SNDCTL_DSP_SETFRAGMENT, &i);
//                     
//     myioctl(audio_fd, SNDCTL_DSP_RESET, 0);
//     i=samplerate;
//     myioctl(audio_fd, SNDCTL_DSP_SPEED, &i);
//     i=1;    
//     myioctl(audio_fd, SNDCTL_DSP_STEREO, &i);
//             
//     myioctl(audio_fd, SNDCTL_DSP_GETFMTS, &i);
//     if (i&=AFMT_S16_LE)    
//         myioctl(audio_fd, SNDCTL_DSP_SETFMT, &i);
//     else
//         fprintf(stderr, "Could not play signed 16 data\n");

    
    
}

void
I_SubmitOutputBuffer
( void *	samples,
  int	samplecount )
{
    // signed short *ptr;
    // int cptr;
    // ptr = samples;
    // cptr = samplecount;
    // 
    // while (cptr > 0) {
    //     err = snd_pcm_writei(pcm_handle, ptr, cptr);
    //     if (err < 0) {
    //         printf("Write error: %s\n", snd_strerror(err));
    //         exit(EXIT_FAILURE);
    //         init = 1;
    //         break;  /* skip one period */
    //     }
    //     if (snd_pcm_state(pcm_handle) == SND_PCM_STATE_RUNNING)
    //         init = 0;
    //     ptr += err * CHNUM;
    //     cptr -= err;
    //     if (cptr == 0)
    //         break;
    // }

    // write(audio_fd, samples, samplecount*4);
    // printf("buff %d\n", samplecount);
    signed short *ptr;
    int cptr;
    ptr = samples;
    cptr = samplecount;
    while (cptr > 0) {
        err = snd_pcm_writei(pcm_handle, ptr, cptr);
        if (err == -EAGAIN)
            continue;
        if (err < 0) {
            if (err == -EPIPE) {    /* under-run */
                printf("U\n");
                err = snd_pcm_prepare(pcm_handle);
                if (err < 0)
                    printf("Can't recovery from underrun, prepare failed: %s\n", snd_strerror(err));
            } else if (err == -ESTRPIPE) {
                while ((err = snd_pcm_resume(pcm_handle)) == -EAGAIN)
                    sleep(1);   /* wait until the suspend flag is released */
                if (err < 0) {
                    err = snd_pcm_prepare(pcm_handle);
                    if (err < 0)
                        printf("Can't recovery from suspend, prepare failed: %s\n", snd_strerror(err));
                }
            } else {
                printf("Write error: %s\n", snd_strerror(err));
                exit(EXIT_FAILURE);
            }
            break;  /* skip one period */
        }
        ptr += err * 1;
        cptr -= err;
    }
}

void I_ShutdownSound(void)
{

    // close(audio_fd);
    snd_pcm_drain(pcm_handle);
	snd_pcm_close(pcm_handle);

}

void I_ShutdownMusic(void)
{
}
