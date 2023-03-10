/***********************************************************************
Copyright (c) 2006-2011, Skype Limited. All rights reserved.
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
- Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in the
documentation and/or other materials provided with the distribution.
- Neither the name of Internet Society, IETF or IETF Trust, nor the
names of specific contributors, may be used to endorse or promote
products derived from this software without specific prior written
permission.
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
***********************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef FEATURES
#include <stdio.h>
#include <stdlib.h>
#endif

#include "main.h"
#include "stack_alloc.h"
#include "PLC.h"

/****************/
/* Decode frame */
/****************/
opus_int silk_decode_frame(
    silk_decoder_state          *psDec,                         /* I/O  Pointer to Silk decoder state               */
    ec_dec                      *psRangeDec,                    /* I/O  Compressor data structure                   */
    opus_int16                  pOut[],                         /* O    Pointer to output speech frame              */
    opus_int32                  *pN,                            /* O    Pointer to size of output frame             */
    opus_int                    lostFlag,                       /* I    0: no loss, 1 loss, 2 decode fec            */
    opus_int                    condCoding,                     /* I    The type of conditional coding to use       */
    int                         arch                            /* I    Run-time architecture                       */
)
{
    VARDECL( silk_decoder_control, psDecCtrl );
    opus_int         L, mv_len, ret = 0;
#ifdef FEATURES
    opus_int32 num_bits;
    static float num_bits_smooth = -1;
    static FILE* f_numbits = NULL;
    static FILE* f_numbits_smooth = NULL;
#endif
    SAVE_STACK;

#ifdef FEATURES
    if(f_numbits == NULL) {f_numbits = fopen("features_num_bits.s32", "wb");}
    if (f_numbits_smooth == NULL) {f_numbits_smooth = fopen("features_num_bits_smooth.f32", "wb");}
#endif

    L = psDec->frame_length;
    ALLOC( psDecCtrl, 1, silk_decoder_control );
    psDecCtrl->LTP_scale_Q14 = 0;

    /* Safety checks */
    celt_assert( L > 0 && L <= MAX_FRAME_LENGTH );

    if(   lostFlag == FLAG_DECODE_NORMAL ||
        ( lostFlag == FLAG_DECODE_LBRR && psDec->LBRR_flags[ psDec->nFramesDecoded ] == 1 ) )
    {
        VARDECL( opus_int16, pulses );
        ALLOC( pulses, (L + SHELL_CODEC_FRAME_LENGTH - 1) &
                       ~(SHELL_CODEC_FRAME_LENGTH - 1), opus_int16 );
#ifdef FEATURES
        num_bits = ec_tell(psRangeDec);
#endif
        /*********************************************/
        /* Decode quantization indices of side info  */
        /*********************************************/
        silk_decode_indices( psDec, psRangeDec, psDec->nFramesDecoded, lostFlag, condCoding );

        /*********************************************/
        /* Decode quantization indices of excitation */
        /*********************************************/
        silk_decode_pulses( psRangeDec, pulses, psDec->indices.signalType,
                psDec->indices.quantOffsetType, psDec->frame_length );
#ifdef FEATURES
        num_bits = ec_tell(psRangeDec) - num_bits;
        num_bits_smooth = 0.9 * num_bits_smooth + 0.1 * num_bits;
        fwrite(&num_bits, sizeof(num_bits), 1, f_numbits);
        fwrite(&num_bits_smooth, sizeof(num_bits_smooth), 1, f_numbits_smooth);
#endif

        /********************************************/
        /* Decode parameters and pulse signal       */
        /********************************************/
        silk_decode_parameters( psDec, psDecCtrl, condCoding );

        /********************************************************/
        /* Run inverse NSQ                                      */
        /********************************************************/
        silk_decode_core( psDec, psDecCtrl, pOut, pulses, arch );

        /********************************************************/
        /* Update PLC state                                     */
        /********************************************************/
        silk_PLC( psDec, psDecCtrl, pOut, 0, arch );

        psDec->lossCnt = 0;
        psDec->prevSignalType = psDec->indices.signalType;
        celt_assert( psDec->prevSignalType >= 0 && psDec->prevSignalType <= 2 );

        /* A frame has been decoded without errors */
        psDec->first_frame_after_reset = 0;
    } else {
        /* Handle packet loss by extrapolation */
        printf("packet lost!");
        exit(1);
        silk_PLC( psDec, psDecCtrl, pOut, 1, arch );
    }

    /*************************/
    /* Update output buffer. */
    /*************************/
    celt_assert( psDec->ltp_mem_length >= psDec->frame_length );
    mv_len = psDec->ltp_mem_length - psDec->frame_length;
    silk_memmove( psDec->outBuf, &psDec->outBuf[ psDec->frame_length ], mv_len * sizeof(opus_int16) );
    silk_memcpy( &psDec->outBuf[ mv_len ], pOut, psDec->frame_length * sizeof( opus_int16 ) );

    /************************************************/
    /* Comfort noise generation / estimation        */
    /************************************************/
    silk_CNG( psDec, psDecCtrl, pOut, L );

    /****************************************************************/
    /* Ensure smooth connection of extrapolated and good frames     */
    /****************************************************************/
    silk_PLC_glue_frames( psDec, pOut, L );

    /* Update some decoder state variables */
    psDec->lagPrev = psDecCtrl->pitchL[ psDec->nb_subfr - 1 ];

    /* Set output frame length */
    *pN = L;

    RESTORE_STACK;
    return ret;
}
