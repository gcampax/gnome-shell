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

#include <string.h>

#include "st-theme-context.h"
#include "st-css-private.h"
#include "st-theme-node-private.h"
#include "st-css-value.h"

typedef enum {
  ST_PARSER_NOT_SIDED = 1,
} StParserFlags;

typedef struct _StPropertyClass StPropertyClass;

typedef gboolean (*StPropertyParser) (const StPropertyClass  *klass,
                                      CRDeclaration          *decl,
                                      CRTerm                **term,
                                      StParserFlags           flags,
                                      GPtrArray              *out);
typedef void (*StPropertyAssigner) (const StPropertyClass *klass,
                                    StProperty            *prop,
                                    StThemeNode           *node);

struct _StPropertyClass {
  const char *name;

  StPropertyParser parse;
  StPropertyAssigner assign;

  /* For simple properties */
  guint field;
  int side;
};

/* Simple types */
static void st_assign_color_property (const StPropertyClass *klass, StProperty *prop, StThemeNode *node);
static void st_assign_image_property (const StPropertyClass *klass, StProperty *prop, StThemeNode *node);
static void st_assign_length_property (const StPropertyClass *klass, StProperty *prop, StThemeNode *node);
static void st_assign_shadow_property (const StPropertyClass *klass, StProperty *prop, StThemeNode *node);
static void st_assign_time_property (const StPropertyClass *klass, StProperty *prop, StThemeNode *node);
static void st_assign_enum_property (const StPropertyClass *klass, StProperty *prop, StThemeNode *node);
static gboolean st_parse_unknown_property (const StPropertyClass *klass, CRDeclaration *decl, CRTerm **term, StParserFlags flags, GPtrArray *out);
static gboolean st_parse_color_property (const StPropertyClass *klass, CRDeclaration *decl, CRTerm **term, StParserFlags flags, GPtrArray *out);
static gboolean st_parse_image_property (const StPropertyClass *klass, CRDeclaration *decl, CRTerm **term, StParserFlags flags, GPtrArray *out);
static gboolean st_parse_length_property (const StPropertyClass *klass, CRDeclaration *decl, CRTerm **term, StParserFlags flags, GPtrArray *out);
static gboolean st_parse_shadow_property (const StPropertyClass *klass, CRDeclaration *decl, CRTerm **term, StParserFlags flags, GPtrArray *out);
static gboolean st_parse_time_property (const StPropertyClass *klass, CRDeclaration *decl, CRTerm **term, StParserFlags flags, GPtrArray *out);

/* Generic compound parsers */
static gboolean st_parse_compound_property (CRDeclaration *decl, CRTerm **term, const char * const *properties, StParserFlags flags, GPtrArray *out);
static gboolean st_parse_sided_property (const StPropertyClass *klass, CRDeclaration *decl, CRTerm **term, StParserFlags flags, GPtrArray *out);

/* Ad-hoc properties */
static gboolean st_parse_background_position (const StPropertyClass *klass, CRDeclaration *decl, CRTerm **term, StParserFlags flags, GPtrArray *out);
static gboolean st_parse_background_size (const StPropertyClass *klass, CRDeclaration *decl, CRTerm **term, StParserFlags flags, GPtrArray *out);
static gboolean st_parse_background_repeat (const StPropertyClass *klass, CRDeclaration *decl, CRTerm **term, StParserFlags flags, GPtrArray *out);
static gboolean st_parse_background_gradient_direction (const StPropertyClass *klass, CRDeclaration *decl, CRTerm **term, StParserFlags flags, GPtrArray *out);
static gboolean st_parse_border_image_slice (const StPropertyClass *klass, CRDeclaration *decl, CRTerm **term, StParserFlags flags, GPtrArray *out);
static gboolean st_parse_font_variant (const StPropertyClass *klass, CRDeclaration *decl, CRTerm **term, StParserFlags flags, GPtrArray *out);
static gboolean st_parse_font_family (const StPropertyClass *klass, CRDeclaration *decl, CRTerm **term, StParserFlags flags, GPtrArray *out);
static gboolean st_parse_font_style (const StPropertyClass *klass, CRDeclaration *decl, CRTerm **term, StParserFlags flags, GPtrArray *out);
static gboolean st_parse_font_size (const StPropertyClass *klass, CRDeclaration *decl, CRTerm **term, StParserFlags flags, GPtrArray *out);
static gboolean st_parse_font_weight (const StPropertyClass *klass, CRDeclaration *decl, CRTerm **term, StParserFlags flags, GPtrArray *out);
static gboolean st_parse_text_decoration (const StPropertyClass *klass, CRDeclaration *decl, CRTerm **term, StParserFlags flags, GPtrArray *out);
static gboolean st_parse_text_align (const StPropertyClass *klass, CRDeclaration *decl, CRTerm **term, StParserFlags flags, GPtrArray *out);

static void st_assign_background_position (const StPropertyClass *klass, StProperty *prop, StThemeNode *node);
static void st_assign_background_size (const StPropertyClass *klass, StProperty *prop, StThemeNode *node);
static void st_assign_background_repeat (const StPropertyClass *klass, StProperty *prop, StThemeNode *node);
static void st_assign_border_image_slice (const StPropertyClass *klass, StProperty *prop, StThemeNode *node);
static void st_assign_font_family (const StPropertyClass *klass, StProperty *prop, StThemeNode *node);
static void st_assign_font_size (const StPropertyClass *klass, StProperty *prop, StThemeNode *node);
static void st_assign_font_weight (const StPropertyClass *klass, StProperty *prop, StThemeNode *node);
static void st_assign_font_variant (const StPropertyClass *klass, StProperty *prop, StThemeNode *node);
static void st_assign_font_style (const StPropertyClass *klass, StProperty *prop, StThemeNode *node);

/* Shorthand properties */
static gboolean st_parse_outline (const StPropertyClass *klass, CRDeclaration *decl, CRTerm **term, StParserFlags flags, GPtrArray *out);
static gboolean st_parse_border (const StPropertyClass *klass, CRDeclaration *decl, CRTerm **term, StParserFlags flags, GPtrArray *out);
static gboolean st_parse_background (const StPropertyClass *klass, CRDeclaration *decl, CRTerm **term, StParserFlags flags, GPtrArray *out);
static gboolean st_parse_font (const StPropertyClass *klass, CRDeclaration *decl, CRTerm **term, StParserFlags flags, GPtrArray *out);
static gboolean st_parse_border_image (const StPropertyClass *klass, CRDeclaration *decl, CRTerm **term, StParserFlags flags, GPtrArray *out);

#define SIDED_PROPERTY(pre_name, post_name, field_name, kind)           \
  { #pre_name "-top" #post_name, st_parse_##kind##_property, st_assign_##kind##_property, offsetof(StThemeNode, field_name), ST_SIDE_TOP }, \
  { #pre_name "-right" #post_name, st_parse_##kind##_property, st_assign_##kind##_property, offsetof(StThemeNode, field_name), ST_SIDE_RIGHT }, \
  { #pre_name "-bottom" #post_name, st_parse_##kind##_property, st_assign_##kind##_property, offsetof(StThemeNode, field_name), ST_SIDE_BOTTOM }, \
  { #pre_name "-left" #post_name, st_parse_##kind##_property, st_assign_##kind##_property, offsetof(StThemeNode, field_name), ST_SIDE_LEFT }, \
  { #pre_name #post_name, st_parse_sided_property, NULL, 0, -1 }

#define SIMPLE_SIDED_PROPERTY(name, kind)       \
  SIDED_PROPERTY(name, , name, kind)

/* See StSpecialProperty for the required order */
static const StPropertyClass properties[] = {
  { NULL, st_parse_unknown_property, NULL, 0 },
  { "color", st_parse_color_property, st_assign_color_property, offsetof(StThemeNode, foreground_color), -1 },
  { "font-size", st_parse_font_size, st_assign_font_size, 0, -1 },

  SIMPLE_SIDED_PROPERTY(padding, length),

  { "background-color", st_parse_color_property, st_assign_color_property, offsetof(StThemeNode, background_color) },
  { "background-image", st_parse_image_property, st_assign_image_property, offsetof(StThemeNode, background_image) },
  { "background-position", st_parse_background_position, st_assign_background_position, 0, -1 },
  { "background-size", st_parse_background_size, st_assign_background_size, 0, -1 },
  { "background-repeat", st_parse_background_repeat, st_assign_background_repeat, 0, -1 },
  { "-st-background-gradient-direction", st_parse_background_gradient_direction, st_assign_enum_property, offsetof(StThemeNode, background_gradient_type), -1 },
  { "-st-background-gradient-end", st_parse_color_property, st_assign_color_property, offsetof(StThemeNode, background_gradient_end), -1 },
  { "background", st_parse_background, NULL, 0, -1 },

  /* FIXME: these should be shorthands */
  { "outline-width", st_parse_length_property, st_assign_length_property, offsetof(StThemeNode, outline_width), -1 },
  { "outline-color", st_parse_color_property, st_assign_color_property, offsetof(StThemeNode, outline_color), -1 },
  { "outline", st_parse_outline, NULL, 0, -1 },

  /* FIXME: border-style ? */
  SIDED_PROPERTY(border, -width, border_width, length),
  SIDED_PROPERTY(border, -color, border_color, color),

  /* FIXME: we should support horizontal/vertical radius (and maybe percentages too) */
  /* It is very nice of the CSS authors that this works like a normal sided property */
  { "border-top-left-radius", st_parse_length_property, st_assign_length_property, offsetof(StThemeNode, border_radius), ST_CORNER_TOPLEFT },
  { "border-top-right-radius", st_parse_length_property, st_assign_length_property, offsetof(StThemeNode, border_radius), ST_CORNER_TOPRIGHT },
  { "border-bottom-right-radius", st_parse_length_property, st_assign_length_property, offsetof(StThemeNode, border_radius), ST_CORNER_BOTTOMRIGHT },
  { "border-bottom-left-radius", st_parse_length_property, st_assign_length_property, offsetof(StThemeNode, border_radius), ST_CORNER_BOTTOMLEFT },
  { "border-radius", st_parse_sided_property, 0, -1 },

  { "border", st_parse_border, NULL, 0, -1 },
  /* FIXME: border-image-width, border-image-outset, border-image-repeat missing */
  { "border-image-source", st_parse_image_property, st_assign_image_property, offsetof(StThemeNode, border_image_source), 0 },
  { "border-image-slice", st_parse_border_image_slice, st_assign_border_image_slice, offsetof(StThemeNode, border_image_slice), 0 },
  { "border-image", st_parse_border_image, NULL, 0, -1 },

  { "width", st_parse_length_property, st_assign_length_property, offsetof(StThemeNode, width), -1 },
  { "min-width", st_parse_length_property, st_assign_length_property, offsetof(StThemeNode, min_width), -1 },
  { "max-width", st_parse_length_property, st_assign_length_property, offsetof(StThemeNode, max_width), -1 },
  { "height", st_parse_length_property, st_assign_length_property, offsetof(StThemeNode, height), -1 },
  { "min-height", st_parse_length_property, st_assign_length_property, offsetof(StThemeNode, min_height), -1 },
  { "max-height", st_parse_length_property, st_assign_length_property, offsetof(StThemeNode, max_height), -1 },

  /* FIXME: what about the other transition-* properties ? */
  { "transition-duration", st_parse_time_property, st_assign_time_property, offsetof(StThemeNode, transition_duration), -1 },

  { "box-shadow", st_parse_shadow_property, st_assign_shadow_property, offsetof(StThemeNode, box_shadow), -1 },
  { "-st-background-image-shadow", st_parse_shadow_property, st_assign_shadow_property, offsetof(StThemeNode, background_image_shadow), -1 },
  { "text-shadow", st_parse_shadow_property, st_assign_shadow_property, offsetof(StThemeNode, text_shadow) },

  { "font-variant", st_parse_font_variant, st_assign_font_variant, 0, -1 },
  { "font-style", st_parse_font_style, st_assign_font_style, 0, -1 },
  { "font-weight", st_parse_font_weight, st_assign_font_weight, 0, -1 },
  { "font-family", st_parse_font_family, st_assign_font_family, 0, -1 },
  { "font", st_parse_font, NULL, 0 },

  { "text-decoration", st_parse_text_decoration, st_assign_enum_property, 0, -1 },
  { "text-align", st_parse_text_align, st_assign_enum_property, 0, -1 },

  { "-st-warning-color", st_parse_color_property, st_assign_color_property, offsetof(StThemeNode, warning_color), -1 },
  { "-st-error-color", st_parse_color_property, st_assign_color_property, offsetof(StThemeNode, warning_color), -1 },
  { "-st-success-color", st_parse_color_property, st_assign_color_property, offsetof(StThemeNode, warning_color), -1 },

};

static GHashTable *
st_ensure_property_table (void)
{
  static GHashTable *table;
  int i;

  if (G_LIKELY (table != NULL))
    return table;

  table = g_hash_table_new (g_str_hash, g_str_equal);

  /* ID = 0 is reserved for the unknown property class */
  for (i = 1; i < G_N_ELEMENTS (properties); i++)
    g_hash_table_insert (table, (gpointer) properties[i].name, (gpointer) &properties[i]);

  return table;
}

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

static inline int
cr_declaration_compute_sort_key (CRDeclaration *decl)
{
  CRStatement *statement;
  StStylesheet * stylesheet;
  int origin;
  int specificity;

  statement = decl->parent_statement;
  stylesheet = statement->parent_sheet->app_data;
  origin = stylesheet->origin;
  specificity = statement->specificity;

  if (decl->important)
    origin += ST_ORIGIN_OFFSET_IMPORTANT;

  return (origin + (specificity << ST_ORIGIN_SHIFT));
}

static inline StProperty *
st_property_new (const StPropertyClass *klass,
                 CRDeclaration         *decl)
{
  StProperty *self;

  self = g_slice_new (StProperty);
  self->ref_count = 1;
  self->sort_key = cr_declaration_compute_sort_key (decl);
  self->property_id = klass - properties;

  if (self->property_id == 0)
    self->property_name = g_quark_from_string (cr_to_string (decl->property));
  else
    self->property_name = 0;

  return self;
}

StProperty *
st_property_copy (StProperty *other)
{
  StProperty *self;

  self = g_slice_new (StProperty);
  self->ref_count = 1;
  self->sort_key = other->sort_key;
  self->property_id = other->property_id;
  self->property_name = other->property_name;
  self->value = st_css_value_ref (other->value);

  return self;
}

StProperty *
st_property_ref (StProperty *self)
{
  self->ref_count++;
  return self;
}

void
st_property_unref (StProperty *self)
{
  if (--self->ref_count == 0)
    {
      st_css_value_unref (self->value);
      g_slice_free (StProperty, self);
    }
}

static gboolean
st_parse_unknown_property (const StPropertyClass  *klass,
                           CRDeclaration          *decl,
                           CRTerm                **term,
                           StParserFlags           flags,
                           GPtrArray              *out)
{
  StProperty *prop;
  CRTerm *item;

  item = *term;
  prop = st_property_new (klass, decl);
  prop->value = st_css_value_new (ST_VALUE_COMPLEX);
  cr_term_ref (item);
  prop->value->value.cr_term = item;
  g_ptr_array_add (out, prop);

  *term = item->next;

  return TRUE;
}

static gboolean
st_parse_common (const StPropertyClass *klass,
                 CRDeclaration         *decl,
                 GPtrArray             *out)
{
  StCssValue *v;
  CRTerm *term;
  StProperty *prop;

  term = decl->value;
  v = st_css_value_parse_common (&term);
  if (v == NULL)
    return FALSE;

  prop = st_property_new (klass, decl);
  prop->value = v;
  g_ptr_array_add (out, prop);
  return TRUE;
}

static const StPropertyClass *
st_property_find_class (const char *name)
{
  GHashTable *property_table;
  const StPropertyClass *klass;

  property_table = st_ensure_property_table ();
  klass = g_hash_table_lookup (property_table, name);

  if (klass == NULL)
    return &properties[0];
  else
    return klass;
}

static inline gboolean
st_property_parse_internal (const StPropertyClass  *klass,
                            CRDeclaration          *declaration,
                            CRTerm                **term,
                            StParserFlags           flags,
                            GPtrArray              *out)
{
  return klass->parse (klass, declaration, term, flags, out);
}

gboolean
st_property_parse (CRDeclaration   *declaration,
                   GPtrArray       *out)
{
  const StPropertyClass *klass;
  CRTerm *value;
  gboolean ok;

  if (declaration->value == NULL)
    return FALSE;

  klass = st_property_find_class (cr_to_string (declaration->property));

  if (st_parse_common (klass, declaration, out))
      return TRUE;

  value = declaration->value;
  ok = st_property_parse_internal (klass, declaration, &value, 0, out);

  return ok && value == NULL;
}

void
st_property_assign (StProperty  *property,
                    StThemeNode *node)
{
  const StPropertyClass *klass;

  klass = properties + property->property_id;
  klass->assign (klass, property, node);
}

static gboolean
st_parse_simple_property (const StPropertyClass  *klass,
                          CRDeclaration          *decl,
                          CRTerm                **term,
                          StCssValue *          (*parser) (CRTerm **),
                          GPtrArray              *out)
{
  StCssValue *v;
  StProperty *prop;

  v = parser (term);
  if (v == NULL)
    return FALSE;

  prop = st_property_new (klass, decl);
  prop->value = v;
  g_ptr_array_add (out, prop);
  return TRUE;
}

static gboolean
st_parse_color_property (const StPropertyClass  *klass,
                         CRDeclaration          *decl,
                         CRTerm                **term,
                         StParserFlags           flags,
                         GPtrArray              *out)
{
  return st_parse_simple_property (klass, decl, term,
                                   st_css_value_parse_color, out);
}

static gboolean
st_parse_length_property (const StPropertyClass  *klass,
                          CRDeclaration          *decl,
                          CRTerm                **term,
                          StParserFlags           flags,
                          GPtrArray              *out)
{
  return st_parse_simple_property (klass, decl, term,
                                   st_css_value_parse_length, out);
}

static gboolean
st_parse_time_property (const StPropertyClass  *klass,
                        CRDeclaration          *decl,
                        CRTerm                **term,
                        StParserFlags           flags,
                        GPtrArray              *out)
{
  return st_parse_simple_property (klass, decl, term,
                                   st_css_value_parse_time, out);
}

static gboolean
st_parse_image_property (const StPropertyClass  *klass,
                         CRDeclaration          *decl,
                         CRTerm                **term,
                         StParserFlags           flags,
                         GPtrArray              *out)
{
  StCssValue *v;
  StProperty *prop;
  CRStatement *statement;
  StStylesheet *stylesheet;

  GFile *parent_file;

  statement = decl->parent_statement;
  stylesheet = statement->parent_sheet->app_data;
  parent_file = stylesheet->file;

  v = st_css_value_parse_image (term, parent_file);
  if (v == NULL)
    return FALSE;

  prop = st_property_new (klass, decl);
  prop->value = v;
  g_ptr_array_add (out, prop);
  return TRUE;
}

static gboolean
st_parse_shadow_property (const StPropertyClass  *klass,
                          CRDeclaration          *decl,
                          CRTerm                **term,
                          StParserFlags           flags,
                          GPtrArray              *out)
{
  return st_parse_simple_property (klass, decl, term,
                                   st_css_value_parse_shadow, out);
}

static gboolean
st_parse_compound_property (CRDeclaration       *decl,
                            CRTerm             **term,
                            const char * const  *properties,
                            StParserFlags        flags,
                            GPtrArray              *out)
{
  /* Attempt parsing each of properties, starting at term */
  const StPropertyClass **prop_classes;
  int i, n_props;
  int mask, full_mask;
  CRTerm *item;

  n_props = g_strv_length ((char**) properties);
  g_assert (n_props < 8 * sizeof(int));

  mask = 0;
  full_mask = (1 << n_props) - 1;

  prop_classes = g_alloca (sizeof (const StPropertyClass) * n_props);
  for (i = 0; i < n_props; i++)
    prop_classes[i] = st_property_find_class (properties[i]);

  item = *term;
  while (item && mask < full_mask)
    {
      gboolean ok = FALSE;

      for (i = 0; i < n_props; i++)
        {
          if (mask & (1 << i))
            continue;

          if (st_property_parse_internal (prop_classes[i],
                                          decl, &item,
                                          flags, out))
            {
              mask |= 1 << i;
              ok = TRUE;
              break;
            }
        }

      if (!ok)
        return FALSE;
    }

  /* Fill missing properties with initial values */
  for (i = 0; i < n_props; i++)
    {
      StProperty *prop;

      if (mask & (1 << i))
        continue;

      prop = st_property_new (prop_classes[i], decl);
      prop->value = (StCssValue*) &st_css_value_initial;
      g_ptr_array_add (out, prop);
    }

  *term = item;
  return TRUE;
}

static gboolean
st_parse_sided_property (const StPropertyClass  *klass,
                         CRDeclaration          *decl,
                         CRTerm                **term,
                         StParserFlags           flags,
                         GPtrArray              *out)
{
  CRTerm *item;
  int property_id;
  int i, n;
  StProperty *top, *right, *bottom, *left;

  property_id = klass - properties;

  g_assert (property_id >= 4);

  if ((flags & ST_PARSER_NOT_SIDED) != 0)
    n = 1;
  else
    n = 4;

  item = *term; i = 0;
  while (item && i < n) {
    if (!st_property_parse_internal (&properties[property_id - 4 + i],
                                     decl, &item,
                                     flags & ~(ST_PARSER_NOT_SIDED),
                                     out))
      return FALSE;

    i++;
  }

  switch (i) {
  case 1:
    {
      /* Duplicate for the other three sides */
      top = g_ptr_array_index (out, out->len - 1);

      right = st_property_copy (top);
      right->property_id = property_id - 4 + ST_SIDE_RIGHT;
      g_ptr_array_add (out, right);

      bottom = st_property_copy (top);
      bottom->property_id = property_id - 4 + ST_SIDE_BOTTOM;
      g_ptr_array_add (out, bottom);

      left = st_property_copy (top);
      left->property_id = property_id - 4 + ST_SIDE_LEFT;
      g_ptr_array_add (out, left);

      break;
    }

  case 2:
    {
      /* Duplicate for matching sides */
      top = g_ptr_array_index (out, out->len - 2);
      right = g_ptr_array_index (out, out->len - 1);

      bottom = st_property_copy (top);
      bottom->property_id = property_id - 4 + ST_SIDE_BOTTOM;
      g_ptr_array_add (out, bottom);

      left = st_property_copy (right);
      left->property_id = property_id - 4 + ST_SIDE_LEFT;
      g_ptr_array_add (out, left);

      break;
    }

  case 3:
    {
      /* Duplicate just left */
      right = g_ptr_array_index (out, out->len - 2);

      left = st_property_copy (right);
      left->property_id = property_id - 4 + ST_SIDE_LEFT;
      g_ptr_array_add (out, left);

      break;
    }

  case 4:
    /* No duplication */
    break;

  default:
    g_assert_not_reached ();
  }

  *term = item;
  return TRUE;
}

static gboolean
st_parse_border (const StPropertyClass  *klass,
                 CRDeclaration          *decl,
                 CRTerm                **term,
                 StParserFlags           flags,
                 GPtrArray                 *out)
{
  const StPropertyClass *border_image_klass;
  StProperty *border_image;

  static const char * subproperties[] = {
    "border-width", "border-color", NULL
  };

  if (!st_parse_compound_property (decl, term, subproperties,
                                   flags | ST_PARSER_NOT_SIDED, out))
    return FALSE;

  /* We're required by CSS to reset border-image when
     setting border. */
  border_image_klass = st_property_find_class ("border-image");
  border_image = st_property_new (border_image_klass, decl);
  border_image->value = (StCssValue*) &st_css_value_initial;
  g_ptr_array_add (out, border_image);

  return TRUE;
}

static gboolean
st_parse_outline (const StPropertyClass  *klass,
                  CRDeclaration          *decl,
                  CRTerm                **term,
                  StParserFlags           flags,
                  GPtrArray                 *out)
{
  static const char * subproperties[] = {
    "outline-width", "outline-color", NULL
  };

  return st_parse_compound_property (decl, term, subproperties,
                                     flags, out);
}

static gboolean
st_parse_background (const StPropertyClass  *klass,
                     CRDeclaration          *decl,
                     CRTerm                **term,
                     StParserFlags           flags,
                     GPtrArray                 *out)
{
  const StPropertyClass *sub_klass;
  StProperty *prop;

  static const char * subproperties[] = {
    "background-color", "background-image", "background-size",
    "background-repeat", NULL,
  };

  if (!st_parse_compound_property (decl, term, subproperties,
                                   flags, out))
    return FALSE;

  /* We also reset our custom gradient properties when
     setting background, but we don't allow setting them
     using the shorthand (the syntax would be ambiguous) */
  sub_klass = st_property_find_class ("-st-background-gradient-direction");
  prop = st_property_new (sub_klass, decl);
  prop->value = (StCssValue*) &st_css_value_initial;
  g_ptr_array_add (out, prop);
  sub_klass = st_property_find_class ("-st-background-gradient-end");
  prop = st_property_new (sub_klass, decl);
  prop->value = (StCssValue*) &st_css_value_initial;
  g_ptr_array_add (out, prop);

  return st_parse_compound_property (decl, term, subproperties,
                                     flags, out);
}

static gboolean
st_parse_border_image (const StPropertyClass  *klass,
                       CRDeclaration          *decl,
                       CRTerm                **term,
                       StParserFlags           flags,
                       GPtrArray                 *out)
{
  static const char * subproperties[] = {
    "border-image-slice", "border-image-source", NULL
  };

  return st_parse_compound_property (decl, term, subproperties,
                                     flags, out);
}

static gboolean
st_parse_font (const StPropertyClass  *klass,
               CRDeclaration          *decl,
               CRTerm                **term,
               StParserFlags           flags,
               GPtrArray                 *out)
{
  const StPropertyClass *font_size, *font_family;
  static const char *subproperties[] = {
    "font-style", "font-variant", "font-weight", NULL
  };

  if (!st_parse_compound_property (decl, term, subproperties,
                                   flags, out))
    return FALSE;

  font_size = st_property_find_class ("font-size");
  if (!st_property_parse_internal (font_size, decl, term,
                                   flags, out))
    return FALSE;

  /* Note: no line-height here */

  font_family = st_property_find_class ("font-family");
  if (!st_property_parse_internal (font_family, decl, term,
                                   flags, out))
    return FALSE;

  return TRUE;
}

static gboolean
st_parse_enum_property (const StPropertyClass  *klass,
                        CRDeclaration          *decl,
                        CRTerm                **term,
                        const char * const     *nicks,
                        GPtrArray              *out)
{
  StCssValue *v;
  StProperty *prop;

  v = st_css_value_parse_enum (term, nicks);
  if (v == NULL)
    return FALSE;

  prop = st_property_new (klass, decl);
  prop->value = v;
  g_ptr_array_add (out, prop);
  return TRUE;
}

static gboolean
st_parse_font_variant (const StPropertyClass  *klass,
                       CRDeclaration          *decl,
                       CRTerm                **term,
                       StParserFlags           flags,
                       GPtrArray              *out)
{
  /* Keep this in the same order as PangoVariant */
  static const char * values[] = {
    "normal", "small-caps", NULL
  };

  return st_parse_enum_property (klass, decl, term, values, out);
}

static gboolean
st_parse_font_style (const StPropertyClass  *klass,
                     CRDeclaration          *decl,
                     CRTerm                **term,
                     StParserFlags           flags,
                     GPtrArray              *out)
{
  /* Keep this in the same order as PangoStyle */
  static const char * values[] = {
    "normal", "oblique", "italic", NULL
  };

  return st_parse_enum_property (klass, decl, term, values, out);
}

static gboolean
st_parse_font_size (const StPropertyClass  *klass,
                    CRDeclaration          *decl,
                    CRTerm                **term,
                    StParserFlags           flags,
                    GPtrArray              *out)
{
  /* Keep this in the same order as StFontScale */
  static const char * values[] = {
    "xx-small", "x-small", "small", "medium", "large", "x-large",
    "xx-large", "larger", "smaller", NULL
  };
  CRTerm *item;

  if (st_parse_enum_property (klass, decl, term, values, out))
    return TRUE;

  item = *term;
  if (item->type == TERM_NUMBER &&
      item->content.num->type == NUM_PERCENTAGE)
    {
      StProperty *prop;

      prop = st_property_new (klass, decl);
      prop->value = st_css_value_new (ST_VALUE_FONT_RELATIVE);
      prop->value->value.factor = item->content.num->val / 100.;
      g_ptr_array_add (out, prop);

      *term = item->next;
      return TRUE;
    }

  return st_parse_length_property (klass, decl, term, flags, out);
}

static gboolean
st_parse_font_weight (const StPropertyClass  *klass,
                      CRDeclaration          *decl,
                      CRTerm                **term,
                      StParserFlags           flags,
                      GPtrArray              *out)
{
  /* Keep this in the same order as StFontWeight */
  static const char * values[] = {
    "normal", "bold", "bolder", "lighter", NULL
  };
  StProperty *prop;
  CRTerm *item;
  guint v;

  if (st_parse_enum_property (klass, decl, term, values,
                              out))
    return TRUE;

  item = *term;
  if (item->type != TERM_NUMBER ||
      item->content.num->type != NUM_GENERIC)
    return FALSE;

  v = item->content.num->val;

  if ((v % 100) != 0)
    return FALSE;

  prop = st_property_new (klass, decl);
  prop->value = st_css_value_new (ST_VALUE_INT);
  prop->value->value.integer = v;
  g_ptr_array_add (out, prop);

  *term = item->next;
  return TRUE;
}

static gboolean
st_parse_font_family (const StPropertyClass  *klass,
                      CRDeclaration          *decl,
                      CRTerm                **term,
                      StParserFlags           flags,
                      GPtrArray              *out)
{
  GString *family_string;
  gboolean last_was_quoted = FALSE;
  CRTerm *item;
  StProperty *prop;

  family_string = g_string_new (NULL);

  item = *term;
  while (item)
    {
      if (item->type != TERM_STRING && item->type != TERM_IDENT)
        goto error;

      if (family_string->len > 0)
        {
          if (item->the_operator != COMMA && item->the_operator != NO_OP)
            goto error;
          /* Can concatenate two bare words, but not two quoted strings */
          if ((item->the_operator == NO_OP && last_was_quoted) || item->type == TERM_STRING)
            goto error;

          if (item->the_operator == NO_OP)
            g_string_append (family_string, " ");
          else
            g_string_append (family_string, ", ");
        }
      else
        {
          if (item->the_operator != NO_OP)
            goto error;
        }

      g_string_append (family_string, cr_to_string (item->content.str));

      item = item->next;
    }

  prop = st_property_new (klass, decl);
  prop->value = st_css_value_new (ST_VALUE_STRING);
  prop->value->value.string = g_string_free (family_string, FALSE);

  *term = item;
  g_ptr_array_add (out, prop);
  return TRUE;

 error:

  g_string_free (family_string, TRUE);
  return FALSE;
}

static gboolean
st_parse_text_decoration (const StPropertyClass  *klass,
                          CRDeclaration          *decl,
                          CRTerm                **term,
                          StParserFlags           flags,
                          GPtrArray              *out)
{
  StProperty *prop;
  StCssValue *v;
  int mask, full_mask;
  CRTerm *item;

  v = st_css_value_parse_keyword (term, "none", ST_TEXT_DECORATION_NONE);
  if (v != NULL)
    {
      prop = st_property_new (klass, decl);
      prop->value = v;

      g_ptr_array_add (out, prop);
      return TRUE;
    }

  mask = 0;
  full_mask = ST_TEXT_DECORATION_MASK;

  for (item = *term; item && mask < full_mask; item = item->next)
    {
      if (item->type != TERM_IDENT)
        return FALSE;

      if ((mask & ST_TEXT_DECORATION_UNDERLINE) == 0 &&
          cr_string_is (item->content.str, "underline"))
        mask |= ST_TEXT_DECORATION_UNDERLINE;

      else if ((mask & ST_TEXT_DECORATION_OVERLINE) == 0 &&
          cr_string_is (item->content.str, "overline"))
        mask |= ST_TEXT_DECORATION_OVERLINE;

      else if ((mask & ST_TEXT_DECORATION_LINE_THROUGH) == 0 &&
          cr_string_is (item->content.str, "line-through"))
        mask |= ST_TEXT_DECORATION_LINE_THROUGH;

      else
        return FALSE;
    }

  prop = st_property_new (klass, decl);
  prop->value = st_css_value_new (ST_VALUE_ENUM);
  prop->value->value.v_enum = mask;

  *term = item;
  g_ptr_array_add (out, prop);
  return TRUE;
}

static gboolean
st_parse_text_align (const StPropertyClass  *klass,
                     CRDeclaration          *decl,
                     CRTerm                **term,
                     StParserFlags           flags,
                     GPtrArray              *out)
{
  /* Keep this in the same order as StTextAlign and PangoAlignment */
  static const char * values[] = {
    "left", "center", "right", "justify", NULL
  };

  return st_parse_enum_property (klass, decl, term, values, out);
}

static gboolean
st_parse_background_gradient_direction (const StPropertyClass  *klass,
                                        CRDeclaration          *decl,
                                        CRTerm                **term,
                                        StParserFlags           flags,
                                        GPtrArray              *out)
{
  /* Keep this in the same order as StGradientType */
  static const char * values[] = {
    "none", "vertical", "horizontal", "radial", NULL
  };

  return st_parse_enum_property (klass, decl, term, values, out);
}

static gboolean
st_parse_background_position (const StPropertyClass *klass,
                              CRDeclaration          *decl,
                              CRTerm                **term,
                              StParserFlags           flags,
                              GPtrArray              *out)
{
  StProperty *prop;
  StCssValue *v1, *v2;

  v1 = st_css_value_parse_length (term);
  if (v1 == NULL)
    return FALSE;

  v2 = st_css_value_parse_length (term);
  if (v2 == NULL)
    {
      st_css_value_unref (v1);
      return FALSE;
    }

  prop = st_property_new (klass, decl);
  prop->value = st_css_value_new (ST_VALUE_ARRAY);
  g_ptr_array_add (prop->value->value.array, v1);
  g_ptr_array_add (prop->value->value.array, v2);

  g_ptr_array_add (out, prop);
  return TRUE;
}

static gboolean
st_parse_background_size (const StPropertyClass  *klass,
                          CRDeclaration          *decl,
                          CRTerm                **term,
                          StParserFlags           flags,
                          GPtrArray              *out)
{
  StProperty *prop;
  StCssValue *v, *v1, *v2;
  static const char * values[] = {
    "contain", "cover", NULL
  };

  v = st_css_value_parse_enum (term, values);
  if (v != NULL)
    {
      prop = st_property_new (klass, decl);
      prop->value = v;

      g_ptr_array_add (out, prop);
      return TRUE;
    }

  v1 = st_css_value_parse_keyword (term, "auto", ST_BACKGROUND_SIZE_AUTO);
  if (v1 == NULL)
    v1 = st_css_value_parse_length (term);
  if (v1 == NULL)
    return FALSE;

  if (*term != NULL)
    {
      v2 = st_css_value_parse_keyword (term, "auto", ST_BACKGROUND_SIZE_AUTO);
      if (v2 == NULL)
        v2 = st_css_value_parse_length (term);
      if (v2 == NULL)
        {
          st_css_value_unref (v1);
          return FALSE;
        }
    }
  else
    {
      v2 = st_css_value_new (ST_VALUE_ENUM);
      v2->value.v_enum = ST_BACKGROUND_SIZE_AUTO;
    }

  prop = st_property_new (klass, decl);
  prop->value = st_css_value_new (ST_VALUE_ARRAY);
  g_ptr_array_add (prop->value->value.array, v1);
  g_ptr_array_add (prop->value->value.array, v2);

  g_ptr_array_add (out, prop);
  return TRUE;
}

static gboolean
st_parse_background_repeat (const StPropertyClass  *klass,
                            CRDeclaration          *decl,
                            CRTerm                **term,
                            StParserFlags           flags,
                            GPtrArray              *out)
{
  StCssValue *v;
  StProperty *prop;
  static const char * values[] = {
    "no-repeat", "repeat", NULL
  };

  v = st_css_value_parse_enum (term, values);
  if (v == NULL)
    return FALSE;

  prop = st_property_new (klass, decl);
  prop->value = v;

  g_ptr_array_add (out, prop);
  return TRUE;
}

static gboolean
st_parse_border_image_slice (const StPropertyClass  *klass,
                             CRDeclaration          *decl,
                             CRTerm                **term,
                             StParserFlags           flags,
                             GPtrArray              *out)
{
  StProperty *prop;
  StCssValue *slices[4];
  int n_slices;
  CRTerm *item;
  int i;

  n_slices = 0;
  item = *term;

  while (item && n_slices < 4)
    {
      slices[n_slices] = st_css_value_parse_float (&item);

      if (slices[n_slices] == NULL)
        goto error;

      n_slices ++;
    }

  switch (n_slices)
    {
    case 1:
      /* Duplicate for the other three sides */
      slices[ST_SIDE_RIGHT] = st_css_value_ref (slices[ST_SIDE_TOP]);
      slices[ST_SIDE_BOTTOM] = st_css_value_ref (slices[ST_SIDE_TOP]);
      slices[ST_SIDE_LEFT] = st_css_value_ref (slices[ST_SIDE_TOP]);
      break;

    case 2:
      /* Duplicate for matching sides */
      slices[ST_SIDE_BOTTOM] = st_css_value_ref (slices[ST_SIDE_TOP]);
      slices[ST_SIDE_LEFT] = st_css_value_ref (slices[ST_SIDE_RIGHT]);
      break;

    case 3:
      /* Duplicate just left */
      slices[ST_SIDE_LEFT] = st_css_value_ref (slices[ST_SIDE_RIGHT]);
      break;

    case 4:
      /* No duplication */
      break;

    default:
      g_assert_not_reached ();
    }

  prop = st_property_new (klass, decl);
  prop->value = st_css_value_new (ST_VALUE_ARRAY);
  for (i = 0; i < 4; i++)
    g_ptr_array_add (prop->value->value.array, slices[i]);

  *term = item->next;
  g_ptr_array_add (out, prop);
  return TRUE;

 error:
  for (i = 0; i < n_slices; i++)
    st_css_value_unref (slices[i]);

  return FALSE;
}

static inline StKeyword
st_get_actual_keyword (StThemeNode *node,
                       StCssValue  *value)
{
  if (st_css_value_is_inherit (value))
    {
      if (node->parent_node)
        {
          _st_theme_node_ensure_computed (node);
          return ST_KEYWORD_INHERIT;
        }
      else
        return ST_KEYWORD_INITIAL;
    }
  else if (st_css_value_is_initial (value))
    return ST_KEYWORD_INITIAL;
  else
    return ST_KEYWORD_INVALID;
}

static void
st_assign_length_property (const StPropertyClass *klass,
                           StProperty            *prop,
                           StThemeNode           *node)
{
  float *dst, *src;
  int offset;
  StKeyword kw;

  offset = klass->field;
  if (klass->side >= 0)
    offset += sizeof(float) * klass->side;

  dst = G_STRUCT_MEMBER_P (node, offset);
  kw = st_get_actual_keyword (node, prop->value);

  switch (kw)
    {
    case ST_KEYWORD_INHERIT:
      src = G_STRUCT_MEMBER_P (node->parent_node, offset);
      *dst = *src;
      break;

    case ST_KEYWORD_INITIAL:
      *dst = 0;
      break;

    default:
      st_css_value_compute_length (node, prop->property_id, prop->value, dst);
    }
}

static void
st_assign_color_property (const StPropertyClass *klass,
                          StProperty            *prop,
                          StThemeNode           *node)
{
  ClutterColor *dst, *src;
  int offset;
  StKeyword kw;

  offset = klass->field;
  if (klass->side >= 0)
    offset += sizeof(int) * klass->side;

  dst = G_STRUCT_MEMBER_P (node, offset);
  kw = st_get_actual_keyword (node, prop->value);

  switch (kw)
    {
    case ST_KEYWORD_INHERIT:
      src = G_STRUCT_MEMBER_P (node->parent_node, offset);
      memcpy (dst, src, sizeof (ClutterColor));
      break;

    case ST_KEYWORD_INITIAL:
      /* Slight deviation from css3 specification: the initial
         value of all color properties is transparent, except for
         color itself, which is black
      */
      if (prop->property_id == ST_PROPERTY_COLOR)
        memcpy (dst, clutter_color_get_static (CLUTTER_COLOR_BLACK),
                sizeof (ClutterColor));
      else
        memcpy (dst, clutter_color_get_static (CLUTTER_COLOR_TRANSPARENT),
                sizeof (ClutterColor));
      break;

    default:
      st_css_value_compute_color (node, prop->property_id,
                                  prop->value, dst);
    }
}

static void
st_assign_time_property (const StPropertyClass *klass,
                         StProperty            *prop,
                         StThemeNode           *node)
{
  int *dst, *src;
  int offset;
  StKeyword kw;

  offset = klass->field;
  if (klass->side >= 0)
    offset += sizeof(int) * klass->side;

  dst = G_STRUCT_MEMBER_P (node, offset);
  kw = st_get_actual_keyword (node, prop->value);

  switch (kw)
    {
    case ST_KEYWORD_INHERIT:
      src = G_STRUCT_MEMBER_P (node->parent_node, offset);
      *dst = *src;
      break;

    case ST_KEYWORD_INITIAL:
      *dst = 0;
      break;

    default:
      st_css_value_compute_time (node, prop->property_id, prop->value, dst);
    }
}

static void
st_assign_enum_property (const StPropertyClass *klass,
                         StProperty            *prop,
                         StThemeNode           *node)
{
  int *dst, *src;
  int offset;
  StKeyword kw;

  offset = klass->field;
  if (klass->side >= 0)
    offset += sizeof(int) * klass->side;

  dst = G_STRUCT_MEMBER_P (node, offset);
  kw = st_get_actual_keyword (node, prop->value);

  switch (kw)
    {
    case ST_KEYWORD_INHERIT:
      src = G_STRUCT_MEMBER_P (node->parent_node, offset);
      *dst = *src;
      break;

    case ST_KEYWORD_INITIAL:
      *dst = 0;
      break;

    default:
      g_assert (prop->value->value_kind == ST_VALUE_ENUM);
      *dst = prop->value->value.v_enum;
    }
}


static void
st_assign_shadow_property (const StPropertyClass *klass,
                           StProperty            *prop,
                           StThemeNode           *node)
{
  StShadow **dst, **src;
  int offset;
  StKeyword kw;

  offset = klass->field;
  if (klass->side >= 0)
    offset += sizeof(StShadow*) * klass->side;

  dst = G_STRUCT_MEMBER_P (node, offset);
  kw = st_get_actual_keyword (node, prop->value);

  switch (kw)
    {
    case ST_KEYWORD_INHERIT:
      src = G_STRUCT_MEMBER_P (node->parent_node, offset);
      *dst = st_shadow_ref (*src);
      break;

    case ST_KEYWORD_INITIAL:
      *dst = NULL;
      break;

    default:
      *dst = g_slice_new (StShadow);
      st_css_value_compute_shadow (node, prop->property_id, prop->value, *dst);
    }
}

static void
st_assign_image_property (const StPropertyClass *klass,
                          StProperty            *prop,
                          StThemeNode           *node)
{
  GFile **dst, **src;
  int offset;
  StKeyword kw;

  offset = klass->field;
  if (klass->side >= 0)
    offset += sizeof(GFile*) * klass->side;

  dst = G_STRUCT_MEMBER_P (node, offset);
  kw = st_get_actual_keyword (node, prop->value);

  switch (kw)
    {
    case ST_KEYWORD_INHERIT:
      src = G_STRUCT_MEMBER_P (node->parent_node, offset);
      if (*src)
        *dst = g_object_ref (*src);
      else
        *dst = NULL;
      break;

    case ST_KEYWORD_INITIAL:
      *dst = NULL;
      break;

    default:
      st_css_value_compute_image (node, prop->property_id, prop->value, dst);
    }
}

static void
st_assign_background_size (const StPropertyClass *klass,
                           StProperty            *prop,
                           StThemeNode           *node)
{
  StCssValue *v1, *v2;
  StKeyword kw;

  kw = st_get_actual_keyword (node, prop->value);

  switch (kw)
    {
    case ST_KEYWORD_INHERIT:
      node->background_size = node->parent_node->background_size;
      node->background_size_w = node->parent_node->background_size_w;
      node->background_size_h = node->parent_node->background_size_h;
      break;

    case ST_KEYWORD_INITIAL:
      node->background_size = ST_BACKGROUND_SIZE_AUTO;
      node->background_size_w = -1;
      node->background_size_h = -1;
      break;

    default:
      if (prop->value->value_kind == ST_VALUE_ENUM)
        {
          node->background_size = prop->value->value.v_enum;
          node->background_size_w = -1;
          node->background_size_h = -1;
        }
      else
        {
          g_assert (prop->value->value_kind == ST_VALUE_ARRAY);
          g_assert (prop->value->value.array->len == 2);
          v1 = g_ptr_array_index (prop->value->value.array, 0);
          v2 = g_ptr_array_index (prop->value->value.array, 1);

          if (v1->value_kind != ST_VALUE_ENUM ||
              v2->value_kind != ST_VALUE_ENUM)
            node->background_size = ST_BACKGROUND_SIZE_FIXED;
          else
            node->background_size = ST_BACKGROUND_SIZE_AUTO;

          if (v1->value_kind != ST_VALUE_ENUM)
            st_css_value_compute_length (node, prop->property_id,
                                         v1, &node->background_size_w);
          else
            node->background_size_w = -1;

          if (v1->value_kind != ST_VALUE_ENUM)
            st_css_value_compute_length (node, prop->property_id,
                                         v1, &node->background_size_w);
          else
            node->background_size_h = -1;
        }
    }
}

static void
st_assign_background_position (const StPropertyClass *klass,
                               StProperty            *prop,
                               StThemeNode           *node)
{
  StCssValue *v1, *v2;
  StKeyword kw;

  kw = st_get_actual_keyword (node, prop->value);

  switch (kw)
    {
    case ST_KEYWORD_INHERIT:
      node->background_position_x = node->parent_node->background_position_x;
      node->background_position_y = node->parent_node->background_position_y;
      node->background_position_set = node->parent_node->background_position_set;
      break;

    case ST_KEYWORD_INITIAL:
      node->background_position_x = 0;
      node->background_position_y = 0;
      node->background_position_set = 0;
      break;

    default:
      g_assert (prop->value->value_kind == ST_VALUE_ARRAY);
      g_assert (prop->value->value.array->len == 2);
      v1 = g_ptr_array_index (prop->value->value.array, 0);
      v2 = g_ptr_array_index (prop->value->value.array, 1);

      st_css_value_compute_length (node, prop->property_id,
                                   v1, &node->background_position_x);
      st_css_value_compute_length (node, prop->property_id,
                                   v2, &node->background_position_y);
      node->background_position_set = 1;
    }
}

static void
st_assign_background_repeat (const StPropertyClass *klass,
                             StProperty            *prop,
                             StThemeNode           *node)
{
  StKeyword kw;

  kw = st_get_actual_keyword (node, prop->value);

  switch (kw)
    {
    case ST_KEYWORD_INHERIT:
      node->background_repeat = node->parent_node->background_repeat;
      break;

    case ST_KEYWORD_INITIAL:
      node->background_repeat = 1;
      break;

    default:
      g_assert (prop->value->value_kind == ST_VALUE_ENUM);

      node->background_repeat = prop->value->value.v_enum;
    }
}

static void
st_assign_font_family (const StPropertyClass *klass,
                       StProperty            *prop,
                       StThemeNode           *node)
{
  const PangoFontDescription *root_font;
  const char *family;
  StKeyword kw;

  kw = st_get_actual_keyword (node, prop->value);

  if (node->font_desc == NULL)
    node->font_desc = pango_font_description_new ();

  switch (kw)
    {
    case ST_KEYWORD_INHERIT:
      family = pango_font_description_get_family (node->parent_node->font_desc);
      break;

    case ST_KEYWORD_INITIAL:
      root_font = st_theme_context_get_font (node->context);
      family = pango_font_description_get_family (root_font);
      break;

    default:
      g_assert (prop->value->value_kind == ST_VALUE_STRING);
      family = prop->value->value.string;
    }

  pango_font_description_set_family (node->font_desc, family);
}

static void
st_assign_font_style (const StPropertyClass *klass,
                      StProperty            *prop,
                      StThemeNode           *node)
{
  const PangoFontDescription *root_font;
  PangoStyle style;
  StKeyword kw;

  kw = st_get_actual_keyword (node, prop->value);

  if (node->font_desc == NULL)
    node->font_desc = pango_font_description_new ();

  switch (kw)
    {
    case ST_KEYWORD_INHERIT:
      style = pango_font_description_get_style (node->parent_node->font_desc);
      break;

    case ST_KEYWORD_INITIAL:
      root_font = st_theme_context_get_font (node->context);
      style = pango_font_description_get_style (root_font);
      break;

    default:
      g_assert (prop->value->value_kind == ST_VALUE_ENUM);
      style = prop->value->value.v_enum;
    }

  pango_font_description_set_style (node->font_desc, style);
}

static void
st_assign_font_variant (const StPropertyClass *klass,
                        StProperty            *prop,
                        StThemeNode           *node)
{
  const PangoFontDescription *root_font;
  PangoVariant variant;
  StKeyword kw;

  kw = st_get_actual_keyword (node, prop->value);

  if (node->font_desc == NULL)
    node->font_desc = pango_font_description_new ();

  switch (kw)
    {
    case ST_KEYWORD_INHERIT:
      variant = pango_font_description_get_variant (node->parent_node->font_desc);
      break;

    case ST_KEYWORD_INITIAL:
      root_font = st_theme_context_get_font (node->context);
      variant = pango_font_description_get_variant (root_font);
      break;

    default:
      g_assert (prop->value->value_kind == ST_VALUE_ENUM);
      variant = prop->value->value.v_enum;
    }

  pango_font_description_set_variant (node->font_desc, variant);
}

static void
st_assign_font_weight (const StPropertyClass *klass,
                       StProperty            *prop,
                       StThemeNode           *node)
{
  const PangoFontDescription *parent_font;
  StKeyword kw;
  int weight;

  kw = st_get_actual_keyword (node, prop->value);

  if (node->font_desc == NULL)
    node->font_desc = pango_font_description_new ();

  if (kw == ST_KEYWORD_INITIAL)
    {
      parent_font = st_theme_context_get_font (node->context);
      weight = pango_font_description_get_weight (parent_font);
    }
  else
    {
      if (node->parent_node)
        parent_font = st_theme_node_get_font (node->parent_node);
      else
        parent_font = st_theme_context_get_font (node->context);

      weight = pango_font_description_get_weight (parent_font);

      if (kw != ST_KEYWORD_INHERIT)
        {
          if (prop->value->value_kind == ST_VALUE_ENUM)
            {
              StFontWeight weight_enum = prop->value->value.v_enum;

              switch (weight_enum)
                {
                case ST_FONT_WEIGHT_NORMAL:
                  weight = PANGO_WEIGHT_NORMAL;
                  break;
                case ST_FONT_WEIGHT_BOLD:
                  weight = PANGO_WEIGHT_BOLD;
                  break;
                case ST_FONT_WEIGHT_BOLDER:
                  weight = MIN (900, weight + 200);
                  break;
                case ST_FONT_WEIGHT_LIGHTER:
                  weight = MAX (100, weight - 200);
                  break;
                }
            }
          else
            {
              g_assert (prop->value->value_kind == ST_VALUE_INT);
              weight = prop->value->value.integer;
            }
        }
    }

  pango_font_description_set_weight (node->font_desc, weight);
}

static void
st_assign_font_size (const StPropertyClass *klass,
                     StProperty            *prop,
                     StThemeNode           *node)
{
  const PangoFontDescription *parent_font;
  float size;
  StKeyword kw;
  gboolean absolute;
  double resolution;

  static const int font_sizes[] = {
    6 * 1024,   /* xx-small */
    8 * 1024,   /* x-small */
    10 * 1024,  /* small */
    12 * 1024,  /* medium */
    16 * 1024,  /* large */
    20 * 1024,  /* x-large */
    24 * 1024,  /* xx-large */
  };

  resolution = clutter_backend_get_resolution (clutter_get_default_backend ());
  kw = st_get_actual_keyword (node, prop->value);

  if (node->font_desc == NULL)
    node->font_desc = pango_font_description_new ();

  if (kw == ST_KEYWORD_INITIAL)
    {
      parent_font = st_theme_context_get_font (node->context);
      size = pango_font_description_get_size (parent_font);
      absolute = pango_font_description_get_size_is_absolute (parent_font);
    }
  else
    {
      if (node->parent_node)
        parent_font = st_theme_node_get_font (node->parent_node);
      else
        parent_font = st_theme_context_get_font (node->context);

      size = pango_font_description_get_size (parent_font);
      absolute = pango_font_description_get_size_is_absolute (parent_font);

      if (kw != ST_KEYWORD_INHERIT)
        {
          /* We work in integers to avoid float comparisons when converting back
           * from a size in pixels to a logical size.
           */
          int size_points = (int)(0.5 + size * (72. / resolution));

          if (prop->value->value_kind == ST_VALUE_ENUM)
            {
              StFontScale scale = prop->value->value.v_enum;

              if (scale == ST_FONT_SCALE_LARGER)
                {
                  /* Find the standard size equal to or larger than the current size */
                  int i = 6;

                  while (i >= 0 && font_sizes[i] > size_points)
                    i--;

                  if (i < 0) /* original size smaller than any standard size */
                    i = 0;

                  /* Go one larger than that, if possible */
                  if (i < 6)
                    i++;
                }
              else if (scale == ST_FONT_SCALE_SMALLER)
                {
                  /* Find the standard size equal to or smaller than the current size */
                  int i = 0;

                  while (i <= 6 && font_sizes[i] < size_points)
                    i++;

                  if (i > 6)
                    {
                      /* original size greater than any standard size */
                      size_points = (int)(0.5 + size_points / 1.2);
                    }
                  else
                    {
                      /* Go one smaller than that, if possible */
                      if (i > 0)
                        i--;

                      size_points = font_sizes[i];
                    }
                }
              else
                {
                  size_points = font_sizes[scale];
                }

              size = size_points * (resolution / 72.);
            }
          else
            {
              st_css_value_compute_length (node, prop->property_id,
                                           prop->value, &size);
            }

          absolute = TRUE;
        }
    }

  if (!absolute)
    size *= (resolution / 72.);
  pango_font_description_set_absolute_size (node->font_desc, size);
}

static void
st_assign_border_image_slice (const StPropertyClass *klass,
                              StProperty            *prop,
                              StThemeNode           *node)
{
  StKeyword kw;
  int i;

  kw = st_get_actual_keyword (node, prop->value);

  switch (kw)
    {
    case ST_KEYWORD_INHERIT:
      for (i = 0; i < 4; i++)
        node->border_image_slice[i] = node->parent_node->border_image_slice[i];
      break;

    case ST_KEYWORD_INITIAL:
      for (i = 0; i < 4; i++)
        node->border_image_slice[i] = 0;
      break;

    default:
      g_assert (prop->value->value_kind == ST_VALUE_ARRAY);
      g_assert (prop->value->value.array->len == 4);

      for (i = 0; i < 4; i++)
        st_css_value_compute_float (node, prop->property_id,
                                    g_ptr_array_index (prop->value->value.array, i),
                                    &node->border_image_slice[i]);
      break;
    }
}

gboolean
st_property_get_color (StProperty   *property,
                       StThemeNode  *node,
                       ClutterColor *out)
{
  StCssValue *v;
  CRTerm *item;

  if (property->value->value_kind != ST_VALUE_COMPLEX)
    return FALSE;

  item = property->value->value.cr_term;
  v = st_css_value_parse_color (&item);

  if (v == NULL)
    return FALSE;

  st_css_value_compute_color (node, property->property_id,
                              v, out);
  return TRUE;
}

gboolean
st_property_get_length (StProperty   *property,
                        StThemeNode  *node,
                        float        *out)
{
  StCssValue *v;
  CRTerm *item;

  if (property->value->value_kind != ST_VALUE_COMPLEX)
    return FALSE;

  item = property->value->value.cr_term;
  v = st_css_value_parse_length (&item);

  if (v == NULL)
    return FALSE;

  st_css_value_compute_length (node, property->property_id,
                               v, out);
  return TRUE;
}

gboolean
st_property_get_double (StProperty   *property,
                        StThemeNode  *node,
                        double       *out)
{
  StCssValue *v;
  CRTerm *item;

  if (property->value->value_kind != ST_VALUE_COMPLEX)
    return FALSE;

  item = property->value->value.cr_term;
  v = st_css_value_parse_double (&item);

  if (v == NULL)
    return FALSE;

  st_css_value_compute_double (node, property->property_id,
                               v, out);
  return TRUE;
}

gboolean
st_property_get_shadow (StProperty   *property,
                        StThemeNode  *node,
                        StShadow    **out)
{
  StCssValue *v;
  CRTerm *item;

  if (property->value->value_kind != ST_VALUE_COMPLEX)
    return FALSE;

  item = property->value->value.cr_term;
  v = st_css_value_parse_shadow (&item);

  if (v == NULL)
    return FALSE;

  *out = g_slice_new (StShadow);
  st_css_value_compute_shadow (node, property->property_id,
                               v, *out);
  return TRUE;
}

