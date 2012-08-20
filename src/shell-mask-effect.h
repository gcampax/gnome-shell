/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * shell-mask-effect.h: a effect to show through a Cairo path
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
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#ifndef _SHELL_ANIMATED_MASK
#define _SHELL_ANIMATED_MASK

#include <glib-object.h>
#include <gio/gio.h>
#include <cairo/cairo.h>

G_BEGIN_DECLS

#define SHELL_TYPE_MASK_EFFECT shell_mask_effect_get_type()

#define SHELL_MASK_EFFECT(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), SHELL_TYPE_MASK_EFFECT, ShellMaskEffect))

#define SHELL_MASK_EFFECT_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), SHELL_TYPE_MASK_EFFECT, ShellMaskEffectClass))

#define SHELL_IS_MASK_EFFECT(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SHELL_TYPE_MASK_EFFECT))

#define SHELL_IS_MASK_EFFECT_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), SHELL_TYPE_MASK_EFFECT))

#define SHELL_MASK_EFFECT_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), SHELL_TYPE_MASK_EFFECT, ShellMaskEffectClass))

typedef struct _ShellMaskEffect       ShellMaskEffect;
typedef struct _ShellMaskEffectClass  ShellMaskEffectClass;

GType shell_mask_effect_get_type (void);

ClutterEffect *shell_mask_effect_new (void);

void          shell_mask_effect_set_vertical_offset  (ShellMaskEffect *mask,
                                                      gfloat           offset);
gfloat        shell_mask_effect_get_vertical_offset  (ShellMaskEffect *mask);

G_END_DECLS

#endif /* _SHELL_MASK_EFFECT */

