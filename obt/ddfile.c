/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   obt/ddfile.c for the Openbox window manager
   Copyright (c) 2009        Dana Jansens

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   See the COPYING file for a copy of the GNU General Public License.
*/

#include "obt/ddfile.h"
#include <glib.h>
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_STDIO_H
#include <stdio.h>
#endif

typedef struct _ObtDDParse {
    gchar *filename;
    gulong lineno;
} ObtDDParse;

typedef enum {
    DATA_STRING,
    DATA_LOCALESTRING,
    DATA_BOOLEAN,
    DATA_NUMERIC,
    NUM_DATA_TYPES
} ObtDDDataType;

struct _ObtDDFile {
    guint ref;

    ObtDDFileType type;
    gchar *name; /*!< Specific name for the object (eg Firefox) */
    gchar *generic; /*!< Generic name for the object (eg Web Browser) */
    gchar *comment; /*!< Comment/description to display for the object */
    gchar *icon; /*!< Name/path for an icon for the object */

    union _ObtDDFileData {
        struct {
            gchar *exec; /*!< Executable to run for the app */
            gchar *wdir; /*!< Working dir to run the app in */
            gboolean term; /*!< Run the app in a terminal or not */
            ObtDDFileAppOpen open;

            /* XXX gchar**? or something better, a mime struct.. maybe
               glib has something i can use. */
            gchar **mime; /*!< Mime types the app can open */

            ObtDDFileAppStartup startup;
            gchar *startup_wmclass;
        } app;
        struct {
            gchar *url;
        } link;
        struct {
        } dir;
    } d;
};

static void parse_error(const gchar *m, const ObtDDParse *const parse,
                        gboolean *error)
{
    if (!parse->filename)
        g_warning("%s at line %lu of input\n", m, parse->lineno);
    else
        g_warning("%s at line %lu of file %s\n",
                  m, parse->lineno, parse->filename);
    if (error) *error = TRUE;
}

/* reads an input string, strips out invalid stuff, and parses
   backslash-stuff */
static gchar* parse_string(const gchar *in, gboolean locale,
                           const ObtDDParse *const parse,
                           gboolean *error)
{
    const gint bytes = strlen(in);
    gboolean backslash;
    gchar *out, *o;
    const gchar *end, *i;

    g_return_val_if_fail(in != NULL, NULL);

    if (!locale) {
        end = in + bytes;
        for (i = in; i < end; ++i) {
            if (*i > 126 || *i < 32) { /* non-control character ascii */
                end = i;
                parse_error("Invalid bytes in string", parse, error);
                break;
            }
        }
    }
    else if (!g_utf8_validate(in, bytes, &end))
        parse_error("Invalid bytes in localestring", parse, error);

    out = g_new(char, bytes + 1);
    i = in; o = out;
    backslash = FALSE;
    while (i < end) {
        const gchar *next = locale ? g_utf8_find_next_char(i, end) : i+1;
        if (backslash) {
            switch(*i) {
            case 's': *o++ = ' '; break;
            case 'n': *o++ = '\n'; break;
            case 't': *o++ = '\t'; break;
            case 'r': *o++ = '\r'; break;
            case '\\': *o++ = '\\'; break;
            default:
                parse_error((locale ?
                             "Invalid escape sequence in localestring" :
                             "Invalid escape sequence in string"),
                            parse, error);
            }
            backslash = FALSE;
        }
        else if (*i == '\\')
            backslash = TRUE;
        else if (*i >= 127 || *i < 32) { /* avoid ascii control characters */
            parse_error("Found control character in string", parse, error);
            break;
        }
        else {
            memcpy(o, i, next-i);
            o += next-i;
        }
        i = next;
    }
    *o = '\0';
    return o;
}

static gboolean parse_bool(const gchar *in, const ObtDDParse *const parse,
                           gboolean *error)
{
    if (strcmp(in, "true") == 0)
        return TRUE;
    else if (strcmp(in, "false") != 0)
        parse_error("Invalid boolean value", parse, error);
    return FALSE;
}

static float parse_numeric(const gchar *in, const ObtDDParse *const parse,
    gboolean *error)
{
    float out = 0;
    if (sscanf(in, "%f", &out) == 0)
        parse_error("Invalid numeric value", parse, error);
    return out;
}

gboolean parse_file_line(FILE *f, gchar **buf, gulong *size, gulong *read,
                         ObtDDParse *parse, gboolean *error)
{
    const gulong BUFMUL = 80;
    size_t ret;
    gulong i, null;

    if (*size == 0) {
        g_assert(*read == 0);
        *size = BUFMUL;
        *buf = g_new(char, *size);
    }

    /* remove everything up to a null zero already in the buffer and shift
       the rest to the front */
    null = *size;
    for (i = 0; i < *read; ++i) {
        if (null < *size)
            (*buf)[i-null-1] = (*buf)[i];
        else if ((*buf)[i] == '\0')
            null = i;
    }
    if (null < *size)
        *read -= null + 1;

    /* is there already a newline in the buffer? */
    for (i = 0; i < *read; ++i)
        if ((*buf)[i] == '\n') {
            /* turn it into a null zero and done */
            (*buf)[i] = '\0';
            return TRUE;
        }

    /* we need to read some more to find a newline */
    while (TRUE) {
        gulong eol;
        gchar *newread;

        newread = *buf + *read;
        ret = fread(newread, sizeof(char), *size-*read, f);
        if (ret < *size - *read && !feof(f)) {
            parse_error("Error reading", parse, error);
            return FALSE;
        }
        *read += ret;

        /* strip out null zeros in the input and look for an endofline */
        null = 0;
        eol = *size;
        for (i = newread-*buf; i < *read; ++i) {
            if (null > 0)
                (*buf)[i] = (*buf)[i+null];
            if ((*buf)[i] == '\0') {
                ++null;
                --(*read);
                --i; /* try again */
            }
            else if ((*buf)[i] == '\n' && eol == *size) {
                eol = i;
                /* turn it into a null zero */
                (*buf)[i] = '\0';
            }
        }

        if (eol != *size)
            /* found an endofline, done */
            break;
        else if (feof(f) && *read < *size) {
            /* found the endoffile, done (if there is space) */
            if (*read > 0) {
                /* stick a null zero on if there is test on the last line */
                (*buf)[(*read)++] = '\0';
            }
            break;
        }
        else {
            /* read more */
            size += BUFMUL;
            *buf = g_renew(char, *buf, *size);
        }
    }
    return *read > 0;
}

static gboolean parse_file(ObtDDFile *dd, FILE *f, ObtDDParse *parse)
{
    gchar *buf = NULL;
    gulong bytes = 0, read = 0;
    gboolean error = FALSE;

    while (parse_file_line(f, &buf, &bytes, &read, parse, &error)) {
        /* XXX use the string in buf */
        ++parse->lineno;
    }

    if (buf) g_free(buf);
    return !error;
}

ObtDDFile* obt_ddfile_new_from_file(const gchar *name, GSList *paths)
{
    ObtDDFile *dd;
    ObtDDParse parse;
    GSList *it;
    FILE *f;

    dd = g_slice_new(ObtDDFile);
    dd->ref = 1;

    f = NULL;
    for (it = paths; it && !f; it = g_slist_next(it)) {
        gchar *path = g_strdup_printf("%s/%s", (char*)it->data, name);
        if ((f = fopen(path, "r"))) {
            parse.filename = path;
            parse.lineno = 0;
            if (!parse_file(dd, f, &parse)) f = NULL;
        }
    }
    if (!f) {
        obt_ddfile_unref(dd);
        dd = NULL;
    }
    return dd;
}

void obt_ddfile_ref(ObtDDFile *dd)
{
    ++dd->ref;
}

void obt_ddfile_unref(ObtDDFile *dd)
{
    if (--dd->ref < 1) {
        g_slice_free(ObtDDFile, dd);
    }
}
