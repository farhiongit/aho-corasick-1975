
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

#include "aho_corasick_template_impl.h"

/* *INDENT-OFF* */
// Template: Declare and define instanciations of template for as many types needed.
ACM_DECLARE (wchar_t)
ACM_DEFINE (wchar_t)
/* *INDENT-ON* */

static int words;
static int current_pos;

// User defined case insensitive comparisons
static int
nocaseeq (wchar_t k, wchar_t t)
{
  return k == towlower (t);
}

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
print_match (MatchHolder (wchar_t) match, void *value)
{
  if (value && *(size_t *) value == 0)
    return;
  if (ACM_MATCH_LENGTH (match))
  {
    current_pos += printf ("{'");
    for (size_t k = 0; k < ACM_MATCH_LENGTH (match); k++)
      current_pos += printf ("%lc", ACM_MATCH_SYMBOLS (match)[k]);

    current_pos += printf ("'");
    if (value)
    {
      current_pos += printf ("=");
      current_pos += printf ("%lu", *(size_t *) value);
    }
    current_pos += printf ("}");
  }
}

// A unit test
int
main (void)
{
  setlocale (LC_ALL, "");
  // Template: Declare an equality operator for type wchar_t.
  SET_EQ_OPERATOR (wchar_t, nocaseeq);

  /****************** First test ************************/
  // This test constructs and plays with the graph from the original paper of Aho-Corasick.
  // The text where keywords are searched for.
  wchar_t text[] =
    L"He found his pencil, but she could not find hers (Hi! Ushers !!) ; abcdz ; bCz ; cZZ ; _abcde_xyzxyt";

  // 3. The machine state is initialized with 0 before any call to ACM_register_keyword.
  // Template: Creates a new machine for type wchar_t.
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
#define EXTRA ,0

#define X(MACHINE, ...) \
  {  \
    Keyword (wchar_t) VAR;  \
    wchar_t _VAR[] = { __VA_ARGS__ };  \
    ACM_KEYWORD_SET (VAR, _VAR, sizeof (_VAR) / sizeof (*_VAR));  \
    /* 4. Add keywords (of type Keyword) to the state machine calling ACM_register_keyword() repeatedly. */ \
    if (ACM_register_keyword (MACHINE, VAR EXTRA EXTRA))          \
      print_match (VAR EXTRA);  \
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

  // 5. Initialize a match holder with ACM_MATCH_INIT before the first use by ACM_get_match.
  // Template: define a receptable for found matches.
  MatchHolder (wchar_t) match;

  ACM_MATCH_INIT (match);
  Keyword (wchar_t) kw = {.letter = text,.length = wcslen (text) };
  current_pos = 0;
  print_match (kw EXTRA);

  for (size_t i = 0; i < wcslen (text); i++)
  {
    // 6. Inject symbols of the text, one at a time by calling ACM_nb_matches().
    //    After each insertion of a symbol, check if the last inserted symbols match a keyword.
    //    Get the matching keywords for the actual state of the machine.
    size_t nb_matches = ACM_nb_matches (M, text[i]);

    // 7. If matches were found, retrieve them calling ACM_get_match() for each match.
    for (size_t j = 0; j < nb_matches; j++)
    {
      // Get the ith matching keyword for the actual state of the machine.
      size_t rank =
        ACM_get_match (M, j, &match, 0);

      // Display matching pattern
      if (current_pos > i + 1 - ACM_MATCH_LENGTH (match))
      {
        current_pos = 0;
        printf ("\n");
      }
      for (size_t tab = current_pos; tab < i + 1 - ACM_MATCH_LENGTH (match); tab++)
        current_pos += printf (" ");
      print_match (match EXTRA);
      current_pos += printf ("[%zu]", rank);
    }
  }

  // 8. After the last call to ACM_get_match(), release to keyword by calling ACM_MATCH_RELEASE.
  ACM_MATCH_RELEASE (match);

  // 9. Release resources allocated by the state machine after usage.
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

  // 3. M is prepared for reuse.
  // Template: creates a new machine for type wchar_t, with a delaration of a specific equality operator.
  M = ACM_create (wchar_t, alphaeq);

  clock_t myclock = clock ();
  while (fgetws (line + 1, sizeof (line) / sizeof (*line) - 1, stream))
  {
    // Template: declare a keyword for type wchar_t.
    Keyword (wchar_t) k;

    if (line[wcslen (line) - 1] == L'\n')
      line[wcslen (line) - 1] = L' ';   // keywords end with ' \0'
    line[1] = towlower (line[1]);
    ACM_KEYWORD_SET (k, line, wcsnlen (line, sizeof (line) / sizeof (*line)));
    // 4. Add keywords (of type Keyword) to the state machine calling ACM_register_keyword() repeatedly.
    // Values can be associated registered keywords. Values are allocated in the user program.
    // This memory will be freed by the function passed as the 4th argument (here 'free', but it could be a user defined finction).
    // That function will be called for each registered keyword by ACM_release.
    size_t *v = malloc (sizeof (*v));

    // Initialize the value associated to the keyword.
    *v = 0;
    ACM_register_keyword (M, k, v, free);
  }
  printf ("Elapsed CPU time for processing keywords: %f s.\n", (clock () - myclock) * 1.0 / CLOCKS_PER_SEC);

  fclose (stream);

  // 6. Inject symbols of the text, one at a time by calling ACM_change_state().
  ACM_nb_matches (M, L' ');
  printf ("[%zu] keywords registered.\n", ACM_nb_keywords (M));

  stream = fopen ("mrs_dalloway.txt", "r");
  if (stream == 0)
    exit (EXIT_FAILURE);

  myclock = clock ();
  for (wint_t wc; (wc = fgetwc (stream)) != WEOF;)
  {
    // 6. Inject symbols of the text, one at a time by calling ACM_nb_matches().
    //    After each insertion of a symbol, check if the last inserted symbols match a keyword.
    size_t nb = ACM_nb_matches (M, wc);

    if (nb)
    {
      // 7. If matches were found, retrieve them calling ACM_get_match() for each match.
      for (size_t j = 0; j < nb; j++)
      {
        void *v;
        ACM_get_match (M, j, 0, &v);
        // Increment the value associated to the keyword.
        (*(size_t *) v)++;
      }
    }
  }
  printf ("Elapsed CPU time for scaning text for keywords: %f s.\n", (clock () - myclock) * 1.0 / CLOCKS_PER_SEC);
  printf ("\n");

  fclose (stream);

  // Display keywords and their associated value.
  ACM_foreach_keyword (M, print_match);

  printf ("\n");

  // 9. After usage, release the state machine calling ACM_release() on M.
  // ACM_release also frees the values associated to registered keywords.
  ACM_release (M);
}
