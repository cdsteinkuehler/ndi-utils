/*
 * SPDX-FileCopyrightText: 2024 Vizrt NDI AB
 * SPDX-License-Identifier: MIT
 */

#include "../ndi_common/stdafx.h"
#include "nditx.h"

#include <chrono>

// Global debug variables, from debug.h
FILE *dbgstream = stderr;
int  debug_level = LOG_ERR;
bool debug_flush = false;

void boilerplate()
{
	// Report the NDI SDK Version
	printf("%s\n", NDIlib_version());
	printf("\n");
}

void arg2rate(char* arg, int* rate_n, int* rate_d)
{
    if(arg)
    {
        char* separator = strchr(arg, '/');

        // See if a range specifier was found
        if (separator == nullptr) {
            // No range specifier, just convert to an integer rate
            *rate_n = strtol(arg, nullptr, 0);
            *rate_d  = 1;
            return;
        }

        // Look for first value
        if (separator == arg) {
            // No first value, leave unchanged
        } else {
            *rate_n = strtol(arg, nullptr, 0);
        }

        // Look for last value
        if (separator == (arg + strlen(arg) - 1)) {
            // No last value, leave unchanged
        } else {
            *rate_d = strtol(separator+1, nullptr, 0);
        }
    }
}

int main(int argc, char* argv[])
{
	// See if we're running from a terminal and can be interactive
	bool interactive = isatty(fileno(stdin));

	// Process command-line options

	// Default options
	int xres = 1920;
	int yres = 1080;
	const char* bitrate = "100";
	const char* shqmode = "auto";
	char* machinename = NULL;
	char* ndiname = NULL;
	int rate_n = 6000;
	int rate_d = 1001;
	NDIlib_source_t ndi_source;
	FILE *infile = stdin;
	bool user_abort = false;
	bool waitconnect = false;
	int num_frames = -1;

	debug_flush = false;
	int temp;

	// Passed on the command line
	int opt;
	while ((opt = getopt(argc, argv, "x:y:r:c:b:s:i:m:n:wvqf")) != -1) {
		switch (opt) {
		// Resolution
		case 'x':
			xres = strtol(optarg, NULL, 0);
			break;
		case 'y':
			yres = strtol(optarg, NULL, 0);
			break;

		// Frame rate
		case 'r':
			arg2rate(optarg, &rate_n, &rate_d);
			break;

		// Input file
		case 'i':
			infile = fopen(optarg, "rb");
			if (infile == NULL) {
				fprintf (stderr, "Cannot open %s for reading!\n", optarg);
				abort();
			}
			// Input is not stdin, enable user abort if we're interactive
			user_abort = interactive;
			break;

		// Frame count
		case 'c':
			temp = strtol(optarg, NULL, 0);
			if (temp > 0) num_frames = temp;
			break;

		// Bitrate
		case 'b':
			bitrate = optarg;
			break;

		// SHQ Mode
		case 's':
			shqmode = optarg;
			break;

		// NDI source name
		case 'n':
			ndiname = optarg;
			break;

		// NDI machine name
		case 'm':
			machinename = optarg;
			break;

		// Wait for connection to start streaming
		case 'w':
			waitconnect = true;
			break;

		// Debugging
		case 'v':	// Increase debugging level
			debug_level++;
			break;
		case 'q':	// Decrease debugging level
			if (debug_level > 0) debug_level--;
			break;
		case 'f':	// fflush() debug messages
			debug_flush = true;
			break;

		default:	// '?'
			fprintf(stderr, "Usage:\n");
			fprintf(stderr, "%s [-x XRes] [-y Yres] [-r framerate-n[/framerate_d]] [-c frame-count] [-b bitrate] [-s SHQ-mode] [-i infile] [-m <machine name>] [-n <NDI name>] [-wvqf]\n", argv[0]);
			fprintf(stderr, "  -x Horizontal resolution (default: 1920)\n");
			fprintf(stderr, "  -y Vertical resolution (default: 1080)\n");
			fprintf(stderr, "  -r Frame rate (default: 6000/1001)\n");
			fprintf(stderr, "  -c Frame count or number of frames to send (default: send until EOF)\n");
			fprintf(stderr, "  -b Bit-rate multiplier (default: 100)\n");
			fprintf(stderr, "  -s SpeedHQ mode: 4:2:0, 4:2:2, or auto (default: auto)\n");
			fprintf(stderr, "  -i Input filename (default: stdin)\n");
			fprintf(stderr, "  -m NDI machine name (default: hostname)\n");
			fprintf(stderr, "  -n NDI stream name (default: %s)\n", argv[0]);
			fprintf(stderr, "  -w Wait for receiver to connect before sending frames and disconnect before exiting (use with -c)\n");
			fprintf(stderr, "  -v Increase debugging output level\n");
			fprintf(stderr, "  -q Decrease debugging output level\n");
			fprintf(stderr, "  -f fflush() after each debug message\n");
			exit(EXIT_FAILURE);
		}
	}

	// It's safe to send some info to stdout
	boilerplate();

	// Setup the NDI sender
	////////////////////////////////////////////////////////////

	// Not required, but "correct" (see the SDK documentation.
	if (!NDIlib_initialize()) throw std::runtime_error("Cannot run NDI!");

	// Configure our sender settings
	NDIlib_send_create_t my_settings;
	my_settings.p_ndi_name = ndiname;
	my_settings.p_groups = nullptr;
	my_settings.clock_video = true;
	my_settings.clock_audio = false;

	// Create a JSON configuration string we can pass to the NDI library
	// These configuration settings can also be done using a file
	// See "Configuration Files" in the SDK documentation for details

	// Opening stanza
	std::string ndi_config;
	ndi_config = R"({ "ndi": {)";

	// Without a valid vendor ID, the Embedded NDI stack runs in demo mode for 30 minutes
	// If you have a valid vendor ID, add it below, eg:
	// ndi_config.append( R"( "vendor": { "name": "My Company", "id": "00000000000000000000000000000000000000000000", } )";

	// If machinename is specified, add it to our JSON config
	if (machinename) {
		ndi_config.append( R"( "machinename": ")" );
		ndi_config.append( machinename );
		ndi_config.append( R"(", )" );
	}

	// Specify codec settings
	ndi_config.append( R"( "codec": { "shq": { "quality": )" );
	ndi_config.append( bitrate );
	ndi_config.append( R"(, "mode": ")" );
	ndi_config.append( shqmode );
	ndi_config.append( R"(" } } )" );

	// Closing braces
	ndi_config.append( R"(} })" );

	LOG(LOG_INFO, "ndi_config: %s\n",  ndi_config.c_str());

	// Create an NDI sender
	NDIlib_send_instance_t ndi_send = NDIlib_send_create_v2(&my_settings, ndi_config.c_str());
	if (!ndi_send) throw std::runtime_error("Cannot create NDI Sender!");

	// Setup to poll stdin to see if read data is available
	pollfd fds[1];
	fds[0].fd = STDIN_FILENO;
	fds[0].events = POLLIN;
	fds[0].revents = 0;

	// We need two NDI frames so we can use asynchronous sending and allow
	// the NDI library to do video compression on a different thread than
	// our thread which is reading the video data
	NDIlib_video_frame_v2_t video_frame[2];

	// Calculate expected line stride and frame size
	int line_stride =  xres * sizeof(uint16_t);
	size_t frame_size = line_stride * yres * 2;

	// Initialize our frame structures with the video settings
	video_frame[0].xres = xres;
	video_frame[0].yres = yres;
	video_frame[0].FourCC = NDIlib_FourCC_video_type_P216;
	video_frame[0].frame_rate_N = rate_n;
	video_frame[0].frame_rate_D = rate_d;
	video_frame[0].picture_aspect_ratio = 16.0/9.0;
	video_frame[0].frame_format_type = NDIlib_frame_format_type_progressive;
	video_frame[0].timecode = NDIlib_send_timecode_synthesize;
	video_frame[0].p_data = NULL;
	video_frame[0].line_stride_in_bytes = line_stride;
	video_frame[0].p_metadata = NULL;
	video_frame[1] = video_frame[0];

	// Allocate our video buffers
	for (int i=0; i<2; i++) {
		video_frame[i].p_data = (uint8_t*) malloc(frame_size);
		if (!video_frame[i].p_data) throw std::runtime_error("Cannot allocate video buffer!");
	}

	// Wait until a receiver connects
	if (waitconnect) {
		int nConnections;
		LOG(LOG_ERR, "Waiting for connection with a receiver. Ctrl+C to cancel.\n");
		do {
			nConnections = NDIlib_send_get_no_connections(ndi_send, 1000);
		} while (nConnections == 0);
	}

	int j = 0;
	while (num_frames != 0)
	{
		// Check for user abort (data available on stdin)
		if (user_abort && (poll(fds, 1, 0) != 0)) {
			break;
		}

		// Read a frame from the input file
		size_t readsize = fread(video_frame[j & 0x01].p_data, 1, frame_size, infile);
		if (readsize != frame_size) {
			LOG(LOG_ERR, "Unable to read from input!\n");
			break;
		}

		// Send the frame to our NDI sender
		NDIlib_send_send_video_v2(ndi_send, &video_frame[j & 0x01]);

		j++;
		if (num_frames > 0) num_frames--;
	}

	// Make sure NDI has sent our last frame
	NDIlib_send_send_video_v2(ndi_send, NULL);

	// Release our video buffers
	for (int i=0; i<2; i++) {
		if (video_frame[i].p_data) free(video_frame[i].p_data);
	}

	// Wait until the receiver disconnects
	int nConnections;
	LOG(LOG_ERR, "Waiting for connection to end. Ctrl+C to cancel.\n");
	do {
		nConnections = NDIlib_send_get_no_connections(ndi_send, 1000);
	} while (nConnections > 0);

	// Destroy the sender
	NDIlib_send_destroy(ndi_send);

	// Not required, but nice
	NDIlib_destroy();

	// Exit
	return 0;
}
