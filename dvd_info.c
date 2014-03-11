#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdbool.h>
#include <getopt.h>
#include <linux/cdrom.h>
#include <dvdread/dvd_reader.h>
#include <dvdread/dvd_udf.h>
#include <dvdread/ifo_read.h>
#include "dvd_device.h"
#include "dvd_drive.h"

void print_usage(char *binary) {

	printf("Usage %s [options] [-t track_number] [dvd path]\n", binary);
	printf("\n");
	printf("Display DVD info:\n");
	printf("  --all			Display all\n");
	printf("  --id			Unique DVD identifier\n");
	printf("  --title		DVD title\n");
	printf("  --num_tracks		Number of tracks\n");
	printf("  --num_vts		Number of VTSs\n");
	printf("  --provider_id 	Provider ID\n");
	printf("  --vmg_id		VMG ID\n");

}

int dvd_info_title(char* device_filename, char *dvd_title) {

	char title[33];
	FILE* filehandle = 0;
	int x, y, z;

	filehandle = fopen(device_filename, "r");
	if(filehandle == NULL) {
		return 1;
	}

	if(fseek(filehandle, 32808, SEEK_SET) == -1) {
		fclose(filehandle);
		return 2;
	}

	x = fread(title, 1, 32, filehandle);
	if(x == 0) {
		fclose(filehandle);
		return 3;
	}
	title[32] = '\0';

	fclose(filehandle);

	y = sizeof(title);
	while(y-- > 2) {
		if(title[y] == ' ') {
			title[y] = '\0';
		}
	}

	strncpy(dvd_title, title, 32);

	return 0;

}

int main(int argc, char **argv) {

	int dvd_fd;
	int drive_status;
	int track_number = 0;
	bool is_hardware;
	bool is_image;
	bool ready = false;
	bool verbose = false;
	char* device_filename = "/dev/dvd";
	char* status;

	// getopt_long
	int display_all = 0;
	int display_id = 0;
	int display_title = 0;
	int display_num_tracks = 0;
	int display_num_vts = 0;
	int display_provider_id = 0;
	int display_vmg_id = 0;
	int long_index = 0;
	int opt;
	// Suppress getopt sending 'invalid argument' to stderr
	// opterr = 0;
	struct option long_options[] = {
		{ "device", required_argument, 0, 'i' },
		{ "track", required_argument, 0, 't' },

		{ "verbose", no_argument, 0, 'v' },

		{ "all", no_argument, & display_all, 1 },
		{ "id", no_argument, & display_id, 1 },
		{ "title", no_argument, & display_title, 1 },
		{ "num_tracks", no_argument, & display_num_tracks, 1 },
		{ "num_vts", no_argument, & display_num_vts, 1 },
		{ "provider_id", no_argument, & display_provider_id, 1 },
		{ "vmg_id", no_argument, & display_vmg_id, 1 },

		{ 0, 0, 0, 0 }
	};

	while((opt = getopt_long(argc, argv, "hi:t:v", long_options, &long_index )) != -1) {

		switch(opt) {

			case 'h':
				print_usage(argv[0]);
				return 0;

			case 'i':
				device_filename = optarg;
				break;

			case 't':
				track_number = 0;
				break;

			case 'v':
				verbose = true;
				break;

			// ignore unknown arguments
			case '?':
				break;

			// let getopt_long set the variable
			case 0:
				break;

			default:
				break;
		}
	}

	if (argv[optind]) {
		device_filename = argv[optind];
	}

	if(verbose)
		printf("dvd: %s\n", device_filename);

	// Handle options
	if(track_number < 1)
		track_number = 0;


	/** Begin dvd_info :) */

	// Check to see if device can be accessed
	if(dvd_device_access(device_filename) == 1) {
		fprintf(stderr, "cannot access %s\n", device_filename);
		return 1;
	}

	// Check to see if device can be opened
	dvd_fd = dvd_device_open(device_filename);
	if(dvd_fd < 0) {
		fprintf(stderr, "error opening %s\n", device_filename);
		return 1;
	}
	dvd_device_close(dvd_fd);

	// Check if it is hardware or an image file
	is_hardware = dvd_device_is_hardware(device_filename);
	is_image = dvd_device_is_image(device_filename);

	if(is_image)
		ready = true;

	// Poll drive status if it is hardware
	if(is_hardware)
		drive_status = dvd_drive_get_status(device_filename);

	if(is_hardware)
		dvd_drive_display_status(device_filename);

	// Wait for the drive to become ready
	if(is_hardware) {
		if(!dvd_drive_has_media(device_filename)) {
			printf("waiting for drive to become ready\n");
			while(!dvd_drive_has_media(device_filename)) {
				usleep(1000000);
			}
		}
	}

	// libdvdread

	// open DVD device and don't cache queries
	dvd_reader_t *dvd;
	dvd = DVDOpen(device_filename);
	// DVDUDFCacheLevel(dvd, 0);

	// Open IFO zero
	ifo_handle_t *ifo_zero;
	ifo_zero = ifoOpen(dvd, 0);
	if(!ifo_zero) {
		fprintf(stderr, "dvd_info: opening IFO zero failed\n");
	}

	// --id
	// Display DVDDiscID from libdvdread
	if(display_id || display_all) {

		int dvd_disc_id;
		unsigned char tmp_buf[16];

		dvd_disc_id = DVDDiscID(dvd, tmp_buf);

		if(dvd_disc_id == -1) {
			fprintf(stderr, "dvd_info: querying DVD id failed\n");
		} else {

			if(verbose)
				printf("id: ");

			for(int x = 0; x < sizeof(tmp_buf); x++) {
				printf("%02x", tmp_buf[x]);
			}
			printf("\n");

		}

	}

	// --num_tracks
	if((display_num_tracks || display_all) && ifo_zero) {

		int num_tracks;

		num_tracks = ifo_zero->tt_srpt->nr_of_srpts;

		if(verbose)
			printf("num_tracks: ");
		printf("%i\n", num_tracks);

	}

	// --num_vts
	// Display number of VTSs on DVD
	if((display_num_vts || display_all) && ifo_zero) {

		int num_vts;

		num_vts = ifo_zero->vts_atrt->nr_of_vtss;

		if(verbose)
			printf("num_vts: ");
		printf("%i\n", num_vts);

	} else if((display_num_vts || display_all) && !ifo_zero) {

		fprintf(stderr, "dvd_info: cannot display num_vts\n");

	}

	// --provider_id
	// Display provider ID
	if((display_provider_id || display_all) && ifo_zero) {

		char *provider_id;
		bool has_provider_id = false;

		provider_id = ifo_zero->vmgi_mat->provider_identifier;

		if(verbose)
			printf("provider_id: ");
		printf("%s\n", provider_id);

		// Having an empty provider ID is very common.
		if(provider_id[0] != '\0')
			has_provider_id = true;

	} else if((display_provider_id || display_all) && !ifo_zero) {

		fprintf(stderr, "dvd_info: cannot display provider_id\n");

	}

	// --title
	// Display DVD title
	if(display_title || display_all) {

		char dvd_title[33];
		int dvd_info_title_ret;

		dvd_info_title_ret = dvd_info_title(device_filename, dvd_title);

		if(dvd_info_title_ret == 1) {
			fprintf(stderr, "dvd_info: could not open device %s for reading\n", device_filename);
		} else if(dvd_info_title_ret == 2) {
			fprintf(stderr, "dvd_info: could not seek on device %s\n", device_filename);
		} else if(dvd_info_title_ret == 3) {
			fprintf(stderr, "dvd_info: could not read device %s\n", device_filename);
		} else {
			if(verbose)
				printf("title: ");
			printf("%s\n", dvd_title);
		}

	}

	/**
	 * Display VMG_ID
	 *
	 * It's entirely possible, and common, that the string is blank.  If it's not
	 * blank, it is probably 'DVDVIDEO-VMG'.
	 *
	 */
	if((display_vmg_id || display_all) && ifo_zero) {

		char *vmg_id;

		vmg_id = ifo_zero->vmgi_mat->vmg_identifier;
		vmg_id[12] = '\0';

		if(verbose)
			printf("vmg_id: ");
		printf("%s\n", vmg_id);

	}

	// Cleanup

	if(ifo_zero)
		ifoClose(ifo_zero);

	if(dvd)
		DVDClose(dvd);

}