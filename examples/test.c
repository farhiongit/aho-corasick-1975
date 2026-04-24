#include "aho_corasick.h"
int
main (void) {
  ACMachine *machine = acm_create (ACM_CMP_DEFAULT, &(size_t){ sizeof (char) }, 0);
  ACState *state = acm_initiate (machine);
  char *words[] = { "he", "she", "his", "hers" };
  for (size_t i = 0; i < sizeof (words) / sizeof (*words); i++) {
    for (char *p = words[i]; *p; p++)
      acm_insert_letter_of_keyword (&state, p);
    acm_insert_end_of_keyword (&state, 0, 0);
  }
  const char *text = "To ushers: he found his pencil, but she could not find hers.";
  printf ("%s\n", text);
  MatchHolder matcher;
  acm_matcher_init (&matcher);
  const ACState *cst_state = acm_initiate (machine);
  for (size_t i = 0; text[i]; i++)
    for (size_t j = acm_match (&cst_state, &text[i]); j > 0; j--) {
      acm_get_match (cst_state, j - 1, &matcher);
      printf (" %zu:", i + 2 - matcher.length);
      for (size_t k = 0; k < matcher.length; k++)
        printf ("%c", *(const char *)matcher.letters[k]);
    }
  printf ("\n");
  acm_matcher_release (&matcher);
  acm_release (machine);
}
