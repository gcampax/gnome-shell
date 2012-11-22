/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * st-theme-node-private.h: private structures and functions for StThemeNode
 *
 * Copyright 2009, 2010 Red Hat, Inc.
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

#ifndef __ST_THEME_NODE_PRIVATE_H__
#define __ST_THEME_NODE_PRIVATE_H__

#include <gdk/gdk.h>

#include "st-theme-node.h"
#include "st-types.h"
#include "st-css-private.h"

G_BEGIN_DECLS

struct _StThemeNode {
  GObject parent;

  StThemeContext *context;
  StThemeNode *parent_node;
  StCascade *cascade;

  PangoFontDescription *font_desc;

  ClutterColor background_color;
  /* If gradient is set, then background_color is the gradient start */
  StGradientType background_gradient_type;
  ClutterColor background_gradient_end;

  float background_position_x;
  float background_position_y;

  StBackgroundSize background_size;
  float background_size_w;
  float background_size_h;

  ClutterColor foreground_color;
  ClutterColor border_color[4];
  ClutterColor outline_color;
  ClutterColor warning_color;
  ClutterColor error_color;
  ClutterColor success_color;

  float border_width[4];
  float border_radius[4];
  float outline_width;
  float padding[4];

  float width;
  float height;
  float min_width;
  float min_height;
  float max_width;
  float max_height;

  int transition_duration;

  GFile  *background_image;
  GFile  *border_image_source;
  float   border_image_slice[4];

  StShadow *box_shadow;
  StShadow *background_image_shadow;
  StShadow *text_shadow;
  StIconColors *icon_colors;

  StTextDecoration text_decoration;
  StTextAlign text_align;

  GType   element_type;
  GQuark  element_id;
  GArray *element_classes;
  GArray *pseudo_classes;
  char *inline_style;

  GHashTable *custom_properties;

  /* We hold onto these separately so we can destroy them on finalize */
  CRDeclaration *inline_properties;

  guint background_position_set : 1;
  guint background_repeat : 1;

  guint properties_computed : 1;
  guint geometry_computed : 1;
  guint background_computed : 1;
  guint border_image_computed : 1;
  guint box_shadow_computed : 1;
  guint background_image_shadow_computed : 1;
  guint text_shadow_computed : 1;
  guint link_type : 2;

  /* Graphics state */
  float alloc_width;
  float alloc_height;

  CoglHandle background_shadow_material;
  CoglHandle box_shadow_material;
  CoglHandle background_texture;
  CoglHandle background_material;
  CoglHandle border_slices_texture;
  CoglHandle border_slices_material;
  CoglHandle prerendered_texture;
  CoglHandle prerendered_material;
  CoglHandle corner_material[4];
};

struct _StThemeNodeClass {
  GObjectClass parent_class;

};

StThemeNode *st_theme_node_new (StThemeContext *context,
                                StThemeNode    *parent_node,
                                GType           element_type,
                                const char     *element_id,
                                GArray         *element_classes,
                                GArray         *element_pseudo_classes,
                                const char     *inline_style);

void _st_theme_node_ensure_computed (StThemeNode *node);

void _st_theme_node_init_drawing_state (StThemeNode *node);
void _st_theme_node_free_drawing_state (StThemeNode *node);

gboolean _st_theme_node_has_id     (StThemeNode *node, GQuark id);
gboolean _st_theme_node_has_class  (StThemeNode *node, GQuark class);
gboolean _st_theme_node_has_pseudo (StThemeNode *node, GQuark pseudo);

G_END_DECLS

#endif /* __ST_THEME_NODE_PRIVATE_H__ */
