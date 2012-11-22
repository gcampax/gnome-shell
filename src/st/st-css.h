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

#ifndef __ST_CSS__
#define __ST_CSS__

#include <glib.h>
#include <gio/gio.h>
#include "st-theme-node.h"

/**
 * SECTION:st-cascade
 * @short_description: a set of stylesheets
 *
 * #StCascade holds a set of stylesheets. (The "cascade" of the name
 * Cascading Stylesheets.). The lifetime of the cascade is managed
 * by #StThemeContext and retrieved via st_theme_context_get_cascade()
 */

typedef struct _StCascade      StCascade;
typedef struct _StCascadeClass StCascadeClass;

#define ST_TYPE_CASCADE              (st_cascade_get_type ())
#define ST_CASCADE(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), ST_TYPE_CASCADE, StCascade))
#define ST_CASCADE_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), ST_TYPE_CASCADE, StCascadeClass))
#define ST_IS_CASCADE(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), ST_TYPE_CASCADE))
#define ST_IS_CASCADE_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), ST_TYPE_CASCADE))
#define ST_CASCADE_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), ST_TYPE_CASCADE, StCascadeClass))

GType  st_cascade_get_type (void) G_GNUC_CONST;

/* The CSS cascade does not make sense in the context of gnome-shell,
   so we make up the set of origins that are useful.
*/
typedef enum {
  ST_ORIGIN_THEME,
  ST_ORIGIN_EXTENSION,
  ST_ORIGIN_THEME_IMPORTANT,
  ST_ORIGIN_EXTENSION_IMPORTANT
} StCascadeOrigin;

gboolean   st_cascade_load_theme        (StCascade        *cascade,
                                         GFile            *file,
                                         GError          **error);
gboolean   st_cascade_insert_stylesheet (StCascade        *cascade,
                                         GFile            *file,
                                         StCascadeOrigin   origin,
                                         GError          **error);
void       st_cascade_remove_stylesheet (StCascade        *cascade,
                                         GFile            *file);

#endif
