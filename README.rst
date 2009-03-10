OS X Launch Services registry
=============================

C library, enabling programmatic access to the registry of all installed bundles and mounted disks on a system.

Usage
-----

The launch registry contains three different type of records:

* **Bundles** – Applications, plugins, etc.
* **Volumes** – Mounted and previously mounted disks.
* **Handlers** – Mapping file types to applications handling them.

lsreg enables introspection of all three types.

The interface is very simplistic and iteration-based. Let's look at a really simple example:

::
  
  #include "lsreg.h"
  
  static lsreg_rec_t record;
  
  static lsreg_rec_t *rec_factory(void *userdata) {
    return &record;
  }

  static int rec_handler(lsreg_rec_t *rec, void *userdata) {
    lsreg_rec_dump(rec, stdout);
    return 0;
  }

  int main (int argc, const char * argv[]) {
    lsreg_iterate(rec_factory, rec_handler, NULL);
    return 0;
  }

This little program dumps the whole Launch Registry to stdout. Not very useful. Anyway, what happens here?

* We need to define two functions:

  * **Record factory** – provides each iteration with a ``lsreg_rec_t`` structure. In our example above we run everything in sequence and can thus reuse a lsreg_rec_t allocated on the stack.

  * **Record handlers** – called with an actual record. This is where your code does something useful (doh!).

* We call lsreg_iterate, starting iteration (the order in which records are walked is undefined).

Let's rewrite the program above to only print information about bundle records which identifier have a certain prefix, passed to the program as an command line argument.

::

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

The header file ``lsreg.h`` is pretty much self-documenting.

