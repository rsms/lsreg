/*
 * The lsreg CLI program
 */

#include "lsreg.h"
#include "revision.h"

#include <stdlib.h>
#include <getopt.h>
#include <libgen.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

const char *progname;

static struct {
  const char* format;
} options;

// Declarations
void version();
void usage();

// Macros
#define die(fmt, ...) {\
  fprintf(stderr, "%s: " fmt "\n", progname, ##__VA_ARGS__);\
  fprintf(stderr, "Type '%s help' for usage.\n", progname);\
  exit(2); \
}

// ---------------------------------------------
#pragma mark -
#pragma mark Command parsing

typedef struct {
  const char *name;
  const char *alias1;
  const char *alias2;
  const char *alias3;
} command_t;


int command_get(int argc, char * const *argv, const command_t *commands) {
  size_t x, y;
  command_t cmd;
  y = x = 0;
  
  for(;;x++) {
    cmd = commands[x];
    if(cmd.name == NULL) {
      break;
    }
    for(y=0; y<argc; y++) {
      if( (strcasecmp(cmd.name, argv[y]) == 0)
         ||(cmd.alias1 && (strcasecmp(cmd.alias1, argv[y]) == 0))
         ||(cmd.alias2 && (strcasecmp(cmd.alias2, argv[y]) == 0))
         ||(cmd.alias3 && (strcasecmp(cmd.alias3, argv[y]) == 0)) )
      {
        return (int)x;
      }
    }
  }
  return -1;
}


// ---------------------------------------------
#pragma mark -
#pragma mark Dump

static lsreg_rec_t dump_rec;
static lsreg_rec_t *dump_rec_factory(void *d) {
  return &dump_rec;
}
static int dump_rec_c_cb(lsreg_rec_t *rec, void *d) {
  lsreg_rec_dump(rec, stdout);
  return 0;
}

static const char *dump_rec_xml_indents[] = {
  "","  ","    ","      ","        ","          ",NULL
};


// 'foo <bar> "baz" & etc'  becomes  'foo &#62;bar&#60; &#34;baz&#34; &#38; etc'
// You are responsible for freeing the newly allocated string returned.
char *xml_encode(const char *str) {
  char *buf, *sub;
  size_t siz, i;
  
  if(!str) {
    return NULL;
  }
  
  i = 0;
  siz = strlen(str) + 1;
  if((buf = (char *)malloc(siz)) == NULL) {
    die("malloc() failed in main.c");
  }

  for (; *str; str++, i++)  {
    switch(*str) {
      case '&': sub = "&#38;"; break;
      case '"': sub = "&#34;"; break;
      case '>': sub = "&#60;"; break;
      case '<': sub = "&#62;"; break;
      default:  sub = NULL;
    }
    
    if (sub) {
      if((buf = realloc(buf, siz += 4)) == NULL) {
        die("realloc() failed in main.c");
      }
      strcpy(buf+i, sub);
      i += 4;
    }
    else {
      buf[i] = *str;
    }
  }

  buf[i] = '\0';
  return buf;
}


static void dump_rec_xml_identifier(lsreg_identifier_t *s, const char *tagname, int indent) {
  if(s && s->name) {
    char *name = xml_encode(s->name);
    fprintf(stdout,
            "%s<%s hash=\"%x\">%s</%s>\n",
            dump_rec_xml_indents[indent],
            tagname,
            s->hash,
            name,
            tagname);
    free(name);
  }
}


static void dump_rec_xml_string(const char *ptr, const char *tagname, int indent) {
  if(ptr && strlen(ptr)) {
    char *escaped = xml_encode(ptr);
    fprintf(stdout, "%s<%s>%s</%s>\n", 
            dump_rec_xml_indents[indent], tagname, escaped, tagname);
    free(escaped);
  }
}


static void dump_rec_xml_date(const struct tm *date, const char *tagname, int indent) {
  if(date) {
    char *formatted = strdup("0000-00-00T00:00:00+0000");
    strftime(formatted, strlen(formatted), "%Y-%m-%dT%T%z", date);
    fprintf(stdout, "%s<%s>%s</%s>\n", 
            dump_rec_xml_indents[indent],
            tagname,
            formatted,
            tagname);
    free(formatted);
  }
}


static void dump_rec_xml_strings(const char **ptr, const char *tagname, const char *item_tagname, int indent) {
  const char *item;
  const char **v;
  int first;
  
  if(ptr) {
    first = 1;
    
    for( v = ptr; (item = *v); v++ ) {
      if(first) {
        fprintf(stdout, "%s<%s>\n", dump_rec_xml_indents[indent], tagname);
        indent++;
        first = 0;
      }
      dump_rec_xml_string(item, item_tagname, indent);
    }
    
    if(!first) {
      indent--;
      fprintf(stdout, "%s</%s>\n", dump_rec_xml_indents[indent], tagname);
    }
  }
}


static void dump_rec_xml_bundle(lsreg_rec_t *rec, const char *tagname, int indent) {
  lsreg_bundle_t *bundle;
  char *name, *version, *type_code, *id;
  
  if((bundle = (lsreg_bundle_t *)rec->rec) == NULL) {
    return;
  }
  name = xml_encode(bundle->name);
  version = xml_encode(bundle->version);
  type_code = xml_encode(bundle->type_code);
  id = xml_encode(bundle->canonical_identifier.name);
  
  fprintf(stdout,
          "%s<%s id=\"%u\" name=\"%s\" version=\"%s\" type_code=\"%s\" identifier=\"%s\">\n",
          dump_rec_xml_indents[indent],
          tagname,
          bundle->uid,
          (name ? name : ""),
          (version ? version : ""),
          (type_code ? type_code : ""),
          (id ? id : ""));
  
  if(name)      free(name);
  if(version)   free(version);
  if(type_code) free(type_code);
  if(id)        free(id);
  
  indent++;
  
  dump_rec_xml_identifier(&bundle->identifier,            "identifier", indent);
  dump_rec_xml_identifier(&bundle->canonical_identifier,  "canonical_identifier", indent);
  dump_rec_xml_string(bundle->path,       "path", indent);
  dump_rec_xml_string(bundle->executable, "executable", indent);
  dump_rec_xml_date(bundle->regdate,      "regdate", indent);
  dump_rec_xml_date(bundle->moddate,      "moddate", indent);
  dump_rec_xml_string(bundle->library,    "library", indent);
  dump_rec_xml_strings((const char**)bundle->library_items, "library_items", "item", indent);
  
  indent--;
  fprintf(stdout, "%s</%s>\n", dump_rec_xml_indents[indent], tagname);
}


static void dump_rec_xml_volume(lsreg_rec_t *rec, const char *tagname, int indent) {
  lsreg_volume_t *s;
  
  s = (lsreg_volume_t *)rec->rec;
  if(s == NULL) {
    return;
  }
  fprintf(stdout, "%s<%s id=\"%u\" mounted=\"%s\" vrefnum=\"%d\" flags=\"%08x\">\n", 
          dump_rec_xml_indents[indent],
          tagname,
          s->uid,
          (s->is_mounted ? "true" : "false"),
          s->vrefnum,
          s->flags);
  indent++;
  
  dump_rec_xml_string(s->path,        "path", indent);
  dump_rec_xml_string(s->disk_image,  "disk_image", indent);
  
  indent--;
  fprintf(stdout, "%s</%s>\n", dump_rec_xml_indents[indent], tagname);
}


static void dump_rec_xml_handler(lsreg_rec_t *rec, const char *tagname, int indent) {
  lsreg_handler_t *s;
  char *content_type, *extension, *uri_scheme;
  
  if((s = (lsreg_handler_t *)rec->rec) == NULL) {
    return;
  }
  content_type = xml_encode(s->content_type);
  extension = xml_encode(s->extension);
  uri_scheme = xml_encode(s->uri_scheme);
  
  fprintf(stdout, "%s<%s id=\"%u\" content_type=\"%s\" extension=\"%s\" uri_scheme=\"%s\" options=\"%08x\">\n", 
          dump_rec_xml_indents[indent],
          tagname,
          s->uid,
          (content_type ? content_type : ""),
          (extension ? extension : ""),
          (uri_scheme ? uri_scheme : ""),
          s->options
  );
  
  if(content_type)  free(content_type);
  if(extension)     free(extension);
  if(uri_scheme)   free(uri_scheme);
  
  indent++;
  dump_rec_xml_identifier(&s->roles, "roles", indent);
  indent--;
  
  fprintf(stdout, "%s</%s>\n", dump_rec_xml_indents[indent], tagname);
}


static int dump_rec_xml_cb(lsreg_rec_t *rec, void *d) {
  int indent = 1;
  
  switch(rec->type) {
    case kLSRegRecTypeBundle:
      dump_rec_xml_bundle(rec, "bundle", indent);
      break;
    case kLSRegRecTypeVolume:
      dump_rec_xml_volume(rec, "volume", indent);
      break;
    case kLSRegRecTypeHandler:
      dump_rec_xml_handler(rec, "handler", indent);
      break;
  }
  return 0;
}


void dump(int argc, const char * argv[]) {
  if( (options.format == NULL) || (strcasecmp(options.format, "c") == 0) ) {
    lsreg_iterate(dump_rec_factory, dump_rec_c_cb, NULL);
  }
  else if(strcasecmp(options.format, "xml") == 0) {
    fputs("<?xml version=\"1.0\" encoding=\"UTF-8\">\n"
          "<records>\n", stdout);
    lsreg_iterate(dump_rec_factory, dump_rec_xml_cb, NULL);
    fputs("</records>\n", stdout);
  }
  else {
    die("Unsupported format: %s", options.format);
  }
}



// ---------------------------------------------
#pragma mark -
#pragma mark Main

void usage() {
  fprintf(stderr,
          "usage: %s [options] command\n"
          "Launch Services registry access.\n"
          "\n"
          "Options:\n"
          "  -f --format FORMAT  Output format. Valid formats are: 'xml' and 'c' (default).\n"
          "  -h --help           Show this help message and quit.\n"
          "  -V --version        Show version number and build date and quit.\n"
          "\n"
          "Commands:\n"
          "  dump (list)         Output all information available.\n"
          "  help                Show this help message and quit.\n"
          ,
          progname);
  exit(1);
}

void version() {
  fputs("liblsreg " LIBLSREG_VERSION " (" LIBLSREG_REVISION 
        ") built on " __DATE__ ", " __TIME__ "\n", stderr);
  exit(0);
}


int main (int argc, const char * argv[]) {
  int ch;
  
  options.format = NULL;
  progname = basename(strdup(argv[0]));
  
  // Options
  static struct option longopts[] = {
    { "format",   optional_argument, NULL, 'f' },
    { "help",     no_argument,       NULL, 'h' },
    { "version",  no_argument,       NULL, 'V' },
  {NULL,0,NULL,0}/* sentinel */};
  
  // Commands
  static command_t commands[] = {
    { "dump", "list", NULL,NULL },
    { "help", NULL,   NULL,NULL },
  {NULL,NULL,NULL,NULL}/* sentinel */};
  
  // Parse options
  while ((ch = getopt_long(argc, (char * const *)argv, "f:hV", longopts, NULL)) != -1) switch (ch) {
    case 'f':
      options.format = optarg;
      break;
    case 'V':
      version();
      break;
    case 'h':
      usage();
      break;
    default:
      fprintf(stderr, "Type '%s help' for usage.\n", progname);
      exit(1);
  }
  
  argc -= optind;
  argv += optind;
  
  // Parse & run command
  switch(command_get(argc, (char * const *)argv, commands)) {
    case 0:
      dump(argc, argv);
      break;
    case 1:
      usage();
    default:
      if(argc) {
        die("Unknown command: '%s'", argv[0]);
      } else {
        usage();
      }
  }
  
  return 0;
}
