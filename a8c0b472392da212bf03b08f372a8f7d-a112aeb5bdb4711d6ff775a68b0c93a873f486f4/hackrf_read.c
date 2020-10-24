/*

  Simple HackRF programming exercise
    Sipke '19

  This program receives raw data from a certain frequency until the user pressed ctrl+c.
  Code is mostly taken from hackrf_sweep.c (hackrf toolbox) and converted to the bare minimum.
  It's created to learn about how the HackRF behaves and to do something with it via custom code that doesn't involve GnuRadio.
  Note that you can directly use the command line tools hackrf_sweep, hackrf_transfer and rtl_sdr for the same thing and more.


  --------------------------- Usage

    Requirement
  brew install hackrf

    Compile
  gcc -I /opt/local/include/libhackrf/ hackrf_read.c -o hackrf_read -lhackrf

    Use
  ./hackrf_read


  --------------------------- References

  The LibHackRF reference:
    https://github.com/mossmann/hackrf/wiki/libHackRF-API

  For structs like hackrf_transfer see:
    https://github.com/mossmann/hackrf/blob/master/host/libhackrf/src/hackrf.h

  Used applications for copying code (from the hackrf toolbox):
    https://github.com/mossmann/hackrf/blob/master/host/hackrf-tools/src/hackrf_sweep.c
    https://github.com/mossmann/hackrf/blob/master/host/hackrf-tools/src/hackrf_transfer.c
  
  Someone explaining the data format of hackrf_start_rx buffers
    https://hackrf-dev.greatscottgadgets.narkive.com/29i7Kivv/what-s-the-data-format-of-hackrf-transfer-buffer-when-i-call-hackrf-start-rx
  
  Also not bad: someone trying to port rtl_433 to HackRF
    https://github.com/Lefinnois/hackrf_433

*/


// HackRF lib
#include <hackrf.h>
// Normal system libs
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <signal.h>



// Device object
static hackrf_device* device = NULL;
// Cross-thread boolean trigger
volatile bool do_exit = false;
// File for output
FILE *fp;


// Settings
#define DEFAULT_SAMPLE_RATE_HZ 256000
#define RADIO_FREQ 85500000
#define OUTPUT_FILE "output_samples.raw"


// Break on signals like ctrl+c
void sigint_callback_handler(int signum)  
{
  fprintf(stderr, "Caught signal %d\n", signum);
  do_exit = true;
}


// Callback for data receive
int rx_callback(hackrf_transfer* transfer) 
{
  if (do_exit)
    return 0;

  // Write raw samples to output file
  if (fp != NULL)
    fwrite(transfer->buffer, transfer->valid_length, 1, fp);

  // To not capture too much traffic
  // usleep(50000);

  // Debugging recv
  // fprintf(stderr, "Valid length: %i\n", transfer->valid_length);
  // fprintf(stderr, "frequency: %llu\n", frequency);

  return 0;
}


int main(int argc, char** argv) 
{
  int result = 0;
  
  // Frequency in hertz
  const uint64_t freq_hz = RADIO_FREQ;

  // Open output file
  fp = fopen(OUTPUT_FILE, "w");
  if( fp == NULL )
  {
    fprintf(stderr, "Error opening output file");   
    return EXIT_FAILURE;            
  }

  // Init HackRF
  result = hackrf_init();
  if( result != HACKRF_SUCCESS )
  {
    fprintf(stderr, "hackrf_init() failed: %s (%d)\n", hackrf_error_name(result), result);
    return EXIT_FAILURE;
  }

  // Open device
  const char* serial_number = NULL;
  result = hackrf_open_by_serial(serial_number, &device);
  if( result != HACKRF_SUCCESS ) 
  {
    fprintf(stderr, "hackrf_open() failed: %s (%d)\n", hackrf_error_name(result), result);
    return EXIT_FAILURE;
  }

  // Set frequency
  result = hackrf_set_freq(device, freq_hz);
  if ( result != HACKRF_SUCCESS )
  {
    fprintf(stderr, "hackrf_set_freq() failed: %s (%d)\n", hackrf_error_name(result), result);
    return EXIT_FAILURE;
  }

  // Set sample rate
  result = hackrf_set_sample_rate(device, DEFAULT_SAMPLE_RATE_HZ);
  if( result != HACKRF_SUCCESS ) {
    fprintf(stderr, "hackrf_sample_rate_set() failed: %s (%d)\n", hackrf_error_name(result), result);
    return EXIT_FAILURE;
  }

  // Handle signals (For ctrl+c and kill signals)
  signal(SIGINT, &sigint_callback_handler);
  signal(SIGILL, &sigint_callback_handler);
  signal(SIGFPE, &sigint_callback_handler);
  signal(SIGSEGV, &sigint_callback_handler);
  signal(SIGTERM, &sigint_callback_handler);
  signal(SIGABRT, &sigint_callback_handler);

  // Start recieving
  result |= hackrf_start_rx(device, rx_callback, NULL);
  if (result != HACKRF_SUCCESS) 
  {
    fprintf(stderr, "hackrf_start_rx() failed: %s (%d)\n", hackrf_error_name(result), result);
    return EXIT_FAILURE;
  }

  // Loop until break by signal or device stops
  fprintf(stderr, "Stop with Ctrl-C\n");
  while((hackrf_is_streaming(device) == HACKRF_TRUE) && (do_exit == false)) 
  {
    sleep(1); // Wait a second
  }

  // Stop HackRF and clean up
  if(device != NULL) 
  {
    // Stop recieving
    result = hackrf_stop_rx(device);
    if(result != HACKRF_SUCCESS) 
      fprintf(stderr, "hackrf_stop_rx() failed: %s (%d)\n", hackrf_error_name(result), result);

    // Close device
    result = hackrf_close(device);
    if(result != HACKRF_SUCCESS) 
      fprintf(stderr, "hackrf_close() failed: %s (%d)\n", hackrf_error_name(result), result);

    // Exit HackRF and clean up
    hackrf_exit();

    // Close file
    if (fp != NULL)
      fclose(fp);
  }

  return 0;

}