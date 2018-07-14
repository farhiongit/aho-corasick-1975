A small, documented, easy to use implementation of the Aho-Corasick algorithm.
----------------------------------

![alt text](dictionary.jpeg "A fully indexed dictionary")

An Aho-Corasick Machine allows to implement a fully indexed dixtionnary of words.
A word or pattern is to be considered here in its general meaning, that is a sequence of signs or symbols of a given user defined alphabet (set of symbols).

The dictionary is used in two steps:

1. Register words in the dictionary, and optionnally definitions associated to words.
2. Read a text and compare it to words in the dictionary.

The dictionary can also be traversed, and an operator can be applied to each word or associated value.

# Introduction

This project offers an efficient implement of the Aho-Corasick algorithm which shows several benefits:

- It faithfully and acurately sticks, step by step, to the pseudo-code given in the original paper from Aho and Corasick
  (btw exquisitely clear and well written, see Aho, Alfred V.; Corasick, Margaret J. (June 1975).
  "[Efficient string matching: An aid to bibliographic search](https://pdfs.semanticscholar.org/3547/ac839d02f6efe3f6f76a8289738a22528442.pdf)".
  Communications of the ACM. 18 (6): 333–340.)
- It is generic in the sense that it works for any kind of alphabet (of any number of signs and not limited to 256) and not only for char.
- The interface is minimal, complete and easy to use.
- The implementation has low memory footprint and is fast.

In more details, compared to the implementation proposed by Aho and Corasick, this one adds several enhancements:

1. First of all, it does not make any assumption on the size of the alphabet used.
Particularly, unlike most implementations, the alphabet is not limited to 256 signs.
The number of possible signs is only defined by the type of symbol the user decides to use.
For instance, if the type of signs is defined as `long long int` rather than the usual `char`,
then the number of possible signs would be 18446744073709551616 !

      - For this to be possible, the assertion "for all a such that g(0, a) = `fail` do g(0, a) <- 0" in the Aho-Corasick paper,
        at the end of algorithm 2 can not be fulfilled because it would require to set g(0, a) for all the values of 'a'
        in the set of possible values of the alphabet,
        and thus would require to allocate (if not exhaust) a lot of memory.
      - Therefore, rather than being assigned to 0, g(0, a) is kept equal to `fail` (i.e. g(0, a) is kept undefined) for all 'a'
        not being the first sign of a registered keyword.
      - Nevertheless, for the state machine to work properly, it must behave as if g(0, a) would be equal to 0 whenever g(0, a) = fail
        after algorithm 2 has ended: thus, algorithms 1 and 3 (called after algorithm 2) must be adapted accordingly
        (modifications are tagged with [1], [2] and [3] in the code):

         - [1] g(0, a) = (resp. !=) 0 must be replaced by: g(0, a) = (resp. !=) fail
         - [2] g(state, a) = fail must be replaced by: g(state, a) = fail and state != 0
         - [3] s <- g(state, a) must be replaced by: if g(state, a) != fail then s <- g(state, a) else s <- 0

2. To reduce the memory footprint, it does not store output keywords associated to states.
   Instead, it reconstructs the matching keywords by traversing the branch of the tree backward.
   (Attributes `previous` and `is_matching` are added the the state object ACState, see code of `ACM_get_match`).
3. It permits to search for keywords even though all keywords have not been registered yet.
   To achieve this, failure states are reconstructed after every registration of a new keyword
   (see `ACM_register_keyword` which alternates calls to algorithms 2 and 3.)
4. It keeps track of the rank of each registered keyword as returned by ACM_get_match().
   This rank can be used, for a given state machine, as a unique identifier of a keyword.
5. It can associate user allocated and defined values to registered keywords,
   and retrieve them together with the found keywords:
      - a third and fourth arguments are passed to `ACM_register_keyword` calls: a pointer to a previously allocated value,
        and a pointer to function for deallocation of the associated value. This function will be called when the state machine will be release
        by `ACM_release`.
      - a fourth argument is passed to `ACM_get_match` calls: the address of a pointer to an associated value.
        The pointer to associated value is modified by `ACM_get_match` to the address of the value associated to the keyword.
6. It extends the state machine in order to be used as well as an indexed dictionary of keywords:
      - `ACM_is_registered_keyword()` check for the existence of a keyword in the state machine.
      - `ACM_unregister_keyword()` removes a keyword from the state machine.
      - `ACM_foreach_keyword()` applies a user defined operator to each keyword of the state machine.
7. Search for matching patterns is thread safe: several texts can be parsed concurrently (by several threads).
8. It is short: aho_corasick.c is about 700 effective lines of code.
9. Last but not least, it is very fast. On my slow HD and slow CPU old computer, it takes 0.92 seconds to register 370,099 keywords
   with a total of 3,864,776 characters, and 0.12 seconds to find (and count occurencies of) those keywords in a text of 376,617 characters.

# Implementation

This implementation provides a syntax similar to the C++ templates.
It makes use of a nice idea of Randy Gaul for [Generic Programming in C](http://www.randygaul.net/2012/08/10/generic-programming-in-c/).

It allows to instanciate the Aho-Corasick machine at compile-time for one or several type specified in the user program
(to be compared to the standard implementation which instanciate the machine for a unique type defined in ACM_SYMBOL.)

Except for ACM_register_keyword() and ACM_unregister_keyword(), all functions are thread-safe.
Therefore, a given shared Aho-Corasick machine can be used by multiple threads to scan different texts for matching keywords.

## Usage

In global scope:

1. Insert "aho_corasick_template_impl.h" in global scope.
2. Declare, in global scope, the types for which the Aho-Corasick machines have to be instanciated.

```c
    ACM_DECLARE (char);
    ACM_DEFINE (char);
```

In local scope (function or main entry point), preprocess keywords:

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
      - An optional second argument of type EQ_OPERATOR_TYPE(*T*) can specify a user defined equality operator for type *T*.
      - An optional third argument of type COPY_OPERATOR_TYPE(*T*) can specify a user defined constructor operator for type *T*.
      - An optional fourth argument of type DESTRUCTOR_OPERATOR_TYPE(*T*) can specify a user defined destuctor operator for type *T*.
      - Those operators supersedes those defined by SET_EQ_OPERATOR, SET_COPY_CONSTRUCTOR, SET_DESTRUCTOR for a specific
        instance of Aho-Corasick machine.
```c
    ACMachine (char) *M = ACM_create (char, [equality], [constructor], [destructor]);
```

5. Add keywords (of type `Keyword (*T*)`) to the state machine calling `ACM_register_keyword()`, one at a time, repeatedly.
      - An optional third argument, if not 0, can point to a value to be associated to the registerd keyword.
      - An optional fourth argument, if not 0, provides a pointer to a destructor (`void (*dtor) (void *)`)
        to be used to destroy the associated value.
      - The macro helper `ACM_KEYWORD_SET (keyword,symbols,length)` can be used to initialize keywords with a single statement.
      - The rank of insertion of a keyword is registered together with the keyword.
      - If a keywords was already registered in the machine, its rank (and possibly associated value) is left unchanged.
      - `ACM_nb_keywords (machine)` returns the number of keywords already inserted in the state machine.
```c
    int has_been_registered = ACM_register_keyword (machine, keyword, [value], [destructor]);
```

Then, parse any sequence of any number of texts, searching for previously registered keywords:

6. (Optionally) Initialize a match holder (of type `MatchHolder` (*T*)) with `ACM_MATCH_INIT (match)`
   before the first use by ACM_get_match (if necessary).
7. Initialize a state (of type `const ACState` (*T*)) with `ACM_reset (machine)`
8. Inject symbols of the text, one at a time by calling `ACM_match (state, symbol)`.
9. After each insertion of a symbol, check the returned value to know if the last inserted symbols match at least one keyword.
      - If a new text has to be processed by the state machine, reset it to its initial state (`ACM_reset`) so that the next symbol will
        be matched against the first letter of each keyword.
10. (Optionally) If matches were found, retrieve them calling `ACM_get_match ()` for each match (if necessary).
      - `ACM_MATCH_LENGTH (match)` and `ACM_MATCH_SYMBOLS (match)` can be used to get the length and the content of a retreieved match.
      - An optional third argument, a pointer to a `MatchHolder` (*T*), if not 0, will point to the matching keyword on return.
      - An optional fourth argument, a pointer to a `(void *)` pointer, if not 0, will point to the pointer to the value associated
        with the matching keyword.
```c
    size_t rank = ACM_get_match (machine, index, [match], [value]);
```

11. (Optionally) After the last call to `ACM_get_match ()`, release to match holder by calling `ACM_MATCH_RELEASE (match)` (if necessary).

Steps 6, 10 and 11 are optional.

Finally, when all texts have been parsed:

12. After usage, release the state machine calling ACM_release() on M.

Extra features are available to manage keywords:

- `ACM_is_registered_keyword (machine, keyword, [value])` can check if a keyword is already registered, and retreives its associated value
  if the third argument (of type `void **`) is provided and not equal to 0.
- `ACM_unregister_keyword (machine, keyword)` allows to unregister a keyword (if previously registered).
- `ACM_foreach_keyword (machine, function)` applies a function (`void (*function) (Keyword (T), void *)`) on each
  registerd keyword. The `function` is called for each keyword and should keep it unchanged.
  The first argument of this function is the keyword, the second is the pointer to the value associated to the keyword.
- `ACM_nb_keywords (machine)` yields the number of registered keywords.

Here is a simple example:

```c
#include <string.h>
#include "aho_corasick_template_impl.h"

ACM_DECLARE (char);                            /* template */
ACM_DEFINE (char);                             /* template */

int
main (void)
{
  ACMachine (char) *M = ACM_create (char);    /* template */

  char *keywords[] = { "buckle", "shoe", "knock", "door", "pick", "sticks", "ten" };
  for (size_t i = 0; i < sizeof (keywords) / sizeof (*keywords); i++)
  {
    Keyword (char) kw;                        /* template */
    ACM_KEYWORD_SET (kw, keywords[i], strlen (keywords[i]));
    ACM_register_keyword (M, kw);
  }

  char BuckleMyShoe[] =
    "One, two buckle my shoe\nThree, four knock on the door\nFive, six pick up sticks\nNine, ten a big fat hen...\n";

  const ACState (char) * state = ACM_reset (M);
  MatchHolder (char) m;                       /* template */
  ACM_MATCH_INIT (m);
  for (size_t i = 0; i < strlen (BuckleMyShoe); i++)
  {
    size_t nb = ACM_match (state, BuckleMyShoe[i]);
    for (size_t j = 0; j < nb; j++)
    {
      ACM_get_match (state, j, &m);
      for (size_t k = 0; k < ACM_MATCH_LENGTH (m); k++)
        printf ("%c", ACM_MATCH_SYMBOLS (m)[k]);
      printf ("\n");
    }
  }
  ACM_MATCH_RELEASE (m);
  ACM_release (M);
}
```

## Files

Source code:

- aho_corasick_template.h defines and fully documents the interface.
- aho_corasick_template_impl.h defines the implementation.

Examples:

- aho_corasick_template_test.c gives a complete and commented example.
- words and mrs_dalloway.txt are input files used by the example.

## API

### User defined type helpers

#### Instanciator

> `ACM_DECLARE (`*T*`)`
>
> `ACM_DEFINE (`*T*`)`
>
> *T* can be any standard basic type or any user defined type, such as a structure.

These macros let declare and define tools to use a Aho-Corasick machine for a user defined type *T*.

*Example*:

     ACM_DECLARE (int);
     ACM_DEFINE (int);

#### Operators

If a user defined type *T* uses internal allocated resources, operators can be optionnaly defined.

> `SET_DESTRUCTOR (`*T*`, destructor)`
>
> `SET_COPY_CONSTRUCTOR (`*T*`, copy_constructor)`
>
> `SET_EQ_OPERATOR (`*T*`, equal_operator)`

`SET_DESTRUCTOR` optionally declares a destructor for type *T*, of type: `void (*destructor) (const `*T*`)` (a.k.a `DESTRUCTOR_TYPE(`*T*`)`).

`SET_COPY_CONSTRUCTOR` optionally declares a copy constructor for type *T*, of type: *T*` (*copy_constructor) (const `*T*`)` (a.k.a `COPY_CONSTRUCTOR_TYPE(`*T*`)`).

`SET_EQ_OPERATOR` optionally declares equality operator for type *T*, of type: `int (*equal_operator) (const `*T*`, const `*T*`)` (a.k.a `EQ_OPERATOR_TYPE(`*T*`)`).
The default equality operator `memcmp` is used otherwise.
`equal_operator` must return `0` if its two arguments are different, non `0` otherwise.

### Dictionnary declarator

> `ACM_DECL (var, `*T*`, [equal_operator], [copy_constructor, destructor])`
>
> Available with compilers `gcc` and `clang`.

This macro let declare a local variable `var` of type `ACMachine (`*T*`)` where
`ACMachine (`*T*`)` is the type of the Aho-Corasick finite state machine for type T

`var` will be properly freed when going out of scope.

Specific operators `equal_operator`, `copy_constructor`, `destructor` can optionnaly be declared for type *T* and dictionary `var`.
They supersede the operators applied to type *T*.

### Dictionnary dynamic allocation

#### Creation

> `ACMachine (`*T*`) *ACM_create (`*T*`, [equality_operator], [copy constructor, destructor])`
>
> Parameters:
>
> - [in] *T* type of symbols composing keywords and text to be parsed.
>
> - [in, optional] equality_operator Equality operator of type EQ_OPERATOR_TYPE(T).
>
> - [in, optional] copy constructor Copy constructor of type COPY_CONSTRUCTOR_TYPE(T).
>
> - [in, optional] destructor Destructor of type DESTRUCTOR_TYPE(T).
>
> Returns: A pointer to a Aho-Corasick machine for type *T*.

This function `ACM_create` creates a dictionary (implemented with a Aho-Corasick finite state machine) for type *T*.

Specific operators `equal_operator`, `copy_constructor`, `destructor` can optionnaly be declared for type *T* and the allocated dictionary.
They supersede the operators applied to type *T*.

*Example*: `ACMachine (char) * M = ACM_create (char);`

#### Destruction

> `void ACM_release (const ACMachine (`*T*`) *machine)`
>
> [in] machine A pointer to a Aho-Corasick machine to be realeased.

`ACM_release` must be called to release the ressources of a dictionary created with `ACM_create`.

*Example*: `ACM_release (M);`

### Words

Words are any sequences of symbols of type *T*.

They can be instanciated with

> `void ACM_KEYWORD_SET (Keyword(T) kw, T* array, size_t length)`

Parameters:
- [in] kw Keyword of symbols of type T.
- [in] array Array of symbols
- [in] length Length of the array

`ACM_KEYWORD_SET` initializes a keyword from an array of symbols

The `array` is **NOT** duplicated by `ACM_KEYWORD_SET` and should be allocated and desallocated by the calling user program.

Example: `ACM_KEYWORD_SET (kw, "Duck", 4);`

### Dictionary initializarion

#### Word registration

`ACM_register_keyword` add a word in the dictionary, together with an optional pointer to an associated value.

> `int ACM_register_keyword (ACMachine (`*T*`) *machine, Keyword (`*T*`) kw, [void * value_ptr], [void (*destructor) (void *)])`

Parameters:
- [in] machine A pointer to a Aho-Corasick machine.
- [in] kw Keyword of symbols of type T to be registered.
- [in, optional] value_ptr Pointer to a previously allocated value to associate with keyword kw.
- [in, optional] destructor A destructor to be used to free the value pointed by value_ptr.
  - The default destructor is the standard library function `free.
  - Use `0` if the allocated value need not be managed by the finite state machine (in case of automatic or static values).

`ACM_register_keyword` returns 1 if the keyword was successfully registered, 0 otherwise (if the keywpord is already registered in the machine).

Notes:
- Keyword `kw` is passed by value and can be released after its registration.
  The equality operator, either associated to the machine, or associated to the type T, is used if declared.
- The keyword is registered together with its rank.
  The rank is a unique 0-based identifier of the registered keyword.
  It can later be retrieved by `ACM_get_match`.

*Example*:

     ACM_register_keyword (M, kw);
     ACM_register_keyword (M, kw, calloc (1, sizeof (int)), free);

#### Word deletion

`ACM_unregister_keyword` removes a word from the dictionary.

> `int ACM_unregister_keyword (ACMachine(`*T*`) *machine, Keyword(T) kw)`

Parameters:
- [in] machine A pointer to a Aho-Corasick machine.
- [in] kw Keyword of symbols of type T to be registered.

`ACM_unregister_keyword` returns 1 if the keyword was successfully unregistered, 0 otherwise (if the keyword is not registered in the machine).

The equality operator, either associated to the machine, or associated to the type T, is used if declared.

`ACM_is_registered_keyword` checks whether a word is already registered in the dictionary and optionally retrieves the associated value.

> `int ACM_is_registered_keyword (const ACMachine(`*T*`) * machine, Keyword(`*T*`) kw, [void **value_ptr])`

Parameters:
- [in] machine A pointer to a Aho-Corasick machine.
- [in] kw Keyword of symbols of type T to be checked.
- [out, optional] value_ptr *value_ptr is set to the pointer of the value associated to the keyword after the call.

#### Word checking

`ACM_is_registered_keyword` returns 1 if the keyword is registered in the machine, 0 otherwise.

The equality operator, either associated to the machine, or associated to the type T, is used if declared.

> `size_t ACM_nb_keywords (const ACMachine (`*T*`) *machine)

returns the number of keywords registered in the dictionary.

Parameters:
- [in] machine A pointer to a Aho-Corasick machine.

#### Dictionary iterator

> `void ACM_foreach_keyword (const ACMachine(`*T*`) * machine, void (*operator) (MatchHolder(`*T*`) kw, void *value))`

`ACM_foreach_keyword` applies an operator to every registered keyword (by `ACM_register_keyword`) in the machine.

Parameters:
- [in] machine A pointer to a Aho-Corasick machine.
- [in] operator Function of type void (*operator) (Keyword (T), void *)

The operator is called for each registered keyword and pointer to associated value successively.
The order in which the keywords are processed in unspecified.

*Example*:

     static void print_match (MatchHolder (wchar_t) match, void *value) { /* user code here */ }
     ACM_foreach_keyword (M, print_match);

### Word matching

> `const ACState (`*T*`) * ACM_reset (ACMachine(`*T*`) * machine)`

gets a valid state, ignoring all the symbols previously matched by ACM_match.

Parameters:
- [in] machine A pointer to a Aho-Corasick machine.
- [in] state A pointer to a valid Aho-Corasick machine state.

Calls to `ACM_reset` on the same machine can be used to parse several texts concurrently (e.g. by several threads).


> `size_t ACM_match (const ACState(`*T*`) *& state, `*T*` letter)`

`ACM_match` sends a symbol `letter` into the Aho-Corasick machine and counts the number of registered word in the dictionary matching with
the last symbols sent since the last call to `ACM_reset`.
This is the main function used to parse a text, one symbol after the other, and to search for pattern matching.

Parameters:
- [in, out] state A pointer to a valid Aho-Corasick machine state, initialized by `ACM_reset`.
  `state` is *passed by reference* (à la C++): it is modified by the function call.
- [in] letter A symbol.

`ACM_match` returns the number of registered keywords that match a sequence of last letters sent to the last calls to ACM_match.

The equality operator, either associated to the machine, or associated to the type T, is used if declared.

*Example*: `size_t nb = ACM_match(state, letter);`

> `size_t ACM_get_match (const ACState(T) * state, size_t index, [MatchHolder(T) * match], [void **value_ptr])`

gets the ith keyword matching with the last symbols.

Parameters:
- [in] state A pointer to a valid Aho-Corasick machine state.
- [in] index Index (ith) of the ith matching keyword.
  `index` must be lower than value returned by the last call to `ACM_match`.
- [out, optional] match *match is set to the ith matching keyword.
- [out, optional] value_ptr *value_ptr is set to the pointer of the value associated to the keyword after the call.

`ACM_get_match` returns the rank (unique id) of the ith matching keyword.

`*match` should have been declared as a variable of type `MatchHolder (`*T*`)` and initialized by ACM_MATCH_INIT before use, and released by ACM_MACTH_RELEASE after use.

*Example*:

     MatchHolder (int) match;
     ACM_MATCH_INIT (match);
     size_t rank = ACM_get_match (state, j, &match, 0);
     size_t len = ACM_MATCH_LENGTH (match);
     int* word = ACM_MATCH_SYMBOLS (match);
     ACM_MATCH_RELEASE (match);

# Performance test

This implementation is fast.

The following performance test (also look at file aho_corasick_template_test.c) can be applied on a sample data of 184 MB available
[here](http://storage.googleapis.com/books/ngrams/books/googlebooks-eng-all-1gram-20120701-0.gz).

- Get the sample data from the internet:
```
wget http://storage.googleapis.com/books/ngrams/books/googlebooks-eng-all-1gram-20120701-0.gz
gzip -d googlebooks-eng-all-1gram-20120701-0.gz
```
- Type in the source of the performance test, say in ahoperftest.c:

```c
#include <stdio.h>
#include "aho_corasick_template_impl.h"

ACM_DECLARE (char)
ACM_DEFINE (char)
int main (void)
{
  ACMachine (char) * M = ACM_create (char);
  Keyword (char) kw;

  ACM_KEYWORD_SET (kw, "1984", 4);
  ACM_register_keyword (M, kw);

  ACM_KEYWORD_SET (kw, "1985", 4);
  ACM_register_keyword (M, kw);

  size_t nb_matches = 0;
  FILE *f = fopen ("googlebooks-eng-all-1gram-20120701-0", "r");
  char line[4096];
  const ACState (char) *state = ACM_reset (M);
  while (fgets (line, sizeof (line) / sizeof (*line), f))
    for (char *c = line; *c; c++)
      nb_matches += ACM_match (state, *c);
  fclose (f);
  printf ("%lu\n", nb_matches);

  ACM_release (M);
}
```
- Build and run:

```
clang -O3 -pthread ahoperftest.c -o ahoperftest
time ./ahoperftest
280503

real  0m3.710s
user  0m3.620s
sys 0m0.088s
```

It's a little bit slower than usual implementations (such as  https://github.com/morenice/ahocorasick)
but with a clean, generic and template interface.
Genericity (alphabet is user defined and not restricted to 256 characters as most implementations do) comes
with a slight loss of performance.

Anyway, it can process 184 MB against matching keywords is few seconds.

Hopes this helps. Let me know !

**Have fun !**
