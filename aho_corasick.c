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

/// This implementation follows step by step the pseudo-code given in the original paper from Aho and Corasick.
/// Comments in the code starting by "Aho-Corasick" refer to that pseudo-code.
///
/// @see Aho, Alfred V.; Corasick, Margaret J. (June 1975). "Efficient string matching: An aid to bibliographic search".
/// Communications of the ACM. 18 (6): 333–340.
/// https://pdfs.semanticscholar.org/3547/ac839d02f6efe3f6f76a8289738a22528442.pdf
///
/// @see https://en.wikipedia.org/wiki/Aho%E2%80%93Corasick_algorithm
///
/// Compared to the implemenation proposed by Aho and Corasick, this one adds four enhancements:
/// 1. First of all, the implementation does not define any assumption on the size of alphabet used.
///    Particularly, the akphanet is not limited to 256 signs.
///    For instance, if ACM_SYMBOL is defined as 'long long int', then the number of possible signs would be 18446744073709551616.
///    For this to be possible, the assertion "for all a such that g(0, a) = fail do g(0, a) <- 0" at the end of algorithm 2 can not be fulfilled
///    because it would require to set g(0, a) for all the values of 'a' in the set of possible values of the alphabet,
///    and thus allocate (if not exhaust) a lot of memory.
///    Therefore, g(0, a) is kept equal to fail (i.e. a or g(0, a) is kept undefined) for all a not yet defined by keyword registration.
///    Nevertheless, for the state machine to work properly, it must behave as if g(0, a) would be equal to 0 whenever g(0, a) = fail.
///    Algorithms 1 and 3 (called after algorithm 2) must be adapted accordingly (modifications are tagged with [1], [2] and [3] in the code):
///    - [1] g(0, a) = (resp. !=) 0 must be replaced by: g(0, a) = (resp. !=) fail
///    - [2] g(state, a) = fail must be replaced by: g(state, a) = fail and state != 0
///    - [3] s <- g(state, a) must be replaced by: if g(state, a) != fail then s <- g(state, a) else s <-0
/// 2. It does not stores output keywords associated to states.
///    It rather reconstructs matching keywords by traversing the branch of the tree backward (see ACM_get_match).
/// 3. It permits to search for keywords even though all keywords have not been registered yet, and continue to register keywords afterwards.
///    To achieve this, failure states are reconstructed after any registration of a new keyword
///    (see ACM_register_keyword which alternates calls to algorithms 2 and 3.)
/// 4. This implemtation keeps track of the rank of a registered keyword as returned by ACM_get_match().
///    This can be used as a unique identifiant of a keyword for a given state machine.

#include <stdio.h>
#include <pthread.h>

#ifndef ACM_SYMBOL
// User defined file containing the definition of ACM_SYMBOL.
#include "aho_corasick_symbol.h"
#endif

#include "aho_corasick.h"

#ifdef PRIVATE_MODULE
#warning The Aho-Corasick algorithm is compiled as a private module (PRIVATE_MODULE is defined).
#endif

#ifdef ACM_ASSOCIATED_VALUE
#warning States are compiled with associated values (ACM_ASSOCIATED_VALUE is defined).
#endif

#define ASSERT(cond) do { if (!(cond)) {  \
      fprintf(stderr, "FATAL ERROR: !(%1$s) in function %2$s at %3$s:%4$i)\n", #cond, __func__, __FILE__, __LINE__);\
      pthread_exit(0) ;\
} } while (0)

#undef ACM_SYMBOL_EQ
#ifdef ACM_SYMBOL_EQ_OPERATOR
#ifndef PRIVATE_MODULE
extern int ACM_SYMBOL_EQ_OPERATOR (ACM_SYMBOL a, ACM_SYMBOL b);
#endif
static int (*__eqdefault) (ACM_SYMBOL a, ACM_SYMBOL b) = ACM_SYMBOL_EQ_OPERATOR;
#warning User defined equality function of symbols (ACM_SYMBOL_EQ_OPERATOR is defined).
#define ACM_SYMBOL_EQ(keyword_sign, text_sign) __eqdefault((keyword_sign), (text_sign))
#else
#define ACM_SYMBOL_EQ(keyword_sign, text_sign) ((keyword_sign) == (text_sign))
#endif

#undef ACM_SYMBOL_COPY
#ifdef ACM_SYMBOL_COPY_OPERATOR
static ACM_SYMBOL (*__copydefault) (ACM_SYMBOL a) = ACM_SYMBOL_COPY_OPERATOR;

#define ACM_SYMBOL_COPY(a) __copydefault(a)
#else
#define ACM_SYMBOL_COPY(a) (a)
#endif

#undef ACM_SYMBOL_DTOR
#ifdef ACM_SYMBOL_DTOR_OPERATOR
static void (*__dtordefault) (ACM_SYMBOL a) = ACM_SYMBOL_DTOR_OPERATOR;

#define ACM_SYMBOL_DTOR(a) __dtordefault(a)
#else
#define ACM_SYMBOL_DTOR(a)
#endif

/// A link between states
struct _ac_link
{
  ACM_SYMBOL letter;            // [a symbol]
  struct _ac_state *state;      // [g(s, letter)]
};

/// A state of the state machine.
/// @note Memory usage is of the order of the size of all registered keywords.
struct _ac_state                // [state s]
{
  struct _ac_link *goto_array;  // next states in the tree of the goto function
  size_t nb_goto;

  struct _ac_link *previous;    // previous state

  struct _ac_state *fail_state; // [f(s)]

  int is_matching;              // true if the state matches a keyword.
  size_t nb_sequence;           // number of matching keywords.
  size_t rank;                  // Rank of insertion of a keyword in the machine.

#ifdef ACM_ASSOCIATED_VALUE
  void *value;                  // An optional value associated to a state.
  void (*value_dtor) (void *);  // Destrcutor of the associated value, called a state machine release.
#endif
};

//-----------------------------------------------------
static struct _ac_state UNSET_STATE;

static struct _ac_state *
state_init (void)
{
  struct _ac_state *s = malloc (sizeof (*s));   /* [state s] */

  ASSERT (s);

  // [g(s, a) is undefined (= fail) for all input symbol a]
  s->goto_array = 0;
  s->nb_goto = 0;
  s->previous = 0;

  // Aho-Corasick Algorithm 2: "We assume output(s) is empty when state s is first created."
  s->nb_sequence = 0;           // number of outpur=ts in [output(s)]
  s->is_matching = 0;           // indicates the state is the last node of a registered keyword

  s->fail_state = &UNSET_STATE; // f(s) is undefined and has not been computed yet
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
state_goto_update (struct _ac_state *state_0, Keyword sequence /* a[1] a[2] ... a[n] */ )
#else
static int
state_goto_update (struct _ac_state *state_0, Keyword sequence  /* a[1] a[2] ... a[n] */
                   , void *value, void (*dtor) (void *))
#endif
{
  // Iterators
  // Aho-Corasick Algorithm 2: state <- 0
  struct _ac_state *state = state_0;    // [state 0]

  // Aho-Corasick Algorithm 2: j <- 1
  size_t j = 0;

  // Aho-Corasick Algorithm 2: while g(state, a[j]) != fail [and j <= m] do
  // Iterations on i and s until a final state
  for (; j < sequence.length /* [m] */ ;)
  {
    struct _ac_state *next = 0;

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
    ASSERT (state->goto_array = realloc (state->goto_array, sizeof (*state->goto_array) * state->nb_goto));

    // Creation of a new state
    // Aho-Corasick Algorithm 2: newstate <- newstate + 1
    struct _ac_state *newstate = state_init ();

    // Aho-Corasick Algorithm 2: g(state, a[p]) <- newstate
    state->goto_array[state->nb_goto - 1].state = newstate;
    state->goto_array[state->nb_goto - 1].letter = ACM_SYMBOL_COPY (sequence.letter[p]);

    // Backward link: previous(newstate, a[p]) <- state
    ASSERT (newstate->previous = malloc (sizeof (*newstate->previous)));
    newstate->previous->state = state;
    newstate->previous->letter = state->goto_array[state->nb_goto - 1].letter;

    // Aho-Corasick Algorithm 2: state <- newstate
    state = newstate;
  }

#ifdef ACM_ASSOCIATED_VALUE
  if (state->value && state->value_dtor)
    state->value_dtor (state->value);
  state->value = value;
  state->value_dtor = dtor;
#endif

  if (state->is_matching)
    // The keyword was already previously registered
    return 0;

  // Aho-Corasick Algorithm 2: output (state) <- { a[1] a[2] ... a[n] }
  // Aho-Corasick Algorithm 2: "We assume output(s) is empty when state s is first created."
  // Adding the sequence to the last found state (created or not)
  state->is_matching = 1;
  state->nb_sequence = 1;

  state->rank = state_0->rank++;        // rank is a 0-based index

  return 1;
}

static struct _ac_state *state_goto (struct _ac_state *state, ACM_SYMBOL letter);

static void
state_reset_output (struct _ac_state *state_0)
{
  size_t queue_length = 1;
  struct _ac_state **queue = malloc (sizeof (*queue));

  ASSERT (queue);

  // queue <- {0}
  queue[0] = state_0;

  size_t queue_read_pos = 0;

  while (queue_read_pos < queue_length)
  {
    // Let r be the next state in queue
    struct _ac_state *r = queue[queue_read_pos];

    // queue <- queue - {r}
    queue_read_pos++;

    ASSERT (queue = realloc (queue, sizeof (*queue) * (queue_length + r->nb_goto)));
    for (size_t i = 0; i < r->nb_goto; i++)
    {
      struct _ac_state *s = r->goto_array[i].state;     // [s <- g(r, a)]

      // queue <- queue U {s}
      queue_length++;
      queue[queue_length - 1] = s;
    }

    if (r->is_matching)
      r->nb_sequence = 1;       // Reset to original output from state_goto_update
    else
      r->nb_sequence = 0;
  }                             // while (queue_read_pos < queue_length)

  free (queue);
}

/// @see Aho-Corasick Algorithm 3: construction of the failure function.
static void
state_fail_state_construct (struct _ac_state *state_0 /* state 0 */ )
{
  state_reset_output (state_0);

  // Aho-Corasick Algorithm: "(except state 0 for which the failure function is not defined)."
  state_0->fail_state = 0;

  // Aho-Corasick Algorithm 3: queue <- empty
  // The first element in the queue will not be processed, therefore it can be added harmlessly.
  size_t queue_length = 0;
  struct _ac_state **queue = 0;

  ASSERT (queue = realloc (queue, sizeof (*queue) * (queue_length + state_0->nb_goto)));
  // Aho-Corasick Algorithm 3: for each a such that s != 0 [fail], where s <- g(0, a) do   [1]
  for (size_t i = 0; i < state_0->nb_goto; i++)
  {
    struct _ac_state *s = state_0->goto_array[i].state; // [s <- g(0, a)]

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
    struct _ac_state *r = queue[queue_read_pos];

    // Aho-Corasick Algorithm 3: queue <- queue - {r}
    queue_read_pos++;
    ASSERT (queue = realloc (queue, sizeof (*queue) * (queue_length + r->nb_goto)));
    // Aho-Corasick Algorithm 3: for each a such that s != fail, where s <- g(r, a)
    for (size_t i = 0; i < r->nb_goto; i++)
    {
      struct _ac_state *s = r->goto_array[i].state;     // [s <- g(r, a)]
      ACM_SYMBOL a = r->goto_array[i].letter;

      // Aho-Corasick Algorithm 3: queue <- queue U {s}
      queue_length++;
      queue[queue_length - 1] = s;

      // Aho-Corasick Algorithm 3: state <- f(r)
      struct _ac_state *state = r->fail_state /* f(r) */ ;

      // Aho-Corasick Algorithm 3: while g(state, a) = fail [and state != 0] do state <- f(state)        [2]
      //                           [if g(state, a) != fail then] f(s) <- g(state, a) [else f(s) <- 0]    [3]
      s->fail_state /* f(s) */  = state_goto (state, a);

      // Aho-Corasick Algorithm 3: output (s) <-output (s) U output (f(s))
      s->nb_sequence += s->fail_state->nb_sequence;
    }                           // for (size_t i = 0; i < r->nb_goto; i++)
  }                             // while (queue_read_pos < queue_length)

  free (queue);
}

#ifndef ACM_ASSOCIATED_VALUE
ACM_PRIVATE struct _ac_state *
ACM_register_keyword (struct _ac_state *state_0, Keyword y /* a[1] a[2] ... a[n] */ )
#else
ACM_PRIVATE struct _ac_state *
ACM_register_keyword (struct _ac_state *state_0, Keyword y      /* a[1] a[2] ... a[n] */
                      , void *value, void (*dtor) (void *))
#endif
{
  // Aho-Corasick Algorithm 2: newstate <- 0
  // Create state 0.
  // Executed only once, when 0 is passed as root argument
  if (!state_0)
    state_0 = state_init ();

#ifndef ACM_ASSOCIATED_VALUE
  if (!state_goto_update (state_0, y))
#else
  if (!state_goto_update (state_0, y, value, dtor))
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

  // Force reconstruction of fail_states on first call to ACM_change_state since a new keyword has been added.
  state_0->fail_state = &UNSET_STATE;

  return state_0;
}

ACM_PRIVATE size_t
ACM_nb_keywords (struct _ac_state * machine)
{
  return machine->rank;
}

ACM_PRIVATE void
ACM_release (struct _ac_state *state_0)
{
  size_t queue_length = 1;
  struct _ac_state **queue = malloc (sizeof (*queue));

  ASSERT (queue);

  // queue <- {0}
  queue[0] = state_0;

  size_t queue_read_pos = 0;

  while (queue_read_pos < queue_length)
  {
    // Let r be the next state in queue
    struct _ac_state *r = queue[queue_read_pos];

    // queue <- queue - {r}
    queue_read_pos++;

    ASSERT (queue = realloc (queue, sizeof (*queue) * (queue_length + r->nb_goto)));
    for (size_t i = 0; i < r->nb_goto; i++)
    {
      struct _ac_state *s = r->goto_array[i].state;     // [s <- g(r, a)]

      // queue <- queue U {s}
      queue_length++;
      queue[queue_length - 1] = s;
    }

    // Release r->goto_array
    free (r->goto_array);

    // Release previous
    free (r->previous);

#ifdef ACM_ASSOCIATED_VALUE
    if (r->value && r->value_dtor)
      r->value_dtor (r->value);
#endif

    // Release r
    ACM_SYMBOL_DTOR (r->letter);
    free (r);
  }

  free (queue);
}

/// @see Aho-Corasick Algorithm 1: Pattern matching machine - while loop.
static struct _ac_state *
state_goto (struct _ac_state *state, ACM_SYMBOL letter /* Aho-Corasick Algorithm 1: a[i] */ )
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
    //
    // Algorithms 1 cannot consider that g(0, a) never fails because propoerty LOOP_0 has not been implemented.
    // Therefore, for state 0, we must simulate the property LOOP_0, i.e state 0 must be returned,
    // as if g(0, a[i]) would have been set to state 0 if g(0, a[i]) = fail (property LOOP_0).
    // After Algorithm 3 has been processed, the only state for which f(state) = 0 is state 0.
    // [if g(state, a[i]) = fail and state = 0 then return state 0]
    // Aho-Corasick Algorithm: "(except state 0 for which the failure function is not defined)."
    if (state->fail_state == 0)
      return state;

    // From here, [state !=0]
    //
    // [if g(state, a[i]) = fail and state != 0 then state <- f(state)
    state = state->fail_state;
  }
}

ACM_PRIVATE struct _ac_state *
ACM_change_state (struct _ac_state *state, ACM_SYMBOL letter)
{
  // N.B.: In Aho-Corasick, algorithm 3 is executed after all sequences have been inserted
  //       in the goto graph one after the other by algorithm 2.
  //       As a slight enhancement: the fail state chains are rebuilted from scratch when needed,
  ///      i.e. if a keyword has been added since the last pattern maching serch.
  //       Therefore, algorithms 2 and 3 can be processed sequentially.
  //       but then, algorithm 3 must traverse the full goto graph every time a sequence has been added.
  if (state->fail_state == &UNSET_STATE)
    state_fail_state_construct (state);

  return state_goto (state, letter);
}

/// @see Aho-Corasick Algorithm 1: Pattern matching machine - if output (stat) != empty
ACM_PRIVATE size_t
ACM_nb_matches (const struct _ac_state * state)
{
  return state->nb_sequence;
}

/// @see Aho-Corasick Algorithm 1: Pattern matching machine - print output (state) [ith element]
#ifndef ACM_ASSOCIATED_VALUE
ACM_PRIVATE size_t
ACM_get_match (const struct _ac_state * state, size_t index, Keyword * match)
#else
ACM_PRIVATE size_t
ACM_get_match (const struct _ac_state * state, size_t index, Keyword * match, void **value)
#endif
{
  // Aho-Corasick Algorithm 1: if output(state) [ith element]
  if (index >= state->nb_sequence)
    return 0;

  size_t i = 0;

  for (; state; state = state->fail_state, i++ /* skip to the next failing state */ )
  {
    // Look for the first state in the "failing states" chain which matches a keyword.
    while (!state->is_matching && state->fail_state)
      state = state->fail_state;

    if (i == index)
      break;
  }

  // Aho-Corasick Algorithm 1: [print i]
  // Aho-Corasick Algorithm 1: print output(state) [ith element]
  // Reconstruct the matching keyword moving backward from the matching state to the state 0.
  match->length = 0;
  for (const struct _ac_state * s = state; s && s->previous; s = s->previous->state)
    match->length++;

  // Reallocation of match->letter. match->letter should be freed by the user after the last call to ACM_get_match on match.
  ASSERT (match->letter = realloc (match->letter, sizeof (*match->letter) * match->length));
  i = 0;
  for (const struct _ac_state * s = state; s && s->previous; s = s->previous->state)
  {
    match->letter[match->length - i - 1] = s->previous->letter;
    i++;
  }

#ifdef ACM_ASSOCIATED_VALUE
  if (value)
    *value = state->value;
#endif

  return state->rank;
}
