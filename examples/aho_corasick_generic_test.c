#undef NDEBUG
#include "aho_corasick.h"
#include <assert.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
  if (match.length) {
    current_pos += printf ("{");
    current_pos += printf ("'");
    for (size_t k = 0; k < match.length; k++)
      current_pos += printf ("%lc", *(const wint_t *)match.letters[k]);

    current_pos += printf ("'");
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
static int
alphaneq (const void *k, const void *t, const void *eq_arg) {
  (void)eq_arg;
  wint_t lk = towlower (*(const wint_t *)k);
  wint_t lt = towlower (*(const wint_t *)t);
  return lk > lt ? 1 : (lk < lt ? -1 : 0);
}

int
main (int argc, char **argv) {
  argc = argc > 1 ? atoi (argv[1]) : ~0;
  setlocale (LC_ALL, "");
  printf ("Incremental string matching (Meyer, 1985) %sin use.\n", ACM_INCREMENTAL_STRING_MATCHING ? "" : "NOT ");
  clock_t myclock;
  if (argc & 1 << (1 - 1)) {
    printf ("/****************** First test ************************/\n");
    // This test constructs and plays with the graph from the original paper of Aho-Corasick.
    // The text where keywords are searched for.
    static wchar_t text[] = L"He found his pencil, but she could not find hers (Hi! Ushers !! --abcdefgh--)";

    // 4. Initialise a state machine of type ACMachine (T) using ACM_create (T):
    ACMachine *M1 = acm_create (alphaneq, 0, 0 /* letters are automatically allocated */);
    assert (acm_match (&(const ACState *){ acm_initiate (M1) }, &(wchar_t){ L'a' }) == 0); // No keyword in the dictionary yet.

    // Declares all the keywords
#define LIST_OF_KEYWORDS \
  X (L"he", 1, 0)        \
  X (L"she", 1, 1)       \
  X (L"sheers", 1, 2)    \
  X (L"his", 1, 3)       \
  X (L"hi", 1, 4)        \
  X (L"hers", 1, 5)      \
  X (L"ushers", 1, 6)    \
  X (L"abcde", 1, 7)     \
  X (L"bcd", 1, 8)       \
  X (L"hers", 0, 14)     \
  X (L"hen", 1, 10)      \
  X (L"hen", 0, 21)      \
  X (L"bcdef", 1, 12)    \
  X (L"pen", 1, 13)      \
  X (L"cdefg", 1, 14)    \
  X (L"pen", 0, 28)      \
  X (L"bcd", 0, 24)      \
  X (L"abc", 1, 17)      \
  X (L"abcd", 1, 18)     \
  X (L"abcde", 0, 26)    \
  X (L"bcde", 1, 20)     \
  X (L"cde", 1, 21)      \
  X (L"cd", 1, 22)       \
  X (L"bc", 1, 23)       \
  X (L"u", 1, 24)        \
  X (L"uu", 1, 25)

    ACState *s1 = acm_initiate (M1);
#define X(...) +1
    wchar_t *words[LIST_OF_KEYWORDS]; /* letters are automatically allocated */
#undef X

    // Loop on LIST_OF_KEYWORDS
    size_t index = 0;
    void *prev, *val;
#define X(KW, CHECK, SUM)                                                                                                            \
  words[index] = (wchar_t[]){ (KW) }; /* letters are automatically allocated */                                                      \
  for (size_t i = 0; i < wcslen (words[index]); i++)                                                                                 \
    acm_insert_letter_of_keyword (&s1, &words[index][i]);                                                                            \
  prev = acm_insert_end_of_keyword (&s1, (val = &(size_t){ index } /* automatically allocated unnamed object in block scope */), 0); \
  assert ((prev ? 0 : 1) == (CHECK));                                                                                                \
  if (prev)                                                                                                                          \
    *(size_t *)prev += *(size_t *)val; /* User defined appender */                                                                   \
  assert ((prev ? *(size_t *)prev : *(size_t *)val) == (SUM));                                                                       \
  print_keyword (words[index], wcslen (words[index]));                                                                               \
  index++;

    LIST_OF_KEYWORDS;
#undef X

    printf (" [%zu]\n", acm_nb_keywords (M1));
    acm_foreach_keyword (M1, print_match);
    printf (" [%zu]\n", acm_nb_keywords (M1));
    acm_print (M1, stdout, print_wchar_t);

    print_keyword (text, wcslen (text));
    printf ("\n");

    // 6. Initialise a match holder.
    MatchHolder m1;
    acm_matcher_init (&m1);

    // 7. Initialise a state with `acm_initiate (machine)`
    current_pos = 0;
    const ACState *cs1 = acm_initiate (M1);
    for (size_t i = 0; i < wcslen (text); i++) {
      // 8. Inject symbols of the text, one at a time.
      size_t nb_matches = acm_match (&cs1, &text[i]);
      // 9. After each insertion of a symbol, check the returned value to know
      //    if the last inserted symbols match at least one keyword.

      for (size_t j = 0; j < nb_matches; j++) {
        // 10. If matches were found, retrieve them for each match.
        void *pul;
        acm_get_match (cs1, j, &m1, &pul);

        if (current_pos > (int)i + 1 - (int)m1.length) {
          current_pos = 0;
          printf ("\n");
        }
        for (int tab = current_pos; tab < (int)i + 1 - (int)m1.length; tab++)
          current_pos += printf (" ");
        // Display matching pattern
        print_match (m1, pul);
      }
    }
    printf ("\n");

    // 11. Release to match holder.
    acm_matcher_release (&m1);
    acm_release (M1);
  }

  if (argc & 1 << (2 - 1)) {
    printf ("/****************** Second test ************************/\n");
    // This test counts the number of time the words in the English dictionary ("words") appear
    // in the Woolf's book Mrs Dalloway ("mrs_dalloway.txt").
    // The dictionary is built up incrementally from the novel and not list of words.
    FILE *stream;

    wchar_t line[100] = L" "; // keywords start with ' '

    // 4. Initialise a state machine with a user defined equality operator.
    ACMachine *M3 = acm_create (alphaneq, 0, free /* letters are dynamically allocated */);

    stream = fopen ("mrs_dalloway.txt", "r");
    if (stream == 0)
      exit (EXIT_FAILURE);

    myclock = clock ();
    // 7. Initialise a state with `acm_initiate (machine)`
    const ACState *cs3 = acm_initiate (M3);
    // 8. Inject symbols of the text, one at a time.
    acm_match (&cs3, &(wchar_t){ L' ' });
    line[0] = L' ';
    line[1] = 0;
    for (wint_t wc; (wc = fgetwc (stream)) != WEOF;) {
      if (!iswalpha (wc))
        wc = L' ';
      else
        wc = towlower (wc);
      // 8. Inject symbols of the text, one at a time.
      // 9. After each insertion of a symbol, check the returned value to know if the last inserted symbols match at least one keyword.
      size_t nb = acm_match (&cs3, &wc);
      size_t len = wcsnlen (line, sizeof (line) / sizeof (*line) - 1);
      line[len] = (wchar_t)wc;
      line[len + 1] = L'\0';

      if (nb) {
        for (size_t j = 0; j < nb; j++) {
          void *v;

          // 10. If matches were found, retrieve them for each match.
          //     An optional fourth argument will point to the pointer to the value associated with the matching keyword.
          acm_get_match (cs3, j, 0, &v);
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
          ACState *s3 = acm_initiate (M3);
          for (size_t i = 0; i < wcsnlen (line, sizeof (line) / sizeof (*line)); i++) {
            wchar_t *letter = malloc (sizeof (*letter) /* letters are dynamically allocated */);
            assert (letter);
            *letter = line[i];
            acm_insert_letter_of_keyword (&s3, letter);
          }
          size_t *v = malloc (sizeof (*v));
          *v = 1;
          if (acm_insert_end_of_keyword (&s3, v, free) != 0) // The keyword with an associated value is already in the dictionary.
            free (v);
        }
        line[0] = L' ';
        line[1] = L'\0';
      }
    } // for (wint_t wc; (wc = fgetwc (stream)) != WEOF;)
    printf ("Elapsed CPU time for scanning text for keywords: %f s.\n", (double)(clock () - myclock) / CLOCKS_PER_SEC);
    fclose (stream);

    // Get the number of registered keywords.
    printf ("%zu keywords registered.\n", acm_nb_keywords (M3));
    // Applies a function on each registered keyword (display keywords and their associated value.)
    acm_foreach_keyword (M3, print_match);

    printf ("\n");

    // 12. After usage, release the state machine.
    //     ACM_release also frees the values associated to registered keywords.
    acm_release (M3);
  }

  if (argc & 1 << (3 - 1)) {
    printf ("/****************** Third test ************************/\n");
    const size_t NB_INCEREMENTS = 10;
    const size_t NB_KEYWORDS = 25000;
    const size_t KEYWORD_LENGTH = 7;
    const size_t TEXT_LENGTH = 1000000;
    char ALPHABET[] = "abcdefghijklmnopqrstuvwxyz";
    myclock = clock ();
    ACMachine *M2 = acm_create (ACM_CMP_DEFAULT, &(size_t){ sizeof (char) }, 0);
    ACState *s2 = acm_initiate (M2);        // Initialise once.
    const ACState *cs2 = acm_initiate (M2); // Initialise once.
    size_t nb_keywords = 0;
    for (size_t l = 0; l < NB_INCEREMENTS; l++) {
      for (size_t i = 0; i < NB_KEYWORDS; i++) {
        for (size_t j = 0; j < KEYWORD_LENGTH; j++)
          acm_insert_letter_of_keyword (&s2, &ALPHABET[(size_t)rand () % strlen (ALPHABET)]);
        acm_insert_end_of_keyword (&s2, 0, 0);
      }
      printf ("[%'zu] %'zu new keywords added in %f s.\n", l + 1, acm_nb_keywords (M2) - nb_keywords, (double)(clock () - myclock) / CLOCKS_PER_SEC);
      myclock = clock ();
      nb_keywords = acm_nb_keywords (M2);
      size_t nb_matches = 0;
      for (size_t k = 0; k < TEXT_LENGTH; k++)
        nb_matches += acm_match (&cs2, &ALPHABET[(size_t)rand () % strlen (ALPHABET)]);
      printf ("[%'zu] %'zu matches found (amongst %'zu keywords) in a text of %'zu characters in %f s.\n", l + 1, nb_matches, nb_keywords, TEXT_LENGTH, (double)(clock () - myclock) / CLOCKS_PER_SEC);
      myclock = clock ();
    }
    acm_release (M2);
  }
}
