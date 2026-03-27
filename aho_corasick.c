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
#include "map.h"
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>

#define ACM_ASSERT(cond, msg)                                                                       \
  do {                                                                                              \
    if (!(cond)) {                                                                                  \
      fflush (stdout);                                                                              \
      fprintf (stderr, "FATAL ERROR: A prerequisite is not fulfilled in function %s.\n", __func__); \
      fprintf (stderr, "             ");                                                            \
      if ((msg)[0] != 0)                                                                            \
        fprintf (stderr, "%s\n", ("" msg)); /* msg must be a static string */                       \
      else                                                                                          \
        fprintf (stderr, "The condition (%s) is false (%s:%i).\n", #cond, __FILE__, __LINE__);      \
      thrd_exit (EXIT_FAILURE);                                                                     \
    }                                                                                               \
  } while (0)
//-----------------------------------------------------------------
/* A link to a next states */
struct _ac_transition {
  void *letter;            /* [a symbol] : transition to the next state */
  struct _ac_state *state; /* [g(s, letter)] : next state, allocated */
};
/* A state of the state machine. */
struct _ac_state /* [state s] */
{
  /* Transitions to the next states */
  struct _ac_transitions {
    struct _ac_transition value;  /* Next state */
    struct _ac_transitions *next; /* Linked list (stable on insertion, unordered on letter, searchable on letter) of transitions from a state */
  } *transitions;                 /* Next states in the tree of the goto function */
  /* A link to the previous states */
  struct _prev {
    const void *letter;               /* [a symbol] : transition from the previous state */
    const struct _ac_state *state;    /* Reference to the previous state */
  } previous;                         /* Previous state (used for building the outputs of the matches) */
  const struct _ac_state *fail_state; /* [f(s)] is a reference to a state (on another branch) which is the continuation of a keyword with the longest prefix */
  int is_end_of_keyword;              /* true if the state matches a keyword. */
  size_t nb_outputs;                  /* Number of matching keywords (Aho-Corasick : size (output (s)) */
  struct _def {
    void *value;           /* An optional value associated to a keyword (is_end_of_keyword == true). */
    void (*dtor) (void *); /* Destructor of the associated value, called at state machine release. */
  } definition;            /* Definition associated to the keyword. */
  ACMachine *machine;      /* Machine the state belongs to */
  size_t id;               /* state UID, for debugging purpose */
#ifndef NMEYER_85
  map * /* of ACState* */ inverse_fail_states; /* Meyer 85: " record IF of the inverse function of F" */
#endif
};

struct _ac_machine {
  struct _ac_state *state_0; /* state 0 */
  size_t nb_sequences;       /* Number of keywords in the machine. */
  size_t reconstruct;        /* Number of new keywords since the last fail-states (re)construction */
  size_t nb_states;          /* Number of states in the machine. */
  struct _ops {
    struct {
      EQ_TYPE f;
      void *arg;
    } eq; /* Equality operator between two letters */
    struct {
      DESTROY_TYPE f;
    } destroy; /* Destructor of a letter */
  } operators;
  mtx_t token;
};
//-----------------------------------------------------------------
static ACState *
state_create (void) {
  ACState *s = calloc (1, sizeof (*s)); // [state s] ACState allocation, all attributes set to 0
  ACM_ASSERT (s, "Out of memory.");
  /* [g(s, a) is undefined (= fail) for all input symbol a] : s->transitions = 0 */
  /* Aho-Corasick Algorithm 2: "We assume output(s) is empty when state s is first created." : s->nb_outputs = 0 */
#ifndef NMEYER_85
  static const size_t inverse_fail_states_size = sizeof (ACState *);                                                     // Static persistent value.
  ACM_ASSERT (s->inverse_fail_states = map_create (0, MAP_GENERIC_CMP, &inverse_fail_states_size, 1), "Out of memory."); // s->inverse_fail_states will manage pointers to ACState.
#endif
  return s;
}

static void
state_release (ACState *state) {
  /* Release transitions and letters */
  struct _ac_transitions *n;
  for (struct _ac_transitions *p = state->transitions; p; p = n) {
    n = p->next;
    state_release (p->value.state);
    if (state->machine->operators.destroy.f)
      state->machine->operators.destroy.f (p->value.letter); // letter deallocation
    free (p);
  }
  /* Release associated value */
  if (state->definition.dtor)
    state->definition.dtor (state->definition.value);
  /* Release state */
#ifndef NMEYER_85
  map_traverse (state->inverse_fail_states, MAP_REMOVE_ALL, 0, 0, 0);
  map_destroy (state->inverse_fail_states);
#endif
  free (state);
}
//-----------------------------------------------------------------
static int
eq_default (const void *a, const void *b, const void *eq_arg) {
  return !memcmp (a, b, *(const size_t *)eq_arg);
}
const EQ_TYPE ACM_EQ_DEFAULT = eq_default;

ACMachine *
acm_create (EQ_TYPE eq, void *eq_arg, DESTROY_TYPE dtor) {
  ACM_ASSERT (eq, "An equality operator should be provided.");
  ACMachine *machine = calloc (1, sizeof (*machine)); // ACMachine allocation, all attributes set to 0.
  ACM_ASSERT (machine, "Out of memory.");
  /* Aho-Corasick Algorithm 2: newstate <- 0 */
  /* Create state 0. */
  machine->operators = (struct _ops){ .eq = { .f = eq, .arg = eq_arg }, .destroy = { .f = dtor } };
  (machine->state_0 = state_create ())->machine = machine;
  machine->nb_states++;
  ACM_ASSERT (mtx_init (&machine->token, mtx_plain) == thrd_success, "Out of memory.");
  return machine;
}

void
acm_release (ACMachine *machine) {
  ACM_ASSERT (machine, "Invalid null machine.");
  state_release (machine->state_0);
  mtx_destroy (&machine->token);
  free (machine);
}
//-----------------------------------------------------------------
ACState *
acm_initiate (ACMachine *machine) {
  ACM_ASSERT (machine, "Invalid null machine.");
  return machine->state_0;
}

static const ACState *
state_goto (const ACState *state, const void *letter /* a[i] */) {
  /* Aho-Corasick Algorithm 1: while g(state, a[i]) = fail [and state != 0] do state <- f(state)           [2] */
  /*                           [if g(state, a[i]) != fail then] state <- g(state, a[i]) [else state <- 0]  [3] */
  /*                           [The function returns state] */
  while (1) {
    /* [if g(state, a[i]) != fail then return g(state, a[i])] */
    for (struct _ac_transitions *p = state->transitions; p; p = p->next)
      if (state->machine->operators.eq.f (p->value.letter, letter, state->machine->operators.eq.arg))
        return p->value.state;
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
complete_fail_state (ACState *r /* the previous state of s */, ACState *s, const void *a) {
  ACM_ASSERT (r->fail_state || r == r->machine->state_0, "Undefined fail state.");
  ACM_ASSERT (s != s->machine->state_0, "Incorrect state.");
  if (r->fail_state) /* fail_state is defined --> r is not state 0 : compute f(s) from f(r) */
    /* Aho-Corasick Algorithm 3: state <- f(r) */
    /* Aho-Corasick Algorithm 3: while g(state, a) = fail [and state != 0] do state <- f(state)        [2] */
    /*                           [if g(state, a) != fail then] f(s) <- g(state, a) [else f(s) <- 0]    [3] */
    s->fail_state /* f(s) */ = state_goto (r->fail_state /* f(r) */, a); /* g(f(r), a) */
  else                                                                   /* fail_state is not defined --> r is state 0: [for each a such that s != 0 [fail], where s <- g(0, a)] */
    /* Aho-Corasick Algorithm 3: f(s) <- 0 */
    s->fail_state = r; /* f(s) = state 0 */ /* /!\ Meyer 85 does not consider this case (Complete_failure does not work for state 0) */
  /* Aho-Corasick Algorithm 3: output (s) <-output (s) U output (f(s)) */
  s->nb_outputs += s->fail_state->nb_outputs;
}

#ifndef NMEYER_85
static int
complete_inverse_one_ifs (void *data, void *op_arg, int *remove) {
  /* Meyer 85 : "Recursive precondition: n' is a suffix of nc and T[bA, c] is undefined for any proper suffix bA of n (where n' = Ac)
                 Record n' as new failure value for all x' such that nc = longest proper suffix of x'." */
  (void)op_arg;
  (void)remove;
  ACState *x = *(ACState **)data; // map of ACState *
  struct _ac_transition *transition = op_arg /* n', c */;

  ACState **xprime = 0;
  for (struct _ac_transitions *pt = x->transitions /* T[x, ...] */; pt /* T[x, letter */; pt = pt->next) // For each transition labelled letter in T[x, ...]
    if (x->machine->operators.eq.f (pt->value.letter, transition->letter /* c */, x->machine->operators.eq.arg)) /* it exists T[x, letter] for which c = letter --> T[x, c] is defined */ {
      xprime = &pt->value.state; // x' = T[x, c] (&pt->value.state is persistent and stable)
      break;
    }

  if (xprime) {
    // Remove previous failure value of x'.
    ACM_ASSERT (map_find_key ((*xprime)->fail_state->inverse_fail_states, xprime, MAP_REMOVE_ALL, 0, 0, 0) == 1, ""); // IF[f[x'] = IF[f[x'] - {x'}
    // Install new one [failure value].
    (*xprime)->fail_state = transition->state;                                                  // f[x'] = n'
    ACM_ASSERT (map_insert_data (transition->state /* n' */->inverse_fail_states, xprime), ""); // IF[n'] = IF[n'] + {x'}
  } else                                                                                        /* There is no T[x, letter] for which c = letter --> T[x, c] is undefined */
    /* Meyer 85 : "[...] inductively [...] all proper suffixes of n are accessible through f. */
    /*             enter_child becomes recursive. */
    // Complete_inverse (x, n', c) : for y in IF[x], do complete_inverse_one_ifs (y, n', c)
    map_traverse (/* for y in IF[x], x = lps(y) */ x->inverse_fail_states, complete_inverse_one_ifs, transition /* n', c */, 0, 0); // Try recursively with nodes having x as proper suffix.

  return 1;
}
#endif
//-----------------------------------------------------------------
static ACState *
enter_child (ACState *n, void *c) {
  /* Creation of a new state */
  ACState *nprime = state_create (); // New state
  struct _ac_transition *pt = 0;
  struct _ac_transitions *tr = calloc (1, sizeof (*tr));
  ACM_ASSERT (tr, "Out of memory.");
  /* Aho-Corasick Algorithm 2: g(n, a[p]) <- nprime */
  *tr = (struct _ac_transitions){ .value = { .state = nprime, .letter = c }, .next = n->transitions }; // *c is supposed to be persistent until acm_release is called.
  n->transitions = tr;
  pt = &n->transitions->value; /* persistent and steady */
  /* Backward link: previous(nprime, a[p]) <- n */
  nprime->previous = (struct _prev){ .state = n, .letter = c };
  nprime->machine = n->machine;
  nprime->id = n->machine->nb_states; /* state UID */
  n->machine->nb_states++;
  (void)pt;
#ifndef NMEYER_85
  /* T[n, c] = n' */
  /* Meyer 85 : "Build the f function incrementally, as new strings are entered into T.
                 [...] the longest proper suffix of n' = T[n, c] in the tree must be of the form T[m, c] for some proper suffix m of n,
                 When a new node n' = T[n, c] is entered, the value of f[x'] must be changed for some existing nodes if and only if n' is the longest proper suffix of the string associated x'.
                 [...] we must keep a record IF of the inverse function of f." */
  complete_fail_state (n, nprime, c); // Compute f[n']
  ACM_ASSERT (pt && nprime->fail_state && nprime->fail_state->inverse_fail_states, "");
  ACM_ASSERT (map_insert_data (nprime->fail_state->inverse_fail_states, &pt->state), ""); // Update IF for m' = f[n']
  // Complete_inverse (n, nprime, c) : for x in IF[n], do complete_inverse_one_ifs (x, n', c)
  // Meyer 85 : "Compute IF[n'] and change to n' the corresponding values of f."
  map_traverse (/* for y in IF[n], n = lps (y) */ n->inverse_fail_states, complete_inverse_one_ifs, pt /* n', c */, 0, 0);
#endif
  return nprime;
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
  ACM_ASSERT (state && *state && letter, "Invalid null state or letter.");
  ACMachine *machine = (*state)->machine;
  ACM_ASSERT (mtx_lock (&machine->token) == thrd_success, "");
  ACState *next = 0;
  /* Aho-Corasick Algorithm 2: "g(s, l) = fail if l is undefined or if g(s, l) has not been defined." */
  /* Loop on all symbols a for which g(state, a) is defined. */
  for (struct _ac_transitions *p = (*state)->transitions; p; p = p->next)
    if (machine->operators.eq.f (p->value.letter, letter, machine->operators.eq.arg)) {
      /* [if g(state, a[j]) is defined] */
      next = p->value.state;
      break;
    }
  /* [if g(state, a[j]) is defined (!= fail)] */
  if (next) { // the letter is already in the machine and need not be inserted twice.
    /* Aho-Corasick Algorithm 2: state <- g(state, a[j]) */
    *state = next;
    /* Aho-Corasick Algorithm 2: j <- j + 1 */
    if (machine->operators.destroy.f)
      machine->operators.destroy.f (letter); // letter deallocation
    ACM_ASSERT (mtx_unlock (&machine->token) == thrd_success, "");
    return;
  }

  /* [g(state, a[j]) is not defined (= fail)] */
  /* Aho-Corasick Algorithm 2: for p <- j until m do */
  /* Appending states for the new sequence to the final state found */
  ACState *newstate = enter_child ((*state), letter); // T[state, letter] = newstate

  /* Aho-Corasick Algorithm 2: state <- newstate */
  /* Aho-Corasick Algorithm 2: newstate <- newstate + 1 */
  *state = newstate;
  ACM_ASSERT (mtx_unlock (&machine->token) == thrd_success, "");
}
//-----------------------------------------------------------------
#ifndef NMEYER_85
static void enter_output (ACState *n);
static int
enter_output_one_ifs (void *x, void *op_arg, int *remove) {
  (void)op_arg;
  (void)remove;
  /* Meyer 85 : "enter_output becomes recursive. */
  enter_output (*(ACState **)x);
  return 1;
}
#endif

static void
enter_output (ACState *n) {
  n->nb_outputs += 1;
#ifndef NMEYER_85
  /* Meyer 85 : "Add output to the output set of any node which has n as a suffix. */
  /* for x in IF[n] loop enter_output (x) */
  map_traverse (n->inverse_fail_states, enter_output_one_ifs, 0, 0, 0);
#endif
}

void *
acm_insert_end_of_keyword (ACState **state, void *value, void (*dtor) (void *)) {
  ACM_ASSERT (state && *state, "Invalid null state.");
  ACMachine *machine = (*state)->machine;
  ACM_ASSERT (mtx_lock (&machine->token) == thrd_success, "");
  ACM_ASSERT (*state != machine->state_0, "acm_insert_letter_of_keyword should be called first.");
  if (!(*state)->is_end_of_keyword) {
    /* Aho-Corasick Algorithm 2: output (state) <- { a[1] a[2] ... a[n] } */
    /* Aho-Corasick Algorithm 2: "We assume output(s) is empty when state s is first created." */
    /* Adding the sequence to the last found state (created or not) */
    enter_output (*state);
    (*state)->is_end_of_keyword = 1; // The state is the last node of a registered keyword.
    machine->nb_sequences++;         // Number of keywords in the machine.
    if (!++machine->reconstruct)     // Manage overflow.
      machine->reconstruct = 1;      // f(s) must be recomputed.
  }
  /* Set the keyword associated value. */
  void *ret = (*state)->definition.value; // Previously associated value.
  if ((*state)->definition.value == 0)
    (*state)->definition = (struct _def){ .value = value, .dtor = dtor };
  *state = machine->state_0; /* [state 0], get ready for a new keyword to be inserted. */
  ACM_ASSERT (mtx_unlock (&machine->token) == thrd_success, "");
  return ret;
}
//-----------------------------------------------------------------
#ifdef NMEYER_85
/* Aho-Corasick Algorithm 3: construction of the failure function and outputs using a FIFO  and a BFS algorithm (re-entrant). */
static void
state_fail_state_construct (ACMachine *machine) {
  /* Double-checked locking */
  if (!machine->reconstruct)
    return;
  ACM_ASSERT (mtx_lock (&machine->token) == thrd_success, "");
  if (!machine->reconstruct) {
    ACM_ASSERT (mtx_unlock (&machine->token) == thrd_success, "");
    return;
  }

  /* Aho-Corasick Algorithm: "(except state 0 for which the failure function is not defined)." */
  /* Aho-Corasick Algorithm 3: queue <- empty */
  size_t queue_length = 1;
  ACState **queue = 0;
  ACM_ASSERT (queue = malloc (sizeof (*queue) * machine->nb_states), "Out of memory.");
  queue[0] = machine->state_0; /* [state 0] */
  size_t queue_read_pos = 0;
  /* Aho-Corasick Algorithm 3: while queue != empty do */
  while (queue_read_pos < queue_length) { // Breadth-first search
    /* Aho-Corasick Algorithm 3: let r be the next state in queue */
    ACState *r = queue[queue_read_pos];
    /* Aho-Corasick Algorithm 3: queue <- queue - {r} */
    queue_read_pos++;
    /* Aho-Corasick Algorithm 3: for each a such that s != 0 [fail], where s <- g(0, a) do   [1] */
    /* Aho-Corasick Algorithm 3: for each a such that s != fail, where s <- g(r, a) */
    for (struct _ac_transition *p = r->transitions; p; p = p->next) /* loop on transitions */
    {
      ACState *s = p->value.state; /* [s <- g(r, a)] */
      void *a = p->value.letter;
      /* Aho-Corasick Algorithm 3: queue <- queue U {s} */
      queue_length++;
      queue[queue_length - 1] = s; /* s */
      s->nb_outputs = s->is_end_of_keyword ? 1 /* Re-entrant: reset to original output (as in acm_insert_end_of_keyword) */ : 0;
      complete_fail_state (r, s, a);
    }
  } /* while (queue_read_pos < queue_length) */
  free (queue);
  machine->reconstruct = 0;
  ACM_ASSERT (mtx_unlock (&machine->token) == thrd_success, "");
}
#endif
//-----------------------------------------------------------------
void
acm_matcher_init (MatchHolder *matcher) {
  ACM_ASSERT (matcher, "Invalid null matcher.");
  *matcher = (MatchHolder){ 0 };
}

void
acm_matcher_release (MatchHolder *matcher) {
  ACM_ASSERT (matcher, "Invalid null matcher.");
  free (matcher->letters);
  acm_matcher_init (matcher);
}

/* Aho-Corasick Algorithm 1: Pattern matching machine - if output (state) != empty */
size_t
acm_match (const ACState **state, const void *letter) {
  /* N.B.: In Aho-Corasick, algorithm 3 is executed after all keywords have been inserted */
  /*       in the goto graph one after the other by algorithm 2. */
  /*       As a slight enhancement: the fail state chains are rebuilt from scratch when needed, */
  /*       i.e. if a keyword has been added since the last pattern maching search. */
  /*       Therefore, algorithms 2 and 3 can be processed alternately. */
  /*       (algorithm 3 will traverse the full goto graph after a keyword has been added.) */
  ACM_ASSERT (state && *state && letter, "Invalid null state or letter.");
#ifdef NMEYER_85
  ACMachine *machine = (*state)->machine;
  state_fail_state_construct (machine);
#endif
  return (*state = state_goto (*state, letter))->nb_outputs;
}

/* Aho-Corasick Algorithm 1: Pattern matching machine - print output (state) [ith element] */
void
acm_get_match (const ACState *state, size_t index, MatchHolder *matcher, void **value) {
  /* Aho-Corasick Algorithm 1: if output(state) [ith element] */
  ACM_ASSERT (state, "Invalid null state.");
  ACM_ASSERT (state != state->machine->state_0, "acm_match should be called first.");
  ACM_ASSERT (index < state->nb_outputs, "Index out of bounds.");
  size_t i = 0;
  // Starting from state, search for the index-th matching keyword in the "failing states" chain:
  for (; state; state = state->fail_state /* again */, i++ /* skip to the next failing state (next matching keyword) */) {
    /* state != state->machine->state_0, since index < state->nb_outputs */
    /* Look for the first state in the "failing states" chain which matches a keyword. */
    while (!state->is_end_of_keyword)
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
    for (const ACState *s = state; s && s->previous.state; s = s->previous.state, i++)
      matcher->letters[matcher->length - i - 1] = s->previous.letter;
  }
  /* Argument value could passed to 0 if the associated value is not needed. */
  if (value)
    *value = state->definition.value;
}
//-----------------------------------------------------------------
size_t
acm_nb_keywords (const ACMachine *machine) {
  ACM_ASSERT (machine, "Invalid null machine.");
  return machine->nb_sequences;
}

static void
foreach_keyword (const ACState *state, const void ***letters, size_t *max_length, size_t depth, void (*operator) (MatchHolder, void *)) {
  if (state->is_end_of_keyword && depth) {
    MatchHolder k = { .letters = *letters, .length = depth };
    (*operator) (k, state->definition.value);
  }
  for (struct _ac_transitions *p = state->transitions; p; p = p->next) {
    if (depth >= *max_length) {
      *max_length = depth + 1;
      *letters = realloc (*letters, *max_length * sizeof (**letters)); // letters allocation
      ACM_ASSERT (*letters, "Out of memory.");
    }
    (*letters)[depth] = p->value.letter;
    foreach_keyword (p->value.state, letters, max_length, depth + 1, operator);
  }
}

void
acm_foreach_keyword (const ACMachine *machine, void (*operator) (MatchHolder, void *)) {
  ACM_ASSERT (machine, "Invalid null machine.");
  if (!operator)
    return;
  ACState *state_0 = machine->state_0; /* [state 0] */
  const void **letters = 0;
  size_t max_length = 0;
  foreach_keyword (state_0, &letters, &max_length, 0, operator);
  free (letters); // letters deallocation
}
//-----------------------------------------------------------------
static void
state_print (ACState *state, FILE *stream, int *pcursor, int indent, PRINT_TYPE printer) {
  int cur_pos = *pcursor;
  ACM_ASSERT (!state->is_end_of_keyword || state->nb_outputs, "Keyword without defined output.");
  ACM_ASSERT (state == state->machine->state_0 ? state->fail_state == 0 : state->fail_state != 0, "Incorrect fail state.");
  for (struct _ac_transitions *pt = state->transitions; pt; pt = pt->next) {
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
      cur_pos += fprintf (stream, "(%03zu)", state->id); // Initial state #
    cur_pos += fprintf (stream, "---");

    struct _ac_transitions next = *pt;
    ACM_ASSERT (next.value.state->previous.state == state && next.value.state->previous.letter == next.value.letter, "Incorrect previous state.");
    if (printer)
      cur_pos += printer (stream, next.value.letter); // Transition "letter"
    cur_pos += fprintf (stream, "-->");
    cur_pos += fprintf (stream, "(%03zu)", next.value.state->id); // Final state #
    if (next.value.state->is_end_of_keyword)
      cur_pos += fprintf (stream, "[+%zu]", next.value.state->nb_outputs); // # of matching
    if (next.value.state->fail_state != state->machine->state_0)
      cur_pos += fprintf (stream, "(v %03zu)", next.value.state->fail_state->id); // Fail state #
    *pcursor = cur_pos;
    state_print (next.value.state, stream, pcursor, cur_pos, printer);
  } // for (struct _ac_transitions *pt = state->transitions; pt; pt = pt->next)
}

void
acm_print (ACMachine *machine, FILE *stream, PRINT_TYPE printer) {
  ACM_ASSERT (machine, "Invalid null machine.");
#ifdef NMEYER_85
  state_fail_state_construct (machine);
#endif
  if (stream) {
    fprintf (stream, "\n");
    state_print (machine->state_0, stream, &(int){ 0 }, 0, printer);
    fprintf (stream, "\n");
  }
}

#ifndef NMEYER_85
const int ACM_INCREMENTAL_STRING_MATCHING = 1;
#else
const int ACM_INCREMENTAL_STRING_MATCHING = 0;
#endif
