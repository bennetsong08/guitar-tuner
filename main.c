#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <stdbool.h>
#include <time.h>
#include <complex.h>
#include <pthread.h>

#include <SDL2/SDL.h>
#include <kissfft/kiss_fft.h>
#include <gtk/gtk.h>
#include <cairo.h>

// Variables for audio recording/processing
#define SAMPLE_RATE 		44100
#define AUDIO_FORMAT		AUDIO_S16SYS
#define AUDIO_CHANNELS		1
#define AUDIO_SAMPLES	 	1024
#define SAMPLE_COUNT		65536

#define SENSITIVITY		200	// Sensitivity of pitch detection when displaying histogram

#define BIN_COUNT		343

// Array to store audio data
short audio[SAMPLE_COUNT];
int data_pos = 0;

// GUI components
GtkWidget *window;
GtkWidget *box;
GtkWidget *button_E;
GtkWidget *button_A;
GtkWidget *button_D;
GtkWidget *button_G;
GtkWidget *button_B;
GtkWidget *button_e;
GtkWidget *t_window;
GtkWidget *t_box;
GtkWidget *c_box;
GtkWidget *back_button;
GtkWidget *note_label[50];
GtkWidget *space;

// Callback function for when audio device needs data
void audio_callback(void *userdata, Uint8 *stream, int len)
{
	short *new = (short *)stream;

	for (int i = 0; i < len / sizeof(short); i++) {
		audio[data_pos] = new[i];
		data_pos++;
		data_pos %= SAMPLE_COUNT;
	}
}

// FFT is used for converting digital signals to frequencies
void audio_fft(short *input, short *output, int len)
{
	// Configuration for KISS-FFT algorithm
	kiss_fft_cfg fft_config = kiss_fft_alloc(len, 0, NULL, NULL);

	kiss_fft_cpx *complex_input = (kiss_fft_cpx *)malloc(sizeof(kiss_fft_cpx) * len);	// Complex datatype for FFT input
	kiss_fft_cpx *complex_output = (kiss_fft_cpx *)malloc(sizeof(kiss_fft_cpx) * len);	// Initialize FFT output

	// Array to store windowed audio data
	short windowed[len];

	// Convert existing input to complex number
	for (int i = 0; i < len; i++) {
		// Apply windowing function to audio
		double mul = 0.5 * (1 - cos(2*M_PI*i/(len - 1)));
		windowed[i] = input[i] * mul;
		complex_input[i].r = windowed[i];
		complex_input[i].i = 0;
	}

	// Use FFT function
	kiss_fft(fft_config, complex_input, complex_output);

	// Convert complex output to real number for usage
	for (int i = 0; i < len; i++) {
		double curr = sqrt(complex_output[i].r * complex_output[i].r + complex_output[i].i * complex_output[i].i) / 200;
		output[i] = (short)fabs(curr);

		if (output[i] > 32767)
			output[i] = 32767;
	}

	free(complex_input);
	free(complex_output);
}

int *pitch_detection()
{
	int *bins = malloc(sizeof(int) * BIN_COUNT);		// Array to store height of each bin (bar) in histogram
	const float ref_pitch = 55.0;				// Pitch of A1 (55 Hz) as reference
	const float freq_interval = pow(2, 1.0 / BIN_COUNT); 	// Frequency interval of each bin. An octave has a music interval of 2:1 (x to 2x Hz is an octave)

	short output[SAMPLE_COUNT];

	audio_fft(audio, output, SAMPLE_COUNT);			// Use FFT function

	// Calculations
	for (int i = 0; i < BIN_COUNT; i++) {
		bins[i] = 0;
		float bin_freq = ref_pitch * pow(freq_interval, (float)i);	// Base frequency of current bin
		float next_bin_freq = bin_freq * freq_interval;			// Frequency of next bin (used to find average of current and next)
		float res = (float)SAMPLE_COUNT / (float)SAMPLE_RATE;
		float index = bin_freq * res;
		float index_next = next_bin_freq * res;

		// Loop to check through 8 octaves
		for (int j = 0; j < 8; j++) {
			// Add average frequency
			for (int k = round(index); k < round(index_next); k++)
				bins[i] += output[k] / 8 / (index_next - index);

			// Next octave
			index *= 2;
			index_next *= 2;
		}
	}

	return bins;
}

// Notes corresponding to indices on note array
enum note_index {E = 103,  A = 0, D = 75, G = 147, B = 30};

// Axis for histogram
char axis[] = "A              A#             B              C              C#             D             D#            E             F              F#             G             G#            ";

void draw_histogram(int *bins, int note_index)
{
	// Clear terminal for redraw
	system("clear");

	int start_index = note_index - 15, end_index = note_index + 16;

	if (start_index < 0)
		start_index = 0;
	if (end_index > BIN_COUNT - 1)
		end_index = BIN_COUNT - 1;

	for (int i = start_index; i <= end_index; i++)
		printf("%c", axis[i]);

	printf("\n");

	// Display histogram for selected range (note)
	for (int i = 30; i > 0; i--) {
		for (int j = start_index; j <= end_index; j++) {
			if (bins[j] > i * SENSITIVITY)
				printf("#");
			else
				printf(" ");
		}

		printf("\n");
	}
}

char str[31][33];

// Draw graph on GUI
void draw_gui_graph(int *bins, int note_index)
{
	int start_index = note_index - 15, end_index = note_index + 16;

	if (start_index < 0)
		start_index = 0;
	if (end_index > BIN_COUNT - 1)
		end_index = BIN_COUNT - 1;

	for (int i = start_index; i <= end_index; i++)
		str[0][i - start_index] = axis[i];

	// Display histogram for selected range (note)
	for (int i = 30; i > 0; i--) {
		for (int j = start_index; j <= end_index; j++) {
			if (bins[j] > i * SENSITIVITY)
				str[31 - i][j - start_index] = '#';
			else
				str[31 - i][j - start_index] = ' ';
		}

	}

	for (int i = 0; i < 31; i++)
		gtk_label_set_text((GtkLabel *)note_label[i], str[i]);
}

// Combined function to complete pitch detection and draw
gboolean draw(gpointer data)
{
	int note = (intptr_t)data;
	
	int *bins = pitch_detection();
	draw_histogram(bins, note);
	draw_gui_graph(bins, note);

	return G_SOURCE_CONTINUE;
}

int timeout_status;

// Function for closing tuning page (return to main)
static void close_page(GtkWidget *widget, gpointer data)
{
	// Show main window
	gtk_widget_set_visible(window, true);

	// Close current window
	gtk_window_destroy(GTK_WINDOW(t_window));
	g_source_remove(timeout_status);

	// Clear terminal
	system("clear");
}

// Function to open and display page for tuning
static void tune_page(GtkWidget *widget, gpointer data)
{
	// Hide main window
	gtk_widget_set_visible(window, false);

	// New window for tuning
	t_window = gtk_window_new();
	gtk_window_set_title(GTK_WINDOW(t_window), "Guitar tuner");
	gtk_window_set_default_size(GTK_WINDOW(t_window), 500, 500);

	// Box to organize components
	t_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_window_set_child(GTK_WINDOW(t_window), t_box);

	space = gtk_label_new("\n\n\n");
	gtk_box_append(GTK_BOX(t_box), space);

	// Axis for notes
	for (int i = 0; i < 35; i++) {
		note_label[i] = gtk_label_new(NULL);
		gtk_box_append(GTK_BOX(t_box), note_label[i]);
	}

	// Button to return to main page
	back_button = gtk_button_new_with_label("Back");
	g_signal_connect(back_button, "clicked", G_CALLBACK(close_page), NULL);
	gtk_box_append(GTK_BOX(t_box), back_button);

	// Show window
	gtk_window_present(GTK_WINDOW(t_window));

	// Call pitch detection calculations and drawing function every 50ms
	timeout_status = g_timeout_add(50, draw, data);
}

// Function to initalize and activate user interface (main page)
static void activate_ui(GtkApplication *app, gpointer user_data)
{
	// Application window
	window = gtk_application_window_new(app);
	gtk_window_set_title(GTK_WINDOW(window), "Guitar tuner");
	gtk_window_set_default_size(GTK_WINDOW(window), 500, 500);

	// Box to organize buttons
	box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_set_halign(box, GTK_ALIGN_CENTER);
	gtk_widget_set_valign(box, GTK_ALIGN_CENTER);
	gtk_window_set_child(GTK_WINDOW(window), box);

	// Buttons
	button_E = gtk_button_new_with_label("E");
	g_signal_connect(button_E, "clicked", G_CALLBACK(tune_page), (gpointer)E);
	gtk_box_append(GTK_BOX(box), button_E);

	button_A = gtk_button_new_with_label("A");
	g_signal_connect(button_A, "clicked", G_CALLBACK(tune_page), (gpointer)A);
	gtk_box_append(GTK_BOX(box), button_A);

	button_D = gtk_button_new_with_label("D");
	g_signal_connect(button_D, "clicked", G_CALLBACK(tune_page), (gpointer)D);
	gtk_box_append(GTK_BOX(box), button_D);

	button_G = gtk_button_new_with_label("G");
	g_signal_connect(button_G, "clicked", G_CALLBACK(tune_page), (gpointer)G);
	gtk_box_append(GTK_BOX(box), button_G);

	button_B = gtk_button_new_with_label("B");
	g_signal_connect(button_B, "clicked", G_CALLBACK(tune_page), (gpointer)B);
	gtk_box_append(GTK_BOX(box), button_B);

	button_e = gtk_button_new_with_label("e");
	g_signal_connect(button_e, "clicked", G_CALLBACK(tune_page), (gpointer)E);
	gtk_box_append(GTK_BOX(box), button_e);

	// Show window
	gtk_window_present(GTK_WINDOW(window));
}

int main(int argc, char *argv[])
{
	// Initalize audio
	SDL_Init(SDL_INIT_AUDIO);
	SDL_AudioSpec audio_spec;

	// Set specifications for audio input (sample rate, channels, etc.)
	audio_spec.freq = SAMPLE_RATE;
	audio_spec.format = AUDIO_FORMAT;
	audio_spec.channels = AUDIO_CHANNELS;
	audio_spec.samples = AUDIO_SAMPLES;
	audio_spec.callback = audio_callback;

	// Set audio device to microphone
	SDL_AudioDeviceID audio_device = SDL_OpenAudioDevice(NULL, 1, &audio_spec, NULL, 0);

	// Unpause audio device (start recording)
	SDL_PauseAudioDevice(audio_device, 0);

	// Run user interface + calculations
	GtkApplication *app;
	app = gtk_application_new(NULL, G_APPLICATION_DEFAULT_FLAGS);;
	g_signal_connect(app, "activate", G_CALLBACK(activate_ui), NULL);
	int status = g_application_run(G_APPLICATION(app), argc, argv);

	// Stop audio device (end recording)
	SDL_CloseAudioDevice(audio_device);

	return status;
}

