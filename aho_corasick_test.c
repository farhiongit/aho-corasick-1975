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
#define ACM_SYMBOL char

#include "aho_corasick.h"

static void
print_keyword (Keyword match)
{
  if (match.length)
  {
    printf ("[");
    for (size_t k = 0; k < match.length; k++)
      printf ("%c", match.letter[k]);
    printf ("]");
  }
}

// A unit test
int
main (void)
{
  // The machine state is initialized with 0 before any call to ACM_register_keyword.
  InitialState *M = 0;

// Declares all the keywords
// "hers" appears twice but will be registerd once.
#define LIST_OF_KEYWORDS  \
  X(M, 'h', 'e')  \
  X(M, 's', 'h', 'e')  \
  X(M, 'h', 'i', 's')  \
  X(M, 'h', 'e', 'r', 's')  \
  X(M, 'u', 's', 'h', 'e', 'r', 's')  \
  X(M, 'a', 'b', 'c', 'd', 'e')  \
  X(M, 'b', 'c', 'd')  \
  X(M, 'h', 'e', 'r', 's')  \
  X(M, 'c')  \

// Function applied to register a keyword in the state machine
// We could also have more conveniently written something like keyword = {.letter = "string", .length = strlen ("string") }
#define X(MACHINE, ...) \
  {  \
    Keyword VAR;  \
    ACM_SYMBOL _VAR[] = { __VA_ARGS__ };  \
    ACM_KEYWORD_SET (VAR, _VAR, sizeof (_VAR) / sizeof (*_VAR));  \
    InitialState * is;  \
    if ((is = ACM_register_keyword (MACHINE, VAR)))  \
    {  \
      MACHINE = is;  \
      print_keyword (VAR);  \
    }  \
    else  \
      printf  ("X");  \
  }

  // Registers all keywords in the state machine
  LIST_OF_KEYWORDS;             // Ending ';' is not necessary and is only here for 'indent' to switch to correct indentation.
#undef X

  printf (" {%zu}\n", ACM_nb_keywords (M));
  // The text where keywords are searched for.
  char text[] = "But, as he found his pencil, she could not find hers (ask ushers !!)\nabcdz\nbcz\ncz\n_abcde_";

  // Actual state of the machine, initialized with the machine state before any call to ACM_change_state.
  InternalState *s = M;

  // Initialize keyword to 0.
  Keyword match = { 0, 0 };
  for (size_t i = 0; i < strlen (text); i++)
  {
    printf ("%c", text[i]);

    // Cycle on machine states
    s = ACM_change_state (s, text[i]);

    // Get the matching keywords for the actual state of the machine.
    for (size_t j = 0; j < ACM_nb_matches (s); j++)
    {
      // Get the ith matching keyword for the actual state of the machine.
      printf ("{%zu}", ACM_get_match (s, j, &match));

      // Display matching pattern
      print_keyword (match);
    }
  }

  // Free keyword after usage;
  ACM_KEYWORD_RELEASE (match);
  match.length = 0;

  printf ("\n");

  // Release resources allocated by the state machine after usage.
  ACM_release (M);
  // M is prepared for reuse.
  M = 0;

  char message[] = "hello\n, this\nis\na\ngreat\nmessage\n, bye\n !";

  FILE *stream;

  stream = fopen ("words", "r");
  if (stream == 0)
    exit (EXIT_FAILURE);

  char *line = 0;
  size_t len = 0;

  for (int stage = 1; stage <= 2; stage++)
  {
    for (size_t read = 0; (read < 50000 || stage == 2) && getline (&line, &len, stream) != -1; read++)
    {
      InitialState *s;
      Keyword k;

      ACM_KEYWORD_SET (k, line, strlen (line)); // keywords end with newline
      if ((s = ACM_register_keyword (M, k)))
        M = s;
    }

    printf ("{%zu}: ", ACM_nb_keywords (M));

    // Internal state MUST BE set to M after a keyword has been inserted by ACM_register_keyword.
    s = M;
    for (size_t i = 0; i < strlen (message); i++)
      if (ACM_nb_matches (s = ACM_change_state (s, message[i])))
        printf ("%zu ", i);

    printf ("\n");
  }

  free (line);
  fclose (stream);

  ACM_release (M);
}
