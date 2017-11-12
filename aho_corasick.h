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

/// @see Aho, Alfred V.; Corasick, Margaret J. (June 1975). "Efficient string matching: An aid to bibliographic search".
/// Communications of the ACM. 18 (6): 333–340.
/// https://pdfs.semanticscholar.org/3547/ac839d02f6efe3f6f76a8289738a22528442.pdf
///
/// @see https://en.wikipedia.org/wiki/Aho%E2%80%93Corasick_algorithm

#pragma once
#ifndef AHO_CORASICK
#define AHO_CORASICK

#include <stddef.h>
#include <inttypes.h>
#include <stdlib.h>

#ifdef PRIVATE_MODULE
#define ACM_PRIVATE static __attribute__((__unused__))
#else
#define ACM_PRIVATE
#endif

// Type of SYMBOL int is generic and suits to numerous use cases.
#ifndef ACM_SYMBOL
#error Please define ACM_SYMBOL before include.
#endif

/******************** User interface ********************/
/// Type of keywords. A keyword is a succession of symbols of user defined type ACM_SYMBOL.
typedef struct
{
  ACM_SYMBOL *letter;           ///< An array of symbols
  size_t length;                ///< Length of the array
} Keyword;
typedef Keyword MatchHolder;

/// Matches macro helpers
// - getters: ACM_MATCH_LENGTH and ACM_MATCH_SYMBOLS
#define ACM_MATCH_LENGTH(match) ((match).length)
#define ACM_MATCH_SYMBOLS(match) ((match).letter)

/// Keyword macro helpers
// - setter: ACM_KEYWORD_SET
#define ACM_KEYWORD_SET(keyword, symbols, length)  \
  do { ACM_MATCH_SYMBOLS (keyword) = (symbols); ACM_MATCH_LENGTH (keyword) = (length); } while (0)

/// Matches macro helpers
// - memory managment: ACM_MATCH_INIT and ACM_MATCH_RELEASE
#define ACM_MATCH_INIT(match) ACM_KEYWORD_SET((match), 0, 0)
#define ACM_MATCH_RELEASE(match) do { free (ACM_MATCH_SYMBOLS (match)); ACM_MATCH_INIT (match); } while (0)

// State machine macro helpers
// - ACM_REGISTER_KEYWORD
#ifndef ACM_ASSOCIATED_VALUE
#define ACM_REGISTER_KEYWORD(machine, keyword)  \
  do { InitialState * tmp = ACM_register_keyword ((machine), (keyword)); if (tmp) (machine) = tmp; } while (0)
#else
#define ACM_REGISTER_KEYWORD(machine, keyword, value_ptr, dtor)  \
  do { InitialState * tmp = ACM_register_keyword ((machine), (keyword), (value_ptr), (dtor)); if (tmp) (machine) = tmp; } while (0)
#endif

// - ACM_CHANGE_STATE
#define ACM_CHANGE_STATE(state, letter)  \
  do { (state) = ACM_change_state ((state), (letter)) ; } while (0)

struct _ac_state;               // Partially declared structure
typedef struct _ac_state InitialState;
typedef struct _ac_state InternalState;

/// Register a new keyword into a Aho-Corasick Machine.
/// @param [in] machine 0 or a pointer to a Aho-Corasick Machine allocated by a previous call to ACM_register_keyword.
/// @param [in] keyword A new keyword to register.
/// @param [in] value A pointer to the value associated to the new keyword.
/// @param [in] dtor A pointer to the function called to release the value associated to keyword when the state machine is release by ACM_release.
/// @returns Pointer to the Aho-Corasick Machine or 0 if the keyword had already been registered and was not registered again.
/// machine should be 0 on first call.
/// Usage: InitialState is, machine = 0; if ((is = ACM_register_keyword (machine, keyword))) machine = is;
#ifndef ACM_ASSOCIATED_VALUE
ACM_PRIVATE InitialState *ACM_register_keyword (InitialState * machine, Keyword keyword);
#else
ACM_PRIVATE InitialState *ACM_register_keyword (InitialState * machine, Keyword keyword,
                                                void *value, void (*dtor) (void *));
#endif

/// Returns the number of registered keywords in the Aho-Corasick Machine.
/// @param [in] machine A pointer to a Aho-Corasick Machine allocated by a previous call to ACM_register_keyword.
/// @returns The number of registered keywords in the Aho-Corasick Machine.
ACM_PRIVATE size_t ACM_nb_keywords (InitialState * machine);

/// Change internal state.
/// @param [in] state Internal state.
/// @param [in] letter State transition.
/// @returns Updated internal state.
/// @note On first call or after a call to ACM_register_keyword(), the internal state should be initialized with value returned by
/// the last call to ACM_register_keyword().
/// Usage: state = ACM_change_state (state, letter);
ACM_PRIVATE InternalState *ACM_change_state (InternalState * state, ACM_SYMBOL letter);

/// Get the number of keywords associated to the internal state.
/// @param [in] state Internal state.
/// @returns The number of keywords associated to the internal state.
ACM_PRIVATE size_t ACM_nb_matches (const InternalState * state);

/// Get the ith keyword associate to the internal state.
/// @param [in] state Internal state.
/// @param [in] index of the requested keyword.
/// @param [out] match 0 or a pointer to a keyword.
///                       *match is modified to the keyword of rank 'index' associated to the internal state.
/// @param [out] value 0 or the address of the pointer to the value assiociated with the keyword match.
/// @returns The rank of insertion of the keyword when it was registered in the machine state by ACM_register_keyword().
/// index should be less than the value returned by state_nb_keywords().
/// match->letter should have been initialized to 0 prior to first call to ACM_get_match.
/// match->letter should be freed by the user program after the last call to ACM_get_match.
#ifndef ACM_ASSOCIATED_VALUE
ACM_PRIVATE size_t ACM_get_match (const InternalState * state, size_t index, MatchHolder * match);
#else
ACM_PRIVATE size_t ACM_get_match (const InternalState * state, size_t index, MatchHolder * match, void **value);
#endif

/// Release allocated resources.
/// @param [in] machine A pointer to a Aho-Corasick Machine allocated by a previous call to ACM_register_keyword.
/// The user program should call ACM_release after usage of machine.
/// machine should be set to 0 before reuse.
ACM_PRIVATE void ACM_release (InitialState * machine);

#ifdef PRIVATE_MODULE
#include "aho_corasick.c"
#endif

#endif
