A small, documented, easy to use implementation of the Aho-Corasick algorithm.
----------------------------------

This project offers an efficient implement of the Aho-Corasick algorithm which shows several benefits:

- It sticks acurately and step by step to the pseudo-code given in the original paper from Aho and Corasick
  (btw exquisitely clear and well written, see Aho, Alfred V.; Corasick, Margaret J. (June 1975).
  "[Efficient string matching: An aid to bibliographic search](https://pdfs.semanticscholar.org/3547/ac839d02f6efe3f6f76a8289738a22528442.pdf)".
  Communications of the ACM. 18 (6): 333–340.)
- It is generic in the sense that it works for any kind of alphabet and not only for char.
- The interface is minimal, complete and easy to use.
- The implementation has low memory footprint and is fast.

In more details, compared to the implementation proposed by Aho and Corasick, this one adds several enhancements:

1. First of all, it does not make any assumption on the size of the alphabet used.
Particularly, the alphanet is not limited to 256 signs.
The number of possible signs is only defined by the type of symbol the user decides to use.
For instance, if ACM_SYMBOL would be defined as 'long long int', then the number of possible signs would be 18446744073709551616.

      - For this to be possible, the assertion "for all a such that g(0, a) = fail do g(0, a) <- 0" in the Aho-Corasick paper,
        at the end of algorithm 2 can not be fulfilled because it would require to set g(0, a) for all the values of 'a'
        in the set of possible values of the alphabet,
        and thus allocate (if not exhaust) a lot of memory.
      - Therefore, g(0, a) is kept equal to fail (i.e. g(0, a) is kept undefined) for all 'a' not being the first sign of a registered keyword.
      - Nevertheless, for the state machine to work properly, it must behave as if g(0, a) would be equal to 0 whenever g(0, a) = fail
        after algorithm 2 has ended: thus, algorithms 1 and 3 (called after algorithm 2) must be adapted accordingly
        (modifications are tagged with [1], [2] and [3] in the code):

         - [1] g(0, a) = (resp. !=) 0 must be replaced by: g(0, a) = (resp. !=) fail
         - [2] g(state, a) = fail must be replaced by: g(state, a) = fail and state != 0
         - [3] s <- g(state, a) must be replaced by: if g(state, a) != fail then s <- g(state, a) else s <- 0

2. To reduce the memory footprint, it does not store output keywords associated to states.
   Instead, it reconstructs the matching keywords by traversing the branch of the tree backward.
   Attributes 'previous' and 'is_matching' are added the the state object ACState (see ACM_get_match).
3. It permits to search for keywords even though all keywords have not been registered yet.
   To achieve this, failure states are reconstructed after every registration of a new keyword
   (see ACM_register_keyword which alternates calls to algorithms 2 and 3.)
4. It keeps track of the rank of each registered keyword as returned by ACM_get_match().
   This rank can be used, together with the state machine, as a unique identifiant of a keyword.
5. If ACM_ASSOCIATED_VALUE is defined at compile time, it can associate user allocated and defined values to registered keywords,
   and retreive them together with the found keywords:
      - a third and fourth arguments are passed to ACM_register_keyword calls: a pointer to a previously allocated value,
        and a pointer to function for deallocation of the associated value. This function will be called when the state machine will be release
        by ACM_release.
      - a fourth argument is passed to ACM_get_match calls: the address of a pointer to an associated value.
        The pointer to associated value is modified by ACM_get_match to the address of the value associated to the keyword.
6. It extends the state machine in order to be used as well as an indexed dictionnary of keywords:
      - ACM_is_registered_keyword() check for the existence of a keyword in the state machine.
      - ACM_unregister_keyword() removes a keyword from the state machine.
      - ACM_foreach_keyword() applies a user defined operator to each keyword of the state machine.
7. It is short: aho_corasick.c is about 450 effective lines of code.
8. Last but not least, it is very fast. On my slow HD and slow CPU old computer, it takes 1.5 seconds to register
   370,099 keywords with a total of 3,864,776 characters, and to find those keywords in a text of 376,617 characters.

# Implementations

Two flavours of implementation are proposed.

- The first "standard" implementation allows one instanciation of the Aho-Corasick machine for the type defined by ACM_SYMBOL.
- The second "template" instanciaton allows to instanciate Aho-Corasick machine for several types. **It is also the recommended instanciation.**

## Standard implementation

### Usage

First, initialize the finite state machine with a set of keywords to be searched for:

In global scope:

1. Define the type ACM_SYMBOL with the type of symbols that constitute keywords. char or int should match most needs.
   Either define ACM_SYMBOL direcly in the user program or include a user defines aho_corasick_symbol.h file.
   E.g.:
      - '\#define ACM_SYMBOL char' (simple choice if the algorithm is compiled as a private module).
      - '\#include "aho_corasick_symbol.h"' (better choice if the algorithm is compiled as an object or library, and not as a private module).
2. Insert "aho_corasick.h"

In local scope (function or main entry point), preprocess keywords (once):

3. Initialize a state machine: ACMachine * M = 0;
4. Add keywords (of type Keyword) to the state machine calling ACM_register_keyword(), one at a time, repeatedly.
      - The rank of insertion of a keyword is registered together with the keyword.
      - The macro helper ACM_KEYWORD_SET can be used to initialize keywords with a single statement.
      - The macro helper ACM_REGISTER_KEYWORD can be conveniently used if the result (success or failure) of the registration is not needed.
      - ACM_nb_keywords() returns the number of keywords already inserted in the state machine.
      - If a keywords was already registered in the machine, its rank (and possibly associated value) is left unchanged.

Then, parse any sequence of any number of texts, searching for previously registered keywords:

5. (Optionally) Initialize a match holder with ACM_MATCH_INIT before the first use by ACM_get_match (if necessary).
6. Inject symbols of the text, one at a time by calling ACM_nb_matches(), and,
   after each insertion of a symbol, check the returned value to know if the last inserted symbols match at least one keyword.
      - If a new text has to be processed by the state machine, reset it to its initial state (ACM_reset) so that the next symbol will
        be matched against the first letter of each keyword.
7. (Optionally) If matches were found, retrieve them calling ACM_get_match() for each match (if necessary).
      - ACM_MATCH_LENGTH and ACM_MATCH_SYMBOLS can be used to get the length and the content of a retreieved match.
8. (Optionally) After the last call to ACM_get_match(), release to match holder by calling ACM_MATCH_RELEASE (if necessary).

Steps 5, 7 and 8 are optional.

Finally, when all texts have been parsed:

9. After usage, release the state machine calling ACM_release() on M.

Here is a simple example:
```c
#include <string.h>
#define ACM_SYMBOL char
#define PRIVATE_MODULE
#include "aho_corasick.h"

int
main (void)
{
  ACMachine *M = 0;

  char *keywords[] = { "buckle", "shoe", "knock", "door", "pick", "sticks", "ten" };
  for (size_t i = 0; i < sizeof (keywords) / sizeof (*keywords); i++)
  {
    Keyword kw;
    ACM_KEYWORD_SET (kw, keywords[i], strlen (keywords[i]));
    ACM_REGISTER_KEYWORD (M, kw);
  }

  char BuckleMyShoe[] =
    "One, two buckle my shoe\nThree, four knock on the door\nFive, six pick up sticks\nNine, ten a big fat hen...\n";

  MatchHolder m;
  ACM_MATCH_INIT (m);
  for (size_t i = 0; i < strlen (BuckleMyShoe); i++)
  {
    size_t nb = ACM_nb_matches (M, BuckleMyShoe[i]);
    for (size_t j = 0; j < nb; j++)
    {
      ACM_get_match (M, j, &m);
      for (size_t k = 0; k < ACM_MATCH_LENGTH (m); k++)
        printf ("%c", ACM_MATCH_SYMBOLS (m)[k]);
      printf ("\n");
    }
  }
  ACM_MATCH_RELEASE (m);
  ACM_release (M);
}
```

You can look at
   - aho_corasick_test.c for a fully documented example.
   - aho_corasick.h for a detailed documentation of the interface

### Note on ACM_SYMBOL

ACM_SYMBOL is the type of letters of the alphabet that constitutes the keywords.

ACM_SYMBOL can be any basic type (char, int, unisigned long long int for instance), or a user defined structure.

If ACM_SYMBOL is a basic type, the default equality operator is '=='.
This operator can be overwritten by a user defined function which takes two arguments of type ACM_SYMBOL and returns an int:

  - the first argument is a symbol taken from a keyword;
  - the second argument is a symbol taken from the text into which keywords are searched for.
  - the function should return 1 if symbols are considered equal, 0 otherwise.

The equality operator '==' with signature 'int eq (ACM_SYMBOL a, ACM_SYMBOL b)' should be defined
in the user program and the name of the function should be defined in macro ACM_SYMBOL_EQ_OPERATOR.

E.g. the following code will make string matching case insensitive.

    static int nocaseeq (char k, char t)
    { return tolower (k) == tolower (t); }
    #define ACM_SYMBOL_EQ_OPERATOR nocaseeq

## Note if ACM_SYMBOL is a structure (does not apply for basic types such as int or char)

If ACM_SYMBOL is a structure:

  - Defining a user defined equality operator ACM_SYMBOL_EQ_OPERATOR is compulsory.
  - If ACM_SYBOL contains pointers to allocated memory,
      - an assignment operator '=' with signature 'ACM_SYMBOL *function* (ACM_SYMBOL a)' should be defined in the user program and
        the name of the function should be defined in macro ACM_SYMBOL_COPY_OPERATOR.
      - a destructor operator with signature 'void *function* (ACM_SYMBOL a)' should be defined in the user program and
        the name of the function should be defined in macro ACM_SYMBOL_DTOR_OPERATOR.

### Compilation

The algorithm can be compiled either as an object (aho_corasick.o) or as a private module (if PRIVATE_MODULE is defined in the user program).

If PRIVATE_MODULE is set in the user program, then:

- the implementation of the algorithm will be compiled in the same compilation unit as the user program, therefore without requiring linkage.
  There is no need to compile aho_corasick.c separately, the source will be include in the user program including aho_corasick.h.
- the warning "The Aho-Corasick algorithm is compiled as a private module." is emitted during compilation.

### Files

Source code:

- aho_corasick.h defines the interface. It is fully documented.
- aho_corasick.c defines the implementation. It is fully documented.
  The implementation complies to the original algorithm as published by Aho and Corasick in 1975.

Examples:

- aho_corasick_test.c gives a complete and commented example.
- words and mrs_dalloway.txt are input files used by the example.
- aho_corasick_symbol.h is an example of a declaration of the ACM_SYMBOL.

## template instanciation

This implementation provides a syntax similar to the C++ templates.
It allows to instanciate the Aho-Corasick machine at compile-time for a type specified in the user program.

### Usage

The usage is very similar to the "Standard" version:

1. Insert "aho_corasick_template_impl.h"
2. Declare the types for which the Aho-Corasick machines have to be instanciated.

```c
    ACM_DECLARE (char)
    ACM_DEFINE (char)
```

In local scope (function or main entry point), preprocess keywords (once):

3. **Optionally**, user defined operators can be specified for type *T*.

     - An optional equality operator can be user defined for a type *T* with SET_EQ_OPERATOR(*T*, equality)
     - An optional constructor can be user defined for a type *T* with SET_COPY_CONSTRUCTOR(*T*, constructor)
     - An optional destructor operator can be user defined for a type *T* with SET_DESTRUCTOR(*T*, destructor)
```c
    SET_EQ_OPERATOR (*T*, equality);
    SET_COPY_CONSTRUCTOR (*T*, constructor);
    SET_DESTRUCTOR (*T*, destructor);
```

4. Initialize a state machine of type ACMachine (*T*) using ACM_create (*T*):
      - an optional second argument of type EQ_OPERATOR_TYPE(*T*) can specify a user defined equality operator for type *T*.
      - an optional third argument of type COPY_OPERATOR_TYPE(*T*) can specify a user defined constructor operator for type *T*.
      - an optional fourth argument of type DESTRUCTOR_OPERATOR_TYPE(*T*) can specify a user defined destuctor operator for type *T*.
      - Those operators supersedes those defined by SET_EQ_OPERATOR, SET_COPY_CONSTRUCTOR, SET_DESTRUCTOR for a specific instance of Aho-Corasick machine.
```c
    ACMachine (char) *M = ACM_create (char, [equality], [constructor], [destructor]);
```

5. Add keywords (of type Keyword (*T*)) to the state machine calling ACM_register_keyword(), one at a time, repeatedly.
      - The rank of insertion of a keyword is registered together with the keyword.
      - The macro helper ACM_KEYWORD_SET can be used to initialize keywords with a single statement.
      - The macro helper ACM_REGISTER_KEYWORD can be conveniently used if the result (success or failure) of the registration is not needed.
      - ACM_nb_keywords() returns the number of keywords already inserted in the state machine.
      - If a keywords was already registered in the machine, its rank (and possibly associated value) is left unchanged.

Then, parse any sequence of any number of texts, searching for previously registered keywords:

6. (Optionally) Initialize a match holder (of type MatchHolder (*T*)) with ACM_MATCH_INIT before the first use by ACM_get_match (if necessary).
7. Inject symbols of the text, one at a time by calling ACM_nb_matches(), and,
   after each insertion of a symbol, check the returned value to know if the last inserted symbols match at least one keyword.
      - If a new text has to be processed by the state machine, reset it to its initial state (ACM_reset) so that the next symbol will
        be matched against the first letter of each keyword.
8. (Optionally) If matches were found, retrieve them calling ACM_get_match() for each match (if necessary).
      - ACM_MATCH_LENGTH and ACM_MATCH_SYMBOLS can be used to get the length and the content of a retreieved match.
9. (Optionally) After the last call to ACM_get_match(), release to match holder by calling ACM_MATCH_RELEASE (if necessary).

Steps 6, 8 and 9 are optional.

Finally, when all texts have been parsed:

10. After usage, release the state machine calling ACM_release() on M.
Here is a simple example:
```c
#include <string.h>
#include "../aho_corasick_template_impl.h"

ACM_DECLARE (char)
ACM_DEFINE (char)

int
main (void)
{
  ACMachine (char) *M = ACM_create (char);

  char *keywords[] = { "buckle", "shoe", "knock", "door", "pick", "sticks", "ten" };
  for (size_t i = 0; i < sizeof (keywords) / sizeof (*keywords); i++)
  {
    Keyword (char) kw;
    ACM_KEYWORD_SET (kw, keywords[i], strlen (keywords[i]));
    ACM_REGISTER_KEYWORD (M, kw);
  }

  char BuckleMyShoe[] =
    "One, two buckle my shoe\nThree, four knock on the door\nFive, six pick up sticks\nNine, ten a big fat hen...\n";

  MatchHolder (char) m;
  ACM_MATCH_INIT (m);
  for (size_t i = 0; i < strlen (BuckleMyShoe); i++)
  {
    size_t nb = ACM_nb_matches (M, BuckleMyShoe[i]);
    for (size_t j = 0; j < nb; j++)
    {
      ACM_get_match (M, j, &m);
      for (size_t k = 0; k < ACM_MATCH_LENGTH (m); k++)
        printf ("%c", ACM_MATCH_SYMBOLS (m)[k]);
      printf ("\n");
    }
  }
  ACM_MATCH_RELEASE (m);
  ACM_release (M);
}
```

### Files

Source code:

- aho_corasick_template.h defines the interface.
- aho_corasick_template_impl.h defines the implementation.

Examples:

- aho_corasick_template_test.c gives a complete and commented example.
- words and mrs_dalloway.txt are input files used by the example.

Hopes this helps.
Have fun !
