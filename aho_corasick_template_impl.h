
/*
* Copyright 2017 Laurent Farhi
* Contact: lfarhi@sfr.fr
*
*  This file is free software: you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation, either version 3 of the License, or
*  (at your option) any later version.
*
*  This file is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
*
*  You should have received a copy of the GNU General Public License
*  along with this file.  If not, see <http://www.gnu.org/licenses/>.
*/

// Initialized by gcc -fpreprocessed -dD -E -P aho_corasick.c | grep -v '^$' | indent

#ifndef __ACM_TEMPLATE_IMPL__

#  define __ACM_TEMPLATE_IMPL__
#  include <stddef.h>
#  include <inttypes.h>
#  include <stdlib.h>
#  include <stdio.h>
#  include <pthread.h>
#  include <string.h>
#  include <signal.h>

#  include "aho_corasick_template.h"

#  define ACM_ASSERT(cond) do { if (!(cond)) { \
      fprintf(stderr, "FATAL ERROR: !(%1$s) in function %2$s at %3$s:%4$i)\n", #cond, __func__, __FILE__, __LINE__);\
      pthread_exit(0) ;\
} } while (0)

static char *
__str_copy__ (const char *v)
{
  if (!v)
    return 0;

  return strdup (v);
}

static void
__str_free__ (const char *v)
{
  if (!v)
    return;

  free ((char *) v);
}

static int
__eqchar (const char a, const char b)
{
  return a == b;
}

static int
__equchar (const unsigned char a, const unsigned char b)
{
  return a == b;
}

static int
__eqshort (const short a, const short b)
{
  return a == b;
}

static int
__equshort (const unsigned short a, const unsigned short b)
{
  return a == b;
}

static int
__eqint (const int a, const int b)
{
  return a == b;
}

static int
__equint (const unsigned int a, const unsigned int b)
{
  return a == b;
}

static int
__eqlong (const long a, const long b)
{
  return a == b;
}

static int
__equlong (const unsigned long a, const unsigned long b)
{
  return a == b;
}

static int
__eqllong (const long long a, const long long b)
{
  return a == b;
}

static int
__equllong (const unsigned long long a, const unsigned long long b)
{
  return a == b;
}

static int
__eqfloat (const float a, const float b)
{
  return a == b;
}

static int
__eqdouble (const double a, const double b)
{
  return a == b;
}

static int
__eqldouble (const long double a, const long double b)
{
  return a == b;
}

static int
__eqstring (const char * const a, const char * const b)
{
  return strcoll (a, b) == 0;
}

#  define EQ_DEFAULT(ACM_SYMBOL) _Generic(*(ACM_SYMBOL *)0,              \
  char:               __eqchar,                                        \
  unsigned char:      __equchar,                                       \
  short:              __eqshort,                                       \
  unsigned short:     __equshort,                                      \
  int:                __eqint,                                         \
  unsigned int:       __equint,                                        \
  long:               __eqlong,                                        \
  unsigned long:      __equlong,                                       \
  long long:          __eqllong,                                       \
  unsigned long long: __equllong,                                      \
  float:              __eqfloat,                                       \
  double:             __eqdouble,                                      \
  long double:        __eqldouble,                                     \
  char*:              __eqstring,                                      \
  default:            EQ_##ACM_SYMBOL##_DEFAULT                        \
  )

#  define COPY_DEFAULT(ACM_SYMBOL)                                     \
  _Generic(*(ACM_SYMBOL*)0, char*:__str_copy__, default:(COPY_##ACM_SYMBOL##_TYPE)0)

#  define DESTROY_DEFAULT(ACM_SYMBOL)                                  \
  _Generic(*(ACM_SYMBOL*)0, char*:__str_free__, default:(DESTROY_##ACM_SYMBOL##_TYPE)0)

// BEGIN DEFINE_ACM
#  define ACM_DEFINE(ACM_SYMBOL)                                       \
\
static ACM_SYMBOL (*COPY_##ACM_SYMBOL) (const ACM_SYMBOL) = 0;         \
static void (*DESTROY_##ACM_SYMBOL) (const ACM_SYMBOL) = 0;            \
static int (*EQ_##ACM_SYMBOL) (const ACM_SYMBOL, const ACM_SYMBOL) = 0;\
\
static void                                                            \
__DTOR_##ACM_SYMBOL(const ACM_SYMBOL letter)                           \
{                                                                      \
  if(DESTROY_##ACM_SYMBOL)                                             \
    DESTROY_##ACM_SYMBOL( letter );                                    \
  else if(DESTROY_DEFAULT(ACM_SYMBOL))                                 \
    DESTROY_DEFAULT(ACM_SYMBOL)( letter );                             \
}                                                                      \
\
static ACM_SYMBOL                                                      \
__COPY_##ACM_SYMBOL(const ACM_SYMBOL letter)                           \
{                                                                      \
  return COPY_##ACM_SYMBOL ?                                           \
           COPY_##ACM_SYMBOL(letter) : COPY_DEFAULT(ACM_SYMBOL) ?      \
                                         COPY_DEFAULT(ACM_SYMBOL)(letter) : letter;   \
}                                                                      \
\
static int EQ_##ACM_SYMBOL##_DEFAULT (const ACM_SYMBOL a, const ACM_SYMBOL b)      \
{                                                                      \
  const size_t size = sizeof (ACM_SYMBOL);                             \
  unsigned char *pa = (unsigned char *)(&a);                           \
  unsigned char *pb = (unsigned char *)(&b);                           \
\
  for (size_t i = 0 ; i < size ; i++)                                  \
    if (pa[i] != pb[i])                                                \
      return 0;       /* a < b */                                      \
\
  return 1;           /* a = b */                                      \
}                                                                      \
\
static int __EQ_##ACM_SYMBOL(const ACM_SYMBOL a, const ACM_SYMBOL b)   \
{                                                                      \
  return   EQ_##ACM_SYMBOL ?                                           \
             EQ_##ACM_SYMBOL (a,   b) :                                \
             (size_t)0 != (size_t)(EQ_DEFAULT (ACM_SYMBOL)) ?          \
               EQ_DEFAULT (ACM_SYMBOL)(a,   b) :                       \
               (fprintf (stderr, "%s", "ERROR: " "Missing equality operator for type '" #ACM_SYMBOL "'.\n"  \
                                       "       " "Use SET_EQ_OPERATOR(" #ACM_SYMBOL ", operator),\n"        \
                                       "       " "where operator is a function defined as:\n"               \
                                       "       " "int operator(" #ACM_SYMBOL " a, " #ACM_SYMBOL " b) { return a == b ; }.\n"   \
                                       "ABORT  " "\n"), fflush (0), raise (SIGABRT));                       \
}                                                                      \
\
struct _ac_state_##ACM_SYMBOL                                          \
{                                                                      \
  struct                                                               \
  {                                                                    \
    ACM_SYMBOL letter;                                                 \
    struct _ac_state_##ACM_SYMBOL *state;                              \
  } *goto_array;                                                       \
  size_t nb_goto;                                                      \
  struct                                                               \
  {                                                                    \
    size_t i_letter;                                                   \
    struct _ac_state_##ACM_SYMBOL *state;                              \
  } previous;                                                          \
  const struct _ac_state_##ACM_SYMBOL *fail_state;                     \
  int is_matching;                                                     \
  size_t nb_sequence;                                                  \
  size_t rank;                                                         \
  void *value;                                                         \
  void (*value_dtor) (void *);                                         \
};                                                                     \
typedef struct _ac_state_##ACM_SYMBOL ACState_##ACM_SYMBOL;            \
\
static ACState_##ACM_SYMBOL *                                          \
state_create_##ACM_SYMBOL (void)                                       \
{                                                                      \
  ACState_##ACM_SYMBOL *s = malloc (sizeof (*s));                      \
  ACM_ASSERT (s);                                                      \
  s->goto_array = 0;                                                   \
  s->nb_goto = 0;                                                      \
  s->previous.state = 0;                                               \
  s->previous.i_letter = 0;                                            \
  s->nb_sequence = 0;                                                  \
  s->is_matching = 0;                                                  \
  s->fail_state = 0;                                                   \
  s->rank = 0;                                                         \
  s->value = 0;                                                        \
  s->value_dtor = 0;                                                   \
  return s;                                                            \
}                                                                      \
\
static int                                                             \
state_goto_update_##ACM_SYMBOL (ACMachine_##ACM_SYMBOL * machine, Keyword_##ACM_SYMBOL sequence,\
                                void *value, void (*dtor) (void *))    \
{                                                                      \
  if (!sequence.length)                                                \
    return 0;                                                          \
  ACState_##ACM_SYMBOL *state_0 = machine->state_0;                    \
  ACState_##ACM_SYMBOL *state = state_0;                               \
  size_t j = 0;                                                        \
  for (; j < sequence.length;)                                         \
  {                                                                    \
    ACState_##ACM_SYMBOL *next = 0;                                    \
    for (size_t k = 0; k < state->nb_goto; k++)                        \
      if (machine->eq (state->goto_array[k].letter, sequence.letter[j]))         \
      {                                                                \
        next = state->goto_array[k].state;                             \
        break;                                                         \
      }                                                                \
    if (next)                                                          \
    {                                                                  \
      state = next;                                                    \
      j++;                                                             \
    }                                                                  \
    else                                                               \
      break;                                                           \
  }                                                                    \
  for (size_t p = j; p < sequence.length; p++)                         \
  {                                                                    \
    state->nb_goto++;                                                  \
    ACM_ASSERT (state->goto_array = realloc (state->goto_array,        \
                sizeof (*state->goto_array) * state->nb_goto));        \
    ACState_##ACM_SYMBOL *newstate = state_create_##ACM_SYMBOL ();     \
    state->goto_array[state->nb_goto - 1].state = newstate;            \
    state->goto_array[state->nb_goto - 1].letter = machine->copy (sequence.letter[p]);  \
    newstate->previous.state = state;                                  \
    newstate->previous.i_letter = state->nb_goto - 1;                  \
    state = newstate;                                                  \
    machine->size++;                                                   \
  }                                                                    \
  if (state->is_matching)                                              \
    return 0;                                                          \
  if (state->value && state->value_dtor)                               \
    state->value_dtor (state->value);                                  \
  state->value = value;                                                \
  state->value_dtor = dtor;                                            \
  state->is_matching = 1;                                              \
  state->nb_sequence = 1;                                              \
  state->rank = machine->rank++;                                       \
  machine->nb_sequence++;                                              \
  if (!machine->reconstruct)                                           \
    machine->reconstruct = 2;                                          \
  return 1;                                                            \
}                                                                      \
\
static const ACState_##ACM_SYMBOL *state_goto_##ACM_SYMBOL (           \
                const ACState_##ACM_SYMBOL * state,                    \
                ACM_SYMBOL letter, EQ_##ACM_SYMBOL##_TYPE eq);         \
\
static void                                                            \
state_reset_output_##ACM_SYMBOL (ACState_##ACM_SYMBOL * r)             \
{                                                                      \
  if (r->is_matching)                                                  \
    r->nb_sequence = 1;                                                \
  else                                                                 \
    r->nb_sequence = 0;                                                \
  for (size_t i = 0; i < r->nb_goto; i++)                              \
    state_reset_output_##ACM_SYMBOL (r->goto_array[i].state);          \
}                                                                      \
\
static void                                                            \
state_fail_state_construct_##ACM_SYMBOL (ACMachine_##ACM_SYMBOL * machine) \
{                                                                      \
  ACState_##ACM_SYMBOL *state_0 = machine->state_0;                    \
  if (machine->reconstruct == 2)                                       \
    state_reset_output_##ACM_SYMBOL (state_0);                         \
  machine->reconstruct = 0;                                            \
  state_0->fail_state = 0;                                             \
  size_t queue_length = 0;                                             \
  ACState_##ACM_SYMBOL **queue = 0;                                    \
  ACM_ASSERT (queue = malloc (sizeof (*queue) * (machine->size - 1))); \
  for (size_t i = 0; i < state_0->nb_goto; i++)                        \
  {                                                                    \
    ACState_##ACM_SYMBOL *s = state_0->goto_array[i].state;            \
    queue_length++;                                                    \
    queue[queue_length - 1] = s;                                       \
    s->fail_state = state_0;                                           \
  }                                                                    \
  size_t queue_read_pos = 0;                                           \
  while (queue_read_pos < queue_length)                                \
  {                                                                    \
    ACState_##ACM_SYMBOL *r = queue[queue_read_pos];                   \
    queue_read_pos++;                                                  \
    for (size_t i = 0; i < r->nb_goto; i++)                            \
    {                                                                  \
      ACState_##ACM_SYMBOL *s = r->goto_array[i].state;                \
      ACM_SYMBOL a = r->goto_array[i].letter;                          \
      queue_length++;                                                  \
      queue[queue_length - 1] = s;                                     \
      const ACState_##ACM_SYMBOL *state = r->fail_state;               \
      s->fail_state = state_goto_##ACM_SYMBOL (state, a, machine->eq); \
      s->nb_sequence += s->fail_state->nb_sequence;                    \
    }                                                                  \
  }                                                                    \
  free (queue);                                                        \
}                                                                      \
\
static ACMachine_##ACM_SYMBOL *                                        \
machine_create_##ACM_SYMBOL (ACState_##ACM_SYMBOL * state_0,           \
                             EQ_##ACM_SYMBOL##_TYPE eq,                \
                             COPY_##ACM_SYMBOL##_TYPE copier,          \
                             DESTROY_##ACM_SYMBOL##_TYPE dtor);        \
static ACMachine_##ACM_SYMBOL *ACM_create_##ACM_SYMBOL (EQ_##ACM_SYMBOL##_TYPE eq, \
                                                 COPY_##ACM_SYMBOL##_TYPE copier,  \
                                                 DESTROY_##ACM_SYMBOL##_TYPE dtor) \
{                                                                      \
  ACMachine_##ACM_SYMBOL *machine = machine_create_##ACM_SYMBOL (state_create_##ACM_SYMBOL (), eq, copier, dtor); \
  return machine;                                                      \
}                                                                      \
\
static ACMachine_##ACM_SYMBOL *                                        \
ACM_register_keyword_##ACM_SYMBOL (ACMachine_##ACM_SYMBOL * machine, Keyword_##ACM_SYMBOL y,\
                                   void *value, void (*dtor) (void *))                      \
{                                                                      \
  ACM_ASSERT (machine);                                                \
  if (!state_goto_update_##ACM_SYMBOL (machine, y, value, dtor))       \
    return 0;                                                          \
  return machine;                                                      \
}                                                                      \
\
static size_t                                                          \
ACM_nb_keywords_##ACM_SYMBOL (const ACMachine_##ACM_SYMBOL * machine)  \
{                                                                      \
  return machine ? machine->nb_sequence : 0;                           \
}                                                                      \
\
static ACState_##ACM_SYMBOL *                     \
get_last_state_##ACM_SYMBOL (const ACMachine_##ACM_SYMBOL * machine, Keyword_##ACM_SYMBOL sequence) \
{                                                                      \
  if (!machine || !sequence.length)                                    \
    return 0;                                                          \
  ACState_##ACM_SYMBOL *state = machine->state_0;                      \
  for (size_t j = 0; j < sequence.length; j++)                         \
  {                                                                    \
    ACState_##ACM_SYMBOL *next = 0;                                    \
    for (size_t k = 0; k < state->nb_goto; k++)                        \
      if (machine->eq (state->goto_array[k].letter, sequence.letter[j])) \
      {                                                                \
        next = state->goto_array[k].state;                             \
        break;                                                         \
      }                                                                \
    if (next)                                                          \
      state = next;                                                    \
    else                                                               \
      return 0;                                                        \
  }                                                                    \
  return state->is_matching ? state : 0;                               \
}                                                                      \
\
static int                     \
ACM_is_registered_keyword_##ACM_SYMBOL (const ACMachine_##ACM_SYMBOL * machine, \
                                        Keyword_##ACM_SYMBOL sequence, \
                                        void **value)                  \
{                                                                      \
  ACState_##ACM_SYMBOL *last = get_last_state_##ACM_SYMBOL (machine, sequence);  \
  if (last && value)                                                   \
    *value = last->value;                                              \
  return last ? 1 : 0;                                                 \
}                                                                      \
\
static int                     \
ACM_unregister_keyword_##ACM_SYMBOL (ACMachine_##ACM_SYMBOL * machine, Keyword_##ACM_SYMBOL y)  \
{                                                                      \
  ACState_##ACM_SYMBOL *state_0 = machine->state_0;                    \
  ACState_##ACM_SYMBOL *last = get_last_state_##ACM_SYMBOL (machine, y); \
  if (!last)                                                           \
    return 0;                                                          \
  machine->nb_sequence--;                                              \
  if (last->nb_goto)                                                   \
  {                                                                    \
    last->is_matching = 0;                                             \
    last->nb_sequence = 0;                                             \
    last->rank = 0;                                                    \
    return 1;                                                          \
  }                                                                    \
  ACState_##ACM_SYMBOL *prev = 0;                                      \
  do                                                                   \
  {                                                                    \
    prev = last->previous.state;                                       \
    prev->nb_goto--;                                                   \
    for (size_t k = last->previous.i_letter; k < prev->nb_goto; k++)   \
    {                                                                  \
      machine->destroy (prev->goto_array[k].letter);                 \
      prev->goto_array[k] = prev->goto_array[k + 1];                   \
      prev->goto_array[k].state->previous.i_letter = k;                \
    }                                                                  \
    prev->goto_array = realloc (prev->goto_array, sizeof (*prev->goto_array) * prev->nb_goto);  \
    ACM_ASSERT (!prev->nb_goto || prev->goto_array);                   \
    if (last->value && last->value_dtor)                               \
      last->value_dtor (last->value);                                  \
    free (last);                                                       \
    machine->size--;                                                   \
    last = prev;                                                       \
  }                                                                    \
  while (prev && prev != state_0 && !prev->is_matching && !prev->nb_goto);  \
  if (!machine->reconstruct)                                           \
    machine->reconstruct = 2;                                          \
  return 1;                                                            \
}                                                                      \
\
static void                                                            \
foreach_keyword_##ACM_SYMBOL (const ACState_##ACM_SYMBOL * state, ACM_SYMBOL ** letters, size_t * length, size_t depth, \
                              void (*operator) (Keyword_##ACM_SYMBOL, void *)) \
{                                                                      \
  if (state->is_matching && depth)                                     \
  {                                                                    \
    Keyword_##ACM_SYMBOL k = {.letter = *letters,.length = depth };    \
    (*operator) (k, state->value);                                     \
  }                                                                    \
  if (state->nb_goto && depth >= *length)                              \
  {                                                                    \
    (*length)++;                                                       \
    *letters = realloc (*letters, sizeof (**letters) * (*length));     \
    ACM_ASSERT (letters);                                              \
  }                                                                    \
  for (size_t i = 0; i < state->nb_goto; i++)                          \
  {                                                                    \
    (*letters)[depth] = state->goto_array[i].letter;                   \
    foreach_keyword_##ACM_SYMBOL (state->goto_array[i].state, letters, length, depth + 1, operator);  \
  }                                                                    \
}                                                                      \
\
static void                                                            \
ACM_foreach_keyword_##ACM_SYMBOL (const ACMachine_##ACM_SYMBOL * machine, void (*operator) (Keyword_##ACM_SYMBOL, void *))                     \
{                                                                      \
  if (!machine || !operator)                                           \
    return;                                                            \
  ACState_##ACM_SYMBOL *state_0 = machine->state_0;                    \
  ACM_SYMBOL *letters = 0;                                             \
  size_t depth = 0;                                                    \
  foreach_keyword_##ACM_SYMBOL (state_0, &letters, &depth, 0, operator);\
  free (letters);                                                      \
}                                                                      \
\
static void                                                            \
state_release_##ACM_SYMBOL (const ACState_##ACM_SYMBOL * state,        \
                            DESTROY_##ACM_SYMBOL##_TYPE dtor)          \
{                                                                      \
  for (size_t i = 0; i < state->nb_goto; i++)                          \
    state_release_##ACM_SYMBOL (state->goto_array[i].state, dtor);     \
  for (size_t i = 0; i < state->nb_goto; i++)                          \
    if (dtor)                                                          \
      dtor (state->goto_array[i].letter);                              \
    else                                                               \
    __DTOR_##ACM_SYMBOL (state->goto_array[i].letter);                 \
  free (state->goto_array);                                            \
  if (state->value && state->value_dtor)                               \
    state->value_dtor (state->value);                                  \
  free ((ACState_##ACM_SYMBOL *) state);                               \
}                                                                      \
\
static void                                                            \
ACM_release_##ACM_SYMBOL (const ACMachine_##ACM_SYMBOL * machine)      \
{                                                                      \
  if (!machine)                                                        \
    return;                                                            \
  state_release_##ACM_SYMBOL (machine->state_0, machine->destroy);     \
  free ((ACMachine_##ACM_SYMBOL *) machine);                           \
}                                                                      \
\
static const ACState_##ACM_SYMBOL *                                    \
state_goto_##ACM_SYMBOL (const ACState_##ACM_SYMBOL * state, ACM_SYMBOL letter,\
                         EQ_##ACM_SYMBOL##_TYPE eq)                    \
{                                                                      \
  while (1)                                                            \
  {                                                                    \
    for (size_t i = 0; i < state->nb_goto; i++)                        \
      if (eq (state->goto_array[i].letter, letter)) \
        return state->goto_array[i].state;                             \
    if (state->fail_state == 0)                                        \
      return state;                                                    \
    state = state->fail_state;                                         \
  }                                                                    \
}                                                                      \
\
static size_t                                                          \
ACM_nb_matches_##ACM_SYMBOL (ACMachine_##ACM_SYMBOL * machine, ACM_SYMBOL letter)     \
{                                                                      \
  if (!machine)                                                        \
    return 0;                                                          \
  if (machine->reconstruct)                                            \
    state_fail_state_construct_##ACM_SYMBOL (machine);                 \
  machine->current_state = state_goto_##ACM_SYMBOL (machine->current_state, letter, machine->eq);  \
  return machine->current_state->nb_sequence;                          \
}                                                                      \
\
static size_t                                                          \
ACM_get_match_##ACM_SYMBOL (const ACMachine_##ACM_SYMBOL * machine, size_t index,\
                            Keyword_##ACM_SYMBOL * match, void **value)          \
{                                                                      \
  ACM_ASSERT (machine && index < machine->current_state->nb_sequence); \
  const ACState_##ACM_SYMBOL *state = machine->current_state;          \
  size_t i = 0;                                                        \
  for (; state; state = state->fail_state, i++)                        \
  {                                                                    \
    while (!state->is_matching && state->fail_state)                   \
      state = state->fail_state;                                       \
    if (i == index)                                                    \
      break;                                                           \
  }                                                                    \
  if (match)                                                           \
  {                                                                    \
    match->length = 0;                                                 \
    for (const ACState_##ACM_SYMBOL * s = state; s && s->previous.state; s = s->previous.state)            \
      match->length++;                                                 \
    ACM_ASSERT (match->letter = realloc (match->letter, sizeof (*match->letter) * match->length));         \
    i = 0;                                                             \
    for (const ACState_##ACM_SYMBOL * s = state; s && s->previous.state; s = s->previous.state)            \
    {                                                                  \
      match->letter[match->length - i - 1] = s->previous.state->goto_array[s->previous.i_letter].letter;   \
      i++;                                                             \
    }                                                                  \
  }                                                                    \
  if (value)                                                           \
    *value = state->value;                                             \
  return state->rank;                                                  \
}                                                                      \
\
static void                                                            \
ACM_reset_##ACM_SYMBOL (ACMachine_##ACM_SYMBOL * machine)              \
{                                                                      \
  if (machine)                                                         \
    machine->current_state = machine->state_0;                         \
}                                                                      \
\
static const struct _ac_vtable_##ACM_SYMBOL VTABLE_##ACM_SYMBOL =      \
{                                                                      \
  ACM_register_keyword_##ACM_SYMBOL,                                   \
  ACM_is_registered_keyword_##ACM_SYMBOL,                              \
  ACM_unregister_keyword_##ACM_SYMBOL,                                 \
  ACM_nb_keywords_##ACM_SYMBOL,                                        \
  ACM_foreach_keyword_##ACM_SYMBOL,                                    \
  ACM_release_##ACM_SYMBOL,                                            \
                                                                       \
  ACM_nb_matches_##ACM_SYMBOL,                                         \
  ACM_reset_##ACM_SYMBOL,                                              \
  ACM_get_match_##ACM_SYMBOL,                                          \
};                                                                     \
\
static ACMachine_##ACM_SYMBOL *                                        \
machine_create_##ACM_SYMBOL (ACState_##ACM_SYMBOL * state_0,           \
                             EQ_##ACM_SYMBOL##_TYPE eq,                \
                             COPY_##ACM_SYMBOL##_TYPE copier,          \
                             DESTROY_##ACM_SYMBOL##_TYPE dtor)         \
{                                                                      \
  ACMachine_##ACM_SYMBOL *machine = malloc (sizeof (*machine));        \
  ACM_ASSERT (machine);                                                \
  machine->reconstruct = 1;                                            \
  machine->size = 1;                                                   \
  machine->current_state = machine->state_0 = state_0;                 \
  machine->rank = machine->nb_sequence = 0;                            \
  machine->vtable = &(VTABLE_##ACM_SYMBOL);                            \
  machine->copy = copier ? copier : __COPY_##ACM_SYMBOL;               \
  machine->destroy = dtor ? dtor : __DTOR_##ACM_SYMBOL;                \
  machine->eq = eq ? eq : __EQ_##ACM_SYMBOL;                           \
  return machine;                                                      \
}                                                                      \
// END DEFINE_ACM

#endif
