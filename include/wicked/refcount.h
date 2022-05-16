/*
 *	refcount -- reference couting utils
 *
 *	Copyright (C) 2022 SUSE LLC
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 *	Authors:
 *		Marius Tomaschewski
 *		Clemens Famulla-Conrad
 */
#ifndef WICKED_REFCOUNT_H
#define WICKED_REFCOUNT_H

#include <wicked/types.h>

typedef unsigned int	ni_refcount_t;

inline void		ni_refcount_init(ni_refcount_t *refcount)
{
	ni_assert(refcount);
	*refcount = 1;
}

inline ni_bool_t	ni_refcount_increment(ni_refcount_t *refcount)
{
	ni_assert(refcount && *refcount);
	(*refcount)++;
	return *refcount != 0;
}

inline ni_bool_t	ni_refcount_decrement(ni_refcount_t *refcount)
{
	ni_assert(refcount && *refcount);
	(*refcount)--;
	return *refcount == 0;
}


#define ni_declare_refcounted_ref(object)			\
object##_t *		object##_ref(object##_t *)
#define ni_declare_refcounted_free(object)			\
void			object##_free(object##_t *)
#define ni_declare_refcounted_hold(object)			\
ni_bool_t		object##_hold(object##_t **, object##_t *)
#define ni_declare_refcounted_drop(object)			\
ni_bool_t		object##_drop(object##_t **)
#define ni_declare_refcounted_move(object)			\
ni_bool_t		object##_move(object##_t **, object##_t **)


#define ni_define_refcounted_ref(object)			\
object##_t *							\
object##_ref(object##_t *ref)					\
{								\
	if (ref && ni_refcount_increment(&ref->refcount))	\
		return ref;					\
	return NULL;						\
}

#define ni_define_refcounted_free(object)			\
void								\
object##_free(object##_t *ref)					\
{								\
	if (ref && ni_refcount_decrement(&ref->refcount)) {	\
		object##_destroy(ref);				\
		free(ref);					\
	}							\
}

#define ni_define_refcounted_hold(object)			\
ni_bool_t							\
object##_hold(object##_t **tie, object##_t *ref)		\
{								\
	object##_t *old;					\
	if (tie && ref) {					\
		old = *tie;					\
		*tie = object##_ref(ref);			\
		object##_free(old);				\
		return TRUE;					\
	}							\
	return FALSE;						\
}

#define ni_define_refcounted_drop(object)			\
ni_bool_t							\
object##_drop(object##_t **tie)					\
{								\
	object##_t *old;					\
	if (tie) {						\
		old = *tie;					\
		*tie = NULL;					\
		object##_free(old);				\
		return TRUE;					\
	}							\
	return FALSE;						\
}

#define ni_define_refcounted_move(object)			\
ni_bool_t							\
object##_move(object##_t **dst, object##_t **src)		\
{								\
	if (src && object##_hold(dst, *src))			\
		return object##_drop(src);			\
	return FALSE;						\
}

#endif /* WICKED_REFCOUNT_H */
