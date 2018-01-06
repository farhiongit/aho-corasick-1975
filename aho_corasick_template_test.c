
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
#include <assert.h>
#include <wctype.h>
#include <wchar.h>
#include <locale.h>
#include <time.h>

// 1. Insert "aho_corasick_template_impl.h" in global scope.
#include "aho_corasick_template_impl.h"

// 2. Declare, in global scope, the types for which the Aho-Corasick machines have to be instanciated (for as many types needed.)
/* *INDENT-OFF* */
ACM_DECLARE (wchar_t)
ACM_DEFINE (wchar_t)
/* *INDENT-ON* */

static int words;
static int current_pos;

// 3. Optionally, user defined operators can be specified.
// User defined case insensitive comparison:
static int
nocaseeq (wchar_t k, wchar_t t)
{
  return k == towlower (t);
}

// User defined alphabet only comparison:
static int
alphaeq (wchar_t k, wchar_t t)
{
  if (words)
  {
    int nak = !iswalpha (k);
    int nat = !iswalpha (t);
    if (nak)
      return nat;
    else if (nat)
      return 0;
  }

  return k == towlower (t);
}

static void
print_keyword (Keyword (wchar_t) kw)
{
  printf ("{'");
  for (size_t k = 0; k < ACM_MATCH_LENGTH (kw); k++)
    printf ("%lc", ACM_MATCH_SYMBOLS (kw)[k]);
  printf ("'}");
}

static void
print_match (MatchHolder (wchar_t) match, void *value)
{
  // Filter matching keywords which value is equal to 0.
  if (value && *(size_t *) value == 0)
    return;
  if (ACM_MATCH_LENGTH (match))
  {
    current_pos += printf ("{");
    current_pos += printf ("'");
    for (size_t k = 0; k < ACM_MATCH_LENGTH (match); k++)
      current_pos += printf ("%lc", ACM_MATCH_SYMBOLS (match)[k]);

    current_pos += printf ("'");
    if (value)
    {
      current_pos += printf ("=");
      current_pos += printf ("%zu", *(size_t *) value);
    }
    current_pos += printf ("[%zu]", ACM_MATCH_UID (match));
    current_pos += printf ("}");
  }
}

// A unit test
int
main (void)
{
  setlocale (LC_ALL, "");

  // 3. An optional equality operator can be user defined with SET_EQ_OPERATOR.
  SET_EQ_OPERATOR (wchar_t, nocaseeq);

  /****************** First test ************************/
  // This test constructs and plays with the graph from the original paper of Aho-Corasick.
  // The text where keywords are searched for.
  wchar_t text[] =
    L"He found his pencil, but she could not find hers (Hi! Ushers !!) ; abcdz ; bCz ; cZZ ; _abcde_xyzxyt";

  // 4. Initialize a state machine of type ACMachine (T) using ACM_create (T):
  ACMachine (wchar_t) * M = ACM_create (wchar_t);

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

// Function applied to register a keyword in the state machine
#define X(MACHINE, ...) \
  {  \
    Keyword (wchar_t) VAR;  \
    wchar_t _VAR[] = { __VA_ARGS__ };  \
    ACM_KEYWORD_SET (VAR, _VAR, sizeof (_VAR) / sizeof (*_VAR));  \
    /* 5. Add keywords (of type `Keyword (T)`) to the state machine calling `ACM_register_keyword()`, one at a time, repeatedly. */ \
    if (ACM_register_keyword (MACHINE, VAR, 0, 0))          \
      print_keyword (VAR);  \
    else  \
      printf  ("X");  \
  }

  // Function applied to register a keyword in the state machine
  LIST_OF_KEYWORDS;             // Ending ';' is not necessary and is only here for 'indent' to switch to correct indentation.
#undef X

  printf (" [%zu]\n", ACM_nb_keywords (M));
  {
    /* *INDENT-OFF* */
    // Template: define keyword
    Keyword (wchar_t) kw = {.letter = L"sheers",.length = 6};
    /* *INDENT-ON* */
    assert (ACM_is_registered_keyword (M, kw, 0));
    assert (ACM_unregister_keyword (M, kw));
    assert (!ACM_unregister_keyword (M, kw));
    assert (!ACM_is_registered_keyword (M, kw, 0));
  }
  {
    /* *INDENT-OFF* */
    Keyword (wchar_t) kw = {.letter = L"hi",.length = 2};
    /* *INDENT-ON* */
    assert (ACM_unregister_keyword (M, kw));
  }
  {
    /* *INDENT-OFF* */
    Keyword (wchar_t) kw = {.letter = L"pen",.length = 3};
    /* *INDENT-ON* */
    assert (ACM_unregister_keyword (M, kw));
  }
  {
    /* *INDENT-OFF* */
    Keyword (wchar_t) kw = {.letter = L"zzz",.length = 3};
    /* *INDENT-ON* */
    assert (ACM_unregister_keyword (M, kw));
  }
  {
    /* *INDENT-OFF* */
    Keyword (wchar_t) kw = {.letter = L"xyt",.length = 3};
    /* *INDENT-ON* */
    assert (ACM_unregister_keyword (M, kw));
  }

  ACM_foreach_keyword (M, print_match);
  printf (" [%zu]\n", ACM_nb_keywords (M));

  // 6. Initialize a match holder with `ACM_MATCH_INIT (match)`
  MatchHolder (wchar_t) match;

  ACM_MATCH_INIT (match);

  Keyword (wchar_t) kw = {.letter = text,.length = wcslen (text) };
  current_pos = 0;
  print_keyword (kw);
  printf ("\n");

  // 7. Initialize a state with `ACM_reset (machine)`
  const ACState(wchar_t) * state = ACM_reset (M);
  for (size_t i = 0; i < wcslen (text); i++)
  {
    // 8. Inject symbols of the text, one at a time by calling `ACM_match`.
    size_t nb_matches = ACM_match (state, text[i]);
    // 9. After each insertion of a symbol, check the returned value to know
    //    if the last inserted symbols match at least one keyword.

    for (size_t j = 0; j < nb_matches; j++)
    {
      // 10. If matches were found, retrieve them calling `ACM_get_match ()` for each match.
      size_t rank =
        ACM_get_match (state, j, &match, 0);
      ACM_ASSERT (rank == ACM_MATCH_UID (match));

      // `ACM_MATCH_LENGTH (match)` and `ACM_MATCH_SYMBOLS (match)` can be used to get the length and the content of a retreieved match.
      if (current_pos > i + 1 - ACM_MATCH_LENGTH (match))
      {
        current_pos = 0;
        printf ("\n");
      }
      for (size_t tab = current_pos; tab < i + 1 - ACM_MATCH_LENGTH (match); tab++)
        current_pos += printf (" ");
      // Display matching pattern
      print_match (match, 0);
    }
  }

  // 11. After the last call to `ACM_get_match ()`, release to match holder by calling `ACM_MATCH_RELEASE (match)`.
  ACM_MATCH_RELEASE (match);

  // 12. Release the state machine calling ACM_release() on M.
  ACM_release (M);

  printf ("\n");

  /****************** Second test ************************/
  // This test counts the number of time the words in the english dictionnary ("words") appear
  // in the Woolf's book Mrs Dalloway ("mrs_dalloway.txt").
  words = 1;

  FILE *stream;

  stream = fopen ("words", "r");
  if (stream == 0)
    exit (EXIT_FAILURE);

  wchar_t line[100] = L" ";     // keywords start with ' '

  // 4. Initialize a state machine of type ACMachine (T) using ACM_create (T)
  //    An optional second argument of type EQ_OPERATOR_TYPE(*T*) can specify a user defined equality operator.
  M = ACM_create (wchar_t, alphaeq);

  clock_t myclock = clock ();
  while (fgetws (line + 1, sizeof (line) / sizeof (*line) - 1, stream))
  {
    // Template: declare a keyword for type wchar_t.
    Keyword (wchar_t) k;

    if (line[wcslen (line) - 1] == L'\n')
      line[wcslen (line) - 1] = L' ';   // keywords end with ' \0'
    line[1] = towlower (line[1]);
    // The macro helper `ACM_KEYWORD_SET (keyword,symbols,length)` can be used to initialize keywords with a single statement.
    ACM_KEYWORD_SET (k, line, wcsnlen (line, sizeof (line) / sizeof (*line)));
    // 5. Add keywords to the state machine calling `ACM_register_keyword()`, one at a time, repeatedly.
    //    An optional third argument, if not 0, can point to a value to be associated to the registerd keyword.
    //    An optional fourth argument, if not 0, provides a pointer to a destructor to be used to destroy the associated value.
    //    Values are allocated in the user program.
    //    This memory will be freed by the function passed as the 4th argument (here 'free', but it could be a user defined finction).
    //    That function will be called for each registered keyword by ACM_release.
    size_t *v = malloc (sizeof (*v));

    // Initialize the value associated to the keyword.
    *v = 0;
    ACM_register_keyword (M, k, v, free);
  }
  printf ("Elapsed CPU time for processing keywords: %f s.\n", (clock () - myclock) * 1.0 / CLOCKS_PER_SEC);

  fclose (stream);

  // 7. Initialize a state with `ACM_reset (machine)`
  state = ACM_reset (M);
  // 8. Inject symbols of the text, one at a time by calling `ACM_match (state, symbol)`.
  ACM_match (state, L' ');
  // `ACM_nb_keywords (machine)` yields the number of registered keywords.
  printf ("[%zu] keywords registered.\n", ACM_nb_keywords (M));

  stream = fopen ("mrs_dalloway.txt", "r");
  if (stream == 0)
    exit (EXIT_FAILURE);

  myclock = clock ();
  for (wint_t wc; (wc = fgetwc (stream)) != WEOF;)
  {
    // 8. Inject symbols of the text, one at a time by calling `ACM_match (state, symbol)`.
    // 9. After each insertion of a symbol, check the returned value to know if the last inserted symbols match at least one keyword.
    size_t nb = ACM_match (state, wc);

    if (nb)
    {
      for (size_t j = 0; j < nb; j++)
      {
        void *v;

        // 10. If matches were found, retrieve them calling `ACM_get_match ()` for each match.
        //     An optional fourth argument will point to the pointer to the value associated with the matching keyword.
        ACM_get_match (state, j, 0, &v);
        // Increment the value associated to the keyword.
        (*(size_t *) v)++;
      }
    }
  }
  printf ("Elapsed CPU time for scaning text for keywords: %f s.\n", (clock () - myclock) * 1.0 / CLOCKS_PER_SEC);
  printf ("\n");

  fclose (stream);

  // `ACM_foreach_keyword (machine, function)` applies a function (`void (*function) (Keyword (T), void *)`) on each registerd keyword.
  // Display keywords and their associated value.
  ACM_foreach_keyword (M, print_match);

  printf ("\n");

  // 12. After usage, release the state machine calling ACM_release() on M.
  //     ACM_release also frees the values associated to registered keywords.
  ACM_release (M);
}
