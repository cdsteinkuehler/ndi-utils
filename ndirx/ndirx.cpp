/*
 * SPDX-FileCopyrightText: 2024 Vizrt NDI AB
 * SPDX-License-Identifier: MIT
 */

#include "../ndi_common/stdafx.h"
#include "ndirx.h"

#include <chrono>

// Global debug variables, from debug.h
FILE *dbgstream = stderr;
int  debug_level = LOG_ERR;
bool debug_flush = false;

struct writer
{
	// Constructor and destructor
	writer(NDIlib_recv_instance_t m_ndi_recv, FILE *outfile);
	~writer(void);

	// Start processing thread
	void begin(void);

	// Add a captured frame for processing
	bool add_frame(NDIlib_video_frame_v2_t* frame);

	// Finish processessing all queued frames
	void flush(void);
private:
	// Process frames
	void write_frames(void);

	// NDI Receiver
	NDIlib_recv_instance_t m_ndi_recv;

	// Output file
	FILE *m_outfile;

	// Queue for NDI frames
	queue<NDIlib_video_frame_v2_t> m_ndi_q;

	// The processing thread
	std::thread m_thread;
};

writer::writer(NDIlib_recv_instance_t ndi_recv, FILE *outfile)
	: m_ndi_recv(ndi_recv), m_outfile(outfile)
{
	LOG(LOG_INFO, "writer Constructor\n");
}

writer::~writer(void)
{
	LOG(LOG_INFO, "writer Destructor\n");
}

void writer::begin(void)
{
	// Configure the queue to not drop any frames
	m_ndi_q.set_depth(0);

	// Start a thread to process frames
	m_thread = std::thread(&writer::write_frames, this);
}

bool writer::add_frame(NDIlib_video_frame_v2_t* frame)
{
	// Bail if there is no data!
	if ((frame) && (!frame->p_data))
	{
		LOG(LOG_WARN,"N");	// No data in NDI frame!
		return true;
	}

	// Create a shared pointer we can pass along
	std::shared_ptr<NDIlib_video_frame_v2_t> s_frame;

	// If we were passed a valid frame, copy it into the shared pointer
	// NULL is passed to indicate the decode thread should exit
	if (frame)
	{
		s_frame = std::make_shared<NDIlib_video_frame_v2_t>(*frame);
	}

	// let's add it to the queue!
	return m_ndi_q.push(s_frame);
}

void writer::flush(void)
{
	LOG(LOG_INFO, "Flushing %i elements from queue\n", m_ndi_q.get_depth());

	// Submit an empty frame and wait for the thread to exit
	add_frame(NULL);
	m_thread.join();

	LOG(LOG_INFO, "Queue flushed\n");
}

void writer::write_frames(void)
{
	pthread_setname_np(pthread_self(), "video_decode");
	LOG(LOG_INFO, "writer thread\n");

	// Local temporary variable to hold details of a compressed frame
	std::shared_ptr<NDIlib_video_frame_v2_t> video_frame;

	// Cycle forever, exit when we get sent an empty frame
	while (true)
	{
		// Get a frame to process from the queue
		video_frame = m_ndi_q.pop();

		// Indicate frame "popped" from the queue
		LOG(LOG_DBG, "p");	// Popped video frame from NDI queue

		// An empty frame is submitted as a signal to exit the thread
		if (!video_frame) break;

		// Calculate expected line stride and frame size
		int line_stride =  video_frame->xres * sizeof(uint16_t);
		size_t frame_size = line_stride * video_frame->yres * 2;

		// Sanity check, we don't currently handle non-packed line stride
		if (line_stride != video_frame->line_stride_in_bytes) {
			LOG(LOG_ERR, "%i:%i\n", line_stride, video_frame->line_stride_in_bytes);
			throw std::runtime_error("Unsupported line stride!");
		}

		// Write video data
		size_t wlen = fwrite(video_frame->p_data, 1, frame_size, m_outfile);
		if (wlen != frame_size) {
			throw std::runtime_error("Something went wrong writing the output file!\n");
		}

		// Free the video data
		NDIlib_recv_free_video_v2(m_ndi_recv, video_frame.get());
	}
}

void boilerplate()
{
	// Report the NDI SDK Version
	printf("%s\n", NDIlib_version());
	printf("\n");
}

int main(int argc, char* argv[])
{
	// See if we're running from a terminal and can be interactive
	bool interactive = isatty(fileno(stdin));

	// Process command-line options

	// Default NDI source
	NDIlib_source_t ndi_source;

	// Default output file
	FILE *outfile = stdout;

	// Number of frames to record
	int num_frames = -1;

	debug_flush = false;

	int temp;

	// Passed on the command line
	int opt;
	while ((opt = getopt(argc, argv, "s:o:c:vqf")) != -1) {
		switch (opt) {
		// NDI Source
		case 's':
			ndi_source.p_ndi_name = optarg;
			ndi_source.p_url_address = NULL;
			break;

		// Output file
		case 'o':
			outfile = fopen(optarg, "wb");
			if (outfile == NULL) {
				fprintf (stderr, "Cannot open %s for writing!\n", optarg);
				abort();
			}
			break;

		// Frame count
		case 'c':
			temp = strtol(optarg, NULL, 0);
			if (temp > 0) num_frames = temp;
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
			fprintf(stderr, "Usage: %s [-s <NDI Source>] [-o <filename>] [-c <framecount>] [-vqf]\n", argv[0]);
			fprintf(stderr, "  -s Specify NDI source to display\n");
			fprintf(stderr, "  -o Specify output filename (default is to use stdout)\n");
			fprintf(stderr, "  -c Frame count or number of frames to record (default: Wait for user input)\n");
			fprintf(stderr, "  -v Increase debugging output level\n");
			fprintf(stderr, "  -q Decrease debugging output level\n");
			fprintf(stderr, "  -f fflush() after each debug message\n");
			exit(EXIT_FAILURE);
		}
	}

	// Check for conflicting options
	if ((num_frames == 0) && (interactive == false)) {
		LOG(LOG_ERR, "ERROR: Number of frames to record was not specified and stdin is not a tty!\n");
		exit(EXIT_FAILURE);
	}

	// Setup output
	if (outfile != stdout) {
		// It's safe to send some info to stdout
		boilerplate();
	}

	// Setup the NDI receiver
	////////////////////////////////////////////////////////////

	// Not required, but "correct" (see the SDK documentation.
	if (!NDIlib_initialize()) throw std::runtime_error("Cannot run NDI!");

	// NDI Finder instance, if needed
	NDIlib_find_instance_t pNDI_find = NULL;

	// The user didn't request a particular NDI source, just use the
	// first one we find
	if (ndi_source.p_ndi_name) {
		// Just use the specified NDI source
	} else {
		// We need to look for a source...

		// Create a finder instance
		pNDI_find = NDIlib_find_create_v2();
		if (!pNDI_find) throw std::runtime_error("Cannot create NDI finder!");

		uint32_t num_sources = 0;
		const NDIlib_source_t* p_sources = NULL;

		// Wait until there is at least one source
		while (!num_sources)
		{	// Wait until the sources on the network have changed
			LOG(LOG_INFO, "Looking for sources ...\n");
			NDIlib_find_wait_for_sources(pNDI_find, 1000/* One second */);
			p_sources = NDIlib_find_get_current_sources(pNDI_find, &num_sources);
		}

		LOG(LOG_INFO, "Found %u sources\n", num_sources);

		// Use the first source found
		ndi_source = p_sources[0];
	}

	LOG(LOG_INFO, "Using source %s\n", ndi_source.p_ndi_name);

	// Configure our receiver settings
	NDIlib_recv_create_v3_t my_settings;
	my_settings.source_to_connect_to = NULL; // Specified later
	my_settings.color_format = (NDIlib_recv_color_format_e) NDIlib_recv_color_format_best;
	my_settings.bandwidth = NDIlib_recv_bandwidth_highest;
	my_settings.allow_video_fields = true;
	my_settings.p_ndi_recv_name = "ndirx";

	// Create a JSON configuration string we can pass to the NDI library
	// These configuration settings can also be done using a file
	// See "Configuration Files" in the SDK documentation for details

	// Opening stanza
	std::string ndi_config;
	ndi_config = R"({ "ndi": { )";

	// Without a valid vendor ID, the Embedded NDI stack runs in demo mode for 30 minutes
	// If you have a valid vendor ID, add it below, eg:
	// ndi_config.append( R"("vendor": { "name": "My Company", "id": "00000000000000000000000000000000000000000000", } )";

	// Examples of specifying connection settings, uncomment if desired:

	// Explicitly enable mTCP and UDP formats
	// ndi_config.append( R"(
	// 	"tcp": { "recv": { "enable": true } },
	// 	"unicast": { "recv": { "enable": true } },
	// 	"rudp": { "recv": { "enable": true } } )" );

	// Disable mTCP and UDP formats which use a *LOT* of CPU on lower end platforms (eg: 32-bit ARM)
	// ndi_config.append( R"(
	// 	"tcp": { "recv": { "enable": false } },
	// 	"unicast": { "recv": { "enable": false } },
	// 	"rudp": { "recv": { "enable": false } } )" );

	// ndi_config.append( R"( "adapters": { "allowed": ["10.34.1.11"] } )" );

	// Closing braces
	ndi_config.append( R"(} })" );

	LOG(LOG_INFO, "ndi_config: %s\n",  ndi_config.c_str());

	// Create an NDI receiver
	NDIlib_recv_instance_t ndi_recv = NDIlib_recv_create_v4(&my_settings, ndi_config.c_str());
	if (!ndi_recv) throw std::runtime_error("Cannot create NDI Receiver!");

	// Connect to our sources
	LOG(LOG_INFO, "Connecting to %s\n", ndi_source.p_ndi_name);
	NDIlib_recv_connect(ndi_recv, &ndi_source);

	// We can now destroy the NDI finder if we created one...
	// ...we no longer need access to ndi_source (aka: p_sources[0])
	if (pNDI_find) NDIlib_find_destroy(pNDI_find);

	// Create a writer class to disconnect write performance from
	// NDI receiving performance
	writer *my_writer = new writer(ndi_recv, outfile);

	// Start the write thread
	my_writer->begin();

	// Setup to poll stdin to see if read data is available
	pollfd fds[1];
	fds[0].fd = STDIN_FILENO;
	fds[0].events = POLLIN;
	fds[0].revents = 0;

	bool active = false;
	int delay = 0;

	while (num_frames != 0)
	{
		// Check for user abort (data available on stdin)
		if (interactive && (poll(fds, 1, 0) != 0)) {
			break;
		}

		// Keep tabs on our performance
		NDIlib_recv_queue_t recv_q;
		NDIlib_recv_get_queue(ndi_recv, &recv_q);
		LOG(LOG_INFO, "q%i", recv_q.video_frames);

		// Wait for up to 1 second to see if there are any frames available
		NDIlib_frame_type_e frame_type;
		NDIlib_video_frame_v2_t video_frame;

		frame_type = NDIlib_recv_capture_v3(ndi_recv, &video_frame, NULL, NULL, 1000);
		if (frame_type == NDIlib_frame_type_video) {
			// Received a video frame
			LOG(LOG_INFO, ".");
			active = true;

			// Make sure it's the format we expect!
			if (video_frame.FourCC != NDIlib_FourCC_type_P216) {
				throw std::runtime_error("Unexpected video format!");
			}

			// Add the frame to the write queue
			my_writer->add_frame(&video_frame);

			// Keep going until we're finished
			if (num_frames > 0) num_frames--;
		} else if (frame_type == NDIlib_frame_type_none) {
			if (active) {
				// We were seeing video frames, but not any more
				// Our sender probably went away, give it a few
				// seconds and then exit cleanly
				if (delay++ >= 5) num_frames = 0;
			}
		}
	}

	// Wait for all the writes to finish
	LOG(LOG_INFO, "Flushing write queue\n");
	my_writer->flush();

	// Destroy the writer
	delete my_writer;

	// Destroy the receiver
	NDIlib_recv_destroy(ndi_recv);

	// Not required, but nice
	NDIlib_destroy();

	// Exit
	return 0;
}
