/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * st-theme-node.c: style information for one node in a tree of themed objects
 *
 * Copyright 2008-2010 Red Hat, Inc.
 * Copyright 2009 Steve Frécinaux
 * Copyright 2009, 2010 Florian Müllner
 * Copyright 2010 Adel Gadllah
 * Copyright 2010 Giovanni Campagna
 * Copyright 2011 Quentin "Sardem FF7" Glidic
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <string.h>

#include "st-theme-context.h"
#include "st-theme-node-private.h"

static void st_theme_node_init               (StThemeNode          *node);
static void st_theme_node_class_init         (StThemeNodeClass     *klass);
static void st_theme_node_dispose           (GObject                 *object);
static void st_theme_node_finalize           (GObject                 *object);

extern gfloat st_slow_down_factor;

G_DEFINE_TYPE (StThemeNode, st_theme_node, G_TYPE_OBJECT)

static void
st_theme_node_init (StThemeNode *node)
{
  node->transition_duration = -1;
  _st_theme_node_init_drawing_state (node);
}

static void
st_theme_node_class_init (StThemeNodeClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = st_theme_node_dispose;
  object_class->finalize = st_theme_node_finalize;
}


static void
st_theme_node_dispose (GObject *gobject)
{
  StThemeNode *node = ST_THEME_NODE (gobject);

  if (node->parent_node)
    {
      g_object_unref (node->parent_node);
      node->parent_node = NULL;
    }

  if (node->icon_colors)
    {
      st_icon_colors_unref (node->icon_colors);
      node->icon_colors = NULL;
    }

  G_OBJECT_CLASS (st_theme_node_parent_class)->dispose (gobject);
}

static void
st_theme_node_finalize (GObject *object)
{
  StThemeNode *node = ST_THEME_NODE (object);

  g_array_unref (node->element_classes);
  g_array_unref (node->pseudo_classes);
  g_free (node->inline_style);

  g_hash_table_unref (node->custom_properties);

  if (node->inline_properties)
    {
      /* This destroys the list, not just the head of the list */
      cr_declaration_destroy (node->inline_properties);
    }

  if (node->font_desc)
    {
      pango_font_description_free (node->font_desc);
      node->font_desc = NULL;
    }

  if (node->box_shadow)
    {
      st_shadow_unref (node->box_shadow);
      node->box_shadow = NULL;
    }

  if (node->background_image_shadow)
    {
      st_shadow_unref (node->background_image_shadow);
      node->background_image_shadow = NULL;
    }

  if (node->text_shadow)
    {
      st_shadow_unref (node->text_shadow);
      node->text_shadow = NULL;
    }

  if (node->background_image)
    g_object_unref (node->background_image);
  if (node->border_image_source)
    g_object_unref (node->border_image_source);

  _st_theme_node_free_drawing_state (node);

  G_OBJECT_CLASS (st_theme_node_parent_class)->finalize (object);
}

/**
 * st_theme_node_new:
 * @context: the context representing global state for this themed tree
 * @parent_node: (allow-none): the parent node of this node
 * @element_type: the type of the GObject represented by this node
 *  in the tree (corresponding to an element if we were theming an XML
 *  document. %G_TYPE_NONE means this style was created for the stage
 * actor and matches a selector element name of 'stage'.
 * @element_id: (allow-none): the ID to match CSS rules against
 * @element_class: (allow-none): a whitespace-separated list of classes
 *   to match CSS rules against
 * @pseudo_class: (allow-none): a whitespace-separated list of pseudo-classes
 *   (like 'hover' or 'visited') to match CSS rules against
 *
 * Creates a new #StThemeNode. Once created, a node is immutable. Of any
 * of the attributes of the node (like the @element_class) change the node
 * and its child nodes must be destroyed and recreated.
 *
 * Return value: (transfer full): the theme node
 */
StThemeNode *
st_theme_node_new (StThemeContext    *context,
                   StThemeNode       *parent_node,
                   GType              element_type,
                   const char        *element_id,
                   GArray            *element_classes,
                   GArray            *pseudo_classes,
                   const char        *inline_style)
{
  StThemeNode *node;

  g_return_val_if_fail (ST_IS_THEME_CONTEXT (context), NULL);
  g_return_val_if_fail (parent_node == NULL || ST_IS_THEME_NODE (parent_node), NULL);

  node = g_object_new (ST_TYPE_THEME_NODE, NULL);

  node->context = context;
  if (parent_node != NULL)
    node->parent_node = g_object_ref (parent_node);
  else
    node->parent_node = NULL;

  node->cascade = st_theme_context_get_cascade (context);

  node->element_type = element_type;
  node->element_id = g_quark_from_string (element_id);
  node->element_classes = g_array_ref (element_classes);
  node->pseudo_classes = g_array_ref (pseudo_classes);
  node->inline_style = g_strdup (inline_style);

  return node;
}

/**
 * st_theme_node_get_parent:
 * @node: a #StThemeNode
 *
 * Gets the parent themed element node.
 *
 * Return value: (transfer none): the parent #StThemeNode, or %NULL if this
 *  is the root node of the tree of theme elements.
 */
StThemeNode *
st_theme_node_get_parent (StThemeNode *node)
{
  g_return_val_if_fail (ST_IS_THEME_NODE (node), NULL);

  return node->parent_node;
}

static gboolean
g_quark_array_equal (GArray *one, GArray *two)
{
  int i;

  if (one->len != two->len)
    return FALSE;

  for (i = 0; i < one->len; i++)
    if (g_array_index (one, GQuark, i) != g_array_index (two, GQuark, i))
      return FALSE;

  return TRUE;
}

/**
 * st_theme_node_equal:
 * @node_a: first #StThemeNode
 * @node_b: second #StThemeNode
 *
 * Compare two #StThemeNodes. Two nodes which compare equal will match
 * the same CSS rules and have the same style properties. However, two
 * nodes that have ended up with identical style properties do not
 * necessarily compare equal.
 * In detail, @node_a and @node_b are considered equal iff
 * <itemizedlist>
 *   <listitem>
 *     <para>they share the same #StTheme and #StThemeContext</para>
 *   </listitem>
 *   <listitem>
 *     <para>they have the same parent</para>
 *   </listitem>
 *   <listitem>
 *     <para>they have the same element type</para>
 *   </listitem>
 *   <listitem>
 *     <para>their id, class, pseudo-class and inline-style match</para>
 *   </listitem>
 * </itemizedlist>
 *
 * Returns: %TRUE if @node_a equals @node_b
 */
gboolean
st_theme_node_equal (StThemeNode *node_a, StThemeNode *node_b)
{
  g_return_val_if_fail (ST_IS_THEME_NODE (node_a), FALSE);

  if (node_a == node_b)
    return TRUE;

  g_return_val_if_fail (ST_IS_THEME_NODE (node_b), FALSE);

  return (node_a->parent_node  == node_b->parent_node  &&
          node_a->context      == node_b->context      &&
          node_a->element_type == node_b->element_type &&
          node_a->element_id   == node_b->element_id   &&
          g_quark_array_equal (node_a->element_classes, node_b->element_classes) &&
          g_quark_array_equal (node_a->pseudo_classes, node_b->pseudo_classes));
}

guint
st_theme_node_hash (StThemeNode *node)
{
  int i;

  guint hash = GPOINTER_TO_UINT (node->parent_node);

  hash = hash * 33 + GPOINTER_TO_UINT (node->context);
  hash = hash * 33 + ((guint) node->element_type);
  hash = hash * 33 + ((guint) node->element_id);

  if (node->inline_style != NULL)
    hash = hash * 33 + g_str_hash (node->inline_style);

  for (i = 0; i < node->element_classes->len; i++)
    {
      GQuark q;

      q = g_array_index (node->element_classes, GQuark, i);
      hash = hash * 33 + ((guint) q);
    }

  for (i = 0; i < node->pseudo_classes->len; i++)
    {
      GQuark q;

      q = g_array_index (node->pseudo_classes, GQuark, i);
      hash = hash * 33 + ((guint) q);
    }

  return hash;
}

static void
add_properties_from_array (GHashTable *property_bag,
                           GHashTable *custom_properties,
                           GPtrArray  *properties)
{
  StProperty *prop;
  int i;

  for (i = 0; i < properties->len; i++)
    {
      prop = g_ptr_array_index (properties, i);

      if (prop->property_id != ST_PROPERTY_UNKNOWN)
        g_hash_table_replace (property_bag, GINT_TO_POINTER (prop->property_id),
                              st_property_ref (prop));
      else
        g_hash_table_replace (custom_properties, GINT_TO_POINTER (prop->property_name),
                              st_property_ref (prop));
    }
}

static void
assign_properties (StThemeNode *node,
                   GHashTable  *property_bag)
{
  GHashTableIter iter;
  StProperty *prop;
  int i;

  /* First handle the special properties in order */
  for (i = ST_PROPERTY_UNKNOWN + 1; i < ST_PROPERTY_N_SPECIALS; i++)
    {
      prop = g_hash_table_lookup (property_bag, GINT_TO_POINTER (i));

      st_property_assign (prop, node);
      g_hash_table_remove (property_bag, GINT_TO_POINTER (i));
    }

  /* Then handle everything else, unsorted */
  g_hash_table_iter_init (&iter, property_bag);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer*) &prop))
    st_property_assign (prop, node);
}

void
_st_theme_node_ensure_computed (StThemeNode *node)
{
  GPtrArray *properties = NULL;
  GHashTable *property_bag;

  if (G_LIKELY (node->properties_computed))
    return;

  node->custom_properties = g_hash_table_new_full (g_direct_hash, g_direct_equal,
                                                   NULL, (GDestroyNotify) st_property_unref);
  property_bag = g_hash_table_new_full (g_direct_hash, g_direct_equal,
                                        NULL, (GDestroyNotify) st_property_unref);

  properties = st_cascade_get_matched_properties (node->cascade, node);
  add_properties_from_array (property_bag, node->custom_properties, properties);
  g_ptr_array_unref (properties);

  if (node->inline_style)
    {
      GPtrArray *inline_props;

      inline_props = st_cascade_parse_declaration_list (node->cascade,
                                                        node->inline_style);
      add_properties_from_array (property_bag, node->custom_properties, inline_props);
      g_ptr_array_unref (inline_props);
    }

  assign_properties (node, property_bag);

  if (node->width != -1)
    {
      if (node->min_width == -1)
        node->min_width = node->width;
      else if (node->width < node->min_width)
        node->width = node->min_width;
      if (node->max_width == -1)
        node->max_width = node->width;
      else if (node->width > node->max_width)
        node->width = node->max_width;
    }

  if (node->height != -1)
    {
      if (node->min_height == -1)
        node->min_height = node->height;
      else if (node->height < node->min_height)
        node->height = node->min_height;
      if (node->max_height == -1)
        node->max_height = node->height;
      else if (node->height > node->max_height)
        node->height = node->max_height;
    }

  node->properties_computed = TRUE;
}

/**
 * st_theme_node_lookup_color:
 * @node: a #StThemeNode
 * @property_name: The name of the color property
 * @inherit: if %TRUE, if a value is not found for the property on the
 *   node, then it will be looked up on the parent node, and then on the
 *   parent's parent, and so forth. Note that if the property has a
 *   value of 'inherit' it will be inherited even if %FALSE is passed
 *   in for @inherit; this only affects the default behavior for inheritance.
 * @color: (out caller-allocates): location to store the color that was
 *   determined. If the property is not found, the value in this location
 *   will not be changed.
 *
 * Generically looks up a property containing a single color value. When
 * specific getters (like st_theme_node_get_background_color()) exist, they
 * should be used instead. They are cached, so more efficient, and have
 * handling for shortcut properties and other details of CSS.
 *
 * See also st_theme_node_get_color(), which provides a simpler API.
 *
 * Return value: %TRUE if the property was found in the properties for this
 *  theme node (or in the properties of parent nodes when inheriting.)
 */
gboolean
st_theme_node_lookup_color (StThemeNode  *node,
                            const char   *property_name,
                            gboolean      inherit,
                            ClutterColor *color)
{
  GQuark property_quark;
  StProperty *prop;

  _st_theme_node_ensure_computed (node);

  property_quark = g_quark_from_string (property_name);
  prop = g_hash_table_lookup (node->custom_properties, GINT_TO_POINTER (property_quark));

  if (prop)
    return st_property_get_color (prop, node, color);
  else if (inherit && node->parent_node)
    return st_theme_node_lookup_color (node->parent_node, property_name, inherit, color);
  else
    return FALSE;
}

/**
 * st_theme_node_get_color:
 * @node: a #StThemeNode
 * @property_name: The name of the color property
 * @color: (out caller-allocates): location to store the color that
 *   was determined.
 *
 * Generically looks up a property containing a single color value. When
 * specific getters (like st_theme_node_get_background_color()) exist, they
 * should be used instead. They are cached, so more efficient, and have
 * handling for shortcut properties and other details of CSS.
 *
 * If @property_name is not found, a warning will be logged and a
 * default color returned.
 *
 * See also st_theme_node_lookup_color(), which provides more options,
 * and lets you handle the case where the theme does not specify the
 * indicated color.
 */
void
st_theme_node_get_color (StThemeNode  *node,
                         const char   *property_name,
                         ClutterColor *color)
{
  if (!st_theme_node_lookup_color (node, property_name, FALSE, color))
    {
      g_warning ("Did not find color property '%s'", property_name);
      memset (color, 0, sizeof (ClutterColor));
    }
}

/**
 * st_theme_node_lookup_double:
 * @node: a #StThemeNode
 * @property_name: The name of the numeric property
 * @inherit: if %TRUE, if a value is not found for the property on the
 *   node, then it will be looked up on the parent node, and then on the
 *   parent's parent, and so forth. Note that if the property has a
 *   value of 'inherit' it will be inherited even if %FALSE is passed
 *   in for @inherit; this only affects the default behavior for inheritance.
 * @value: (out): location to store the value that was determined.
 *   If the property is not found, the value in this location
 *   will not be changed.
 *
 * Generically looks up a property containing a single numeric value
 *  without units.
 *
 * See also st_theme_node_get_double(), which provides a simpler API.
 *
 * Return value: %TRUE if the property was found in the properties for this
 *  theme node (or in the properties of parent nodes when inheriting.)
 */
gboolean
st_theme_node_lookup_double (StThemeNode *node,
                             const char  *property_name,
                             gboolean     inherit,
                             double      *value)
{
  GQuark property_quark;
  StProperty *prop;

  _st_theme_node_ensure_computed (node);

  property_quark = g_quark_from_string (property_name);
  prop = g_hash_table_lookup (node->custom_properties, GINT_TO_POINTER (property_quark));

  if (prop)
    return st_property_get_double (prop, node, value);
  else if (inherit && node->parent_node)
    return st_theme_node_lookup_double (node->parent_node, property_name, inherit, value);
  else
    return FALSE;
}

/**
 * st_theme_node_get_double:
 * @node: a #StThemeNode
 * @property_name: The name of the numeric property
 *
 * Generically looks up a property containing a single numeric value
 *  without units.
 *
 * See also st_theme_node_lookup_double(), which provides more options,
 * and lets you handle the case where the theme does not specify the
 * indicated value.
 *
 * Return value: the value found. If @property_name is not
 *  found, a warning will be logged and 0 will be returned.
 */
gdouble
st_theme_node_get_double (StThemeNode *node,
                          const char  *property_name)
{
  gdouble value;

  if (st_theme_node_lookup_double (node, property_name, FALSE, &value))
    return value;
  else
    {
      g_warning ("Did not find double property '%s'", property_name);
      return 0.0;
    }
}

/**
 * st_theme_node_lookup_length:
 * @node: a #StThemeNode
 * @property_name: The name of the length property
 * @inherit: if %TRUE, if a value is not found for the property on the
 *   node, then it will be looked up on the parent node, and then on the
 *   parent's parent, and so forth. Note that if the property has a
 *   value of 'inherit' it will be inherited even if %FALSE is passed
 *   in for @inherit; this only affects the default behavior for inheritance.
 * @length: (out): location to store the length that was determined.
 *   If the property is not found, the value in this location
 *   will not be changed. The returned length is resolved
 *   to pixels.
 *
 * Generically looks up a property containing a single length value. When
 * specific getters (like st_theme_node_get_border_width()) exist, they
 * should be used instead. They are cached, so more efficient, and have
 * handling for shortcut properties and other details of CSS.
 *
 * See also st_theme_node_get_length(), which provides a simpler API.
 *
 * Return value: %TRUE if the property was found in the properties for this
 *  theme node (or in the properties of parent nodes when inheriting.)
 */
gboolean
st_theme_node_lookup_length (StThemeNode *node,
                             const char  *property_name,
                             gboolean     inherit,
                             float       *length)
{
  GQuark property_quark;
  StProperty *prop;

  _st_theme_node_ensure_computed (node);

  property_quark = g_quark_from_string (property_name);
  prop = g_hash_table_lookup (node->custom_properties, GINT_TO_POINTER (property_quark));

  if (prop)
    return st_property_get_length (prop, node, length);
  else if (inherit && node->parent_node)
    return st_theme_node_lookup_length (node->parent_node, property_name, inherit, length);
  else
    return FALSE;
}

/**
 * st_theme_node_get_length:
 * @node: a #StThemeNode
 * @property_name: The name of the length property
 *
 * Generically looks up a property containing a single length value. When
 * specific getters (like st_theme_node_get_border_width()) exist, they
 * should be used instead. They are cached, so more efficient, and have
 * handling for shortcut properties and other details of CSS.
 *
 * Unlike st_theme_node_get_color() and st_theme_node_get_double(),
 * this does not print a warning if the property is not found; it just
 * returns 0.
 *
 * See also st_theme_node_lookup_length(), which provides more options.
 *
 * Return value: the length, in pixels, or 0 if the property was not found.
 */
float
st_theme_node_get_length (StThemeNode *node,
                          const char  *property_name)
{
  float length;

  if (st_theme_node_lookup_length (node, property_name, FALSE, &length))
    return length;
  else
    return 0;
}

float
st_theme_node_get_border_width (StThemeNode *node,
                                StSide       side)
{
  g_return_val_if_fail (ST_IS_THEME_NODE (node), 0.);
  g_return_val_if_fail (side >= ST_SIDE_TOP && side <= ST_SIDE_LEFT, 0.);

  _st_theme_node_ensure_computed (node);

  return node->border_width[side];
}

float
st_theme_node_get_border_radius (StThemeNode *node,
                                 StCorner     corner)
{
  g_return_val_if_fail (ST_IS_THEME_NODE (node), 0.);
  g_return_val_if_fail (corner >= ST_CORNER_TOPLEFT && corner <= ST_CORNER_BOTTOMLEFT, 0.);

  _st_theme_node_ensure_computed (node);

  return node->border_radius[corner];
}

float
st_theme_node_get_outline_width (StThemeNode  *node)
{
  g_return_val_if_fail (ST_IS_THEME_NODE (node), 0);

  _st_theme_node_ensure_computed (node);

  return node->outline_width;
}

/**
 * st_theme_node_get_outline_color:
 * @node: a #StThemeNode
 * @color: (out caller-allocates): location to store the color
 *
 * Gets the color of @node's outline.
 */
void
st_theme_node_get_outline_color (StThemeNode  *node,
                                 ClutterColor *color)
{
  g_return_if_fail (ST_IS_THEME_NODE (node));

  _st_theme_node_ensure_computed (node);
  *color = node->outline_color;
}

float
st_theme_node_get_width (StThemeNode *node)
{
  g_return_val_if_fail (ST_IS_THEME_NODE (node), -1);

  _st_theme_node_ensure_computed (node);
  return node->width;
}

float
st_theme_node_get_height (StThemeNode *node)
{
  g_return_val_if_fail (ST_IS_THEME_NODE (node), -1);

  _st_theme_node_ensure_computed (node);
  return node->height;
}

float
st_theme_node_get_min_width (StThemeNode *node)
{
  g_return_val_if_fail (ST_IS_THEME_NODE (node), -1);

  _st_theme_node_ensure_computed (node);
  return node->min_width;
}

float
st_theme_node_get_min_height (StThemeNode *node)
{
  g_return_val_if_fail (ST_IS_THEME_NODE (node), -1);

  _st_theme_node_ensure_computed (node);
  return node->min_height;
}

float
st_theme_node_get_max_width (StThemeNode *node)
{
  g_return_val_if_fail (ST_IS_THEME_NODE (node), -1);

  _st_theme_node_ensure_computed (node);
  return node->max_width;
}

float
st_theme_node_get_max_height (StThemeNode *node)
{
  g_return_val_if_fail (ST_IS_THEME_NODE (node), -1);

  _st_theme_node_ensure_computed (node);
  return node->max_height;
}

/**
 * st_theme_node_get_background_color:
 * @node: a #StThemeNode
 * @color: (out caller-allocates): location to store the color
 *
 * Gets @node's background color.
 */
void
st_theme_node_get_background_color (StThemeNode  *node,
                                    ClutterColor *color)
{
  g_return_if_fail (ST_IS_THEME_NODE (node));

  _st_theme_node_ensure_computed (node);
  *color = node->background_color;
}

/**
 * st_theme_node_get_background_image:
 * @node: a #StThemeNode
 *
 * Gets @node's background image.
 *
 * Returns: (transfer none):
 */
GFile *
st_theme_node_get_background_image (StThemeNode *node)
{
  g_return_val_if_fail (ST_IS_THEME_NODE (node), NULL);

  _st_theme_node_ensure_computed (node);
  return node->background_image;
}

/**
 * st_theme_node_get_foreground_color:
 * @node: a #StThemeNode
 * @color: (out caller-allocates): location to store the color
 *
 * Gets @node's foreground color.
 */
void
st_theme_node_get_foreground_color (StThemeNode  *node,
                                    ClutterColor *color)
{
  g_return_if_fail (ST_IS_THEME_NODE (node));

  _st_theme_node_ensure_computed (node);
  *color = node->foreground_color;
}

/**
 * st_theme_node_get_background_gradient:
 * @node: A #StThemeNode
 * @type: (out): Type of gradient
 * @start: (out caller-allocates): Color at start of gradient
 * @end: (out caller-allocates): Color at end of gradient
 *
 * The @start and @end arguments will only be set if @type is not #ST_GRADIENT_NONE.
 */
void
st_theme_node_get_background_gradient (StThemeNode    *node,
                                       StGradientType *type,
                                       ClutterColor   *start,
                                       ClutterColor   *end)
{
  g_return_if_fail (ST_IS_THEME_NODE (node));

  _st_theme_node_ensure_computed (node);

  *type = node->background_gradient_type;
  if (*type != ST_GRADIENT_NONE)
    {
      *start = node->background_color;
      *end = node->background_gradient_end;
    }
}

/**
 * st_theme_node_get_border_color:
 * @node: a #StThemeNode
 * @side: a #StSide
 * @color: (out caller-allocates): location to store the color
 *
 * Gets the color of @node's border on @side
 */
void
st_theme_node_get_border_color (StThemeNode  *node,
                                StSide        side,
                                ClutterColor *color)
{
  g_return_if_fail (ST_IS_THEME_NODE (node));
  g_return_if_fail (side >= ST_SIDE_TOP && side <= ST_SIDE_LEFT);

  _st_theme_node_ensure_computed (node);
  *color = node->border_color[side];
}

float
st_theme_node_get_padding (StThemeNode *node,
                           StSide       side)
{
  g_return_val_if_fail (ST_IS_THEME_NODE (node), 0.);
  g_return_val_if_fail (side >= ST_SIDE_TOP && side <= ST_SIDE_LEFT, 0.);

  _st_theme_node_ensure_computed (node);
  return node->padding[side];
}

/**
 * st_theme_node_get_transition_duration:
 * @node: an #StThemeNode
 *
 * Get the value of the transition-duration property, which
 * specifies the transition time between the previous #StThemeNode
 * and @node.
 *
 * Returns: the node's transition duration in milliseconds
 */
int
st_theme_node_get_transition_duration (StThemeNode *node)
{
  g_return_val_if_fail (ST_IS_THEME_NODE (node), 0);

  _st_theme_node_ensure_computed (node);

  if (node->transition_duration > -1)
    return st_slow_down_factor * node->transition_duration;

  return st_slow_down_factor * node->transition_duration;
}

StTextDecoration
st_theme_node_get_text_decoration (StThemeNode *node)
{
  _st_theme_node_ensure_computed (node);
  return node->text_decoration;
}

StTextAlign
st_theme_node_get_text_align(StThemeNode *node)
{
  _st_theme_node_ensure_computed (node);
  return node->text_align;
}

const PangoFontDescription *
st_theme_node_get_font (StThemeNode *node)
{
  _st_theme_node_ensure_computed (node);
  return node->font_desc;
}

/**
 * st_theme_node_get_border_image_source:
 * @node: a #StThemeNode
 *
 * Gets the file which is used to paint @node's border-image.
 *
 * Returns: (transfer none):
 */
GFile *
st_theme_node_get_border_image_source (StThemeNode *node)
{
  _st_theme_node_ensure_computed (node);
  return node->border_image_source;
}

/**
 * st_theme_node_get_horizontal_padding:
 * @node: a #StThemeNode
 *
 * Gets the total horizonal padding (left + right padding)
 *
 * Return value: the total horizonal padding
 *   in pixels
 */
float
st_theme_node_get_horizontal_padding (StThemeNode *node)
{
  float padding = 0.0;
  padding += st_theme_node_get_padding (node, ST_SIDE_LEFT);
  padding += st_theme_node_get_padding (node, ST_SIDE_RIGHT);

  return padding;
}

/**
 * st_theme_node_get_vertical_padding:
 * @node: a #StThemeNode
 *
 * Gets the total vertical padding (top + bottom padding)
 *
 * Return value: the total vertical padding
 *   in pixels
 */
float
st_theme_node_get_vertical_padding (StThemeNode *node)
{
  float padding = 0.0;
  padding += st_theme_node_get_padding (node, ST_SIDE_TOP);
  padding += st_theme_node_get_padding (node, ST_SIDE_BOTTOM);

  return padding;
}

/**
 * st_theme_node_lookup_shadow:
 * @node: a #StThemeNode
 * @property_name: The name of the shadow property
 * @inherit: if %TRUE, if a value is not found for the property on the
 *   node, then it will be looked up on the parent node, and then on the
 *   parent's parent, and so forth. Note that if the property has a
 *   value of 'inherit' it will be inherited even if %FALSE is passed
 *   in for @inherit; this only affects the default behavior for inheritance.
 * @shadow: (out): location to store the shadow
 *
 * If the property is not found, the value in the shadow variable will not
 * be changed.
 *
 * Generically looks up a property containing a set of shadow values. When
 * specific getters (like st_theme_node_get_box_shadow ()) exist, they
 * should be used instead. They are cached, so more efficient, and have
 * handling for shortcut properties and other details of CSS.
 *
 * See also st_theme_node_get_shadow(), which provides a simpler API.
 *
 * Return value: %TRUE if the property was found in the properties for this
 *  theme node (or in the properties of parent nodes when inheriting.)
 */
gboolean
st_theme_node_lookup_shadow (StThemeNode  *node,
                             const char   *property_name,
                             gboolean      inherit,
                             StShadow    **shadow)
{
  GQuark property_quark;
  StProperty *prop;

  _st_theme_node_ensure_computed (node);

  property_quark = g_quark_from_string (property_name);
  prop = g_hash_table_lookup (node->custom_properties, GINT_TO_POINTER (property_quark));

  if (prop)
    return st_property_get_shadow (prop, node, shadow);
  else if (inherit && node->parent_node)
    return st_theme_node_lookup_shadow (node->parent_node, property_name, inherit, shadow);
  else
    return FALSE;
}

/**
 * st_theme_node_get_shadow:
 * @node: a #StThemeNode
 * @property_name: The name of the shadow property
 *
 * Generically looks up a property containing a set of shadow values. When
 * specific getters (like st_theme_node_get_box_shadow()) exist, they
 * should be used instead. They are cached, so more efficient, and have
 * handling for shortcut properties and other details of CSS.
 *
 * Like st_theme_get_length(), this does not print a warning if the property is
 * not found; it just returns %NULL
 *
 * See also st_theme_node_lookup_shadow (), which provides more options.
 *
 * Return value: (transfer full): the shadow, or %NULL if the property was not found.
 */
StShadow *
st_theme_node_get_shadow (StThemeNode  *node,
                          const char   *property_name)
{
  StShadow *shadow;

  if (st_theme_node_lookup_shadow (node, property_name, FALSE, &shadow))
    return shadow;
  else
    return NULL;
}

/**
 * st_theme_node_get_box_shadow:
 * @node: a #StThemeNode
 *
 * Gets the value for the box-shadow style property
 *
 * Return value: (transfer none): the node's shadow, or %NULL
 *   if node has no shadow
 */
StShadow *
st_theme_node_get_box_shadow (StThemeNode *node)
{
  _st_theme_node_ensure_computed (node);
  return node->box_shadow;
}

/**
 * st_theme_node_get_background_image_shadow:
 * @node: a #StThemeNode
 *
 * Gets the value for the -st-background-image-shadow style property
 *
 * Return value: (transfer none): the node's background image shadow, or %NULL
 *   if node has no such shadow
 */
StShadow *
st_theme_node_get_background_image_shadow (StThemeNode *node)
{
  _st_theme_node_ensure_computed (node);
  return node->background_image_shadow;
}

/**
 * st_theme_node_get_text_shadow:
 * @node: a #StThemeNode
 *
 * Gets the value for the text-shadow style property
 *
 * Return value: (transfer none): the node's text-shadow, or %NULL
 *   if node has no text-shadow
 */
StShadow *
st_theme_node_get_text_shadow (StThemeNode *node)
{
  _st_theme_node_ensure_computed (node);
  return node->text_shadow;
}

/**
 * st_theme_node_get_icon_colors:
 * @node: a #StThemeNode
 *
 * Gets the colors that should be used for colorizing symbolic icons according
 * the style of this node.
 *
 * Return value: (transfer full): the icon colors to use for this theme node
 */
StIconColors *
st_theme_node_get_icon_colors (StThemeNode *node)
{
  StIconColors *colors;

  _st_theme_node_ensure_computed (node);
  colors = st_icon_colors_new ();
  colors->foreground = node->foreground_color;
  colors->warning = node->warning_color;
  colors->error = node->error_color;
  colors->success = node->success_color;

  return colors;
}

static float
get_width_inc (StThemeNode *node)
{
  return ((int)(0.5 + node->border_width[ST_SIDE_LEFT]) + node->padding[ST_SIDE_LEFT] +
          (int)(0.5 + node->border_width[ST_SIDE_RIGHT]) + node->padding[ST_SIDE_RIGHT]);
}

static float
get_height_inc (StThemeNode *node)
{
  return ((int)(0.5 + node->border_width[ST_SIDE_TOP]) + node->padding[ST_SIDE_TOP] +
          (int)(0.5 + node->border_width[ST_SIDE_BOTTOM]) + node->padding[ST_SIDE_BOTTOM]);
}

/**
 * st_theme_node_adjust_for_height:
 * @node: a #StThemeNode
 * @for_height: (inout): the "for height" to adjust
 *
 * Adjusts a "for height" passed to clutter_actor_get_preferred_width() to
 * account for borders and padding. This is a convenience function meant
 * to be called from a get_preferred_width() method of a #ClutterActor
 * subclass. The value after adjustment is the height available for the actor's
 * content.
 */
void
st_theme_node_adjust_for_height (StThemeNode  *node,
                                 float        *for_height)
{
  g_return_if_fail (ST_IS_THEME_NODE (node));
  g_return_if_fail (for_height != NULL);

  _st_theme_node_ensure_computed (node);

  if (*for_height >= 0)
    {
      float height_inc = get_height_inc (node);
      *for_height = MAX (0, *for_height - height_inc);
    }
}

/**
 * st_theme_node_adjust_preferred_width:
 * @node: a #StThemeNode
 * @min_width_p: (inout) (allow-none): the minimum width to adjust
 * @natural_width_p: (inout): the natural width to adjust
 *
 * Adjusts the minimum and natural width computed for an actor by
 * adding on the necessary space for borders and padding and taking
 * into account any minimum or maximum width. This is a convenience
 * function meant to be called from the get_preferred_width() method
 * of a #ClutterActor subclass
 */
void
st_theme_node_adjust_preferred_width (StThemeNode  *node,
                                      float        *min_width_p,
                                      float        *natural_width_p)
{
  float width_inc;

  g_return_if_fail (ST_IS_THEME_NODE (node));

  _st_theme_node_ensure_computed (node);

  width_inc = get_width_inc (node);

  if (min_width_p)
    {
      if (node->min_width != -1)
        *min_width_p = node->min_width;
      *min_width_p += width_inc;
    }

  if (natural_width_p)
    {
      if (node->width != -1)
        *natural_width_p = node->width;
      if (node->max_width != -1)
        *natural_width_p = MIN (*natural_width_p, node->max_width);
      *natural_width_p += width_inc;
    }
}

/**
 * st_theme_node_adjust_for_width:
 * @node: a #StThemeNode
 * @for_width: (inout): the "for width" to adjust
 *
 * Adjusts a "for width" passed to clutter_actor_get_preferred_height() to
 * account for borders and padding. This is a convenience function meant
 * to be called from a get_preferred_height() method of a #ClutterActor
 * subclass. The value after adjustment is the width available for the actor's
 * content.
 */
void
st_theme_node_adjust_for_width (StThemeNode  *node,
                                float        *for_width)
{
  g_return_if_fail (ST_IS_THEME_NODE (node));
  g_return_if_fail (for_width != NULL);

  _st_theme_node_ensure_computed (node);

  if (*for_width >= 0)
    {
      float width_inc = get_width_inc (node);
      *for_width = MAX (0, *for_width - width_inc);
    }
}

/**
 * st_theme_node_adjust_preferred_height:
 * @node: a #StThemeNode
 * @min_height_p: (inout) (allow-none): the minimum height to adjust
 * @natural_height_p: (inout): the natural height to adjust
 *
 * Adjusts the minimum and natural height computed for an actor by
 * adding on the necessary space for borders and padding and taking
 * into account any minimum or maximum height. This is a convenience
 * function meant to be called from the get_preferred_height() method
 * of a #ClutterActor subclass
 */
void
st_theme_node_adjust_preferred_height (StThemeNode  *node,
                                       float           *min_height_p,
                                       float           *natural_height_p)
{
  float height_inc;

  g_return_if_fail (ST_IS_THEME_NODE (node));

  _st_theme_node_ensure_computed (node);

  height_inc = get_height_inc (node);

  if (min_height_p)
    {
      if (node->min_height != -1)
        *min_height_p = node->min_height;
      *min_height_p += height_inc;
    }
  if (natural_height_p)
    {
      if (node->height != -1)
        *natural_height_p = node->height;
      if (node->max_height != -1)
        *natural_height_p = MIN (*natural_height_p, node->max_height);
      *natural_height_p += height_inc;
    }
}

/**
 * st_theme_node_get_content_box:
 * @node: a #StThemeNode
 * @allocation: the box allocated to a #ClutterAlctor
 * @content_box: (out caller-allocates): computed box occupied by the actor's content
 *
 * Gets the box within an actor's allocation that contents the content
 * of an actor (excluding borders and padding). This is a convenience function
 * meant to be used from the allocate() or paint() methods of a #ClutterActor
 * subclass.
 */
void
st_theme_node_get_content_box (StThemeNode           *node,
                               const ClutterActorBox *allocation,
                               ClutterActorBox       *content_box)
{
  double noncontent_left, noncontent_top, noncontent_right, noncontent_bottom;
  double avail_width, avail_height, content_width, content_height;

  g_return_if_fail (ST_IS_THEME_NODE (node));

  _st_theme_node_ensure_computed (node);

  avail_width = allocation->x2 - allocation->x1;
  avail_height = allocation->y2 - allocation->y1;

  noncontent_left = node->border_width[ST_SIDE_LEFT] + node->padding[ST_SIDE_LEFT];
  noncontent_top = node->border_width[ST_SIDE_TOP] + node->padding[ST_SIDE_TOP];
  noncontent_right = node->border_width[ST_SIDE_RIGHT] + node->padding[ST_SIDE_RIGHT];
  noncontent_bottom = node->border_width[ST_SIDE_BOTTOM] + node->padding[ST_SIDE_BOTTOM];

  content_box->x1 = (int)(0.5 + noncontent_left);
  content_box->y1 = (int)(0.5 + noncontent_top);

  content_width = avail_width - noncontent_left - noncontent_right;
  if (content_width < 0)
    content_width = 0;
  content_height = avail_height - noncontent_top - noncontent_bottom;
  if (content_height < 0)
    content_height = 0;

  content_box->x2 = (int)(0.5 + content_box->x1 + content_width);
  content_box->y2 = (int)(0.5 + content_box->y1 + content_height);
}

/**
 * st_theme_node_get_background_paint_box:
 * @node: a #StThemeNode
 * @allocation: the box allocated to a #ClutterActor
 * @paint_box: (out caller-allocates): computed box occupied when painting the actor's background
 *
 * Gets the box used to paint the actor's background, including the area
 * occupied by properties which paint outside the actor's assigned allocation.
 */
void
st_theme_node_get_background_paint_box (StThemeNode           *node,
                                        const ClutterActorBox *actor_box,
                                        ClutterActorBox       *paint_box)
{
  StShadow *background_image_shadow;
  ClutterActorBox shadow_box;

  g_return_if_fail (ST_IS_THEME_NODE (node));
  g_return_if_fail (actor_box != NULL);
  g_return_if_fail (paint_box != NULL);

  *paint_box = *actor_box;

  background_image_shadow = st_theme_node_get_background_image_shadow (node);
  if (!background_image_shadow)
    return;

  st_shadow_get_box (background_image_shadow, actor_box, &shadow_box);

  paint_box->x1 = MIN (paint_box->x1, shadow_box.x1);
  paint_box->x2 = MAX (paint_box->x2, shadow_box.x2);
  paint_box->y1 = MIN (paint_box->y1, shadow_box.y1);
  paint_box->y2 = MAX (paint_box->y2, shadow_box.y2);
}

/**
 * st_theme_node_get_paint_box:
 * @node: a #StThemeNode
 * @allocation: the box allocated to a #ClutterActor
 * @paint_box: (out caller-allocates): computed box occupied when painting the actor
 *
 * Gets the box used to paint the actor, including the area occupied
 * by properties which paint outside the actor's assigned allocation.
 * When painting @node to an offscreen buffer, this function can be
 * used to determine the necessary size of the buffer.
 */
void
st_theme_node_get_paint_box (StThemeNode           *node,
                             const ClutterActorBox *actor_box,
                             ClutterActorBox       *paint_box)
{
  StShadow *box_shadow;
  ClutterActorBox shadow_box;
  int outline_width;

  g_return_if_fail (ST_IS_THEME_NODE (node));
  g_return_if_fail (actor_box != NULL);
  g_return_if_fail (paint_box != NULL);

  box_shadow = st_theme_node_get_box_shadow (node);
  outline_width = st_theme_node_get_outline_width (node);

  st_theme_node_get_background_paint_box (node, actor_box, paint_box);

  if (!box_shadow && !outline_width)
    return;

  paint_box->x1 -= outline_width;
  paint_box->x2 += outline_width;
  paint_box->y1 -= outline_width;
  paint_box->y2 += outline_width;

  if (box_shadow)
    {
      st_shadow_get_box (box_shadow, actor_box, &shadow_box);

      paint_box->x1 = MIN (paint_box->x1, shadow_box.x1);
      paint_box->x2 = MAX (paint_box->x2, shadow_box.x2);
      paint_box->y1 = MIN (paint_box->y1, shadow_box.y1);
      paint_box->y2 = MAX (paint_box->y2, shadow_box.y2);
    }
}

/**
 * st_theme_node_geometry_equal:
 * @node: a #StThemeNode
 * @other: a different #StThemeNode
 *
 * Tests if two theme nodes have the same borders and padding; this can be
 * used to optimize having to relayout when the style applied to a Clutter
 * actor changes colors without changing the geometry.
 */
gboolean
st_theme_node_geometry_equal (StThemeNode *node,
                              StThemeNode *other)
{
  StSide side;

  g_return_val_if_fail (ST_IS_THEME_NODE (node), FALSE);

  if (node == other)
    return TRUE;

  g_return_val_if_fail (ST_IS_THEME_NODE (other), FALSE);

  _st_theme_node_ensure_computed (node);
  _st_theme_node_ensure_computed (other);

  for (side = ST_SIDE_TOP; side <= ST_SIDE_LEFT; side++)
    {
      if (node->border_width[side] != other->border_width[side])
        return FALSE;
      if (node->padding[side] != other->padding[side])
        return FALSE;
    }

  if (node->width != other->width || node->height != other->height)
    return FALSE;
  if (node->min_width != other->min_width || node->min_height != other->min_height)
    return FALSE;
  if (node->max_width != other->max_width || node->max_height != other->max_height)
    return FALSE;

  return TRUE;
}

/**
 * st_theme_node_paint_equal:
 * @node: a #StThemeNode
 * @other: a different #StThemeNode
 *
 * Check if st_theme_node_paint() will paint identically for @node as it does
 * for @other. Note that in some cases this function may return %TRUE even
 * if there is no visible difference in the painting.
 *
 * Return value: %TRUE if the two theme nodes paint identically. %FALSE if the
 *   two nodes potentially paint differently.
 */
gboolean
st_theme_node_paint_equal (StThemeNode *node,
                           StThemeNode *other)
{
  GFile *border_image, *other_border_image;
  StShadow *shadow, *other_shadow;
  int i;

  g_return_val_if_fail (ST_IS_THEME_NODE (node), FALSE);

  if (node == other)
    return TRUE;

  g_return_val_if_fail (ST_IS_THEME_NODE (other), FALSE);

  _st_theme_node_ensure_computed (node);
  _st_theme_node_ensure_computed (other);

  if (!clutter_color_equal (&node->background_color, &other->background_color))
    return FALSE;

  if (node->background_gradient_type != other->background_gradient_type)
    return FALSE;

  if (node->background_gradient_type != ST_GRADIENT_NONE &&
      !clutter_color_equal (&node->background_gradient_end, &other->background_gradient_end))
    return FALSE;

  if (g_file_equal (node->background_image, other->background_image) != 0)
    return FALSE;

  for (i = 0; i < 4; i++)
    {
      if (node->border_width[i] != other->border_width[i])
        return FALSE;

      if (node->border_width[i] > 0 &&
          !clutter_color_equal (&node->border_color[i], &other->border_color[i]))
        return FALSE;

      if (node->border_radius[i] != other->border_radius[i])
        return FALSE;
    }

  if (node->outline_width != other->outline_width)
    return FALSE;

  if (node->outline_width > 0 &&
      !clutter_color_equal (&node->outline_color, &other->outline_color))
    return FALSE;

  border_image = node->border_image_source;
  other_border_image = other->border_image_source;

  if ((border_image == NULL) != (other_border_image == NULL))
    return FALSE;

  if (border_image != NULL && !g_file_equal (border_image, other_border_image))
    return FALSE;

  for (i = 0; i < 4; i++)
    {
      if (node->border_image_slice[i] != other->border_image_slice[i])
        return FALSE;
    }

  shadow = st_theme_node_get_box_shadow (node);
  other_shadow = st_theme_node_get_box_shadow (other);

  if ((shadow == NULL) != (other_shadow == NULL))
    return FALSE;

  if (shadow != NULL && !st_shadow_equal (shadow, other_shadow))
    return FALSE;

  shadow = st_theme_node_get_background_image_shadow (node);
  other_shadow = st_theme_node_get_background_image_shadow (other);

  if ((shadow == NULL) != (other_shadow == NULL))
    return FALSE;

  if (shadow != NULL && !st_shadow_equal (shadow, other_shadow))
    return FALSE;

  return TRUE;
}

gboolean
_st_theme_node_has_id (StThemeNode *node,
                       GQuark       id)
{
  return node->element_id == id;
}

gboolean
_st_theme_node_has_class (StThemeNode *node,
                          GQuark       class)
{
  int i;

  for (i = 0; i < node->element_classes->len; i++)
    if (g_array_index (node->element_classes, GQuark, i) == class)
      return TRUE;

  return FALSE;
}

gboolean
_st_theme_node_has_pseudo (StThemeNode *node,
                           GQuark       pseudo)
{
  int i;

  for (i = 0; i < node->pseudo_classes->len; i++)
    if (g_array_index (node->pseudo_classes, GQuark, i) == pseudo)
      return TRUE;

  return FALSE;
}
