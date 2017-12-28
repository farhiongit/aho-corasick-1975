
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
#include "aho_corasick_template_impl.h"

ACM_DECLARE (char)
ACM_DEFINE (char)

static void
print_match (MatchHolder (char) match, void *value)
{
  printf ("{'");
  for (size_t k = 0; k < ACM_MATCH_LENGTH (match); k++)
    printf ("%lc", ACM_MATCH_SYMBOLS (match)[k]);
  printf ("'=%i}", *(int *) value);
}

int
main (void)
{
  ACMachine (char) * M = ACM_create (char);
  Keyword (char) kw;

  ACM_KEYWORD_SET (kw, "1984", 4);
  ACM_register_keyword (M, kw, calloc (1, sizeof (int)), free);

  ACM_KEYWORD_SET (kw, "1985", 4);
  ACM_register_keyword (M, kw, calloc (1, sizeof (int)), free);

  FILE *f = fopen ("googlebooks-eng-all-1gram-20120701-0", "r");
  if (f == 0)
  {
    printf ("Get sample data:\n"
            "  wget http://storage.googleapis.com/books/ngrams/books/googlebooks-eng-all-1gram-20120701-0.gz\n"
            "  gzip -d googlebooks-eng-all-1gram-20120701-0.gz\n");
    exit (EXIT_FAILURE);
  }

  const ACState (char) * state = ACM_reset (M);
  char line[4096];
  while (fgets (line, sizeof (line) / sizeof (*line), f))
    for (char *c = line; *c; c++)
    {
      state = ACM_match (state, *c);
      size_t nb = ACM_nb_matches (state);

      for (size_t j = 0; j < nb; j++)
      {
        void *v;
        ACM_get_match (state, j, 0, &v);
        (*(int *) v)++;
      }
    }
  fclose (f);

  ACM_foreach_keyword (M, print_match);
  printf ("\n");

  ACM_release (M);
}
