A small, documented, easy to use implementation of the Aho-Corasick algorithm.
------------------------------------------------------------------------------

Compared to the implemenation proposed by Aho and Corasick, this one adds three small enhancements:
- This implementation does not stores output keywords associated to states.
  It rather reconstructs matching keywords by traversing the branch of the tree backward (see ACM_get_match).
- This implementation permits to search for keywords even though all keywords have not been registered yet.
  To achieve this, failure states are reconstructed after every registration of a next keyword
  (see ACM_register_keyword which alternates calls to algorithms 2 and 3.)
- This implemtation keeps track of the rank of a registered keyword as returned by ACM_get_match().
  This can be used as a unique identifiant of a keyword for a given machine state.

Usage:
1/ Define the type ACM_SYMBOL with the type of symbols that constitute keywords. char or int should match most needs.
   Either define ACM_SYMBOL direcly in the user program or include a user defines aho_corasick_symbol.h file.
   E.g.:
   - #define ACM_SYMBOL char (simple choice if the algorithm is compiled as a private module).
   - #include "aho_corasick_symbol.h" (better choice if the algorithm is compiled as an object or library, and not as a private module).
2/ Insert "aho_corasick.h"
3/ Initialize a state machine: InitialState * M = 0;
4/ Add keywords (of type Keyword) to the state machine calling ACM_register_keyword() repeatedly.
   Notes: ACM_KEYWORD_SET can help initialize keywords with a single statement.
          ACM_nb_matches() returns the number of keywords already inserted in the state machine.
5/ Initialize an internal machine state to M: InternalState s = M;
Repeatedly:
6/   Scan some text where to search for keywords, injecting symbols of the text, one at a time by calling ACM_change_state() on s.
7/   After each insertion, call ACM_nb_matches() on the internal state s to check if the last inserted symbols match a keyword.
8/   If matches were found, retrieve them calling ACM_get_match() for each match.
9/ After usage, release the state machine calling ACM_release() on M.

Note if ACM_SYMBOL is a structure (does not apply for basic types such as int or char):
- An equality operator '==' with signature 'int eq (ACM_SYMBOL a, ACM_SYMBOL b)' should be defined
  in the user program and the name of the function should be defined in macro ACM_SYMBOL_EQ_OPERATOR.
  E.g.: #define ACM_SYMBOL_EQ_OPERATOR myequalityoperaor
- Keywords are passed by value to ACM_register_keyword().
  Therefore, if ACM_SYBOL contains pointers to allocated memory,
  - a copy operator '=' with signature 'ACM_SYMBOL copy (ACM_SYMBOL a)' should be defined in the user program and
    the name of the function should be defined in macro ACM_SYMBOL_COPY_OPERATOR.
  - a destructor operator with signature 'void dtor (ACM_SYMBOL a)' should be defined in the user program and
    the name of the function should be defined in macro ACM_SYMBOL_DTOR_OPERATOR.

Compilation:
The algorithm can be compiled either as an object (aho_corasick.o) or as a private module (if PRIVATE_MODULE is defined in the user program).
If PRIVATE_MODULE is set in the user program, then:
- the implementation of the algorithm will be compiled in the same compilation unit as the user program, therefore without requiring linkage.
- the warning "The Aho-Corasick algorithm is compiled as a private module." is emitted during compilation.

Files:
- aho_corasick.h defines the interface. It is fully documented.
- aho_corasick.c defines the implementation. It is fully documented.
  The implementation complies to the original algorithm as published by Aho and Corasick in 1975.
- aho_corasick_test.c gives an example of use (words.gz should be gunzip'ed befire use).
- words.gz is an input file used by the example.
- aho_corasick_symbol.h is an example of a declaration of the ACM_SYMBOL.

Have fun !
