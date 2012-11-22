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

const StCssValue st_css_value_initial =
  { NULL, -1, ST_VALUE_KEYWORD, { .keyword = ST_KEYWORD_INITIAL } };
const StCssValue st_css_value_inherit =
  { NULL, -1, ST_VALUE_KEYWORD, { .keyword = ST_KEYWORD_INHERIT } };
static const StCssValue st_css_value_length_zero =
  { NULL, -1, ST_VALUE_LENGTH, { .length = 0 } };

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

static void
st_css_value_shadow_free (StCssValue *self)
{
  st_css_value_unref (self->value.shadow->color);
  st_css_value_unref (self->value.shadow->xoffset);
  st_css_value_unref (self->value.shadow->yoffset);
  st_css_value_unref (self->value.shadow->spread);
  st_css_value_unref (self->value.shadow->blur);

  g_slice_free (StCssShadow, self->value.shadow);
}

static void
st_css_value_file_free (StCssValue *self)
{
  if (self->value.file)
    g_object_unref (self->value.file);
}

static void
st_css_value_complex_free (StCssValue *self)
{
  cr_term_unref (self->value.cr_term);
}

static void
st_css_value_array_free (StCssValue *self)
{
  g_ptr_array_unref (self->value.array);
}

static void
st_css_value_string_free (StCssValue *self)
{
  g_free (self->value.string);
}

StCssValue *
st_css_value_new (StCssValueKind kind)
{
  StCssValue *v;

  v = g_slice_new (StCssValue);
  v->value_kind = kind;

  switch (kind) {
  case ST_VALUE_SHADOW:
    v->value.shadow = g_slice_new (StCssShadow);
    v->free = st_css_value_shadow_free;
    break;

  case ST_VALUE_ARRAY:
    v->value.array = g_ptr_array_new_with_free_func ((GDestroyNotify) st_css_value_unref);
    v->free = st_css_value_array_free;
    break;

  case ST_VALUE_FILE:
    v->free = st_css_value_file_free;
    break;

  case ST_VALUE_COMPLEX:
    v->free = st_css_value_complex_free;
    break;

  case ST_VALUE_STRING:
    v->free = st_css_value_string_free;
    break;

  default:
    v->free = NULL;
  }

  return v;
}

StCssValue *
st_css_value_ref (StCssValue *value)
{
  if (value->ref_count < 0)
    return value;

  value->ref_count++;
  return value;
}

void
st_css_value_unref (StCssValue *value)
{
  if (value->ref_count < 0)
    return;

  if (--value->ref_count == 0)
    {
      if (value->free)
        value->free (value);

      g_slice_free (StCssValue, value);
    }
}

StCssValue *
st_css_value_parse_common (CRTerm **term)
{
  StCssValue *v;

  if ((*term)->type != TERM_IDENT)
    return NULL;

  if ((*term)->next != NULL)
    return NULL;

  if (cr_string_is ((*term)->content.str, "inherit"))
    v = (StCssValue *) &st_css_value_inherit;
  else if (cr_string_is ((*term)->content.str, "initial"))
    v = (StCssValue *) &st_css_value_initial;
  else
    return NULL;

  *term = (*term)->next;
  return v;
}

gboolean
st_css_value_is_initial (StCssValue *value)
{
  return value->value_kind == ST_VALUE_KEYWORD &&
    value->value.keyword == ST_KEYWORD_INITIAL;
}

gboolean
st_css_value_is_inherit (StCssValue *value)
{
  return value->value_kind == ST_VALUE_KEYWORD &&
    value->value.keyword == ST_KEYWORD_INHERIT;
}

StCssValue *
st_css_value_parse_length (CRTerm **terms)
{
  StCssValue *v;
  StCssValueKind kind;
  gfloat factor, multiplier;
  CRTerm *item;

  item = *terms;

  if (item->type != TERM_NUMBER)
    return NULL;

  switch (item->content.num->type)
    {
    case NUM_GENERIC:
      if (item->content.num->val != 0)
        return NULL;
      else
        return (StCssValue*) &st_css_value_length_zero;

    case NUM_LENGTH_PX:
      kind = ST_VALUE_LENGTH;
      multiplier = 0;
      break;

    case NUM_LENGTH_PT:
      kind = ST_VALUE_LENGTH;
      multiplier = 1;
      break;

    case NUM_LENGTH_IN:
      kind = ST_VALUE_LENGTH;
      multiplier = 72;
      break;

    case NUM_LENGTH_CM:
      kind = ST_VALUE_LENGTH;
      multiplier = 72. / 2.54;
      break;

    case NUM_LENGTH_MM:
      kind = ST_VALUE_LENGTH;
      multiplier = 72. / 25.4;
      break;

    case NUM_LENGTH_PC:
      kind = ST_VALUE_LENGTH;
      multiplier = 12. / 25.4;
      break;

    case NUM_LENGTH_EM:
      kind = ST_VALUE_FONT_RELATIVE;
      multiplier = 1;
      break;

    case NUM_LENGTH_EX:
      /* Doing better would require actually resolving the font description
       * to a specific font, and Pango doesn't have an ex metric anyways,
       * so we'd have to try and synthesize it by complicated means.
       *
       * The 0.5em is the CSS spec suggested thing to use when nothing
       * better is available.
       */
      kind = ST_VALUE_FONT_RELATIVE;
      multiplier = 0.5;

    default:
      return NULL;
    }

  v = st_css_value_new (kind);

  factor = item->content.num->val * multiplier;

  if (kind == ST_VALUE_LENGTH)
    {
      if (multiplier == 0)
        v->value.length = item->content.num->val;
      else
        v->value.length = factor * clutter_backend_get_resolution (clutter_get_default_backend ());
    }
  else
    {
      v->value.factor = factor;
    }

  *terms = item->next;
  return v;
}

static gboolean
st_css_value_length_is_negative (StCssValue *v)
{
  switch (v->value_kind)
    {
    case ST_VALUE_LENGTH:
      return v->value.length < 0;
    case ST_VALUE_FONT_RELATIVE:
      return v->value.factor < 0;
    default:
      g_assert_not_reached ();
      return FALSE;
    }
}

StCssValue *
st_css_value_parse_time (CRTerm **term)
{
  CRTerm *item;
  StCssValue *v;
  int time;

  item = *term;

  if (item->type != TERM_NUMBER)
    return NULL;

  switch (item->content.num->type)
    {
    case NUM_TIME_MS:
      time = item->content.num->val;
      break;

    case NUM_TIME_S:
      time = (int)item->content.num->val * 1000;
      break;

    case NUM_GENERIC:
      if (item->content.num->val != 0)
        return NULL;

      time = 0;
      break;

    default:
      return NULL;
    }

  v = st_css_value_new (ST_VALUE_TIME);
  v->value.time = time;

  *term = item->next;
  return v;
}

StCssValue *
st_css_value_parse_float (CRTerm **term)
{
  CRTerm *item;
  StCssValue *v;

  item = *term;

  if (item->type != TERM_NUMBER)
    return NULL;

  if (item->content.num->type != NUM_GENERIC)
    return NULL;

  v = st_css_value_new (ST_VALUE_FLOAT);
  v->value.float_num = item->content.num->val;

  *term = item->next;
  return v;
}

StCssValue *
st_css_value_parse_double (CRTerm **term)
{
  CRTerm *item;
  StCssValue *v;

  item = *term;

  if (item->type != TERM_NUMBER)
    return NULL;

  if (item->content.num->type != NUM_GENERIC)
    return NULL;

  v = st_css_value_new (ST_VALUE_DOUBLE);
  v->value.double_num = item->content.num->val;

  *term = item->next;
  return v;
}

StCssValue *
st_css_value_parse_enum (CRTerm             **term,
                         const char * const  *nicks)
{
  CRTerm *item;
  StCssValue *v;
  const char *nick;
  int value;

  item = *term;

  if (item->type != TERM_IDENT)
    return NULL;

  value = -1;
  for (nick = *nicks; nick; nick++)
    {
      if (*nick && cr_string_is (item->content.str, nick))
        {
          value = nick - *nicks;
          break;
        }
    }

  if (value < 0)
    return NULL;

  v = st_css_value_new (ST_VALUE_ENUM);
  v->value.v_enum = value;

  *term = item->next;
  return v;
}

StCssValue *
st_css_value_parse_shadow (CRTerm **terms)
{
  StCssValue *v;
  CRTerm *item;
  StCssShadow *shadow;
  StCssValue *color = NULL;
  StCssValue *lengths[4];
  int n_lengths = 0;
  gboolean inset = FALSE;
  enum {
    LENGTH = 1,
    INSET = 2,
    COLOR = 4,
    MASK = 4+2+1
  } bits = 0;

  for (item = *terms; item && bits < MASK; )
    {
      if ((bits & COLOR) == 0 &&
          (color = st_css_value_parse_color (&item)))
        {
          bits |= COLOR;
        }
      else if ((bits & INSET) == 0 &&
               item->type == TERM_IDENT)
        {
          if (cr_string_is (item->content.str, "inset"))
            {
              inset = TRUE;
              bits |= INSET;

              item = item->next;
            }
          else
            goto error;
        }
      else if ((bits & LENGTH) == 0 &&
               (lengths[n_lengths] = st_css_value_parse_length (&item)))
        {
          /* Blur radius and spread distance cannot be negative */
          if (n_lengths > 1 &&
              st_css_value_length_is_negative (lengths[n_lengths]))
            goto error;

          n_lengths ++;
          if (n_lengths == 4)
            bits |= LENGTH;
        }
      else
        goto error;
    }

  /* Offsets are required for a <shadow> value */
  if (n_lengths < 2)
    goto error;

  v = st_css_value_new (ST_VALUE_SHADOW);

  shadow = v->value.shadow;
  shadow->inset = inset;

  if (color)
    shadow->color = color;
  else
    shadow->color = (StCssValue*) &st_css_value_color_black;

  switch (n_lengths) {
  case 4:
    shadow->spread = lengths[3];
    shadow->blur = lengths[2];
    shadow->yoffset = lengths[1];
    shadow->xoffset = lengths[0];
    break;

  case 3:
    shadow->spread = (StCssValue*) &st_css_value_length_zero;
    shadow->blur = lengths[2];
    shadow->yoffset = lengths[1];
    shadow->xoffset = lengths[0];
    break;

  case 2:
    shadow->spread = (StCssValue*) &st_css_value_length_zero;
    shadow->blur = (StCssValue*) &st_css_value_length_zero;
    shadow->yoffset = lengths[1];
    shadow->xoffset = lengths[0];
    break;

  default:
    g_assert_not_reached ();
  }

  *terms = item->next;
  return v;

 error:
  {
    int i;

    if (color)
      st_css_value_unref (color);
    for (i = 0; i < n_lengths; i++)
      st_css_value_unref (lengths[i]);

    return NULL;
  }
}

StCssValue *
st_css_value_parse_image (CRTerm **term,
                          GFile   *parent_file)
{
  StCssValue *v;
  CRTerm *item;

  item = *term;

  if (item->type == TERM_IDENT &&
      cr_string_is (item->content.str, "none"))
    {
      v = st_css_value_new (ST_VALUE_FILE);
      v->value.file = NULL;

      *term = item->next;
      return v;
    }

  if (item->type != TERM_URI)
    return NULL;

  v = st_css_value_new (ST_VALUE_FILE);
  v->value.file = st_resolve_relative_url (parent_file, cr_to_string (item->content.str));

  *term = item->next;
  return v;
}

StCssValue *
st_css_value_parse_keyword (CRTerm     **term,
                            const char  *keyword,
                            int          value)
{
  StCssValue *v;
  CRTerm *item;

  item = *term;

  if (item->type != TERM_IDENT ||
      !cr_string_is (item->content.str, keyword))
    return NULL;

  v = st_css_value_new (ST_VALUE_ENUM);
  v->value.v_enum = value;

  *term = item->next;
  return v;
}

void
st_css_value_compute_length (StThemeNode *node,
                             gint         for_property,
                             StCssValue  *value,
                             gfloat      *length)
{
  float factor, font_size;
  const PangoFontDescription *font;

  if (value->value_kind == ST_VALUE_LENGTH)
    {
      *length = value->value.length;
      return;
    }

  g_assert (value->value_kind == ST_VALUE_FONT_RELATIVE);

  factor = value->value.factor;

  if (for_property == ST_PROPERTY_FONT_SIZE)
    {
      StThemeNode *parent = node->parent_node;

      if (parent == NULL)
        font = st_theme_context_get_font (node->context);
      else
        {
          _st_theme_node_ensure_computed (parent);
          font = parent->font_desc;
        }
    }
  else
    {
      /* Don't call st_theme_node_get_font here, as that
         calls ensure_computed causing infinite recusion */
      font = node->font_desc;
    }

  font_size = (float)pango_font_description_get_size (font) / PANGO_SCALE;

  if (!pango_font_description_get_size_is_absolute (font))
    {
      float resolution = clutter_backend_get_resolution (clutter_get_default_backend ());
      font_size = factor * (resolution / 72.);
    }

  *length = factor * font_size;
}

void
st_css_value_compute_time (StThemeNode *node,
                           gint         for_property,
                           StCssValue  *value,
                           gint        *time)
{
  g_assert (value->value_kind == ST_VALUE_TIME);

  *time = value->value.time;
}

void
st_css_value_compute_float (StThemeNode *node,
                            gint         for_property,
                            StCssValue  *value,
                            float       *float_num)
{
  g_assert (value->value_kind == ST_VALUE_FLOAT);

  *float_num = value->value.float_num;
}

void
st_css_value_compute_double (StThemeNode *node,
                             gint         for_property,
                             StCssValue  *value,
                             double      *double_num)
{
  g_assert (value->value_kind == ST_VALUE_DOUBLE);

  *double_num = value->value.double_num;
}

void
st_css_value_compute_image (StThemeNode  *node,
                            gint          for_property,
                            StCssValue   *value,
                            GFile       **image)
{
  g_assert (value->value_kind == ST_VALUE_FILE);

  if (value->value.file)
    *image = g_object_ref (value->value.file);
  else
    *image = NULL;
}

void
st_css_value_compute_shadow (StThemeNode *node,
                             gint         for_property,
                             StCssValue  *value,
                             StShadow    *shadow)
{
  g_assert (value->value_kind == ST_VALUE_SHADOW);

  st_css_value_compute_color (node, for_property,
                              value->value.shadow->color, &shadow->color);
  st_css_value_compute_length (node, for_property,
                               value->value.shadow->xoffset, &shadow->xoffset);
  st_css_value_compute_length (node, for_property,
                               value->value.shadow->yoffset, &shadow->yoffset);
  st_css_value_compute_length (node, for_property,
                               value->value.shadow->blur, &shadow->blur);
  st_css_value_compute_length (node, for_property,
                               value->value.shadow->spread, &shadow->spread);
  shadow->inset = value->value.shadow->inset;
}
