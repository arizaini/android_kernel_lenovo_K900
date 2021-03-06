#ifndef __VECTOR_OPS_H_INCLUDED__
#define __VECTOR_OPS_H_INCLUDED__

#include "storage_class.h"

#ifndef __INLINE_VECTOR_OPS__
#define STORAGE_CLASS_VECTOR_OPS_H STORAGE_CLASS_EXTERN
#define STORAGE_CLASS_VECTOR_OPS_C
#include "vector_ops_public.h"
#else  /* __INLINE_VECTOR_OPS__ */
#define STORAGE_CLASS_VECTOR_OPS_H STORAGE_CLASS_INLINE
#define STORAGE_CLASS_VECTOR_OPS_C STORAGE_CLASS_INLINE
#include "vector_ops_private.h"
#endif /* __INLINE_VECTOR_OPS__ */

#endif /* __VECTOR_OPS_H_INCLUDED__ */
