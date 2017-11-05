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

/// @see Aho, Alfred V.; Corasick, Margaret J. (June 1975). "Efficient string matching: An aid to bibliographic search". Communications of the ACM. 18 (6): 333–340.
/// @see https://en.wikipedia.org/wiki/Aho%E2%80%93Corasick_algorithm
///
/// Compared to the implemenation proposed by Aho and Corasick, this one adds two small enhancements:
/// - This implementation does not stores output keywords associated to states.
///   It rather reconstructs matching keywords by traversing the branch of the tree backward (see ACM_get_match).
/// - This implementation permits to search for keywords even though all keywords have not been registered yet.
///   To achieve this, failure states are reconstructed after every registration of a next keyword
///   (see ACM_register_keyword which alternates calls to algorithms 2 and 3.)
/// - This implemtation keeps track of the rank of a registered keyword as returned by ACM_get_match().
///   This can be used as a unique identifiant of a keyword for a given machine state.

/// Usage:
/// 1/ Define the type ACM_SYMBOL with the type of symbols that constitute keywords. char or int should match most needs.
///    Either define ACM_SYMBOL direcly in the user program or include a user defines aho_corasick_symbol.h file.
///    E.g.:
///    - #define ACM_SYMBOL char (simple choice if the algorithm is compiled as a private module).
///    - #include "aho_corasick_symbol.h" (better choice if the algorithm is compiled as an object or library, and not as a private module).
/// 2/ Insert "aho_corasick.h"
/// 3/ Initialize a state machine: InitialState * M = 0;
/// 4/ Add keywords (of type Keyword) to the state machine calling ACM_register_keyword() repeatedly.
///    Notes: ACM_KEYWORD_SET can help initialize keywords with a single statement.
///           ACM_nb_matches() returns the number of keywords already inserted in the state machine.
/// 5/ Initialize an internal machine state to M: InternalState s = M;
/// Repeatedly:
/// 6/   Scan some text where to search for keywords, injecting symbols of the text, one at a time by calling ACM_change_state() on s.
/// 7/   After each insertion, call ACM_nb_matches() on the internal state s to check if the last inserted symbols match a keyword.
/// 8/   If matches were found, retrieve them calling ACM_get_match() for each match.
/// 9/ After usage, release the state machine calling ACM_release() on M.
///
/// Note if ACM_SYMBOL is a structure (does not apply for basic types such as int or char):
/// - An equality operator '==' with signature 'int eq (ACM_SYMBOL a, ACM_SYMBOL b)' should be defined
///   in the user program and the name of the function should be defined in macro ACM_SYMBOL_EQ_OPERATOR.
///   E.g.: #define ACM_SYMBOL_EQ_OPERATOR myequalityoperaor
/// - Keywords are passed by value to ACM_register_keyword().
///   Therefore, if ACM_SYBOL contains pointers to allocated memory,
///   - a copy operator '=' with signature 'ACM_SYMBOL copy (ACM_SYMBOL a)' should be defined in the user program and
///     the name of the function should be defined in macro ACM_SYMBOL_COPY_OPERATOR.
///   - a destructor operator with signature 'void dtor (ACM_SYMBOL a)' should be defined in the user program and
///     the name of the function should be defined in macro ACM_SYMBOL_DTOR_OPERATOR.
///
/// Compilation:
/// The algorithm can be compiled either as an object (aho_corasick.o) or as a private module (if PRIVATE_MODULE is defined in the user program).
/// If PRIVATE_MODULE is set in the user program, then:
/// - the implementation of the algorithm will be compiled in the same compilation unit as the user program, therefore without requiring linkage.
/// - the warning "The Aho-Corasick algorithm is compiled as a private module." is emitted during compilation.

#pragma once
#ifndef AHO_CORASICK
#define AHO_CORASICK

#include <stddef.h>
#include <inttypes.h>
#include <stdlib.h>

#ifdef PRIVATE_MODULE
#warning The Aho-Corasick algorithm is compiled as a private module.
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

/// Keyword helpers
#define ACM_KEYWORD_LENGTH(keyword) ((keyword).length)
#define ACM_KEYWORD_SYMBOLS(keyword) ((keyword).letter)
#define ACM_KEYWORD_SET(keyword, symbols, length)  \
  do { ACM_KEYWORD_SYMBOLS(keyword) = symbols ; ACM_KEYWORD_LENGTH(keyword) = length ; } while(0)
#define ACM_KEYWORD_RELEASE(keyword) do { free (ACM_KEYWORD_SYMBOLS(keyword)) ; } while(0)

struct _ac_state;               // Partially declared structure
typedef struct _ac_state InitialState;
typedef struct _ac_state InternalState;

/// Register a new keyword into a Aho-Corasick Machine.
/// @param [in] machine 0 or a pointer to a Aho-Corasick Machine allocated by a previous call to ACM_register_keyword.
/// @param [in] keyword A new keyword to register.
/// @returns Pointer to the Aho-Corasick Machine or 0 if the keyword had already been registered and was not registered again.
/// machine should be 0 on first call.
/// Usage: InitialState is, machine = 0; if ((is = ACM_register_keyword (machine, keyword))) machine = is;
ACM_PRIVATE InitialState *ACM_register_keyword (InitialState * machine, Keyword keyword);

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
/// @param [in,out] match A pointer to the keyword of rank index associated to the internal state.
/// @returns The rank of insertion of the keyword when it was registered in the machine state by ACM_register_keyword().
/// index should be less than the value returned by state_nb_keywords().
/// match->letter should have been initialized to 0 prior to first call to ACM_get_match.
/// match->letter should be freed by the user program after the last call to ACM_get_match.
ACM_PRIVATE size_t ACM_get_match (const InternalState * state, size_t index, Keyword * match);

/// Release allocated resources.
/// @param [in] machine A pointer to a Aho-Corasick Machine allocated by a previous call to ACM_register_keyword.
/// The user program should call ACM_release after usage of machine.
/// machine should be set to 0 before reuse.
ACM_PRIVATE void ACM_release (InitialState * machine);

#ifdef PRIVATE_MODULE
#include "aho_corasick.c"
#endif

#endif
