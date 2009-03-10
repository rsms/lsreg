#include <string.h>
#include "lsreg.h"

static lsreg_rec_t record;

static lsreg_rec_t *rec_factory(void *match_prefix) {
  return &record;
}

static int rec_handler(lsreg_rec_t *rec, void *match_prefix) {
  lsreg_bundle_t *bundle;
  size_t pxlen;
  
  if(rec->type == kLSRegRecTypeBundle) {
    bundle = (lsreg_bundle_t *)rec->rec;
    pxlen = strlen((const char *)match_prefix);
    if ( (pxlen <= strlen(bundle->identifier.name)) &&
         (strncasecmp((const char *)match_prefix, bundle->identifier.name, pxlen) == 0)
       )
    {
      puts(bundle->path);
    }
  }
  
  return 0;
}

int main (int argc, const char * argv[]) {
  if (argc < 2) {
    fprintf(stderr, "usage: %s PREFIX\n", argv[0]);
    return 1;
  }
  lsreg_iterate(rec_factory, rec_handler, (void *)argv[1]);
  return 0;
}
