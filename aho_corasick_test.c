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

// The interface of aho_corasick.h will be linked as static functions, not exportable to other compilation unit.
#define PRIVATE_MODULE

#include <stdio.h>
#include <assert.h>
#include <wctype.h>
#include <wchar.h>
#include <locale.h>

// 1. Define the type ACM_SYMBOL with the type of symbols that constitute keywords.
#define ACM_SYMBOL wchar_t

// A comparator of symbols can be user defined instead of the standard '==' operator.
static int nocaseeq (wchar_t k, wchar_t t);

#define ACM_SYMBOL_EQ_OPERATOR nocaseeq

// 2. Include "aho_corasick.h"
#include "aho_corasick.h"

static int nocase;
static int words;
static int
nocaseeq (wchar_t k, wchar_t t)
{
  if (words)
  {
    if (!iswalpha (k) && !iswalpha (t))
      return 1;
    if (iswalpha (k) && !iswalpha (t))
      return 0;
    if (!iswalpha (k) && iswalpha (t))
      return 0;
  }

  if (nocase)
    return k == towlower (t);
  else
    return k == t;
}

#ifndef ACM_ASSOCIATED_VALUE
static void
print_match (MatchHolder match)
#else
static void
print_match (MatchHolder match, void *value)
#endif
{
#ifdef ACM_ASSOCIATED_VALUE
  if (value && *(size_t *) value == 0)
    return;
#endif
  if (ACM_MATCH_LENGTH (match))
  {
    printf ("{");
    for (size_t k = 0; k < ACM_MATCH_LENGTH (match); k++)
      printf ("%lc", ACM_MATCH_SYMBOLS (match)[k]);

#ifdef ACM_ASSOCIATED_VALUE
    if (value)
    {
      printf (", ");
      printf ("%lu", *(size_t *) value);
    }
#endif
    printf ("}");
  }
}

// A unit test
int
main (void)
{
  setlocale (LC_ALL, "");

  /****************** First test ************************/
  // This test constructs and plays with the graph from the original paper of Aho-Corasick.
  // The text where keywords are searched for.
  wchar_t text[] = L"He found his pencil, but she could not find hers (hi! ushers !!)\nabcdz\nbcz\nczz\n_abcde_xyzxyt";

  nocase = 1;                   // not case sensitive

  // 3. The machine state is initialized with 0 before any call to ACM_register_keyword.
  ACMachine *M = 0;

// Declares all the keywords
// "hers" appears twice but will be registerd once.
#define LIST_OF_KEYWORDS  \
  X(M, L'h', L'e')  \
  X(M, L's', L'h', L'e')  \
  X(M, L's', L'h', L'e', L'e', L'r', L's')  \
  X(M, L'h', L'i', L's')  \
  X(M, L'h', L'i')  \
  X(M, L'h', L'e', L'r', L's')  \
  X(M, L'u', L's', L'h', L'e', L'r', L's')  \
  X(M, L'a', L'b', L'c', L'd', L'e')  \
  X(M, L'b', L'c', L'd')  \
  X(M, L'h', L'e', L'r', L's')  \
  X(M, L'z', L'z')  \
  X(M, L'c')  \
  X(M, L'z')  \
  X(M, L'p', L'e', L'n')  \
  X(M, L'z', L'z', L'z')  \
  X(M, L'x', L'y', L'z')  \
  X(M, L'x', L'y', L't')  \

#ifdef ACM_ASSOCIATED_VALUE
#  define EXTRA ,0
#else
#  define EXTRA
#endif

#define X(MACHINE, ...) \
  {  \
    Keyword VAR;  \
    ACM_SYMBOL _VAR[] = { __VA_ARGS__ };  \
    ACM_KEYWORD_SET (VAR, _VAR, sizeof (_VAR) / sizeof (*_VAR));  \
    ACMachine * is;  \
    /* 4. Add keywords (of type Keyword) to the state machine calling ACM_register_keyword() repeatedly. */ \
    if ((is = ACM_register_keyword (MACHINE, VAR EXTRA EXTRA)))  \
    {  \
      MACHINE = is;  \
      print_match (VAR EXTRA);  \
    }  \
    else  \
      printf  ("X");  \
  }

  // Function applied to register a keyword in the state machine
  LIST_OF_KEYWORDS;             // Ending ';' is not necessary and is only here for 'indent' to switch to correct indentation.
#undef X

  printf (" [%zu]\n", ACM_nb_keywords (M));
  {
    Keyword kw = {.letter = L"sheers",.length = 6 };
    assert (ACM_is_registered_keyword (M, kw EXTRA));
    assert (ACM_unregister_keyword (M, kw));
    assert (!ACM_unregister_keyword (M, kw));
    assert (!ACM_is_registered_keyword (M, kw EXTRA));
  }
  {
    Keyword kw = {.letter = L"hi",.length = 2 };
    assert (ACM_unregister_keyword (M, kw));
  }
  {
    Keyword kw = {.letter = L"pen",.length = 3 };
    assert (ACM_unregister_keyword (M, kw));
  }
  {
    Keyword kw = {.letter = L"zzz",.length = 3 };
    assert (ACM_unregister_keyword (M, kw));
  }
  {
    Keyword kw = {.letter = L"xyt",.length = 3 };
    assert (ACM_unregister_keyword (M, kw));
  }

  ACM_foreach_keyword (M, print_match);
  printf (" [%zu]\n", ACM_nb_keywords (M));

  // 5. Initialize a match holder with ACM_MATCH_INIT before the first use by ACM_get_match.
  MatchHolder match;

  ACM_MATCH_INIT (match);
  printf ("%ls\n", text);
  for (size_t i = 0; i < wcslen (text); i++)
  {
    printf ("%lc", text[i]);

    // 6. Inject symbols of the text, one at a time by calling ACM_change_state().
    ACM_change_state (M, text[i]);

    // 7. After each insertion of a symbol, call ACM_nb_matches() to check if the last inserted symbols match a keyword.
    // Get the matching keywords for the actual state of the machine.
    size_t nb_matches = ACM_nb_matches (M);

    // 8. If matches were found, retrieve them calling ACM_get_match() for each match.
    for (size_t j = 0; j < nb_matches; j++)
    {
      // Get the ith matching keyword for the actual state of the machine.
#ifndef ACM_ASSOCIATED_VALUE
      printf ("[%zu]", ACM_get_match (M, j, &match));
#else
      printf ("[%zu]", ACM_get_match (M, j, &match, 0));
#endif

      // Display matching pattern
      print_match (match EXTRA);
    }
  }

  // 9. After the last call to ACM_get_match(), release to keyword by calling ACM_MATCH_RELEASE.
  ACM_MATCH_RELEASE (match);

  printf ("\n");

  // 10. Release resources allocated by the state machine after usage.
  ACM_release (M);

  /****************** Second test ************************/
  // This test counts the number of time the words in the english dictionnary ("words") appear
  // in the Woolf's book Mrs Dalloway ("mrs_dalloway.txt").
  nocase = 0;                   // Case sensitive
  words = 1;

  FILE *stream;

  stream = fopen ("words", "r");
  if (stream == 0)
    exit (EXIT_FAILURE);

  wchar_t line[100] = L" ";     // keywords start with ' '

  // 3. M is prepared for reuse.
  M = 0;

  while (fgetws (line + 1, sizeof (line) / sizeof (*line) - 1, stream))
  {
    Keyword k;

    line[wcslen (line) - 1] = L' ';     // keywords end with ' \0'
    ACM_KEYWORD_SET (k, line, wcsnlen (line, sizeof (line) / sizeof (*line)));
#ifdef ACM_ASSOCIATED_VALUE
    size_t *v = malloc (sizeof (*v));

    *v = 0;

    // 4. Add keywords (of type Keyword) to the state machine calling ACM_register_keyword() repeatedly.
    // Values can be associated registered keywords. Values are allocated in the user program.
    // This memory will be freed by the function passed as the 4th argument (here 'free', but it could be a user defined finction).
    // That function will be called for each registered keyword by ACM_release.
    ACM_REGISTER_KEYWORD (M, k, v, free);
#else
    ACM_REGISTER_KEYWORD (M, k);
#endif

    if (1)
      ;
    else if (!wcscmp (L" m ", line))
    {
      ACM_change_state (M, L' ');
      printf ("[%zu]:\n", ACM_nb_keywords (M));
      // If a new text has to be processed by the state machine, reset it to its initial state so that the next symbol
      // will be matched against the first letter of each keyword.
      ACM_reset_state (M);
    }
    else if (!wcscmp (L" n ", line))
    {
      ACM_change_state (M, L' ');
      printf ("[%zu]:\n", ACM_nb_keywords (M));
      ACM_reset_state (M);
    }
  }

  fclose (stream);

  // 6. Inject symbols of the text, one at a time by calling ACM_change_state().
  ACM_change_state (M, L' ');
  printf ("[%zu]:\n", ACM_nb_keywords (M));

  stream = fopen ("mrs_dalloway.txt", "r");
  if (stream == 0)
    exit (EXIT_FAILURE);

  for (wint_t wc; (wc = fgetwc (stream)) != WEOF;)
  {
    // 6. Inject symbols of the text, one at a time by calling ACM_change_state().
    ACM_change_state (M, wc);
    // 7. After each insertion of a symbol, call ACM_nb_matches() to check if the last inserted symbols match a keyword.
    size_t nb = ACM_nb_matches (M);

    if (nb)
    {
#ifdef ACM_ASSOCIATED_VALUE
      // 8. If matches were found, retrieve them calling ACM_get_match() for each match.
      for (size_t j = 0; j < nb; j++)
      {
        void *v;
        ACM_get_match (M, j, 0, &v);
        (*(size_t *) v)++;
      }
#endif
    }
  }

  fclose (stream);

#ifdef ACM_ASSOCIATED_VALUE
  ACM_foreach_keyword (M, print_match);
#endif

  printf ("\n");

  // 10. After usage, release the state machine calling ACM_release() on M.
  // ACM_release also frees the values associated to registered keywords.
  ACM_release (M);
}
