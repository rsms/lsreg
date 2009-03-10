/*
 * A very simple example program which dumps out all
 * volume records to stdout. It illustrates the important
 * features of liblsreg.
 */

#include "lsreg.h"

// This is the record we will reuse, since
// we do not keep any records.
static lsreg_rec_t record;

// Record factory
static lsreg_rec_t *rec_factory(void *d) {
  // Simply return our statically allocated record
  return &record;
}

// Record handler
static int rec_handler(lsreg_rec_t *rec, void *d) {
  // Only print volume records
  if(rec->type == kLSRegRecTypeVolume) {
    // Use the built-in dump function to print
    // the contents of rec to stdout.
    lsreg_rec_dump(rec, stdout);
  }
  // If we return 1, iteration will end
  return 0;
}

// Main program
int main (int argc, const char * argv[]) {
  // Start iteration
  lsreg_iterate(rec_factory, rec_handler, NULL);
  // Exit OK
  return 0;
}
