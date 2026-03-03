/*
 * Copyright 2026 Laurent Farhi
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

#ifndef __ACM__
#define __ACM__

#include <stdio.h>

typedef struct
{
  void **letters; /* An array of pointers to symbols */
  size_t length;  /* Length of the array */
  size_t rank;    /* Rank of the registered keyword */
} MatchHolder;

typedef struct _ac_state ACState;
typedef struct _ac_machine ACMachine;

typedef int (*EQ_TYPE) (const void *letter_a, const void *letter_b, void *eq_arg);
typedef void (*DESTROY_TYPE) (void *letter); // Compatible with the signature of free.
extern EQ_TYPE ACM_EQ_DEFAULT;               // Default equality operator (!memcmp). eq_arg MUST be a pointer to the size of a letter, &(size_t){ sizeof (T) }, where T is the type of the letters.

// An equality operator `eq` must be provided. The optional argument `eq_arg` will be passed to each call to `eq`.
// If the letters fed to the machine are not statically allocated until the machine is releases (with `acm_release`),
// - letters must be dynamically allocated before each call to `acm_insert_letter_of_keyword`.
// - a destructor `dtor` must be provided.
// - letters will be automatically deallocated by the machine.
// - if letters are allocated by a single call to `malloc`, `free` is a suitable destructor.
// `acm_release` must be subsequently called when the machine is not needed anymore.
ACMachine *acm_create (EQ_TYPE eq, void *eq_arg, DESTROY_TYPE dtor);

// Initialise a state that must be passed to the first call to `acm_insert_letter_of_keyword` or to the first call to `acm_match`.
ACState *acm_initiate (ACMachine *machine);

// A state must have been initialised with `acm_initiate` before the first call to `acm_insert_letter_of_keyword`.
// A `letter` must be provided.
// If it was allocated dynamically, it will be automatically deallocated by the machine.
void acm_insert_letter_of_keyword (ACState **state, void *letter);

// `acm_insert_letter_of_keyword` should have been previously called at least once before `acm_insert_end_of_keyword` is called.
// The `value` will be retrieved by a subsequent call to `acm_get_match`.
// If the `value` feed to the machine is not statically allocated until the machine is releases (with `acm_release`),
// - `value` must be dynamically allocated.
// - a destructor `dtor` must be provided.
// - `value` will be automatically deallocated by the machine.
// - if `value` is allocated by a single call to `malloc`, `free` is a suitable destructor.
// The keyword is given a unique internal rank in the machine.
void acm_insert_end_of_keyword (ACState **state, void *value, void (*dtor) (void *));

// A state msut have been initialised with `acm_initiate` before the first call to `acm_match`.
// A `letter` must be provided.
// Returns the number of matches found.
size_t acm_match (ACState **state, void *letter);

// A `MatchHolder` can be used to retrieve several matches (with `acm_get_match`) of different match searches (with `acm_match`).
// a `MatchHolder` must be release by a subsequent call to `acm_match_release` after use.
void acm_matcher_init (MatchHolder *matcher);

// `state` must the one used by a previous call to `acm_match`.
// The index must be lower than the number of matches found by a previous call to `acm_match`.
// If `match` is not null,
// - it must have been initialised by a previous call to `acm_match_init` before use.
// - it will be filled with the found match (with a keyword defined by a previous call to `acm_insert_end_of_keyword`).
// If `value` is not null, `*value` will be set to the value passed to a previous call to `acm_insert_end_of_keyword`.
// Returns the unique internal rank of the found keyword.
size_t acm_get_match (const ACState *state, size_t index, MatchHolder *matcher, void **value);

// The matcher must have been initialised by a previous call to `acm_match_init`.
void acm_matcher_release (MatchHolder *matcher);

// A machine must have been initialised by a previous call to `acm_create`.
size_t acm_nb_keywords (ACMachine *machine);

// A machine must have been initialised by a previous call to `acm_create`.
void acm_foreach_keyword (ACMachine *machine, void (*operator) (MatchHolder, void *value));

// For debugging purpose.
typedef int (*PRINT_TYPE) (FILE *, const void *letter);
void acm_print (ACMachine *machine, FILE *stream, PRINT_TYPE printer);

// The machine must have been initialised by a previous call to `acm_create`.
void acm_release (ACMachine *machine);

#endif
