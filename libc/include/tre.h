/*
 * TRE internal definitions (from musl libc, adapted for sbunix).
 * Original TRE by Ville Laurikari; musl integration by Rich Felker.
 * See regcomp.c / regexec.c for full license text.
 */

#ifndef _LIBC_TRE_H
#define _LIBC_TRE_H

#include <regex.h>
#include <wchar.h>
#include <wctype.h>
#include <stddef.h>
#include <stdint.h>
#include <limits.h>

#undef TRE_MBSTATE
#define NDEBUG
#define TRE_REGEX_T_FIELD __opaque

#ifndef CHARCLASS_NAME_MAX
#define CHARCLASS_NAME_MAX 32
#endif

#define hidden
#define reg_errcode_t int

typedef wchar_t tre_char_t;

#define DPRINT(msg) do { } while (0)
#define elementsof(x) (sizeof(x) / sizeof((x)[0]))

#define tre_mbrtowc(pwc, s, n, ps) (mbtowc((pwc), (s), (n)))

typedef wint_t tre_cint_t;
#define TRE_CHAR_MAX 0x10ffff

#define tre_isalnum iswalnum
#define tre_isalpha iswalpha
#define tre_isblank iswblank
#define tre_iscntrl iswcntrl
#define tre_isdigit iswdigit
#define tre_isgraph iswgraph
#define tre_islower iswlower
#define tre_isprint iswprint
#define tre_ispunct iswpunct
#define tre_isspace iswspace
#define tre_isupper iswupper
#define tre_isxdigit iswxdigit

#define tre_tolower towlower
#define tre_toupper towupper
#define tre_strlen  wcslen

typedef wctype_t tre_ctype_t;
#define tre_isctype iswctype
#define tre_ctype   wctype

#define ALIGN(ptr, type) \
  ((((long)(ptr)) % sizeof(type)) ? ((sizeof(type) - (((long)(ptr)) % sizeof(type)))) : 0)

#undef MAX
#undef MIN
#define MAX(a, b) (((a) >= (b)) ? (a) : (b))
#define MIN(a, b) (((a) <= (b)) ? (a) : (b))

typedef struct tnfa_transition tre_tnfa_transition_t;

struct tnfa_transition {
  tre_cint_t code_min;
  tre_cint_t code_max;
  tre_tnfa_transition_t *state;
  int state_id;
  int *tags;
  int assertions;
  union {
    tre_ctype_t class;
    int backref;
  } u;
  tre_ctype_t *neg_classes;
};

#define ASSERT_AT_BOL         1
#define ASSERT_AT_EOL         2
#define ASSERT_CHAR_CLASS     4
#define ASSERT_CHAR_CLASS_NEG 8
#define ASSERT_AT_BOW         16
#define ASSERT_AT_EOW         32
#define ASSERT_AT_WB          64
#define ASSERT_AT_WB_NEG      128
#define ASSERT_BACKREF        256
#define ASSERT_LAST           256

typedef enum {
  TRE_TAG_MINIMIZE = 0,
  TRE_TAG_MAXIMIZE = 1
} tre_tag_direction_t;

struct tre_submatch_data {
  int so_tag;
  int eo_tag;
  int *parents;
};

typedef struct tre_submatch_data tre_submatch_data_t;

typedef struct tnfa tre_tnfa_t;

struct tnfa {
  tre_tnfa_transition_t *transitions;
  unsigned int num_transitions;
  tre_tnfa_transition_t *initial;
  tre_tnfa_transition_t *final;
  tre_submatch_data_t *submatch_data;
  char *firstpos_chars;
  int first_char;
  unsigned int num_submatches;
  tre_tag_direction_t *tag_directions;
  int *minimal_tags;
  int num_tags;
  int num_minimals;
  int end_tag;
  int num_states;
  int cflags;
  int have_backrefs;
  int have_approx;
};

#define TRE_MEM_BLOCK_SIZE 1024

typedef struct tre_list {
  void *data;
  struct tre_list *next;
} tre_list_t;

typedef struct tre_mem_struct {
  tre_list_t *blocks;
  tre_list_t *current;
  char *ptr;
  size_t n;
  int failed;
  void **provided;
} *tre_mem_t;

#define tre_mem_new_impl   __tre_mem_new_impl
#define tre_mem_alloc_impl __tre_mem_alloc_impl
#define tre_mem_destroy    __tre_mem_destroy

tre_mem_t tre_mem_new_impl(int provided, void *provided_block);
void *tre_mem_alloc_impl(tre_mem_t mem, int provided, void *provided_block,
                         int zero, size_t size);
void tre_mem_destroy(tre_mem_t mem);

#define tre_mem_new() tre_mem_new_impl(0, NULL)
#define tre_mem_alloc(mem, size) tre_mem_alloc_impl(mem, 0, NULL, 0, size)
#define tre_mem_calloc(mem, size) tre_mem_alloc_impl(mem, 0, NULL, 1, size)

#define xmalloc malloc
#define xcalloc calloc
#define xfree free
#define xrealloc realloc

#endif
