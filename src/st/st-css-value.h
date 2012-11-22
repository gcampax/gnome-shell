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

#ifndef __ST_CSS_VALUE__
#define __ST_CSS_VALUE__

#include <clutter/clutter.h>
#include <libcroco/libcroco.h>

#include "st-css.h"
#include "st-css-private.h"

typedef struct _StCssShadow StCssShadow;

typedef enum {
  ST_VALUE_COMPLEX,
  ST_VALUE_KEYWORD,
  ST_VALUE_COLOR,
  ST_VALUE_LENGTH,
  ST_VALUE_FONT_RELATIVE,
  ST_VALUE_TIME,
  ST_VALUE_ENUM,
  ST_VALUE_SHADOW,
  ST_VALUE_FILE,
  ST_VALUE_ARRAY,
  ST_VALUE_STRING,
  ST_VALUE_FLOAT,
  ST_VALUE_DOUBLE,
  ST_VALUE_INT,
} StCssValueKind;

struct _StCssValue {
  void (*free) (StCssValue *value);
  int ref_count;
  StCssValueKind value_kind;

  union {
    CRTerm *cr_term;     /* ST_VALUE_COMPLEX */
    guint keyword;       /* ST_VALUE_KEYWORD */
    ClutterColor color;  /* ST_VALUE_COLOR */
    gfloat length;       /* ST_VALUE_LENGTH */
    int time;            /* ST_VALUE_TIME */
    gfloat factor;       /* ST_VALUE_FONT_RELATIVE */
    guint v_enum;        /* ST_VALUE_ENUM */
    StCssShadow *shadow; /* ST_VALUE_SHADOW */
    GFile *file;         /* ST_VALUE_FILE */
    GPtrArray *array;    /* ST_VALUE_ARRAY */
    char *string;        /* ST_VALUE_STRING */
    float float_num;     /* ST_VALUE_FLOAT */
    double double_num;   /* ST_VALUE_DOUBLE */
    int integer;         /* ST_VALUE_INT */
  } value;
};

struct _StCssShadow {
  StCssValue *color;
  StCssValue *xoffset;
  StCssValue *yoffset;
  StCssValue *blur;
  StCssValue *spread;
  gboolean inset;
};

typedef enum {
  ST_KEYWORD_INVALID,
  ST_KEYWORD_INHERIT,
  ST_KEYWORD_INITIAL,
  ST_KEYWORD_CURRENT_COLOR,
} StKeyword;

typedef enum {
  ST_FONT_SCALE_XX_SMALL,
  ST_FONT_SCALE_X_SMALL,
  ST_FONT_SCALE_SMALL,
  ST_FONT_SCALE_MEDIUM,
  ST_FONT_SCALE_LARGE,
  ST_FONT_SCALE_X_LARGE,
  ST_FONT_SCALE_XX_LARGE,
  ST_FONT_SCALE_LARGER,
  ST_FONT_SCALE_SMALLER,
} StFontScale;

typedef enum {
  ST_FONT_WEIGHT_NORMAL,
  ST_FONT_WEIGHT_BOLD,
  ST_FONT_WEIGHT_BOLDER,
  ST_FONT_WEIGHT_LIGHTER
} StFontWeight;

StCssValue *st_css_value_new          (StCssValueKind  kind);
StCssValue *st_css_value_ref          (StCssValue     *value);
void        st_css_value_unref        (StCssValue     *value);

extern const StCssValue st_css_value_color_transparent;
extern const StCssValue st_css_value_color_black;
extern const StCssValue st_css_value_initial;
extern const StCssValue st_css_value_inherit;

StCssValue *st_css_value_parse_common       (CRTerm **term);
StCssValue *st_css_value_parse_color        (CRTerm **term);
StCssValue *st_css_value_parse_length       (CRTerm **term);
StCssValue *st_css_value_parse_time         (CRTerm **term);
StCssValue *st_css_value_parse_shadow       (CRTerm **term);
StCssValue *st_css_value_parse_float        (CRTerm **term);
StCssValue *st_css_value_parse_double       (CRTerm **term);
StCssValue *st_css_value_parse_enum         (CRTerm **term,
                                             const char * const  *nicks);
StCssValue *st_css_value_parse_image        (CRTerm **term,
                                             GFile   *parent_file);
StCssValue *st_css_value_parse_keyword      (CRTerm     **term,
                                             const char  *keyword,
                                             int          value);

gboolean    st_css_value_is_inherit (StCssValue *value);
gboolean    st_css_value_is_initial (StCssValue *value);

void        st_css_value_compute_color        (StThemeNode *node, int for_property, StCssValue *value, ClutterColor  *color);
void        st_css_value_compute_length       (StThemeNode *node, int for_property, StCssValue *value, gfloat        *length);
void        st_css_value_compute_time         (StThemeNode *node, int for_property, StCssValue *value, gint          *time);
void        st_css_value_compute_shadow       (StThemeNode *node, int for_property, StCssValue *value, StShadow      *shadow);
void        st_css_value_compute_enum         (StThemeNode *node, int for_property, StCssValue *value, guint         *v_enum);
void        st_css_value_compute_image        (StThemeNode *node, int for_property, StCssValue *value, GFile        **image);
void        st_css_value_compute_float        (StThemeNode *node, int for_property, StCssValue *value, float         *float_num);
void        st_css_value_compute_double       (StThemeNode *node, int for_property, StCssValue *value, double        *double_num);
#endif
