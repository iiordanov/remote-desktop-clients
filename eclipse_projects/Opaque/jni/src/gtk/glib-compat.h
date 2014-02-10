/* -*- Mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, see <http://www.gnu.org/licenses/>.
*/
#ifndef GLIB_COMPAT_H
#define GLIB_COMPAT_H

#include <glib-object.h>
#include <gio/gio.h>

#if !GLIB_CHECK_VERSION(2,26,0)
#define G_DEFINE_BOXED_TYPE(TypeName, type_name, copy_func, free_func) G_DEFINE_BOXED_TYPE_WITH_CODE (TypeName, type_name, copy_func, free_func, {})
#define G_DEFINE_BOXED_TYPE_WITH_CODE(TypeName, type_name, copy_func, free_func, _C_) _G_DEFINE_BOXED_TYPE_BEGIN (TypeName, type_name, copy_func, free_func) {_C_;} _G_DEFINE_TYPE_EXTENDED_END()
#if __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 7)
#define _G_DEFINE_BOXED_TYPE_BEGIN(TypeName, type_name, copy_func, free_func) \
GType \
type_name##_get_type (void) \
{ \
  static volatile gsize g_define_type_id__volatile = 0; \
  if (g_once_init_enter (&g_define_type_id__volatile))  \
    { \
      GType (* _g_register_boxed) \
        (const gchar *, \
         union \
           { \
             TypeName * (*do_copy_type) (TypeName *); \
             TypeName * (*do_const_copy_type) (const TypeName *); \
             GBoxedCopyFunc do_copy_boxed; \
           } __attribute__((__transparent_union__)), \
         union \
           { \
             void (* do_free_type) (TypeName *); \
             GBoxedFreeFunc do_free_boxed; \
           } __attribute__((__transparent_union__)) \
        ) = g_boxed_type_register_static; \
      GType g_define_type_id = \
        _g_register_boxed (g_intern_static_string (#TypeName), copy_func, free_func); \
      { /* custom code follows */
#else
#define _G_DEFINE_BOXED_TYPE_BEGIN(TypeName, type_name, copy_func, free_func) \
GType \
type_name##_get_type (void) \
{ \
  static volatile gsize g_define_type_id__volatile = 0; \
  if (g_once_init_enter (&g_define_type_id__volatile))  \
    { \
      GType g_define_type_id = \
        g_boxed_type_register_static (g_intern_static_string (#TypeName), \
                                      (GBoxedCopyFunc) copy_func, \
                                      (GBoxedFreeFunc) free_func); \
      { /* custom code follows */
#endif /* __GNUC__ */

#define g_source_set_name(source, name) G_STMT_START { } G_STMT_END

#define G_TYPE_ERROR (spice_error_get_type ())
GType spice_error_get_type (void) G_GNUC_CONST;

#define G_PARAM_DEPRECATED  (1 << 31)

void      g_key_file_set_uint64             (GKeyFile             *key_file,
					     const gchar          *group_name,
					     const gchar          *key,
					     guint64               value);
#endif /* glib 2.26 */

#if !GLIB_CHECK_VERSION(2,28,0)
#define g_clear_object(object_ptr) \
  G_STMT_START {                                                             \
    /* Only one access, please */                                            \
    gpointer *_p = (gpointer) (object_ptr);                                  \
    gpointer _o;                                                             \
                                                                             \
    do                                                                       \
      _o = g_atomic_pointer_get (_p);                                        \
    while G_UNLIKELY (!g_atomic_pointer_compare_and_exchange (_p, _o, NULL));\
                                                                             \
    if (_o)                                                                  \
      g_object_unref (_o);                                                   \
  } G_STMT_END

void
g_simple_async_result_take_error(GSimpleAsyncResult *simple,
                                 GError             *error);

void
g_slist_free_full(GSList         *list,
                  GDestroyNotify free_func);

#endif /* glib 2.28 */

#if !GLIB_CHECK_VERSION(2,30,0)
#define G_TYPE_MAIN_CONTEXT (spice_main_context_get_type ())
GType spice_main_context_get_type (void) G_GNUC_CONST;
#endif

#if !GLIB_CHECK_VERSION(2,32,0)
# define G_SIGNAL_DEPRECATED (1 << 9)
#endif

#ifndef g_clear_pointer
#define g_clear_pointer(pp, destroy) \
  G_STMT_START {                                                               \
    G_STATIC_ASSERT (sizeof *(pp) == sizeof (gpointer));                       \
    /* Only one access, please */                                              \
    gpointer *_pp = (gpointer *) (pp);                                         \
    gpointer _p;                                                               \
    /* This assignment is needed to avoid a gcc warning */                     \
    GDestroyNotify _destroy = (GDestroyNotify) (destroy);                      \
                                                                               \
    (void) (0 ? (gpointer) *(pp) : 0);                                         \
    do                                                                         \
      _p = g_atomic_pointer_get (_pp);                                         \
    while G_UNLIKELY (!g_atomic_pointer_compare_and_exchange (_pp, _p, NULL)); \
                                                                               \
    if (_p)                                                                    \
      _destroy (_p);                                                           \
  } G_STMT_END
#endif

#if !GLIB_CHECK_VERSION(2,27,2)
guint64 g_get_monotonic_time(void);
#endif

#endif /* GLIB_COMPAT_H */
