/// @file

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

#include <stdio.h>
#include <pthread.h>

#ifndef ACM_SYMBOL
// User defined file containing the definition of ACM_SYMBOL.
#  include "aho_corasick_symbol.h"
#endif

#include "aho_corasick.h"

#ifdef PRIVATE_MODULE
#  warning The Aho-Corasick algorithm is compiled as a private module (PRIVATE_MODULE is defined).
#endif

#ifdef ACM_ASSOCIATED_VALUE
#  warning States are compiled with associated values (ACM_ASSOCIATED_VALUE is defined).
#endif

#define ACM_ASSERT(cond) do { if (!(cond)) {  \
      fprintf(stderr, "FATAL ERROR: !(%1$s) in function %2$s at %3$s:%4$i)\n", #cond, __func__, __FILE__, __LINE__);\
      pthread_exit(0) ;\
} } while (0)

#undef ACM_SYMBOL_EQ
#ifdef ACM_SYMBOL_EQ_OPERATOR
#  ifndef PRIVATE_MODULE
extern int ACM_SYMBOL_EQ_OPERATOR (ACM_SYMBOL a, ACM_SYMBOL b);
#  endif
static int (*__eqdefault) (ACM_SYMBOL a, ACM_SYMBOL b) = ACM_SYMBOL_EQ_OPERATOR;

#  warning User defined equality function of symbols (ACM_SYMBOL_EQ_OPERATOR is defined).
#  define ACM_SYMBOL_EQ(keyword_sign, text_sign) __eqdefault((keyword_sign), (text_sign))
#else
#  define ACM_SYMBOL_EQ(keyword_sign, text_sign) ((keyword_sign) == (text_sign))
#endif

#undef ACM_SYMBOL_COPY
#ifdef ACM_SYMBOL_COPY_OPERATOR
static ACM_SYMBOL (*__copydefault) (ACM_SYMBOL a) = ACM_SYMBOL_COPY_OPERATOR;

#  define ACM_SYMBOL_COPY(a) __copydefault(a)
#else
// Same side effect even if no copy constructor is defined for type ACM_SYMBOL.
#  define ACM_SYMBOL_COPY(a) (a)
#endif

#undef ACM_SYMBOL_DTOR
#ifdef ACM_SYMBOL_DTOR_OPERATOR
static void (*__dtordefault) (ACM_SYMBOL a) = ACM_SYMBOL_DTOR_OPERATOR;

#  define ACM_SYMBOL_DTOR(a) __dtordefault(a)
#else
#  define ACM_SYMBOL_DTOR(a) (void)(a)
#endif

/// A state of the state machine.
/// @note Memory usage is of the order of the size of all registered keywords.
struct _ac_state                // [state s]
{
  /// A link to the next states
  struct
  {
    ACM_SYMBOL letter;          // [a symbol]
    struct _ac_state *state;    // [g(s, letter)]
  } *goto_array;                // next states in the tree of the goto function
  size_t nb_goto;

  /// A link to the previous state
  struct
  {
    size_t i_letter;            // Index of the letter in the goto_array
    // ACM_SYMBOL letter = previous.state->goto_array[previous.i_letter].letter
    struct _ac_state *state;
  } previous;                   // Previous state

  const struct _ac_state *fail_state;   // [f(s)]

  int is_matching;              // true if the state matches a keyword.
  size_t nb_sequence;           // Number of matching keywords (Aho-Corasick : size (output (s))
  size_t rank;                  // Rank (0-based) of insertion of a keyword in the machine.

#ifdef ACM_ASSOCIATED_VALUE
  void *value;                  // An optional value associated to a state.
  void (*value_dtor) (void *);  // Destrcutor of the associated value, called a state machine release.
#endif
};
typedef struct _ac_state ACState;

struct _ac_machine
{
  ACState *state_0;
  const ACState *current_state;
  size_t rank;                  // Number of keywords registered in the machine.
  size_t nb_sequence;           // Number of keywords in the machine.
  int reconstruct;
  size_t size;
};

//-----------------------------------------------------
static ACState *
state_create (void)
{
  ACState *s = malloc (sizeof (*s));    /* [state s] */

  ACM_ASSERT (s);

  // [g(s, a) is undefined (= fail) for all input symbol a]
  s->goto_array = 0;
  s->nb_goto = 0;
  s->previous.state = 0;
  s->previous.i_letter = 0;

  // Aho-Corasick Algorithm 2: "We assume output(s) is empty when state s is first created."
  s->nb_sequence = 0;           // number of outputs in [output(s)]
  s->is_matching = 0;           // indicates that the state is the last node of a registered keyword

  s->fail_state = 0;
  s->rank = 0;

#ifdef ACM_ASSOCIATED_VALUE
  s->value = 0;
  s->value_dtor = 0;
#endif

  return s;
}

/// @see Aho-Corasick Algorithm 2: construction of the goto function - procedure enter(a[1] a[2] ... a[n]).
#ifndef ACM_ASSOCIATED_VALUE
static int
state_goto_update (ACMachine * machine, Keyword sequence /* a[1] a[2] ... a[n] */ )
#else
static int
state_goto_update (ACMachine * machine, Keyword sequence        /* a[1] a[2] ... a[n] */
                   , void *value, void (*dtor) (void *))
#endif
{
  if (!sequence.length)
    return 0;

  ACState *state_0 = machine->state_0;  // [state 0]

  // Iterators
  // Aho-Corasick Algorithm 2: state <- 0
  ACState *state = state_0;

  // Aho-Corasick Algorithm 2: j <- 1
  size_t j = 0;

  // Aho-Corasick Algorithm 2: while g(state, a[j]) != fail [and j <= m] do
  // Iterations on i and s until a final state
  for (; j < sequence.length /* [m] */ ;)
  {
    ACState *next = 0;

    // Aho-Corasick Algorithm 2: "g(s, l) = fail if l is undefined or if g(s, l) has not been defined."
    // Loop on all symbols a for which g(state, a) is defined.
    for (size_t k = 0; k < state->nb_goto; k++)
      if (ACM_SYMBOL_EQ (state->goto_array[k].letter, sequence.letter[j]))
      {
        // [if g(state, a[j]) is defined]
        next = state->goto_array[k].state;
        break;
      }

    // [if g(state, a[j]) is defined (!= fail)]
    if (next)
    {
      // Aho-Corasick Algorithm 2: state <- g(state, a[j])
      state = next;
      // Aho-Corasick Algorithm 2: j <- j + 1
      j++;
    }
    // [g(state, a[j]) is not defined (= fail)]
    else
      break;                    // exit while g(state, a[j]) != fail
  }

  // Aho-Corasick Algorithm 2: for p <- j until m do
  // Appending states for the new sequence to the final state found
  for (size_t p = j; p < sequence.length /* [m] */ ; p++)
  {
    state->nb_goto++;
    ACM_ASSERT (state->goto_array = realloc (state->goto_array, sizeof (*state->goto_array) * state->nb_goto));

    // Creation of a new state
    // Aho-Corasick Algorithm 2: newstate <- newstate + 1
    ACState *newstate = state_create ();

    // Aho-Corasick Algorithm 2: g(state, a[p]) <- newstate
    state->goto_array[state->nb_goto - 1].state = newstate;
    state->goto_array[state->nb_goto - 1].letter = ACM_SYMBOL_COPY (sequence.letter[p]);

    // Backward link: previous(newstate, a[p]) <- state
    newstate->previous.state = state;
    //state->goto_array[state->nb_goto - 1].state->previous.i_letter = state->nb_goto - 1;
    newstate->previous.i_letter = state->nb_goto - 1;

    // Aho-Corasick Algorithm 2: state <- newstate
    state = newstate;
    machine->size++;
  }

  // If the keyword was already previously registered, its rank and associated value are left unchanged.
  if (state->is_matching)
    return 0;

#ifdef ACM_ASSOCIATED_VALUE
  if (state->value && state->value_dtor)
    state->value_dtor (state->value);
  state->value = value;
  state->value_dtor = dtor;
#endif

  // Aho-Corasick Algorithm 2: output (state) <- { a[1] a[2] ... a[n] }
  // Aho-Corasick Algorithm 2: "We assume output(s) is empty when state s is first created."
  // Adding the sequence to the last found state (created or not)
  state->is_matching = 1;
  state->nb_sequence = 1;

  state->rank = machine->rank++;        // rank is a 0-based index
  machine->nb_sequence++;

  if (!machine->reconstruct)
    machine->reconstruct = 2;   // f(s) must be recomputed

  return 1;
}

static const ACState *state_goto (const ACState * state, ACM_SYMBOL letter);

static void
state_reset_output (ACState * r)
{
  if (r->is_matching)
    r->nb_sequence = 1;         // Reset to original output (as in state_goto_update)
  else
    r->nb_sequence = 0;

  for (size_t i = 0; i < r->nb_goto; i++)
    state_reset_output (r->goto_array[i].state);
}

/// @see Aho-Corasick Algorithm 3: construction of the failure function.
static void
state_fail_state_construct (ACMachine * machine)
{
  ACState *state_0 = machine->state_0;  // [state 0]

  if (machine->reconstruct == 2)
    state_reset_output (state_0);

  machine->reconstruct = 0;

  // Aho-Corasick Algorithm: "(except state 0 for which the failure function is not defined)."
  state_0->fail_state = 0;

  // Aho-Corasick Algorithm 3: queue <- empty
  // The first element in the queue will not be processed, therefore it can be added harmlessly.
  size_t queue_length = 0;
  ACState **queue = 0;

  ACM_ASSERT (queue = malloc (sizeof (*queue) * (machine->size - 1)));
  // Aho-Corasick Algorithm 3: for each a such that s != 0 [fail], where s <- g(0, a) do   [1]
  for (size_t i = 0; i < state_0->nb_goto; i++)
  {
    ACState *s = state_0->goto_array[i].state;  // [for each a such that s != 0 [fail], where s <- g(0, a)]

    // Aho-Corasick Algorithm 3: queue <- queue U {s}
    queue_length++;
    queue[queue_length - 1] = s /* s */ ;

    // Aho-Corasick Algorithm 3: f(s) <- 0
    s->fail_state = state_0;
  }                             // for (size_t i = 0; i < state_0->nb_goto; i++)

  size_t queue_read_pos = 0;

  // Aho-Corasick Algorithm 3: while queue != empty do
  while (queue_read_pos < queue_length)
  {
    // Aho-Corasick Algorithm 3: let r be the next state in queue
    ACState *r = queue[queue_read_pos];

    // Aho-Corasick Algorithm 3: queue <- queue - {r}
    queue_read_pos++;
    // Aho-Corasick Algorithm 3: for each a such that s != fail, where s <- g(r, a)
    for (size_t i = 0; i < r->nb_goto; i++)
    {
      ACState *s = r->goto_array[i].state;      // [s <- g(r, a)]
      ACM_SYMBOL a = r->goto_array[i].letter;

      // Aho-Corasick Algorithm 3: queue <- queue U {s}
      queue_length++;
      queue[queue_length - 1] = s;

      // Aho-Corasick Algorithm 3: state <- f(r)
      const ACState *state = r->fail_state /* f(r) */ ;

      // Aho-Corasick Algorithm 3: while g(state, a) = fail [and state != 0] do state <- f(state)        [2]
      //                           [if g(state, a) != fail then] f(s) <- g(state, a) [else f(s) <- 0]    [3]
      s->fail_state /* f(s) */  = state_goto (state, a);

      // Aho-Corasick Algorithm 3: output (s) <-output (s) U output (f(s))
      s->nb_sequence += s->fail_state->nb_sequence;
    }                           // for (size_t i = 0; i < r->nb_goto; i++)
  }                             // while (queue_read_pos < queue_length)

  free (queue);
}

static ACMachine *
machine_create (ACState * state_0)
{
  ACMachine *machine = malloc (sizeof (*machine));
  machine->reconstruct = 1;     // f(s) is undefined and has not been computed yet
  machine->size = 1;
  machine->current_state = machine->state_0 = state_0;

  return machine;
}

#ifndef ACM_ASSOCIATED_VALUE
ACM_PRIVATE ACMachine *
ACM_register_keyword (ACMachine * machine, Keyword y /* a[1] a[2] ... a[n] */ )
#else
ACM_PRIVATE ACMachine *
ACM_register_keyword (ACMachine * machine, Keyword y    /* a[1] a[2] ... a[n] */
                      , void *value, void (*dtor) (void *))
#endif
{
  // Aho-Corasick Algorithm 2: newstate <- 0
  // Create state 0.
  // Executed only once, when 0 is passed as root argument
  if (!machine)
    machine = machine_create (state_create ());

#ifndef ACM_ASSOCIATED_VALUE
  if (!state_goto_update (machine, y))
#else
  if (!state_goto_update (machine, y, value, dtor))
#endif
    return 0;

  // Aho-Corasick Algorithm 2: for all a such that g(0, a) = fail do g(0, a) <- 0
  // This statement is aimed to set the following property (here called the Aho-Corasick LOOP_0 property):
  //   "All our pattern matching machines have the property that g(0, l) != fail for all input symbol l.
  //    [...] this property of the goto function [g] on state 0 [root] ensures that one input symbol will be processed
  //    by the machine in every machine cycle [state_goto]."
  //   "We add a loop from state 0 to state 0 on all input symbols other than [the symbols l for which g(0, l) is already defined].
  //
  // N.B.: This property is *NOT* implemented in this code after calls to enter(y[i]) because
  //       it requires that the alphabet of all possible symbols is known in advance.
  //       This would kill the genericity of the code.
  //       Therefore, Algorithms 1, 3 and 4 *CANNOT* consider that g(0, l) never fails for any symbol l.
  //       g(0, l) can fail like any other state transition.
  //       Thus, the implementation slightly differs from the one proposed by Aho-Corasick.

  return machine;
}

ACM_PRIVATE size_t
ACM_nb_keywords (const ACMachine * machine)
{
  return machine ? machine->nb_sequence : 0;
}

static ACState *
get_last_state (const ACMachine * machine, Keyword sequence)
{
  if (!machine || !sequence.length)
    return 0;

  ACState *state = machine->state_0;    // [state 0]

  for (size_t j = 0; j < sequence.length; j++)
  {
    ACState *next = 0;

    for (size_t k = 0; k < state->nb_goto; k++)
      if (ACM_SYMBOL_EQ (state->goto_array[k].letter, sequence.letter[j]))
      {
        next = state->goto_array[k].state;
        break;
      }
    if (next)
      state = next;
    else
      return 0;
  }

  return state->is_matching ? state : 0;
}

#ifndef ACM_ASSOCIATED_VALUE
ACM_PRIVATE int
ACM_is_registered_keyword (const ACMachine * machine, Keyword sequence)
#else
ACM_PRIVATE int
ACM_is_registered_keyword (const ACMachine * machine, Keyword sequence, void **value)
#endif
{
  ACState *last = get_last_state (machine, sequence);

#ifdef ACM_ASSOCIATED_VALUE
  if (last && value)
    *value = last->value;
#endif

  return last ? 1 : 0;
}

ACM_PRIVATE int
ACM_unregister_keyword (ACMachine * machine, Keyword y)
{
  ACState *state_0 = machine->state_0;  // [state 0]

  ACState *last = get_last_state (machine, y);

  if (!last)
    return 0;

  // machine->rank is not decreased, so as to ensure unicity.
  machine->nb_sequence--;

  if (last->nb_goto)
  {
    last->is_matching = 0;
    last->nb_sequence = 0;
    last->rank = 0;
    return 1;
  }

  ACState *prev = 0;

  do
  {
    // last->nb_goto == 0
    prev = last->previous.state;

    // Remove last from prev->goto_array
    prev->nb_goto--;
    for (size_t k = last->previous.i_letter; k < prev->nb_goto; k++)
    {
      ACM_SYMBOL_DTOR (prev->goto_array[k].letter);
      prev->goto_array[k] = prev->goto_array[k + 1];
      prev->goto_array[k].state->previous.i_letter = k;
    }
    prev->goto_array = realloc (prev->goto_array, sizeof (*prev->goto_array) * prev->nb_goto);

#ifdef ACM_ASSOCIATED_VALUE
    // Release associated value;
    if (last->value && last->value_dtor)
      last->value_dtor (last->value);
#endif

    // Release last
    free (last);
    machine->size--;

    last = prev;
  }
  while (prev && prev != state_0 && !prev->is_matching && !prev->nb_goto);

  if (!machine->reconstruct)
    machine->reconstruct = 2;   // f(s) must be recomputed

  return 1;
}

#ifndef ACM_ASSOCIATED_VALUE
static void
foreach_keyword (const ACState * state, ACM_SYMBOL ** letters, size_t * length, size_t depth,
                 void (*operator) (Keyword))
#else
static void
foreach_keyword (const ACState * state, ACM_SYMBOL ** letters, size_t * length, size_t depth,
                 void (*operator) (Keyword, void *))
#endif
{
  if (state->is_matching && depth)
  {
    Keyword k = {.letter = *letters,.length = depth };
#ifndef ACM_ASSOCIATED_VALUE
    (*operator) (k);
#else
    (*operator) (k, state->value);
#endif
  }

  if (state->nb_goto && depth >= *length)
  {
    (*length)++;
    *letters = realloc (*letters, sizeof (**letters) * (*length));
  }

  for (size_t i = 0; i < state->nb_goto; i++)
  {
    (*letters)[depth] = state->goto_array[i].letter;
    foreach_keyword (state->goto_array[i].state, letters, length, depth + 1, operator);
  }
}

#ifndef ACM_ASSOCIATED_VALUE
ACM_PRIVATE void
ACM_foreach_keyword (const ACMachine * machine, void (*operator) (Keyword))
#else
ACM_PRIVATE void
ACM_foreach_keyword (const ACMachine * machine, void (*operator) (Keyword, void *))
#endif
{
  if (!machine || !operator)
    return;

  ACState *state_0 = machine->state_0;  // [state 0]

  ACM_SYMBOL *letters = 0;
  size_t depth = 0;

  foreach_keyword (state_0, &letters, &depth, 0, operator);

  free (letters);
}

static void
state_release (ACState * state)
{
  for (size_t i = 0; i < state->nb_goto; i++)
    state_release (state->goto_array[i].state);

  // Release goto_array
  for (size_t i = 0; i < state->nb_goto; i++)
    ACM_SYMBOL_DTOR (state->goto_array[i].letter);
  free (state->goto_array);

#ifdef ACM_ASSOCIATED_VALUE
  // Release associated value
  if (state->value && state->value_dtor)
    state->value_dtor (state->value);
#endif

  // Release state
  free (state);
}

ACM_PRIVATE void
ACM_release (ACMachine * machine)
{
  if (!machine)
    return;

  state_release (machine->state_0);
  free (machine);
}

/// @see Aho-Corasick Algorithm 1: Pattern matching machine - while loop.
static const ACState *
state_goto (const ACState * state, ACM_SYMBOL letter /* Aho-Corasick Algorithm 1: a[i] */ )
{
  // Aho-Corasick Algorithm 1: while g(state, a[i]) = fail [and state != 0] do state <- f(state)           [2]
  //                           [if g(state, a[i]) != fail then] state <- g(state, a[i]) [else state <- 0]  [3]
  //                           [The function returns state]
  while (1)
  {
    // [if g(state, a[i]) != fail then return g(state, a[i])]
    for (size_t i = 0; i < state->nb_goto; i++)
      if (ACM_SYMBOL_EQ (state->goto_array[i].letter, letter))
        return state->goto_array[i].state;
    // From here, [g(state, a[i]) = fail]

    // Algorithms 1 cannot consider that g(0, a) never fails because propoerty LOOP_0 has not been implemented.
    // Therefore, for state 0, we must simulate the property LOOP_0, i.e state 0 must be returned,
    // as if g(0, a[i]) would have been set to state 0 if g(0, a[i]) = fail (property LOOP_0).
    // After Algorithm 3 has been processed, the only state for which f(state) = 0 is state 0.
    // [if g(state, a[i]) = fail and state = 0 then return state 0]
    // Aho-Corasick Algorithm: "(except state 0 for which the failure function is not defined)."
    if (state->fail_state == 0)
      return state;
    // From here, [state != 0]

    // [if g(state, a[i]) = fail and state != 0 then state <- f(state)
    state = state->fail_state;
  }
}

/// @see Aho-Corasick Algorithm 1: Pattern matching machine - if output (stat) != empty
ACM_PRIVATE size_t
ACM_nb_matches (ACMachine * machine, ACM_SYMBOL letter)
{
  if (!machine)
    return 0;

  // N.B.: In Aho-Corasick, algorithm 3 is executed after all sequences have been inserted
  //       in the goto graph one after the other by algorithm 2.
  //       As a slight enhancement: the fail state chains are rebuilted from scratch when needed,
  //       i.e. if a keyword has been added since the last pattern maching serch.
  //       Therefore, algorithms 2 and 3 can be processed sequentially.
  //       but then, algorithm 3 must traverse the full goto graph every time a sequence has been added.
  if (machine->reconstruct)
    state_fail_state_construct (machine);

  machine->current_state = state_goto (machine->current_state, letter);
  return machine->current_state->nb_sequence;
}

/// @see Aho-Corasick Algorithm 1: Pattern matching machine - print output (state) [ith element]
#ifndef ACM_ASSOCIATED_VALUE
ACM_PRIVATE size_t
ACM_get_match (const ACMachine * machine, size_t index, MatchHolder * match)
#else
ACM_PRIVATE size_t
ACM_get_match (const ACMachine * machine, size_t index, MatchHolder * match, void **value)
#endif
{
  // Aho-Corasick Algorithm 1: if output(state) [ith element]
  ACM_ASSERT (machine && index < machine->current_state->nb_sequence);

  const ACState *state = machine->current_state;
  size_t i = 0;

  for (; state; state = state->fail_state, i++ /* skip to the next failing state */ )
  {
    // Look for the first state in the "failing states" chain which matches a keyword.
    while (!state->is_matching && state->fail_state)
      state = state->fail_state;

    if (i == index)
      break;
  }

  // Argument match could be passed to 0 if only value or rank is needed.
  if (match)
  {
    // Aho-Corasick Algorithm 1: [print i]
    // Aho-Corasick Algorithm 1: print output(state) [ith element]
    // Reconstruct the matching keyword moving backward from the matching state to the state 0.
    match->length = 0;
    for (const ACState * s = state; s && s->previous.state; s = s->previous.state)
      match->length++;

    // Reallocation of match->letter. match->letter should be freed by the user after the last call to ACM_get_match on match.
    ACM_ASSERT (match->letter = realloc (match->letter, sizeof (*match->letter) * match->length));
    i = 0;
    for (const ACState * s = state; s && s->previous.state; s = s->previous.state)
    {
      match->letter[match->length - i - 1] = s->previous.state->goto_array[s->previous.i_letter].letter;
      i++;
    }
  }

#ifdef ACM_ASSOCIATED_VALUE
  // Argument value could passed to 0 if the associated value is not needed.
  if (value)
    *value = state->value;
#endif

  return state->rank;
}

ACM_PRIVATE void
ACM_reset (ACMachine * machine)
{
  if (machine)
    machine->current_state = machine->state_0;
}
