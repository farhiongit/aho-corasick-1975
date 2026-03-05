A small, documented, easy to use implementation of the Aho-Corasick algorithm.
----------------------------------

![alt text](dictionary.jpeg "A fully indexed dictionary")

An Aho-Corasick State Machine allows to implement a fully indexed dictionary of words.
A word or pattern is to be considered here in its general meaning, that is a sequence of signs or symbols of a given user defined alphabet (set of symbols).

Once the dictionary has been defined with a list of words, it can be used to search for those words in a text composed of a sequence of signs.

The dictionary is [used](#usage) in two steps:

1. Register words in the dictionary, and optionally values associated to words.
2. Read a text and compare it to words in the dictionary.

The algorithm proposed by Aho and Corasick in a paper of 1975 is efficient when the alphabet contains a limited number of signs and words are composed of several of those signs.

> [!IMPORTANT]
> If the dictionary is populated with words of only one sign from a large or unbound alphabet, a [traditional map](https://github.com/farhiongit/minimaps) should be preferred.

> [!TIP]
> This is a recommended generic implementation of the algorithm, that applies to any type of signs (not only `char`, it could also be a complex structure as long as an equality operator exists for it).
> There is a template version of the same algorithm [here](./template).

# License

This library is distributed under the terms of the GNU *Lesser* General Public License, version 3 (see [COPYING](COPYING) and [COPYING.LESSER](COPYING.LESSER).

Therefore, you should give prominent notice, with each copy of your files using the library (modified or not), that the library is used in it and that the library and its use are covered by this license, accompanied with a copy of it and the copyright notice.

# Introduction

This project offers an efficient [implementation](#implementation) of the Aho-Corasick algorithm which shows several enhancements:

- It faithfully and accurately sticks, step by step, to the pseudo-code given in the original paper from Aho and Corasick
  (btw exquisitely clear and well written, see Aho, Alfred V.; Corasick, Margaret J. (June 1975).
  "[Efficient string matching: An aid to bibliographic search](https://github.com/tpn/pdfs/blob/master/Efficient%20String%20Matching%20-%20An%20Aid%20to%20Bibliographic%20Search%20-%20Aho-Corasick%20(1975).pdf)".
  Communications of the ACM. 18 (6): 333–340.)
- The code is commented and annotated with excerpts from the original text of Aho and Corasick.
- The original algorithm is refactored so that it can used iteratively and not be constrained by a given type of container for words.
- It is generic in the sense that it works for any kind of alphabet (of any type) and not only for `char`.
- It works with alphabets of any number of signs (and not limited to 26 or 256).
- The Aho Corasick state machine is stable: new keywords can be registered (`acm_insert_letter_of_keyword`) while scanning is already in progress (`acm_match`).
- The interface is minimal, complete and easy to use.
- The implementation has low memory footprint and is fast.

Hope this helps. Let me know !

# Usage

The usage of the Aho-Corasick dictionary is straight forward.

You can look at an [example](./examples/aho_corasick_generic_test.c) while you read.

## Declare a dictionary

Create and returns a new state machine with `acm_create`.

```c
typedef int (*EQ_TYPE) (const void *letter_a, const void *letter_b, void *eq_arg);
typedef void (*DESTROY_TYPE) (void *letter); // Compatible with the signature of free.

ACMachine *acm_create (EQ_TYPE eq, void *eq_arg, DESTROY_TYPE dtor);
```

- The first argument is the equality operator for the signs that will be used by the dictionary.
- The second optional argument will be passed to the equality operator when used by the machine.

> [!NOTE]
> An equality operator `eq` must be provided. The optional argument `eq_arg` will be passed to each call to `eq`.
>
> If the signs fed to the machine are automatically or statically allocated (until the machine is releases with `acm_release`), then `0` must be passed as `dtor`.
>
> If the signs fed to the machine are not statically allocated until the machine is releases (with `acm_release`),
> - signs (and possibly its internals) must be dynamically allocated before each call to `acm_insert_letter_of_keyword`.
> - a destructor `dtor` must be provided. It must deallocate the sign (and possibly its internals) with as many calls to `free` as calls needed to `malloc` to allocate a sign (see `acm_insert_letter_of_keyword`).
> - signs will be automatically deallocated by the machine.
> - if signs are allocated by a single call to `malloc`, `free` is a suitable destructor.
>
> `acm_release` must be subsequently called when the machine is not needed anymore.

> [!TIP]
> A handy default equality operator `ACM_EQ_DEFAULT` can be used for a simple lexical comparison (based on `memcmp`).
> `&(size_t){ sizeof (` *T* `)` must then be passed as second argument, where *T* is the type of the signs.

## Create the dictionary with words

### Initialise

To fill the dictionary with words :

- Call first `acm_initiate` once for all.

```c
ACState *acm_initiate (ACMachine *machine);
```

### Add words in the dictionary

- Then, for each word compose of a sequence of sign,

  - Send each sign, one after the other, in sequence, with successive calls to `acm_insert_letter_of_keyword`.

```c
void acm_insert_letter_of_keyword (ACState **state, void *letter);
```

> [!NOTE]
> A state must have been initialised with `acm_initiate` before the first call to `acm_insert_letter_of_keyword`.
> A `letter` must be provided. It is a pointer to an allocated sign that must persist until the machine is release with (`acm_release`).
> If it was allocated dynamically, it will be automatically deallocated by the machine using the destructor previously passed to `acm_create`.

  - Call `acm_insert_end_of_keyword`. A definition associated to the word can be passed as second argument.

```c
void acm_insert_end_of_keyword (ACState **state, void *value, void (*dtor) (void *));
```

> [!NOTE]
> `acm_insert_letter_of_keyword` should have been previously called at least once before `acm_insert_end_of_keyword` is called.
> The `value` will be later retrieved by a subsequent call to `acm_get_match`.
> If the `value` fed to the machine is not statically allocated until the machine is releases (with `acm_release`),
> - `value` must be dynamically allocated.
> - a destructor `dtor` must be provided.
> - `value` will be automatically deallocated by the machine.
> - if `value` is allocated by a single call to `malloc`, `free` is a suitable destructor.
> The keyword is given a unique internal rank in the machine that will be later returned by calls to `acm_get_match`.

## Scan a text for words of the dictionary

### Initialise

To search for the words of the dictionary in a text composed of a sequence of signs :

- Call first `acm_initiate` once for each text to scan.

> [!NOTE]
> The very same previously used to feed the dictionary with words.

- Initialise a matcher with `acm_matcher_init`.

```c
void acm_matcher_init (MatchHolder *matcher);
```

> [!NOTE]
> A `MatchHolder` can be used to retrieve several matches (with `acm_get_match`) of different match searches (with `acm_match`).
> a `MatchHolder` must be release by a subsequent call to `acm_match_release` after use.

### Search for words and retrieve the found words

Then,

- make successive calls to `acm_match` for each sign of the text, in sequence, from beginning to end of the text.

```c
size_t acm_match (ACState **state, void *letter);
```

> [!NOTE]
> A state msut have been initialised with `acm_initiate` before the first call to `acm_match`.
> A `letter` must be provided. It is a pointer to a sign that must persist until to call to `acm_match` has returned.
> Returns the number of matches found.

  - If one or several matches are found by `acm_match` while reading the text, `acm_match` will return a non-zero value of the number of matches found.
  - Loop on these matches with a call to `acm_get_match` for each match.

```c
size_t acm_get_match (const ACState *state, size_t index, MatchHolder *matcher, void **value);
```

> [!NOTE]
> `state` must the one used by a previous call to `acm_match`.
> The index must be lower than the number of matches found by a previous call to `acm_match`.
> If `match` is not null,
> - it must have been initialised by a previous call to `acm_match_init` before use.
> - it will be filled with the found match (with a keyword defined by a previous call to `acm_insert_end_of_keyword`).
> If `value` is not null, `*value` will be set to the value passed to a previous call to `acm_insert_end_of_keyword`.
> Returns the unique internal rank of the found keyword, as given by `acm_insert_end_of_keyword`.

> [!TIP]
> [Adding words](#add-words-in-the-dictionary) to the dictionary and [searching](#search-for-words-and-retrieve-the-found-words) can be realised consecutively, alternatively or concurrently (by different threads).

- Release the resources used by the matcher with `acm_matcher_release`.

```c
void acm_matcher_release (MatchHolder *matcher);
```

> [!NOTE]
> The `matcher` must have been initialised by a previous call to `acm_match_init`.

## Destroy the dictionary

Release the resources uses by the state machine after use with `acm_match_release`.

```c
void acm_release (ACMachine *machine);
```

> [!NOTE]
> The `machine` must have been initialised by a previous call to `acm_create`.

## Extra features

> [!NOTE]
> The `machine` must have been initialised by a previous call to `acm_create`.

### Get the number of keywords defined in the dictionary
```c
size_t acm_nb_keywords (ACMachine *machine);
```

### Lopp on the defined keywords of the dictionary
```c
void acm_foreach_keyword (ACMachine *machine, void (*operator) (MatchHolder, void *value));
```

> [!NOTE]
> The user-declared `operator` is called successively for each keyword of the dictionary.

# Files

- [`aho_corasick.h`](./aho_corasick.h) defines and fully documents the interface (28 effective lines).
- [`aho_corasick.c`](./aho_corasick.c) defines the fully commented implementation (379 effective lines).

# API

|| Description                                                           | Function name               |
|-|----------------------------------------------------------------------|-----------------------------|
|**Dictionary instanciators**|
|| Allocates a dictionary dynamically                                    | `acm_create`                |
|| Deallocates a dictionary                                              | `acm_release`               |
|**Keyword management**|
|| Prepares a dictionary for keyword insertion                           | `acm_initiate`              |
|| Registers a keyword in a dictionary                                   | `acm_insert_letter_of_keyword` and `acm_insert_end_of_keyword` |
|| Gets the number of registered keywords in a dictionary                | `acm_nb_keywords`           |
|| Calls a callback function for each keyword registered in a dictionary | `acm_foreach_keyword`       |
|**Keyword matching**|
|| Prepares a dictionary for keyword matching                            | `acm_initiate`              |
|| Searches text for matching keywords registered in a dictionary        | `acm_match`                 |
|| Initialises a container for registered keywords from a dictionary     | `acm_matcher_init`          |
|| Retrieves one of the found matching keywords                          | `acm_get_match`             |
|| Releases a container for registered keywords from a dictionary        | `acm_matcher_release`       |

> [!NOTE]
> `acm_initiate` is used both for keyword insertion and searching.

# Implementation

Compared to the implementation proposed by Aho and Corasick, this one adds several original enhancements (not seen anywhere else):

1. First of all, it does not make any assumption on the size of the alphabet used.
Particularly, unlike most implementations, the alphabet is not limited to 256 signs of type `char`.
For instance, if the type of signs is defined as `long long int` rather than the usual `char`,
then the number of possible signs would be 18446744073709551616 !

For this to be possible, the three algorithms of the original paper have been ajusted:

- The assertion "for all a such that g(0, a) = `fail` do g(0, a) <- 0" in the Aho-Corasick paper,
  at the end of algorithm 2 can not be fulfilled because it would require to set g(0, a) for all the values of 'a'
  in the set of possible values of the alphabet,
  and thus would require to allocate (if not exhaust) a lot of memory.
- Therefore, rather than being assigned to 0, g(0, a) is kept equal to `fail` (i.e. g(0, a) is kept undefined) for all 'a'
  not being the first sign (g(0, a)) of a registered keyword.
- Nevertheless, for the state machine to work properly, it must behave as if g(0, a) would be equal to 0 whenever g(0, a) = fail
  after algorithm 2 has ended: thus, algorithms 1 and 3 (called after algorithm 2) must be adapted accordingly
  (modifications are tagged with [1], [2] and [3] in the code):

  - [1] g(0, a) = (resp. !=) 0 must be replaced by: g(0, a) = (resp. !=) fail
  - [2] g(state, a) = fail must be replaced by: g(state, a) = fail and state != 0
  - [3] s <- g(state, a) must be replaced by: if g(state, a) != fail then s <- g(state, a) else s <- 0

2. To reduce the memory footprint, the implementation does not store output keywords associated to states.
   Instead, it reconstructs the matching keywords by traversing the branch of the tree backward.
   (Attributes `previous` and `is_matching` are added the the state object ACState, see code of `acm_get_match`).
3. It permits to search for keywords even though all keywords have not been registered yet.
   In other words, a new keyword can be registered with iterative calls to `acm_insert_letter_of_keyword` alternatively or concurrently with calls to `acm_match`
   without disrupting the current match search.
   To achieve this, failure states are reconstructed after every registration of a new keyword
   (see `acm_insert_keyword` which alternates calls to algorithms 2 and 3.)
4. It keeps track of the rank of each registered keyword.
   This rank is afterwards returned by `acm_get_match` and can be used, for a given state machine, as a unique identifier of a keyword.
5. It can associate user allocated and defined values to registered keywords,
   and retrieve them together with the found keywords:
  - two arguments are passed to `acm_insert_end_of_keyword` calls: a pointer to a previously allocated value,
    and a pointer to function for deallocation of the associated value. This function will be called when the state machine will be release
    by `acm_release`.
  - a fourth argument is passed to `acm_get_match` calls: the address of a pointer to an associated value.
    The pointer to associated value is modified by `acm_get_match` to the address of the value associated to the keyword.
6. It extends the state machine in order to be used as well as an indexed dictionary of keywords:
  - `acm_foreach_keyword()` applies a user defined operator to each keyword of the state machine.
7. All functions are thread-safe. Therefore, a given shared Aho-Corasick machine can be used by multiple threads concurrently to insert keywords or to scan different texts for matching keywords.
8. It is short (about 500 effective lines of code.)
9. Last but not least, it is very fast. On my slow HD and slow CPU old computer, it takes 0.92 seconds to register 370,099 keywords
   with a total of 3,864,776 characters, and 0.12 seconds to find (and count occurencies of) those keywords in a text of 376,617 characters.

# Example

- [`examples/aho_corasick_generic_test.c`](./examples/aho_corasick_generic_test.c) gives a complete and commented example.
- The novel Mrs Dalloway of Virgina Woolf  [`mrs_dalloway.txt`](./examples/mrs_dalloway.txt) serves as an input file used by the example.

**Have fun !**
