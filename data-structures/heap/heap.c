/**
   heap.c

   Implementation of a generic dynamicaly allocated (min) heap with a hash
   table parameter. 

   The implementation provides a dynamic set in the min heap form for any
   elements in memory associated with priority values of basic type
   (e.g. char, int, long, double).

   The hash table parameter specifies a hash table used for in-heap
   search and modifications, and enables the optimization of space and
   time resources associated with heap operations by choice of a hash
   table, its load factor upper bound, and known or expected minimum
   number of simultaneously present elements.

   The implementation assumes that for every element in a heap, the 
   block of size elt_size pointed to by an argument passed as the elt
   parameter in heap_push is unique. Because an element can be
   represented by its unique pointer, this invariant only prevents
   associating a given element in memory with more than one priority
   value in a heap.

   Optimization:

   -  the pointer computations in pty_ptr and elt_ptr were optimized out
   in tests; if char *p = pty_ptr(h, i), then the corresponding element
   is pointed to by p + h->pty_size; pty_ptr and elt_ptr are left for
   readability.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "heap.h"
#include "utilities-mem.h"

static void swap(heap_t *h, size_t i, size_t j);
static void half_swap(heap_t *h, size_t t, size_t s);
static void heapify_up(heap_t *h, size_t i);
static void heapify_down(heap_t *h, size_t i);
static void *pty_ptr(const heap_t *h, size_t i);
static void *elt_ptr(const heap_t *h, size_t i);

/**
   Initializes a heap.
   h           : pointer to a preallocated block of size sizeof(heap_t)
   init_count  : > 0
   pty_size    : size of a contiguous priority object
   elt_size    : - size of an element, if the element is within a contiguous
                 memory block and a copy of the element is inserted,
                 - size of a pointer to an element, if the element is within
                 a noncontiguous memory block or a pointer to a contiguous
                 element is inserted
   hht         : a non-NULL pointer to a set of parameters specifying a
                 hash table for in-heap search and modifications; a hash
                 key has the size and bit pattern of the block of size
                 elt_size pointed to by elt in heap_push
   cmp_pty     : comparison function which returns a negative integer value
                 if the priority value pointed to by the first argument is
                 less than the priority value pointed to by the second, a
                 positive integer value if the priority value pointed to by
                 the first argument is greater than the priority value 
                 pointed to by the second, and zero integer value if the two
                 priority values are equal
   cmp_elt     : 
   rdc_elt     : both have to work on the same subset of bits in the element,
                 whether the element is within a contiguous or non-contiguous
                 memory block
   free_elt    : - if an element is within a contiguous memory block and
                 a copy of the element was inserted, then NULL as free_elt
                 is sufficient to delete the element,
                 - if an element is within a noncontiguous memory block or
                 a pointer to a contiguous element was inserted, then an
                 element-specific free_elt, taking a pointer to a pointer to an
                 element as its argument and leaving a block of size elt_size
                 pointed to by the argument, is necessary to delete the element
*/
void heap_init(heap_t *h,
	       size_t pty_size,
	       size_t elt_size,
	       size_t min_num,
	       size_t alpha_n,
	       size_t log_alpha_d,
	       const heap_ht_t *hht,
	       int (*cmp_pty)(const void *, const void *),
	       int (*cmp_elt)(const void *, const void *),
	       size_t (*rdc_elt)(const void *),
	       void (*free_elt)(void *)){
  size_t elt_rem, pty_rem;
  h->pty_size = pty_size;
  h->elt_size = elt_size;
  /* align priority relative to a malloc's pointer and compute pair_size */
  if (h->pty_size <= h->elt_size){
    h->elt_offset = h->elt_size;
  }else{
    elt_rem = h->pty_size % h->elt_size;
    h->elt_offset = add_sz_perror(h->pty_size,
				  (elt_rem > 0) * (h->elt_size - elt_rem));
  }
  pty_rem = add_sz_perror(h->elt_offset, h->elt_size) % h->pty_size;
  h->pair_size = add_sz_perror(h->elt_offset + h->elt_size,
			       (pty_rem > 0) * (h->pty_size - pty_rem));
  h->count = min_num;
  h->num_elts = 0;
  h->buf = malloc_perror(2, h->pair_size); /* 1st heapify, 2nd swap */
  h->pty_elts = malloc_perror(h->count, h->pair_size);
  h->hht = hht;
  h->cmp_pty = cmp_pty;
  h->cmp_elt = cmp_elt;
  h->rdc_elt = rdc_elt;
  h->free_elt = free_elt;
  /* hash table maps an element to a size_t index */ 
  h->hht->init(hht->ht,
	       elt_size,
	       sizeof(size_t),
	       h->count,
	       alpha_n,
	       log_alpha_d,
	       h->cmp_elt,
	       h->rdc_elt,
	       NULL, /* only elt_size block is deleted in hash table */
	       NULL);
  h->hht->align(h->hht->ht, sizeof(size_t)); /* size_t * to be dereferenced */ 
}

/**
*/
void heap_align(heap_t *h,
		size_t pty_alignment,
		size_t elt_alignment,
		size_t sz_alignment){
  size_t elt_rem, pty_rem;
  if (h->pty_size <= elt_alignment){
    h->elt_offset = elt_alignment;
  }else{
    elt_rem = h->pty_size % elt_alignment;
    h->elt_offset = add_sz_perror(h->pty_size,
				  (elt_rem > 0) * (elt_alignment - elt_rem));
  }
  pty_rem = add_sz_perror(h->elt_offset, h->elt_size) % pty_alignment;
  h->pair_size = add_sz_perror(h->elt_offset + h->elt_size,
			       (pty_rem > 0) * (pty_alignment - pty_rem));
  h->buf = realloc_perror(h->buf, 2, h->pair_size);
  memset(h->buf, 0, 2 * h->pair_size);
  h->pty_elts = realloc_perror(h->pty_elts, h->count, h->pair_size);
  h->hht->align(h->hht->ht, sz_alignment);
}

/**
   Pushes an element not in a heap and an associated priority value. 
   Prior to pushing, the membership of an element can be tested, if 
   necessary, with heap_search in O(1) time in expectation under the
   uniformity assumptions suitable for the used hash table.
   h           : pointer to an initialized heap
   pty         : pointer to a block of size pty_size that is an object of
                 basic type (e.g. char, int, long, double)
   elt         : pointer to a block of size elt_size that is either a
                 contiguous element object or a pointer to a contiguous or
                 non-contiguous element; the block must have a unique
                 bit pattern for each pushed element
*/
void heap_push(heap_t *h, const void *pty, const void *elt){
  size_t ix = h->num_elts;
  if (h->count == ix){
    /* grow heap; amortized constant overhead per push, 
       without considering realloc's search */
    h->count = mul_sz_perror(2, h->count);
    h->pty_elts = realloc_perror(h->pty_elts, h->count, h->pair_size);
  }
  memcpy(pty_ptr(h, ix), pty, h->pty_size);
  memcpy(elt_ptr(h, ix), elt, h->elt_size);
  h->hht->insert(h->hht->ht, elt, &ix);
  h->num_elts++;
  heapify_up(h, ix);
}

/** 
   Returns a pointer to the priority of an element in a heap or NULL if the
   element is not in the heap in O(1) time in expectation under the
   uniformity assumptions suitable for the used hash table. The returned
   pointer is guaranteed to point to the current priority value until another
   heap operation is performed. Please see the parameter specification in
   heap_push.
*/
void *heap_search(const heap_t *h, const void *elt){
  const size_t *ix_ptr = h->hht->search(h->hht->ht, elt);
  if (ix_ptr != NULL){
    return pty_ptr(h, *ix_ptr);
  }else{
    return NULL;
  }
}

/**
   Updates the priority value of an element that is in a heap. Prior
   to updating, the membership of an element can be tested, if necessary, 
   with heap_search in O(1) time in expectation under the uniformity
   assumptions suitable for the used hash table. Please see the parameter
   specification in heap_push.
*/
void heap_update(heap_t *h, const void *pty, const void *elt){
  size_t ix = *(const size_t *)h->hht->search(h->hht->ht, elt);
  memcpy(pty_ptr(h, ix), pty, h->pty_size);
  heapify_up(h, ix);
  heapify_down(h, ix);
}

/**
   Pops an element associated with a minimal priority value in a heap 
   according to cmp_pty. If the heap is empty, the memory blocks pointed 
   to by elt and pty remain unchanged. Please see the parameter
   specification in heap_push.
*/
void heap_pop(heap_t *h, void *pty, void *elt){
  size_t ix_buf, ix = 0;
  if (h->num_elts == 0) return;
  memcpy(pty, pty_ptr(h, ix), h->pty_size);
  memcpy(elt, elt_ptr(h, ix), h->elt_size);
  swap(h, ix, h->num_elts - 1);
  h->hht->remove(h->hht->ht, elt, &ix_buf);
  h->num_elts--;
  if (h->num_elts > 0) heapify_down(h, ix);
}

/**
   Frees a heap and leaves a block of size sizeof(heap_t) pointed to by
   an argument passed as the h parameter.
*/
void heap_free(heap_t *h){
  size_t i;
  if (h->free_elt != NULL){
    for (i = 0; i < h->num_elts; i++){
      h->free_elt(elt_ptr(h, i));
    } 
  }
  free(h->buf);
  free(h->pty_elts);
  h->hht->free(h->hht->ht); /* leaves a block of size of ht struct */
  h->buf = NULL;
  h->pty_elts = NULL;
}

/** Helper functions */

/**
   Swaps priorities and elements at indices i and j and maps the elements
   to their new indices in the hash table.
*/
static void swap(heap_t *h, size_t i, size_t j){
  void *buf = (char *)h->buf + h->pair_size; /* second subbuffer */
  if (i == j) return;
  memcpy(buf, pty_ptr(h, i), h->pair_size);
  memcpy(pty_ptr(h, i), pty_ptr(h, j), h->pair_size);
  memcpy(pty_ptr(h, j), buf, h->pair_size);
  h->hht->insert(h->hht->ht, elt_ptr(h, i), &i); /* two updates */
  h->hht->insert(h->hht->ht, elt_ptr(h, j), &j);
}

/**
   Copies the priority and element at index s to index t, and maps the
   copied element at index t to t in the hash table.
*/
static void half_swap(heap_t *h, size_t t, size_t s){
  if (s == t) return;
  memcpy(pty_ptr(h, t), pty_ptr(h, s), h->pair_size);
  h->hht->insert(h->hht->ht, elt_ptr(h, t), &t); /* update */
}

/**
   Heapifies the heap structure from the ith element upwards.
*/
static void heapify_up(heap_t *h, size_t i){
  size_t ju;
  size_t ix = i;
  memcpy(h->buf, pty_ptr(h, ix), h->pair_size);
  while (ix > 0){
    ju = (ix - 1) >> 1; /* divide by 2 */;
    if (h->cmp_pty(pty_ptr(h, ju), h->buf) > 0){
      half_swap(h, ix, ju);
      ix = ju;
    }else{
      break;
    }
  }
  memcpy(pty_ptr(h, ix), h->buf, h->pair_size);
  h->hht->insert(h->hht->ht, elt_ptr(h, ix), &ix);
}

/**
   Heapifies the heap structure with at least one element from the ith
   element downwards.
*/
static void heapify_down(heap_t *h, size_t i){
  size_t jl, jr;
  size_t ix = i;
  memcpy(h->buf, pty_ptr(h, ix), h->pair_size);
  /* 0 <= ix <= num_elts - 1 <= SIZE_MAX - 2 */
  while (ix + 2 <= h->num_elts - 1 - ix){
    /* both next left and next right indices have elements */
    jl = 2 * ix + 1;
    jr = 2 * ix + 2;
    if (h->cmp_pty(h->buf, pty_ptr(h, jl)) > 0 &&
	h->cmp_pty(pty_ptr(h, jl), pty_ptr(h, jr)) <= 0){
      half_swap(h, ix, jl);
      ix = jl;
    }else if (h->cmp_pty(h->buf, pty_ptr(h, jr)) > 0){
      /* jr has min pty relative to jl and the ith pty is greater */
      half_swap(h, ix, jr);
      ix = jr;
    }else{
      break;
    }
  }
  if (ix + 1 == h->num_elts - 1 - ix){
    jl = 2 * ix + 1;
    if (h->cmp_pty(h->buf, pty_ptr(h, jl)) > 0){
      half_swap(h, ix, jl);
      ix = jl;
    }
  }
  memcpy(pty_ptr(h, ix), h->buf, h->pair_size);
  h->hht->insert(h->hht->ht, elt_ptr(h, ix), &ix);
}

/**
   Computes a pointer to an element in the element-priority array of a heap.
*/
static void *pty_ptr(const heap_t *h, size_t i){
  return (void *)((char *)h->pty_elts + i * h->pair_size);
}

/**
   Computes a pointer to a priority in the element-priority array of a heap.
*/
static void *elt_ptr(const heap_t *h, size_t i){
  return (void *)((char *)h->pty_elts + i * h->pair_size + h->elt_offset);
}
