
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
ACM_DECLARE (wchar_t);
ACM_DEFINE (wchar_t);
/* *INDENT-ON* */

static int words;
static int current_pos;

// 3. Optionally, user defined operators can be specified.
// User defined case insensitive comparison:
static int
nocaseeq (wchar_t k, wchar_t t)
{
  return k == (wchar_t) towlower ((wint_t) t);
}

// User defined alphabet only comparison:
static int
alphaeq (wchar_t k, wchar_t t)
{
  if (words)
  {
    int nak = !iswalpha ((wint_t) k);
    int nat = !iswalpha ((wint_t) t);
    if (nak)
      return nat;
    else if (nat)
      return 0;
  }

  return nocaseeq (k, t);
}

static void
print_keyword (Keyword (wchar_t) kw)
{
  printf ("{'");
  for (size_t k = 0; k < ACM_MATCH_LENGTH (kw); k++)
    printf ("%lc", (wint_t) ACM_MATCH_SYMBOLS (kw)[k]);
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
      current_pos += printf ("%lc", (wint_t) ACM_MATCH_SYMBOLS (match)[k]);

    current_pos += printf ("'");
    current_pos += printf ("[%zu]", ACM_MATCH_UID (match));
    if (value)
    {
      current_pos += printf ("=");
      current_pos += printf ("%zu", *(size_t *) value);
    }
    current_pos += printf ("}");
  }
}

// Must return the number of shifts to the right (that is the number of printed characters)
static int
print_wchar_t (FILE *f, wchar_t wc)
{
  return fprintf (f, "%lc", (wint_t) wc);
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

  // 4. Initialise a state machine of type ACMachine (T) using ACM_create (T):
  ACMachine (wchar_t) * M = ACM_create (wchar_t);

// Declares all the keywords
// "hers" appears twice and will be registered twice, replacing the first registration.
#define LIST_OF_KEYWORDS  \
  do {\
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
  X(M, L'x', L'y')  \
  } while (0)

// Function applied to register a keyword in the state machine
#define X(MACHINE, ...) \
  {  \
    Keyword (wchar_t) VAR;  \
    size_t *pul;  \
    wchar_t _VAR[] = { __VA_ARGS__ };  \
    ACM_KEYWORD_SET (VAR, _VAR, sizeof (_VAR) / sizeof (*_VAR));  \
    /* 5. Add keywords (of type `Keyword (T)`) to the state machine calling `ACM_register_keyword()`, one at a time, repeatedly. */ \
    if (ACM_register_keyword (MACHINE, VAR, (*(pul = malloc(sizeof (*pul))) = __COUNTER__ + 100, pul)) && \
        ACM_is_registered_keyword (MACHINE, VAR, 0))  \
      print_keyword (VAR);  \
    else  \
      printf  ("X");  \
  }

  // Function applied to register a keyword in the state machine
  LIST_OF_KEYWORDS;
#undef X

  printf (" [%zu]\n", ACM_nb_keywords (M));
  ACM_print (M, stdout, print_wchar_t);
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
      void *pul;
      size_t rank =
        ACM_get_match (state, j, &match, &pul);
      ACM_ASSERT (rank == ACM_MATCH_UID (match));

      // `ACM_MATCH_LENGTH (match)` and `ACM_MATCH_SYMBOLS (match)` can be used to get the length and the content of a retreieved match.
      if (current_pos > (int) i + 1 - (int) ACM_MATCH_LENGTH (match))
      {
        current_pos = 0;
        printf ("\n");
      }
      for (int tab = current_pos; tab < (int) i + 1 - (int) ACM_MATCH_LENGTH (match); tab++)
        current_pos += printf (" ");
      // Display matching pattern
      print_match (match, pul);
    }
  }

  // 11. After the last call to `ACM_get_match ()`, release to match holder by calling `ACM_MATCH_RELEASE (match)`.
  ACM_MATCH_RELEASE (match);

  // 12. Release the state machine calling ACM_release() on M.
  ACM_release (M);

  printf ("\n");

  /****************** Second test ************************/
  // This test counts the number of time the words in the English dictionnary ("words") appear
  // in the Woolf's book Mrs Dalloway ("mrs_dalloway.txt").
  words = 1;

  FILE *stream;

  wchar_t line[100] = L" ";     // keywords start with ' '

  // 4. Initialise a state machine of type ACMachine (T) using ACM_create (T)
  //    An optional second argument of type EQ_OPERATOR_TYPE(*T*) can specify a user defined equality operator.
  M = ACM_create (wchar_t, alphaeq);

  stream = fopen ("mrs_dalloway.txt", "r");
  if (stream == 0)
    exit (EXIT_FAILURE);

  clock_t myclock = clock ();
  // 7. Initialise a state with `ACM_reset (machine)`
  state = ACM_reset (M);
  // 8. Inject symbols of the text, one at a time by calling `ACM_match (state, symbol)`.
  ACM_match (state, L' ');
  line[0] = L' ';
  line[1] = 0;
  for (wint_t wc; (wc = fgetwc (stream)) != WEOF;)
  {
    if (!iswalpha (wc))
      wc = L' ';
    if (iswupper (wc))
      wc = towlower (wc);
    // 8. Inject symbols of the text, one at a time by calling `ACM_match (state, symbol)`.
    // 9. After each insertion of a symbol, check the returned value to know if the last inserted symbols match at least one keyword.
    size_t nb = ACM_match (state, (wchar_t) wc);
    size_t len = wcsnlen (line, sizeof (line) / sizeof (*line) - 1);
    line[len] = (wchar_t) wc;
    line[len + 1] = L'\0';

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
      assert (wc == L' ');
      line[0] = L' ';
      line[1] = L'\0';
    }
    else if (wc == L' ')
    {
      if (wcscmp (line, L"  "))
      {
        Keyword (wchar_t) k;   // Keywords are made of wchar_t.
        ACM_KEYWORD_SET (k, line, wcsnlen (line, sizeof (line) / sizeof (*line)));
        size_t *v = malloc (sizeof (*v));
        *v = 1;
        ACM_register_keyword (M, k, v, free);  // keywords can be added to the state-machine while scanning.
      }
      line[0] = L' ';
      line[1] = L'\0';
    }
  }  // for (wint_t wc; (wc = fgetwc (stream)) != WEOF;)
  printf ("Elapsed CPU time for scanning text for keywords: %f s.\n", (double) (clock () - myclock) / CLOCKS_PER_SEC);
  printf ("\n");

  fclose (stream);

  // `ACM_nb_keywords (machine)` yields the number of registered keywords.
  printf ("[%zu] keywords registered.\n", ACM_nb_keywords (M));
  // `ACM_foreach_keyword (machine, function)` applies a function (`void (*function) (Keyword (T), void *)`) on each registered keyword.
  // Display keywords and their associated value.
  ACM_foreach_keyword (M, print_match);

  printf ("\n");

  // 12. After usage, release the state machine calling ACM_release() on M.
  //     ACM_release also frees the values associated to registered keywords.
  ACM_release (M);
}
