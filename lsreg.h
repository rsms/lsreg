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

/*
 *  lsreg
 *  Launch Services Registry access
 */

#ifndef LSREG_H
#define LSREG_H
#define LIBLSREG_VERSION "0.1.0"

#include <stdio.h>

#pragma mark -
#pragma mark Constants

// The underlying lsregister command
extern const char *kLSRegisterCmd;

// Record types
enum kLSRegRecType {
  kLSRegRecTypeUnknown = 0,
  kLSRegRecTypeBundle,
  kLSRegRecTypeVolume,
  kLSRegRecTypeHandler
};

// Parser status
enum kLSRegParseStatus {
  kLSRegParseStatusContinue = 0,
  kLSRegParseStatusDone
};

// Volume record flags
enum kLSRegVolumeFlags {
  kLSRegVolumeLocalFlag = 1,
  kLSRegVolumeDiskImageFlag = 2,
  kLSRegVolumeSystemDeviceFlag = 4
};

// Handler record options
enum kLSRegHandlerOptions {
  kLSRegHandlerIgnoreCreator = 1
};

#pragma mark -
#pragma mark Types

// Record container
typedef struct {
  unsigned int uid; // registry database unique id
  enum kLSRegRecType type;
  void *rec;
} lsreg_rec_t;

// Identifier
typedef struct {
  char *name; // "foo.bar.SomeThing"
  unsigned int hash; // 0x8000a10b
} lsreg_identifier_t;

// Bundle record
typedef struct {
  unsigned int uid;       // registry database unique id
  lsreg_identifier_t identifier;           // "foo.bar.SomeThing", 0x8000a10b
  lsreg_identifier_t canonical_identifier; // "foo.bar.something", 0x8000e20a
  char *path;            // /Applications/Foo Bar.app
  char *name;            // "Foo Bar"
  char *version;         // Might be "123", "1.2.3" (or anything, really)
  char *type_code;       // "APPL"
  char *executable;      // "Contents/MacOS/Slides"
  char *icon;            // "Contents/Resources/PPIcon.icns"
  struct tm *regdate;    // registration time
  struct tm *moddate;    // modification time
  char *library;         // "Contents/Library/"
  char **library_items;  // NULL terminated list of "library items". NULL if no items.
} lsreg_bundle_t;

// Volume record
typedef struct {
  unsigned int uid;  // registry database unique id
  char *path;       // Mount path. May not exist.
  char *disk_image; // May be NULL if not a disk_image
  int is_mounted;   // 1 or 0
  int vrefnum;      // May be 0 if not present
  enum kLSRegVolumeFlags flags;
} lsreg_volume_t;

// Handler record
typedef struct {
  unsigned int uid;    // registry database unique id
  char *content_type; // "com.apple.binhex-archive"
  char *extension;    // "html"
  char *uri_scheme;  // "http"
  lsreg_identifier_t roles;        // Canonical identifier, i.e. "com.apple.ical (0x2ab0080)"
  enum kLSRegHandlerOptions options;
} lsreg_handler_t;

// Record factory callback.
// If an old record is reused, make sure to call lsreg_rec_free_members()
// on it, before returning it using this callback.
// 
// See: lsreg_iterate()
typedef lsreg_rec_t *lsreg_rec_factory_cb(void *something);

// Record handler callback.
// Iteration ends on non-zero return.
// You are responsible for freeing the record, whis is the one which 
// was previously returned/created by lsreg_rec_factory_cb()
// 
// See: lsreg_iterate()
typedef int lsreg_rec_handler_cb(lsreg_rec_t *record, void *something);


#pragma mark -
#pragma mark Record methods

// Initialize record
void lsreg_rec_init(lsreg_rec_t *s);

// Dump a record, in a human readable format, to stream
void lsreg_rec_dump(lsreg_rec_t *s, FILE *stream);

// Free record members, but not the record itself
void lsreg_rec_free_members(lsreg_rec_t *s);

// Free record and all it's members
void lsreg_rec_free(lsreg_rec_t *s);


#pragma mark -
#pragma mark Identifier methods

int lsreg_identifier_parse(const char *ptr, size_t length, lsreg_identifier_t *s);

// Dump to stream in a human readable format
void lsreg_identifier_dump(lsreg_identifier_t *s, FILE *stream, const char *indent);


#pragma mark -
#pragma mark Bundle record methods

// Initialize bundle
void lsreg_bundle_init(lsreg_bundle_t *s);

// Free all bundle members, but not the bundle itself
void lsreg_bundle_free_members(lsreg_bundle_t *s);

// Free bundle and all its members
void lsreg_bundle_free(lsreg_bundle_t *s);

// Dump bundle, in a human readable format, to stream
void lsreg_bundle_dump(lsreg_bundle_t *bundle, FILE *stream);

// Set value for key on a bundle.
// This effectively parses the value into the internal format.
// For example, if key is "identifier" the value should be in the
// format <STRING> " " "(" <HEXADECIMAL> ")" <NL>
// which will put the string into identifier.name and HEX into
// identifier.hash.
int lsreg_bundle_nset(lsreg_bundle_t *bundle, 
                      const char *key, size_t keylen,
                      const char *val, size_t vallen);


#pragma mark -
#pragma mark Volume record methods

// Initialize volume
void lsreg_volume_init(lsreg_volume_t *s);

// Free all volume members, but not the volume itself
void lsreg_volume_free_members(lsreg_volume_t *s);

// Free volume and all its members
void lsreg_volume_free(lsreg_volume_t *s);

// Dump volume, in a human readable format, to stream
void lsreg_volume_dump(lsreg_volume_t *s, FILE *stream);


#pragma mark -
#pragma mark Handler record methods

// Initialize handler
void lsreg_handler_init(lsreg_handler_t *s);

// Free all handler members, but not the handler itself
void lsreg_handler_free_members(lsreg_handler_t *s);

// Free handler and all its members
void lsreg_handler_free(lsreg_handler_t *s);

// Dump handler, in a human readable format, to stream
void lsreg_handler_dump(lsreg_handler_t *s, FILE *stream);


#pragma mark -
#pragma mark Parsing

// Parse a line of bundle record data
int lsreg_parse_bundle(FILE *f,
                       char *linebuf, size_t linebufsize,
                       char *line, size_t linelen,
                       char *key, size_t keylen,
                       char *val, size_t vallen,
                       lsreg_rec_t *record);

// Parse a line of volume record data
int lsreg_parse_volume(FILE *f,
                       char *linebuf, size_t linebufsize,
                       char *line, size_t linelen,
                       char *key, size_t keylen,
                       char *val, size_t vallen,
                       lsreg_rec_t *record);

// Parse a record beginning on the current line
int lsreg_parse_record(FILE *f,
                       char *linebuf, size_t linebufsize,
                       lsreg_rec_t *rec);

// Parse lsregistry output
void lsreg_parse(FILE *f);

// Open registry dump
FILE *lsreg_regdump_open();

// Close registry dump
void lsreg_regdump_close(FILE *f);

// Iterate records, calling factory_cb and handler_cb for each record.
void lsreg_iterate(lsreg_rec_factory_cb *factory_cb,
                   lsreg_rec_handler_cb *handler_cb,
                   void *something);

// Convenience function which dumps everything in the registry to stdout.
void lsreg_dump();

#endif
