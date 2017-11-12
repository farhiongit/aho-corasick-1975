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

/// @see Efficient String Matching: An Aid to Bibliographic Search - Aho and Corasick - June 1975
/// Compared to the implemenation proposed by Aho and Corasick, this one adds two small enhancements:
/// - This implementation does not stores output keywords associated to states.
///   It rather reconstructs matching keywords by traversing the branch of the tree backward (see ACM_get_match).
/// - This implementation permits to search for keywords even though all keywords have not been registered yet.
///   To achieve this, failure states are reconstructed after every registration of a next keyword
///   (see ACM_register_keyword which alternates calls to algorithms 2 and 3.)

// The interface of aho_corasick.h will be linked as static functions, not exportable to other compilation unit.
#define PRIVATE_MODULE

#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <ctype.h>

// 1. Define the type ACM_SYMBOL with the type of symbols that constitute keywords.
#define ACM_SYMBOL char

// A comparator of symbols can be user defined instead of the standard '==' operator.
static int nocaseeq (char k, char t);

#define ACM_SYMBOL_EQ_OPERATOR nocaseeq

// 2. Include "aho_corasick.h"
#include "aho_corasick.h"

static int nocase;
static int
nocaseeq (char k, char t)
{
  if (nocase)
    return k == tolower (t);
  else
    return k == t;
}

#ifndef ACM_ASSOCIATED_VALUE
static void
print_match (MatchHolder match)
#else
static void
print_match (MatchHolder match, void * value)
#endif
{
  if (ACM_MATCH_LENGTH (match))
  {
    printf ("{");
    for (size_t k = 0; k < ACM_MATCH_LENGTH (match); k++)
      printf ("%c", ACM_MATCH_SYMBOLS (match)[k]);
    printf ("}");
  }
}

// A unit test
int
main (void)
{

  /****************** First test ************************/
  nocase = 1;

  // 3. The machine state is initialized with 0 before any call to ACM_register_keyword.
  InitialState *M = 0;

// Declares all the keywords
// "hers" appears twice but will be registerd once.
#define LIST_OF_KEYWORDS  \
  X(M, 'h', 'e')  \
  X(M, 's', 'h', 'e')  \
  X(M, 's', 'h', 'e', 'e', 'r', 's')  \
  X(M, 'h', 'i', 's')  \
  X(M, 'h', 'i')  \
  X(M, 'h', 'e', 'r', 's')  \
  X(M, 'u', 's', 'h', 'e', 'r', 's')  \
  X(M, 'a', 'b', 'c', 'd', 'e')  \
  X(M, 'b', 'c', 'd')  \
  X(M, 'h', 'e', 'r', 's')  \
  X(M, 'z', 'z')  \
  X(M, 'c')  \
  X(M, 'p', 'e', 'n')  \

// Function applied to register a keyword in the state machine
#ifdef ACM_ASSOCIATED_VALUE
#define EXTRA ,0
#else
#define EXTRA
#endif

#define X(MACHINE, ...) \
  {  \
    Keyword VAR;  \
    ACM_SYMBOL _VAR[] = { __VA_ARGS__ };  \
    ACM_KEYWORD_SET (VAR, _VAR, sizeof (_VAR) / sizeof (*_VAR));  \
    InitialState * is;  \
    if ((is = ACM_register_keyword (MACHINE, VAR EXTRA EXTRA)))  \
    {  \
      MACHINE = is;  \
      print_match (VAR EXTRA);  \
    }  \
    else  \
      printf  ("X");  \
  }

  // 4. Add keywords (of type Keyword) to the state machine calling ACM_register_keyword() repeatedly.
  LIST_OF_KEYWORDS;             // Ending ';' is not necessary and is only here for 'indent' to switch to correct indentation.
#undef X

  printf (" [%zu]\n", ACM_nb_keywords (M));
  {
    Keyword kw = {.letter = "sheers",.length = 6 };
    ACM_unregister_keyword (M, kw);
  }
  {
    Keyword kw = {.letter = "hi",.length = 2 };
    ACM_unregister_keyword (M, kw);
  }
  {
    Keyword kw = {.letter = "pen",.length = 3 };
    ACM_unregister_keyword (M, kw);
  }
  {
    Keyword kw = {.letter = "zz",.length = 2 };
    ACM_unregister_keyword (M, kw);
  }

  ACM_foreach_keyword (M, print_match);
  printf (" [%zu]\n", ACM_nb_keywords (M));

  // The text where keywords are searched for.
  char text[] = "He found his pencil, but she could not find hers (hi! ushers !!)\nabcdz\nbcz\ncz\n_abcde_";

  // 5. Initialize an internal machine state to M.
  // Actual state of the machine, initialized with the machine state before any call to ACM_change_state.
  InternalState *s = M;

  // 6. Initialize a match holder with ACM_MATCH_INIT before the first use by ACM_get_match.
  MatchHolder match;

  ACM_MATCH_INIT (match);
  for (size_t i = 0; i < strlen (text); i++)
  {
    printf ("%c", text[i]);

    // 7. Inject symbols of the text, one at a time by calling ACM_change_state() on s. Cycle on machine states.
    ACM_CHANGE_STATE (s, text[i]);

    // 8. After each insertion of a symbol, call ACM_nb_matches() on the internal state s to check if the last inserted symbols match a keyword.
    // Get the matching keywords for the actual state of the machine.
    size_t nb_matches = ACM_nb_matches (s);

    // 9. If matches were found, retrieve them calling ACM_get_match() for each match.
    for (size_t j = 0; j < nb_matches; j++)
    {
      // Get the ith matching keyword for the actual state of the machine.
#ifndef ACM_ASSOCIATED_VALUE
      printf ("[%zu]", ACM_get_match (s, j, &match));
#else
      printf ("[%zu]", ACM_get_match (s, j, &match, 0));
#endif

      // Display matching pattern
      print_match (match EXTRA);
    }
  }

  // 10. After the last call to ACM_get_match(), release to keyword by calling ACM_MATCH_RELEASE.
  ACM_MATCH_RELEASE (match);

  printf ("\n");

  // 11. Release resources allocated by the state machine after usage.
  ACM_release (M);

  /****************** Second test ************************/
  nocase = 0;

  char message[] = "hello\n, this\nis\na\ngreat\nmessage\n, bye\n !";

  FILE *stream;

  stream = fopen ("words", "r");
  if (stream == 0)
    exit (EXIT_FAILURE);

  char *line = 0;
  size_t len = 0;
  size_t read = 0;

  // 3. M is prepared for reuse.
  M = 0;

  // 4. Add keywords (of type Keyword) to the state machine calling ACM_register_keyword() repeatedly.
  for (int stage = 1; stage <= 2; stage++)
  {
    for (; (read < 50000 || stage == 2) && getline (&line, &len, stream) != -1; read++)
    {
      InitialState *s;
      Keyword k;

      ACM_KEYWORD_SET (k, line, strlen (line)); // keywords end with newline
#ifdef ACM_ASSOCIATED_VALUE
      size_t *v = malloc (sizeof (*v));

      *v = ACM_nb_keywords (M);

      // Values can be associated registered keywords. Values are allocated in the user program.
      // This memory will be freed by the function passed as the 4th argument (here 'free', but it could be a user defined finction).
      // That function will be called for each registered keyword by ACM_release.
      if ((s = ACM_register_keyword (M, k, v, free)))
#else
      if ((s = ACM_register_keyword (M, k)))
#endif
        M = s;
    }

    printf ("[%zu]: ", ACM_nb_keywords (M));

    // 5. Initialize an internal machine state to M.
    // Internal state MUST BE set to M after a keyword has been inserted by ACM_register_keyword.
    s = M;

    for (size_t i = 0; i < strlen (message); i++)
    {
      if (message[i] != '\n')
        printf ("%c", message[i]);
      // 7. Inject symbols of the text, one at a time by calling ACM_change_state() on s.
      ACM_CHANGE_STATE (s, message[i]);
      // 8. After each insertion of a symbol, call ACM_nb_matches() on the internal state s to check if the last inserted symbols match a keyword.
      size_t nb = ACM_nb_matches (s);

      if (nb)
      {
        printf ("< ");

#ifdef ACM_ASSOCIATED_VALUE
        // 9. If matches were found, retrieve them calling ACM_get_match() for each match.
        for (size_t j = 0; j < nb; j++)
        {
          void *v;
          size_t u = ACM_get_match (s, j, 0, &v);

          assert (*(size_t *) v == u);
        }
#endif
      }
    }

    printf ("\n");
  }

  free (line);
  fclose (stream);

  // 11. After usage, release the state machine calling ACM_release() on M.
  // ACM_release also frees the values associated to registered keywords.
  ACM_release (M);
}
