
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

#ifndef __ACM_TEMPLATE__

#  define __ACM_TEMPLATE__

#  define __NARG__(...)  __NARG_I_(__VA_ARGS__,__RSEQ_N())
#  define __NARG_I_(...) __ARG_N(__VA_ARGS__)
#  define __ARG_N( \
      _1, _2, _3, _4, _5, _6, _7, _8, _9,_10, \
     _11,_12,_13,_14,_15,_16,_17,_18,_19,_20, \
     _21,_22,_23,_24,_25,_26,_27,_28,_29,_30, \
     _31,_32,_33,_34,_35,_36,_37,_38,_39,_40, \
     _41,_42,_43,_44,_45,_46,_47,_48,_49,_50, \
     _51,_52,_53,_54,_55,_56,_57,_58,_59,_60, \
     _61,_62,_63,N,...) N
#  define __RSEQ_N() \
     63,62,61,60,                   \
     59,58,57,56,55,54,53,52,51,50, \
     49,48,47,46,45,44,43,42,41,40, \
     39,38,37,36,35,34,33,32,31,30, \
     29,28,27,26,25,24,23,22,21,20, \
     19,18,17,16,15,14,13,12,11,10, \
     9,8,7,6,5,4,3,2,1,0

#  define _VFUNC_(name, n) name##n
#  define _VFUNC(name, n) _VFUNC_(name, n)
#  define VFUNC(func, ...) _VFUNC(func, __NARG__(__VA_ARGS__)) (__VA_ARGS__)

#  define ACM_MATCH_LENGTH(match)                   ((match).length)
#  define ACM_MATCH_SYMBOLS(match)                  ((match).letter)
#  define ACM_KEYWORD_SET(keyword,symbols,length)   do { ACM_MATCH_SYMBOLS (keyword) = (symbols); ACM_MATCH_LENGTH (keyword) = (length); } while (0)
#  define ACM_MATCH_INIT(match)                     ACM_KEYWORD_SET((match), 0, 0)
#  define ACM_MATCH_RELEASE(match)                  do { free (ACM_MATCH_SYMBOLS (match)); ACM_MATCH_INIT (match); } while (0)

// BEGIN DECLARE_ACM
#  define ACM_DECLARE(ACM_SYMBOL)                                      \
\
typedef ACM_SYMBOL (*COPY_##ACM_SYMBOL##_TYPE) (const ACM_SYMBOL);     \
typedef void (*DESTROY_##ACM_SYMBOL##_TYPE) (const ACM_SYMBOL);        \
typedef int (*EQ_##ACM_SYMBOL##_TYPE) (const ACM_SYMBOL, const ACM_SYMBOL);  \
\
typedef struct                                                         \
{                                                                      \
  ACM_SYMBOL *letter;                                                  \
  size_t length;                                                       \
} Keyword_##ACM_SYMBOL;                                                \
\
struct _ac_state_##ACM_SYMBOL;                                         \
struct _ac_machine_##ACM_SYMBOL;                                       \
typedef struct _ac_machine_##ACM_SYMBOL ACMachine_##ACM_SYMBOL;        \
\
struct _ac_vtable_##ACM_SYMBOL                                         \
{                                                                      \
  ACMachine_##ACM_SYMBOL *(*register_keyword) (ACMachine_##ACM_SYMBOL * machine, Keyword_##ACM_SYMBOL keyword,         \
                                               void *value, void (*dtor) (void *));                                    \
  int (*is_registered_keyword) (const ACMachine_##ACM_SYMBOL * machine, Keyword_##ACM_SYMBOL keyword, void **value);   \
  int (*unregister_keyword) (ACMachine_##ACM_SYMBOL * machine, Keyword_##ACM_SYMBOL keyword);                          \
  size_t (*nb_keywords) (const ACMachine_##ACM_SYMBOL * machine);                                                      \
  void (*foreach_keyword) (const ACMachine_##ACM_SYMBOL * machine, void (*operator) (Keyword_##ACM_SYMBOL, void *));   \
  void (*release) (ACMachine_##ACM_SYMBOL * machine);                                                                  \
\
  size_t (*nb_matches) (ACMachine_##ACM_SYMBOL * machine, ACM_SYMBOL letter);                                          \
  void (*reset) (ACMachine_##ACM_SYMBOL * machine);                                                                    \
  size_t (*get_match) (const ACMachine_##ACM_SYMBOL * machine, size_t index, Keyword_##ACM_SYMBOL * match,             \
                       void **value);                                                                                  \
};                                                                     \
\
struct _ac_machine_##ACM_SYMBOL                                        \
{                                                                      \
  struct _ac_state_##ACM_SYMBOL *state_0;                              \
  const struct _ac_state_##ACM_SYMBOL *current_state;                  \
  size_t rank;                                                         \
  size_t nb_sequence;                                                  \
  int reconstruct;                                                     \
  size_t size;                                                         \
  const struct _ac_vtable_##ACM_SYMBOL *vtable;                        \
  ACM_SYMBOL (*copy) (const ACM_SYMBOL);                               \
  void (*destroy) (const ACM_SYMBOL);                                  \
  int (*eq) (const ACM_SYMBOL, const ACM_SYMBOL);                      \
};                                                                     \
\
static ACMachine_##ACM_SYMBOL *ACM_create_##ACM_SYMBOL (EQ_##ACM_SYMBOL##_TYPE eq, \
                                                 COPY_##ACM_SYMBOL##_TYPE copier,  \
                                                 DESTROY_##ACM_SYMBOL##_TYPE dtor);
// END DECLARE_ACM

// BEGIN MACROS
#  define ACState(ACM_SYMBOL)                           struct _ac_state_##ACM_SYMBOL
#  define SET_DESTRUCTOR(ACM_SYMBOL, destructor)        do { DESTROY_##ACM_SYMBOL = (destructor) ; } while (0)
#  define SET_COPY_CONSTRUCTOR(ACM_SYMBOL, constructor) do { COPY_##ACM_SYMBOL = (constructor) ; } while (0)
#  define SET_EQ_OPERATOR(ACM_SYMBOL, operator)         do { EQ_##ACM_SYMBOL = (operator) ; } while (0)
#  define DESTRUCTOR_TYPE(ACM_SYMBOL)                   DESTROY_##ACM_SYMBOL##_TYPE
#  define COPY_CONSTRUCTOR_TYPE(ACM_SYMBOL)             COPY_##ACM_SYMBOL##_TYPE
#  define EQ_OPERATOR_TYPE(ACM_SYMBOL)                  EQ_##ACM_SYMBOL##_TYPE
#  define Keyword(ACM_SYMBOL)                           Keyword_##ACM_SYMBOL
#  define MatchHolder(ACM_SYMBOL)                       Keyword_##ACM_SYMBOL
#  define ACMachine(ACM_SYMBOL)                         ACMachine_##ACM_SYMBOL
#  define ACM_create4(ACM_SYMBOL, eq, copy, dtor)       ACM_create_##ACM_SYMBOL((eq), (copy), (dtor))
#  define ACM_create2(ACM_SYMBOL, eq)                   ACM_create4(ACM_SYMBOL, eq, 0, 0)
#  define ACM_create1(ACM_SYMBOL)                       ACM_create4(ACM_SYMBOL, 0, 0, 0)
#  define ACM_create(...)                               VFUNC(ACM_create, __VA_ARGS__)

#  define ACM_register_keyword4(machine, keyword, value, dtor)          (machine)->vtable->register_keyword ((machine), (keyword), (value), (dtor))
#  define ACM_register_keyword2(machine, keyword)                       ACM_register_keyword4(machine, keyword, 0, 0)
#  define ACM_register_keyword(...)                                     VFUNC(ACM_register_keyword, __VA_ARGS__)

#  define ACM_is_registered_keyword3(machine, keyword, value)           (machine)->vtable->is_registered_keyword ( (machine), (keyword), (value))
#  define ACM_is_registered_keyword2(machine, keyword)                  ACM_is_registered_keyword3(machine, keyword, 0)
#  define ACM_is_registered_keyword(...)                                VFUNC(ACM_is_registered_keyword, __VA_ARGS__)

#  define ACM_unregister_keyword(machine, keyword)                      (machine)->vtable->unregister_keyword ( (machine), (keyword))
#  define ACM_nb_keywords(machine)                                      (machine)->vtable->nb_keywords ((machine))

#  define ACM_foreach_keyword(machine, operator)                        do { (machine)->vtable->foreach_keyword ((machine), (operator)) ; } while (0)
#  define ACM_release(machine)                                          do { (machine)->vtable->release ((machine)) ; } while (0)
#  define ACM_nb_matches(machine, letter)                               (machine)->vtable->nb_matches ((machine), (letter))
#  define ACM_reset(machine)                                            (machine)->vtable->reset ((machine))

#  define ACM_get_match4(machine, index, matchholder, value)            (machine)->vtable->get_match ((machine), (index), (matchholder), (value))
#  define ACM_get_match3(machine, index, matchholder)                   ACM_get_match4(machine, index, matchholder, 0)
#  define ACM_get_match2(machine, index)                                ACM_get_match4(machine, index, 0, 0)
#  define ACM_get_match(...)                                            VFUNC(ACM_get_match, __VA_ARGS__)

#  define ACM_REGISTER_KEYWORD4(machine, keyword, value_ptr, dtor)      do { __auto_type tmp = ACM_register_keyword4 ((machine), (keyword), (value_ptr), (dtor)); if (tmp) (machine) = tmp; } while (0)
#  define ACM_REGISTER_KEYWORD2(machine, keyword)                       ACM_REGISTER_KEYWORD4(machine, keyword, 0, 0)
#  define ACM_REGISTER_KEYWORD(...)                                     VFUNC(ACM_REGISTER_KEYWORD, __VA_ARGS__)
// END MACROS

#endif
