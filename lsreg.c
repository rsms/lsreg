/*
 * Copyright (c) 2007-2009 Rasmus Andersson
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>

#include "lsreg.h"

#pragma mark -
#pragma mark Macros

#define log_error(fmt, ...)   fprintf(stderr, __FILE__ ":%d: " fmt "\n", __LINE__, ##__VA_ARGS__)
#define log_error_n(fmt, ...) fprintf(stderr, __FILE__ ":%d: " fmt, __LINE__, ##__VA_ARGS__)

#define STRCREATE(target, from, len) \
  (target) = (char *)malloc(sizeof(char)*((len)+1));\
  if(len){ memcpy((target), (from), (len)); }\
  (target)[(len)] = '\0';\


#pragma mark -
#pragma mark Constants

//MAC_OS_X_VERSION_MINOR - for example, 10.4.10 is 0410 and 10.5.0 is 0500

#if MAC_OS_X_VERSION < MAC_OS_X_VERSION_10_5
const char *kLSRegisterCmd = "/System/Library/Frameworks/ApplicationServices.framework/Versions/A/Frameworks/LaunchServices.framework/Versions/A/Support/lsregister -dump";
#else
const char *kLSRegisterCmd = "/System/Library/Frameworks/CoreServices.framework/Versions/A/Frameworks/LaunchServices.framework/Versions/A/Support/lsregister -dump";
#endif


// Record subtype parser
typedef int lsreg_parser_func(FILE *f,
                              char *linebuf, size_t linebufsize,
                              char *line, size_t linelen,
                              char *key, size_t keylen,
                              char *val, size_t vallen,
                              lsreg_rec_t *record);


// ---------------------------------------------
#pragma mark -
#pragma mark Internal utilites


// Converts a hexadecimal string to unsigned integer
static int _x32ntoi(const char* xs, size_t length, unsigned int *result) {
  int i, xv, fact;
  if (length) {
    // Converting more than 32bit hexadecimal value?
    if (length>8) return 1; // exit
    // Begin conversion here
    *result = 0;
    fact = 1;
    // Run until no more character to convert
    for(i=length-1; i>=0 ;i--) {
      if (isxdigit(*(xs+i))) {
        if (*(xs+i)>=97) {
          xv = ( *(xs+i) - 97) + 10;
        }
        else if ( *(xs+i) >= 65) {
          xv = (*(xs+i) - 65) + 10;
        }
        else {
          xv = *(xs+i) - 48;
        }
        *result += (xv * fact);
        fact *= 16;
      }
      else {
        // Conversion was abnormally terminated
        // by non hexadecimal digit, hence
        // returning only the converted with
        // an error value 2 (illegal hex character)
        return 2;
      }
    }
  }
  return 0;
}


static void *_memrchr(const void *buf, unsigned char c, size_t length) {
  unsigned char *p = (unsigned char *) buf;
  if(length == 0) {
    return NULL;
  }
  for (;;) {
    if (*p-- == c) {
      break;
    }
    if (length-- == 0) {
      return NULL;
    }
  }
  return (void *) (p + 1);
}


static char *_memltrim(char *bytes, size_t *length) {
  size_t i;
  for (i=0; isspace(*bytes) && (i < *length); i++) {
    bytes++;
  }
  *length -= i;
  return bytes;
}


static void _memrtrim(const char *bytes, size_t *length) {
  if(*length == 0) {
    return;
  }
  const char *p = bytes+(*length);
  for (; (*length)-- && isspace(*(p--)) ;);
}


static char *_readline(char *buf, int bufsize, FILE *f) {
  char *line;
  if((line = fgets(buf, bufsize, f)) == NULL) {
    int errnum = ferror(f);
    if(errnum) {
      log_error("Error while reading: %s", strerror(errnum));
      clearerr(f);
    }
  }
  return line;
}


// ---------------------------------------------
#pragma mark -
#pragma mark Record methods

// Initialize record
void lsreg_rec_init(lsreg_rec_t *s) {
  s->uid = 0;
  s->type = kLSRegRecTypeUnknown;
  s->rec = NULL;
}

// Dump a record, in a human readable format, to stream
void lsreg_rec_dump(lsreg_rec_t *s, FILE *stream) {
  switch(s->type) {
    case kLSRegRecTypeBundle:
      lsreg_bundle_dump((lsreg_bundle_t *)s->rec, stream);
      break;
    case kLSRegRecTypeVolume:
      lsreg_volume_dump((lsreg_volume_t *)s->rec, stream);
      break;
    case kLSRegRecTypeHandler:
      lsreg_handler_dump((lsreg_handler_t *)s->rec, stream);
      break;
    default:
      fprintf(stream, "No dump function for record of type unknown\n");
  }
}

// Free record members, but not the record itself
void lsreg_rec_free_members(lsreg_rec_t *s) {
  if(s->rec != NULL) {
    switch(s->type) {
      case kLSRegRecTypeBundle:
        lsreg_bundle_free((lsreg_bundle_t *)s->rec);
        break;
      case kLSRegRecTypeVolume:
        lsreg_volume_free((lsreg_volume_t *)s->rec);
        break;
      case kLSRegRecTypeHandler:
        lsreg_handler_free((lsreg_handler_t *)s->rec);
        break;
      default:
        log_error("Failed to free record. No free method for record type.");
    }
  }
}

// Free record and all it's members
void lsreg_rec_free(lsreg_rec_t *s) {
  if(s != NULL) {
    lsreg_rec_free_members(s);
    free(s);
  }
}


#pragma mark -
#pragma mark Identifier methods


int lsreg_identifier_parse(const char *ptr, size_t length, lsreg_identifier_t *s) {
  size_t idlen;
  const char *idstart;
  int status;
  
  status = 0;
  
  // "foo.bar.SomeThing (0x8000702f)"
  if((idstart = _memrchr(ptr+length, '(', length)) != NULL) {
    idstart += 3; // skip "(0x"
    idlen = (length-(idstart-ptr))-1;
    length = (idstart-ptr)-3; // skip "(0x"
    if(_x32ntoi(idstart, idlen, &s->hash)) {
      log_error_n("Failed to parse hexadecimal number in string '");
      fwrite(idstart, sizeof(char), idlen, stderr);
      fprintf(stderr, "'\n");
      status = 1;
    }
  }
  _memrtrim(ptr, &length);
  STRCREATE(s->name, ptr, length);
  return status;
}


// Dump bundle, in a human readable format, to stream
void lsreg_identifier_dump(lsreg_identifier_t *s, FILE *stream, const char *indent) {
  fputs("<lsreg_identifier_t>", stream);
  if(s == NULL) {
    fputs("NULL\n", stream);
  }
  else {
    fprintf(stream,
            "{\n"
            "%s  name = \"%s\"\n"
            "%s  hash = 0x%x\n"
            "%s}\n",
            indent, s->name,
            indent, s->hash,
            indent);
  }
}


// ---------------------------------------------
#pragma mark -
#pragma mark Bundle methods

// Initialize bundle
void lsreg_bundle_init(lsreg_bundle_t *s) {
  s->uid = 0;
  s->identifier.name = NULL;
  s->identifier.hash = 0;
  s->canonical_identifier.name = NULL;
  s->canonical_identifier.hash = 0;
  s->path = NULL;
  s->name = NULL;
  s->version = NULL;
  s->type_code = NULL;
  s->executable = NULL;
  s->icon = NULL;
  s->regdate = NULL;
  s->moddate = NULL;
  s->library = NULL;
  s->library_items = NULL;
}

// Free all bundle members, but not the bundle itself
void lsreg_bundle_free_members(lsreg_bundle_t *s) {
  if(s->identifier.name)            free(s->identifier.name);
  if(s->canonical_identifier.name)  free(s->canonical_identifier.name);
  if(s->path)           free(s->path);
  if(s->name)           free(s->name);
  if(s->version)        free(s->version);
  if(s->type_code)      free(s->type_code);
  if(s->executable)     free(s->executable);
  if(s->icon)           free(s->icon);
  if(s->regdate)        free(s->regdate);
  if(s->moddate)        free(s->moddate);
  if(s->library)        free(s->library);
  if(s->library_items) {
    char *p;
    char **pp;
    for( pp = s->library_items; (p = *pp); pp++ ) {
      free(p);
    }
    free(s->library_items);
  }
}

// Free bundle and all its members
void lsreg_bundle_free(lsreg_bundle_t *s) {
  if(s != NULL) {
    lsreg_bundle_free_members(s);
    free(s);
  }
}

// Dump bundle, in a human readable format, to stream
void lsreg_bundle_dump(lsreg_bundle_t *bundle, FILE *stream) {
  if(bundle == NULL) {
    fprintf(stream, "<lsreg_bundle_t>NULL\n");
  }
  else {
    
    char *regdate, *moddate;
    regdate = strdup("0000-00-00 00:00:00");
    moddate = strdup("0000-00-00 00:00:00");
    if(bundle->regdate) {
      strftime(regdate, 19, "%Y-%m-%d %T", bundle->regdate);
    }
    if(bundle->moddate) {
      strftime(moddate, 19, "%Y-%m-%d %T", bundle->moddate);
    }
    
    fprintf(stream,
            "<lsreg_bundle_t>{\n"
            "  uid                  = %u\n"
            "  identifier           = ", bundle->uid);
    lsreg_identifier_dump(&bundle->identifier, stream, "  ");
    fputs("  canonical_identifier = ", stream);
    lsreg_identifier_dump(&bundle->canonical_identifier, stream, "  ");
    
    fprintf(stream,
            "  path                 = \"%s\"\n"
            "  name                 = \"%s\"\n"
            "  version              = \"%s\"\n"
            "  type_code            = \"%s\"\n"
            "  executable           = \"%s\"\n"
            "  icon                 = \"%s\"\n"
            "  regdate              = %s\n"
            "  moddate              = %s\n"
            "  library              = \"%s\"\n"
            "  library_items        = "
            ,
            bundle->path,
            bundle->name,
            bundle->version,
            bundle->type_code,
            bundle->executable,
            bundle->icon,
            regdate,
            moddate,
            bundle->library);
    
    free(regdate);
    free(moddate);
    
    if(bundle->library_items) {
      fputs("[\n", stream);
      char *item;
      char **v;
      for( v = bundle->library_items; (item = *v); v++ ) {
        fprintf(stream, "    \"%s\"\n", item);
      }
      fputs("  ]\n", stream);
    }
    else {
      fputs("NULL\n", stream);
    }
    
    fputs("}\n", stream);
  }
}


// Set key and value
int lsreg_bundle_nset(lsreg_bundle_t *bundle, 
                      const char *key, size_t keylen,
                      const char *val, size_t vallen)
{
  int status;
  
  status = 0;
  
  if( (keylen == 4) && (memcmp(key, "path", keylen) == 0) ) {
    bundle->path = strdup(val);
  }
  else if( (keylen == 4) && (memcmp(key, "name", keylen) == 0) ) {
    bundle->name = strdup(val);
  }
  else if( (keylen == 7) && (memcmp(key, "version", keylen) == 0) ) {
    bundle->version = strdup(val);
  }
  else if( (keylen == 9) && (memcmp(key, "type code", keylen) == 0) ) {
    STRCREATE(bundle->type_code, val+1, vallen-2); // remove wrapping "'" chars
  }
  else if( (keylen == 10) && (memcmp(key, "executable", keylen) == 0) ) {
    bundle->executable = strdup(val);
  }
  else if( (keylen == 4) && (memcmp(key, "icon", keylen) == 0) ) {
    bundle->icon = strdup(val);
  }
  else if( (keylen == 8) && (memcmp(key, "mod date", keylen) == 0) ) {
    // input example: "6/26/2006 2:41:56"
    bundle->moddate = (struct tm *)malloc(sizeof(struct tm));
    if(strptime(val, "%m/%d/%Y %T", bundle->moddate) == NULL) {
      free(bundle->moddate);
      bundle->moddate = NULL;
      log_error("Failed to parse date '%s'", val);
    }
  }
  else if( (keylen == 8) && (memcmp(key, "reg date", keylen) == 0) ) {
    bundle->regdate = (struct tm *)malloc(sizeof(struct tm));
    if(strptime(val, "%m/%d/%Y %T", bundle->regdate) == NULL) {
      free(bundle->regdate);
      bundle->regdate = NULL;
      log_error("Failed to parse date '%s'", val);
    }
  }
  else if(((keylen == 10) && (memcmp(key, "identifier", keylen) == 0)) ||
          ((keylen == 12) && (memcmp(key, "canonical id", keylen) == 0)) ){
    
    lsreg_identifier_parse(val, vallen, (keylen == 10) ? &bundle->identifier : &bundle->canonical_identifier);
  }
  else {
    status = 1; // no matching key
  }
  
  return status;
}


// ---------------------------------------------
#pragma mark -
#pragma mark Volume record methods

/*
 char *path;       // Mount path. May not exist.
 char *disk_image; // May be NULL if not a disk_image
 int is_mounted;   // 1 or 0
 int vrefnum;      // May be 0 if not present
 enum kLSRegVolumeFlags flags;
 */

// Initialize volume
void lsreg_volume_init(lsreg_volume_t *s) {
  s->uid = 0;
  s->path = NULL;
  s->disk_image = NULL;
  s->is_mounted = 0;
  s->vrefnum = 0;
  s->flags = 0;
}

// Free all bundle members, but not the volume itself
void lsreg_volume_free_members(lsreg_volume_t *s) {
  if(s->path)           free(s->path);
  if(s->disk_image)     free(s->disk_image);
}

// Free volume and all its members
void lsreg_volume_free(lsreg_volume_t *s) {
  if(s != NULL) {
    lsreg_volume_free_members(s);
    free(s);
  }
}

// Dump bundle, in a human readable format, to stream
void lsreg_volume_dump(lsreg_volume_t *s, FILE *stream) {
  if(s == NULL) {
    fprintf(stream, "<lsreg_volume_t>NULL\n");
  }
  else {
    fprintf(stream,
            "<lsreg_volume_t>{\n"
            "  uid        = %u\n"
            "  path       = \"%s\"\n"
            "  disk_image = \"%s\"\n"
            "  is_mounted = %s\n"
            "  vrefnum    = %d\n"
            "  flags      = 0x%x\n"
            "}\n",
            s->uid,
            s->path,
            s->disk_image,
            s->is_mounted ? "YES" : "NO",
            s->vrefnum,
            s->flags);
  }
}


// ---------------------------------------------
#pragma mark -
#pragma mark Handler record methods

// Initialize volume
void lsreg_handler_init(lsreg_handler_t *s) {
  s->uid = 0;
  s->content_type = NULL;
  s->extension = NULL;
  s->uri_scheme = NULL;
  s->roles.name = NULL;
  s->roles.hash = 0;
  s->options = 0;
}

// Free all bundle members, but not the volume itself
void lsreg_handler_free_members(lsreg_handler_t *s) {
  if(s->content_type) free(s->content_type);
  if(s->extension)    free(s->extension);
  if(s->uri_scheme)  free(s->uri_scheme);
  if(s->roles.name)   free(s->roles.name);
}

// Free volume and all its members
void lsreg_handler_free(lsreg_handler_t *s) {
  if(s != NULL) {
    lsreg_handler_free_members(s);
    free(s);
  }
}

// Dump bundle, in a human readable format, to stream
void lsreg_handler_dump(lsreg_handler_t *s, FILE *stream) {
  fputs("<lsreg_handler_t>", stream);
  if(s == NULL) {
    fputs("NULL\n", stream);
  }
  else {
    fprintf(stream,
            "{\n"
            "  uid          = %u\n"
            "  content_type = \"%s\"\n"
            "  extension    = \"%s\"\n"
            "  uri_scheme   = \"%s\"\n"
            "  roles        = ",
            s->uid,
            s->content_type,
            s->extension,
            s->uri_scheme);
    
    lsreg_identifier_dump(&s->roles, stream, "  ");
    
    fprintf(stream,
            "  options      = 0x%x\n"
            "}\n",
            s->options);
  }
}


// ---------------------------------------------
#pragma mark -
#pragma mark Parsing
  
int lsreg_parse_bundle(FILE *f,
                       char *linebuf, size_t linebufsize,
                       char *line, size_t linelen,
                       char *key, size_t keylen,
                       char *val, size_t vallen,
                       lsreg_rec_t *record)
{
  char *s;
  lsreg_bundle_t *bundle = (lsreg_bundle_t *)record->rec;
  
  // The "library items" key is special
  if( (keylen == 13) && (memcmp(key, "library items", keylen) == 0) ) {
    
    // Copy normal identifier to canonical if same.
    // We know "library items" always comes after canonical id,
    // so if it's not set, we know it does not exist, thus the
    // normal identifier IS canonical.
    if(bundle->canonical_identifier.name == NULL && bundle->identifier.name != NULL) {
      bundle->canonical_identifier.name = strdup(bundle->identifier.name);
      bundle->canonical_identifier.hash = bundle->identifier.hash;
    }
    
    if(vallen) {
      size_t vsize, vlen;
      vsize = 2;
      vlen = 0;
      
      bundle->library_items = (char **)malloc(sizeof(char *)*vsize);
      
      // Add first item
      bundle->library_items[vlen++] = strdup(val);
      
      while( (line = _readline(linebuf, linebufsize, f)) ) {
        linelen = strlen(line);
        // Prefix signature
        // "\t               " 1+15
        if(linelen > 16 && line[0] == '\t' && line[1] == ' ' && line[2] == ' ') {
          if(vlen == vsize) {
            vsize *= 2;
            bundle->library_items = (char **)realloc(bundle->library_items, sizeof(char *)*vsize);
          }
          line = _memltrim(line, &linelen);
          STRCREATE(s, line, linelen-1);
          bundle->library_items[vlen++] = s;
        }
        else {
          // we're done reading library items. Realloc and save.
          bundle->library_items = (char **)realloc(bundle->library_items, sizeof(char *)*(vlen+1));
          bundle->library_items[vlen] = NULL; /* sentinel */
          break;
        }
      } // end while
    }
  } // <- if library items
  // The "properties" key is also special
  else if( (keylen == 10) && (memcmp(key, "properties", keylen) == 0) ) {
    int known_to_be_plist = 0;
    while( (line = _readline(linebuf, linebufsize, f)) ) {
      linelen = strlen(line);
      if(!known_to_be_plist) {
        if(line[0] == '\t') {
          log_error("Expected plist xml document but found new key. This is probably a bug.");
          break;
        }
        known_to_be_plist = 1;
      }
      if( (linelen > 7) && (memcmp(line, "</plist>", 8) == 0) ) {
        // We have now passed the properties chunk
        break;
      }
    }
  }
  else {
    // Set key and value in the bundle struct
    lsreg_bundle_nset(bundle, key, keylen, val, vallen);
  }
  
  return (line == NULL) ? kLSRegParseStatusDone : kLSRegParseStatusContinue;
}



int lsreg_parse_volume(FILE *f,
                       char *linebuf, size_t linebufsize,
                       char *line, size_t linelen,
                       char *key, size_t keylen,
                       char *val, size_t vallen,
                       lsreg_rec_t *record)
{
  /*
   char *path;       // Mount path. May not exist.
   char *disk_image; // May be NULL if not a disk_image
   int is_mounted;   // 1 or 0
   int vrefnum;      // May be 0 if not present
   enum kLSRegVolumeFlags flags;
   */
  
  lsreg_volume_t *vol = (lsreg_volume_t *)record->rec;
  
  if( (keylen == 4) && (memcmp(key, "path", keylen) == 0) ) {
    vol->path = strdup(val);
  }
  else if( (keylen == 10) && (memcmp(key, "disk image", keylen) == 0) ) {
    vol->disk_image = strdup(val);
  }
  else if( (keylen == 5) && (memcmp(key, "state", keylen) == 0) ) {
    vol->is_mounted = (vallen == 7) ? 1 : 0;
  }
  else if( (keylen == 7) && (memcmp(key, "vrefnum", keylen) == 0) ) {
    vol->vrefnum = atoi(val);
  }
  else if( (keylen == 5) && (memcmp(key, "flags", keylen) == 0) ) {
    //vol->flags = atoi(val);
  }
  
  return kLSRegParseStatusContinue;
}



int lsreg_parse_handler(FILE *f,
                       char *linebuf, size_t linebufsize,
                       char *line, size_t linelen,
                       char *key, size_t keylen,
                       char *val, size_t vallen,
                       lsreg_rec_t *record)
{
  /*
   char *content_type; // "com.apple.binhex-archive"
   char *extension;    // "html"
   char *uri_scheme;  // "http"
   lsreg_identifier_t roles;        // Canonical identifier, i.e. "com.apple.ical (0x2ab0080)"
   enum kLSRegHandlerOptions options;
   */
  
  lsreg_handler_t *s = (lsreg_handler_t *)record->rec;
  
  if( (keylen == 12) && (memcmp(key, "content type", keylen) == 0) ) {
    s->content_type = strdup(val);
  }
  else if( (keylen == 9) && (memcmp(key, "extension", keylen) == 0) ) {
    s->extension = strdup(val);
  }
  else if( (keylen == 7) && (memcmp(key, "unknown", keylen) == 0) ) {
    s->uri_scheme = strdup(val);
  }
  else if( (keylen == 9) && (memcmp(key, "all roles", keylen) == 0) ) {
    lsreg_identifier_parse(val, vallen, &s->roles);
  }
  else if( (keylen == 7) && (memcmp(key, "options", keylen) == 0) ) {
    //vol->options = ...
  }
  
  return kLSRegParseStatusContinue;
}



int lsreg_parse_record(FILE *f,
                       char *linebuf, size_t linebufsize,
                       lsreg_rec_t *rec)
{  
  char *line, *keyend, *key, *val;
  size_t linelen, keylen, vallen;
  int passed_main;
  enum kLSRegParseStatus status;
  lsreg_parser_func *parser;
  
  // Init
  passed_main = 0;
  status = kLSRegParseStatusContinue;
  rec->type = kLSRegRecTypeUnknown;
  parser = NULL;
  
  // Detect record type
  if((line = _readline(linebuf, linebufsize, f)) == NULL) {
    return 1; // done!
  }
  linelen = strlen(line);
  if(linelen > 7) {
    // Parse id
    rec->uid = (unsigned int)atoi(strchr(line, ':')+1);
    
    if(memcmp(line, "bundle", 6) == 0) {
      rec->type = kLSRegRecTypeBundle;
      rec->rec = malloc(sizeof(lsreg_bundle_t));
      lsreg_bundle_init((lsreg_bundle_t *)rec->rec);
      ((lsreg_bundle_t *)rec->rec)->uid = rec->uid;
      parser = lsreg_parse_bundle;
    }
    else if(memcmp(line, "volume", 6) == 0) {
      rec->type = kLSRegRecTypeVolume;
      rec->rec = malloc(sizeof(lsreg_volume_t));
      lsreg_volume_init((lsreg_volume_t *)rec->rec);
      ((lsreg_volume_t *)rec->rec)->uid = rec->uid;
      parser = lsreg_parse_volume;
    }
    else if(memcmp(line, "handler", 7) == 0) {
      rec->type = kLSRegRecTypeHandler;
      rec->rec = malloc(sizeof(lsreg_handler_t));
      lsreg_handler_init((lsreg_handler_t *)rec->rec);
      ((lsreg_handler_t *)rec->rec)->uid = rec->uid;
      parser = lsreg_parse_handler;
    }
  }
  
  if(parser == NULL) {
    log_error("Unsupported record type");
    return kLSRegParseStatusDone;
  }
  
  while( (line = _readline(linebuf, linebufsize, f)) ) {
    linelen = strlen(line);
    
    if(linelen == 0) {
      // Skip empty line
      continue;
    }
    
    if(line[0] == '-') {
      // End of section
      break;
    }
    
    if(passed_main) {
      // Passed main info - read off remaining lines
      continue;
    }
    
    if(linelen > 1 && line[0] == '\t' && line[1] == '-') {
      // End of main info
      passed_main = 1;
      continue;
    }
    
    // Now, a line passed all checks down here is probably a key-value pair.
    if((keyend = (char *)memchr(line, ':', linelen)) == NULL) {
      log_error("Unable to parse line '%s'", line);
      continue;
    }
    
    // Find key and value
    keylen = keyend-line;
    vallen = linelen-keylen-1; // -1 is for ':'
    val = line;
    val += keylen+1;
    val = _memltrim(val, &vallen);
    if(vallen) {
      vallen--; // Remove LN and...
      val[vallen] = '\0'; // ...replace it with \0
    }
    key = _memltrim(line, &keylen);
    
    // Handle key-value assignment record type-wise...
    status = parser(f, linebuf, linebufsize,
                    line, linelen,
                    key, keylen,
                    val, vallen,
                    rec);
    
    // If not continue, stop and return
    if(status != kLSRegParseStatusContinue) {
      return status;
    }
  }
  
  return (line == NULL) ? kLSRegParseStatusDone : kLSRegParseStatusContinue;
}


void lsreg_parse(FILE *f) {
  size_t skip_lines;
  enum kLSRegParseStatus status;
  char *linebuf;
  const size_t linebufsize = 1024;
  
  linebuf = (char *)malloc(sizeof(char)*linebufsize+1);
  skip_lines = 3;
  
  while(skip_lines--) {
    if(_readline(linebuf, linebufsize, f) == NULL) {
      // error occured
      free(linebuf);
      return;
    }
  }
  
  // Parse sections
  do {
    
    lsreg_rec_t record;
    lsreg_rec_init(&record);
    status = lsreg_parse_record(f, linebuf, linebufsize, &record);
    lsreg_rec_dump(&record, stdout);
    lsreg_rec_free_members(&record);
    
    /*
        
    lsreg_bundle_t bundle;
    lsreg_bundle_init(&bundle);
    done = lsreg_parse_bundle_section(f, linebuf, linebufsize, &bundle);
    //if(bundle.library_items)
    lsreg_bundle_dump(&bundle, stdout);*/
    
  } while(status == kLSRegParseStatusContinue);
  
  free(linebuf);
}


// Open registry dump
FILE *lsreg_regdump_open() {
  FILE *f;
  if((f = popen(kLSRegisterCmd, "r")) == NULL) {
    int errnum = ferror(f);
    if(errnum) {
      log_error("Failed to open regdump: %s", strerror(errnum));
    } else {
      log_error("Failed to open regdump");
    }
  }
  return f;
}


// Close registry dump
void lsreg_regdump_close(FILE *f) {
  pclose(f);
}


// Iterate records, calling factory_cb and handler_cb for each record.
void lsreg_iterate(lsreg_rec_factory_cb *factory_cb,
                   lsreg_rec_handler_cb *handler_cb,
                   void *something)
{
  FILE *f;
  size_t skip_lines;
  enum kLSRegParseStatus status;
  char *linebuf;
  const size_t linebufsize = 1024;
  
  if((f = lsreg_regdump_open()) == NULL) {
    return;
  }
  linebuf = (char *)malloc(sizeof(char)*linebufsize+1);
  skip_lines = 3;
  
  while(skip_lines--) {
    if(_readline(linebuf, linebufsize, f) == NULL) {
      // error occured
      free(linebuf);
      return;
    }
  }
  
  // Parse sections
  do {
    lsreg_rec_t *record = factory_cb(something);
    lsreg_rec_init(record);
    status = lsreg_parse_record(f, linebuf, linebufsize, record);
    //lsreg_rec_dump(&record, stdout);
    if(handler_cb(record, something)) {
      break;
    }
  } while(status == kLSRegParseStatusContinue);
  
  free(linebuf);
  lsreg_regdump_close(f);
}
