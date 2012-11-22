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

#include "config.h"

#include <string.h>

#include "st-theme-context.h"
#include "st-css-private.h"
#include "st-theme-node-private.h"
#include "st-css-value.h"

const StCssValue st_css_value_color_black =
  { NULL, -1, ST_VALUE_COLOR, { .color = { 0, 0, 0, 255 } } };
const StCssValue st_css_value_color_transparent =
  { NULL, -1, ST_VALUE_COLOR, { .color = { 0, 0, 0, 0 } } };
const StCssValue st_css_value_color_currentColor =
  { NULL, -1, ST_VALUE_KEYWORD, { .keyword = ST_KEYWORD_CURRENT_COLOR } };

static inline char *
cr_to_string (CRString *string)
{
  return string->stryng->str;
}

static inline gboolean
cr_string_is (CRString *string, const char *cmp)
{
  return g_ascii_strcasecmp (cr_to_string (string), cmp) == 0;
}

static inline void
clutter_color_init_from_cr_rgb (ClutterColor *color,
                                CRRgb        *rgb)
{
  if (rgb->is_transparent)
    clutter_color_init (color, 0, 0, 0, 0);
  else if (rgb->is_percentage)
    clutter_color_init (color,
                        rgb->red * 255 / 100,
                        rgb->green * 255 / 100,
                        rgb->blue * 255 / 100,
                        255);
  else
    clutter_color_init (color,
                        rgb->red, rgb->green, rgb->blue,
                        255);
}

static gboolean
cr_term_read_color_components (CRTerm *term,
                               int     n,
                               float  *out)
{
  int i;

  g_assert (n <= 4);

  for (i = 0; i < n; i++)
    {
      if ((i == 0 && term->the_operator != NO_OP) ||
          (i > 0 && term->the_operator != COMMA))
        return FALSE;

      if (term->type != TERM_NUMBER)
        return FALSE;

      if (n < 4)
        {
          if (term->content.num->type == NUM_PERCENTAGE)
            out[i] = term->content.num->val / 100;
          else if (term->content.num->type == NUM_GENERIC)
            out[i] = term->content.num->val / 255;
          else
            return FALSE;
        }
      else
        {
          if (term->content.num->type != NUM_GENERIC)
            out[i] = CLAMP (1.0, term->content.num->val, 0.0);
        }
    }

  return TRUE;
}

static inline gboolean
clutter_color_init_from_cr_rgba (ClutterColor *color,
                                 CRTerm       *term)
{
  float c[4];

  if (!cr_term_read_color_components (term, 4, c))
    return FALSE;

  clutter_color_init (color,
                      c[0] * 255, c[1] * 255, c[2] * 255, c[3] * 255);
  return TRUE;
}

/* Translated literally from the CSS3 specification */
static inline float
hue_to_rgb (float m1,
            float m2,
            float h)
{
  if (h < 0)
    h = h + 1;
  if (h > 1)
    h = h - 1;
  if (h * 6 < 1)
    return m1 + (m2 - m1) * h * 6;
  if (h * 2 < 1)
    return m2;
  if (h * 3 < 2)
    return m1 + (m2 - m1) * (2/3 - h) * 6;
  return m1;
}

static inline void
hsl_to_rgb (float *hsl,
            float *rgb)
{
  float m2, m1;

  if (hsl[2] <= 0.5)
    m2 = hsl[2] * (hsl[1] + 1);
  else
    m2 = hsl[1] + hsl[2] - hsl[1] * hsl[2];

  m1 = hsl[2]*2 - m2;
  rgb[0] = hue_to_rgb(m1, m2, hsl[0]+1/3);
  rgb[1] = hue_to_rgb(m1, m2, hsl[0]);
  rgb[2] = hue_to_rgb(m1, m2, hsl[0]-1/3);
}

static inline gboolean
clutter_color_init_from_cr_hsl (ClutterColor *color,
                                CRTerm       *term)
{
  float hsl[3];
  float rgb[3];

  if (!cr_term_read_color_components (term, 3, hsl))
    return FALSE;

  hsl_to_rgb (hsl, rgb);

  clutter_color_init (color,
                      rgb[0] * 255, rgb[1] * 255, rgb[2] * 255,
                      255);
  return TRUE;
}

static inline gboolean
clutter_color_init_from_cr_hsla (ClutterColor *color,
                                 CRTerm       *term)
{
  float hsla[4];
  float rgb[3];

  if (!cr_term_read_color_components (term, 4, hsla))
    return FALSE;

  hsl_to_rgb (hsla, rgb);

  clutter_color_init (color,
                      rgb[0] * 255, rgb[1] * 255, rgb[2] * 255,
                      hsla[3] * 255);
  return TRUE;
}

StCssValue *
st_css_value_parse_color (CRTerm **terms)
{
  StCssValue *v;
  CRTerm *term;
  CRRgb rgb;

  term = *terms;

  if (term->type == TERM_IDENT &&
      cr_string_is (term->content.str, "currentColor"))
    return (StCssValue*) &st_css_value_color_currentColor;

  /* Not a color -> no match */
  if (term->type != TERM_RGB &&
      term->type != TERM_IDENT &&
      term->type != TERM_HASH &&
      term->type != TERM_FUNCTION)
    return NULL;

  v = st_css_value_new (ST_VALUE_COLOR);

  switch (term->type) {
  case TERM_RGB:
    clutter_color_init_from_cr_rgb (&v->value.color, term->content.rgb);
    break;

  case TERM_IDENT:
    if (cr_rgb_set_from_name (&rgb, (guchar*) cr_to_string (term->content.str)) != CR_OK)
      goto error;

    clutter_color_init_from_cr_rgb (&v->value.color, &rgb);
    break;

  case TERM_HASH:
    if (cr_rgb_set_from_hex_str (&rgb, (guchar*) cr_to_string (term->content.str)) != CR_OK)
      goto error;

    clutter_color_init_from_cr_rgb (&v->value.color, &rgb);
    break;

  case TERM_FUNCTION:
    if (cr_string_is (term->content.str, "rgba"))
      {
        if (!clutter_color_init_from_cr_rgba (&v->value.color, term->ext_content.func_param))
          goto error;
      }
    else if (cr_string_is (term->content.str, "hsl"))
      {
        if (!clutter_color_init_from_cr_hsl (&v->value.color, term->ext_content.func_param))
          goto error;
      }
    else if (cr_string_is (term->content.str, "hsla"))
      {
        if (!clutter_color_init_from_cr_hsla (&v->value.color, term->ext_content.func_param))
          goto error;
      }
    else
      goto error;

  default:
    /* Can't really happen, just prevent a bunch of warnings */
    g_assert_not_reached ();
    goto error;
  }

  *terms = term->next;
  return v;

 error:
  g_slice_free (StCssValue, v);
  return NULL;
}

void
st_css_value_compute_color (StThemeNode  *node,
                            gint          for_property,
                            StCssValue   *v,
                            ClutterColor *color)
{
  const ClutterColor *current_color;

  switch (v->value_kind)
    {
    case ST_VALUE_KEYWORD:
      g_assert (v->value.keyword == ST_KEYWORD_CURRENT_COLOR);

      if (for_property == ST_PROPERTY_COLOR)
        {
          StThemeNode *parent = node->parent_node;

          if (parent == NULL)
            {
              current_color = clutter_color_get_static (CLUTTER_COLOR_BLACK);
            }
          else
            {
              _st_theme_node_ensure_computed (parent);
              current_color = &parent->foreground_color;
            }
        }
      else
        {
          current_color = &node->foreground_color;
        }

      memcpy (color, current_color, sizeof(ClutterColor));
      break;

    case ST_VALUE_COLOR:
      memcpy (color, &v->value.color, sizeof(ClutterColor));
      break;

    default:
      g_assert_not_reached ();
    }
}
