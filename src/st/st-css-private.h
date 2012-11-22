/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * st-css.h: A layer of caching above (ugh) libcroco
 *
 * Copyright 2012 Giovanni Campagna <scampa.giovanni@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU Lesser General Public License,
 * version 2.1, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef __ST_CSS_PRIVATE__
#define __ST_CSS_PRIVATE__

#include <clutter/clutter.h>
#include <libcroco/libcroco.h>

#include "st-css.h"
#include "st-shadow.h"

typedef struct _StStylesheet   StStylesheet;
typedef struct _StStatement    StStatement;
typedef struct _StProperty     StProperty;
typedef struct _StCssValue     StCssValue;

struct _StStylesheet {
  GPtrArray    *stylesheets;
  GPtrArray    *statements;
  GFile        *file;

  guint origin;
};

struct _StProperty {
  int ref_count;
  GQuark property_name;
  guint property_id;
  guint sort_key;

  StCssValue *value;
};

/* A set of special property IDs, that are used in the computation
   of other values */
typedef enum {
  ST_PROPERTY_UNKNOWN,
  ST_PROPERTY_COLOR,
  ST_PROPERTY_FONT_SIZE,
  ST_PROPERTY_N_SPECIALS,
} StSpecialProperty;

StCascade *st_cascade_new  (void);

StStylesheet *st_property_get_stylesheet (StProperty      *property);

GPtrArray    *st_cascade_get_matched_properties (StCascade   *cascade,
                                                 StThemeNode *node);
GPtrArray    *st_cascade_parse_declaration_list (StCascade   *cascade,
                                                 const char  *decl_list);

StProperty *st_property_ref              (StProperty      *prop);
StProperty *st_property_copy             (StProperty      *prop);
void        st_property_unref            (StProperty      *prop);

gulong      st_property_hash             (gconstpointer    prop);
gboolean    st_property_equal_name       (gconstpointer    one,
                                          gconstpointer    two);

gboolean   st_property_parse             (CRDeclaration   *decl,
                                          GPtrArray       *out);
void       st_property_assign            (StProperty      *property,
                                          StThemeNode     *node);

gboolean   st_property_get_double        (StProperty      *property,
                                          StThemeNode     *node,
                                          gdouble         *out);
gboolean   st_property_get_length        (StProperty      *property,
                                          StThemeNode     *node,
                                          float           *out);
gboolean   st_property_get_color         (StProperty      *property,
                                          StThemeNode     *node,
                                          ClutterColor    *out);
gboolean   st_property_get_shadow        (StProperty      *property,
                                          StThemeNode     *node,
                                          StShadow       **out);

// FIXME: need to change this to be ST_ORIGIN_EXTENSION
#define ST_ORIGIN_OFFSET_IMPORTANT (ST_ORIGIN_EXTENSION)
#define ST_ORIGIN_SHIFT            (2 * ST_ORIGIN_EXTENSION)

GFile *    st_resolve_relative_url       (GFile           *parent_file,
                                          const char      *url);


#endif
