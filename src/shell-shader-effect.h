/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#ifndef __SHELL_SHADER_EFFECT_H__
#define __SHELL_SHADER_EFFECT_H__

#include <clutter/clutter.h>
#include <meta/meta-background-actor.h>

G_BEGIN_DECLS

/**
 * ShellSnippetHook:
 * Temporary hack to work around Cogl not exporting CoglSnippetHook in
 * the 1.0 API. Don't use.
 */
typedef enum {
  /* Per pipeline vertex hooks */
  SHELL_SNIPPET_HOOK_VERTEX = 0,
  SHELL_SNIPPET_HOOK_VERTEX_TRANSFORM,

  /* Per pipeline fragment hooks */
  SHELL_SNIPPET_HOOK_FRAGMENT = 2048,

  /* Per layer vertex hooks */
  SHELL_SNIPPET_HOOK_TEXTURE_COORD_TRANSFORM = 4096,

  /* Per layer fragment hooks */
  SHELL_SNIPPET_HOOK_LAYER_FRAGMENT = 6144,
  SHELL_SNIPPET_HOOK_TEXTURE_LOOKUP
} ShellSnippetHook;


#define SHELL_TYPE_SHADER_EFFECT        (shell_shader_effect_get_type ())
#define SHELL_SHADER_EFFECT(obj)        (G_TYPE_CHECK_INSTANCE_CAST ((obj), SHELL_TYPE_SHADER_EFFECT, ShellShaderEffect))
#define SHELL_IS_SHADER_EFFECT(obj)     (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SHELL_TYPE_SHADER_EFFECT))

/**
 * ShellShaderEffect:
 *
 * <structname>ShellShaderEffect</structname> is an opaque structure
 * whose members cannot be accessed directly
 */
typedef struct _ShellShaderEffect        ShellShaderEffect;
typedef struct _ShellShaderEffectPrivate ShellShaderEffectPrivate;
typedef struct _ShellShaderEffectClass   ShellShaderEffectClass;

struct _ShellShaderEffect {
  ClutterOffscreenEffect parent;

  ShellShaderEffectPrivate *priv;
};

struct _ShellShaderEffectClass {
  ClutterOffscreenEffectClass parent_class;

  CoglPipeline *base_pipeline;

  void (*build_pipeline) (ShellShaderEffect *effect);
};

GType shell_shader_effect_get_type (void) G_GNUC_CONST;

ClutterEffect *shell_shader_effect_new (void);

void shell_shader_effect_add_glsl_snippet (ShellShaderEffect *effect,
                                           ShellSnippetHook    hook,
                                           const char        *declarations,
                                           const char        *code,
                                           gboolean           is_replace);


G_END_DECLS

#endif /* __SHELL_SHADER_EFFECT_H__ */
