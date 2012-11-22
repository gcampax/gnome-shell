/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * st-css.c: A layer of caching above (ugh) libcroco
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

/* Note: all memory management is done by libcroco, so we don't need to
   refcount or even deep-copy these structures
*/

#include <glib.h>

#include "st-css-private.h"
#include "st-theme-node-private.h"

struct _StCascade {
  GObject parent;

  GPtrArray *stylesheets;
};

struct _StCascadeClass {
  GObjectClass parent_class;
};

enum {
  SELECTOR_ID,
  SELECTOR_CLASS,
  SELECTOR_PSEUDO,
};

typedef struct {
  int    kind;
  GQuark value;
} StAdditionalSelector;

/* '+' is not supported by St */
enum {
  COMBINATOR_SPACE,
  COMBINATOR_GREATER,
};

/* Simple selectors (i.e. combinations of id, class and pseudo-class that all
   match the same element) are stored in a singly linked list, starting from the
   inner most. This allows us to bail out fast if an element does not match.
   The combinator refers to the relationship between this simple selector
   and its parent.
*/
typedef struct _StSimpleSelector StSimpleSelector;

struct _StSimpleSelector {
  GType type;
  GArray *additionals;
  int combinator;
  StSimpleSelector *prev;
};

struct _StStatement {
  CRRuleSet        *cr_ruleset;
  StSimpleSelector *simple_chain;
  GPtrArray        *properties;
};

G_DEFINE_TYPE (StCascade, st_cascade, G_TYPE_OBJECT);

static void          st_stylesheet_free   (StStylesheet *stylesheet);

static StStatement * st_statement_new     (CRStatement *statement);
static void          st_statement_free    (StStatement *statement);

static int           st_property_compare  (StProperty **one, StProperty **two);

static inline void
make_gerror_from_croco (enum CRStatus   status,
                        GError        **error)
{
  /* TODO: a better one */
  g_set_error (error, 0, 0, "Error parsing stylesheet");
}

static inline char *
cr_to_string (CRString *string)
{
  return string->stryng->str;
}

static CRStyleSheet *
st_load_stylesheet_from_file (GFile   *file,
                              GError **error)
{
  CRStyleSheet *parsed;
  enum CRStatus ok;
  char *contents;
  gsize size;

  if (!g_file_load_contents (file,
                             NULL, /* cancellable */
                             &contents,
                             &size,
                             NULL,
                             error))
    return NULL;

  ok = cr_om_parser_simply_parse_buf ((guchar*)contents,
                                      size,
                                      CR_UTF_8,
                                      &parsed);
  g_free (contents);

  if (ok != CR_OK)
    {
      make_gerror_from_croco (ok, error);
      return NULL;
    }

  return parsed;
}

GFile *
st_resolve_relative_url (GFile      *parent_file,
                         const char *url)
{
  char *scheme;
  GFile *resource;

  if ((scheme = g_uri_parse_scheme (url)))
    {
      g_free (scheme);
      resource = g_file_new_for_uri (url);
    }
  else if (parent_file != NULL)
    {
      GFile *directory;

      directory = g_file_get_parent (parent_file);
      resource = g_file_resolve_relative_path (directory, url);

      g_object_unref (directory);
    }
  else
    {
      resource = g_file_new_for_path (url);
    }

  return resource;
}

static gboolean
st_stylesheet_precache (StStylesheet  *self,
                        GFile         *parent_file,
                        CRStyleSheet  *stylesheet,
                        GError       **error)
{
  CRStatement *statement;

  for (statement = stylesheet->statements; statement; statement = statement->next)
    {
      if (G_UNLIKELY (statement->type != RULESET_STMT &&
                      statement->type != AT_IMPORT_RULE_STMT))
        {
          /* FIXME: */
          g_set_error (error, 0, 0, "Error parsing stylesheet: unsupported statement");
          return FALSE;
        }

      if (statement->type == AT_IMPORT_RULE_STMT)
        {
          CRAtImportRule *at_import = statement->kind.import_rule;
          const char *url;
          GFile *file;
          CRStyleSheet *child;

          url = cr_to_string (at_import->url);
          file = st_resolve_relative_url (parent_file, url);

          child = st_load_stylesheet_from_file (file, error);
          g_object_unref (file);

          if (child == NULL)
            return FALSE;

          child->app_data = self;
          g_ptr_array_add (self->stylesheets, child);
          if (!st_stylesheet_precache (self, file, child, error))
            return FALSE;
        }
      else
        {
          StStatement *st_statement;

          st_statement = st_statement_new (statement);
          if (G_UNLIKELY (st_statement == NULL))
            {
              /* FIXME: */
              g_set_error (error, 0, 0, "Error parsing stylesheet: unsupported property value");
              return FALSE;
            }

          g_ptr_array_add (self->statements, st_statement);
        }
    }

  return TRUE;
}

static void
cr_stylesheet_free (gpointer sheet)
{
  /* How can you return a boolean from a destroy notify,
     seriously? */
  cr_stylesheet_unref (sheet);
}

static StStylesheet *
st_stylesheet_new (GFile   *file,
                   guint    origin,
                   GError **error)
{
  StStylesheet *self;
  CRStyleSheet *parsed;

  parsed = st_load_stylesheet_from_file (file, error);
  if (parsed == NULL)
    return NULL;

  self = g_slice_new (StStylesheet);
  self->stylesheets = g_ptr_array_new_with_free_func (cr_stylesheet_free);
  self->statements = g_ptr_array_new_with_free_func ((GDestroyNotify)st_statement_free);
  self->file = g_object_ref (file);
  self->origin = origin;

  parsed->app_data = self;
  g_ptr_array_add (self->stylesheets, parsed);
  if (G_UNLIKELY (!st_stylesheet_precache (self, file, parsed, error)))
    {
      st_stylesheet_free (self);
      return NULL;
    }

  return self;
}

static void
st_stylesheet_free (StStylesheet *self)
{
  g_ptr_array_unref (self->stylesheets);
  g_ptr_array_unref (self->statements);
  g_object_unref (self->file);

  g_slice_free (StStylesheet, self);
}

void
st_cascade_init (StCascade *self)
{
  self->stylesheets = g_ptr_array_new_with_free_func ((GDestroyNotify)st_stylesheet_free);

  /* make space for the theme, in position 0 */
  g_ptr_array_add (self->stylesheets, NULL);
}

static void
st_cascade_finalize (GObject *object)
{
  StCascade *self = ST_CASCADE (object);

  g_ptr_array_unref (self->stylesheets);
}

static void
st_cascade_class_init (StCascadeClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = st_cascade_finalize;
}

gboolean
st_cascade_load_theme (StCascade  *cascade,
                       GFile      *file,
                       GError    **error)
{
  StStylesheet *sheet;

  sheet = st_stylesheet_new (file, ST_ORIGIN_THEME, error);
  if (G_UNLIKELY (sheet == NULL))
    return FALSE;

  if (cascade->stylesheets->pdata[0])
    st_stylesheet_free (cascade->stylesheets->pdata[0]);

  cascade->stylesheets->pdata[0] = sheet;

  return TRUE;
}

gboolean
st_cascade_insert_stylesheet (StCascade  *cascade,
                              GFile      *file,
                              guint       origin,
                              GError    **error)
{
  StStylesheet *sheet;

  sheet = st_stylesheet_new (file, origin, error);
  if (G_UNLIKELY (sheet))
    return FALSE;

  g_ptr_array_add (cascade->stylesheets, sheet);

  return TRUE;
}

void
st_cascade_remove_stylesheet (StCascade *cascade,
                              GFile     *file)
{
  StStylesheet *sheet;
  int i;

  for (i = 0; i < cascade->stylesheets->len; i++)
    {
      sheet = cascade->stylesheets->pdata[i];

      if (g_file_equal (sheet->file, file))
        {
          g_ptr_array_remove_index_fast (cascade->stylesheets, i);
          return;
        }
    }
}

StCascade *
st_cascade_new (void)
{
  return g_object_new (ST_TYPE_CASCADE, NULL);
}

static gboolean
st_precache_declaration_list (GPtrArray     *properties,
                              CRDeclaration *declaration)
{
  for ( ; declaration; declaration = declaration->next)
    {
      if (!st_property_parse (declaration, properties))
          return FALSE;
    }

  return TRUE;
}

static StStatement *
st_statement_new (CRStatement *statement)
{
  StStatement *self;
  CRSelector *cr_selector;
  StSimpleSelector *simple_sel, *next_simple_sel;

  g_assert (statement->type == RULESET_STMT);

  self = g_slice_new (StStatement);
  self->cr_ruleset = statement->kind.ruleset;
  self->properties = g_ptr_array_new_with_free_func ((GDestroyNotify) st_property_unref);

  /* Find the last selector and build a chain of StSimpleSelectors */
  for (cr_selector = self->cr_ruleset->sel_list;
       cr_selector && cr_selector->next;
       cr_selector = cr_selector->next)
    ;

  next_simple_sel = NULL;
  for ( ; cr_selector; cr_selector = cr_selector->prev)
    {
      CRSimpleSel *cr_simple_sel;
      CRAdditionalSel *cr_additional_sel;

      cr_simple_sel = cr_selector->simple_sel;

      simple_sel = g_slice_new (StSimpleSelector);
      simple_sel->additionals = g_array_new (FALSE, FALSE, sizeof (StAdditionalSelector));
      if (cr_simple_sel->type_mask == TYPE_SELECTOR)
        simple_sel->type = g_type_from_name (cr_to_string (cr_simple_sel->name));
      else
        simple_sel->type = G_TYPE_INVALID;

      /* The combinator in cr_simple_sel is left-to-right, but our
         linked list is right-to-left, so we need to fetch another
         previous, and get the combinator from there */
      if (cr_selector->prev)
        {
          if (cr_selector->prev->simple_sel->combinator == COMB_GT)
            simple_sel->combinator = COMBINATOR_GREATER;
          else
            simple_sel->combinator = COMBINATOR_SPACE;
        }

      for (cr_additional_sel = cr_simple_sel->add_sel;
           cr_additional_sel; cr_additional_sel = cr_additional_sel->next)
        {
          StAdditionalSelector additional_sel;

          switch (cr_additional_sel->type)
            {
            case NO_ADD_SELECTOR:
              continue;

            case CLASS_ADD_SELECTOR:
              additional_sel.kind = SELECTOR_CLASS;
              additional_sel.value = g_quark_from_string (cr_to_string (cr_additional_sel->content.class_name));
              break;

            case ID_ADD_SELECTOR:
              additional_sel.kind = SELECTOR_ID;
              additional_sel.value = g_quark_from_string (cr_to_string (cr_additional_sel->content.id_name));
              break;

            case PSEUDO_CLASS_ADD_SELECTOR:
              additional_sel.kind = SELECTOR_PSEUDO;
              additional_sel.value = g_quark_from_string (cr_to_string (cr_additional_sel->content.pseudo->name));
              break;

            default:
              g_warn_if_reached ();
            }

          g_array_append_val (simple_sel->additionals, additional_sel);
        }

      if (next_simple_sel != NULL)
        next_simple_sel->prev = simple_sel;
      else
        self->simple_chain = simple_sel;

      next_simple_sel = simple_sel;
    }

  if (!st_precache_declaration_list (self->properties, self->cr_ruleset->decl_list))
    {
      st_statement_free (self);
      return NULL;
    }

  return self;
}

static void
st_statement_free (StStatement *self)
{
  StSimpleSelector *simple, *previous;

  for (simple = self->simple_chain; simple; simple = previous)
    {
      previous = simple->prev;

      g_array_unref (simple->additionals);
      g_slice_free (StSimpleSelector, simple);
    }

  g_ptr_array_unref (self->properties);
  g_slice_free (StStatement, self);
}

/* Order of comparison is so that higher priority statements compare after
 * lower priority statements */
static int
st_property_compare (StProperty **one,
                     StProperty **two)
{
  return (*one)->sort_key - (*two)->sort_key;
}

static gboolean
st_simple_selector_matches (StSimpleSelector *simple,
                            StThemeNode      *node)
{
  GType type = node->element_type;
  int i;

  if (simple->type != G_TYPE_INVALID &&
      !g_type_is_a (type, simple->type))
    return FALSE;

  for (i = 0; i < simple->additionals->len; i++)
    {
      StAdditionalSelector *additional = &g_array_index (simple->additionals,
                                                         StAdditionalSelector, i);

      switch (additional->kind) {
      case ID_ADD_SELECTOR:
        if (!_st_theme_node_has_id (node, additional->value))
          return FALSE;

      case CLASS_ADD_SELECTOR:
        if (!_st_theme_node_has_class (node, additional->value))
          return FALSE;

      case PSEUDO_CLASS_ADD_SELECTOR:
        if (!_st_theme_node_has_pseudo (node, additional->value))
          return FALSE;
      }
    }

  return TRUE;
}

static gboolean
st_selector_matches (StSimpleSelector *simple_chain,
                     StThemeNode      *node)
{
  StThemeNode *parent;

  if (!st_simple_selector_matches (simple_chain, node))
    return FALSE;

  if (simple_chain->prev == NULL)
    return TRUE;

  parent = st_theme_node_get_parent (node);

  if (simple_chain->combinator == COMBINATOR_GREATER)
    {
      if (parent)
        return st_selector_matches (simple_chain->prev, parent);
      else
        return FALSE;
    }
  else
    {
      /* Try every parent, until we find one that matches */
      while (parent) {
        if (st_selector_matches (simple_chain->prev, parent))
          return TRUE;

        parent = st_theme_node_get_parent (parent);
      }

      /* No match */
      return FALSE;
    }
}

GPtrArray *
st_cascade_get_matched_properties (StCascade      *cascade,
                                   StThemeNode    *node)
{
  GPtrArray *properties;
  int i, j, k;

  properties = g_ptr_array_new_with_free_func ((GDestroyNotify) st_property_unref);

  for (i = 0; i < cascade->stylesheets->len; i++)
    {
      StStylesheet *sheet = cascade->stylesheets->pdata[i];

      for (j = 0; j < sheet->statements->len; j++)
        {
          StStatement *statement = sheet->statements->pdata[j];

          if (st_selector_matches (statement->simple_chain, node))
            {
              for (k = 0; k < statement->properties->len; k++)
                {
                  StProperty *prop;

                  prop = g_ptr_array_index (statement->properties, i);
                  g_ptr_array_add (properties, st_property_ref (prop));
                }
            }
        }
    }

  /* We count on a stable sort here so that later declarations come
   * after earlier declarations */
  g_ptr_array_sort (properties, (GCompareFunc) st_property_compare);

  return properties;
}

/* FIXME: leaking memory? */
GPtrArray *
st_cascade_parse_declaration_list (StCascade  *cascade,
                                   const char *str)
{
  GPtrArray *array;
  CRDeclaration *decl;

  decl = cr_declaration_parse_list_from_buf ((const guchar *)str,
                                             CR_UTF_8);

  array = g_ptr_array_new_with_free_func ((GDestroyNotify) st_property_unref);
  st_precache_declaration_list (array, decl);

  return array;
}
