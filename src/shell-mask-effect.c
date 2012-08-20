/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * shell-mask-effect.c: a effect to show through a Cairo path
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
 */

#define CLUTTER_ENABLE_EXPERIMENTAL_API
#define COGL_ENABLE_EXPERIMENTAL_API

#include <math.h>

#include <clutter/clutter.h>
#include <cogl/cogl.h>
#include <cairo/cairo.h>
#include <cairo/cairo-gobject.h>

#include "shell-mask-effect.h"

static const gchar *mask_glsl_declarations =
"uniform vec2 offset;\n";
static const gchar *mask_glsl_shader =
"  vec4 real_texel = texture2D (cogl_sampler0, cogl_tex_coord.st + offset);\n"
"  vec4 mask_texel = texture2D (cogl_sampler, cogl_tex_coord.st);\n"
"  if (mask_texel.a > 0.0f) {\n"
"    cogl_texel = real_texel;\n"
"  } else \n"
"    cogl_texel = vec4(0);\n";

struct _ShellMaskEffect
{
  ClutterOffscreenEffect parent_instance;

  /* a back pointer to our actor, so that we can query it */
  ClutterActor *actor;

  gint   offset_uniform;
  gfloat vertical_offset;

  gint tex_width;
  gint tex_height;

  CoglPipeline *pipeline;

  CoglTexture  *mask_texture;
};

struct _ShellMaskEffectClass
{
  ClutterOffscreenEffectClass parent_class;

  CoglPipeline *base_pipeline;
};

G_DEFINE_TYPE (ShellMaskEffect,
               shell_mask_effect,
               CLUTTER_TYPE_OFFSCREEN_EFFECT);

enum {
  SIGNAL_BUILD_MASK,
  SIGNAL_LAST
};

enum {
  PROP_0,
  PROP_VERTICAL_OFFSET,
  PROP_LAST
};

static gint signals[SIGNAL_LAST];
static GParamSpec *properties[PROP_LAST];

static gboolean
ensure_mask_texture (ShellMaskEffect *effect)
{
  cairo_surface_t *surface;
  cairo_t *context;
  gboolean handled;

  if (effect->mask_texture &&
      effect->tex_width == cogl_texture_get_width (effect->mask_texture) &&
      effect->tex_height == cogl_texture_get_height (effect->mask_texture))
    return TRUE;

  if (effect->mask_texture)
    {
      cogl_object_unref (effect->mask_texture);
      effect->mask_texture = NULL;
    }

  surface = cairo_image_surface_create (CAIRO_FORMAT_A8,
                                        effect->tex_width, effect->tex_height);
  context = cairo_create (surface);
  cairo_set_source_rgba (context, 0, 0, 0, 1);

  g_signal_emit (effect, signals[SIGNAL_BUILD_MASK], 0, context, &handled);

  if (!handled)
    cairo_fill (context);
  cairo_surface_flush (surface);

  effect->mask_texture = cogl_texture_new_from_data (effect->tex_width,
                                                     effect->tex_height,
                                                     COGL_TEXTURE_NONE,
                                                     COGL_PIXEL_FORMAT_A_8,
                                                     COGL_PIXEL_FORMAT_A_8,
                                                     cairo_image_surface_get_stride (surface),
                                                     cairo_image_surface_get_data (surface));

  cairo_destroy (context);
  cairo_surface_destroy (surface);

  return effect->mask_texture != NULL;
}

static void
update_offset_uniform (ShellMaskEffect *effect)
{
  if (effect->offset_uniform > -1)
    {
      gfloat offset[2] = { 0, effect->vertical_offset };

      cogl_pipeline_set_uniform_float (effect->pipeline,
                                       effect->offset_uniform,
                                       2, /* n_components */
                                       1, /* count */
                                       offset);
    }
}

static gboolean
shell_mask_effect_pre_paint (ClutterEffect *effect)
{
  ShellMaskEffect *self = SHELL_MASK_EFFECT (effect);
  ClutterEffectClass *parent_class;

  if (!clutter_actor_meta_get_enabled (CLUTTER_ACTOR_META (effect)))
    return FALSE;

  self->actor = clutter_actor_meta_get_actor (CLUTTER_ACTOR_META (effect));
  if (self->actor == NULL)
    return FALSE;

  if (!clutter_feature_available (CLUTTER_FEATURE_SHADERS_GLSL))
    {
      /* if we don't have support for GLSL shaders then we
       * forcibly disable the ActorMeta
       */
      g_warning ("Unable to use the ShaderEffect: the graphics hardware "
                 "or the current GL driver does not implement support "
                 "for the GLSL shading language.");
      clutter_actor_meta_set_enabled (CLUTTER_ACTOR_META (effect), FALSE);
      return FALSE;
    }

  parent_class = CLUTTER_EFFECT_CLASS (shell_mask_effect_parent_class);
  if (parent_class->pre_paint (effect))
    {
      ClutterOffscreenEffect *offscreen_effect =
        CLUTTER_OFFSCREEN_EFFECT (effect);
      CoglHandle texture;

      texture = clutter_offscreen_effect_get_texture (offscreen_effect);
      self->tex_width = cogl_texture_get_width (texture);
      self->tex_height = cogl_texture_get_height (texture);

      if (!ensure_mask_texture (self))
        return FALSE;

      update_offset_uniform (self);

      cogl_pipeline_set_layer_texture (self->pipeline, 0, texture);
      cogl_pipeline_set_layer_texture (self->pipeline, 1, self->mask_texture);

      return TRUE;
    }
  else
    return FALSE;
}

static void
shell_mask_effect_paint_target (ClutterOffscreenEffect *effect)
{
  ShellMaskEffect *self = SHELL_MASK_EFFECT (effect);
  guint8 paint_opacity;

  paint_opacity = clutter_actor_get_paint_opacity (self->actor);

  cogl_pipeline_set_color4ub (self->pipeline,
                              paint_opacity,
                              paint_opacity,
                              paint_opacity,
                              paint_opacity);
  cogl_push_source (self->pipeline);

  cogl_rectangle (0, 0, self->tex_width, self->tex_height);

  cogl_pop_source ();
}

static void
shell_mask_effect_dispose (GObject *gobject)
{
  ShellMaskEffect *self = SHELL_MASK_EFFECT (gobject);

  g_clear_pointer (&self->pipeline, cogl_object_unref);
  g_clear_pointer (&self->mask_texture, cogl_object_unref);

  G_OBJECT_CLASS (shell_mask_effect_parent_class)->dispose (gobject);
}

static void
shell_mask_effect_set_property (GObject      *gobject,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  ShellMaskEffect *effect = SHELL_MASK_EFFECT (gobject);

  switch (prop_id)
    {
    case PROP_VERTICAL_OFFSET:
      shell_mask_effect_set_vertical_offset (effect,
                                             g_value_get_float (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
shell_mask_effect_get_property (GObject    *gobject,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  ShellMaskEffect *effect = SHELL_MASK_EFFECT (gobject);

  switch (prop_id)
    {
    case PROP_VERTICAL_OFFSET:
      g_value_set_float (value, effect->vertical_offset);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}


static void
shell_mask_effect_class_init (ShellMaskEffectClass *klass)
{
  ClutterEffectClass *effect_class = CLUTTER_EFFECT_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterOffscreenEffectClass *offscreen_class;

  gobject_class->dispose = shell_mask_effect_dispose;
  gobject_class->set_property = shell_mask_effect_set_property;
  gobject_class->get_property = shell_mask_effect_get_property;

  effect_class->pre_paint = shell_mask_effect_pre_paint;

  offscreen_class = CLUTTER_OFFSCREEN_EFFECT_CLASS (klass);
  offscreen_class->paint_target = shell_mask_effect_paint_target;

  signals[SIGNAL_BUILD_MASK] = g_signal_new ("build-mask",
                                             G_TYPE_FROM_CLASS (klass),
                                             G_SIGNAL_RUN_LAST,
                                             0, /* class offset */
                                             g_signal_accumulator_true_handled,
                                             NULL,
                                             NULL, /* c marshaller */
                                             G_TYPE_BOOLEAN,
                                             1, CAIRO_GOBJECT_TYPE_CONTEXT);

  properties[PROP_VERTICAL_OFFSET] = g_param_spec_float ("vertical-offset",
                                                         "", "",
                                                         -G_MAXFLOAT, G_MAXFLOAT, 0,
                                                         G_PARAM_READABLE | G_PARAM_WRITABLE);
  g_object_class_install_properties (gobject_class, PROP_LAST, properties);
}

static void
shell_mask_effect_init (ShellMaskEffect *self)
{
  ShellMaskEffectClass *klass = SHELL_MASK_EFFECT_GET_CLASS (self);

  if (G_UNLIKELY (klass->base_pipeline == NULL))
    {
      CoglSnippet *snippet;
      CoglContext *ctx =
        clutter_backend_get_cogl_context (clutter_get_default_backend ());

      klass->base_pipeline = cogl_pipeline_new (ctx);

      snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_TEXTURE_LOOKUP,
                                  mask_glsl_declarations,
                                  NULL);
      cogl_snippet_set_replace (snippet, mask_glsl_shader);
      cogl_pipeline_add_layer_snippet (klass->base_pipeline, 1, snippet);
      cogl_object_unref (snippet);

      cogl_pipeline_set_layer_null_texture (klass->base_pipeline,
                                            0, /* layer number */
                                            COGL_TEXTURE_TYPE_2D);
      cogl_pipeline_set_layer_null_texture (klass->base_pipeline,
                                            1, /* layer number */
                                            COGL_TEXTURE_TYPE_2D);
    }

  self->pipeline = cogl_pipeline_copy (klass->base_pipeline);

  self->offset_uniform =
    cogl_pipeline_get_uniform_location (self->pipeline, "offset");
}

/**
 * shell_mask_effect_new:
 *
 * Creates a new #ShellMaskEffect to be used with
 * clutter_actor_add_effect()
 *
 * Return value: the newly created #ShellMaskEffect or %NULL
 *
 * Since: 1.4
 */
ClutterEffect *
shell_mask_effect_new (void)
{
  return g_object_new (SHELL_TYPE_MASK_EFFECT, NULL);

}
  
void
shell_mask_effect_set_vertical_offset (ShellMaskEffect *effect,
                                       gfloat           offset)
{
  g_return_if_fail (SHELL_IS_MASK_EFFECT (effect));

  if (fabsf (effect->vertical_offset - offset) >= 0.00001)
    {
      effect->vertical_offset = offset;
      update_offset_uniform (effect);

      clutter_effect_queue_repaint (CLUTTER_EFFECT (effect));

      g_object_notify_by_pspec (G_OBJECT (effect), properties[PROP_VERTICAL_OFFSET]);
    }
}

gfloat
shell_mask_effect_get_vertical_offset (ShellMaskEffect *effect)
{
  g_return_val_if_fail (SHELL_IS_MASK_EFFECT (effect), 0.0);

  return effect->vertical_offset;
}
