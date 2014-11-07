#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <dvdread/ifo_read.h>
#include "dvd_info.h"
#include "dvd_track.h"
#include "dvd_audio.h"

/** Audio Streams **/

/**
 * Get the number of audio streams for a track
 *
 * @param vts_ifo dvdread track IFO handler
 * @return number of audio streams
 */
uint8_t dvd_track_audio_tracks(const ifo_handle_t *vts_ifo) {

	uint8_t audio_streams = vts_ifo->vtsi_mat->nr_of_vts_audio_streams;

	if(audio_streams >= 0)
		return audio_streams;
	else
		return 0;

}

/**
 * Examine the PGC for the track IFO directly and see if there are any audio
 * control entries marked as active.  This is an alternative way of checking
 * for the number of audio streams, compared to looking at the VTS directly.
 * This is useful for debugging, and flushing out either badly mastered DVDs or
 * getting a closer identifier of how many streams this has.
 *
 * Some software uses this number of audio streams in the pgc instead of the
 * one in the VTSI MAT, such as mplayer and HandBrake, which will skip over
 * the other ones completely.
 *
 * @param vmg_ifo dvdread track IFO handler
 * @param vts_ifo dvdread track IFO handler
 * @param title_track track number
 * @return number of PGC audio streams marked as active
 */
uint8_t dvd_audio_active_tracks(const ifo_handle_t *vmg_ifo, const ifo_handle_t *vts_ifo, const uint16_t title_track) {

	pgcit_t *vts_pgcit = vts_ifo->vts_pgcit;
	uint8_t ttn = dvd_track_ttn(vmg_ifo, title_track);
	uint16_t pgcn = vts_ifo->vts_ptt_srpt->title[ttn - 1].ptt[0].pgcn;
	pgc_t *pgc = vts_pgcit->pgci_srp[pgcn - 1].pgc;
	uint8_t idx = 0;
	uint8_t audio_tracks = 0;

	for(idx = 0; idx < DVD_AUDIO_STREAM_LIMIT; idx++) {

		if(pgc->audio_control[idx] & 0x8000)
			audio_tracks++;

	}

	return audio_tracks;

}

/**
 * Look through the program chain to see if an audio track is flagged as
 * active or not.
 *
 * There is no fixed order to active, inactive tracks relative to each other.
 * The first can be inactive, the next active, etc.
 *
 * @param vmg_ifo dvdread track IFO handler
 * @param vts_ifo dvdread track IFO handler
 * @param title_track track number
 * @param audio_track audio track
 * @return boolean
 */
uint8_t dvd_audio_active(const ifo_handle_t *vmg_ifo, const ifo_handle_t *vts_ifo, const uint16_t title_track, const uint8_t audio_track) {

	uint8_t audio_tracks = dvd_track_audio_tracks(vts_ifo);

	if(audio_track > DVD_AUDIO_STREAM_LIMIT || audio_track > audio_tracks)
		return 0;

	pgcit_t *vts_pgcit = vts_ifo->vts_pgcit;
	uint8_t ttn = dvd_track_ttn(vmg_ifo, title_track);
	uint16_t pgcn = vts_ifo->vts_ptt_srpt->title[ttn - 1].ptt[0].pgcn;
	pgc_t *pgc = vts_pgcit->pgci_srp[pgcn - 1].pgc;

	if(pgc->audio_control[audio_track - 1] & 0x8000)
		return 1;

	return 0;

}

/**
 * Get the number of audio streams for a specific language
 *
 * @param vts_ifo dvdread track IFO handler
 * @param lang_code language code
 * @return number of subtitles
 */
uint8_t dvd_track_num_audio_lang_code_streams(const ifo_handle_t *vts_ifo, const char *lang_code) {

	uint8_t audio_tracks = dvd_track_audio_tracks(vts_ifo);
	uint8_t language_tracks = 0;
	char str[DVD_AUDIO_LANG_CODE + 1] = {'\0'};
	uint8_t i = 0;

	for(i = 0; i < audio_tracks; i++) {

		strncpy(str, dvd_audio_lang_code(vts_ifo, i), DVD_AUDIO_LANG_CODE);

		if(strncmp(str, lang_code, DVD_AUDIO_LANG_CODE) == 0)
			language_tracks++;

	}

	return language_tracks;

}

/**
 * Check if a DVD track has a specific audio language
 *
 * @param vts_ifo dvdread track IFO handler
 * @param lang_code language code
 * @return boolean
 */
bool dvd_track_has_audio_lang_code(const ifo_handle_t *vts_ifo, const char *lang_code) {

	if(dvd_track_num_audio_lang_code_streams(vts_ifo, lang_code) > 0)
		return true;
	else
		return false;

}

/**
 * Get the codec for an audio track.
 *
 * FIXME see dvdread/ifo_print.c:196 for how it handles mpeg2ext with drc, no drc,
 * same for lpcm; also a bug if it reports #5, and defaults to bug report if
 * any above 6 (or under 0)
 * FIXME check for multi channel extension
 *
 * Possible values: AC3, MPEG1, MPEG2, LPCM, SDDS, DTS
 * @param vts_ifo dvdread track IFO handler
 * @param audio_stream audio track number
 * @return audio codec
 */
const char *dvd_audio_codec(const ifo_handle_t *vts_ifo, const uint8_t audio_stream) {

	const char *audio_codecs[7] = { "ac3", "", "mpeg1", "mpeg2", "lpcm", "sdds", "dts" };
	audio_attr_t *audio_attr =  &vts_ifo->vtsi_mat->vts_audio_attr[audio_stream];
	uint8_t audio_codec = audio_attr->audio_format;

	return strndup(audio_codecs[audio_codec], DVD_AUDIO_CODEC);

}

/**
 * Get the number of channels for an audio track
 *
 * Human-friendly interpretation of what channels are:
 * 1: mono
 * 2: stereo
 * 3: 2.1, stereo and subwoofer
 * 4: front right, front left, rear right, rear left
 * 5: front right, front left, rear right, rear left, subwoofer
 *
 * @param vts_ifo dvdread track IFO handler
 * @param audio_track audio track number
 * @return num channels
 */
uint8_t dvd_audio_channels(const ifo_handle_t *vts_ifo, const uint8_t audio_track) {

	audio_attr_t *audio_attr = &vts_ifo->vtsi_mat->vts_audio_attr[audio_track];
	uint8_t channels = audio_attr->channels + 1;

	if(channels >= 0)
		return channels;
	else
		return 0;

}

/**
 * Get the stream ID for an audio track
 *
 * Examples: 128, 129, 130
 *
 * @param vts_ifo dvdread track IFO handler
 * @param audio_track audio track number
 * @return audio stream id
 */
const char *dvd_audio_stream_id(const ifo_handle_t *vts_ifo, const uint8_t audio_track) {

	audio_attr_t *audio_attr = &vts_ifo->vtsi_mat->vts_audio_attr[audio_track];
	uint8_t audio_format = audio_attr->audio_format;
	uint8_t audio_id[7] = {0x80, 0, 0xC0, 0xC0, 0xA0, 0, 0x88};
	uint8_t audio_stream_id = audio_id[audio_format] + audio_track;
	char str[DVD_AUDIO_STREAM_ID + 1] = {'\0'};

	snprintf(str, DVD_AUDIO_STREAM_ID + 1, "0x%x", audio_stream_id);

	return strndup(str, DVD_AUDIO_STREAM_ID);

}

/**
 * Get the audio language code for a track.  A two-character string that is a
 * short name for a language.
 *
 * Note: Remember that the language code is set in the IFO
 * See dvdread/ifo_print.c for same functionality (error checking)
 *
 * DVD specification says that there is an ISO-639 character code here:
 * - http://stnsoft.com/DVD/ifo_vts.html
 * - http://www.loc.gov/standards/iso639-2/php/code_list.php
 * lsdvd uses 'und' (639-2) for undetermined if the lang_code and
 * lang_extension are both 0.
 *
 * Examples: en: English, fr: French, es: Spanish
 *
 * @param vts_ifo dvdread track IFO handler
 * @param audio_stream audio track number
 * @return language code
 */
const char *dvd_audio_lang_code(const ifo_handle_t *vts_ifo, const uint8_t audio_stream) {

	audio_attr_t *audio_attr = &vts_ifo->vtsi_mat->vts_audio_attr[audio_stream];
	uint8_t lang_type = audio_attr->lang_type;

	if(lang_type != 1)
		return "";

	char lang_code[3] = {'\0'};

	snprintf(lang_code, DVD_AUDIO_LANG_CODE + 1, "%c%c", audio_attr->lang_code >> 8, audio_attr->lang_code & 0xff);

	return strndup(lang_code, DVD_AUDIO_LANG_CODE);

}