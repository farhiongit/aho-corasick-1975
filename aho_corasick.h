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
#  define AHO_CORASICK

#  include <stdlib.h>

#  ifdef PRIVATE_MODULE
#    define ACM_PRIVATE static __attribute__((__unused__))
#  else
#    define ACM_PRIVATE
#  endif

// Type of ACM_SYMBOL int is generic and suits to numerous use cases.
#  ifndef ACM_SYMBOL
#    error Please define ACM_SYMBOL before include.
#  endif

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
#  define ACM_MATCH_LENGTH(match) ((match).length)
#  define ACM_MATCH_SYMBOLS(match) ((match).letter)

/// Keyword macro helpers
// - setter: ACM_KEYWORD_SET
#  define ACM_KEYWORD_SET(keyword, symbols, length)  \
  do { ACM_MATCH_SYMBOLS (keyword) = (symbols); ACM_MATCH_LENGTH (keyword) = (length); } while (0)

/// Matches macro helpers
// - memory managment: ACM_MATCH_INIT and ACM_MATCH_RELEASE
#  define ACM_MATCH_INIT(match) ACM_KEYWORD_SET((match), 0, 0)
#  define ACM_MATCH_RELEASE(match) do { free (ACM_MATCH_SYMBOLS (match)); ACM_MATCH_INIT (match); } while (0)

// State machine macro helpers
// - ACM_REGISTER_KEYWORD
#  ifndef ACM_ASSOCIATED_VALUE
#    define ACM_REGISTER_KEYWORD(machine, keyword)  \
  do { ACMachine * tmp = ACM_register_keyword ((machine), (keyword)); if (tmp) (machine) = tmp; } while (0)
#  else
#    define ACM_REGISTER_KEYWORD(machine, keyword, value_ptr, dtor)  \
  do { ACMachine * tmp = ACM_register_keyword ((machine), (keyword), (value_ptr), (dtor)); if (tmp) (machine) = tmp; } while (0)
#  endif

struct _ac_state;               // Partially declared structure
typedef struct _ac_state ACState;
struct _ac_machine;             // Partially declared structure
typedef struct _ac_machine ACMachine;

/// Register a new keyword into a Aho-Corasick Machine.
/// @param [in] machine 0 or a pointer to a Aho-Corasick Machine allocated by a previous call to ACM_register_keyword.
/// @param [in] keyword A new keyword to register.
/// @param [in] value A pointer to the value associated to the new keyword.
/// @param [in] dtor A pointer to the function called to release the value associated to keyword when the state machine is release by ACM_release.
/// @returns Pointer to the Aho-Corasick Machine or 0 if the keyword had already been registered and was not registered again.
/// machine should be 0 on first call.
/// Usage: ACMachine is, machine = 0; if ((is = ACM_register_keyword (machine, keyword))) machine = is;
#  ifndef ACM_ASSOCIATED_VALUE
ACM_PRIVATE ACMachine *ACM_register_keyword (ACMachine * machine, Keyword keyword);
#  else
ACM_PRIVATE ACMachine *ACM_register_keyword (ACMachine * machine, Keyword keyword, void *value, void (*dtor) (void *));
#  endif

/// Check if a keyword is registered into a Aho-Corasick Machine.
/// @param [in] machine A pointer to a Aho-Corasick Machine allocated by a previous call to ACM_register_keyword.
/// @param [in] keyword A keyword.
/// @param [out] value 0 or the address of the pointer to the value assiociated with the keyword match.
/// @return 1 if the keyword is registered in the machine, 0 otherwise.
#  ifndef ACM_ASSOCIATED_VALUE
ACM_PRIVATE int ACM_is_registered_keyword (const ACMachine * machine, Keyword keyword);
#  else
ACM_PRIVATE int ACM_is_registered_keyword (const ACMachine * machine, Keyword keyword, void **value);
#  endif

/// Unregister a keyword from a Aho-Corasick Machine.
/// @param [in] machine A pointer to a Aho-Corasick Machine allocated by a previous call to ACM_register_keyword.
/// @param [in] keyword A keyword.
/// @returns 1 if the keyword was successfully unregistered, 0 otherwise.
/// @note The rank of keywords of higher rank are kept unchanged. Ranks can then be larger than ACM_nb_keywords().
ACM_PRIVATE int ACM_unregister_keyword (ACMachine * machine, Keyword keyword);

/// Returns the number of registered keywords in the Aho-Corasick Machine.
/// @param [in] machine A pointer to a Aho-Corasick Machine allocated by a previous call to ACM_register_keyword.
/// @returns The number of registered keywords in the Aho-Corasick Machine.
ACM_PRIVATE size_t ACM_nb_keywords (const ACMachine * machine);

/// Apply operator to each of the registered keywords in the state machine.
/// @param [in] machine A pointer to a Aho-Corasick Machine allocated by a previous call to ACM_register_keyword.
/// @param [in] operator Operator applied to each keyword of the state machine.
/// @note The operator is applied to keywords in unspecified order.
#  ifndef ACM_ASSOCIATED_VALUE
ACM_PRIVATE void ACM_foreach_keyword (const ACMachine * machine, void (*operator) (Keyword));
#  else
ACM_PRIVATE void ACM_foreach_keyword (const ACMachine * machine, void (*operator) (Keyword, void *));
#  endif

/// Setting or resetting the state to the initial state of the state machine
/// will enforce the next letter to try to match with only the first symbol of a registered keyword (if possible).
/// This is useful to start parsing a new text against registered keywords.
/// @param [in] machine A pointer to a valid state machine.
/// @returns A pointer to a valid state.
/// Thread safe.
ACM_PRIVATE const ACState *ACM_reset (const ACMachine * machine);

/// Transit to a valid state matching letter.
/// @param [in] state A pointer to a valid state.
/// @param [in] letter letter to match keywords with.
/// @returns A pointer to a valid state after transition to letter.
/// Thread safe.
ACM_PRIVATE const ACState *ACM_match (const ACState * state, ACM_SYMBOL letter);

/// Get the number of keywords associated to the last provided letters.
/// @param [in] state A pointer to a valid state.
/// @returns The number of keywords matching with the last letters.
/// Thread safe.
ACM_PRIVATE size_t ACM_nb_matches (const ACState * state);

/// Get the number of keywords associated to the last provided letters.
/// @param [in] state A pointer to a valid state.
/// @param [in] letter letter to match keywords with.
/// @returns The number of keywords matching with the last letters.
#define ACM_NB_MATCHES(state, letter) ACM_nb_matches ((state) = ACM_match ((state), (letter)))

/// Get the ith keyword associate to the state.
/// @param [in] state A pointer to a valid state.
/// @param [in] index of the requested keyword.
/// @param [out] match 0 or a pointer to a keyword.
///                       *match is modified to the keyword of rank 'index' associated to the internal state.
/// @param [out] value 0 or the address of the pointer to the value assiociated with the keyword match.
/// @returns The rank of insertion of the keyword when it was registered in the machine state by ACM_register_keyword().
/// index should be less than the value returned by state_nb_keywords().
/// match->letter should have been initialized to 0 prior to first call to ACM_get_match.
/// match->letter should be freed by the user program after the last call to ACM_get_match.
/// Thread safe.
#  ifndef ACM_ASSOCIATED_VALUE
ACM_PRIVATE size_t ACM_get_match (const ACState * state, size_t index, MatchHolder * match);
#  else
ACM_PRIVATE size_t ACM_get_match (const ACState * state, size_t index, MatchHolder * match, void **value);
#  endif

/// Release allocated resources.
/// @param [in] machine A pointer to a Aho-Corasick Machine allocated by a previous call to ACM_register_keyword.
/// The user program should call ACM_release after usage of machine.
/// machine should be set to 0 before reuse.
ACM_PRIVATE void ACM_release (const ACMachine * machine);

#  ifdef PRIVATE_MODULE
#    include "aho_corasick.c"
#  endif

#endif
