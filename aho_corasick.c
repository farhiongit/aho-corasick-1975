/* Copyright 2026 Laurent Farhi
 * Contact: lfarhi@sfr.fr
 *
 *  This file is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation, version 3 of the License.
 *
 *  This file is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this file.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "aho_corasick.h"
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>

#define ACM_ASSERT(cond, msg)                                                                       \
  do {                                                                                              \
    if (!(cond)) {                                                                                  \
      fprintf (stderr, "FATAL ERROR: A prerequisite is not fulfilled in function %s.\n", __func__); \
      fprintf (stderr, "             ");                                                            \
      if ((msg)[0] != 0)                                                                            \
        fprintf (stderr, "%s\n", ("" msg)); /* msg must be a static string */                       \
      else                                                                                          \
        fprintf (stderr, "The condition (%s) is false.\n", #cond);                                  \
      thrd_exit (EXIT_FAILURE);                                                                     \
    }                                                                                               \
  } while (0)

/* A state of the state machine. */
struct _ac_state /* [state s] */
{
  /* A link to the next states */
  struct {
    struct _ac_next {
      void *letter;            /* [a symbol] : transition to the next state */
      struct _ac_state *state; /* [g(s, letter)] : next state */
    } *array;                  /* next states in the tree of the goto function */
    size_t nb;                 /* size of array */
  } transitions;
  /* A link to the previous states */
  struct _prev {
    size_t i_letter; /* Index of the letter in the transitions.array */
    /* letter = previous.state->transitions.array[previous.i_letter].letter */
    struct _ac_state *state;
  } previous;                   /* Previous state (to build the outputs of the matches) */
  struct _ac_state *fail_state; /* [f(s)] is the continuation of the end of a keyword on the longest prefix of another branch */
  int is_matching;              /* true if the state matches a keyword. */
  size_t nb_outputs;            /* Number of matching keywords (Aho-Corasick : size (output (s)) */
  size_t rank;                  /* Rank (0-based) of insertion of a keyword in the machine. */
  struct _def {
    void *value;           /* An optional value associated to a keyword (is_matching == true). */
    void (*dtor) (void *); /* Destructor of the associated value, called at state machine release. */
  } definition;            /* Definition associated to the keyword. */
  ACMachine *machine;      /* Machine the state belongs to */
  size_t id;               /* state UID, for debugging purpose */
};

struct _ac_machine {
  struct _ac_state *state_0; /* state 0 */
  size_t nb_sequences;       /* Number of keywords in the machine. */
  size_t reconstruct;        /* Number of new keywords since the last fail-states (re)construction */
  size_t nb_states;          /* Number of states in the machine. */
  struct _ops {
    struct {
      EQ_TYPE op;
      void *arg;
    } eq; /* Equality operator between two letters */
    struct {
      DESTROY_TYPE op;
    } destroy; /* Destructor of a letter */
  } operators;
  mtx_t token;
};
//-----------------------------------------------------------------
static ACState *
state_create (void) {
  ACState *s = calloc (1, sizeof (*s)); /* [state s] */ // ACState allocation, all attributes set to 0
  ACM_ASSERT (s, "Out of memory.");
  /* [g(s, a) is undefined (= fail) for all input symbol a] : s->transitions.nb = 0, s->transitions.array = 0 */
  /* Aho-Corasick Algorithm 2: "We assume output(s) is empty when state s is first created." : s->nb_outputs = 0 */
  return s;
}

static ACState *
state_goto (ACState *state, void *letter /* a[i] */) {
  /* Aho-Corasick Algorithm 1: while g(state, a[i]) = fail [and state != 0] do state <- f(state)           [2] */
  /*                           [if g(state, a[i]) != fail then] state <- g(state, a[i]) [else state <- 0]  [3] */
  /*                           [The function returns state] */
  while (1) {
    /* [if g(state, a[i]) != fail then return g(state, a[i])] */
    struct _ac_next *p = state->transitions.array;
    struct _ac_next *end = p + state->transitions.nb;
    for (; p < end; p++)
      if (state->machine->operators.eq.op (p->letter, letter, state->machine->operators.eq.arg))
        return p->state;
    /* From here, [g(state, a[i]) = fail] */

    /* Algorithms 1 cannot consider that g(0, a) never fails because property LOOP_0 has not been implemented. */
    /* Therefore, for state 0, we must simulate the property LOOP_0, i.e state 0 must be returned, */
    /* as if g(0, a[i]) would have been set to state 0 if g(0, a[i]) = fail (property LOOP_0). */
    /* After Algorithm 3 has been processed, the only state for which f(state) = 0 is state 0. */
    /* [if g(state, a[i]) = fail and state = 0 then return state 0] */
    /* Aho-Corasick Algorithm: "(except state 0 for which the failure function is not defined)." */
    if (state == state->machine->state_0)
      return state; /* fail_state is never used for state_0. */
    /* From here, [state != 0] */

    /* [if g(state, a[i]) = fail and state != 0 then state <- f(state) */
    state = state->fail_state;
  }
}

static void
state_reset_output (ACState *r) {
  if (r->is_matching)
    r->nb_outputs = 1; /* Reset to original output (as in state_goto_update) */
  else
    r->nb_outputs = 0;
  struct _ac_next *p = r->transitions.array;
  struct _ac_next *end = p + r->transitions.nb;
  for (; p < end; p++)
    state_reset_output (p->state);
}

/* Aho-Corasick Algorithm 3: construction of the failure function and outputs. */
static void
state_fail_state_construct (ACMachine *machine) {
  ACState *state_0 = machine->state_0; /* [state 0] */
  /* Double-checked locking */
  if (!machine->reconstruct)
    return;
  ACM_ASSERT (mtx_lock (&machine->token) == thrd_success, "");
  if (!machine->reconstruct) {
    ACM_ASSERT (mtx_unlock (&machine->token) == thrd_success, "");
    return;
  }

  state_reset_output (state_0);
  /* Aho-Corasick Algorithm: "(except state 0 for which the failure function is not defined)." */
  /* Aho-Corasick Algorithm 3: queue <- empty */
  size_t queue_length = 0;
  ACState **queue = 0;
  ACM_ASSERT (queue = malloc (sizeof (*queue) * (machine->nb_states - 1)), "Out of memory.");
  /* Aho-Corasick Algorithm 3: for each a such that s != 0 [fail], where s <- g(0, a) do   [1] */
  struct _ac_next *p = state_0->transitions.array;
  struct _ac_next *end = p + state_0->transitions.nb;
  for (; p < end; p++) /* loop on state_0->transitions.array */
  {
    ACState *s = p->state; /* [for each a such that s != 0 [fail], where s <- g(0, a)] */
    /* Aho-Corasick Algorithm 3: queue <- queue U {s} */
    queue_length++;
    queue[queue_length - 1] = s; /* s */
    /* Aho-Corasick Algorithm 3: f(s) <- 0 */
    s->fail_state = state_0;
  } /* loop on state_0->transitions.array */
  size_t queue_read_pos = 0;
  /* Aho-Corasick Algorithm 3: while queue != empty do */
  while (queue_read_pos < queue_length) { // Breadth-first search
    /* Aho-Corasick Algorithm 3: let r be the next state in queue */
    ACState *r = queue[queue_read_pos];
    /* Aho-Corasick Algorithm 3: queue <- queue - {r} */
    queue_read_pos++;
    /* Aho-Corasick Algorithm 3: for each a such that s != fail, where s <- g(r, a) */
    p = r->transitions.array;
    end = p + r->transitions.nb;
    for (; p < end; p++) /* loop on r->transitions.array */
    {
      ACState *s = p->state; /* [s <- g(r, a)] */
      void *a = p->letter;
      /* Aho-Corasick Algorithm 3: queue <- queue U {s} */
      queue_length++;
      queue[queue_length - 1] = s;
      /* Aho-Corasick Algorithm 3: state <- f(r) */
      ACState *state = r->fail_state; /* f(r) */
      /* Aho-Corasick Algorithm 3: while g(state, a) = fail [and state != 0] do state <- f(state)        [2] */
      /*                           [if g(state, a) != fail then] f(s) <- g(state, a) [else f(s) <- 0]    [3] */
      s->fail_state /* f(s) */ = state_goto (state, a);
      /* Aho-Corasick Algorithm 3: output (s) <-output (s) U output (f(s)) */
      s->nb_outputs += s->fail_state->nb_outputs;
    } /* loop on r->transitions.array */
  } /* while (queue_read_pos < queue_length) */
  free (queue);
  machine->reconstruct = 0;
  ACM_ASSERT (mtx_unlock (&machine->token) == thrd_success, "");
}
//-----------------------------------------------------------------
void
acm_matcher_init (MatchHolder *matcher) {
  ACM_ASSERT (matcher, "");
  *matcher = (MatchHolder){ 0 };
}

void
acm_matcher_release (MatchHolder *matcher) {
  ACM_ASSERT (matcher, "");
  free (matcher->letters);
  acm_matcher_init (matcher);
}

/* Aho-Corasick Algorithm 1: Pattern matching machine - if output (state) != empty */
size_t
acm_match (ACState **state, void *letter) {
  /* N.B.: In Aho-Corasick, algorithm 3 is executed after all keywords have been inserted */
  /*       in the goto graph one after the other by algorithm 2. */
  /*       As a slight enhancement: the fail state chains are rebuilt from scratch when needed, */
  /*       i.e. if a keyword has been added since the last pattern maching search. */
  /*       Therefore, algorithms 2 and 3 can be processed alternately. */
  /*       (algorithm 3 will traverse the full goto graph after a keyword has been added.) */
  ACM_ASSERT (state && *state && letter, "");
  ACMachine *machine = (*state)->machine;
  state_fail_state_construct (machine);
  return (*state = state_goto (*state, letter))->nb_outputs;
}

/* Aho-Corasick Algorithm 1: Pattern matching machine - print output (state) [ith element] */
size_t
acm_get_match (const ACState *state, size_t index, MatchHolder *matcher, void **value) {
  /* Aho-Corasick Algorithm 1: if output(state) [ith element] */
  ACM_ASSERT (state, "");
  ACM_ASSERT (state != state->machine->state_0, "acm_match should be called first.");
  ACM_ASSERT (index < state->nb_outputs, "Index out of bounds.");
  size_t i = 0;
  // Starting from state, search for the index-th matching keyword in the "failing states" chain:
  for (; state; state = state->fail_state, i++ /* skip to the next failing state (next matching keyword) */) {
    /* state != state->machine->state_0, since index < state->nb_outputs */
    /* Look for the first state in the "failing states" chain which matches a keyword. */
    while (!state->is_matching)
      state = state->fail_state;
    if (i == index)
      break;
  }
  /* Argument match could be passed to 0 if only value or rank is needed. */
  if (matcher) {
    /* Aho-Corasick Algorithm 1: [print i] */
    /* Aho-Corasick Algorithm 1: print output(state) [ith element] */
    /* Reconstruct the matching keyword moving backward from the matching state to the state 0. */
    matcher->length = 0;
    for (const ACState *s = state; s && s->previous.state; s = s->previous.state)
      matcher->length++;
    /* Reallocation of match->letter. match->letter should be freed by the user after the last call to ACM_get_match on match. */
    ACM_ASSERT (matcher->letters = realloc (matcher->letters, matcher->length * sizeof (*matcher->letters)), "Out of memory.");
    i = 0;
    for (const ACState *s = state; s && s->previous.state; s = s->previous.state) {
      matcher->letters[matcher->length - i - 1] = s->previous.state->transitions.array[s->previous.i_letter].letter;
      i++;
    }
    matcher->rank = state->rank;
  }
  /* Argument value could passed to 0 if the associated value is not needed. */
  if (value)
    *value = state->definition.value;
  return state->rank;
}
//-----------------------------------------------------------------
static int
acm_eq_default (const void *a, const void *b, void *eq_arg) {
  return !memcmp (a, b, *(size_t *)eq_arg);
}
EQ_TYPE ACM_EQ_DEFAULT = acm_eq_default;

ACMachine *
acm_create (EQ_TYPE eq, void *eq_arg, DESTROY_TYPE dtor) {
  ACM_ASSERT (eq, "An equality operator should be provided.");
  ACMachine *machine = calloc (1, sizeof (*machine)); // ACMachine allocation, all attributes set to 0.
  ACM_ASSERT (machine, "Out of memory.");
  /* Aho-Corasick Algorithm 2: newstate <- 0 */
  /* Create state 0. */
  machine->nb_states = 1;
  (machine->state_0 = state_create ())->machine = machine;
  machine->operators = (struct _ops){ .eq = { .op = eq, .arg = eq_arg }, .destroy = { .op = dtor } };
  ACM_ASSERT (mtx_init (&machine->token, mtx_plain) == thrd_success, "");
  return machine;
}

ACState *
acm_initiate (ACMachine *machine) {
  ACM_ASSERT (machine, "");
  return machine->state_0;
}

/* Aho-Corasick Algorithm 2: for all a such that g(0, a) = fail do g(0, a) <- 0 */
/* This statement is aimed to set the following property (here called the Aho-Corasick LOOP_0 property): */
/*   "All our pattern matching machines have the property that g(0, l) != fail for all input symbol l. */
/*    [...] this property of the goto function [g] on state 0 [root] ensures that one input symbol will be processed */
/*    by the machine in every machine cycle [state_goto]." */
/*   "We add a loop from state 0 to state 0 on all input symbols other than [the symbols l for which g(0, l) is already defined]. */

/* N.B.: This property is *NOT* implemented in this code after calls to enter(y[i]) because */
/*       it requires that the alphabet of all possible symbols is known in advance. */
/*       This would kill the genericity of the code. */
/*       Therefore, Algorithms 1, 3 and 4 *CANNOT* consider that g(0, l) never fails for any symbol l. */
/*       g(0, l) can fail like any other state transition. */
/*       Thus, the implementation slightly differs from the one proposed by Aho-Corasick. */

/* Aho-Corasick Algorithm 2: construction of the goto function - procedure enter(a[1] a[2] ... a[n]). */
/* Aho-Corasick Algorithm 2: state <- 0 */

/* Should first be called with: */
/* Aho-Corasick Algorithm 2: state <- 0 */
/* Aho-Corasick Algorithm 2: j <- 1 */
/* Aho-Corasick Algorithm 2: while g(state, a[j]) != fail [and j <= m] do */
/* Iterations on i and s until a final state */
void
acm_insert_letter_of_keyword (ACState **state, void *letter) {
  ACM_ASSERT (state && *state && letter, "");
  ACMachine *machine = (*state)->machine;
  ACM_ASSERT (mtx_lock (&machine->token) == thrd_success, "");
  ACState *next = 0;
  /* Aho-Corasick Algorithm 2: "g(s, l) = fail if l is undefined or if g(s, l) has not been defined." */
  /* Loop on all symbols a for which g(state, a) is defined. */
  struct _ac_next *p = (*state)->transitions.array;
  struct _ac_next *end = p + (*state)->transitions.nb;
  for (; p < end; p++)
    if (machine->operators.eq.op (p->letter, letter, machine->operators.eq.arg)) {
      /* [if g(state, a[j]) is defined] */
      next = p->state;
      break;
    }
  /* [if g(state, a[j]) is defined (!= fail)] */
  if (next) { // the letter is already in the machine and need not be inserted twice.
    /* Aho-Corasick Algorithm 2: state <- g(state, a[j]) */
    *state = next;
    /* Aho-Corasick Algorithm 2: j <- j + 1 */
    if (machine->operators.destroy.op)
      machine->operators.destroy.op ((void *)letter); // letter deallocation
    ACM_ASSERT (mtx_unlock (&machine->token) == thrd_success, "");
    return;
  }

  /* [g(state, a[j]) is not defined (= fail)] */
  /* Aho-Corasick Algorithm 2: for p <- j until m do */
  /* Appending states for the new sequence to the final state found */
  (*state)->transitions.nb++;
  ACM_ASSERT ((*state)->transitions.array = realloc ((*state)->transitions.array, sizeof (*(*state)->transitions.array) * (*state)->transitions.nb), "Out of memory.");
  /* Creation of a new state */
  /* Aho-Corasick Algorithm 2: newstate <- newstate + 1 */
  ACState *newstate = state_create ();
  newstate->machine = machine;
  newstate->id = machine->nb_states; /* state UID */
  /* Aho-Corasick Algorithm 2: g(state, a[p]) <- newstate */
  (*state)->transitions.array[(*state)->transitions.nb - 1] = (struct _ac_next){ .state = newstate, .letter = letter };
  /* Backward link: previous(newstate, a[p]) <- state */
  /* state->transitions.array[state->transitions.nb - 1].state->previous.i_letter = state->transitions.nb - 1; */
  newstate->previous = (struct _prev){ .state = (*state), .i_letter = (*state)->transitions.nb - 1 };

  /* Aho-Corasick Algorithm 2: state <- newstate */
  *state = newstate;
  machine->nb_states++;
  ACM_ASSERT (mtx_unlock (&machine->token) == thrd_success, "");
}

void *
acm_insert_end_of_keyword (ACState **state, void *value, void (*dtor) (void *)) {
  ACM_ASSERT (state && *state, "");
  ACMachine *machine = (*state)->machine;
  ACM_ASSERT (mtx_lock (&machine->token) == thrd_success, "");
  ACM_ASSERT (*state != machine->state_0, "acm_insert_letter_of_keyword should be called first.");
  if (!(*state)->is_matching) {
    /* Aho-Corasick Algorithm 2: output (state) <- { a[1] a[2] ... a[n] } */
    /* Aho-Corasick Algorithm 2: "We assume output(s) is empty when state s is first created." */
    /* Adding the sequence to the last found state (created or not) */
    (*state)->is_matching = 1; // The state is the last node of a registered keyword.
    (*state)->nb_outputs = 1;
    (*state)->rank = machine->nb_sequences++; // rank is a 0-based index.
    if (!++machine->reconstruct)              // Manage overflow.
      machine->reconstruct = 1;               // f(s) must be recomputed.
  }
  /* Set the keyword associated value. */
  void *ret = (*state)->definition.value; // Previously associated value.
  if ((*state)->definition.value == 0)
    (*state)->definition = (struct _def){ .value = value, .dtor = dtor };
  *state = machine->state_0; /* [state 0] */
  ACM_ASSERT (mtx_unlock (&machine->token) == thrd_success, "");
  return ret;
}

static void
state_release (ACState *state, DESTROY_TYPE dtor) {
  /* Release transitions.array */
  struct _ac_next *p = state->transitions.array;
  struct _ac_next *end = p + state->transitions.nb;
  for (; p < end; p++) {
    state_release (p->state, dtor);
    if (dtor)
      dtor ((void *)p->letter); // letter deallocation
  }
  free (state->transitions.array);
  /* Release associated value */
  if (state->definition.dtor)
    state->definition.dtor (state->definition.value);
  /* Release state */
  free ((ACState *)state);
}

void
acm_release (ACMachine *machine) {
  ACM_ASSERT (machine, "");
  state_release (machine->state_0, machine->operators.destroy.op);
  mtx_destroy (&machine->token);
  free ((ACMachine *)machine);
}
//-----------------------------------------------------------------
size_t
acm_nb_keywords (ACMachine *machine) {
  ACM_ASSERT (machine, "");
  return machine->nb_sequences;
}

static void
foreach_keyword (const ACState *state, void ***letters, size_t *max_length, size_t depth, void (*operator) (MatchHolder, void *)) {
  if (state->is_matching && depth) {
    MatchHolder k = { .letters = *letters, .length = depth, .rank = state->rank };
    (*operator) (k, state->definition.value);
  }
  if (state->transitions.nb && depth >= *max_length) {
    *max_length = depth + 1;
    *letters = realloc (*letters, *max_length * sizeof (**letters)); // letters allocation
    ACM_ASSERT (*letters, "Out of memory.");
  }
  struct _ac_next *p = state->transitions.array;
  struct _ac_next *end = p + state->transitions.nb;
  for (; p < end; p++) {
    (*letters)[depth] = p->letter;
    foreach_keyword (p->state, letters, max_length, depth + 1, operator);
  }
}

void
acm_foreach_keyword (ACMachine *machine, void (*operator) (MatchHolder, void *)) {
  ACM_ASSERT (machine, "");
  if (!operator)
    return;
  ACState *state_0 = machine->state_0; /* [state 0] */
  void **letters = 0;
  size_t max_length = 0;
  foreach_keyword (state_0, &letters, &max_length, 0, operator);
  free (letters); // letters deallocation
}
//-----------------------------------------------------------------
static void
state_print (ACState *state, FILE *stream, int indent, size_t id_state, PRINT_TYPE printer) {
  static size_t nb_states;
  static int cur_pos;
  for (size_t i = 0; i < state->transitions.nb; i++) {
    if (indent < cur_pos) {
      cur_pos = 0;
      fprintf (stream, "\n");
      if (indent) {
        for (int t = 0; t < indent - 1; t++)
          cur_pos += fprintf (stream, " ");
        cur_pos += fprintf (stream, "L");
      }
    } else if (indent > cur_pos)
      for (int t = 0; t < indent - cur_pos; t++)
        cur_pos += fprintf (stream, " ");
    if (state == state->machine->state_0)
      cur_pos += fprintf (stream, "(%03zu)", id_state);
    cur_pos += fprintf (stream, "---");
    if (printer)
      cur_pos += printer (stream, state->transitions.array[i].letter);
    cur_pos += fprintf (stream, "-->");
    nb_states++;
    /* cur_pos += fprintf (stream, "%03zu", nb_states); */
    cur_pos += fprintf (stream, "(%03zu)", state->transitions.array[i].state->id);
    if (state->transitions.array[i].state->is_matching)
      cur_pos += fprintf (stream, "[#%zu]", state->transitions.array[i].state->rank);
    if (state->transitions.array[i].state->fail_state != state->machine->state_0)
      cur_pos += fprintf (stream, "(-->%03zu)", state->transitions.array[i].state->fail_state->id);
    state_print (state->transitions.array[i].state, stream, cur_pos, nb_states, printer);
  }
}

void
acm_print (ACMachine *machine, FILE *stream, PRINT_TYPE printer) {
  ACM_ASSERT (machine, "");
  state_fail_state_construct (machine);
  if (stream) {
    fprintf (stream, "\n");
    state_print (machine->state_0, stream, 0, 0, printer);
    fprintf (stream, "\n");
  }
}
