#include <stdio.h>
#include "aho_corasick_template_impl.h"

ACM_DECLARE (char);
ACM_DEFINE (char);
int main (void)
{
  ACM_DECL (M, char);
  Keyword (char) kw;

  ACM_KEYWORD_SET (kw, "1984", 4);
  ACM_register_keyword (&M, kw);

  ACM_KEYWORD_SET (kw, "1985", 4);
  ACM_register_keyword (&M, kw);

  size_t nb_matches = 0;
  FILE *f = fopen ("googlebooks-eng-all-1gram-20120701-0", "r");
  char line[4096];
  const ACState (char) *state = ACM_reset (&M);
  while (fgets (line, sizeof (line) / sizeof (*line), f))
    for (char *c = line; *c; c++)
      nb_matches += ACM_match (state, *c);
  fclose (f);
  printf ("%lu\n", nb_matches);
}
