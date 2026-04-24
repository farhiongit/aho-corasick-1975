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
  const void **letters; /* An array of pointers to symbols */
  size_t length;        /* Length of the array */
  void *value;          /* Value associated with the matched pattern */
} MatchHolder;

typedef struct _ac_state ACState;
typedef struct _ac_machine ACMachine;

typedef int (*CMP_TYPE) (const void *letter_a, const void *letter_b, const void *eq_arg);
typedef void (*DESTROY_TYPE) (void *letter); // Compatible with the signature of free.
extern const CMP_TYPE ACM_CMP_DEFAULT;       // Default comparison function (memcmp). cmp_arg MUST be a pointer to the size of a letter, &(size_t){ sizeof (T) }, where T is the type of the letters.

// A comparison function `cmp` must be provided. The optional argument `cmp_arg` will be passed to each call to `cmp`.
// The comparison function `cmp` must return an integer less than, equal to, or greater than zero if the first argument is considered to be respectively less than, equal to, or greater than the second.
// If the letters fed to the machine are not statically allocated until the machine is releases (with `acm_release`),
// - letters must be dynamically allocated before each call to `acm_insert_letter_of_keyword`.
// - a destructor `dtor` must be provided.
// - letters will be automatically deallocated by the machine.
// - if letters are allocated by a single call to `malloc`, `free` is a suitable destructor.
// `acm_release` must be subsequently called when the machine is not needed anymore.
ACMachine *acm_create (CMP_TYPE cmp, void *cmp_arg, DESTROY_TYPE dtor);

// Initialise a state that must be passed to the first call to `acm_insert_letter_of_keyword` or to the first call to `acm_match`.
ACState *acm_initiate (ACMachine *machine);

// A state must have been initialised with `acm_initiate` before the first call to `acm_insert_letter_of_keyword`.
// A `letter` must be provided.
// If it was allocated dynamically, it will be automatically deallocated by the machine.
void acm_insert_letter_of_keyword (ACState **state, void *letter);

// acm_insert_letter_of_keyword must have been previously called at least once before acm_insert_end_of_keyword is called.
// The value fed to the machine can be allocated statically; automatically pr dynamically, as long as it persists until the machine is releases with acm_release.
// If value is dynamically allocated:
// - a destructor dtor must be provided (if value is allocated by a single call to malloc, free is a suitable destructor.)
// - if acm_insert_end_of_keyword returns 0,
//   - value could later be retrieved by a subsequent call to acm_get_match;
//   - value will be automatically deallocated by the machine at release (with a call to acm_release);
// - otherwise, the passed `value` won't be managed by the machine and should be handled and free'd by the caller.
// The keyword is given a unique internal rank in the machine that will later be returned by calls to acm_get_match.
// Returns 0 if the keyword has not been previously inserted yet or has not already an associated value. Otherwise, returns the previously associated value.
void *acm_insert_end_of_keyword (ACState **state, void *value, void (*dtor) (void *));

// A state msut have been initialised with `acm_initiate` before the first call to `acm_match`.
// A `letter` must be provided.
// Returns the number of matches found.
size_t acm_match (const ACState **state, const void *letter);

// A `MatchHolder` can be used to retrieve several matches (with `acm_get_match`) of different match searches (with `acm_match`).
// a `MatchHolder` must be release by a subsequent call to `acm_match_release` after use.
void acm_matcher_init (MatchHolder *matcher);

// `state` must the one used by a previous call to `acm_match`.
// The index must be lower than the number of matches found by a previous call to `acm_match`.
// If `match` is not null,
// - it must have been initialised by a previous call to `acm_match_init` before use.
// - it will be filled with the found match (with a keyword defined by a previous call to `acm_insert_end_of_keyword`).
void acm_get_match (const ACState *state, size_t index, MatchHolder *matcher);

// The matcher must have been initialised by a previous call to `acm_match_init`.
void acm_matcher_release (MatchHolder *matcher);

// A machine must have been initialised by a previous call to `acm_create`.
size_t acm_nb_keywords (const ACMachine *machine);

// A machine must have been initialised by a previous call to `acm_create`.
void acm_foreach_keyword (const ACMachine *machine, void (*operator) (MatchHolder));

// The machine must have been initialised by a previous call to `acm_create`.
void acm_release (ACMachine *machine);

// For debugging purpose.
typedef int (*PRINT_TYPE) (FILE *, const void *letter);
void acm_print (ACMachine *machine, FILE *stream, PRINT_TYPE printer);
extern const int ACM_INCREMENTAL_STRING_MATCHING; // # If set, incremental string matching (Meyer, 1985) is used. Otherwise, original Aho-Corasick algorithm is used (NMEYER_85 defined at compile time).

#endif
