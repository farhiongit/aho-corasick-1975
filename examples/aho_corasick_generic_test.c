#undef NDEBUG
#include "aho_corasick.h"
#include <assert.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <wchar.h>
#include <wctype.h>

static int current_pos;

static void
print_keyword (const wchar_t *word, size_t length) {
  printf ("{'");
  for (size_t k = 0; k < length; k++)
    printf ("%lc", (wint_t)word[k]);
  printf ("'}");
}

// Must return the number of shifts to the right (that is the number of printed characters)
static int
print_wchar_t (FILE *f, const void *wc) {
  return fprintf (f, "%lc", *(const wint_t *)wc);
}

static void
print_match (MatchHolder match, void *value) {
  // Filter matching keywords which value is equal to 0.
  if (value && *(size_t *)value == 0)
    return;
  if (match.length) {
    current_pos += printf ("{");
    current_pos += printf ("'");
    for (size_t k = 0; k < match.length; k++)
      current_pos += printf ("%lc", *(const wint_t *)match.letters[k]);

    current_pos += printf ("'");
    current_pos += printf ("[%zu]", match.rank);
    if (value) {
      current_pos += printf ("=");
      current_pos += printf ("%zu", *(size_t *)value);
    }
    current_pos += printf ("}");
  }
}

// User defined alphabet only comparison:
// 3. Optionally, user defined operators can be specified.
// User defined case insensitive comparison:
static int words;
static int
walphaeq (wchar_t k, wchar_t t) {
  if (words) {
    int nak = !iswalpha ((wint_t)k);
    int nat = !iswalpha ((wint_t)t);
    if (nak)
      return nat;
    else if (nat)
      return 0;
  }

  return (wchar_t)towlower ((wint_t)k) == (wchar_t)towlower ((wint_t)t);
}

static int
alphaeq (const void *k, const void *t, void *eq_arg) {
  (void)eq_arg;
  return walphaeq (*(const wchar_t *)k, *(const wchar_t *)t);
}

int
main (void) {
  setlocale (LC_ALL, "");

  /****************** First test ************************/
  // This test constructs and plays with the graph from the original paper of Aho-Corasick.
  // The text where keywords are searched for.
  static wchar_t text[] = L"He found his pencil, but she could not find hers (Hi! Ushers !!)";

  // 4. Initialise a state machine of type ACMachine (T) using ACM_create (T):
  ACMachine *M = acm_create (ACM_EQ_DEFAULT, &(size_t){ sizeof (*text) } /* creates a static variable on the fly */, free);
  assert (acm_match (&(ACState *){ acm_initiate (M) }, &(wchar_t){ L'a' }) == 0); // No keyword in the dictionary yet.

  // Declares all the keywords
  // "hers" appears twice and will be registered twice, replacing the first registration.
#define LIST_OF_KEYWORDS                      \
  do {                                        \
    X (M, L'h', L'e')                         \
    X (M, L's', L'h', L'e')                   \
    X (M, L's', L'h', L'e', L'e', L'r', L's') \
    X (M, L'h', L'i', L's')                   \
    X (M, L'h', L'i')                         \
    X (M, L'h', L'e', L'r', L's')             \
    X (M, L'u', L's', L'h', L'e', L'r', L's') \
    X (M, L'a', L'b', L'c', L'd', L'e')       \
    X (M, L'b', L'c', L'd')                   \
    X (M, L'h', L'e', L'r', L's')             \
    X (M, L'p', L'e', L'n')                   \
    X (M, L'p', L'e', L'n')                   \
  } while (0)

  ACState *state = acm_initiate (M);

// Loop on LIST_OF_KEYWORDS
#define X(MACHINE, ...)                                                                                                           \
  {                                                                                                                               \
    /* 5. Add keywords (of type `Keyword (T)`) to the state machine calling `acm_insert_keyword()`, one at a time, repeatedly. */ \
    size_t *pul;                                                                                                                  \
    wchar_t word[] = { __VA_ARGS__ };                                                                                             \
    for (size_t i = 0; i < sizeof (word) / sizeof (*word); i++) {                                                                 \
      wchar_t *letter = malloc (sizeof (*letter));                                                                                \
      assert (letter);                                                                                                            \
      *letter = word[i];                                                                                                          \
      acm_insert_letter_of_keyword (&state, letter);                                                                              \
    }                                                                                                                             \
    acm_insert_end_of_keyword (&state, (*(pul = malloc (sizeof (*pul))) = __COUNTER__ + 100, pul), free);                         \
    print_keyword (word, sizeof (word) / sizeof (*word));                                                                         \
  }

  // Function applied to register a keyword in the state machine
  LIST_OF_KEYWORDS;
#undef X

  printf (" [%zu]\n", acm_nb_keywords (M));
  acm_print (M, stdout, print_wchar_t);
  acm_foreach_keyword (M, print_match);
  printf (" [%zu]\n", acm_nb_keywords (M));

  print_keyword (text, wcslen (text));
  printf ("\n");

  // 6. Initialise a match holder with `ACM_MATCH_INIT (match)`
  MatchHolder match;
  acm_matcher_init (&match);

  // 7. Initialise a state with `acm_initiate (machine)`
  current_pos = 0;
  state = acm_initiate (M);
  for (size_t i = 0; i < wcslen (text); i++) {
    // 8. Inject symbols of the text, one at a time by calling `ACM_match`.
    size_t nb_matches = acm_match (&state, &text[i]);
    // 9. After each insertion of a symbol, check the returned value to know
    //    if the last inserted symbols match at least one keyword.

    for (size_t j = 0; j < nb_matches; j++) {
      // 10. If matches were found, retrieve them calling `ACM_get_match ()` for each match.
      void *pul;
      size_t rank = acm_get_match (state, j, &match, &pul);
      assert (rank == match.rank);

      if (current_pos > (int)i + 1 - (int)match.length) {
        current_pos = 0;
        printf ("\n");
      }
      for (int tab = current_pos; tab < (int)i + 1 - (int)match.length; tab++)
        current_pos += printf (" ");
      // Display matching pattern
      print_match (match, pul);
    }
  }
  printf ("\n");

  // 11. After the last call to `ACM_get_match ()`, release to match holder by calling `ACM_MATCH_RELEASE (match)`.
  acm_matcher_release (&match);
  acm_release (M);

  /****************** Second test ************************/
  // This test counts the number of time the words in the English dictionnary ("words") appear
  // in the Woolf's book Mrs Dalloway ("mrs_dalloway.txt").
  // The dictionary is built up incrementally from the novel and not list of words.
  words = 1;

  FILE *stream;

  wchar_t line[100] = L" "; // keywords start with ' '

  // 4. Initialise a state machine of type ACMachine (T) using ACM_create (T)
  //    An optional second argument of type EQ_OPERATOR_TYPE(*T*) can specify a user defined equality operator.
  M = acm_create (alphaeq, 0, free);

  stream = fopen ("mrs_dalloway.txt", "r");
  if (stream == 0)
    exit (EXIT_FAILURE);

  clock_t myclock = clock ();
  // 7. Initialise a state with `acm_initiate (machine)`
  state = acm_initiate (M);
  // 8. Inject symbols of the text, one at a time by calling `ACM_match (state, symbol)`.
  acm_match (&state, &(wchar_t){ L' ' });
  line[0] = L' ';
  line[1] = 0;
  for (wint_t wc; (wc = fgetwc (stream)) != WEOF;) {
    if (!iswalpha (wc))
      wc = L' ';
    if (iswupper (wc))
      wc = towlower (wc);
    // 8. Inject symbols of the text, one at a time by calling `ACM_match (state, symbol)`.
    // 9. After each insertion of a symbol, check the returned value to know if the last inserted symbols match at least one keyword.
    size_t nb = acm_match (&state, &wc);
    size_t len = wcsnlen (line, sizeof (line) / sizeof (*line) - 1);
    line[len] = (wchar_t)wc;
    line[len + 1] = L'\0';

    if (nb) {
      for (size_t j = 0; j < nb; j++) {
        void *v;

        // 10. If matches were found, retrieve them calling `ACM_get_match ()` for each match.
        //     An optional fourth argument will point to the pointer to the value associated with the matching keyword.
        acm_get_match (state, j, 0, &v);
        // Increment the value associated to the keyword.
        (*(size_t *)v)++;
      }
      assert (wc == L' ');
      line[0] = L' ';
      line[1] = L'\0';
    } else if (wc == L' ') {
      if (wcscmp (line, L"  ")) {
        // keywords can be added to the state-machine while scanning.
        // The dictionary is built up incrementally from the novel and not list of words.
        ACState *state2 = acm_initiate (M);
        for (size_t i = 0; i < wcsnlen (line, sizeof (line) / sizeof (*line)); i++) {
          wchar_t *letter = malloc (sizeof (*letter));
          assert (letter);
          *letter = line[i];
          acm_insert_letter_of_keyword (&state2, letter);
        }
        size_t *v = malloc (sizeof (*v));
        *v = 1;
        acm_insert_end_of_keyword (&state2, v, free);
      }
      line[0] = L' ';
      line[1] = L'\0';
    }
  } // for (wint_t wc; (wc = fgetwc (stream)) != WEOF;)
  printf ("\nElapsed CPU time for scanning text for keywords: %f s.\n", (double)(clock () - myclock) / CLOCKS_PER_SEC);
  fclose (stream);

  // `ACM_nb_keywords (machine)` yields the number of registered keywords.
  printf ("[%zu] keywords registered.\n", acm_nb_keywords (M));
  // `ACM_foreach_keyword (machine, function)` applies a function (`void (*function) (Keyword (T), void *)`) on each registered keyword.
  // Display keywords and their associated value.
  acm_foreach_keyword (M, print_match);

  printf ("\n");

  // 12. After usage, release the state machine calling ACM_release() on M.
  //     ACM_release also frees the values associated to registered keywords.
  acm_release (M);
}
