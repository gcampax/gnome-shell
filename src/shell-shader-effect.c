/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/**
 * SECTION:shell-shader-effect
 * @short_description: A shader effect
 * @see_also: #ClutterEffect, #ClutterOffscreenEffect
 *
 * #ShellShaderEffect is a sub-class of #ClutterEffect that allows applying
 * arbitrary GLSL effects to an actor.
 * It exists mainly to expose the #CoglSnippet API, which is not available
 * to introspection.
 * #ShellShaderEffect is not meant to be used directly, it should be subclassed
 * to implement add_snippets()
 *
 * #ShellShaderEffect is available since Shell 1.4
 */

#define SHELL_SHADER_EFFECT_CLASS(klass)        (G_TYPE_CHECK_CLASS_CAST ((klass), SHELL_TYPE_SHADER_EFFECT, ShellShaderEffectClass))
#define SHELL_IS_SHADER_EFFECT_CLASS(klass)     (G_TYPE_CHECK_CLASS_TYPE ((klass), SHELL_TYPE_SHADER_EFFECT))
#define SHELL_SHADER_EFFECT_GET_CLASS(obj)      (G_TYPE_INSTANCE_GET_CLASS ((obj), SHELL_TYPE_SHADER_EFFECT, ShellShaderEffectClass))

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define CLUTTER_ENABLE_EXPERIMENTAL_API
#define COGL_ENABLE_EXPERIMENTAL_API

#include <cogl/cogl.h>
#include "shell-shader-effect.h"

struct _ShellShaderEffectPrivate
{
  /* a back pointer to our actor, so that we can query it */
  ClutterActor *actor;

  gint tex_width;
  gint tex_height;

  CoglPipeline *pipeline;
};

G_DEFINE_ABSTRACT_TYPE (ShellShaderEffect, shell_shader_effect,
                        CLUTTER_TYPE_OFFSCREEN_EFFECT);

static gboolean
shell_shader_effect_pre_paint (ClutterEffect *effect)
{
  ShellShaderEffect *self = SHELL_SHADER_EFFECT (effect);
  ShellShaderEffectPrivate *priv;
  ClutterEffectClass *parent_class;

  priv = self->priv;

  if (!clutter_actor_meta_get_enabled (CLUTTER_ACTOR_META (effect)))
    return FALSE;

  priv->actor = clutter_actor_meta_get_actor (CLUTTER_ACTOR_META (effect));
  if (priv->actor == NULL)
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

  parent_class = CLUTTER_EFFECT_CLASS (shell_shader_effect_parent_class);
  if (parent_class->pre_paint (effect))
    {
      ClutterOffscreenEffect *offscreen_effect =
        CLUTTER_OFFSCREEN_EFFECT (effect);
      CoglHandle texture;

      texture = clutter_offscreen_effect_get_texture (offscreen_effect);
      priv->tex_width = cogl_texture_get_width (texture);
      priv->tex_height = cogl_texture_get_height (texture);

      cogl_pipeline_set_layer_texture (priv->pipeline, 0, texture);

      return TRUE;
    }
  else
    return FALSE;
}

static void
shell_shader_effect_paint_target (ClutterOffscreenEffect *effect)
{
  ShellShaderEffect *self = SHELL_SHADER_EFFECT (effect);
  ShellShaderEffectPrivate *priv;
  guint8 paint_opacity;

  priv = self->priv;

  paint_opacity = clutter_actor_get_paint_opacity (priv->actor);

  cogl_pipeline_set_color4ub (priv->pipeline,
                              paint_opacity,
                              paint_opacity,
                              paint_opacity,
                              paint_opacity);
  cogl_push_source (priv->pipeline);

  cogl_rectangle (0, 0, priv->tex_width, priv->tex_height);

  cogl_pop_source ();
}

static void
shell_shader_effect_dispose (GObject *gobject)
{
  ShellShaderEffect *self = SHELL_SHADER_EFFECT (gobject);
  ShellShaderEffectPrivate *priv;

  priv = self->priv;

  g_clear_pointer (&priv->pipeline, cogl_object_unref);

  G_OBJECT_CLASS (shell_shader_effect_parent_class)->dispose (gobject);
}

static void
shell_shader_effect_init (ShellShaderEffect *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, SHELL_TYPE_SHADER_EFFECT, ShellShaderEffectPrivate);
}

static void
shell_shader_effect_constructed (GObject *object)
{
  ShellShaderEffect *self;
  ShellShaderEffectClass *klass;

  G_OBJECT_CLASS (shell_shader_effect_parent_class)->constructed (object);

  /* Note that, differently from ClutterBlurEffect, we are calling
     this inside constructed, not init, so klass points to the most-derived
     GTypeClass, not ShellShaderEffectClass.
  */
  klass = SHELL_SHADER_EFFECT_GET_CLASS (object);
  self = SHELL_SHADER_EFFECT (object);

  if (G_UNLIKELY (klass->base_pipeline == NULL))
    {
      CoglContext *ctx =
        clutter_backend_get_cogl_context (clutter_get_default_backend ());

      klass->base_pipeline = cogl_pipeline_new (ctx);
      cogl_pipeline_set_layer_null_texture (klass->base_pipeline,
                                            0, /* layer number */
                                            COGL_TEXTURE_TYPE_2D);

      if (klass->build_pipeline != NULL)
        klass->build_pipeline (self);
    }

  self->priv->pipeline = cogl_pipeline_copy (klass->base_pipeline);
}

static void
shell_shader_effect_class_init (ShellShaderEffectClass *klass)
{
  ClutterEffectClass *effect_class = CLUTTER_EFFECT_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterOffscreenEffectClass *offscreen_class;

  gobject_class->constructed = shell_shader_effect_constructed;
  gobject_class->dispose = shell_shader_effect_dispose;

  effect_class->pre_paint = shell_shader_effect_pre_paint;

  offscreen_class = CLUTTER_OFFSCREEN_EFFECT_CLASS (klass);
  offscreen_class->paint_target = shell_shader_effect_paint_target;

  g_type_class_add_private (klass, sizeof (ShellShaderEffectPrivate));
}

/**
 * shell_shader_effect_add_glsl_snippet:
 * @effect: a #ShellShaderEffect
 * @hook: where to insert the code
 * @declarations: GLSL declarations
 * @code: GLSL code
 * @is_replace: wheter Cogl code should be replaced by the custom shader
 *
 * Adds a GLSL snippet to the pipeline used for drawing the actor texture.
 * See #CoglSnippet for details.
 *
 * This is only valid inside the a call to the build_pipeline() virtual
 * function.
 */
void
shell_shader_effect_add_glsl_snippet (ShellShaderEffect *effect,
                                      ShellSnippetHook    hook,
                                      const char        *declarations,
                                      const char        *code,
                                      gboolean           is_replace)
{
  ShellShaderEffectClass *klass = SHELL_SHADER_EFFECT_GET_CLASS (effect);
  CoglSnippet *snippet;

  g_return_if_fail (klass->base_pipeline != NULL);

  if (is_replace)
    {
      snippet = cogl_snippet_new (hook, declarations, NULL);
      cogl_snippet_set_replace (snippet, code);
    }
  else
    {
      snippet = cogl_snippet_new (hook, declarations, code);
    }

  if (hook == SHELL_SNIPPET_HOOK_VERTEX ||
      hook == SHELL_SNIPPET_HOOK_FRAGMENT)
    cogl_pipeline_add_snippet (klass->base_pipeline, snippet);
  else
    cogl_pipeline_add_layer_snippet (klass->base_pipeline, 0, snippet);

  cogl_object_unref (snippet);
}

/**
 * shell_shader_effect_new:
 *
 * Creates a new #ShellShaderEffect to be used with
 * clutter_actor_add_effect()
 *
 * Return value: the newly created #ShellShaderEffect or %NULL
 */
ClutterEffect *
shell_shader_effect_new (void)
{
  return g_object_new (SHELL_TYPE_SHADER_EFFECT, NULL);
}
