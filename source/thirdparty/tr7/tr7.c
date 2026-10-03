/*************************************************************************
* T R 7    2 . X
*
* Tiny R7RS-small scheme interpreter
*
* SPDX-License-Identifier: 0BSD
* https://gitlab.com/jobol/tr7
*
* This is a huge file that contains everything. The good reason for that
* is that you can copy it an use it in any of your project.
* No lib, no dependency, customizable, ... many cool advantages.
* The snag is: this is a huge file.
*
* In order to make browsing of sources more easy, the code is organized
* in sections well identified by comments.
*
* Here is the summary of sections:
*
*   - FEATURING    Handles feature adapation, options of compiling
*   - INCLUDING    Includes
*   - CONSTANTS    Declaration of constants
*   - INTERNAL_TR7 Internal tr7_t typing
*   - MACROS       Declaration of macros
*   - ENUMERATIONS Definition of enumerations
*   - CALLBACKS    Definition of callbacks
*   - STRUCTURES   Definition of structures
*   - DECLARATION  Predeclaration of functions
*   - DATA         Global variables
*   - MEMORY       Memory management
*   - RECENTS      Holding of recent allocations
*   - PAIRS        Management of pairs
*   - LISTS        Management of lists
*   - CHARACTER    Management of characters
*   - CONTROL_CHARACTER Control character naming
*   - IMMUTABLE    Immutable flag for cells
*   - BUFFERS      Management of buffers
*   - BYTEVECTORS  Management of bytevectors
*   - STRINGS      Management of strings
*   - SYMBOLS      Management of symbols
*   - VECTORS      Management of vectors
*   - SYMBOLS_SET  Management of symbol set
*   - FILE_SEARCH  Searching of files
*   - EXTENSIONS   Handle extensions
*   - BIGINTS      Handle big integers
*   - OVERFLOW     Arithmetic with overflow detection
*
**************************************************************************
*
* BEGIN OF PROGRAM SECTION
*/
#ifndef _WANT_DECLARATIONS_
#define _WANT_DECLARATIONS_
/*
**************************************************************************
* SECTION FEATURING Handles feature adaptation, options of compiling
* -----------------
*
* The macros below define what R7RS libraries to implement.
* Macros have to be set at compile time using compiling option
* of the form -DMACRONAME=1 or -DMACRONAME=0
*
* Features of R7RS-small
* ----------------------------+---+----------------------------------
*  Macro                       Def  Description
* ----------------------------+---+----------------------------------
*  USE_SCHEME_CASE_LAMBDA     : 1 : Implement (scheme case-lambda)
*  USE_SCHEME_CHAR            : 1 : Implement (scheme char)
*  USE_SCHEME_COMPLEX         : 0 : Implement (scheme complex)
*  USE_SCHEME_CXR             : 1 : Implement (scheme cxr)
*  USE_SCHEME_EVAL            : 1 : Implement (scheme eval)
*  USE_SCHEME_FILE            : 1 : Implement (scheme file)
*  USE_SCHEME_INEXACT         : 1 : Implement (scheme inexact)
*  USE_SCHEME_LAZY            : 1 : Implement (scheme lazy)
*  USE_SCHEME_LOAD            : 1 : Implement (scheme load)
*  USE_SCHEME_PROCESS_CONTEXT : 1 : Implement (scheme process-context)
*  USE_SCHEME_READ            : 1 : Implement (scheme read)
*  USE_SCHEME_REPL            : 1 : Implement (scheme repl)
*  USE_SCHEME_TIME            : 1 : Implement (scheme time)
*  USE_SCHEME_WRITE           : 1 : Implement (scheme write)
*
* Features of R7RS-large
* ----------------------------+---+----------------------------------
*  Macro                       Def  Description
* ----------------------------+---+----------------------------------
*  USE_SCHEME_BOX             : 1 : Implement (scheme box)
*
* Features of SRFIs
* ----------------------------+---+----------------------------------
*  Macro                       Def  Description
* ----------------------------+---+----------------------------------
*  USE_SRFI_136               : 1 : Implement (srfi 136)
*
* Features of TR7
* ----------------------------+---+----------------------------------
*  Macro                       Def  Description
* ----------------------------+---+----------------------------------
*  USE_TR7_MISC               : 1 : Implement (tr7 misc)
*  USE_TR7_EXTRA              : 1 : Implement (tr7 extra)
*  USE_TR7_ENVIRONMENT        : 1 : Implement (tr7 environment)
*  USE_TR7_EXTENSION          : 0 : Implement (tr7 extension)
*  USE_TR7_GC                 : 1 : Implement (tr7 gc)
*  USE_TR7_DEBUG              : 1 : Implement (tr7 debug)
*  USE_TR7_TRACE              : 1 : Implement (tr7 trace)
*  USE_TR7_TAGGED_CLOSURES    : 1 : Implement (tr7 tagged-closures)
*/
#ifndef USE_SCHEME_CASE_LAMBDA
#define USE_SCHEME_CASE_LAMBDA 1
#endif
#ifndef USE_SCHEME_CHAR
#define USE_SCHEME_CHAR 1
#endif
#ifndef USE_SCHEME_COMPLEX
#define USE_SCHEME_COMPLEX 0
#endif
#ifndef USE_SCHEME_CXR
#define USE_SCHEME_CXR 1
#endif
#ifndef USE_SCHEME_EVAL
#define USE_SCHEME_EVAL 1
#endif
#ifndef USE_SCHEME_FILE
#define USE_SCHEME_FILE 1
#endif
#ifndef USE_SCHEME_INEXACT
#define USE_SCHEME_INEXACT 1
#endif
#ifndef USE_SCHEME_LAZY
#define USE_SCHEME_LAZY 1
#endif
#ifndef USE_SCHEME_LOAD
#define USE_SCHEME_LOAD 1
#endif
#ifndef USE_SCHEME_PROCESS_CONTEXT
#define USE_SCHEME_PROCESS_CONTEXT 1
#endif
#ifndef USE_SCHEME_READ
#define USE_SCHEME_READ 1
#endif
#ifndef USE_SCHEME_REPL
#define USE_SCHEME_REPL 1
#endif
#ifndef USE_SCHEME_TIME
#define USE_SCHEME_TIME 1
#endif
#ifndef USE_SCHEME_WRITE
#define USE_SCHEME_WRITE 1
#endif
#ifndef USE_SCHEME_BOX
#define USE_SCHEME_BOX 1
#endif
#ifndef USE_SRFI_136
#define USE_SRFI_136 1
#endif
#ifndef USE_TR7_MISC
#define USE_TR7_MISC 1
#endif
#ifndef USE_TR7_EXTRA
#define USE_TR7_EXTRA 1
#endif
#ifndef USE_TR7_ENVIRONMENT
#define USE_TR7_ENVIRONMENT 1
#endif
#ifndef USE_TR7_EXTENSION
#define USE_TR7_EXTENSION 0
#endif
#ifndef USE_TR7_GC
#define USE_TR7_GC 1
#endif
#ifndef USE_TR7_DEBUG
#define USE_TR7_DEBUG 1
#endif
#ifndef USE_TR7_TRACE
#define USE_TR7_TRACE 1
#endif
#ifndef USE_TR7_TAGGED_CLOSURES
#define USE_TR7_TAGGED_CLOSURES 1
#endif
/*
* TR7 specific features
* ----------------------------+---+----------------------------------
*  Macro                       Def  Description
* ----------------------------+---+----------------------------------
*  HAS_GREEDY_SYNTAX          : 1 : Implement experimental #![no-]greedy-syntax
*  DUMP_LAMBDAS               : 0 : Allows to dump code of lambdas
*  DUMP_CLOSURES              : 0 : Allows to dump closures
*  SHOW_OPCODES               : 1 : Allows to show names of opcodes
*  USE_ASCII_NAMES            : 1 : Allows naming ASCII control codes
*  USE_MATH                   : 1 : Allows use of functions of math.h
*  EXTRA_TRACING              : 1 : Add extra data to trace execution (if USE_TR7_TRACE)
*  IGNORE_UNKNOWN_SHARP       : 1 : Ignore unknown sharp expression
*  AUTO_SHARP_TO_SYMBOL       : 0 : Translate unknown sharp expression to symbol
*  IGNORE_OVERFLOWS           : 1 : Ignore any integer overflow
*                             :   : (default to 1 at the moment, will change)
*  DEBUG_LINES                : 1 : When USE_TR7_DEBUG is on, also track lines
*  HAS_CHECK_TYPES_NO         : 1 : Allows implementing (check-types BOOLEAN)
*/
#ifndef HAS_GREEDY_SYNTAX
#define HAS_GREEDY_SYNTAX 1
#endif
#ifndef DUMP_LAMBDAS
#define DUMP_LAMBDAS 0
#endif
#ifndef DUMP_CLOSURES
#define DUMP_CLOSURES 0
#endif
#ifndef SHOW_OPCODES
#define SHOW_OPCODES 1
#endif
#ifndef USE_ASCII_NAMES
#define USE_ASCII_NAMES 1
#endif
#ifndef USE_MATH
#define USE_MATH 1
#endif
#ifndef EXTRA_TRACING
#define EXTRA_TRACING 1
#endif
#ifndef IGNORE_UNKNOWN_SHARP
#ifndef AUTO_SHARP_TO_SYMBOL
#define IGNORE_UNKNOWN_SHARP 1
#define AUTO_SHARP_TO_SYMBOL 0
#else
#define IGNORE_UNKNOWN_SHARP !AUTO_SHARP_TO_SYMBOL
#endif
#endif
#ifndef AUTO_SHARP_TO_SYMBOL
#define AUTO_SHARP_TO_SYMBOL 1
#endif
#ifndef USE_RATIOS
#define USE_RATIOS 0
#endif
#ifndef IGNORE_OVERFLOWS
#define IGNORE_OVERFLOWS 1
#endif
#ifndef DEBUG_LINES
#define DEBUG_LINES 1
#endif
#ifndef HAS_CHECK_TYPES_NO
#define HAS_CHECK_TYPES_NO 1
#endif
/*
* Internal constants
* ----------------------------+-------+----------------------------------
*  Macro                       Default  Description
* ----------------------------+-------+----------------------------------
*  VERSION                  : unknown : Default TR7 full version (see Makefile)
*  MINVERSION               : unknown : Default TR7 minor version (see Makefile)
*  ITEM_NSEGMENT              :    10 : Maximum count of segments
*  ITEM_SEGSIZE           : 150000000 : Count of cell per segment
*  NSEGMENT_INITIAL           :     1 : Initial count of segment
*  NRECENTS                   :    30 : Count of temporary protected cells
*  STRBUFFSIZE                :   256 : Size internal buffer
*  SCRATCH_SIZE               :   256 : Default block size for scratch
*  UNREAD_COUNT               :     5 : Size of unread buffer
*  NOMEM_LEVEL                :    20 : Count of cell raising no_memory
*  NVALUESMAX                 :    20 : Maximum count of values (for results ...)
*  DIR_SEP_CHAR               :   '/' : Directory separator character
*  PATH_SEP_CHAR              :   ':' : Path list item separator character
*  LIB_SEP_CHAR       :  DIR_SEP_CHAR : Separator of library item for paths
*  LIBNAME_MAXSZ              :   200 : Maximum length of library path
*  SYMBOL_SET_SIZE            :   461 : Vector size for set of symbols
*  DEFAULT_ENV_SIZE           :    19 : Vector size for other environments
*  INTERACTION_ENV_SIZE       :    29 : Vector size for interaction environments
*  COMMON_ROOT_ENV            :     0 : Provide hierarchical root environments
*  STACK_INITIAL_SIZE         :  1000 : Initial stack size
*  STACK_GROW_MUL             :     3 : Stack grow multiplier
*  STACK_GROW_DIV             :     3 : Stack grow divisor
*  STACK_GROW_INC             :     3 : Stack grow increment
*  STACK_SAFEGAP_INIT         :    20 : Initial safe gap when using global stack safety
*  STACK_SIZE_MAX           : 1000000 : Max stack size
*  SIZE_PREFIXING_BUFFER      :   200 : Size of the buffer used when prefixing
*  TRANSFORM_DEPTH_MAX        :     8 : Transformation depth of imbrication
*  DEBUG_SYNTAX               :     0 : Show syntax processing (high level)
*                                     : 0 show nothing, 1 show match, >1 show any
*  TRACE_SYNTAX               :     0 : Trace syntax processing (low level)
*  SWITCHED_OPERATORS         :     1 : Use a switch case for operators
*/
#ifndef VERSION
#define VERSION         "unknown"
#endif
#ifndef MINVERSION
#define MINVERSION      "unknown"
#endif
#ifndef ITEM_NSEGMENT
#define ITEM_NSEGMENT   10
#endif
#ifndef ITEM_SEGSIZE
#define ITEM_SEGSIZE    150000000
#endif
#ifndef NSEGMENT_INITIAL
#define NSEGMENT_INITIAL 1
#endif
#if NSEGMENT_INITIAL > ITEM_NSEGMENT
#  undef NSEGMENT_INITIAL
#  define NSEGMENT_INITIAL ITEM_NSEGMENT
#endif
#ifndef NRECENTS
#define NRECENTS 30
#endif
#ifndef STRBUFFSIZE
#define STRBUFFSIZE 256
#endif
#ifndef SCRATCH_SIZE
#define SCRATCH_SIZE 256
#endif
#ifndef UNREAD_COUNT
#define UNREAD_COUNT 5
#endif
#ifndef NOMEM_LEVEL
#define NOMEM_LEVEL 20
#endif
#ifndef NVALUESMAX
# define NVALUESMAX 20
#endif
#ifndef DIR_SEP_CHAR
#define DIR_SEP_CHAR   '/'
#endif
#ifndef PATH_SEP_CHAR
#ifdef _WIN32
#define PATH_SEP_CHAR  ';'
#else
#define PATH_SEP_CHAR  ':'
#endif
#endif
#ifndef LIB_SEP_CHAR
#define LIB_SEP_CHAR   DIR_SEP_CHAR
#endif
#ifndef LIBNAME_MAXSZ
#define LIBNAME_MAXSZ  200
#endif
#ifndef SYMBOL_SET_SIZE
#define SYMBOL_SET_SIZE 461 /* interaction-environment has about 372 names */
#endif
#ifndef DEFAULT_ENV_SIZE
#define DEFAULT_ENV_SIZE 19
#endif
#ifndef INTERACTION_ENV_SIZE
#define INTERACTION_ENV_SIZE 29
#endif
#ifndef COMMON_ROOT_ENV
#define COMMON_ROOT_ENV 0
#endif
#ifndef STACK_INITIAL_SIZE
#define STACK_INITIAL_SIZE    1000
#endif
#ifndef STACK_GROW_MUL
#define STACK_GROW_MUL        3
#endif
#ifndef STACK_GROW_DIV
#define STACK_GROW_DIV        2
#endif
#ifndef STACK_GROW_INC
#define STACK_GROW_INC        0
#endif
#ifndef STACK_SAFEGAP_INIT
#define STACK_SAFEGAP_INIT    20
#endif
#ifndef STACK_SIZE_MAX
#define STACK_SIZE_MAX        1000000
#endif
#ifndef SIZE_PREFIXING_BUFFER
#define SIZE_PREFIXING_BUFFER 200
#endif
#ifndef TRANSFORM_DEPTH_MAX
#define TRANSFORM_DEPTH_MAX   8
#endif
#ifndef DEBUG_SYNTAX
#define DEBUG_SYNTAX 0
#endif
#ifndef TRACE_SYNTAX
#define TRACE_SYNTAX 0
#endif
#ifndef SWITCHED_OPERATORS
#define SWITCHED_OPERATORS 1
#endif
/*
* Tunable code optimizations:
* ----------------------------+---+----------------------------------
*  Macro                       Def  Description
* ----------------------------+---+----------------------------------
*  STRESS_GC_RESILIENCE       : 0 : Enforces GC collection at each allocation
*  STACKED_GC                 : 1 : GC use the program stack to mark recursively
*  GLOBAL_STACK_SAFETY        : 1 : Ensure stack safety globally
*  HOLD_UNIQUE_INSTANCE       : 1 : Programs record only one instance of repeated data
*  PURGE_SYMBOLS              : 1 : Purge unused symbols
*
* Normally that value must be 0.
* When set to one, a garbage collection cycle is run
* each time an allocation is required.
* This is very convenient to track bugs linked to GC.
*/
#ifndef STRESS_GC_RESILIENCE
# define STRESS_GC_RESILIENCE 0
#endif
/*
* Stacked GC (STACKED_GC!=0) uses C stack when it explores
* live objects. Otherwise, the GC mostly uses objects it explore
* to keep track of its exploration.
*/
#ifndef STACKED_GC
# define STACKED_GC 1
#endif
/*
* Turn on recording of stack requirements for lambdas and lets
* in such way that stack check is made only at entry of functions,
* not each time a value is pushed.
* Seems that on OOO speculative processors, disabling it is better.
* Tune it to get best performances
*/
#ifndef GLOBAL_STACK_SAFETY
# define GLOBAL_STACK_SAFETY 1
#endif
/*
* Make unique instace of data in programs or not.
* ex: (lambda (x) (if x '(a b) '(a b))) holds only one '(a b) or two
*/
#ifndef HOLD_UNIQUE_INSTANCE
#define HOLD_UNIQUE_INSTANCE 1
#endif
/*
* Purge symbols' set of the unreferened symbols.
* You should keep this option on because it avoids symbol set to grow
* indefinitely.
*/
#ifndef PURGE_SYMBOLS
#define PURGE_SYMBOLS 1
#endif
/*
**************************************************************************
* SECTION INCLUDING - Includes
* -----------------
*
* get tr7.h
*/
#include "tr7.h"
/*
* Get the standards headers
*/
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <float.h>
#include <ctype.h>
#include <wctype.h>
#include <time.h>
#include <stdalign.h>
#if USE_MATH
#  include <math.h>
#endif
#ifdef _WIN32
#  define snprintf _snprintf
#  include <io.h>
#  define access _access
#  undef R_OK
#  undef F_OK
#  define F_OK 00
#  define R_OK 04
#else
#  include <unistd.h>
#endif
#if USE_TR7_EXTENSION
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif
#endif
#ifndef PATH_MAX
#define PATH_MAX 255
#endif
#ifdef _MSC_VER
#include <basetsd.h>
#include <malloc.h>
typedef SSIZE_T ssize_t;
#define TR7_ALLOCA(sz) _alloca(sz)
#else
#define TR7_ALLOCA(sz) __builtin_alloca(sz)
#endif
/*
**************************************************************************
* SECTION CONSTANTS - Declaration of constants
* -----------------
*
* length of a buffer receiving utf8 encoding of a char
*/
#define UTF8BUFFSIZE     6
/*
* value indicating no argument count limit
*/
#define INF_ARG         -1
/*
* indexes and count for standard ports
*/
#define IDX_STDIN        0  /* standard input port index */
#define IDX_STDOUT       1  /* standard output port index */
#define IDX_STDERR       2  /* standard error port index */
#define COUNT_IDX_STD    3  /* count of standard ports */
/*
* flags for printing s-expr
*/
#define PRTFLG_ESCAPE      1     /* escape string and characters */
#define PRTFLG_LOOPS       2     /* detect loops */
#define PRTFLG_SHAREDS     4     /* detect shared parts */
/*
* flag for compiling let expression family
*/
#define CPL_LET            0     /* let */
#define CPL_LET_STAR       1     /* let* */
#define CPL_LET_REC        2     /* letrec */
#define CPL_LET_VALUES     4     /* let-values */
/*
**************************************************************************
* SECTION INTERNAL_TR7 - Internal tr7_t typing
* --------------------
*
**************************************************************************
* Definition of tr7_port_t
* ------------------------
*
* Ports are used for referencing internal ports.
* The structure of ports is made of:
*/
typedef struct tr7_port
{
   tr7_head_t head;       /* the kind */
   struct _port_ *_port_; /* opaque pointer */
}
   *tr7_port_t;
/*
* Macros for manipulating ports from tr7_port_t values
*
* - TR7_PORT__PORT_(p): Returns the opaque port of the tr7_port_t p
*/
#define TR7_PORT__PORT_(p)       (p)->_port_
/*
* Macros for manipulating ports from tr7_cell_t values
*
* - TR7_CELL_IS_PORT(c): Returns true if the tr7_cell_t c is a port
* - TR7_CELL_TO_PORT(c): Returns the port value of the tr7_cell_t c
* - TR7_CELL_AS_PORT(c): Returns the symbol value of the tr7_cell_t c
*                      or return NULL when c is not a symbol
* - TR7_CELL_PORT__PORT_(c): Returns the opaque port of the tr7_cell_t c
*/
#define TR7_CELL_IS_PORT(c)      TR7_CELL_IS_KIND((c), Tr7_Head_Kind_Port)
#define TR7_CELL_TO_PORT(c)      ((tr7_port_t)(c))
#define TR7_CELL_AS_PORT(c)      (TR7_CELL_IS_PORT(c) ? TR7_CELL_TO_PORT(c) : NULL)
#define TR7_CELL_PORT__PORT_(c)  TR7_PORT__PORT_(TR7_CELL_TO_PORT(c))
/*
* Macros for manipulating ports from tr7_t values
*
* - TR7_IS_PORT(t): Returns true if the tr7_t t is a port
* - TR7_TO_PORT(t): Returns the port value of the tr7_t t
* - TR7_AS_PORT(t): Returns the port value of the tr7_t t
*                   or return NULL when t is not a port
* - TR7_FROM_PORT(p): Returns the tr7_t value of the tr7_port_t p
* - TR7__PORT__PORT(t): Returns the opaque port of the tr7_t t
*/
#define TR7_IS_PORT(t)           TR7_IS_CELL_KIND((t), Tr7_Head_Kind_Port)
#define TR7_TO_PORT(t)           ((tr7_port_t)TR7_TO_CELL(t))
#define TR7_AS_PORT(t)           (TR7_IS_PORT(t) ? TR7_TO_PORT(t) : NULL)
#define TR7_FROM_PORT(p)         TR7_FROM_CELL(p)
#define TR7__PORT__PORT(c)       TR7_PORT__PORT_(TR7_TO_PORT(c))
/*
**************************************************************************
* Definition of continuations
* ---------------------------
*
* Continuations are used for implementing call/cc.
* The structure of continuations is made of:
*/
typedef struct tr7_continuation
{
   tr7_head_t head;
   tr7_t dynawind;
   tr7_t params;
   tr7_t noper;
   tr7_t stack[];
}
   *tr7_continuation_t;
#define TR7_CELL_IS_CONTINUATION(c)  TR7_CELL_IS_KIND((c), Tr7_Head_Kind_Continuation)
#define TR7_CELL_TO_CONTINUATION(c)  ((tr7_continuation_t)(c))
#define TR7_IS_CONTINUATION(t)       TR7_IS_CELL_KIND((t), Tr7_Head_Kind_Continuation)
#define TR7_TO_CONTINUATION(t)       ((tr7_continuation_t)TR7_TO_CELL(t))
#define TR7_FROM_CONTINUATION(c)     TR7_FROM_CELL(c)
/*
**************************************************************************
*
* Definition of closures for lambda and case-lambda
*/
typedef struct tr7_closure
{
   tr7_head_t head;
   tr7_t description;
   tr7_t upperframes;
#ifdef USE_TR7_TAGGED_CLOSURES
   tr7_t tag;
#endif
}
   *tr7_closure_t;

#define TR7_TO_CLOSURE(t)     ((tr7_closure_t)TR7_TO_CELL(t))
#define TR7_IS_LAMBDA(t)      TR7_IS_CELL_KIND((t), Tr7_Head_Kind_Lambda)
#if USE_SCHEME_CASE_LAMBDA
#define TR7_IS_CASE_LAMBDA(t) TR7_IS_CELL_KIND((t), Tr7_Head_Kind_Case_Lambda)
#endif
/*
**************************************************************************
*
* Definition of promises
*/
#if USE_SCHEME_LAZY

typedef struct tr7_promise
{
   tr7_head_t head;
   tr7_t item;
}
   *tr7_promise_t;

#define TR7_IS_PROMISE(t)             TR7_IS_CELL_KIND((t), Tr7_Head_Kind_Promise)
#define TR7_TO_PROMISE(t)             ((tr7_promise_t)TR7_TO_CELL(t))
#define TR7_FROM_PROMISE(p)           TR7_FROM_CELL(p)
#define TR7_PROMISE_HEAD_VALUE        TR7_MAKE_HEAD(0, Tr7_Head_Kind_Promise)
#define TR7_PROMISE_HEAD_DELAY        TR7_MAKE_HEAD(1, Tr7_Head_Kind_Promise)
#define TR7_PROMISE_HEAD_DELAY_FORCE  TR7_MAKE_HEAD(2, Tr7_Head_Kind_Promise)
#endif
/*
**************************************************************************
*
* Definition of parameters
*
* A parameter has a current value and a converter
* The head is used only to record the kind
*/
typedef struct tr7_parameter
{
   tr7_head_t head;
   tr7_t value;
   tr7_t converter;
}
   *tr7_parameter_t;

#define TR7_IS_PARAMETER(t)   TR7_IS_CELL_KIND((t), Tr7_Head_Kind_Parameter)
#define TR7_TO_PARAMETER(t)   ((tr7_parameter_t)TR7_TO_CELL(t))
#define TR7_FROM_PARAMETER(p) TR7_FROM_CELL(p)
/*
**************************************************************************
*
* Definition of syntax transforms
*/
typedef struct tr7_transform
{
   tr7_head_t head;
   tr7_t ellipsis;
   tr7_t literals;
   tr7_t rules;
   tr7_t env;
}
   *tr7_transform_t;
#define TR7_IS_TRANSFORM(t)       TR7_IS_CELL_KIND((t), Tr7_Head_Kind_Transform)
#define TR7_TO_TRANSFORM(t)       ((tr7_transform_t)TR7_TO_CELL(t))
#define TR7_FROM_TRANSFORM(p)     TR7_FROM_CELL(p)
/*
**************************************************************************
*
* Definition of environments
*/
typedef struct tr7_environment
{
   tr7_head_t head;         /* count */
   tr7_t lower;             /* lower environment */
   tr7_t items[];           /* content */
}
   *tr7_environment_t;

#define TR7_IS_ENVIRONMENT(t)    TR7_IS_CELL_KIND((t), Tr7_Head_Kind_Environment)
#define TR7_TO_ENVIRONMENT(t)    ((tr7_environment_t)TR7_TO_CELL(t))
/*
**************************************************************************
* Definition of tr7_record_t
* --------------------------
*
* Record cells are just like vectors.
*/
typedef struct tr7_vector *tr7_record_t;
/*
* Macros for manipulating records are:
*/
#define TR7_CELL_IS_RECORD(c)       TR7_CELL_IS_KIND((c), Tr7_Head_Kind_Record)
#define TR7_CELL_TO_RECORD(c)       ((tr7_record_t)(c))
#define TR7_IS_RECORD(t)            TR7_IS_CELL_KIND((t), Tr7_Head_Kind_Record)
#define TR7_TO_RECORD(t)            ((tr7_record_t)TR7_TO_CELL(t))
#define TR7_AS_RECORD(t)            (TR7_IS_RECORD(t) ? TR7_TO_RECORD(t) : NULL)
#define TR7_FROM_RECORD(r)          TR7_FROM_CELL(r)

#define TR7_RECORD_LENGTH(r)        TR7_HEAD_UVALUE((r)->head)
#define TR7_RECORD_ITEMS(r)         ((r)->items)
#define TR7_RECORD_ITEM(r,i)        (TR7_RECORD_ITEMS(r)[i])

#define TR7_CELL_RECORD_LENGTH(c)   TR7_RECORD_LENGTH(TR7_CELL_TO_RECORD(c))
#define TR7_CELL_RECORD_ITEMS(c)    TR7_RECORD_ITEMS(TR7_CELL_TO_RECORD(c))
#define TR7_CELL_RECORD_ITEM(c,i)   TR7_RECORD_ITEM(TR7_CELL_TO_RECORD(c),i)

#define TR7_LENGTH_RECORD(t)        TR7_RECORD_LENGTH(TR7_TO_RECORD(t))
#define TR7_ITEMS_RECORD(t)         TR7_RECORD_ITEMS(TR7_TO_RECORD(t))
#define TR7_ITEM_RECORD(t,i)        TR7_RECORD_ITEM(TR7_TO_RECORD(t),i)
/*
**************************************************************************
* Defining of foreign pointers
*/

typedef struct tr7_cptr
{
   tr7_head_t         head;     /* kind */
   void              *value;    /* the pointer */
   tr7_cptr_vtable_t *vtable;   /* vtable */
}
   *tr7_cptr_t;

#define TR7_CELL_IS_CPTR(c)  TR7_CELL_IS_KIND(c, Tr7_Head_Kind_CPointer)
#define TR7_CELL_TO_CPTR(c)  ((tr7_cptr_t)(c))

#define TR7_IS_CPTR(t)       TR7_IS_CELL_KIND((t), Tr7_Head_Kind_CPointer)
#define TR7_TO_CPTR(t)       ((tr7_cptr_t)TR7_TO_CELL(t))
#define TR7_AS_CPTR(t)       ((tr7_cptr_t)TR7_AS_CELL_KIND(t, Tr7_Head_Kind_CPointer))
#define TR7_FROM_CPTR(f)     TR7_TO_CELL(f)

/*
*****************************************************************************
* Defining of foreign functions
*/
typedef struct tr7_cfunc
{
   tr7_head_t       head;
   const tr7_C_func_def_t *definition;
}
   *tr7_cfunc_t;

#define TR7_IS_CFUNC(t)       TR7_IS_CELL_KIND((t), Tr7_Head_Kind_CFunction)
#define TR7_TO_CFUNC(t)       ((tr7_cfunc_t)TR7_TO_CELL(t))
#define TR7_FROM_CFUNC(f)     TR7_TO_CELL(f)

#define TR7_CELL_IS_CFUNC(c)  TR7_CELL_IS_KIND(c, Tr7_Head_Kind_CFunction)
#define TR7_CELL_TO_CFUNC(c)  ((tr7_cfunc_t)(c))

/*
*****************************************************************************
* Defining of record functions
*/
typedef struct tr7_recfun
{
   tr7_head_t  head;       /* kind and count */
   tr7_t       opterm;     /* operator and index */
   tr7_t       recdesc;    /* the record descriptor */
   tr7_t       args[];     /* arguments for the  constructor */
}
   *tr7_recfun_t;

#define TR7_IS_RECFUN(t)       TR7_IS_CELL_KIND((t), Tr7_Head_Kind_RecFun)
#define TR7_TO_RECFUN(t)       ((tr7_recfun_t)TR7_TO_CELL(t))
#define TR7_FROM_RECFUN(f)     TR7_TO_CELL(f)

#define TR7_CELL_IS_RECFUN(c)  TR7_CELL_IS_KIND(c, Tr7_Head_Kind_RecFun)
#define TR7_CELL_TO_RECFUN(c)  ((tr7_recfun_t)(c))
/*
*****************************************************************************
* Subdefinition of the VSP kind VSP_INTERNAL
* ------------------------------------------
*
* It is used to encode procedures, syntaxes, operators and (ATM) instructions.
*
* It uses the least significant bit of the remaining
* value to distinguish between procedures and syntax.
*
* The idea is to implement the following schema:
*
* *******************************************************************
* *                  <- 25 or 57 bits ->  6   5   4   3   2   1   0
* *                 +---+---+ - - - - - +---+---+---+---+---+---+---+
* *                 |   |   |           | INTER | V S P |  SPECIAL  |
* *                 +---+---+ - - - - - +---+---+---+---+---+---+---+
* * procedures      |   |   |           | 0   0 | 1   0 | 0   1   1 |
* *                 +---+---+ - - - - - +---+---+---+---+---+---+---+
* * syntaxes        |   |   |           | 0   1 | 1   0 | 0   1   1 |
* *                 +---+---+ - - - - - +---+---+---+---+---+---+---+
* * operators       |   |   |           | 1   0 | 1   0 | 0   1   1 |
* *                 +---+---+ - - - - - +---+---+---+---+---+---+---+
* *                 |   |   |           | 1   1 | 1   0 | 0   1   1 |
* *                 +---+---+ - - - - - +---+---+---+---+---+---+---+
* *******************************************************************
*
* The wrapped value is unsigned.
*
* To improve computing, the 6 lower bits of tr7_t items are treated
* altogether.
*/
#define TR7_WIDTH_INTERNAL  (2 + TR7_WIDTH_VSP)
#define TR7_MASK_INTERNAL   (0140 | TR7_MASK_VSP)
/*
* Macros for manipulating very special internals are:
*
* - TAG_INTERNAL(k): returns the VSP tag of the for the VSP kind k
* - INTERNAL_TAG(t): returns the VSP tag of a tr7_t t
* - IS_INTERNAL_TAG(k,t): returns true if the tr7_t t is an internals of kind k
* - TR7_INTERNAL(t): returns the signed value of a special internals tr7_t t
* - TR7_INTERNAL(t): returns the unsigned value of a special internals tr7_t t
* - MAKE_INTERNAL(k,v): returns a special tr7_t of kind k and value v
*/
#define TAG_INTERNAL(k)      (((k) << TR7_WIDTH_VSP) | TR7_TAG_VSP(TR7_VSP_INTERNAL))
#define INTERNAL_TAG(t)      (TR72I(t) & TR7_MASK_INTERNAL)
#define IS_INTERNAL_TAG(k,t) (INTERNAL_TAG(t) == TAG_INTERNAL(k))
#define INTERNAL_VALUE(t)    (TR72I(t) >> TR7_WIDTH_INTERNAL)
#define INTERNAL_UVALUE(t)   (TR72U(t) >> TR7_WIDTH_INTERNAL)
#define MAKE_INTERNAL(k,v)   I2TR7(((intptr_t)(v) << TR7_WIDTH_INTERNAL) | (intptr_t)TAG_INTERNAL(k))
/*
* Subtypes of internals:
*/
#define INTERNAL_PROC      0   /* for procedures */
#define INTERNAL_SYNTAX    1   /* for syntaxes */
#define INTERNAL_OPER      2   /* for operators */
#define INTERNAL_INSTR     3   /* for instructions */
/*
* The procedures INTERNAL_PROC
* ----------------------------
*
* It is used to encode procedures.
*
* Macros for manipulating VSP of this kind are:
*
* - IS_PROC(t): returns true is tr7_t t wraps a procedure
* - TO_PROC(t): returns the unsigned procedure wrapped by tr7_t t
* - FROM_PROC(o): returns a tr7_t wrapping the procedure o
*/
#define IS_PROC(t)    IS_INTERNAL_TAG(INTERNAL_PROC,t)
#define TO_PROC(t)    ((procid_t)INTERNAL_UVALUE(t))
#define FROM_PROC(o)  MAKE_INTERNAL(INTERNAL_PROC,o)
/*
* The syntaxes INTERNAL_SYNTAX
* ----------------------------
*
* It is used to encode syntaxes.
*
* Macros for manipulating VSP of this kind are:
*
* - IS_SYNTAX(t): returns true is tr7_t t wraps a syntax
* - TO_SYNTAX(t): returns the unsigned syntax wrapped by tr7_t t
* - FROM_SYNTAX(s): returns a tr7_t wrapping the syntax s
*/
#define IS_SYNTAX(t)    IS_INTERNAL_TAG(INTERNAL_SYNTAX,t)
#define TO_SYNTAX(t)    ((syntaxid_t)INTERNAL_UVALUE(t))
#define FROM_SYNTAX(s)  MAKE_INTERNAL(INTERNAL_SYNTAX,s)
/*
* The operators INTERNAL_OPER
* ---------------------------
*
* It is used to encode operators.
*
* Macros for manipulating VSP of this kind are:
*
* - IS_OPER(t)           test if the tr7_t 't' is an operator
* - TO_OPER(t)           convert the tr7_t 't' to operid_t
* - FROM_OPER(o)         convert the operid_t 'o' to tr7_t
*/
#define IS_OPER(t)    IS_INTERNAL_TAG(INTERNAL_OPER,t)
#define TO_OPER(t)    ((operid_t)INTERNAL_UVALUE(t))
#define FROM_OPER(s)  MAKE_INTERNAL(INTERNAL_OPER,s)
/*
**************************************************************************
* Definition of boxes
*
* Boxes are like vectors of one element.
*/
typedef struct tr7_vector *tr7_box_t;
/*
* Macros for manipulating boxes from tr7_vector_t values
*
* - BOX_LENGTH(b): Returns the unsigned length of the tr7_box_t b
* - BOX_ITEMS(b):  Returns the array of items of the tr7_box_t b
* - BOX_ITEM(b,i): Returns the item of index i of the tr7_box_t b
* - BOX_GET(b):    Returns the value stored in the tr7_box_t b
* - BOX_SET(b,v):  Store v in the tr7_box_t b and returns it
*/
#define BOX_LENGTH(b)        TR7_HEAD_UVALUE((b)->head)
#define BOX_ITEMS(b)         ((b)->items)
#define BOX_ITEM(b,i)        (BOX_ITEMS(b)[i])
#define BOX_GET(b)           BOX_ITEM((b),0)
#define BOX_SET(b,v)         (BOX_GET(b) = (v))
/*
* Macros for manipulating boxes from tr7_cell_t values
*
* - CELL_IS_VECTOR(c):     Returns true if the tr7_cell_t c is a box
* - CELL_TO_VECTOR(c):     Returns the vector value of the tr7_cell_t c
* - CELL_VECTOR_LENGTH(c): Returns the vector unsigned length of the tr7_cell_t c
* - CELL_VECTOR_ITEMS(c):  Returns the vector's array of items of the tr7_cell_t c
* - CELL_VECTOR_ITEM(c,i): Returns the vector's item of index i of the tr7_cell_t c
* - CELL_BOX_GET(c):       Returns the value stored in the tr7_cell_t c
* - CELL_BOX_SET(c,v):     Store v in the tr7_cell_t c and returns it
*/
#define CELL_IS_BOX(c)       TR7_CELL_IS_KIND((c), Tr7_Head_Kind_Box)
#define CELL_TO_BOX(c)       ((tr7_box_t)(c))
#define CELL_BOX_LENGTH(c)   BOX_LENGTH(CELL_TO_BOX(c))
#define CELL_BOX_ITEMS(c)    BOX_ITEMS(CELL_TO_BOX(c))
#define CELL_BOX_ITEM(c,i)   BOX_ITEM(CELL_TO_BOX(c),i)
#define CELL_BOX_GET(c)      BOX_GET(CELL_TO_BOX(c))
#define CELL_BOX_SET(c,v)    BOX_SET(CELL_TO_BOX(c),(v))
/*
* Macros for manipulating boxes from tr7_t values
*
* - IS_BOX(t): Returns true if the tr7_t t is a box
* - TO_BOX(t): Returns the box value of the tr7_t t
* - AS_BOX(t): Returns the box value of the tr7_t t
*              or return NULL when t is not a box
* - FROM_BOX(b):   Returns the tr7_t value of the tr7_box_t b
* - LENGTH_BOX(t): Returns the box unsigned length of the tr7_t t
* - ITEMS_BOX(t):  Returns the box's array of items of the tr7_t t
* - ITEM_BOX(t,i): Returns the box's item of index i of the tr7_t t
* - GET_BOX(t):    Returns the value stored in the tr7_t t box
* - SET_BOX(t,v):  Store v in the tr7_t t box and returns it
*/
#define IS_BOX(t)            TR7_IS_CELL_KIND((t), Tr7_Head_Kind_Box)
#define TO_BOX(t)            CELL_TO_BOX(TR7_TO_CELL(t))
#define AS_BOX(t)            (IS_BOX(t) ? TO_BOX(t) : NULL)
#define FROM_BOX(b)          TR7_FROM_CELL(b)
#define LENGTH_BOX(t)        BOX_LENGTH(TO_BOX(t))
#define ITEMS_BOX(t)         BOX_ITEMS(TO_BOX(t))
#define ITEM_BOX(t,i)        BOX_ITEM(TO_BOX(t),i)
#define GET_BOX(t)           BOX_GET(TO_BOX(t))
#define SET_BOX(t,v)         BOX_SET(TO_BOX(t),(v))
/*
*************************************************************************
* SECTION MACROS - Declaration of macros
* --------------
*
* Some macro of general duty
*
* is 'c' a control character?
*/
#define IS_CONTROL_CODE(c)  ((c) == 127 || ((unsigned)(c)) < 32)
/*
* converts the digit to a character
* works for digit in range 0..35, giving '0'..'9','a'..'z'
*/
#define DIGIT2CHAR(digit)  ((char)((digit) < 10 ? ('0' + (digit)) : (('a' - 10) + (digit))))
/*
* return an HASH code for a pointer 'p'
*/
#define HASHPTR(p)  ((unsigned)((((uintptr_t)(p))>>4) & 0x7ffffff))
/*
* Access to symbols by name
*
* - SYMBOLID(code)      expand to the index of the symbol of code
* - SYNTAXID(code)      expand to the index of the syntax of code
* - PROCID(code)        expand to the index of the procedure of code
* - OPERID(code)        expand to the index of the operator of code
* - INSTRID(code)       expand to the index of the instruction of code
*/
#define SYMBOLID(CODE)   SYMBOLID_##CODE
#define SYNTAXID(CODE)   SYNTAXID_##CODE
#define OPERID(CODE)     OPERID_##CODE
#define PROCID(CODE)     PROCID_##CODE
#define INSTRID(CODE)    INSTRID_##CODE
/*
* helpers for accessing statically defined cells
* CAUTION: alignment must be consistent
*/
#define STATIC_CELL(x)  ((tr7_t)(&(((char*)x)[TR7_TAG_CELL])))
/*
* helpers for accessing predefined symbols:
*
* - SYMBOL_AT(index)  return the tr7_t value for the symbol at index
* - SYMBOL(code)      return the tr7_t value for the symbol of code
* - SYNTAX(code)      return the tr7_t value for the syntax of code
* - PROC(code)        return the tr7_t value for the procedure of code
* - OPER(code)        return the tr7_t value for the operator of code
*/
#define SYMBOL_AT(IDX)    STATIC_CELL(&predefined_symbols[IDX])
#define SYMBOL(CODE)      SYMBOL_AT(SYMBOLID(CODE))
#define SYNTAX(CODE)      FROM_SYNTAX(SYNTAXID(CODE))
#define PROC(CODE)        FROM_PROC(PROCID(CODE))
#define OPER(CODE)        FROM_OPER(OPERID(CODE))
/*
* The macro below are used for computing the count of cell to be
* allocated:
*
* NCELL_OF_SIZE(size): returns the count of cells needed for a given size in bytes
* NCELL_OF_TYPE(type): returns the count of cells needed for a given C type
* NCELL_OF_PTR(ptr):   returns the count of cells needed for data pointed by ptr
*/
#define NCELL_OF_SIZE(size)   (((size) + sizeof(union tr7_cell) - 1) / sizeof(union tr7_cell))
#define NCELL_OF_TYPE(type)   NCELL_OF_SIZE(sizeof(type))
#define NCELL_OF_PTR(ptr)     NCELL_OF_SIZE(sizeof(*(ptr)))
/*
* FREECELL_MIN_SUCC:   minimum count of successive pair when recording free cells
*/
#define FREECELL_MIN_SUCC     NCELL_OF_TYPE(freecells_t)
/*
* This macro is a helper for allocating cells for a typed pointer ptr
*/
#define GET_CELLS(tsc,ptr,final) get_cells(tsc,NCELL_OF_PTR(ptr),final)
/*
* Push the item on the stack without checking if push is possible
*/
#define DATA_PUSH_UNSAFE(tsc,item) \
            (*(--((tsc)->stack.data)) = (item))
/*
* Push the item on the stack, take care to resize the stack if needed
*/
#define DATA_PUSH_SAFE(tsc,item) \
            do { \
               if ((tsc)->stack.oper == (tsc)->stack.data) \
                  data_stack_push_rescue((tsc), (item)); \
               else \
                  DATA_PUSH_UNSAFE((tsc),(item)); \
            } while(0)
/*
* Push item on the stack, using the default strategy
*/
#if GLOBAL_STACK_SAFETY
# define DATA_PUSH(tsc,item) DATA_PUSH_UNSAFE(tsc,item)
#else
# define DATA_PUSH(tsc,item) DATA_PUSH_SAFE(tsc,item)
#endif
/*
* Pop 'n' items from data stack without checking if it is possible
*/
#define DATA_POP_N(tsc,n)  ((tsc)->stack.data += (n))
/*
* Pop first item from data stack and return it without checking if it is possible
*/
#define DATA_POP(tsc)      (*(tsc)->stack.data++)
/*
* The data at offset
*/
#define DATA(tsc,offset)   (tsc)->stack.data[offset]
/*
* Pop 'n' items from data stack without checking if it is possible
*/
#define OPER_POP(tsc,n)    ((tsc)->stack.oper -= (n))
/*
* The oper at offset
*/
#define OPER_AT(tsc,offset)   (tsc)->stack.oper[-(1 + (int)(offset))]
/*
* Test if there is still oper
*/
#define HAS_OPER(tsc)      ((tsc)->stack.oper != (tsc)->stack.head)
/*
* Set of macro for defining record descriptors
*
* Reference to a descriptor of name
*/
#define RECORD_DESC(name)  STATIC_CELL(recdesc_##name)
/*
* Start description of a record with no parent:
*   - name   the name for referencing later using RECORD_DESC
*   - symbol the symbol as resolved by SYMBOL(symbol)
*   - fldcnt the count of fields of the record
*/
#define DECLARE_RECORD(name,symbol,fldcnt) \
   static alignas(TR7_ALIGNMENT) tr7_t recdesc_##name[(1 + Record_Desc_Idx_First_Field) + 2 * (fldcnt)] = {    \
      TR7_MAKE_HEAD(Record_Desc_Idx_First_Field + 2 * (fldcnt), Tr7_Head_Kind_Record), \
      TR7_VOID,       \
      SYMBOL(symbol), \
      TR7_FALSE,      \
      TR7_FROM_INT(fldcnt),
/*
* Start description of a record with a parent
*   - name      the name for referencing later using RECORD_DESC
*   - symbol    the symbol as resolved by SYMBOL(symbol)
*   - fldcnt    the count of new fields of the record itself
*   - parent    the parent record as resolved by RECORD_DESC(parent)
*   - parfldcnt count of fields of the parent including fields of grand-parents
*/
#define DECLARE_SUBRECORD(name,symbol,fldcnt,parent,parfldcnt) \
   static alignas(TR7_ALIGNMENT) tr7_t recdesc_##name[1 + Record_Desc_Idx_First_Field + 2 * (fldcnt)] = {    \
      TR7_MAKE_HEAD(Record_Desc_Idx_First_Field + 2 * (fldcnt), Tr7_Head_Kind_Record), \
      TR7_VOID,            \
      SYMBOL(symbol),      \
      RECORD_DESC(parent), \
      TR7_FROM_INT((fldcnt) + (parfldcnt)),
/*
* Describe fields of a record
*/
#define FIELD_RECORD_RW(name)  SYMBOL(name), TR7_TRUE,
#define FIELD_RECORD_RO(name)  SYMBOL(name), TR7_FALSE,
#define FIELD_RECORD_VOID      TR7_FALSE, TR7_FALSE,
/*
* End of record description
*/
#define END_RECORD };
/*
**************************************************************************
* SECTION ENUMERATIONS - Definition of enumerations
* --------------------
*
* Definition of syntaxid_t, the syntax codes enumeration
*/
typedef enum
{
#define _SYNTAX_(FUNC,NAME,CODE)  SYNTAXID(CODE),
#include __FILE__
   SYNTAXID(MAXDEFINED)
}
   syntaxid_t;
/*
* Definition of procid_t, the procedure codes enumeration
*/
typedef enum
{
#define _PROC___(FUNC,NAME,MIN,MAX,TYP,CODE)  PROCID(CODE),
#include __FILE__
   PROCID(MAXDEFINED)
}
   procid_t;
/*
* Definition of operid_t, the operator codes enumeration
*/
typedef enum
{
#define ___OPER_(FUNC,CODE)                   OPERID(CODE),
#include __FILE__
   OPERID(MAXDEFINED)
}
   operid_t;
/*
* Definition of instrid_t, the instruction codes enumeration
* instruction ids
*/
typedef enum
{
#define _INSTR__(CODE,MOD1,MOD2)   INSTRID(CODE),
#include __FILE__
   INSTRID(MAXDEFINED)
}
   instrid_t;
/*
* Definition of symbolid_t, the symbol codes enumeration
*/
typedef enum
{
#define _SYMBOL_(NAME,CODE)                   SYMBOLID(CODE),
#define _SYNTAX_(FUNC,NAME,CODE)              SYMBOLID(CODE),
#define _PROC___(FUNC,NAME,MIN,MAX,TYP,CODE)  SYMBOLID(CODE),
#include __FILE__
   SYMBOLID(MAXDEFINED)
}
   symbolid_t;
/*
* defines the types of error handler
*/
typedef enum
{
   Guard_Type_Guard,    /* implement 'guard' */
   Guard_Type_Handler,  /* implement 'with-exception-handler' */
   Guard_Type_Root,     /* root REPL evaluation */
   Guard_Type_Leave,    /* leave the loop */
   Guard_Type_Repeat    /* implement reraising exceptions */
}
   guard_type_t;
/*
* indexes and count of items in guard vectors
*/
typedef enum
{
   Guard_Idx_Previous,   /* link to the guard of the guard */
   Guard_Idx_Handler,    /* the handler */
   Guard_Idx_Type,       /* the type as TR7_FROM_INT(type) */
   Guard_Idx_nData,      /* count of data in stack for the handler */
   Guard_Idx_nOper,      /* count of oper in stack for the handler */
   Guard_Idx_Params,     /* head of parameters in the handler */
   Guard_Idx_DynWind,    /* head of dynamic-wind in the handler */
   Guard_Count_Idx       /* count of items for recording a guard */
}
   guard_index_t;
/*
* indexes and count of items in dynawind vectors
*/
typedef enum
{
   DynaWind_Idx_Previous,   /* link to the upper dynamic-wind */
   DynaWind_Idx_Depth,      /* depth of the dynamic-wind as tr7_t value */
   DynaWind_Idx_Before,     /* thunk before */
   DynaWind_Idx_After,      /* thunk after */
   DynaWind_Idx_Params,     /* head of parameters */
   DynaWind_Count_Idx       /* count of items for recording dynamic-wind */
}
   dynawind_index_t;
/*
* The records are internally handled in the same way that vectors
* but the first item points to the record type
*/
typedef enum
{
   Record_Idx_RecId, /* index of the record identifier (the record type) */
   Record_Idx_First  /* index of the first real item of the record */
}
   record_index_t;
/*
* The record descriptors are records whose record identifier is TR7_VOID.
*/
typedef enum
{
   Record_Desc_Idx_RecId = Record_Idx_RecId,         /* itself */
   Record_Desc_Idx_Name,         /* name of the record type */
   Record_Desc_Idx_Parent,       /* parent record descriptor */
   Record_Desc_Idx_Field_Count,  /* count of fields */
   Record_Desc_Idx_First_Field   /* index of the first field description */
}
   record_desc_index_t;
/*
* constants for tokens
*/
typedef enum
{
   Token_EOF,           /* END OF INPUT */
   Token_Left_Par,      /* ( left parenthesis */
   Token_Right_Par,     /* ) right parenthesis */
   Token_Dot,           /* . dot */
   Token_Quote,         /* ' quote */
   Token_Back_Quote,    /* ` backquote = quasi-quote */
   Token_Comma,         /* , comma = unquote */
   Token_At,            /* @, unquote-splicing */
   Token_Sharp,         /* # */
   Token_Vector,        /* #( */
   Token_Byte_Vector,   /* #u8( */
   Token_Value,         /* VALUE */
   Token_Datum_Set,     /* #N= define data */
   Token_Datum_Ref,     /* #N# reference data */
   Token_Comment_Datum, /* #; comment data */
   Token_Comment,       /* COMMENT */
   Token_Error,         /* ERROR */
}
   token_type_t;
/*
* constants for read errors
* read error are returned using negative values
* here the value defined are positives
*/
typedef enum
{
   read_no_error = 0,               /* no error, must be zero */
   read_error_oom,                  /* Out Of Memory */
   read_error_unexpected_end,       /* unexpected end */
   read_error_dot_at_begin,         /* dot not following any value */
   read_error_dot_at_middle,        /* dot followed by more than one value */
   read_error_dot_at_end,           /* dot not followed by a value */
   read_error_unclosed_parenthesis, /* closing parenthesis is missing */
   read_error_unopened_parenthesis, /* opening parenthesis is missing */
   read_error_unbound_datum,        /* invalid datum reference */
   read_error_duplicated_datum,     /* already set datum */
   read_error_illegal_token         /* invalid token */
}
   read_status_t;
/*
* kind of compile error reported
*/
typedef enum
{
   Cpl_No_Error,       /* no error */
   Cpl_Error_Syntax,   /* syntax error */
   Cpl_Error_Validity, /* validity error */
   Cpl_Error_Internal, /* internal error */
   Cpl_Error_Eval      /* error reported during evaluation */
}
   cpl_error_kind_t;
/*
* Constants for frames: frames are
* arrays structured as: #(parent [lambda caller code] args... locals...)
*/
typedef enum
{
   Frame_Idx_Link = 0,  /* index of link to parent frame */
   Frame_Idx_Arg0       /* index of the first argument */
}
   frame_index_t;
/*
* Constants for XRUN
*/
typedef enum
{
   XRUN_Idx_XRUN = 0,   /* base of the operator */
   XRUN_Idx_Program,    /* index for the program */
   XRUN_Idx_PC,         /* index of the saved program index */
   XRUN_Idx_Frame,      /* index of the frame */
   _XRUN_Idx_Count_
}
   xrun_index_t;
/*
* Constants for program descriptors: program descriptors are
* arrays structured as: #(szheader nparams nlocals nclosures
*  [nStack] [name [file lines]] code [lines])
*/
typedef enum
{
   Program_Idx_Code = 0,    /* count of values before code */
   Program_Idx_nParams,     /* count of parameters (negative when dot) */
   Program_Idx_nLocals,     /* count of local slots required (including parameters) */
#if USE_TR7_DEBUG
   Program_Idx_Name,        /* name of the lambda */
#if DEBUG_LINES
   Program_Idx_Filename,    /* filename */
   Program_Idx_Lines,       /* count of values before lines */
#endif
#endif
   Program_Idx_Quote0       /* index of first quoted item */
}
   program_index_t;
/*
* Constants for record operations
*/
typedef enum
{
   RecFun_Op_Create = 0,
   RecFun_Op_Test = 1,
   RecFun_Op_Get = 2,
   RecFun_Op_Set = 3,
   _RecFun_Op_Mask_ = 3,
   _RecFun_Op_Shift_ = 2,
}
   recfun_op_t;
/*
* Definition of eval status
* -------------------------
* When evaluated, procedures and operators must return a status
* indicating to the VM execution unit what is the continuation.
* The VM has 2 stages: the main-loop and the code-loop.
* The main-loop runs operators and code-loop until end condition.
* The code-loop runs the code of scheme procedures until it returns
* (or raise).
*/
typedef enum
{
   Cycle_Continue,    /* continue execution of current code in code-loop */
   Cycle_Goto,        /* leave code-loop */
   Cycle_Return,      /* leave code-loop and pop procedure frame */
   Cycle_Raise,       /* leave code-loop raising non-continuable exception */
   Cycle_Raise_Cont,  /* leave code-loop raising continuable exception */
   Cycle_OOM,         /* exit with OUT OF MEMORY report */
   Cycle_Leave,       /* leave code-loop and main-loop */
   Cycle_Leave_Error  /* leave code-loop and main-loop with an error status */
}
   eval_status_t;
/*
*  defining port_flag, the flags for handling ports
*/
typedef enum
{
   port_free = 0,          /* the port is neither active nor used */
   port_file = 1,          /* the port is a FILE */
   port_string = 2,        /* the port is for a string */
   port_bytevector = 4,    /* the port is for a bytevector */
   port_scratch = 8,       /* the port is a scratch buffer */
   port_input = 16,        /* the port is for input */
   port_output = 32,       /* the port is for output */
   port_saw_EOF = 64,      /* indicates that EOF was reached */
   port_closeit = 128,     /* indicates that the FILE can be closed */
   port_binary = 256,      /* indicates that the port is binary */
   port_textual = 512,     /* indicates that the port is textual */
   port_ownbuf = 1024      /* indicates that the port holds its buffer */
}
   port_flag_t;
/*
**************************************************************************
* SECTION DECODE_INSTR
* --------------------
*/
#define DECODE_TAG_NONE     0
#define DECODE_TAG_UINT     1
#define DECODE_TAG_VALUE    2
#define DECODE_TAG_RELOC    3
#define DECODE_TAG_PROC     4
#define DECODE_TAG_SINT     5
#define DECODE_TAG_DATA     6

#define DECODE_TAG_SHIFT    3
#define DECODE_TAG_MASK     7

#define DECODE_MODE(x,y)    ((DECODE_TAG_##x) | ((DECODE_TAG_##y) << DECODE_TAG_SHIFT))

#define DECODE_LENGTH(mode) (((mode) > 7) + ((mode) > 0))
/*
* encoded instruction modes
*/
static uint8_t decode_instr_modes[] = {
#define _INSTR__(CODE,MOD1,MOD2)   DECODE_MODE(MOD1,MOD2),
#include __FILE__
};
/*
* name of instructions
*/
#if USE_TR7_DEBUG
static const char *decode_instr_names[] = {
#define _INSTR__(CODE,MOD1,MOD2)   #CODE,
#include __FILE__
};
#endif
/*
**************************************************************************
* SECTION CALLBACKS - Definition of callbacks
* -----------------
*
* Few predefinitions for callbacks
*/
typedef struct cpl_s *cpl_t;
/*
* callback for enumerating items of environments: the couple 'symbol', 'value'
*/
typedef int (*env_enum_cb_t)(tr7_engine_t tsc, tr7_t symbol, tr7_t value, void *closure);
/*
* definition of a operator functions
*/
typedef eval_status_t (*tr7_oper_t)(tr7_engine_t tsc);
/*
* definition of a procedure functions
*/
typedef eval_status_t (*tr7_proc_t)(tr7_engine_t tsc, int nargs);
/*
* compilation callbacks take 2 arguments:
*  - the compilation context
*  - the list expression to compile
*/
typedef int (*cplcb_t)(cpl_t, tr7_t);
/*
**************************************************************************
* SECTION STRUCTURES - Definition of structures
* ------------------
*
* structure for reporting compiling errors
*/
typedef struct
{
   cpl_error_kind_t kind;   /* kind of error if any */
#if USE_TR7_DEBUG && DEBUG_LINES
   int         line;   /* line of the error */
#endif
   const char *text;   /* text explaining the error */
   tr7_t       args;   /* arguments of the error */
   tr7_t       expr;   /* compiled expression */
   tr7_t       error;  /* reported error when kind == Cpl_Error_Eval */
}
   cpl_error_t;
/*
* Compilation occurs in reverse order, continuations are compiled first.
* Sometime, it makes code harder to understand but generally that
* is not the case.
*
* The structure cpl_t is used to hold a compiling context.
* There is one context per lambda and if needed an extra context
* for let.. at global scope.
*/
struct cpl_s
{
   /* context */
   tr7_engine_t  tsc;         /* the engine */
   cpl_error_t  *error;       /* error reporting */
   cpl_t         upper;       /* upper compilation context */
   int           inlet;       /* 0 at global scope, 1 in let or lambda */

   /* manage variables */
   int           varcount;    /* count of variables */
   tr7_t         vars;        /* list of variables ((symbol index . closure-index) ...) */
   tr7_t         vsyn;        /* list of syntaxic variables ((syntax . vars) ...) */

   /* track of current lambda */
   tr7_pair_t    self;        /* identify currently built lambda */

   /* generation of code */
   unsigned      szcode;      /* allocated size of the code */
   unsigned      poscode;     /* write position in code */
   unsigned      lastopos;    /* position of last written operation */
   uint16_t     *code;        /* buffer for code */
   tr7_t         holders;     /* manifest values */

#if USE_TR7_DEBUG && DEBUG_LINES
   /* tracking source position */
   tr7_t         filename;    /* filename */
   tr7_t         linetrack;   /* alist of compiled line starts */
   tr7_t         cur_line;    /* current line */

   /* encoded positions */
   unsigned      szlines;     /* allocated size for encoded lines */
   unsigned      poslines;    /* write position in encoded lines */
   unsigned      curline;     /* last encoded line */
   uint8_t      *lines;       /* buffer for encoded lines */
#endif
};
/*
* structure used for saving context of variables
*/
typedef struct
{
   tr7_t  vars;      /* saved vars */
   tr7_t  vsyn;      /* saved vsyn */
   int    inlet;     /* saved inlet */
}
   cpl_vars_t;
/*
* structure used for wrapping rename, only and except during import
*/
typedef struct
{
   env_enum_cb_t callback; /* next processing */
   void         *closure;  /* closure of next processing */
   tr7_t         list;     /* list specified */
   tr7_t         done;     /* list of processed items */
}
   import_list_t;
/*
* structure used for wrapping prefix during import
*/
typedef struct
{
   env_enum_cb_t callback; /* next processing */
   void         *closure;  /* closure of next processing */
   unsigned       offset;  /* offset at end of prefix in buffer */
   char buffer[SIZE_PREFIXING_BUFFER]; /* buffer for building names */
}
   import_prefix_t;
/*
* The same structure is used for file io,
* for byte or character in-memory buffers
*/
typedef struct _port_
{
   unsigned flags;        /* flags of the port */
#if USE_TR7_DEBUG && DEBUG_LINES
   int line;              /* current line number */
#endif
   union {
      /* regular files */
      struct {
         FILE *file;      /* recorded filename if any */
         tr7_t filename;  /* the filename as a string */
      } stdio;

      /* string/bytearray/scratch */
      struct {
         uint8_t *curr;   /* current position if the stream */
         uint8_t *end;    /* end of byte stream */
         tr7_t    item;   /* handle to string/bytearray for gc */
         uint8_t *start;  /* start of byte stream */
      } inmem;
   } rep;
   struct {
      tr7_char_t stack[UNREAD_COUNT];
      uint8_t count;
   } unread;
}
   port_t;
/*
* Definition of memory segments
* -----------------------------
*
* A memory segment holds a fixed count of cells, the list of free
* cells it has and 2 binary flags per cell.
*/
typedef struct
{
   uint32_t       count;      /* count of cells in the segment */
   uint32_t      *flags;      /* bit array for flags (mark & final) */
   union tr7_cell cells[];    /* cells of the memory segment
                               * on 32 bits, alignment to 8 bytes boundaries
                               * is ensured if memseg is on 8 bytes itself */
}
   memseg_t;
/*
* The list of successives free cells.
*/
typedef struct freecell
{
   tr7_uint_t      count;     /* count of successive free cells */
   struct freecell *next;     /* link to next item of the list */
}
   freecells_t;
/*
* Definition of tr7_engine_t
* --------------------------
*/
struct tr7_engine
{
   struct {
         tr7_t *data;        /* current data item of the stack  */
         tr7_t *oper;        /* 1 + current oper item of the stack  */
         tr7_t *head;        /* first item of the stack */
         tr7_t *tail;        /* 1 + last item of the stack */
#if GLOBAL_STACK_SAFETY
         unsigned safegap;
#endif
      } stack;
   unsigned nvalues;         /* count of items in values */
   tr7_t values[NVALUESMAX]; /* returned values */
   tr7_t read_value;         /* last read value */
#if USE_TR7_DEBUG && DEBUG_LINES
   tr7_t read_file;          /* file of last read value */
   tr7_t read_lines;         /* lines of last read value */
#endif
   tr7_t stof_guards;        /* stack of guards */
   tr7_t stof_params;        /* head of parameters a-list */
   tr7_t stof_dynawinds;     /* head of dynamic-wind list */
   tr7_t symbols_set;        /* symbol table */
   unsigned stack_size_max;  /* max size of the stack */

   /* memory manager */
   memseg_t     *memsegs[ITEM_NSEGMENT];
   unsigned      nmemseg;    /* count of allocated memory segments */
   unsigned      nsuccfrees; /* count of successive free cells in head chunk */
   tr7_cell_t    firstfree;  /* head of free cells in head chunk */
   size_t        free_cells; /* # of free items */
   freecells_t  *freeshead;  /* head of list of free cells */
   freecells_t  *freestail;  /* tail of list of free cells */

   /* flags */
   unsigned gc_verbose: 1;   /* if gc_verbose is not zero, print gc status */
   unsigned no_memory: 1;    /* Whether mem. alloc. has failed */
   unsigned no_stack: 1;     /* Whether stack has overflown */
   unsigned no_recent: 1;    /* set to avoid recent recording */
   unsigned tracing: 5;
#if HAS_GREEDY_SYNTAX
   unsigned no_greedy_syntax: 1;
#endif
#if HAS_CHECK_TYPES_NO
   unsigned no_check_types: 1;
#endif
#if STRESS_GC_RESILIENCE
   unsigned gc_resilience: 1;
#endif

   /* hot items */
   unsigned recent_count;    /* count of recent pushed */
   tr7_t recents[NRECENTS];  /* the recents recorded */

   /* system memory allocator */
   tr7_malloc_t  malloc;
   tr7_free_t    free;

   /* environments */
   tr7_t curenv;            /* the current environment */
#if COMMON_ROOT_ENV
   tr7_t null_env;          /* null environment */
   tr7_t base_env;          /* base environment */
#endif

   tr7_t c_nest;            /* stack for nested calls from C */
   tr7_t c_holds;           /* list of values held by external C */
   tr7_t libraries;         /* available libraries */

   /* parameter implementation of standard ports */
   tr7_t stdports[COUNT_IDX_STD];

   tr7_t loadport;
   tr7_t loadenv;
   unsigned playflags;

   const char *strings[__Tr7_StrID_Count__];
   tr7_t datums;        /* handling references when reading s-expr */
#if USE_TR7_DEBUG && DEBUG_LINES
   int last_line;       /* last readen line or start of line_starts */
   tr7_t line_starts;   /* a-list of starts of lines */
#endif
   struct {
      unsigned length;
      unsigned size;
      char *head;
      char buffer[STRBUFFSIZE];
   } strbuff;
};
/*
* definition of operation description
*/
typedef struct
{
   tr7_proc_t proc;      /* callback implementation function */
   int8_t min_arity;     /* minimum count of arguments */
   int8_t max_arity;     /* maximum count of arguments */
   uint16_t symbolid;    /* id of the symbol of the procedure */
   const char *argtypes; /* description of arguments' types */
}
   proc_desc_t;
/*
* definition a predefined libraries
*/
typedef struct
{
   const char *name;    /* name ex: (scheme eval) become "scheme/eval" */
   unsigned proc_last;  /* indice of the procedure just after the last of the library */
   unsigned syn_last;   /* indice of the syntax just after the last of the library */
}
   libdef_t;
/*
**************************************************************************
* SECTION DECLARATION - Predeclaration of functions
* -------------------
*
* forward declarations of some functions
*/
static int eval_syntax_rules_transform(cpl_t cpl, tr7_t transformer, tr7_t expr, tr7_t *result);
static int syntaxic_expansion(cpl_t cpl, tr7_t expr, tr7_t *result);
static int compile_expression_arg(cpl_t cpl, tr7_t args);
static int compile_expr(cpl_t cpl, tr7_t expr, int predeclared);
static int compile_expression(cpl_t cpl, tr7_t args);
static int compile_body(cpl_t cpl, tr7_t args);
static int compile_make_prog(cpl_t cpl, tr7_t *prog);
static void log_str(tr7_engine_t tsc, const char *string);
static void log_item(tr7_engine_t tsc, tr7_t item);

#if PURGE_SYMBOLS
static void purge_symbols(tr7_engine_t tsc);
#endif
static void finalize_buffer(tr7_engine_t tsc, tr7_cell_t a);
static void finalize_port(tr7_engine_t tsc, tr7_cell_t a);

static int port_write_utf8_length(tr7_engine_t tsc, port_t *pt, const char *s, unsigned len);
static tr7_t get_stdport(tr7_engine_t tsc, int num);
static void print_item(tr7_engine_t tsc, port_t *pt, unsigned pflags, tr7_t item, tr7_t anchors);
static int do_read_with_token(tr7_engine_t tsc, port_t *pt, int funq, token_type_t tok);
static int play(tr7_engine_t tsc);
#if USE_TR7_DEBUG && DEBUG_LINES
static int main_compile(tr7_engine_t tsc, tr7_t expr, tr7_t file, tr7_t linetrack);
static eval_status_t do_compile(tr7_engine_t tsc, tr7_t expr, tr7_t file, tr7_t linetrack);
#else
static int main_compile(tr7_engine_t tsc, tr7_t expr);
static eval_status_t do_compile(tr7_engine_t tsc, tr7_t expr);
#endif
static eval_status_t execute_call(tr7_engine_t tsc, tr7_t oper, int nargs);
static eval_status_t execute_program(tr7_engine_t tsc, tr7_t upperframes, int nargs, tr7_t lambda_desc);
static tr7_t *data_stack_enter_safe(tr7_engine_t tsc, unsigned count);

static void save_from_C_call(tr7_engine_t tsc);
static void restore_from_C_call(tr7_engine_t tsc);
static int import_importset(tr7_engine_t tsc,  tr7_t set, env_enum_cb_t import, void *closure);
/*
* predeclaration of functions for being referenced in below data
*/
#define _SYNTAX_(FUNC,NAME,CODE)                 static int FUNC(cpl_t, tr7_t);
#define _PROC___(FUNC,NAME,MIN,MAX,TYP,CODE)     static eval_status_t FUNC(tr7_engine_t, int);
#define ___OPER_(FUNC,CODE)                      static eval_status_t FUNC(tr7_engine_t);
#include __FILE__
/*
**************************************************************************
* SECTION DATA - Global variables
* ------------
*
* table of predefined symbols
*/
static alignas(TR7_ALIGNMENT) const struct tr7_buffer predefined_symbols[] = {

#define PRDFSYM(symbol) {\
            .head = TR7_MAKE_HEAD(sizeof(symbol)-1,Tr7_Head_Kind_Symbol), \
            .content = (uint8_t*)symbol }

#define _SYMBOL_(NAME,CODE)                   PRDFSYM(NAME),
#define _SYNTAX_(FUNC,NAME,CODE)              PRDFSYM(NAME),
#define _PROC___(FUNC,NAME,MIN,MAX,TYP,CODE)  PRDFSYM(NAME),
#include __FILE__

#undef PRDFSYM
};
/*
* macros for checking if a symbol is predefined or not
*/
#define CELL_IS_PRDFSYM(c) \
   ((0 <= ((const char*)(c) - (const char *)predefined_symbols))  \
    && ((((const char*)(c) - (const char *)predefined_symbols)) < sizeof predefined_symbols))
#define IS_PRDFSYM(t) CELL_IS_PRDFSYM(t) /* unboxing is not needed */
/*
* array for procedures
*/
static const proc_desc_t procedures[] = {
#define _PROC___(FUNC,NAME,MIN,MAX,TYP,CODE)      {FUNC,MIN,MAX,SYMBOLID(CODE),TYP},
#include __FILE__
};
/*
* array of syntax's callbacks
*/
#define _SYNTAX_(FUNC,NAME,CODE)                  FUNC,
static cplcb_t syncbs[] = {
#include __FILE__
};
/*
* array of syntax's symbol-ids
*/
#define _SYNTAX_(FUNC,NAME,CODE)                  SYMBOLID(CODE),
static unsigned synsymbs[] = {
#include __FILE__
};
/*
* array of operator's names
*/
#if SHOW_OPCODES
static const char *operator_names[] = {
#define ___OPER_(FUNC,CODE)       "##" #CODE "##",
#include __FILE__
};
#endif
/*
* fake enum counting the number of previous libraries
*/
enum lib_offset_enum {
#define _BEGIN_LIBRARY_(ID,NAME)                  _OFFSET_LIB_##ID,
#include __FILE__
};
/*
* fake enum for counting operators of libraries
*/
enum oper_lib_count_enum {
#define _PROC___(FUNC,NAME,MIN,MAX,TYP,CODE)      _COUNT_oper__##CODE##__,
#define _END_LIBRARY_(ID)                         _COUNT_oper_LIB_##ID,
#include __FILE__
};
/*
* fake enum for counting syntaxes of libraries
*/
enum syn_lib_count_enum {
#define _SYNTAX_(FUNC,NAME,CODE)                  _COUNT_syn__##CODE##__,
#define _END_LIBRARY_(ID)                         _COUNT_syn_LIB_##ID,
#include __FILE__
};
/*
* table of builtin libraries
*/
static const libdef_t builtin_libs[] = {

#define _LIBCNT_END_(TYPE,ID)     ((unsigned)(_COUNT_##TYPE##_LIB_##ID - _OFFSET_LIB_##ID))
#define _BEGIN_LIBRARY_(ID,NAME) { NAME, _LIBCNT_END_(oper,ID), _LIBCNT_END_(syn,ID) },
#include __FILE__
#undef _LIBCNT_END_
};
/*
* possible extensions when search a scheme file
*/
static const char *suffixes_scheme[] = { ".sld", ".scm", "/init.scm", "" };
/*
* constants for loadable libraries
*/
#if USE_TR7_EXTENSION
/*
* suffixes to try to add
*/
static const char *extensions_suffixes[] = {
#ifdef _WIN32
   ".dll"
#else
   ".so"
#endif
   , ""
};
/*
* name of the symbol to search
*/
static const char extensions_functions[] = "_tr7_C_functions_";
#endif
/*
**************************************************************************
* SECTION RECORD_DEFINITONS - Global static records
* -------------------------
*
* errors are records
*/
typedef enum
{
   Error_Idx_Message = Record_Idx_First, /* message of the error */
   Error_Idx_Irritants,                  /* irritants */
#if USE_TR7_DEBUG
   Error_Idx_Stack,                      /* call stack for debugging */
#endif
   _Error_Idx_Last_,                     /* hack for computing count of fields */
   Error_Count_Fields = _Error_Idx_Last_ - Error_Idx_Message
}
   error_index_t;
/*
* Predefined error record descriptor
*/
DECLARE_RECORD(error, ERROR, Error_Count_Fields)
   FIELD_RECORD_RO(ERRORMSG)    /* Error_Idx_Message */
   FIELD_RECORD_RO(ERRORIRRIT)  /* Error_Idx_Irritants */
#if USE_TR7_DEBUG
   FIELD_RECORD_VOID            /* Error_Idx_Stack */
#endif
END_RECORD
/*
* Predefined read-error record descriptor
*/
DECLARE_SUBRECORD(read_error, ERROR, 0, error, Error_Count_Fields)
END_RECORD
/*
* Predefined file-error record descriptor
*/
DECLARE_SUBRECORD(file_error, ERROR, 0, error, Error_Count_Fields)
END_RECORD
/*
* Predefined syntaxic-variable are records
*/
typedef enum
{
   Synvar_Idx_Name = Record_Idx_First, /* name of the variable */
   Synvar_Idx_Envit,                   /* its environment */
   _Synvar_Idx_Last_,                  /* hack for computing count of fields */
   Synvar_Count_Fields = _Synvar_Idx_Last_ - Synvar_Idx_Name
}
   synvar_index_t;
/*
* Predefined syntaxic-variable record descriptor
*/
DECLARE_RECORD(synvar, SYNVAR, Synvar_Count_Fields)
   FIELD_RECORD_VOID /* name */
   FIELD_RECORD_VOID /* envit*/
END_RECORD
/*
**************************************************************************
* SECTION UTILITIES
* -----------------
*
* scan a string made of records separated by 'seprec'
* each record has items separated by 'sepitm'.
* return the index of the record where the value is found or -1
*/
static int search_cstr_index(
      const char *value,
      const char *records,
      char sepitm,
      char seprec
) {
   int res = 0;
   const char *itval, *itrec = records;
   char cval, crec;
   for(;;) {
      itval = value;
      do { crec = *itrec++; cval = *itval++; } while (cval == crec && cval != 0);
      if (crec != 0 && crec != seprec && crec != sepitm)
         do { crec = *itrec++; } while (crec != 0 && crec != seprec && crec != sepitm);
      else if (cval == 0)
         return res;
      if (crec == 0)
         return -1;
      res += (crec == seprec);
   }
}
/*
* searchs the first char of the record of index 'idx'.
* records are separated using character 'seprec'.
* returns NULL when not found
*/
static const char *search_cstr(
      int idx,
      const char *records,
      char seprec,
      unsigned *length,
      char sepitm
) {
   const char *iter = records;
   for(iter = records ; idx > 0 ; idx--, iter++) {
      while(*iter != 0 && *iter != seprec)
         iter++;
      if (*iter == 0)
         return NULL;
   }
   if (length != NULL) {
      unsigned len = 0;
      while(iter[len] != 0 && iter[len] != sepitm && iter[len] != seprec)
         len++;
      *length = len;
   }
   return iter;
}
/*
**************************************************************************
* SECTION MEMORY - Memory management
* --------------
*
* allocate 'len' bytes
*/
static void *memalloc(tr7_engine_t tsc, size_t len)
{
   return tsc->malloc(len);
}
/*
* allocates 'alen' bytes and copy to it 'clen' first bytes of 'buf' (alen >= clen)
*/
static void *memalloc_copy_stringz(tr7_engine_t tsc, const void *buf, size_t len)
{
   void *mem = memalloc(tsc, len + 1);
   if (mem) {
      memcpy(mem, buf, len);
      ((char*)mem)[len] = 0;
   }
   return mem;
}
/*
* allocates 'len' bytes and copy to it len bytes of 'buf'
*/
static void *memalloc_copy(tr7_engine_t tsc, const void *buf, size_t len)
{
   void *mem = memalloc(tsc, len);
   if (mem)
      memcpy(mem, buf, len);
   return mem;
}
/*
* allocates 'len' bytes and fill it with byte
*/
static void *memalloc_fill(tr7_engine_t tsc, uint8_t fill, size_t len)
{
   void *mem = memalloc(tsc, len);
   if (mem)
      memset(mem, fill, len);
   return mem;
}
/*
* free the memory pointed by 'ptr'
*/
static void memfree(tr7_engine_t tsc, void *ptr)
{
   tsc->free(ptr);
}
/*
*  queue in free list 'count' successive cells starting at 'head'
*/
static unsigned freecells_queue(tr7_engine_t tsc, tr7_cell_t head, unsigned count)
{
   /* turns/casts head has freecell item */
   freecells_t *fc = (freecells_t*)head;

   /* just ignore too small blocks */
   if (count < FREECELL_MIN_SUCC)
      return 0;

   /* init freecell */
   fc->next = NULL;
   fc->count = count;

   /* add at tail of frees' list */
   if (tsc->freestail)
      tsc->freestail->next = fc;
   else
      tsc->freeshead = fc;
   tsc->freestail = fc;

#if STRESS_GC_RESILIENCE
#define FREE_MEMORY_MARK ((tr7_head_t)-1)
   /* if resilience is required, fill freed slots with marker */
   {
      tr7_cell_t it = (tr7_cell_t)(fc + 1);
      tr7_cell_t end = &head[count];
      for (; it != end ; it++)
         TR7_CELL_HEAD(it) = FREE_MEMORY_MARK;
   }
#undef FREE_MEMORY_MARK
#endif
   return count;
}
/*
* dequeue the head of free list
*/
static void freecells_dequeue(tr7_engine_t tsc)
{
   freecells_t *fc = tsc->freeshead;
   if (fc == NULL)
      /* no free slot left */
      tsc->nsuccfrees = 0;
   else {
      /* set current frees */
      tsc->nsuccfrees = fc->count;
      tsc->firstfree = (tr7_cell_t)fc;
      /* update free list */
      tsc->freeshead = fc->next;
      if (fc->next == NULL)
         tsc->freestail = NULL;
   }
}
/*
* searchs the memseg containing the cell 'c', return it or 0 when not found
* dichotomic search.
*/
static memseg_t *search_memseg(tr7_engine_t tsc, tr7_cell_t c)
{
   unsigned low = 0, up = tsc->nmemseg;
   while (low < up) {
      unsigned iseg = (low + up) >> 1;
      memseg_t *ms = tsc->memsegs[iseg];
      if ((void*)ms->cells > (void*)c)
         up = iseg;
      else if ((void*)c >= (void*)ms->flags)
         low = iseg + 1;
      else
         return ms;
   }
   return NULL; /* not found */
}
/*
* mark as used the 'count' successives cells starting at 'icell'
* returns 1 if already marked
*/
static int markmemseg(memseg_t *ms, unsigned icell, unsigned count)
{
   unsigned imark = (icell >> 5) << 1;
   unsigned ibit  = icell & 31;
   uint32_t bits  = ((uint32_t) 1) << ibit;

   if (ms->flags[imark] & bits)
      return 1;

#if INTPTR_MAX == INT32_MAX
   count += count & 1; /* ensure oddity */
#endif

   if (ibit) {
      unsigned nbits = 32 - ibit;
      if (nbits >= count) {
         bits = (bits << count) - bits;
         ms->flags[imark] |= bits;
         return 0;
      }
      ms->flags[imark] |= -bits;
      imark += 2;
      count -= nbits;
   }
   while(count >= 32) {
      ms->flags[imark] = UINT32_MAX;
      imark += 2;
      count -= 32;
   }
   if (count) {
      bits = (((uint32_t) 1) << count) - 1;
      ms->flags[imark] |= bits;
   }
   return 0;
}
/*
* mark as used the 'count' successives cells starting at 'head'
* returns 1 if already marked
*/
static int setmarked(tr7_engine_t tsc, tr7_cell_t head, unsigned count)
{
   memseg_t *ms = search_memseg(tsc, head);
   return ms ? markmemseg(ms, (unsigned)(head - ms->cells), count) : 0;
}
#if PURGE_SYMBOLS
/*
* test if the 'cell' is garbage
* returns 1 if garbage (not marked by marking stage)
* returns 0 if marked or outside of managed memory
*/
static int isgarbage(tr7_engine_t tsc, tr7_cell_t cell)
{
   memseg_t *ms = search_memseg(tsc, cell);
   if (ms) {
      unsigned icell = (unsigned)(cell - ms->cells);
      unsigned imark = (icell >> 5) << 1;
      unsigned ibit  = icell & 31;
      uint32_t bits  = ((uint32_t) 1) << ibit;

      if ((ms->flags[imark] & bits) == 0)
         return 1;
   }
   return 0;
}
#endif
#if STACKED_GC
/*
* This GC mark algorithm use the program stack
*/
void tr7_mark(tr7_engine_t tsc, tr7_t current)
{
   tr7_cell_t cell;
   tr7_pair_t pair;
   unsigned extend; /* full extend in count of cell of a cell */
   unsigned exmark; /* extend of the cell after head to be marked */
   unsigned idx;

next:
   /* special NIL case */
   if (TR7_IS_VOID(current))
      return;

   /* inspect what to mark */
   switch (TR7_TAG(current)) {
   case TR7_TAG_PAIR:
      goto mark_pair;

   case TR7_TAG_CELL:
      goto mark_cell;

   case TR7_TAG_DOUBLE:
      setmarked(tsc, (tr7_cell_t)TR7_TO_DOUBLE(current), NCELL_OF_TYPE(double));
      return;

   default:
      return;
   }

mark_pair:
   /* mark the pair */
   pair = TR7_TO_PAIR(current);
   if (setmarked(tsc, (tr7_cell_t) pair, NCELL_OF_PTR(pair)))
      return;

   tr7_mark(tsc, TR7_PAIR_CAR(pair));
   current = TR7_PAIR_CDR(pair);
   goto next;

mark_cell:
   /* mark a cell */
   cell = TR7_TO_CELL(current);
   switch (TR7_CELL_KIND(cell)) {
   case Tr7_Head_Kind_Rational:
   case Tr7_Head_Kind_Complex:
   default:
      /* should not happen! */
      /* ns of 0 */
      return;

   case Tr7_Head_Kind_Port:
      if (TR7_CELL_PORT__PORT_(cell)->flags & (port_string | port_bytevector))
         tr7_mark(tsc, TR7_CELL_PORT__PORT_(cell)->rep.inmem.item);
      else if (TR7_CELL_PORT__PORT_(cell)->flags & port_file)
         tr7_mark(tsc, TR7_CELL_PORT__PORT_(cell)->rep.stdio.filename);
      /*@fallthrough@*/

   case Tr7_Head_Kind_String:
   case Tr7_Head_Kind_Symbol:
   case Tr7_Head_Kind_Byte_Vector:
   case Tr7_Head_Kind_CFunction:
      exmark = 0;
      extend = 2;
      goto mark_cell_next;

   case Tr7_Head_Kind_CPointer:
      if (TR7_CELL_TO_CPTR(cell)->vtable && TR7_CELL_TO_CPTR(cell)->vtable->marker)
         TR7_CELL_TO_CPTR(cell)->vtable->marker(tsc, TR7_CELL_TO_CPTR(cell)->value);
      exmark = 0;
      extend = NCELL_OF_TYPE(struct tr7_cptr);
      goto mark_cell_next;

   case Tr7_Head_Kind_Continuation:
      exmark = 2 + TR7_HEAD_VALUE(TR7_CELL_HEAD(cell));
      break;

#if USE_SCHEME_LAZY
   case Tr7_Head_Kind_Promise:
      exmark = 1;
      break;
#endif

   case Tr7_Head_Kind_Parameter:
   case Tr7_Head_Kind_Lambda:
#if USE_SCHEME_CASE_LAMBDA
   case Tr7_Head_Kind_Case_Lambda:
#endif
      exmark = 2;
      break;

   case Tr7_Head_Kind_Transform:
      exmark = 4;
      break;

   case Tr7_Head_Kind_Environment:
      exmark = 1 + TR7_HEAD_VALUE(TR7_CELL_HEAD(cell));
      break;

   case Tr7_Head_Kind_Box:
   case Tr7_Head_Kind_Record:
   case Tr7_Head_Kind_Vector:
   case Tr7_Head_Kind_RecFun:
      exmark = TR7_CELL_VECTOR_LENGTH(cell);
      break;

   case Tr7_Head_Kind_Program:
      extend = TR7_CELL_VECTOR_LENGTH(cell);
      exmark = TR7_TO_UINT(TR7_CELL_VECTOR_ITEM(cell, Program_Idx_Code));
      goto mark_cell_next;
   }
   extend = 1 + exmark;

mark_cell_next:
   if (setmarked(tsc, cell, extend))
      return;
   if (exmark == 0)
      return;
   for(idx = 1 ; idx < exmark ; idx++)
      tr7_mark(tsc, ((tr7_vector_t)cell)->items[idx]);
   current = ((tr7_vector_t)cell)->items[0];
   goto next;
}
#else
/*
*  We use algorithm E (Knuth, The Art of Computer Programming Vol.1,
*  sec. 2.3.5), the Schorr-Deutsch-Waite link-inversion algorithm,
*  for marking.
*  Note that for cells other than vector, the vector structure is used
*  to access the items of the cell and flag is used to record where
*  backchaining is done.
*/
void tr7_mark(tr7_engine_t tsc, tr7_t o)
{
   tr7_cell_t cell;
   tr7_pair_t pair;
   tr7_t current;
   tr7_t upper;
   tr7_t tmp;
   unsigned extend; /* full extend in count of cell of a cell */
   unsigned exmark; /* extend of the cell after head to be marked */

   current = o;
   upper = TR7_NIL;

mark_a:
   /* special NIL case */
   if (TR7_IS_VOID(current))
      goto mark_next;

   /* inspect what to mark */
   switch (TR7_TAG(current)) {
   case TR7_TAG_PAIR:
      goto mark_pair;

   case TR7_TAG_CELL:
      goto mark_cell;

   case TR7_TAG_DOUBLE:
      setmarked(tsc, (tr7_cell_t)TR7_TO_DOUBLE(current), NCELL_OF_TYPE(double));
      goto mark_next;

   default:
      goto mark_next;
   }

mark_pair:
   /* mark the pair */
   pair = TR7_TO_PAIR(current);
   if (setmarked(tsc, (tr7_cell_t) pair, NCELL_OF_PTR(pair)))
      goto mark_next;

   /* down car */
   tmp = TR7_PAIR_CAR(pair);
   TR7_PAIR_CAR(pair) = upper;
   upper = I2TR7(TR72I(current) ^ TR7_MASK_PTR);  /* remove the PTR mark to tell it's CAR */
   current = tmp;
   goto mark_a;

mark_cell:
   /* mark a cell */
   cell = TR7_TO_CELL(current);
   switch (TR7_CELL_KIND(cell)) {
   case Tr7_Head_Kind_Rational:
   case Tr7_Head_Kind_Complex:
   default:
      /* should not happen! */
      /* ns of 0 */
      goto mark_next;

   case Tr7_Head_Kind_Port:
      if (TR7_CELL_PORT__PORT_(cell)->flags & (port_string | port_bytevector))
         tr7_mark(tsc, TR7_CELL_PORT__PORT_(cell)->rep.inmem.item);
      else if (TR7_CELL_PORT__PORT_(cell)->flags & port_file)
         tr7_mark(tsc, TR7_CELL_PORT__PORT_(cell)->rep.stdio.filename);
      /*@fallthrough@*/

   case Tr7_Head_Kind_String:
   case Tr7_Head_Kind_Symbol:
   case Tr7_Head_Kind_Byte_Vector:
   case Tr7_Head_Kind_CFunction:
      exmark = 0;
      extend = 2;
      goto mark_cell_next;

   case Tr7_Head_Kind_CPointer:
      if (TR7_CELL_TO_CPTR(cell)->vtable && TR7_CELL_TO_CPTR(cell)->vtable)
         TR7_CELL_TO_CPTR(cell)->vtable->marker(tsc, TR7_CELL_TO_CPTR(cell)->value);
      exmark = 0;
      extend = NCELL_OF_TYPE(struct tr7_cptr);
      goto mark_cell_next;

   case Tr7_Head_Kind_Continuation:
      exmark = 2 + TR7_HEAD_VALUE(TR7_CELL_HEAD(cell));
      break;

#if USE_SCHEME_LAZY
   case Tr7_Head_Kind_Promise:
      exmark = 1;
      break;
#endif

   case Tr7_Head_Kind_Parameter:
   case Tr7_Head_Kind_Lambda:
#if USE_SCHEME_CASE_LAMBDA
   case Tr7_Head_Kind_Case_Lambda:
#endif
      exmark = 2;
      break;

   case Tr7_Head_Kind_Transform:
      exmark = 4;
      break;

   case Tr7_Head_Kind_Environment:
      exmark = 1 + TR7_HEAD_VALUE(TR7_CELL_HEAD(cell));
      break;

   case Tr7_Head_Kind_Box:
   case Tr7_Head_Kind_Record:
   case Tr7_Head_Kind_Vector:
   case Tr7_Head_Kind_RecFun:
      exmark = TR7_CELL_VECTOR_LENGTH(cell);
      break;

   case Tr7_Head_Kind_Program:
      extend = TR7_CELL_VECTOR_LENGTH(cell);
      exmark = TR7_TO_UINT(TR7_CELL_VECTOR_ITEM(cell, Program_Idx_Code));
      goto mark_cell_next;
   }
   extend = 1 + exmark;

mark_cell_next:
   if (!setmarked(tsc, cell, extend)) {
      while (exmark > 0)
         tr7_mark(tsc, ((tr7_vector_t)cell)->items[--exmark]);
   }

mark_next:                    /* up.  Undo the link switching from steps E4 and E5. */
   if (TR7_IS_NIL(upper))
      return; /* terminated */

   if (!TR7_IS_PTR(upper)) {
      /* restore car and down cdr */
      upper = I2TR7(TR72I(upper) ^ TR7_MASK_PTR); /* restore the PTR mark */
      pair = TR7_TO_PAIR(upper);
      /* stored upper moves from car to cdr while current is set to cdr */
      tmp = TR7_PAIR_CAR(pair);
      TR7_PAIR_CAR(pair) = current;
      current = TR7_PAIR_CDR(pair);
      TR7_PAIR_CDR(pair) = tmp;
      /* mark the cdr */
      goto mark_a;
   }

   pair = TR7_TO_PAIR(upper);
   /* restore cdr */
   tmp = TR7_PAIR_CDR(pair);
   TR7_PAIR_CDR(pair) = current;
   /* move upper */
   current = upper;
   upper = tmp;
   /* upper continue to mark */
   goto mark_next;
}
#endif
/*
*
*/
static void gc_prepare(tr7_engine_t tsc)
{
   if (tsc->gc_verbose)
      log_str(tsc, "gc...");
}
/*
*
*/
static void gc_collect(tr7_engine_t tsc)
{
   memseg_t *mseg;
   tr7_cell_t itcells, headfreesucc;
   unsigned idxseg, idxcell, idxflags, nfreeseg, nsuccfree;
   uint32_t maskmark, maskcell, maskfinal;
   size_t prev;

   /* garbage collect */
   prev = tsc->free_cells;
   tsc->free_cells = 0;
   /* free-list is kept sorted by address so as to maintain consecutive
      ranges, if possible, for use with vectors. Here we scan the cells
      (which are also kept sorted by address) to build the free-list.  */
   tsc->freeshead = tsc->freestail = NULL;
   /* iterate over the segments */
   idxseg = tsc->nmemseg;
   while (idxseg > 0) {
      mseg = tsc->memsegs[--idxseg];
      itcells = headfreesucc = mseg->cells;
      nsuccfree = nfreeseg = 0;
      /* iterates in order over cells of the segment */
      for (idxcell = idxflags = 0 ; idxcell < mseg->count ; idxflags += 2) {
         maskmark = mseg->flags[idxflags];
         mseg->flags[idxflags] = 0;/*reset the mark now*/
         if (!~maskmark) {/* fully hmarked */
            if (nsuccfree)
               nfreeseg += freecells_queue(tsc, headfreesucc, nsuccfree);
            nsuccfree = 0;
            idxcell += 32;
            itcells += 32;
         }
         else {
            maskfinal = mseg->flags[idxflags + 1];
            if (!(maskmark | maskfinal)) {/*nomark, no finalizer*/
               if (!nsuccfree)
                  headfreesucc = itcells;
               nsuccfree += 32;
               itcells += 32;
               idxcell += 32;
            }
            else {
               for (maskcell = 1 ; maskcell && idxcell < mseg->count ; idxcell++, maskcell <<= 1, itcells++) {
                  if (maskcell & maskmark) {/*marked cell*/
                     if (nsuccfree)
                        nfreeseg += freecells_queue(tsc, headfreesucc, nsuccfree);
                     nsuccfree = 0;
                  }
                  else {/*not marked cell*/
                     if (!nsuccfree++)
                        headfreesucc = itcells;
                     if (maskfinal & maskcell) {/*finalizer*/
                        maskfinal ^= maskcell;/*reset finalizer*/
                        /* add cases here for finalizing */
                        switch (TR7_CELL_KIND(itcells)) {
                        case Tr7_Head_Kind_String:
                        case Tr7_Head_Kind_Symbol:
                        case Tr7_Head_Kind_Byte_Vector:
                           finalize_buffer(tsc, itcells);
                           break;
                        case Tr7_Head_Kind_Port:
                           finalize_port(tsc, itcells);
                           break;
                        case Tr7_Head_Kind_CPointer:
                           if (TR7_CELL_TO_CPTR(itcells)->vtable && TR7_CELL_TO_CPTR(itcells)->vtable->disposer)
                              TR7_CELL_TO_CPTR(itcells)->vtable->disposer(tsc, TR7_CELL_TO_CPTR(itcells)->value);
                        default:
                           break;
                        }
                     }
                  }
               }
               /* update final flags */
               mseg->flags[idxflags + 1] = maskfinal;
            }
         }
      }
      if (nsuccfree)
         nfreeseg += freecells_queue(tsc, headfreesucc, nsuccfree);
      tsc->free_cells += nfreeseg;
   }
   freecells_dequeue(tsc);

   /* log if required */
   if (tsc->gc_verbose) {
#if 0
      char msg[80];
      snprintf(msg, sizeof msg, "done: %ld items were recovered (%ld).\n", (tsc->free_cells - prev), (long) ((sizeof(union tr7_cell)) * (tsc->free_cells - prev))
          );
#else
      long count = ITEM_SEGSIZE;
      char msg[120];
      if (count & 31) count += 32 - (count & 31);
      snprintf(msg, sizeof msg, "done: %ld items were recovered (%ld), %ld free now (%ld/%ld).\n",
               (long) (tsc->free_cells - prev),
               (long) ((sizeof(union tr7_cell)) * (tsc->free_cells - prev)),
               (long) tsc->free_cells,
               (long) (tsc->free_cells * (long) sizeof(union tr7_cell)),
               (long) (tsc->nmemseg * (long) sizeof(union tr7_cell) * count)
          );
#endif
      log_str(tsc, msg);
   }
}
/*
* allocate 'n' new memory segments
*/
static unsigned memseg_multi_alloc(tr7_engine_t tsc, unsigned n, unsigned count)
{
   size_t nru32, size, szflags;
   unsigned i, k;

   /* adjust count to align of 32 */
   if (count & 31)
      count += 32 - (count & 31);

   /* compute sizes */
   nru32 = count >> 5;   /* count / 32) */
   szflags = nru32 << (2 + 1);    /* 2 * nru32 * sizeof(uint32_t) */
   size = sizeof(memseg_t) + szflags + count * sizeof(union tr7_cell);

   /* iterated allocation */
   for (k = 0; tsc->nmemseg < ITEM_NSEGMENT && k < n; k++) {
      /* allocation */
      memseg_t *ms = memalloc(tsc, size);
      if (ms == NULL)
         break;

      /* init fields */
      ms->count = count;
      ms->flags = (uint32_t *)(&ms->cells[count]);
      memset(ms->flags, 0, szflags);

      /* keep segments sorted */
      i = tsc->nmemseg++;
#if ITEM_NSEGMENT > 1 /* avoid a spurious warning of the compiler */
      while (i > 0 && tsc->memsegs[i - 1] > ms) {
         tsc->memsegs[i] = tsc->memsegs[i - 1];
         i = i - 1;
      }
#endif
      tsc->memsegs[i] = ms;

      /* make it available */
      tsc->free_cells += ms->count;
      freecells_queue(tsc, ms->cells, ms->count);
   }
   return k;
}
/*
*
*/
static void set_final_flag(tr7_engine_t tsc, tr7_cell_t cell)
{
   memseg_t *ms = search_memseg(tsc, cell);
   unsigned i = (unsigned) (cell - ms->cells);
   ms->flags[((i >> 5) << 1) + 1] |= ((uint32_t) 1) << (i & 31);
}
/*
* find 'n' consecutive cells and set its head final flag if 'final' is not nul
* returns the head found or 0
*/
static tr7_cell_t find_consecutive_cells(tr7_engine_t tsc, unsigned n, int final)
{
   tr7_cell_t res;
   freecells_t **pfc, *fc;
   unsigned rnf; /* remaining number of successive free cells */

   if (tsc->nsuccfrees >= n) {
      res = tsc->firstfree;
      rnf = tsc->nsuccfrees - n;
   }
   else {
      pfc = &tsc->freeshead;
      while ((fc = *pfc) && fc->count < n)
         pfc = &fc->next;
      if (!fc)
         return NULL;
      freecells_queue(tsc, tsc->firstfree, tsc->nsuccfrees);
      rnf = (unsigned)fc->count - n;
      *pfc = fc->next;
      res = (tr7_cell_t)fc;
   }
   if (rnf < FREECELL_MIN_SUCC)
      freecells_dequeue(tsc);
   else {
      tsc->nsuccfrees = rnf;
      tsc->firstfree = &res[n];
   }
   tsc->free_cells -= n;
   if (final)
      set_final_flag(tsc, res);
   return res;
}
/*
* garbage collection. parameter a is marked.
*/
static void collect_garbage(tr7_engine_t tsc)
{
   unsigned i;
   tr7_t *it, *end;

   /* prepare marking */
   gc_prepare(tsc);

   /* mark system globals */
   tr7_mark(tsc, tsc->curenv);
#if COMMON_ROOT_ENV
   tr7_mark(tsc, tsc->null_env);
   tr7_mark(tsc, tsc->base_env);
#endif
   tr7_mark(tsc, tsc->libraries);

   /* mark current registers */
   for (i = 0 ; i < tsc->nvalues ; i++)
      tr7_mark(tsc, tsc->values[i]);
   tr7_mark(tsc, tsc->read_value);
   tr7_mark(tsc, tsc->stof_guards);
   tr7_mark(tsc, tsc->stof_params);
   tr7_mark(tsc, tsc->stof_dynawinds);
   tr7_mark(tsc, tsc->loadport);
   tr7_mark(tsc, tsc->loadenv);
   tr7_mark(tsc, tsc->datums);
#if USE_TR7_DEBUG && DEBUG_LINES
   tr7_mark(tsc, tsc->read_file);
   tr7_mark(tsc, tsc->read_lines);
   tr7_mark(tsc, tsc->line_starts);
#endif
   for (i = 0; i < (unsigned)(sizeof tsc->stdports / sizeof *tsc->stdports); i++)
      tr7_mark(tsc, tsc->stdports[i]);

   /* mark the stack */
   if (tsc->stack.head != NULL) {
      setmarked(tsc, (tr7_cell_t)tsc->stack.head,
                (unsigned)(tsc->stack.tail - tsc->stack.head));
      for (it = tsc->stack.data, end = tsc->stack.tail ; it != end ; it++)
         tr7_mark(tsc, *it);
      for (it = tsc->stack.head, end = tsc->stack.oper ; it != end ; it++)
         tr7_mark(tsc, *it);
   }

   /* Mark recent objects the interpreter doesn't know about yet. */
   for (i = tsc->recent_count ; i ;)
      tr7_mark(tsc, tsc->recents[--i]);

   /* Mark any older stuff above nested C calls */
   tr7_mark(tsc, tsc->c_nest);
   tr7_mark(tsc, tsc->c_holds);

   /* purge what can be purged */
#if PURGE_SYMBOLS
   purge_symbols(tsc);
#endif

   /* final mark after purge */
   tr7_mark(tsc, tsc->symbols_set);

   /* garbage collect */
   gc_collect(tsc);
}
/*
* Allocation of n consecutive items in the heap of 'tsc'.
* Returns a pointer to the first item or NULL on memory depletion.
* The boolean 'final' tells to set of not the finalize bit for the
* allocated bloc.
*/
static void *get_cells(tr7_engine_t tsc, unsigned n, int final)
{
   tr7_cell_t x;

#if STRESS_GC_RESILIENCE
   if (tsc->gc_resilience)
      collect_garbage(tsc);
#endif

#if INTPTR_MAX == INT32_MAX
   n += n & 1; /* ensure oddity */
#endif

   /* Are there any cells available? */
   if (n < tsc->free_cells) {
      x = find_consecutive_cells(tsc, n, final);
      if (x != NULL)
         return x;
   }

   /* If not, try gc'ing some */
   collect_garbage(tsc);
   x = find_consecutive_cells(tsc, n, final);
   if (x != NULL)
      return x;

   /* If there still aren't, try getting more heap */
   if (memseg_multi_alloc(tsc, 1, n > ITEM_SEGSIZE ? n : ITEM_SEGSIZE)) {
      /* got more place */
      x = find_consecutive_cells(tsc, n, final);
      if (x != NULL)
         return x;
   }

   /* If all fail, report failure */
   tsc->no_memory = tsc->free_cells < NOMEM_LEVEL;
   return NULL;
}
/*
**************************************************************************
* SECTION RECENTS - Holding of recent allocations
* ---------------
*
* To retain recent allocs before interpreter knows about them - Tehom
*/
static void ok_to_freely_gc(tr7_engine_t tsc)
{
   tsc->recent_count = 0;
}
/*
* returns an array holding the recent allocations
*/
static tr7_t wrap_recent_allocs(tr7_engine_t tsc)
{
   unsigned i, nrec = tsc->recent_count;
   if (nrec) {
      tr7_vector_t vec = get_cells(tsc, 1 + nrec, 0);
      if (vec != NULL) {
         TR7_CELL_HEAD(vec) = TR7_MAKE_HEAD(nrec, Tr7_Head_Kind_Vector);
         for(i = 0 ; i < nrec ; i++)
            vec->items[i] = tsc->recents[i];
         return TR7_FROM_VECTOR(vec);
      }
   }
   return TR7_FALSE;
}
/*
* add 'last' to the list of recently allocated values and return it
*/
static tr7_t push_recent_alloc(tr7_engine_t tsc, tr7_t last)
{
   if (!tsc->no_recent) {
      unsigned i = tsc->recent_count++;
      tsc->recents[i] = last;
      if (tsc->recent_count == NRECENTS) {
         //log_str(tsc, "RECENT OVERFLOW!!\n");
         tr7_t repl = wrap_recent_allocs(tsc);
         if (!TR7_IS_FALSE(repl)) {
            tsc->recents[0] = repl;
            tsc->recent_count = 1;
         }
         else {
            /* older values are maybe already accessible to GC */
            for(i = 0 ; i < NRECENTS - 1 ; i++)
               tsc->recents[i] = tsc->recents[i + 1];
            tsc->recent_count = NRECENTS - 1;
         }
      }
   }
   return last;
}
/*
* add any cell pointer 'last' to the list of recently allocated values
* returns its tr7_t value
*/
static tr7_t push_recent_cell(tr7_engine_t tsc, void *last)
{
   return push_recent_alloc(tsc, TR7_FROM_CELL(last));
}
/*
**************************************************************************
* SECTION PAIRS - Management of pairs
* -------------
*
* creation of count pairs linked together with the given cars and the given cdr
* returns the head or TR7_NIL on failure
*/
tr7_t tr7_cons_n(tr7_engine_t tsc, int count, tr7_t cars[], tr7_t cdr)
{
   tr7_t result;
   tr7_pair_t pairs;
   unsigned npair, ncell;

   /* trivial empty case */
   if (count <= 0)
      return cdr;

   /* try to get the expected count at once */
   ncell = (unsigned)count * NCELL_OF_TYPE(struct tr7_pair);
   if (ncell <= tsc->nsuccfrees) {
oneblock:
      pairs = get_cells(tsc, ncell, 0);
      result = TR7_FROM_PAIR(pairs);
      while (--count) {
         TR7_PAIR_CAR(pairs) = *cars++;
         TR7_PAIR_CDR(pairs) = TR7_FROM_PAIR(&pairs[1]);
         pairs++;
      }
      TR7_PAIR_CAR(pairs) = *cars;
      TR7_PAIR_CDR(pairs) = cdr;
      return push_recent_alloc(tsc, result);
   }
   if (!tsc->nsuccfrees && !tsc->no_memory) {
      collect_garbage(tsc);
      if (tsc->free_cells < NCELL_OF_TYPE(struct tr7_pair))
         memseg_multi_alloc(tsc, 1, ITEM_SEGSIZE);
      if (ncell <= tsc->nsuccfrees)
         goto oneblock;
   }
   npair = tsc->nsuccfrees / NCELL_OF_TYPE(struct tr7_pair);
   npair = npair ? npair : 1;
   result = tr7_cons_n(tsc, (int)npair, &cars[(unsigned)count - npair], cdr);
   return TR7_IS_NIL(result) ? result : tr7_cons_n(tsc, count - (int)npair, cars, result);
}
/*
* get new pair with the given car and cdr
* returns TR7_NIL on failure
*/
tr7_t tr7_cons(tr7_engine_t tsc, tr7_t car, tr7_t cdr)
{
   tr7_pair_t pair = get_cells(tsc, NCELL_OF_TYPE(struct tr7_pair), 0);
   if (!pair)
      return TR7_NIL;
   TR7_PAIR_CAR(pair) = car;
   TR7_PAIR_CDR(pair) = cdr;
   return push_recent_alloc(tsc, TR7_FROM_PAIR(pair));
}
/*
* CxR helpers of level 1
*/
tr7_t tr7_car_or_void(tr7_t t) { return TR7_IS_PAIR(t) ? TR7_CAR(t) : TR7_VOID; }
tr7_t tr7_cdr_or_void(tr7_t t) { return TR7_IS_PAIR(t) ? TR7_CDR(t) : TR7_VOID; }
/*
* CxR helpers of level 2
*/
tr7_t tr7_caar_or_void(tr7_t t) { return tr7_car_or_void(tr7_car_or_void(t)); }
tr7_t tr7_cadr_or_void(tr7_t t) { return tr7_car_or_void(tr7_cdr_or_void(t)); }
tr7_t tr7_cdar_or_void(tr7_t t) { return tr7_cdr_or_void(tr7_car_or_void(t)); }
tr7_t tr7_cddr_or_void(tr7_t t) { return tr7_cdr_or_void(tr7_cdr_or_void(t)); }
/*
* CxR helpers of level 3
*/
tr7_t tr7_caaar_or_void(tr7_t t) { return tr7_car_or_void(tr7_caar_or_void(t)); }
tr7_t tr7_caadr_or_void(tr7_t t) { return tr7_car_or_void(tr7_cadr_or_void(t)); }
tr7_t tr7_cadar_or_void(tr7_t t) { return tr7_car_or_void(tr7_cdar_or_void(t)); }
tr7_t tr7_caddr_or_void(tr7_t t) { return tr7_car_or_void(tr7_cddr_or_void(t)); }
tr7_t tr7_cdaar_or_void(tr7_t t) { return tr7_cdr_or_void(tr7_caar_or_void(t)); }
tr7_t tr7_cdadr_or_void(tr7_t t) { return tr7_cdr_or_void(tr7_cadr_or_void(t)); }
tr7_t tr7_cddar_or_void(tr7_t t) { return tr7_cdr_or_void(tr7_cdar_or_void(t)); }
tr7_t tr7_cdddr_or_void(tr7_t t) { return tr7_cdr_or_void(tr7_cddr_or_void(t)); }
/*
* CxR helpers of level 4
*/
tr7_t tr7_caaaar_or_void(tr7_t t) { return tr7_car_or_void(tr7_caaar_or_void(t)); }
tr7_t tr7_caaadr_or_void(tr7_t t) { return tr7_car_or_void(tr7_caadr_or_void(t)); }
tr7_t tr7_caadar_or_void(tr7_t t) { return tr7_car_or_void(tr7_cadar_or_void(t)); }
tr7_t tr7_caaddr_or_void(tr7_t t) { return tr7_car_or_void(tr7_caddr_or_void(t)); }
tr7_t tr7_cadaar_or_void(tr7_t t) { return tr7_car_or_void(tr7_cdaar_or_void(t)); }
tr7_t tr7_cadadr_or_void(tr7_t t) { return tr7_car_or_void(tr7_cdadr_or_void(t)); }
tr7_t tr7_caddar_or_void(tr7_t t) { return tr7_car_or_void(tr7_cddar_or_void(t)); }
tr7_t tr7_cadddr_or_void(tr7_t t) { return tr7_car_or_void(tr7_cdddr_or_void(t)); }
tr7_t tr7_cdaaar_or_void(tr7_t t) { return tr7_cdr_or_void(tr7_caaar_or_void(t)); }
tr7_t tr7_cdaadr_or_void(tr7_t t) { return tr7_cdr_or_void(tr7_caadr_or_void(t)); }
tr7_t tr7_cdadar_or_void(tr7_t t) { return tr7_cdr_or_void(tr7_cadar_or_void(t)); }
tr7_t tr7_cdaddr_or_void(tr7_t t) { return tr7_cdr_or_void(tr7_caddr_or_void(t)); }
tr7_t tr7_cddaar_or_void(tr7_t t) { return tr7_cdr_or_void(tr7_cdaar_or_void(t)); }
tr7_t tr7_cddadr_or_void(tr7_t t) { return tr7_cdr_or_void(tr7_cdadr_or_void(t)); }
tr7_t tr7_cdddar_or_void(tr7_t t) { return tr7_cdr_or_void(tr7_cddar_or_void(t)); }
tr7_t tr7_cddddr_or_void(tr7_t t) { return tr7_cdr_or_void(tr7_cdddr_or_void(t)); }
/*
**************************************************************************
* SECTION LISTS - Management of lists
* -------------
*
* Compute the length of the proper list 'a'
*/
int tr7_unsafe_list_length(tr7_t a)
{
   int r = 0;
   while(TR7_IS_PAIR(a)) {
      r++;
      a = TR7_CDR(a);
   }
   return TR7_IS_NIL(a) ? r : -1 - r;
}
/*
* Compute the length of the list 'a'
* Result is:
*  proper list: length
*  circular list: -1
*  not even a pair: -2
*  dotted list: -2 minus length before dot
*/
int tr7_list_length(tr7_t a)
{
   int i = 0;
   tr7_t slow = a, fast = a;
   for (;;) {
      if (TR7_IS_NIL(fast))
         return i;
      if (!TR7_IS_PAIR(fast))
         return -2 - i;
      fast = TR7_CDR(fast);
      if ((++i & 1) == 0) {
         slow = TR7_CDR(slow);
         if (TR7EQ(fast, slow))
            return -1; /* circular */
      }
   }
}
/*
* Copy 'a' that MUST be a pair.
* Result in head+tail and return 1 if ok or 0 if improper list
*/
static int list_copy(tr7_engine_t tsc, tr7_t a, tr7_t *head, tr7_t *tail)
{
   int i = 0;
   tr7_t cdr, first = TR7_NIL;
   tr7_pair_t slow, iter, copy, prev = NULL;

   slow = iter = TR7_TO_PAIR(a);
   for (;;) {
      /* copy if possible */
      copy = GET_CELLS(tsc, copy, 0);
      if (copy == NULL)
         break;
      TR7_PAIR_CAR(copy) = TR7_PAIR_CAR(iter);
      if (prev != NULL)
         TR7_PAIR_CDR(prev) =  TR7_FROM_PAIR(copy);
      else {
         first = TR7_FROM_PAIR(copy);
         push_recent_alloc(tsc, first);
      }

      /* next */
      prev = copy;
      cdr = TR7_PAIR_CDR(iter);
      if (!TR7_IS_PAIR(cdr)) {
         TR7_PAIR_CDR(copy) = cdr;
         break;
      }
      iter = TR7_TO_PAIR(cdr);
      TR7_PAIR_CDR(copy) = TR7_NIL;

      /* check loop */
      i++;
      if ((i & 1) == 0) {
         slow = TR7_TO_PAIR(TR7_PAIR_CDR(slow));
         if (iter == slow) {
            *head = *tail = TR7_NIL;
            return 0;
         }
      }
   }
   if (prev == NULL) {
      *tail = *head = TR7_NIL;
      return 0;
   }
   *head = first;
   *tail = TR7_FROM_PAIR(prev);
   return TR7_IS_NIL(cdr);
}
/*
* reverse list 'head' and set 'tail' at its end -- produce new list
*/
tr7_t tr7_reverse(tr7_engine_t tsc, tr7_t head, tr7_t tail)
{
   for ( ; TR7_IS_PAIR(head) ; head = TR7_CDR(head))
      tail = tr7_cons(tsc, TR7_CAR(head), tail);
   return TR7_IS_NIL(head) ? tail : TR7_VOID;   /* signal an error if improper  list */
}
/*
* reverse list 'head' and set 'tail' at its end -- in-place
*/
tr7_t tr7_reverse_in_place(tr7_t head, tr7_t tail)
{
   while (TR7_IS_PAIR(head)) {
      tr7_t tmp = TR7_CDR(head);
      TR7_CDR(head) = tail;
      tail = head;
      head = tmp;
   }
   return TR7_IS_NIL(head) ? tail : TR7_VOID;   /* signal an error if improper  list */
}
/*
* returns the list containing items of all given list in the order
* the last list is not copied but is referenced as the tail of the
* returned list.
*/
tr7_t tr7_append(tr7_engine_t tsc, int nitems, tr7_t items[])
{
   tr7_t head, *tail, t, x;
   int idx = 0;
   if (0 == nitems)
      return TR7_NIL;

   tail = &head;
   x = items[idx];
   while(++idx < nitems) {
      if (!TR7_IS_NIL(x)) {
         if (!TR7_IS_PAIR(x) || !list_copy(tsc, x, tail, &t))
            return TR7_FALSE;
         tail = &TR7_CDR(t);
      }
      x = items[idx];
   }
   *tail = x;
   return head;
}
/*
* Get, at max, 'count' pairs of the 'list' in 'pairs' and return the found count
*/
int tr7_get_list_pairs(tr7_t list, int count, tr7_pair_t pairs[])
{
   int i = 0;
   while (i < count && TR7_IS_PAIR(list)) {
      tr7_pair_t p = pairs[i++] = TR7_TO_PAIR(list);
      list = TR7_PAIR_CDR(p);
   }
   return i;
}
/*
* Get 'count' cars of the 'list' in 'cars' and return the found count
*/
int tr7_get_list_cars(tr7_t list, int count, tr7_t cars[], tr7_t *cdr)
{
   int i = 0;
   while (i < count && TR7_IS_PAIR(list)) {
      tr7_pair_t p = TR7_TO_PAIR(list);
      cars[i++] = TR7_PAIR_CAR(p);
      list = TR7_PAIR_CDR(p);
   }
   if (cdr != NULL)
      *cdr = list;
   return i;
}
/*
* Like scheme's assq but returns a pair pointer or NULL
* unsafe because doesn't check loops but then faster
*/
tr7_pair_t tr7_unsafe_assq_pair(tr7_t x, tr7_t list)
{
   tr7_pair_t plist = TR7_AS_PAIR(list);
   while (plist != NULL) {
      tr7_pair_t pitem = TR7_AS_PAIR(TR7_PAIR_CAR(plist));
      if (pitem != NULL && TR7EQ(x, TR7_PAIR_CAR(pitem)))
         return pitem;
      plist = TR7_AS_PAIR(TR7_PAIR_CDR(plist));
   }
   return plist;
}
/*
* Like scheme's memq but returns a pair pointer or NULL
* unsafe because doesn't check loops but then faster
*/
tr7_pair_t tr7_unsafe_memq_pair(tr7_t x, tr7_t list)
{
   tr7_pair_t plist = TR7_AS_PAIR(list);
   while (plist != NULL) {
      if (TR7EQ(x, TR7_PAIR_CAR(plist)))
         return plist;
      plist = TR7_AS_PAIR(TR7_PAIR_CDR(plist));
   }
   return plist;
}
/*
* Like scheme's memv but returns a pair pointer or NULL
* unsafe because doesn't check loops but then faster
*/
tr7_pair_t tr7_unsafe_memv_pair(tr7_t x, tr7_t list)
{
   tr7_pair_t plist = TR7_AS_PAIR(list);
   while (plist != NULL) {
      if (tr7_eqv(x, TR7_PAIR_CAR(plist)))
         return plist;
      plist = TR7_AS_PAIR(TR7_PAIR_CDR(plist));
   }
   return plist;
}
/*
* Like scheme's member but returns a pair pointer or NULL
* unsafe because doesn't check loops but then faster
*/
tr7_pair_t tr7_unsafe_member_pair(tr7_t x, tr7_t list)
{
   tr7_pair_t plist = TR7_AS_PAIR(list);
   while (plist != NULL) {
      if (tr7_equal(x, TR7_PAIR_CAR(plist)))
         return plist;
      plist = TR7_AS_PAIR(TR7_PAIR_CDR(plist));
   }
   return plist;
}
/*
* Like scheme's assoc but returns a pair pointer or NULL
*/
tr7_pair_t tr7_assoc_pair(tr7_t x, tr7_t list, int (*eq)(tr7_t, tr7_t))
{
   int i = 0;
   tr7_pair_t plist, pitem, slow;

   slow = plist = TR7_AS_PAIR(list);
   while (plist != NULL) {
      pitem = TR7_AS_PAIR(TR7_PAIR_CAR(plist));
      if (pitem != NULL && eq(x, TR7_PAIR_CAR(pitem)))
         return pitem;
      plist = TR7_AS_PAIR(TR7_PAIR_CDR(plist));
      i ^= 1;
      if (i == 0) {
         slow = TR7_TO_PAIR(TR7_PAIR_CDR(slow));
         if (slow == plist)
            break;
      }
   }
   return NULL;
}
/*
* Like scheme's assq but returns a pair pointer or NULL
*/
tr7_pair_t tr7_assq_pair(tr7_t x, tr7_t list)
{
   return tr7_assoc_pair(x, list, tr7_eq);
}
/*
* Like scheme's assv but returns a pair pointer or NULL
*/
tr7_pair_t tr7_assv_pair(tr7_t x, tr7_t list)
{
   return tr7_assoc_pair(x, list, tr7_eqv);
}
/*
* Like scheme's assoc for equal but returns a pair pointer or NULL
*/
tr7_pair_t tr7_asse_pair(tr7_t x, tr7_t list)
{
   return tr7_assoc_pair(x, list, tr7_equal);
}
/*
* Like scheme's member but returns a pair pointer or NULL
*/
tr7_pair_t tr7_member_pair(tr7_t x, tr7_t list, int (*eq)(tr7_t, tr7_t))
{
   int i = 0;
   tr7_pair_t plist, slow;

   slow = plist = TR7_AS_PAIR(list);
   while (plist != NULL) {
      if (eq(x, TR7_PAIR_CAR(plist)))
         return plist;
      plist = TR7_AS_PAIR(TR7_PAIR_CDR(plist));
      i ^= 1;
      if (i == 0) {
         slow = TR7_TO_PAIR(TR7_PAIR_CDR(slow));
         if (slow == plist)
            break;
      }
   }
   return NULL;
}
/*
* Like scheme's memq but returns a pair pointer or NULL
*/
tr7_pair_t tr7_memq_pair(tr7_t x, tr7_t list)
{
   return tr7_member_pair(x, list, tr7_eq);
}
/*
* Like scheme's memv but returns a pair pointer or NULL
*/
tr7_pair_t tr7_memv_pair(tr7_t x, tr7_t list)
{
   return tr7_member_pair(x, list, tr7_eqv);
}
/*
* Like scheme's member for equal but returns a pair pointer or NULL
*/
tr7_pair_t tr7_meme_pair(tr7_t x, tr7_t list)
{
   return tr7_member_pair(x, list, tr7_equal);
}
/*
* Like scheme's assoc
*/
tr7_t tr7_assoc(tr7_t x, tr7_t list, int (*eq)(tr7_t, tr7_t))
{
   tr7_pair_t pair = tr7_assoc_pair(x, list, eq);
   return pair ? TR7_FROM_PAIR(pair) : TR7_FALSE;
}
/*
* Like scheme's assq
*/
tr7_t tr7_assq(tr7_t x, tr7_t list)
{
   return tr7_assoc(x,  list, tr7_eq);
}
/*
* Like scheme's assv
*/
tr7_t tr7_assv(tr7_t x, tr7_t list)
{
   return tr7_assoc(x,  list, tr7_eqv);
}
/*
* Like scheme's assoc but for equal
*/
tr7_t tr7_asse(tr7_t x, tr7_t list)
{
   return tr7_assoc(x,  list, tr7_equal);
}
/*
* Like scheme's member
*/
tr7_t tr7_member(tr7_t x, tr7_t list, int (*eq)(tr7_t, tr7_t))
{
   tr7_pair_t pair = tr7_member_pair(x, list, eq);
   return pair ? TR7_FROM_PAIR(pair) : TR7_FALSE;
}
/*
* Like scheme's memq
*/
tr7_t tr7_memq(tr7_t x, tr7_t list)
{
   return tr7_member(x,  list, tr7_eq);
}
/*
* Like scheme's memv
*/
tr7_t tr7_memv(tr7_t x, tr7_t list)
{
   return tr7_member(x,  list, tr7_eqv);
}
/*
* Like scheme's member but for equal
*/
tr7_t tr7_meme(tr7_t x, tr7_t list)
{
   return tr7_member(x,  list, tr7_equal);
}
/*
**************************************************************************
* SECTION CHARACTER - Management of characters
* -----------------
*
* create a character
*/
tr7_t tr7_from_character(tr7_engine_t tsc, tr7_char_t value)
{
   return TR7_FROM_CHAR(value);
}
/*
* get a character
*/
tr7_char_t tr7_to_character(tr7_t t)
{
   return TR7_TO_CHAR(t);
}
/*
* is character?
*/
int tr7_is_character(tr7_t t)
{
   return TR7_IS_CHAR(t);
}
/*
* is 'car' a valid UNICODE codepoint?
*/
int tr7_is_char_unicode(tr7_char_t car)
{
   return (0 <= car && car <= 0xD7FF)
       || (0xE000 <= car && car <= 0x10FFFF);
}
/*
* is 'car' a valid UNICODE codepoint?
*/
static tr7_char_t to_char_unicode(tr7_char_t car)
{
   return tr7_is_char_unicode(car) ? car : TR7_CHAR_REPLACEMENT;
}
/*
* is 't' a character and a unicode character?
*/
int tr7_is_unicode_character(tr7_t t)
{
   return TR7_IS_CHAR(t) && tr7_is_char_unicode(TR7_TO_CHAR(t));
}
/*
* compare characters
*/
static tr7_compare_t char_cmp(tr7_char_t a, tr7_char_t b)
{
   return a < b ? Tr7_Cmp_Lesser : a == b ? Tr7_Cmp_Equal : Tr7_Cmp_Greater;
}
/*
* compare characters, ignoring case
*/
#if USE_SCHEME_CHAR
static tr7_compare_t char_cmp_ci(tr7_char_t a, tr7_char_t b)
{
   return char_cmp((tr7_char_t)towupper((wint_t)a),
                   (tr7_char_t)towupper((wint_t)b));
}
#endif
/*
* handle extended utf8 encoding
*/
static unsigned char_to_xtf8(tr7_char_t car, uint8_t *utf8)
{
   if (car <= 0x7F) { /* 0xxxxxxx */
      utf8[0] = (uint8_t)car;
      return 1;
   }
   if (car <= 0x7FF) { /* 110xxxxx 10xxxxxx */
      utf8[0] = (uint8_t)(192 + ((car >> 6) & 31));
      utf8[1] = (uint8_t)(128 + (car & 63));
      return 2;
   }
   if (car <= 0xFFFF) { /* 1110xxxx 10xxxxxx 10xxxxxx */
      utf8[0] = (uint8_t)(224 + ((car >> 12) & 15));
      utf8[1] = (uint8_t)(128 + ((car >> 6) & 63));
      utf8[2] = (uint8_t)(128 + (car & 63));
      return 3;
   }
   if (car <= 0x1FFFFF) { /* 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx */
      utf8[0] = (uint8_t)(240 + ((car >> 18) & 7));
      utf8[1] = (uint8_t)(128 + ((car >> 12) & 63));
      utf8[2] = (uint8_t)(128 + ((car >> 6) & 63));
      utf8[3] = (uint8_t)(128 + (car & 63));
      return 4;
   }
   if (car <= 0x3FFFFFF) { /* 111110xx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx */
      utf8[0] = (uint8_t)(248 + ((car >> 24) & 3));
      utf8[1] = (uint8_t)(128 + ((car >> 18) & 63));
      utf8[2] = (uint8_t)(128 + ((car >> 12) & 63));
      utf8[3] = (uint8_t)(128 + ((car >> 6) & 63));
      utf8[4] = (uint8_t)(128 + (car & 63));
      return 5;
   }
   if (car <= 0x7FFFFFFF) { /* 1111110x 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx */
      utf8[0] = (uint8_t)(252 + ((car >> 30) & 1));
      utf8[1] = (uint8_t)(128 + ((car >> 24) & 63));
      utf8[2] = (uint8_t)(128 + ((car >> 18) & 63));
      utf8[3] = (uint8_t)(128 + ((car >> 12) & 63));
      utf8[4] = (uint8_t)(128 + ((car >> 6) & 63));
      utf8[5] = (uint8_t)(128 + (car & 63));
      return 6;
   }
   return 0;
}
/*
* handle utf8 encoding
* even if not optimal in most cases, internal utf8 encoding was
* chosen for its natural compatibility with english language
*/
static unsigned char_to_utf8(tr7_char_t car, uint8_t *utf8)
{
   return char_to_xtf8(to_char_unicode(car), utf8);
}
/*
* convert an 'utf8' sequence to character 'car'
* returns the length of the byte sequence or 0 on error
*/
static unsigned xtf8_to_char(const uint8_t *utf8, tr7_char_t *car)
{
   uint8_t x = utf8[0];
   if (x < 128) { /* 0xxxxxxx */
      *car = (tr7_char_t)x;
      return 1;
   }
   if (x < 192) { /* 10xxxxxx */
      *car = 0;
      return 0;
   }
   if (x < 224) { /* 110xxxxx 10xxxxxx */
      *car = (((tr7_char_t)(x & 31)) << 6)
           | ((tr7_char_t)(utf8[1] & 63));
      return 2;
   }
   if (x < 240) { /* 1110xxxx 10xxxxxx 10xxxxxx */
      *car = (((tr7_char_t)(x & 15)) << 12)
           | (((tr7_char_t)(utf8[1] & 63)) << 6)
           | ((tr7_char_t)(utf8[2] & 63));
      return 3;
   }
   if (x < 248) { /* 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx */
      *car = (((tr7_char_t)(x & 7)) << 18)
           | (((tr7_char_t)(utf8[1] & 63)) << 12)
           | (((tr7_char_t)(utf8[2] & 63)) << 6)
           | ((tr7_char_t)(utf8[3] & 63));
      return 4;
   }
   if (x < 252) { /* 111110xx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx */
      *car = (((tr7_char_t)(x & 3)) << 24)
           | (((tr7_char_t)(utf8[1] & 63)) << 18)
           | (((tr7_char_t)(utf8[2] & 63)) << 12)
           | (((tr7_char_t)(utf8[3] & 63)) << 6)
           | ((tr7_char_t)(utf8[4] & 63));
      return 5;
   }
   if (x < 254) { /* 1111110x 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx */
      *car = (((tr7_char_t)(x & 1)) << 30)
           | (((tr7_char_t)(utf8[1] & 63)) << 24)
           | (((tr7_char_t)(utf8[2] & 63)) << 18)
           | (((tr7_char_t)(utf8[3] & 63)) << 12)
           | (((tr7_char_t)(utf8[4] & 63)) << 6)
           | ((tr7_char_t)(utf8[5] & 63));
      return 6;
   }
   *car = 0;
   return 0;
}
/*
* convert an 'utf8' sequence to character 'car'
* returns the length of the byte sequence or 0 on error
*/
static unsigned utf8_to_char(const uint8_t *utf8, tr7_char_t *car)
{
   tr7_char_t c;
   unsigned resu = xtf8_to_char(utf8, &c);
   if (resu == 0) {
      *car = TR7_CHAR_REPLACEMENT;
      return 1;
   }
   *car = to_char_unicode(c);
   return resu;
}
/*
* get the length in bytes of the utf8 representation of 'var'
*/
static unsigned char_xtf8_length(tr7_char_t car)
{
   if (car <= 0x7F)
      return 1;
   if (car <= 0x7FF)
      return 2;
   if (car <= 0xFFFF)
      return 3;
   if (car <= 0x1FFFFF)
      return 4;
   if (car <= 0x3FFFFFF)
      return 5;
   if (car <= 0x7FFFFFFF)
      return 6;
   return 0;
}
/*
* get the length in bytes of the utf8 representation of 'car'
*/
static unsigned char_utf8_length(tr7_char_t car)
{
   return char_xtf8_length(to_char_unicode(car));
}
/*
* get the length in byte of the extended utf8 sequence whose first
* byte is 'utf8' or return 0 if the byte is invalid utf8 header
*/
static unsigned xtf8_length(uint8_t utf8)
{
   if (utf8 < 128)
      return 1;
   if (utf8 < 192)
      return 0;
   if (utf8 < 224)
      return 2;
   if (utf8 < 240)
      return 3;
   if (utf8 < 248)
      return 4;
   if (utf8 < 252)
      return 5;
   if (utf8 < 254)
      return 6;
   return 0;
}
/*
* get the count of characters of the utf8 string of 'length' bytes
*/
static size_t utf8str_nchars(const uint8_t *str, size_t length)
{
   unsigned nb;
   size_t nchars = 0, index = 0;
   while (index < length) {
      nb = xtf8_length((uint8_t)str[index]);
      if (!nb)
         break;
      index += nb;
      nchars += index <= length;
   }
   return nchars;
}
/*
* get the byte offset of the 'pos'th character of the utf8 string of 'length' bytes
*/
static ssize_t utf8str_offset_end(const uint8_t *sutf8, size_t length, size_t pos)
{
   size_t nb, index = 0;
   for (;;) {
      if (!pos)
         return (ssize_t)index;
      if (index >= length)
         return -1;
      nb = xtf8_length(sutf8[index]);
      if (!nb)
         return -2;
      index += nb;
      pos--;
   }
}
/*
* get the byte offset of the 'pos'th character of the utf8 string of 'length' bytes
*/
static ssize_t utf8str_offset(const uint8_t *sutf8, size_t length, size_t pos)
{
   size_t nb, index = 0;
   for (;;) {
      if (index >= length)
         return -1;
      if (!pos)
         return (ssize_t)index;
      nb = xtf8_length(sutf8[index]);
      if (!nb)
         return -2;
      index += nb;
      pos--;
   }
}
/*
* Validate the utf8 sequence of length
*/
static int utf8str_is_valid(const uint8_t *sutf8, size_t length)
{
   tr7_char_t car;
   while (length) {
      unsigned len = xtf8_length(*sutf8);
      if (!len || len > length)
         return 0;
      xtf8_to_char(sutf8, &car);
      if (!tr7_is_char_unicode(car))
         return 0;
      sutf8 += len;
      length -= len;
   }
   return 1;
}
/*
* convert 'length' bytes of 'src' in lower characters in 'dst'
*/
static const char *fold(char *dst, const char *src, size_t *plength)
{
   tr7_char_t car;
   size_t ird = 0, iwr = 0, length = *plength;
   while (ird < length) {
      ird += utf8_to_char((const uint8_t*)&src[ird], &car);
      car = (tr7_char_t)towlower((wint_t)car);
      iwr += char_to_xtf8(car, (uint8_t*)&dst[iwr]);
   }
   dst[*plength = iwr] = 0;
   return dst;
}
/*
**************************************************************************
* SECTION CONTROL_CHARACTER - Control character naming
* -------------------------
*
* special management for control characters
*/
#if USE_ASCII_NAMES
static const char ctrlnames[] =
/* 0.. 7*/   "nul soh stx etx eot enq ack bel "
/* 8..15*/   "bs ht lf vt ff cr so si "
/*16..23*/   "dle dc1 dc2 dc3 dc4 nak syn etb "
/*24..31*/   "can em sub esc fs gs rs us "
/*!127!!*/   "del";
#endif
/*
 * Put in *'pc' the code for the character of given 'name'
 * and return 1
 * If 'name' is not for a known character, returns 0
 */
static int get_control_code(const char *name, tr7_char_t *pc, size_t length)
{
#if USE_ASCII_NAMES
   int i = search_cstr_index(name, ctrlnames, ',', ' ');
   if (i >= 0) {
      *pc = (tr7_char_t)(i == 32 ? 127 : i);
      return 1;
   }
#endif
   return 0;
}
/*
 * Return the name of the character or NULL is unknown
 */
static unsigned get_control_name(char *buf, size_t size, tr7_char_t c)
{
#if USE_ASCII_NAMES
   unsigned len;
   const char *str = search_cstr(c < 32 ? (int)(c & 31) : 32, ctrlnames, ' ', &len, ' ');
   if (str != NULL) {
      memcpy(buf, str, len);
      buf[len] = 0;
      return len;
   }
#endif
   buf[0] = '#';
   buf[1] = '\\';
   buf[2] = 'x';
   buf[3] = DIGIT2CHAR((c>>4)&15);
   buf[4] = DIGIT2CHAR(c&15);
   buf[5] = 0;
   return 5;
}
/*
**************************************************************************
* SECTION IMMUTABLE - Immutable flag for cells
* -----------------
*
* test if 't' is immutable
*/
int tr7_is_immutable(tr7_t t)
{
   return TR7_IS_CELL(t) && TR7_IS_IMMUTABLE_CELL(t);
}
/*
* set 't' as immutable
*/
void tr7_set_immutable(tr7_t t)
{
   if (TR7_IS_CELL(t))
      TR7_SET_IMMUTABLE_CELL(t);
}
/*
**************************************************************************
* SECTION BUFFERS - Management of buffers
* ---------------
*
* Create a tr7_t holding a buffer of 'kind' pointing the buffer 'buf'
* of allocated 'len' bytes. Set the finalisation flag of the allocated cell
* accordingly to 'final'. When final isn't zero, the given 'buf' is freed
* using memfree when cell becomes garbage (and also, for convenience, when
* the allocation of the cell failed).
*/
static tr7_t create_buffer(tr7_engine_t tsc, uint8_t *buf, size_t len, int final, unsigned kind)
{
   tr7_buffer_t buffer = GET_CELLS(tsc, buffer, final);
   if (!buffer) {
      if (final)
         memfree(tsc, buf);
      return TR7_NIL;
   }
   TR7_CELL_HEAD(buffer) = TR7_MAKE_HEAD(len, kind);
   buffer->content = buf;
   return push_recent_cell(tsc, buffer);
}
/*
* finalize the buffer
*/
static void finalize_buffer(tr7_engine_t tsc, tr7_cell_t a)
{
   memfree(tsc, TR7_CELL_CONTENT_BUFFER(a));
}
/*
**************************************************************************
* SECTION BYTEVECTORS - Management of bytevectors
* -------------------
*
* create a new bytevector pointing a 'mem'ory of 'len'gth bytes
*/
static tr7_t make_bytevector(tr7_engine_t tsc, uint8_t *mem, size_t len, int final)
{
   return mem ? create_buffer(tsc, mem, len, final, Tr7_Head_Kind_Byte_Vector) : TR7_NIL;
}
/*
* get a new bytevector holding the 'bytes' of 'len'. 'bytes' is freed on dispose
*/
tr7_t tr7_make_bytevector_take(tr7_engine_t tsc, uint8_t *bytes, size_t len)
{
   return make_bytevector(tsc, bytes, len, 1);
}
/*
* get a new bytevector holding the 'bytes' of 'len'.
*/
tr7_t tr7_make_bytevector_static(tr7_engine_t tsc, uint8_t *bytes, size_t len)
{
   return make_bytevector(tsc, bytes, len, 0);
}
/*
* get a new bytevector holding a copy of 'len' 'bytes'
*/
tr7_t tr7_make_bytevector_copy(tr7_engine_t tsc, const uint8_t *bytes, size_t len)
{
   return make_bytevector(tsc, memalloc_copy(tsc, bytes, len), len, 1);
}
/*
* get a new bytevector holding a 'len' bytes set to 'byte'
*/
tr7_t tr7_make_bytevector_fill(tr7_engine_t tsc, uint8_t byte, size_t len)
{
   return make_bytevector(tsc, memalloc_fill(tsc, byte, len), len, 1);
}
/*
* get a new bytevector holding a 'len' bytes unintialized (unsafe)
*/
tr7_t tr7_make_bytevector(tr7_engine_t tsc, size_t len)
{
   return make_bytevector(tsc, memalloc(tsc, len), len, 1);
}
/*
**************************************************************************
* SECTION STRINGS - Management of strings
* ---------------
*
* Management of strings
*/
static tr7_t make_string(tr7_engine_t tsc, char *mem, size_t length, int final)
{
   return create_buffer(tsc, (uint8_t*)mem, length, final, Tr7_Head_Kind_String);
}
/*
* get a new string holding a 'len' bytes unintialized (unsafe)
*/
static tr7_t make_string_noinit(tr7_engine_t tsc, size_t length)
{
   char *mem = memalloc(tsc, length + 1);
   if (!mem)
      return TR7_NIL;
   return make_string(tsc, mem, length, 1);
}
/*
* get a string of 'ncars' times the character 'car'
*/
tr7_t tr7_make_string_fill(tr7_engine_t tsc, tr7_char_t car, size_t ncars)
{
   if (tr7_is_char_unicode(car)) {
      uint8_t utf8[UTF8BUFFSIZE];
      unsigned clen = char_to_utf8(car, utf8); /* convert to utf8 */
      size_t i, length = ncars * clen;
      char *mem = memalloc(tsc, 1 + length);
      if (mem) {
         if (clen == 1)
            memset(mem, utf8[0], length);
         else
            for (i = 0 ; i < length ; i += clen)
               memcpy(&mem[i], utf8, clen);
         mem[length] = 0;
         return make_string(tsc, mem, length, 1);
      }
   }
   return TR7_NIL;
}
/*
* get a string being a 'sutf8' of 'length'. sutf8 is freed on dispose
*/
tr7_t tr7_make_string_take_length(tr7_engine_t tsc, char *sutf8, size_t length)
{
   int valid = sutf8 && utf8str_is_valid((uint8_t*)sutf8, length);
   return valid ? make_string(tsc, sutf8, length, 1) : TR7_NIL;
}
/*
* get a string being a 'sutf8'. sutf8 is freed on dispose
*/
tr7_t tr7_make_string_take(tr7_engine_t tsc, char *sutf8)
{
   return sutf8 ? make_string(tsc, sutf8, strlen(sutf8), 1) : TR7_NIL;
}
/*
* get a string being a copy of 'sutf8' of 'length'
*/
tr7_t tr7_make_string_copy_length(tr7_engine_t tsc, const char *sutf8, size_t length)
{
   int valid = (sutf8 || !length) && utf8str_is_valid((const uint8_t*)sutf8, length);
   if (valid) {
      void *mem = memalloc_copy_stringz(tsc, sutf8, length);
      if (mem)
         return make_string(tsc, mem, length, 1);
   }
   return TR7_NIL;
}
/*
* get a string being a copy of 'sutf8' terminated with a zero
*/
tr7_t tr7_make_string_copy(tr7_engine_t tsc, const char *sutf8)
{
   return sutf8 ? tr7_make_string_copy_length(tsc, sutf8, strlen(sutf8)) : TR7_NIL;
}
/*
* get a string referencing 'sutf8' of 'length'
*/
tr7_t tr7_make_string_static_length(tr7_engine_t tsc, const char *sutf8, size_t length)
{
   int valid = sutf8 && utf8str_is_valid((const uint8_t*)sutf8, length);
   return valid ? make_string(tsc, (char*)sutf8, length, 0) : TR7_NIL;
}
/*
* get a string referencing 'sutf8' terminated with a zero
*/
tr7_t tr7_make_string_static(tr7_engine_t tsc, const char *sutf8)
{
   return sutf8 ? make_string(tsc, (char*)sutf8, strlen(sutf8), 0) : TR7_NIL;
}
/*
* get a string for 'sutf8' of 'length', either by copying it
* or just by referencing it
*/
tr7_t tr7_make_string_length(tr7_engine_t tsc, const char *sutf8, size_t length, int copy)
{
   return (copy ? tr7_make_string_copy_length : tr7_make_string_static_length)(tsc, sutf8, length);
}
/*
* get a string for 'sutf8' terminated by zero, either by copying it
* or just by referencing it
*/
tr7_t tr7_make_string(tr7_engine_t tsc, const char *sutf8, int copy)
{
   return (copy ? tr7_make_string_copy : tr7_make_string_static)(tsc, sutf8);
}
/*
* check if 'item' is a string
*/
int tr7_is_string(tr7_t item)
{
   return TR7_IS_STRING(item);
}
/*
* get the utf8 string buffer
*/
const char *tr7_string_buffer(tr7_t string)
{
   return (const char*)TR7_CONTENT_STRING(string);
}
/*
* get the string length (byte length without the terminating zero)
*/
size_t tr7_string_size(tr7_t string)
{
   return TR7_SIZE_STRING(string);
}
/*
* get length (in chars) of 'string'
*/
size_t tr7_string_length(tr7_t string)
{
   const uint8_t *str = TR7_CONTENT_STRING(string);
   size_t length = TR7_SIZE_STRING(string);
   return utf8str_nchars(str, length);
}
/*
* get the character of the 'string' at the position 'pos' (in characters)
*/
tr7_char_t tr7_string_ref(tr7_t string, size_t pos)
{
   tr7_char_t car;
   const uint8_t *str = (uint8_t*)TR7_CONTENT_STRING(string);
   size_t length = TR7_SIZE_STRING(string);
   ssize_t offset = utf8str_offset(str, length, pos);
   if (offset < 0)
      return TR7_CHAR_EOF;
   utf8_to_char(&str[offset], &car);
   return car;
}
/*
* set the character of the 'string' at the position 'pos' (in characters)
* to the character value 'car'
* returns 1 on success or else 0 on memory depletion
*/
int tr7_string_set(tr7_engine_t tsc, tr7_t string, size_t pos, tr7_char_t car)
{
   unsigned nbef, naft, dif;
   uint8_t *cpy, *str = (uint8_t*)TR7_CONTENT_STRING(string);
   size_t offset, length = TR7_SIZE_STRING(string);
   ssize_t off = utf8str_offset(str, length, pos);
   if (off < 0)
      return 0; /* invalid position */
   offset = (size_t)off;
   nbef = xtf8_length(str[offset]); /* utf8 length before */
   naft = char_utf8_length(car); /* utf8 length after */
   if (naft <= nbef) {
      char_to_utf8(car, &str[offset]);
      if (naft < nbef) {
         dif = nbef - naft;
         TR7_SET_SIZE_STRING(string, length - dif);
         str += offset + naft;
         length -= offset + naft;
         while(length) {
            *str = str[dif];
            str++;
            length--;
         }
         *str = 0;
      }
   }
   else {
      dif = naft - nbef;
      cpy = memalloc(tsc, length + dif + 1);
      if (!cpy)
         return 0;
      TR7_SET_SIZE_STRING(string, length + dif);
      TR7_CONTENT_STRING(string) = cpy;
      memcpy(cpy, str, offset);
      memcpy(&cpy[offset+naft], &str[offset+nbef], length - offset - nbef + 1);
      memfree(tsc, str);
      str = cpy;
      char_to_utf8(car, &str[offset]);
   }
   return 1;
}
/*
**************************************************************************
* SECTION SYMBOLS - Management of symbols
* ---------------
*
* management of symbols.
*
* create a new symbol for UTF8 name of length, copying it if required
*/
static tr7_t make_symbol(tr7_engine_t tsc, const char *name, size_t length, int copy)
{
   if (copy)
      name = memalloc_copy_stringz(tsc, name, length);
   return name ? create_buffer(tsc, (uint8_t*)name, length, copy, Tr7_Head_Kind_Symbol | TR7_MASK_HEAD_IMMUTABLE) : TR7_NIL;
}
/*
* check if 'item' is a symbol
*/
int tr7_is_symbol(tr7_t item)
{
   return TR7_IS_SYMBOL(item);
}
/*
* get the UTF8 string of the symbol
*/
const char *tr7_symbol_string(tr7_t symbol)
{
   return (const char*)TR7_CONTENT_SYMBOL(symbol);
}
/*
* get the size in byte of the symbol
*/
size_t tr7_symbol_size(tr7_t symbol)
{
   return TR7_SIZE_SYMBOL(symbol);
}
/*
* get the length in characters of the symbol
*/
size_t tr7_symbol_length(tr7_t symbol)
{
   return tr7_string_length(symbol); /* symbols are like strings */
}
/*
* get the character of the 'symbol' at the position 'pos' (in characters)
*/
tr7_char_t tr7_symbol_ref(tr7_t symbol, size_t pos)
{
   return tr7_string_ref(symbol, pos); /* symbols are like strings */
}
/*
**************************************************************************
* SECTION VECTORS - Management of vectors
* ---------------
*
* management of vectors
*
* allocates a vector of 'len' elements
* doesn't initialize elements
*/
static tr7_t alloc_vector(tr7_engine_t tsc, size_t len)
{
   tr7_vector_t vec = get_cells(tsc, 1 + len, 0);
   if (vec == NULL)
      return TR7_NIL;
   TR7_CELL_HEAD(vec) = TR7_MAKE_HEAD(len, Tr7_Head_Kind_Vector);
   return push_recent_cell(tsc, vec);
}
/*
* allocates a vector of 'len' elements
* initialize elements by copying elements pointed by 'items'
* taking elements from `stride` to `stride`
* i.e.: result[i] = items[i*stride]
*/
tr7_t tr7_make_vector_stride(tr7_engine_t tsc, size_t len, tr7_t *items, size_t stride)
{
   tr7_t res = alloc_vector(tsc, len);
   if (!TR7_IS_NIL(res)) {
      tr7_t *to = TR7_ITEMS_VECTOR(res);
      while(len) {
         *to = *items;
         to++;
         items += stride;
         len--;
      }
   }
   return res;
}
/*
* allocates a vector of 'len' elements
* initialize elements by copying elements pointed by 'items'
*/
tr7_t tr7_make_vector_copy(tr7_engine_t tsc, size_t len, tr7_t *items)
{
   return tr7_make_vector_stride(tsc, len, items, 1);
}
/*
* allocates a vector of 'len' elements
* initialize elements with 'value'
*/
tr7_t tr7_make_vector_fill(tr7_engine_t tsc, size_t len, tr7_t value)
{
   return tr7_make_vector_stride(tsc, len, &value, 0);
}
/*
* get the length of the vector 'vec' (or 0 if it is not a vector)
*/
size_t tr7_vector_length(tr7_t vec)
{
   return (size_t)TR7_LENGTH_VECTOR(vec);
}
/*
* get the item of the vector 'vec' at 'index'
* no check is done but its good if 'index' < tr7_vector_length('vec')
*/
tr7_t tr7_vector_ref(tr7_t vec, size_t index)
{
   return TR7_ITEM_VECTOR(vec, index);
}
/*
* set the item of the vector 'vec' at 'index' to 'value'.
* no check is done but its good if 'index' < tr7_vector_length('vec')
* returns 'value'.
*/
tr7_t tr7_vector_set(tr7_t vec, size_t index, tr7_t value)
{
   return TR7_ITEM_VECTOR(vec, index) = value;
}
/*
* get the vector whose items are those of the 'list'
*/
tr7_t tr7_list_to_vector(tr7_engine_t tsc, tr7_t list)
{
   tr7_t vec = TR7_NIL, *items;
   int i = tr7_list_length(list);
   if (i >= 0) {
      vec = alloc_vector(tsc, (size_t)i);
      if (!TR7_IS_NIL(vec)) {
         items = TR7_ITEMS_VECTOR(vec);
         for (; !TR7_IS_NIL(list) ; list = TR7_CDR(list))
            *items++ = TR7_CAR(list);
      }
   }
   return vec;
}
/*
* returns the list whose items are those of the 'vector'
*/
tr7_t tr7_vector_to_list(tr7_engine_t tsc, tr7_t vector)
{
   tr7_vector_t vec = TR7_AS_VECTOR(vector);
   return vec == NULL ? TR7_NIL
      : tr7_cons_n(tsc, TR7_VECTOR_LENGTH(vec), TR7_VECTOR_ITEMS(vec), TR7_NIL);
}
/*
**************************************************************************
* SECTION HASHING - computing of hash code on items
* ---------------
*/
static tr7_uint_t hash_any(tr7_t item);
/*
* hash a blob of memory
*/
static tr7_uint_t hash_memory(const void *ptr, size_t size)
{
   tr7_uint_t hashed = (tr7_uint_t)size;
   const uint8_t *it = (const uint8_t*)ptr, *end = &it[size];
   while (it != end)
      hashed = (hashed << 7)
             + (hashed >> (sizeof(tr7_uint_t) * CHAR_BIT - 7))
             + (tr7_uint_t)*it++;
   return hashed;
}
/*
* hash a vector of tr7_t values
*/
static tr7_uint_t hash_vector(const tr7_t *vec, size_t count)
{
   tr7_uint_t hashed = (tr7_uint_t)count;
   const tr7_t *end = &vec[count];
   while (vec != end)
      hashed = (hashed << 7)
             + (hashed >> (sizeof(tr7_uint_t) * CHAR_BIT - 7))
             + hash_any(*vec++);
   return hashed;
}
/*
* Computes a hashing code for the key
* the returned value is wrapped to be in the
* interval 0 .. table_size - 1
*/
static tr7_uint_t hash_utf8str(const uint8_t *sutf8, size_t length, int fold)
{
   tr7_char_t car;
   tr7_uint_t hashed = (tr7_uint_t)length;
   const uint8_t *end = &sutf8[length];
   while (sutf8 < end) {
      unsigned n = utf8_to_char(sutf8, &car);
      if (fold)
         car = (tr7_char_t)towlower((wint_t)car);
      hashed = (hashed << 5)
             + (hashed >> (sizeof(hashed) * CHAR_BIT - 5))
             + (tr7_uint_t)car;
      sutf8 += n;
   }
   return hashed >> 1; /* ensure fit also an integer */
}
/*
* hash any tr7_t value
*/
static tr7_uint_t hash_any(tr7_t item)
{
   tr7_cell_t cell;
   switch (TR7_TAG(item)) {
   case TR7_TAG_DOUBLE:
      return hash_memory(TR7_TO_DOUBLE(item), sizeof(double));

   case TR7_TAG_PAIR:
      return hash_any(TR7_CAR(item)) + hash_any(TR7_CAR(item));

   case TR7_TAG_CELL:
      cell = TR7_TO_CELL(item);
      switch (TR7_CELL_KIND(cell)) {
      case Tr7_Head_Kind_String:
      case Tr7_Head_Kind_Symbol:
         return hash_utf8str(TR7_CELL_CONTENT_BUFFER(cell),
                             TR7_CELL_LENGTH_BUFFER(cell), 0);

      case Tr7_Head_Kind_Byte_Vector:
         return hash_memory(TR7_CELL_CONTENT_BUFFER(cell),
                            TR7_CELL_LENGTH_BUFFER(cell));

      case Tr7_Head_Kind_Record:
      case Tr7_Head_Kind_Vector:
      case Tr7_Head_Kind_Box:
         return hash_vector(TR7_CELL_VECTOR_ITEMS(cell),
                            TR7_CELL_VECTOR_LENGTH(cell));
      default:
         break;
      }
      break;
   default:
      break;
   }
   return TR7_TO_UINT(item);
}
/*
**************************************************************************
* SECTION SYMBOLS_SET - Management of symbol set
* -------------------
*
* There is one symbols' set global. It is a vector of
* lists of symbol. The text of the string is used to
* compute the initial index through hash code.
*
* Create a new symbols set of the given size.
*/
static tr7_t symbols_set_initial_value(tr7_engine_t tsc, unsigned size)
{
   return tr7_make_vector_fill(tsc, size, TR7_NIL);
}
#if PURGE_SYMBOLS
/*
* Remove unused removable symbols
*/
static void purge_symbols(tr7_engine_t tsc)
{
   tr7_t set = tsc->symbols_set;
   if (TR7_IS_VECTOR(set)) { /* deinit makes it TR7_NIL */
      tr7_uint_t idx = TR7_LENGTH_VECTOR(set);
      while (idx) {
         tr7_t *prv = &TR7_ITEM_VECTOR(set, --idx);
         tr7_t x = *prv;
         while (!TR7_IS_NIL(x))
            if (isgarbage(tsc, TR7_TO_CELL(TR7_CAR(x))))
               *prv = x = TR7_CDR(x);
            else
               x = *(prv = &TR7_CDR(x));
      }
   }
}
#endif
/*
* Get the symbol matching the given UTF8 name of given length.
* If the symbol if not found, returns TR7_NIL if create == 0.
* But if create is not zero and the symbol isn't found, the symbol
* is created and added to the set. In that case, if copy is zero,
* the symbol reference directly the given string without copying it
* (for use for CDATA segments). It is copied otherwise.
*/
tr7_t tr7_symbol_lookup(tr7_engine_t tsc, const char *name, size_t length, int copy, int create)
{
   tr7_uint_t hash = hash_utf8str((const uint8_t*)name, length, 0); /* no folding */
   size_t index = (size_t)(hash % (size_t)TR7_LENGTH_VECTOR(tsc->symbols_set));
   tr7_t s, x, h = TR7_ITEM_VECTOR(tsc->symbols_set, index);
   /* search in set */
   for (x = h ; !TR7_IS_NIL(x) ; x = TR7_CDR(x)) {
      s = TR7_CAR(x);
      if (length == tr7_symbol_size(s)
       && strncmp(name, tr7_symbol_string(s), length) == 0)
         return s; /* return found symbol */
   }
   /* not found */
   if (!create)
      return TR7_FALSE;
   /* create */
   s = make_symbol(tsc, name, length, copy);
   TR7_ITEM_VECTOR(tsc->symbols_set, index) = TR7_CONS2(tsc, s, h);
   return s;
}
/*
* Get or create the symbol of UTF8 name.
* Copy it if copy isn't zero or else use the name as is (usefull if in CDATA)
*/
tr7_t tr7_get_symbol(tr7_engine_t tsc, const char *name, int copy)
{
   return tr7_get_symbol_length(tsc, name, strlen(name), copy);
}
/*
* Get or create the symbol of UTF8 name of given length.
* Copy it if copy isn't zero or else use the name as is (usefull if in CDATA)
*/
tr7_t tr7_get_symbol_length(tr7_engine_t tsc, const char *name, size_t length, int copy)
{
   return tr7_symbol_lookup(tsc, name, length, copy, 1);
}
/*
* Add to symbols' set all predefined symbols
*/
static void symbols_set_add_predefined_symbols(tr7_engine_t tsc)
{
   unsigned idx = sizeof predefined_symbols / sizeof *predefined_symbols;
   size_t szset = (size_t)TR7_LENGTH_VECTOR(tsc->symbols_set);
   while(idx) {
      const struct tr7_buffer *buffer = &predefined_symbols[--idx];
      const uint8_t *name = TR7_SYMBOL_CONTENT(buffer);
      size_t length = TR7_SYMBOL_SIZE(buffer);
      tr7_uint_t hash = hash_utf8str((const uint8_t*)name, length, 0); /* no folding */
      size_t index = (size_t)(hash % szset);
      tr7_t x = TR7_ITEM_VECTOR(tsc->symbols_set, index);
      tr7_t symbol = TR7_FROM_SYMBOL(buffer);
      TR7_ITEM_VECTOR(tsc->symbols_set, index) = tr7_cons(tsc, symbol, x);
   }
}
#if USE_TR7_ENVIRONMENT
/*
* returns the list of all symbols of the symbols' set
*/
static tr7_t symbols_set_all_symbols(tr7_engine_t tsc)
{
   tr7_t x, result = TR7_NIL;
   tr7_uint_t idx = TR7_LENGTH_VECTOR(tsc->symbols_set);
   while (idx--)
      for (x = TR7_ITEM_VECTOR(tsc->symbols_set, idx); !TR7_IS_NIL(x); x = TR7_CDR(x))
         result = tr7_cons(tsc, TR7_CAR(x), result);
   return result;
}
#endif
/*
**************************************************************************
* SECTION FILE_SEARCH - Searching of files
* -------------------
*
* locate a file based on a dirname of dirlength,
* a basename of baselength and set of possible suffixes
* return the opened file for read or NULL
* on success, path holds the filename
*/
static unsigned search_access_file_in(
   const char *dirname, unsigned dirlength,
   const char *basename, unsigned baselength,
   char path[], unsigned pathsize,
   const char **suffixes, unsigned nrsuffixes)
{
   unsigned len, idx, slen;

   /* prepare prefix */
   if (dirname == NULL) {
      len = baselength;
      if (len >= pathsize)
         return 0;
      memcpy(path, basename, baselength);
   }
   else {
      len = dirlength + baselength + 1;
      if (len >= pathsize)
         return 0;
      memcpy(path, dirname, dirlength);
      path[dirlength] = DIR_SEP_CHAR;
      memcpy(&path[dirlength + 1], basename, baselength);
   }
   path[len] = 0;

   /* try given suffixes */
   for (idx = 0 ; idx < nrsuffixes ; idx++) {
      slen = 1 + strlen(suffixes[idx]);
      if (len + slen <= pathsize) {
         memcpy(&path[len], suffixes[idx], slen);
         if (access(path, R_OK) == 0)
            return 1;
      }
   }
   return 0;
}
/*
* locate a file based basename of baselength, current context and envar
* return the opened file for read or NULL
* on success, path holds the filename
*/
static unsigned search_access_file(
   tr7_engine_t tsc,
   const char *basename, unsigned baselength,
   char path[], unsigned pathsize,
   const char **suffixes, unsigned nrsuffixes,
   const char *defpath)
{
   unsigned len, found = 0;
   const char *envpath;

   /* try first relatively to current loaded file */
   if (TR7_IS_PORT(tsc->loadport)) {
      port_t *pt = TR7__PORT__PORT(tsc->loadport);
      if (pt && (pt->flags & port_file) && TR7_IS_STRING(pt->rep.stdio.filename)) {
         envpath = (const char*)TR7_CONTENT_STRING(pt->rep.stdio.filename);
         for (len = strlen(envpath) ; len > 0 && envpath[len] != DIR_SEP_CHAR ; len--);
         if (len > 0)
            found = search_access_file_in(envpath, len, basename, baselength, path, pathsize, suffixes, nrsuffixes);
      }
   }

   /* if not found, search in current directory (direct search) */
   if (!found)
      found = search_access_file_in(NULL, 0, basename, baselength, path, pathsize, suffixes, nrsuffixes);

   /* if not found, search from defpath with fallback */
   if (!found) {
      envpath = defpath ? defpath : tsc->strings[Tr7_StrID_Path];
      /* search in environment path if found */
      if (envpath != NULL) {
         while(!found && *envpath) {
            while (*envpath == PATH_SEP_CHAR)
               envpath++;
            if (*envpath) {
               /* get next prefix directory */
               for(len = 1 ; envpath[len] && envpath[len] != PATH_SEP_CHAR ; len++);
               found = search_access_file_in(envpath, len, basename, baselength, path, pathsize, suffixes, nrsuffixes);
               envpath = &envpath[len];
            }
         }
      }
   }
   return found;
}
/*
**************************************************************************
* SECTION EXTENSIONS - Handle extensions
* ------------------
* Handle shared library for load-extension
*/
#if USE_TR7_EXTENSION
#ifdef _WIN32
static const void *extension_get(const char *extension, const char *entry)
{
   const void *result = NULL;
   HMODULE handle = LoadLibrary(extension);
   if (handle != NULL)
      result = (const void*)GetProcAddress(handle, entry);
   if (result == NULL) {
      LPVOID msg_buf;
      FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER,
                     NULL, GetLastError(), 0, (LPTSTR) & msg_buf, 0, NULL);
      fprintf(stderr, "Error loading extension \"%s\": %s\n", extension, (char*)msg_buf);
      LocalFree(msg_buf);
      if (handle != NULL)
         FreeLibrary(handle);
   }
   return result;
}
#else
static const void *extension_get(const char *extension, const char *entry)
{
   const void *result = NULL;
   void *handle = dlopen(extension, RTLD_LAZY);
   if (handle != NULL)
      result = dlsym(handle, entry);
   if (result == NULL) {
      fprintf(stderr, "Error loading extension \"%s\": %s\n", extension, dlerror());
      if (handle != NULL)
         dlclose(handle);
   }
   return result;
}
#endif
static int dl_load_ext(tr7_engine_t tsc, const char *name, const char *libname)
{
   char filename[PATH_MAX + 1];
   const tr7_C_func_def_t *funlist;
   unsigned found;

   found = search_access_file(tsc, name, strlen(name), filename, sizeof filename,
               extensions_suffixes,
                  (int)(sizeof extensions_suffixes / sizeof *extensions_suffixes),
               tsc->strings[Tr7_StrID_Extension_Path]);
   if (!found)
      fprintf(stderr, "Error location extension \"%s\"\n", name);
   else {
      funlist = (const tr7_C_func_def_t*)extension_get(filename, extensions_functions);
      if (funlist != NULL) {
         tr7_lib_register_C_func_list(tsc, libname, funlist);
         return 1;
      }
   }
   return 0;
}
#endif
/*
**************************************************************************
* SECTION OVERFLOW
* ----------------
*/
#if IGNORE_OVERFLOWS
# define overflow_add(a,b,res)           ((*(res)=((a)+(b))),0)
# define overflow_sub(a,b,res)           ((*(res)=((a)-(b))),0)
# define overflow_mul(a,b,res)           ((*(res)=((a)*(b))),0)
# define TR7_FROM_INT_OVERFLOW(tsc,val)  TR7_FROM_INT(val)
#elif defined __has_builtin
# if __has_builtin(__builtin_add_overflow)
#  define overflow_add __builtin_add_overflow
# endif
# if __has_builtin(__builtin_sub_overflow)
#  define overflow_sub __builtin_sub_overflow
# endif
# if __has_builtin(__builtin_mul_overflow)
#  define overflow_mul __builtin_mul_overflow
# endif
#endif
#ifndef overflow_add
static inline int overflow_add(tr7_int_t a, tr7_int_t b, tr7_int_t *res)
{
   tr7_int_t r = *res = a + b;
   return ((a ^ r) < 0) && ((a ^ b) >= 0);
}
#endif
#ifndef overflow_sub
static inline int overflow_sub(tr7_int_t a, tr7_int_t b, tr7_int_t *res)
{
   tr7_int_t r = *res = a - b;
   return ((a ^ r) < 0) && ((a ^ b) < 0);
}
#endif
#ifndef overflow_mul
static inline int overflow_mul(tr7_int_t a, tr7_int_t b, tr7_int_t *res)
{
   tr7_int_t c;
   *res = a * b;
   if (a < 0)
      a = -a;
   if (b < 0)
      b = -b;
   if (a < b) {
      tr7_int_t c = a;
      a = b;
      b = c;
   }
   if ((a >> (TR7WIDTH / 2)) == 0)
      return 0;
   if ((b >> (TR7WIDTH / 2)) != 0)
      return 1;
   c = b * (a & (((tr7_int_t)1 << (TR7WIDTH / 2)) - 1));
   c >>= (TR7WIDTH / 2);
   c += b * (a >> (TR7WIDTH / 2));
   return (c >> ((TR7WIDTH / 2) - 1)) != 0; /* minus 1 for the sign */
}
#endif
#ifndef TR7_FROM_INT_OVERFLOW
# define TR7_FROM_INT_OVERFLOW(tsc,val) (TR7_FIT_INT(val) ? TR7_FROM_INT(val) : tr7_from_double(tsc,val))
#endif
/*
**************************************************************************
* SECTION
* --------------
*
*/
#if !USE_MATH
/************* CAUTION this is a hack for compiling with USE_MATH = 0 */
/************* CAUTION don't compile with USE_MATH = 0 at the moment */
/************* support of double should be removed if USE_MATH = 0 */
/************* so name USE_MATH is not the expected one in that case */
union id {
   uint64_t i;
   double d;
};
#define _I2D_(x) (((union id){.i = (x)}).d)
#define _D2I_(x) (((union id){.d = (x)}).i)
#define _NAN_SI_ (((uint64_t)0x1)<<63)
#define _NAN_EI_ (((uint64_t)0x7ff)<<52)
#define _NAN_MI_ ((((uint64_t)0x1)<<52) - 1)
#define _NAN_(x) (((union id){.i = (uint64_t)(x) | _NAN_EI_}).d)
#define INFINITY _NAN_(0)
#define NAN      _NAN_(1)
#define isnan(x) (((_D2I_(x) & _NAN_EI_) == _NAN_EI_) && ((_D2I_(x) & _NAN_MI_) != 0))
#define isinf(x) (((_D2I_(x) & _NAN_EI_) == _NAN_EI_) && ((_D2I_(x) & _NAN_MI_) == 0))
#define isfinite(x) ((_D2I_(x) & _NAN_EI_) != _NAN_EI_)

static double trunc(double x) { return (double)(tr7_int_t)x; }
static double round(double x) { return x < 0 ? -round(-x) : trunc(x+0.5); }
static double floor(double x) { double r = trunc(x); return r == x ? r : x > 0 ? r : r - 1.0; }
static double fmod(double x, double y) { return x - y * trunc(x / y); }
static double sqrt(double x) {
   if (x < 0 || isnan(x)) return NAN;
   if (isinf(x)) return x;
   double i = 1;
   int j = 20;
   while(j--) i = 0.5 * (i + x / i);
   return i;
}
#endif


/* get number atom (integer) */
tr7_t tr7_from_int(tr7_engine_t tsc, tr7_int_t num)
{
   /* TODO shift to long integer if overflow */
   return TR7_FROM_INT(num);
}

tr7_t tr7_from_double(tr7_engine_t tsc, double n)
{
   tr7_double_t x = GET_CELLS(tsc, x, 0);
   *x = n;
   return push_recent_alloc(tsc, TR7_FROM_DOUBLE(x));
}

int tr7_is_number(tr7_t t)
{
   switch (TR7_TAG(t)) {
   case TR7_TAG_EINT:
   case TR7_TAG_OINT:
   case TR7_TAG_DOUBLE:
      return 1;
   default:
      return 0;
   }
}

int tr7_is_integer(tr7_t t)
{
   double x;
   switch (TR7_TAG(t)) {
   case TR7_TAG_EINT:
   case TR7_TAG_OINT:
      return 1;
   case TR7_TAG_DOUBLE:
      x = *TR7_TO_DOUBLE(t);
      return isfinite(x) && round(x) == x;
   default:
      return 0;
   }
}

int tr7_is_real(tr7_t t)
{
   return TR7_IS_DOUBLE(t);
}

int tr7_is_exact(tr7_t t)
{
   return TR7_IS_INT(t);
}

int tr7_is_exact_integer(tr7_t t)
{
   return TR7_IS_INT(t);
}

int tr7_is_NaN(tr7_t t)
{
   return TR7_IS_DOUBLE(t) && isnan(*TR7_TO_DOUBLE(t));
}

int tr7_is_finite(tr7_t t)
{
   return !TR7_IS_DOUBLE(t) || isfinite(*TR7_TO_DOUBLE(t));
}

int tr7_is_infinite(tr7_t t)
{
   return TR7_IS_DOUBLE(t) && isinf(*TR7_TO_DOUBLE(t));
}

tr7_int_t tr7_to_int(tr7_t t)
{
   if (TR7_TAG(t) == TR7_TAG_DOUBLE)
      return (tr7_int_t)*TR7_TO_DOUBLE(t);
   return TR7_TO_INT(t);
}

double tr7_to_double(tr7_t t)
{
   if (TR7_TAG(t) == TR7_TAG_DOUBLE)
      return *TR7_TO_DOUBLE(t);
   return (double)TR7_TO_INT(t);
}

/*
**************************************************************************
*
*/

#ifdef EMULATE_RINT
/* Round to nearest. Round to even if midway */
static double rint(double x)
{
   double fl = floor(x);
   double ce = ceil(x);
   double dfl = x - fl;
   double dce = ce - x;
   if (dfl > dce) {
      return ce;
   }
   else if (dfl < dce) {
      return fl;
   }
   else {
      if (fmod(fl, 2.0) == 0.0) {       /* I imagine this holds */
         return fl;
      }
      else {
         return ce;
      }
   }
}
#endif

static int is_zero_double(double x)
{
   return x < DBL_MIN && x > -DBL_MIN;
}

/*
**************************************************************************
*
*/

/* compares a and b */
tr7_compare_t tr7_cmp_num(tr7_t a, tr7_t b)
{
   tr7_int_t ia, ib;
   double da, db;
   switch (TR7_TAG(a)) {
   case TR7_TAG_EINT:
   case TR7_TAG_OINT:
      ia = TR7_TO_INT(a);
      switch (TR7_TAG(b)) {
      case TR7_TAG_EINT:
      case TR7_TAG_OINT:
         ib = TR7_TO_INT(b);
         return ia == ib ? Tr7_Cmp_Equal : ia < ib ? Tr7_Cmp_Lesser : Tr7_Cmp_Greater;
      case TR7_TAG_DOUBLE:
         da = (double)ia;
         db = *TR7_TO_DOUBLE(b);
         break;
      default:
         return 0;
      }
      break;
   case TR7_TAG_DOUBLE:
      da = *TR7_TO_DOUBLE(a);
      switch (TR7_TAG(b)) {
      case TR7_TAG_EINT:
      case TR7_TAG_OINT:
         db = (double)TR7_TO_INT(b);
         break;
      case TR7_TAG_DOUBLE:
         db = *TR7_TO_DOUBLE(b);
         break;
      default:
         return 0;
      }
      break;
   default:
      return 0;
   }
   return da == db ? Tr7_Cmp_Equal : da < db ? Tr7_Cmp_Lesser : da > db ? Tr7_Cmp_Greater : 0;
}

/*
**************************************************************************
*
*/

enum any_num_kind {
   num_kind_int,
   num_kind_double,
   num_kind_unset
};

typedef struct any_num any_num_t;

struct any_num {
   enum any_num_kind kind;
   union {
      tr7_int_t _int;
      double    _double;
   } value;
   tr7_engine_t tsc;
};

/* n := i */
static void any_num_set_int(any_num_t *n, tr7_int_t i)
{
   n->kind = num_kind_int;
   n->value._int = i;
}

/* n := i */
static void any_num_make_int(tr7_engine_t tsc, any_num_t *n, tr7_int_t i)
{
   n->kind = num_kind_int;
   n->value._int = i;
   n->tsc = tsc;
}

/* n := i */
static void any_num_set_double(any_num_t *n, double d)
{
   n->kind = num_kind_double;
   n->value._double = d;
}

/* n := i */
static void any_num_make_double(tr7_engine_t tsc, any_num_t *n, double d)
{
   n->kind = num_kind_double;
   n->value._double = d;
   n->tsc = tsc;
}

/* n := t */
static void any_num_make(tr7_engine_t tsc, any_num_t *n, tr7_t t)
{
   switch (TR7_TAG(t)) {
   case TR7_TAG_EINT:
   case TR7_TAG_OINT:
      any_num_make_int(tsc, n, TR7_TO_INT(t));
      break;
   case TR7_TAG_DOUBLE:
      any_num_make_double(tsc, n, *TR7_TO_DOUBLE(t));
      break;
   default:
      any_num_make_double(tsc, n, NAN);
      break;
   }
}

/* get the tr7_t for the number n */
static tr7_t any_num_get(any_num_t *n)
{
   switch(n->kind) {
   case num_kind_double:
      return tr7_from_double(n->tsc, n->value._double);
   default:
      return tr7_from_int(n->tsc, n->value._int);
   }
}

/* n = abs(n) */
static void any_num_abs(any_num_t *n)
{
   switch(n->kind) {
   case num_kind_double:
      if (n->value._double < 0)
         n->value._double = -n->value._double;
      break;
   default:
      if (n->value._int < 0)
         n->value._int = -n->value._int;
      break;
   }
}

/* n = min(n, i) */
static void any_num_min_int(any_num_t *n, tr7_int_t i)
{
   double di;
   switch(n->kind) {
   case num_kind_double:
      di = (double)i;
      if (di < n->value._double)
         n->value._double = di;
      break;
   default:
      if (i < n->value._int)
         n->value._int = i;
      break;
   }
}

/* n = min(n, d) */
static void any_num_min_double(any_num_t *n, double d)
{
   double di;
   switch(n->kind) {
   case num_kind_double:
      if (d < n->value._double)
         n->value._double = d;
      break;
   default:
      di = (double)n->value._int;
      n->kind = num_kind_double;
      n->value._double = d < di ? d : di;
      break;
   }
}

/* n = min(n, t) */
static void any_num_min(any_num_t *n, tr7_t t)
{
   switch (TR7_TAG(t)) {
   case TR7_TAG_EINT:
   case TR7_TAG_OINT:
      any_num_min_int(n, TR7_TO_INT(t));
      break;
   case TR7_TAG_DOUBLE:
      any_num_min_double(n, *TR7_TO_DOUBLE(t));
      break;
   default:
      break;
   }
}

/* n = max(n, i) */
static void any_num_max_int(any_num_t *n, tr7_int_t i)
{
   double di;
   switch(n->kind) {
   case num_kind_double:
      di = (double)i;
      if (di > n->value._double)
         n->value._double = di;
      break;
   default:
      if (i > n->value._int)
         n->value._int = i;
      break;
   }
}

/* n = max(n, d) */
static void any_num_max_double(any_num_t *n, double d)
{
   double di;
   switch(n->kind) {
   case num_kind_double:
      if (d > n->value._double)
         n->value._double = d;
      break;
   default:
      di = (double)n->value._int;
      n->kind = num_kind_double;
      n->value._double = d > di ? d : di;
      break;
   }
}

/* n = max(n, t) */
static void any_num_max(any_num_t *n, tr7_t t)
{
   switch (TR7_TAG(t)) {
   case TR7_TAG_EINT:
   case TR7_TAG_OINT:
      any_num_max_int(n, TR7_TO_INT(t));
      break;
   case TR7_TAG_DOUBLE:
      any_num_max_double(n, *TR7_TO_DOUBLE(t));
      break;
   default:
      break;
   }
}

/* n += i */
static void any_num_add_int(any_num_t *n, tr7_int_t i)
{
   switch(n->kind) {
   case num_kind_double:
      n->value._double += (double)i;
      break;
   default:
      n->value._int += i;
      break;
   }
}

/* n += d */
static void any_num_add_double(any_num_t *n, double d)
{
   switch(n->kind) {
   case num_kind_double:
      n->value._double += d;
      break;
   default:
      any_num_set_double(n, (double)n->value._int + d);
      break;
   }
}

/* n += t */
static void any_num_add(any_num_t *n, tr7_t t)
{
   switch (TR7_TAG(t)) {
   case TR7_TAG_EINT:
   case TR7_TAG_OINT:
      any_num_add_int(n, TR7_TO_INT(t));
      break;
   case TR7_TAG_DOUBLE:
      any_num_add_double(n, *TR7_TO_DOUBLE(t));
      break;
   default:
      break;
   }
}

/* n -= i */
static void any_num_sub_int(any_num_t *n, tr7_int_t i)
{
   switch(n->kind) {
   case num_kind_double:
      n->value._double -= (double)i;
      break;
   default:
      n->value._int -= i;
      break;
   }
}

/* n -= d */
static void any_num_sub_double(any_num_t *n, double d)
{
   switch(n->kind) {
   case num_kind_double:
      n->value._double -= d;
      break;
   default:
      any_num_set_double(n, (double)n->value._int - d);
      break;
   }
}

/* n -= t */
static void any_num_sub(any_num_t *n, tr7_t t)
{
   switch (TR7_TAG(t)) {
   case TR7_TAG_EINT:
   case TR7_TAG_OINT:
      any_num_sub_int(n, TR7_TO_INT(t));
      break;
   case TR7_TAG_DOUBLE:
      any_num_sub_double(n, *TR7_TO_DOUBLE(t));
      break;
   default:
      break;
   }
}

/* n *= i */
static void any_num_mul_int(any_num_t *n, tr7_int_t i)
{
   switch(n->kind) {
   case num_kind_double:
      n->value._double *= (double)i;
      break;
   default:
      n->value._int *= i;
      break;
   }
}

/* n *= d */
static void any_num_mul_double(any_num_t *n, double d)
{
   switch(n->kind) {
   case num_kind_double:
      n->value._double *= d;
      break;
   default:
      any_num_set_double(n, (double)n->value._int * d);
      break;
   }
}

/* n *= t */
static void any_num_mul(any_num_t *n, tr7_t t)
{
   switch (TR7_TAG(t)) {
   case TR7_TAG_EINT:
   case TR7_TAG_OINT:
      any_num_mul_int(n, TR7_TO_INT(t));
      break;
   case TR7_TAG_DOUBLE:
      any_num_mul_double(n, *TR7_TO_DOUBLE(t));
      break;
   default:
      break;
   }
}

/* n /= d */
static int any_num_div_double(any_num_t *n, double d)
{
   if (is_zero_double(d))
      return 0;
   switch(n->kind) {
   case num_kind_double:
      n->value._double /= d;
      break;
   default:
      any_num_set_double(n, (double)n->value._int / d);
      break;
   }
   return 1;
}

/* n /= i */
static int any_num_div_int(any_num_t *n, tr7_int_t i)
{
   return any_num_div_double(n, (double)i);
}

/* n /= t */
static int any_num_div(any_num_t *n, tr7_t t)
{
   switch (TR7_TAG(t)) {
   case TR7_TAG_EINT:
   case TR7_TAG_OINT:
      return any_num_div_int(n, TR7_TO_INT(t));
   case TR7_TAG_DOUBLE:
      return any_num_div_double(n, *TR7_TO_DOUBLE(t));
   default:
      return 0;
   }
}

/* (n, nn) := n truncate/ d */
static int any_num_div_trunc_double(any_num_t *n, any_num_t *nn, double d)
{
   double v, q, r;
   if (is_zero_double(d))
      return 0;
   switch(n->kind) {
   case num_kind_double:
      v = n->value._double;
      break;
   default:
      v = (double)n->value._int;
      break;
   }
   q = v / d;
   q = trunc(q);
   any_num_set_double(n, q);
   r = v - q * d;
   any_num_make_double(n->tsc, nn, r);
   return 1;
}

/* (n, nn) := n truncate/ i */
static int any_num_div_trunc_int(any_num_t *n, any_num_t *nn, tr7_int_t i)
{
   tr7_int_t q, r;
   if (i == 0)
      return 0;
   switch(n->kind) {
   case num_kind_double:
      return any_num_div_trunc_double(n, nn, (double)i);
   default:
      break;
   }
   q = n->value._int / i;
   r = n->value._int % i;
   n->value._int = q;
   any_num_make_int(n->tsc, nn, r);
   return 1;
}

/* (n, nn) := n truncate/ t */
static int any_num_div_trunc(any_num_t *n, any_num_t *nn, tr7_t t)
{
   switch (TR7_TAG(t)) {
   case TR7_TAG_EINT:
   case TR7_TAG_OINT:
      return any_num_div_trunc_int(n, nn, TR7_TO_INT(t));
   case TR7_TAG_DOUBLE:
      return any_num_div_trunc_double(n, nn, *TR7_TO_DOUBLE(t));
   default:
      return 0;
   }
}

/* (n, nn) := n floor/ d */
static int any_num_div_floor_double(any_num_t *n, any_num_t *nn, double d)
{
   double v, q, r;
   if (is_zero_double(d))
      return 0;
   switch(n->kind) {
   case num_kind_double:
      v = n->value._double;
      break;
   default:
      v = (double)n->value._int;
      break;
   }
   q = v / d;
   q = floor(q);
   any_num_set_double(n, q);
   r = v - q * d;
   any_num_make_double(n->tsc, nn, r);
   return 1;
}

/* (n, nn) := n floor/ i */
static int any_num_div_floor_int(any_num_t *n, any_num_t *nn, tr7_int_t i)
{
   tr7_int_t q, r;
   if (i == 0)
      return 0;
   switch(n->kind) {
   case num_kind_double:
      return any_num_div_floor_double(n, nn, (double)i);
   default:
      break;
   }
   q = n->value._int / i;
   r = n->value._int % i;
   if (r != 0 && (r ^ i) < 0) {
      r += i;
      q--;
   }
   n->value._int = q;
   any_num_make_int(n->tsc, nn, r);
   return 1;
}

/* (n, nn) := n floor/ t */
static int any_num_div_floor(any_num_t *n, any_num_t *nn, tr7_t t)
{
   switch (TR7_TAG(t)) {
   case TR7_TAG_EINT:
   case TR7_TAG_OINT:
      return any_num_div_floor_int(n, nn, TR7_TO_INT(t));
   case TR7_TAG_DOUBLE:
      return any_num_div_floor_double(n, nn, *TR7_TO_DOUBLE(t));
   default:
      return 0;
   }
}

static tr7_int_t gcd_int(tr7_int_t a, tr7_int_t b)
{
   tr7_int_t x;

   if (a < 0)
      a = -a;
   if (b < 0)
      b = -b;

   if (a != b) {
      if (a < b) {
         x = b;
         b = a;
         a = x;
      }

      while (b) {
         x = a % b;
         a = b;
         b = x;
      }
   }
   return a;
}

static double gcd_double(double a, double b)
{
   double x;

   if (a < 0)
      a = -a;
   if (b < 0)
      b = -b;

   if (a != b) {
      if (a < b) {
         x = b;
         b = a;
         a = x;
      }

      while (b) {
         x = fmod(a, b);
         a = b;
         b = x;
      }
   }
   return a;
}

/* n := gcd(n, i) */
static void any_num_gcd_int(any_num_t *n, tr7_int_t i)
{
   switch(n->kind) {
   case num_kind_double:
      n->value._double = gcd_double((double)i, n->value._double);
      break;
   default:
      n->value._int = gcd_int(i, n->value._int);
      break;
   }
}

/* n := gcd(n, d) */
static void any_num_gcd_double(any_num_t *n, double d)
{
   switch(n->kind) {
   case num_kind_double:
      n->value._double = gcd_double(d, n->value._double);
      break;
   default:
      any_num_set_double(n, gcd_double(d, (double)n->value._int));
      break;
   }
}

/* n := gcd(n, t) */
static void any_num_gcd(any_num_t *n, tr7_t t)
{
   switch (TR7_TAG(t)) {
   case TR7_TAG_EINT:
   case TR7_TAG_OINT:
      any_num_gcd_int(n, TR7_TO_INT(t));
      break;
   case TR7_TAG_DOUBLE:
      any_num_gcd_double(n, *TR7_TO_DOUBLE(t));
      break;
   default:
      break;
   }
}

static tr7_int_t lcm_int(tr7_int_t a, tr7_int_t b)
{
   if (a == 0 || b == 0)
      return 0;

   if (a < 0)
      a = -a;
   if (b < 0)
      b = -b;

   return (a / gcd_int(a, b)) * b;
}

static double lcm_double(double a, double b)
{
   if (a == 0 || b == 0)
      return 0;

   if (a < 0)
      a = -a;
   if (b < 0)
      b = -b;

   return (a / gcd_double(a, b)) * b;
}

/* n := lcm(n, i) */
static void any_num_lcm_int(any_num_t *n, tr7_int_t i)
{
   switch(n->kind) {
   case num_kind_double:
      n->value._double = lcm_double((double)i, n->value._double);
      break;
   default:
      n->value._int = lcm_int(i, n->value._int);
      break;
   }
}

/* n := lcm(n, d) */
static void any_num_lcm_double(any_num_t *n, double d)
{
   switch(n->kind) {
   case num_kind_double:
      n->value._double = lcm_double(d, n->value._double);
      break;
   default:
      any_num_set_double(n, lcm_double(d, (double)n->value._int));
      break;
   }
}

/* n := lcm(n, t) */
static void any_num_lcm(any_num_t *n, tr7_t t)
{
   switch (TR7_TAG(t)) {
   case TR7_TAG_EINT:
   case TR7_TAG_OINT:
      any_num_lcm_int(n, TR7_TO_INT(t));
      break;
   case TR7_TAG_DOUBLE:
      any_num_lcm_double(n, *TR7_TO_DOUBLE(t));
      break;
   default:
      break;
   }
}

static tr7_int_t exsqrt_int(tr7_int_t x, tr7_int_t *pr)
{
   tr7_int_t i, i2, r, ii;
   int n;

   for (i = x, n = 0 ; i >> 6 ; i >>= 2, n++);
   if (i < 16)
      i = i < 4 ? 1 : i < 9 ? 2 : 3;
   else
      i = i < 36 ? (i < 25 ? 4 : 5) : (i < 49 ? 6 : 7);
   i <<= n;
   for (;;) {
      i2 = i * i;
      r = x - i2;
      ii = i << 1;
      if (0 <= r && r <= ii) {
         *pr = r;
         return i;
      }
      i = (i2 + x) / ii;
   }
}

static double exsqrt_double(double x, double *pr)
{
   double r = floor(sqrt(x));
   *pr = x - r * r;
   return r;
}

/* (n, nn) := exact sqrt(i) */
static int any_num_exact_sqrt_int(any_num_t *n, any_num_t *nn, tr7_int_t i)
{
   tr7_int_t a, b;
   if (i < 0)
      return 0;
   a = exsqrt_int(i, &b);
   any_num_set_int(n, a);
   any_num_make_int(n->tsc, nn, b);
   return 1;
}

/* (n, nn) := exact sqrt(t) */
static int any_num_exact_sqrt_double(any_num_t *n, any_num_t *nn, double d)
{
   double a, b;
   if (d < 0)
      return 0;
   a = exsqrt_double(d, &b);
   any_num_set_double(n, a);
   any_num_make_double(n->tsc, nn, b);
   return 1;
}

/* (n, nn) := exact sqrt(n) */
static int any_num_exact_sqrt(any_num_t *n, any_num_t *nn)
{
   switch(n->kind) {
   case num_kind_double:
      return any_num_exact_sqrt_double(n, nn, n->value._double);
   default:
      return any_num_exact_sqrt_int(n, nn, n->value._int);
   }
}
/*
**************************************************************************
* SECTION ENVIRONMENT
* -------------------
*/
#define NEEDSLOC(val)  (!TR7_IS_INTERNAL(val) && !TR7_IS_TRANSFORM(val))
/*
* In this implementation, each frame of the environment may be
* a hash table: a vector of alists hashed by symbol pointer
*/
tr7_t mk_environment(tr7_engine_t tsc, tr7_t lower, unsigned len)
{
   tr7_environment_t env = get_cells(tsc, 2 + (len > 0 ? len : 1), 0);
   TR7_CELL_HEAD(env) = TR7_MAKE_HEAD(len, Tr7_Head_Kind_Environment);
   env->lower = lower;
   while (len)
      env->items[--len] = TR7_NIL;
   return push_recent_cell(tsc, env);
}
/*
* search in environment 'env' the item of 'symbol'
* limit exploration to 'depth'
*/
static tr7_pair_t environment_search_item(tr7_t env, tr7_t symbol, int depth)
{
   tr7_t h;
   tr7_environment_t e;
   unsigned index, hash = HASHPTR(symbol);
   tr7_pair_t pair = NULL;
   while (!TR7_IS_NIL(env) && depth > 0) {
      e = TR7_TO_ENVIRONMENT(env);
      index = hash % (unsigned)TR7_HEAD_UVALUE(TR7_CELL_HEAD(e));
      h = e->items[index];
      pair = tr7_unsafe_assq_pair(symbol, h);
      if (pair != NULL)
         break;
      env = e->lower;
      depth--;
   }
   return pair;
}
/*
* Search in environment 'env' the item of 'symbol'
* Doesn't limit exploration depth.
*/
static tr7_pair_t environment_find_item(tr7_t env, tr7_t symbol)
{
   return environment_search_item(env, symbol, INT_MAX);
}
/*
* Creates a location and initialize it with 'value'
* 'symbol' can be used by debug for giving the original name
* locations are special boxes
*/
static tr7_t environment_make_location(tr7_engine_t tsc, tr7_t value, tr7_t symbol)
{
   tr7_box_t box;
#if USE_TR7_DEBUG
   const unsigned nitems = 2;
#else
   const unsigned nitems = 1;
#endif
   const unsigned ncells = NCELL_OF_SIZE((sizeof*box) + nitems * sizeof(tr7_t));
   box = (tr7_box_t)get_cells(tsc, ncells, 0);
   if (box == NULL)
      return TR7_VOID;
   /* boxes are mutable by nature, ATM, use immutable for flaging locations */
   TR7_CELL_HEAD(box) = TR7_HEAD_SET_IMMUTABLE(TR7_MAKE_HEAD(nitems, Tr7_Head_Kind_Box));
   BOX_SET(box, value);
#if USE_TR7_DEBUG
   BOX_ITEM(box, 1) = symbol;
#endif
   return FROM_BOX(box);
}
/*
* Creates a new item in the environment 'env' for the 'symbol' and the 'value'
*/
static tr7_pair_t environment_create_item(tr7_engine_t tsc, tr7_t env, tr7_t symbol, tr7_t value)
{
   /* allocate the item */
   const unsigned ncells = NCELL_OF_SIZE(2*(sizeof(struct tr7_pair)));
   tr7_pair_t pair = (tr7_pair_t)get_cells(tsc, ncells, 0);
   if (pair != NULL) {
      /* link it at head */
      tr7_environment_t e = TR7_TO_ENVIRONMENT(env);
      unsigned hash = HASHPTR(symbol);
      unsigned index = hash % (unsigned)TR7_HEAD_UVALUE(TR7_CELL_HEAD(e));
      tr7_t h = e->items[index];
      pair[1].car = TR7_FROM_PAIR(pair);
      pair[1].cdr = h;
      pair[0].car = symbol;
      pair[0].cdr = value;
      e->items[index] = TR7_FROM_PAIR(&pair[1]);
   }
   return pair;
}
/*
* Creates or updates an item of the environment 'env' for the 'symbol' of
* initial 'value'. When 'newloc', the value is put in a new location.
*/
static tr7_pair_t environment_make_item(tr7_engine_t tsc, tr7_t env, tr7_t symbol, tr7_t value, int newloc)
{
   tr7_pair_t envit;
   if (newloc) {
      value = environment_make_location(tsc, value, symbol);
      if (TR7_IS_VOID(value))
         return NULL;
   }
   envit = environment_search_item(env, symbol, 1);
   if (envit == NULL)
      return environment_create_item(tsc, env, symbol, value);
   TR7_PAIR_CDR(envit) = value;
   return envit;
}
/*
* Creates an undefined item in 'env' for 'symbol'
*/
static int environment_define_void(tr7_engine_t tsc, tr7_t env, tr7_t symbol)
{
   return NULL != environment_make_item(tsc, env, symbol, TR7_VOID, 0);
}
/*
* Define 'symbol' of 'value' in 'env'
*/
static int environment_define(tr7_engine_t tsc, tr7_t env, tr7_t symbol, tr7_t value)
{
   return NULL != environment_make_item(tsc, env, symbol, value, NEEDSLOC(value));
}
/*
* Imports 'symbol' of 'value' in 'env'
*/
static int environment_import(tr7_engine_t tsc, tr7_t env, tr7_t symbol, tr7_t value)
{
   return NULL != environment_make_item(tsc, env, symbol, value, 0);
}
/*
* set the 'value' to the 'symbol' in 'env'
*/
static int environment_set(tr7_engine_t tsc, tr7_t env, tr7_t symbol, tr7_t value)
{
   tr7_pair_t envit = environment_search_item(env, symbol, INT_MAX);
   if (envit == NULL)
      return 0;
   if (!NEEDSLOC(value))
      TR7_PAIR_CDR(envit) = value;
   else if (IS_BOX(TR7_PAIR_CDR(envit)))
      SET_BOX(TR7_PAIR_CDR(envit), value);
   else {
      tr7_t box = environment_make_location(tsc, value, symbol);
      if (TR7_IS_VOID(box))
         return 0;
      TR7_PAIR_CDR(envit) = box;
   }
   return 1;
}
/*
* enumerate all the values of the given environment
* and call the function 'fun' with closure, name and value
* until it returns a non zero value.
*/
static int environment_enumerate_depth(tr7_engine_t tsc, tr7_t env, env_enum_cb_t fun, void *closure, int depth)
{
   tr7_t cur, y;
   tr7_pair_t envit;
   unsigned count, idx;
   int sts;
   tr7_environment_t e;

   for (sts = 0, cur = env ; sts == 0 && TR7_IS_ENVIRONMENT(cur) && depth > 0 ; cur = e->lower, depth--) {
      e = TR7_TO_ENVIRONMENT(cur);
      count = (unsigned)TR7_HEAD_UVALUE(TR7_CELL_HEAD(e));
      for (idx = 0 ; !sts && idx < count ; idx++) {
         y = e->items[idx];
         while (sts == 0 && !TR7_IS_NIL(y)) {
            envit = TR7_TO_PAIR(TR7_CAR(y));
            /* check that not overriden */
            if (envit == environment_find_item(env, TR7_PAIR_CAR(envit)))
               sts = fun(tsc, TR7_PAIR_CAR(envit), TR7_PAIR_CDR(envit), closure);
            y = TR7_CDR(y);
         }
      }
   }
   return sts;
}
/**
* enumerate all the values of the given environment
* and call the function 'fun' with closure, name and value
* until it returns a non zero value.
*/
static int environment_enumerate(tr7_engine_t tsc, tr7_t env, env_enum_cb_t fun, void *closure)
{
   return environment_enumerate_depth(tsc, env, fun, closure, INT_MAX);
}
/*
**************************************************************************
* SECTION BUILTIN_PROC_SYNTAX
* ---------------------------
*
* returns the name of the procedure of index
*/
static const char *get_proc_name(tr7_uint_t index)
{
   const proc_desc_t *ifo = &procedures[index];
   return (const char*)predefined_symbols[ifo->symbolid].content;
}
/*
* returns the name of the syntax of index
*/
static const char *get_syn_name(tr7_uint_t index)
{
   return (const char*)predefined_symbols[synsymbs[index]].content;
}
/*
* search the builtin library of name
*/
static int search_builtin_lib(const char *name, unsigned len)
{
   int idx = (sizeof builtin_libs / sizeof builtin_libs[0]) - 1;
   while(idx >= 0 && (strncmp(name, builtin_libs[idx].name, len) || builtin_libs[idx].name[len]))
      idx--;
   return idx;
}
/*
* record procedures of a builtin library
*/
static void builtin_lib_enum_proc(tr7_engine_t tsc, int ilib, env_enum_cb_t fun, void *closure)
{
   unsigned idx = ilib > 0 ? builtin_libs[ilib - 1].proc_last : 0;
   unsigned end = builtin_libs[ilib].proc_last;
   for ( ; idx != end ; idx++)
      fun(tsc, SYMBOL_AT(procedures[idx].symbolid), FROM_PROC(idx), closure);

   /* aliases for (scheme base) */
   if (ilib == 0) {
      fun(tsc, SYMBOL(CALL_CC), PROC(CALLCC), closure);
      fun(tsc, SYMBOL(CURR_INPORT), tsc->stdports[IDX_STDIN], closure);
      fun(tsc, SYMBOL(CURR_OUTPORT), tsc->stdports[IDX_STDOUT], closure);
      fun(tsc, SYMBOL(CURR_ERRPORT), tsc->stdports[IDX_STDERR], closure);
   }
}
/*
* record operators of a builtin library
*/
static void builtin_lib_enum_syntax(tr7_engine_t tsc, int ilib, env_enum_cb_t fun, void *closure)
{
   unsigned idx = ilib > 0 ? builtin_libs[ilib - 1].syn_last : 0;
   unsigned end = builtin_libs[ilib].syn_last;
   for ( ; idx != end ; idx++)
      fun(tsc, SYMBOL_AT(synsymbs[idx]), FROM_SYNTAX(idx), closure);

   /* aliases for (scheme base) */
   if (ilib == 0)
      fun(tsc, SYMBOL(LAMBDA_CHAR), SYNTAX(LAMBDA), closure);
}
/*
* record items of a builtin library
*/
static void builtin_lib_enum(tr7_engine_t tsc, int ilib, env_enum_cb_t fun, void *closure)
{
   builtin_lib_enum_proc(tsc, ilib, fun, closure);
   builtin_lib_enum_syntax(tsc, ilib, fun, closure);
}
/*
**************************************************************************
* SECTION CLOSURE-PROGRAM-PROCEDURE
* ---------------------------------
*
* make closure.
*/
static tr7_t mk_closure(tr7_engine_t tsc, tr7_t description, tr7_t upperframes, int kind)
{
   tr7_closure_t x = GET_CELLS(tsc, x, 0);

   TR7_CELL_HEAD(x) = kind;
   x->description = description;
   x->upperframes = upperframes;
#if USE_TR7_TAGGED_CLOSURES
   x->tag = TR7_NIL;
#endif
   return push_recent_cell(tsc, x);
}
/*
* is procedure?
*/
int tr7_is_procedure(tr7_t t)
{
   /*--
    * continuation should be procedure by the example
    * (call-with-current-continuation procedure?) ==> #t
    * in R^3 report sec. 6.9
    */
   return IS_PROC(t)
       || TR7_IS_LAMBDA(t)
#if USE_SCHEME_CASE_LAMBDA
       || TR7_IS_CASE_LAMBDA(t)
#endif
       || TR7_IS_CONTINUATION(t)
       || TR7_IS_RECFUN(t)
       || TR7_IS_CFUNC(t);
}
/*
* is closure?
*/
int tr7_is_closure(tr7_t proc)
{
   return TR7_IS_LAMBDA(proc)
#if USE_SCHEME_CASE_LAMBDA
       || TR7_IS_CASE_LAMBDA(proc)
#endif
   ;
}
/*
* Closure must be a closure object in a cell.
* Set the tag of the closure to value.
*/
void tr7_closure_set_tag(tr7_t closure, tr7_t value)
{
#if USE_TR7_TAGGED_CLOSURES
   TR7_TO_CLOSURE(closure)->tag = value;
#endif
}
/*
* Closure must be a closure object in a cell.
* Return the tag of the closure.
*/
tr7_t tr7_closure_get_tag(tr7_t closure)
{
#if USE_TR7_TAGGED_CLOSURES
   return TR7_TO_CLOSURE(closure)->tag;
#else
   return TR7_FALSE;
#endif
}
/*
**************************************************************************
* SECTION PARAMETERS
* ------------------
*
* Create a parameter object
*/
static tr7_t mk_parameter(tr7_engine_t tsc, tr7_t init, tr7_t converter)
{
   tr7_parameter_t p = GET_CELLS(tsc, p, 0);

   TR7_CELL_HEAD(p) = Tr7_Head_Kind_Parameter;
   p->value = init;
   p->converter = converter;
   return push_recent_cell(tsc, p);
}
/*
* Get the current value of the parameter
*/
static tr7_t parameter_get(tr7_engine_t tsc, tr7_t param)
{
   tr7_pair_t latest = tr7_assq_pair(param, tsc->stof_params);
   return latest ? TR7_PAIR_CDR(latest) : TR7_TO_PARAMETER(param)->value;
}
/*
* Set the value of the current parameter
*/
static void parameter_set(tr7_engine_t tsc, tr7_t param, tr7_t value)
{
   tr7_pair_t latest = tr7_assq_pair(param, tsc->stof_params);
   if (latest)
      TR7_PAIR_CDR(latest) = value;
   else
      TR7_TO_PARAMETER(param)->value = value;
}
/*
* Push a current value for the parameter
*/
static void parameter_push(tr7_engine_t tsc, tr7_t param, tr7_t value)
{
   tsc->stof_params = tr7_cons(tsc, tr7_cons(tsc, param, value), tsc->stof_params);
}
/*
* Removes 'count' top values of parameters
*/
static void parameter_pop(tr7_engine_t tsc, int count)
{
   tr7_t top = tsc->stof_params;
   while(count) {
      top = TR7_CDR(top);
      count--;
   }
   tsc->stof_params = top;
}
/*
**************************************************************************
* SECTION PROMISE
* ---------------
* Creation of promises
*/
#if USE_SCHEME_LAZY
static tr7_t mk_promise(tr7_engine_t tsc, tr7_head_t head, tr7_t item)
{
   tr7_promise_t pro = GET_CELLS(tsc, pro, 0);

   pro->head = head;
   pro->item = item;
   return push_recent_cell(tsc, pro);
}
#endif
/*
**************************************************************************
* SECTION RECORD
* --------------
* Return the record pointer if 'item' is a record descriptor, or otherwise NULL
*/
tr7_record_t tr7_as_record_desc(tr7_t item)
{
   tr7_record_t desc = TR7_AS_RECORD(item);
   return (desc != NULL && TR7_IS_VOID(TR7_RECORD_ITEM(desc, Record_Desc_Idx_RecId))) ? desc : NULL;
}
/*
* Is 'item' a record descriptor?
*/
int tr7_is_record_desc(tr7_t item)
{
   return tr7_as_record_desc(item) != NULL;
}
/*
* get field count of a record
*/
static unsigned record_desc_field_count(tr7_t recdesc)
{
   tr7_t fv = TR7_ITEM_RECORD(recdesc, Record_Desc_Idx_Field_Count);
   return (unsigned)TR7_TO_UINT(fv);
}
/*
* get the field at the given index
*/
static tr7_t record_desc_field(tr7_engine_t tsc, tr7_t recdesc, unsigned index)
{
   tr7_record_t desc = TR7_TO_RECORD(recdesc);
   for (;;) {
      tr7_record_t parent = TR7_AS_RECORD(TR7_RECORD_ITEM(desc, Record_Desc_Idx_Parent));
      unsigned count = 0;
      if (parent != NULL)
         count = TR7_TO_UINT(TR7_RECORD_ITEM(parent, Record_Desc_Idx_Field_Count));
      if (count <= index) {
         index -= count;
         count = (unsigned)(TR7_RECORD_LENGTH(desc) - Record_Desc_Idx_First_Field) >> 1;
         if (index >= count)
            return TR7_VOID;
         return TR7_RECORD_ITEM(desc, Record_Desc_Idx_First_Field + (index << 1));
      }
      desc = parent;
   }
}
/*
* Create a record descriptor of name and count fields, return it
*/
static tr7_t mk_record_desc(tr7_engine_t tsc, tr7_t name, tr7_t parent, unsigned count)
{
   unsigned idx, nrfields = count;
   unsigned len = Record_Desc_Idx_First_Field + 2 * count;
   tr7_record_t par, desc = get_cells(tsc, 1 + len, 0);
   if (!desc)
      return TR7_VOID;
   par = tr7_as_record_desc(parent);
   if (par != NULL)
      nrfields += record_desc_field_count(parent);
   else
      parent = TR7_FALSE;
   TR7_CELL_HEAD(desc) = TR7_MAKE_HEAD(len, Tr7_Head_Kind_Record);
   TR7_RECORD_ITEM(desc, Record_Desc_Idx_RecId) = TR7_VOID;
   TR7_RECORD_ITEM(desc, Record_Desc_Idx_Name) = name;
   TR7_RECORD_ITEM(desc, Record_Desc_Idx_Parent) = parent;
   TR7_RECORD_ITEM(desc, Record_Desc_Idx_Field_Count) = TR7_FROM_INT(nrfields);
   for (idx = Record_Desc_Idx_First_Field ; idx < len ; idx++)
      TR7_RECORD_ITEM(desc, idx) = TR7_FALSE;

   return push_recent_cell(tsc, desc);
}
/*
* add a field to the record desciptor
*/
static int record_desc_put_field(tr7_engine_t tsc, tr7_t recdesc, unsigned index, tr7_t name, int mutable)
{
   tr7_record_t desc = TR7_TO_RECORD(recdesc);
   unsigned idx = Record_Desc_Idx_First_Field + 2 * index;
   if (idx >= TR7_RECORD_LENGTH(desc))
      return 0;
   TR7_RECORD_ITEM(desc, idx) = name;
   TR7_RECORD_ITEM(desc, idx + 1) = mutable ? TR7_TRUE : TR7_FALSE;
   return 1;
}
/*
* create an instance of the record type 'recdesc' and init it with 'init'
*/
static tr7_record_t mk_record(tr7_engine_t tsc, tr7_t recdesc)
{
   unsigned nrfld = record_desc_field_count(recdesc);
   unsigned count = nrfld + Record_Idx_First;

   tr7_record_t rec = get_cells(tsc, 1 + count, 0);
   if (rec != NULL) {
      /* Record it as a record so that gc understands it. */
      TR7_CELL_HEAD(rec) = TR7_MAKE_HEAD(count, Tr7_Head_Kind_Record);
      TR7_RECORD_ITEM(rec, Record_Idx_RecId) = recdesc;
   }
   return rec;
}
/*
* create an instance of the record type 'recdesc' and init it with 'init'
*/
static tr7_t mk_record_instance(tr7_engine_t tsc, tr7_t recdesc, tr7_t init)
{
   tr7_vector_t vec;
   unsigned idx;
   unsigned nrfld = record_desc_field_count(recdesc);
   unsigned count = nrfld + Record_Idx_First;
   tr7_record_t rec = get_cells(tsc, 1 + count, 0);
   if (rec == NULL)
      return TR7_VOID;

   /* Record it as a record so that gc understands it. */
   TR7_CELL_HEAD(rec) = TR7_MAKE_HEAD(count, Tr7_Head_Kind_Record);
   TR7_RECORD_ITEM(rec, Record_Idx_RecId) = recdesc;

   vec = TR7_AS_VECTOR(init);
   if (vec) {
      if (TR7_VECTOR_LENGTH(vec) != nrfld)
         return TR7_VOID;
      for (idx = Record_Idx_First ; idx < count ; idx++)
         TR7_RECORD_ITEM(rec, idx) = TR7_VECTOR_ITEM(vec, idx - Record_Idx_First);
   }
   else {
      for (idx = Record_Idx_First ; idx < count && TR7_IS_PAIR(init) ; idx++, init = TR7_CDR(init))
         TR7_RECORD_ITEM(rec, idx) = TR7_CAR(init);
      if (idx < count || !TR7_IS_NIL(init))
         return TR7_VOID;
   }
   return push_recent_cell(tsc, rec);
}

static tr7_t mk_recfun(tr7_engine_t tsc, tr7_t recdesc, unsigned idx, recfun_op_t op, unsigned count)
{
   tr7_uint_t opterm = ((tr7_uint_t)idx << _RecFun_Op_Shift_) | (tr7_uint_t)op;
   tr7_recfun_t recfun = get_cells(tsc, NCELL_OF_PTR(recfun) + count, 0);
   if (recfun == NULL)
      return TR7_NIL;
   recfun->head = TR7_MAKE_HEAD(NCELL_OF_PTR(recfun) - 1 + count, Tr7_Head_Kind_RecFun);
   recfun->opterm = TR7_FROM_UINT(opterm);
   recfun->recdesc = recdesc;
   return push_recent_cell(tsc, recfun);
}

static tr7_t mk_record_constructor(tr7_engine_t tsc, tr7_t recdesc, tr7_t args)
{
   unsigned idx, count, nargs;
   int iarg;
   tr7_t res, name, iter;
   tr7_recfun_t recfun;

   count = record_desc_field_count(recdesc);
   nargs = TR7_IS_TRUE(args) ? count : (unsigned)tr7_unsafe_list_length(args);
   res = mk_recfun(tsc, recdesc, nargs, RecFun_Op_Create, count);
   recfun = TR7_TO_RECFUN(res);
   for (idx = 0 ; idx < count ; idx++) {
      if (TR7_IS_TRUE(args))
         iarg = (int)idx;
      else {
         name = record_desc_field(tsc, recdesc, idx);
         iarg = 0;
         for (iter = args; TR7_IS_PAIR(iter) && !TR7EQ(name, TR7_CAR(iter)) ; iter = TR7_CDR(iter))
            iarg++;
         if (!TR7_IS_PAIR(iter))
            iarg = -1;
      }
      recfun->args[idx] = TR7_FROM_INT(iarg);
   }
   return res;
}

static tr7_t mk_record_predicate(tr7_engine_t tsc, tr7_t recdesc)
{
   return mk_recfun(tsc, recdesc, 0, RecFun_Op_Test, 0);
}

static tr7_t mk_record_accessor(tr7_engine_t tsc, tr7_t recdesc, unsigned idx)
{
   return mk_recfun(tsc, recdesc, idx + Record_Idx_First, RecFun_Op_Get, 0);
}

static tr7_t mk_record_modifier(tr7_engine_t tsc, tr7_t recdesc, unsigned idx)
{
   return mk_recfun(tsc, recdesc, idx + Record_Idx_First, RecFun_Op_Set, 0);
}
/*
* Is 'item' a record?
*/
int tr7_is_record(tr7_t item)
{
   tr7_record_t rec = TR7_AS_RECORD(item);
   return rec && !TR7_IS_VOID(TR7_RECORD_ITEM(rec, Record_Idx_RecId));
}
/*
* Get the record descriptor of the record 'item' or TR7_VOID when not a record
*/
tr7_t tr7_record_desc(tr7_t item)
{
   tr7_record_t rec = TR7_AS_RECORD(item);
   return rec ? TR7_RECORD_ITEM(rec, Record_Idx_RecId) : TR7_VOID;
}
/*
* Returns the record structure of item or 0 if its type doesn't match recdesc
*/
tr7_record_t tr7_as_record_cond(tr7_t item, tr7_t recdesc)
{
   tr7_record_t rec = TR7_AS_RECORD(item);
   if (rec != NULL) {
      tr7_t desc = TR7_RECORD_ITEM(rec, Record_Idx_RecId);
      while (TR7_IS_RECORD(desc)) {
         if (desc == recdesc)
            return rec;
         desc = TR7_ITEM_RECORD(desc, Record_Desc_Idx_Parent);
      }
   }
   return NULL;
}
/*
* Returns 1 if record structure of item match the type recdesc or 0 otherwise
*/
int tr7_is_record_type(tr7_t item, tr7_t recdesc)
{
   return tr7_as_record_cond(item, recdesc) != NULL;
}

tr7_t tr7_record_desc_name(tr7_t item)
{
   tr7_record_t desc = tr7_as_record_desc(item);
   return desc != NULL ? TR7_RECORD_ITEM(desc, Record_Desc_Idx_Name) : TR7_VOID;
}

tr7_t tr7_record_desc_parent(tr7_t item)
{
   tr7_record_t desc = tr7_as_record_desc(item);
   return desc != NULL ? TR7_RECORD_ITEM(desc, Record_Desc_Idx_Parent) : TR7_VOID;
}

const char *tr7_record_desc_name_string(tr7_t item)
{
   tr7_t name = tr7_record_desc_name(item);
   return TR7_IS_VOID(name) ? "?" : tr7_symbol_string(name);
}

const char *tr7_record_typename_string(tr7_t item)
{
   return tr7_record_desc_name_string(tr7_record_desc(item));
}

/* implementation of make record-type */
static int make_record_type_compiled(tr7_engine_t tsc, tr7_t desc, tr7_t parent, unsigned count)
{
   int mutable;
   unsigned field_count;   /* count of fields */
   unsigned parent_field_count;   /* count of parent fields */
   unsigned idx;
   int haspred;
   tr7_t typename;
   tr7_t recdesc;
   tr7_t iter, fields;
   tr7_t field_name;  /* name of a field */
   tr7_t *items;
   tr7_t constr;

   /* get typename, cons and pred */
   typename = TR7_CAR(desc);
   iter = TR7_CDR(desc);
   constr = TR7_CAR(iter);
   iter = TR7_CDR(iter);
   haspred = !TR7_IS_FALSE(TR7_CAR(iter));
   fields = TR7_CDR(iter);

   /* count fields */
   field_count = (unsigned)tr7_unsafe_list_length(fields);
   parent_field_count = tr7_is_record_desc(parent) ? record_desc_field_count(parent) : 0;

   /* allocates */
   items = data_stack_enter_safe(tsc, count);
   if (items == NULL)
      return -1;

   /* create the record descriptor */
   recdesc = mk_record_desc(tsc, typename, parent, field_count);
   iter = fields;
   for (idx = 0 ; !TR7_IS_NIL(iter) ; idx++) {
      mutable = !TR7_IS_FALSE(TR7_CAAR(iter));
      field_name = TR7_CDAR(iter);
      if (!record_desc_put_field(tsc, recdesc, idx, field_name, mutable)) {
         /* unexpected error */
         return -1;
      }
      iter = TR7_CDR(iter);
   }

   /* start build the result */
   *items++ = recdesc;

   /* creates constructor if required */
   if (!TR7_IS_FALSE(constr))
      *items++ = mk_record_constructor(tsc, recdesc, constr);

   /* creates predicates if required */
   if (haspred)
      *items++ = mk_record_predicate(tsc, recdesc);

   /* iterate over fields */
   iter = fields;
   for (idx = 0 ; !TR7_IS_NIL(iter) ; idx++) {
      mutable = !TR7_IS_FALSE(TR7_CAAR(iter));
      *items++ = mk_record_accessor(tsc, recdesc, idx + parent_field_count);
      if (mutable)
         *items++ = mk_record_modifier(tsc, recdesc, idx + parent_field_count);
      iter = TR7_CDR(iter);
   }
   return 0;
}

#if USE_SRFI_136
static tr7_t record_desc_fields_srfi136(tr7_engine_t tsc, tr7_t recdesc)
{
   unsigned idx, fld, parfld;
   tr7_t nam, mut, acc, mod, par, resu = TR7_NIL;
   tr7_record_t rec = tr7_as_record_desc(recdesc);
   if (rec != NULL) {
      fld = (unsigned)TR7_TO_INT(TR7_RECORD_ITEM(rec, Record_Desc_Idx_Field_Count));
      par = TR7_RECORD_ITEM(rec, Record_Desc_Idx_Parent);
      parfld = TR7_IS_FALSE(par) ? 0 : record_desc_field_count(par);
      while (fld > parfld) {
         idx = Record_Desc_Idx_First_Field + 2 * (--fld - parfld);
         nam = TR7_RECORD_ITEM(rec, idx);
         mut = TR7_RECORD_ITEM(rec, idx + 1);
         acc = mk_record_accessor(tsc, recdesc, fld);
         mod = TR7_IS_FALSE(mut) ? mut : mk_record_modifier(tsc, recdesc, fld);
         resu = tr7_cons(tsc, TR7_LIST3(tsc, nam, acc, mod), resu);
      }
   }
   return resu;
}

/* implementation of (make-record-type-descriptor ...) */
static tr7_t make_record_type_srfi136(tr7_engine_t tsc, tr7_t type, tr7_t parent, tr7_t fields)
{
   int mutable, len;
   unsigned field_count;   /* count of fields */
   unsigned idx;
   tr7_t recdesc;
   tr7_t iter, mut;
   tr7_t field_name;  /* name of a field */

   /* check fields */
   len = tr7_list_length(fields);
   if (len < 0) {
      /* invalid field list */
      return TR7_VOID;
   }
   field_count = (unsigned)len;

   /* create the record descriptor */
   recdesc = mk_record_desc(tsc, type, parent, field_count);

   /* iterate over fields */
   for (idx = 0 ; !TR7_IS_NIL(fields) ; idx++, fields = TR7_CDR(fields)) {
      iter = TR7_CAR(fields);
      if (TR7_IS_SYMBOL(iter)) {
         field_name = iter;
         mutable = 1;
      }
      else {
         mut = TR7_CAR(iter);
         iter = TR7_CDR(iter);
         if (!TR7_IS_PAIR(iter)) {
            /* invalid mutability specifier */
            return TR7_VOID;
         }
         field_name = TR7_CAR(iter);
         if (!TR7_IS_SYMBOL(field_name)) {
            /* bad field spec */
            return TR7_VOID;
         }
         if (TR7EQ(mut, SYMBOL(MUTABLE)))
            mutable = 1;
         else if (TR7EQ(mut, SYMBOL(IMMUTABLE)))
            mutable = 0;
         else {
            /* invalid mutability specifier */
            return TR7_VOID;
         }
      }
      if (!record_desc_put_field(tsc, recdesc, idx, field_name, mutable)) {
         /* unexpected error */
         return TR7_VOID;
      }
   }
   return recdesc;
}
#endif

/*
**************************************************************************
*
*/

#define transform_name(t) tr7_symbol_string(TR7_CAR(TR7_TO_TRANSFORM(t)->literals))

static tr7_t mk_transform(tr7_engine_t tsc, tr7_t name, tr7_t ellipsis, tr7_t literals, tr7_t rules, tr7_t envir)
{

   /* record the rule */
   tr7_t litt = tr7_cons(tsc, name, literals);
   tr7_transform_t t = GET_CELLS(tsc, t, 0);
   if (t == NULL)
      return TR7_NIL;

#if HAS_GREEDY_SYNTAX
   TR7_CELL_HEAD(t) = TR7_MAKE_HEAD(tsc->no_greedy_syntax, Tr7_Head_Kind_Transform);
#else
   TR7_CELL_HEAD(t) = Tr7_Head_Kind_Transform;
#endif
   t->ellipsis = TR7_IS_FALSE(tr7_memq(ellipsis, literals)) ? ellipsis : TR7_NIL;
   t->literals = litt;
   t->rules = rules;
   t->env = envir;

   return push_recent_cell(tsc, t);
}

/*
**************************************************************************
*
*/

static void port_rep_close(tr7_engine_t tsc, port_t *pt, unsigned flag)
{
   pt->flags &= ~flag;
   if ((pt->flags & (port_input | port_output)) == 0) {
      if (pt->flags & port_file) {
         if (pt->flags & port_closeit)
            fclose(pt->rep.stdio.file);
         pt->rep.stdio.filename = TR7_VOID;
      }
      else if (pt->flags & port_ownbuf)
         tsc->free(pt->rep.inmem.start);
      pt->flags = port_free;
   }
}

static void port_rep_free(tr7_engine_t tsc, port_t *pt)
{
   port_rep_close(tsc, pt, port_input | port_output);
   tsc->free(pt);
}

static port_t *port_rep_from_file(tr7_engine_t tsc, FILE * f, const char *fn, unsigned prop)
{
   port_t *pt;
   char mode[4];
   int idx;

   if (f == NULL) {
      if (prop & port_output) {
         if (prop & port_input) {
            mode[0] = 'a';
            mode[1] = '+';
            idx = 2;
         }
         else {
            mode[0] = 'w';
            idx = 1;
         }
      }
      else {
         mode[0] = 'r';
         idx = 1;
      }
      if (prop & port_binary)
         mode[idx++] = 'b';
      mode[idx] = 0;
      f = fopen(fn, mode);
      if (f == NULL)
         return NULL;
      prop |= port_closeit;
   }
   pt = (port_t *) memalloc(tsc, sizeof *pt);
   if (pt != NULL) {
      pt->flags = port_file | prop;
      pt->rep.stdio.file = f;
      pt->rep.stdio.filename = fn == NULL ? TR7_VOID : tr7_make_string_copy(tsc, fn);
#if USE_TR7_DEBUG && DEBUG_LINES
      pt->line = 1;
#endif
      pt->unread.count = 0;
   }
   else if (prop & port_closeit)
      fclose(f);
   return pt;
}

static void port_rep_init(port_t *pt, tr7_t item, uint8_t *start, uint8_t *end, unsigned flags)
{
   pt->flags = flags;
#if USE_TR7_DEBUG && DEBUG_LINES
   pt->line = 1;
#endif
   pt->rep.inmem.item = item;
   pt->rep.inmem.start = start;
   pt->rep.inmem.curr = start;
   pt->rep.inmem.end = end;
   pt->unread.count = 0;
}

static port_t *port_rep_from_string(tr7_engine_t tsc, tr7_t string, uint8_t *start, uint8_t *end)
{
   port_t *pt = (port_t*)memalloc(tsc, sizeof(port_t));
   if (pt)
      port_rep_init(pt, string, start, end ? end : start + strlen((char*)start), port_string | port_input | port_textual);
   return pt;
}

static port_t *port_rep_from_bytevector(tr7_engine_t tsc, tr7_t bytevec)
{
   port_t *pt = (port_t*)memalloc(tsc, sizeof(port_t));
   if (pt) {
      tr7_buffer_t bv = TR7_TO_BYTEVECTOR(bytevec);
      port_rep_init(pt, bytevec, bv->content, &bv->content[TR7_BUFFER_LENGTH(bv)],
                   port_bytevector | port_input | port_binary);
   }
   return pt;
}

static void port_rep_init_scratch(tr7_engine_t tsc, port_t *pt, uint8_t *start, unsigned size, unsigned flags)
{
   port_rep_init(pt, TR7_NIL, start, start + size - 1, flags);
   memset(start, 0, size);
}

static port_t *port_rep_from_scratch(tr7_engine_t tsc, unsigned prop)
{
   port_t *pt = (port_t*)memalloc(tsc, sizeof(port_t));
   if (pt) {
      uint8_t *start = memalloc(tsc, SCRATCH_SIZE);
      if (start)
         port_rep_init_scratch(tsc, pt, start, SCRATCH_SIZE,
                 prop | port_output | port_scratch | port_ownbuf);
      else {
         memfree(tsc, pt);
         pt = NULL;
      }
   }
   return pt;
}

static int port_rep_realloc_scratch_size(tr7_engine_t tsc, port_t * pt, size_t new_size)
{
   uint8_t *start = pt->rep.inmem.start;
   size_t old_size = (size_t)(pt->rep.inmem.end - start);
   if (new_size > old_size) {
      uint8_t *str = memalloc(tsc, new_size + 1);
      if (!str)
         return 0;
      memcpy(str, start, old_size);
      memset(&str[old_size], ' ', new_size - old_size);
      str[new_size] = '\0';
      pt->rep.inmem.start = str;
      pt->rep.inmem.end = str + new_size;
      pt->rep.inmem.curr = &str[pt->rep.inmem.curr - start];
      if (pt->flags & port_ownbuf)
         tsc->free(start);
      else
         pt->flags |= port_ownbuf;
   }
   return 1;
}

/*
static int port_rep_realloc_scratch(tr7_engine_t tsc, port_t * pt)
{
   size_t size = pt->rep.inmem.end - pt->rep.inmem.start;
   return port_rep_realloc_scratch_size(tsc, pt, size + size - 1);
}
*/

static tr7_t port_rep_get_string(tr7_engine_t tsc, port_t * pt)
{
   if ((pt->flags & (port_string | port_scratch)) == (port_string | port_scratch))
      return tr7_make_string_copy_length(tsc, (const char*)pt->rep.inmem.start,
                         (size_t)(pt->rep.inmem.curr - pt->rep.inmem.start));
   return TR7_FALSE;
}

static tr7_t port_rep_get_bytevector(tr7_engine_t tsc, port_t * pt)
{
   if ((pt->flags & (port_bytevector | port_scratch)) == (port_bytevector | port_scratch))
      return tr7_make_bytevector_copy(tsc, (uint8_t*)pt->rep.inmem.start,
                         (size_t)(pt->rep.inmem.curr - pt->rep.inmem.start));
   return TR7_FALSE;
}

/*
**************************************************************************
*
* Routines for reading
*/
static tr7_char_t saw_eof(port_t *pt)
{
   pt->flags |= port_saw_EOF;
   return TR7_CHAR_EOF;
}

static tr7_char_t port_read_char(tr7_engine_t tsc, port_t *pt)
{
   tr7_char_t car;
   if (pt->unread.count)
      car = pt->unread.stack[--pt->unread.count];
   else if (pt->flags & port_file) {
      unsigned n;
      uint8_t buf[UTF8BUFFSIZE];
      size_t sz = fread(buf, 1, 1, pt->rep.stdio.file);
      if (sz == 0)
         return saw_eof(pt);
      n = xtf8_length(buf[0]);
      if (n == 0)
         car = TR7_CHAR_REPLACEMENT;
      else {
         if (n > 1)
            sz = fread(&buf[1], n - 1, 1, pt->rep.stdio.file);
         if (sz == 0)
            car = TR7_CHAR_REPLACEMENT;
         else
            utf8_to_char(buf, &car);
      }
   }
   else if (pt->rep.inmem.curr >= pt->rep.inmem.end)
      return saw_eof(pt);
   else
      pt->rep.inmem.curr += utf8_to_char((uint8_t*)pt->rep.inmem.curr, &car);
   return car;
}

/* back character to input buffer */
static void port_unread_char(tr7_engine_t tsc, port_t *pt, tr7_char_t car)
{
   if (car != TR7_CHAR_EOF) {
      pt->flags &= ~(unsigned)port_saw_EOF;
      pt->unread.stack[pt->unread.count++] = car;
   }
}

/* check if a char is ready */
static int port_has_char(tr7_engine_t tsc, port_t *pt)
{
   /* TODO improve */
   return pt->unread.count || (pt->flags & (port_saw_EOF | port_string));
}

static int port_read_bytes(tr7_engine_t tsc, port_t *pt, uint8_t *buffer, unsigned length)
{
   unsigned nread = 0;
   size_t sz;

   while (pt->unread.count && nread < length)
      buffer[nread++] = (uint8_t)pt->unread.stack[--pt->unread.count];

   if (nread < length) {
      if (pt->flags & port_file) {
         sz = (unsigned)fread(&buffer[nread], 1, length - nread, pt->rep.stdio.file);
         if (sz > 0)
            nread += (unsigned)sz;
         else
            pt->flags |= port_saw_EOF;
      }
      else {
         sz = (size_t)(pt->rep.inmem.end - pt->rep.inmem.curr);
         if (sz == 0)
            pt->flags |= port_saw_EOF;
         else {
            if (sz < length)
               length = (unsigned)sz;
            if (nread < length) {
               length -= nread;
               memcpy(&buffer[nread], pt->rep.inmem.curr, length);
               pt->rep.inmem.curr += length;
               nread += length;
            }
         }
      }
   }
   return nread || !(pt->flags & port_saw_EOF) ? (int)nread : EOF;
}

static int port_read_byte(tr7_engine_t tsc, port_t *pt)
{
   uint8_t byte;
   int res = port_read_bytes(tsc, pt, &byte, 1);
   return res == 1 ? (int)byte : EOF;
}

/* back character to input buffer */
static void port_unread_byte(tr7_engine_t tsc, port_t *pt, int byte)
{
   if (byte != EOF) {
      pt->flags &= ~(unsigned)port_saw_EOF;
      pt->unread.stack[pt->unread.count++] = (tr7_char_t)byte;
   }
}

/*  */
static int port_has_byte(tr7_engine_t tsc, port_t *pt)
{
   /* TODO improve */
   return pt->unread.count || (pt->flags & (port_saw_EOF | port_bytevector));
}

/*
**************************************************************************
*
* Routines for writing
*/


static void port_flush(tr7_engine_t tsc, port_t *pt)
{
   if (pt->flags & port_file)
      fflush(pt->rep.stdio.file);
}

static int port_write_chars_length(tr7_engine_t tsc, port_t *pt, const tr7_char_t *s, unsigned len)
{
   unsigned idx;
   uint8_t buffer[UTF8BUFFSIZE];
   if (pt->flags & port_file) {
      for (idx = 0 ; idx < len ; idx++) {
         unsigned n = char_to_utf8(s[idx], buffer);
         if (fwrite(buffer, 1, n, pt->rep.stdio.file) != n)
            return 0;
      }
   }
   else {
      for (idx = 0 ; idx < len ; idx++)
         if (!port_write_utf8_length(tsc, pt, (char*)buffer, char_to_utf8(s[idx], buffer)))
            return 0;
   }
   return 1;
}

static int port_write_char(tr7_engine_t tsc, port_t *pt, tr7_char_t c)
{
   return port_write_chars_length(tsc, pt, &c, 1);
}

static int port_write_utf8_length(tr7_engine_t tsc, port_t *pt, const char *s, unsigned len)
{
   if (pt->flags & port_file) {
      if (fwrite(s, 1, len, pt->rep.stdio.file) != len)
         return 0;
   }
   else {
      size_t size = (size_t)(pt->rep.inmem.end - pt->rep.inmem.curr);
      if (size < (size_t)len) {
         if (!(pt->flags & port_scratch))
            return 0;
         size = (size_t)(len + ((pt->rep.inmem.curr - pt->rep.inmem.start) << 1));
         if (!port_rep_realloc_scratch_size(tsc, pt, size))
            return 0;
      }
      memcpy(pt->rep.inmem.curr, s, (size_t)len);
      pt->rep.inmem.curr += len;
   }
   return 1;
}

static int port_write_utf8(tr7_engine_t tsc, port_t *pt, const char *s)
{
   return port_write_utf8_length(tsc, pt, s, (unsigned)strlen(s));
}

static int port_write_bytes(tr7_engine_t tsc, port_t *pt, const uint8_t *bytes, unsigned len)
{
   if (pt->flags & port_file) {
      if (fwrite(bytes, 1, len, pt->rep.stdio.file) != len)
            return 0;
   }
   else {
      size_t size = (size_t)(pt->rep.inmem.end - pt->rep.inmem.curr);
      if (size < (size_t)len) {
         if (!(pt->flags & port_scratch))
            return 0;
         size = (size_t)(len + ((pt->rep.inmem.curr - pt->rep.inmem.start) << 1));
         if (!port_rep_realloc_scratch_size(tsc, pt, size))
            return 0;
      }
      memcpy(pt->rep.inmem.curr, bytes, len);
      pt->rep.inmem.curr += len;
   }
   return 1;
}

/*
**************************************************************************
*
*/
static int check_port(tr7_t p, unsigned int flags)
{
   return TR7_IS_PORT(p) && ((TR7__PORT__PORT(p)->flags & flags) == flags);
}

int tr7_is_input_port(tr7_t p)
{
   return check_port(p, port_input);
}

int tr7_is_output_port(tr7_t p)
{
   return check_port(p, port_output);
}

int tr7_is_textual_port(tr7_t p)
{
   return check_port(p, port_textual);
}

int tr7_is_binary_port(tr7_t p)
{
   return check_port(p, port_binary);
}

int tr7_is_textual_input_port(tr7_t p)
{
   return check_port(p, port_input | port_textual);
}

int tr7_is_textual_output_port(tr7_t p)
{
   return check_port(p, port_output | port_textual);
}

int tr7_is_binary_input_port(tr7_t p)
{
   return check_port(p, port_input | port_binary);
}

int tr7_is_binary_output_port(tr7_t p)
{
   return check_port(p, port_output | port_binary);
}

static tr7_t mk_port(tr7_engine_t tsc, port_t * p)
{
   tr7_port_t x = GET_CELLS(tsc, x, 1);
   if (!x)
      return TR7_FALSE;

   TR7_CELL_HEAD(x) = Tr7_Head_Kind_Port;
   x->_port_ = p;
   return push_recent_cell(tsc, x);
}

static void port_close(tr7_engine_t tsc, tr7_t p, unsigned flag)
{
   port_rep_close(tsc, TR7__PORT__PORT(p), flag);
}

static void finalize_port(tr7_engine_t tsc, tr7_cell_t a)
{
   port_rep_free(tsc, TR7_CELL_PORT__PORT_(a));
}

static tr7_t mk_port_cond(tr7_engine_t tsc, port_t * pt)
{
   return pt ? mk_port(tsc, pt) : TR7_FALSE;
}

static tr7_t port_from_file(tr7_engine_t tsc, FILE * f, const char *fn, unsigned prop)
{
   return mk_port_cond(tsc, port_rep_from_file(tsc, f, fn, prop));
}

static tr7_t port_from_string(tr7_engine_t tsc, tr7_t string, uint8_t *start, uint8_t *end)
{
   return mk_port_cond(tsc, port_rep_from_string(tsc, string, start, end));
}

static tr7_t port_from_bytevector(tr7_engine_t tsc, tr7_t bytevec)
{
   return mk_port_cond(tsc, port_rep_from_bytevector(tsc, bytevec));
}

static tr7_t port_from_scratch(tr7_engine_t tsc, unsigned prop)
{
   return mk_port_cond(tsc, port_rep_from_scratch(tsc, prop));
}

static tr7_t port_get_string(tr7_engine_t tsc, tr7_t p)
{
   port_t *pt = TR7__PORT__PORT(p);
   return port_rep_get_string(tsc, pt);
}

static tr7_t port_get_bytevector(tr7_engine_t tsc, tr7_t p)
{
   port_t *pt = TR7__PORT__PORT(p);
   return port_rep_get_bytevector(tsc, pt);
}

/*
**************************************************************************
*
* Routines for loading
*/

static void load_set(tr7_engine_t tsc, tr7_t port, tr7_play_t playflags, tr7_t loadenv)
{
   tsc->loadenv = loadenv;
   tsc->loadport = port;
   tsc->playflags = playflags;
}

static int load_enter(tr7_engine_t tsc, tr7_t port, tr7_play_t playflags)
{
   tr7_t e = TR7_CONS3(tsc, tsc->loadport, TR7_FROM_UINT(tsc->playflags), tsc->loadenv);
   load_set(tsc, port, playflags, e);
   return 1;
}

static int load_leave(tr7_engine_t tsc)
{
   unsigned playflags;
   tr7_t p1, p2;

   p1 = tsc->loadenv;
   if (!TR7_IS_PAIR(p1))
      return 0;
   p2 = TR7_CDR(p1);
   if (!TR7_IS_PAIR(p2))
      return 0;

   playflags = (unsigned)TR7_TO_UINT(TR7_CAR(p2));
   load_set(tsc, TR7_CAR(p1), playflags, TR7_CDR(p2));
   return 1;
}

static int load_enter_for_file(tr7_engine_t tsc, FILE * f, const char *fn, tr7_play_t playflags)
{
   tr7_t p = port_from_file(tsc, f, fn, port_input | port_textual);
   return TR7_IS_FALSE(p) ? 0 : load_enter(tsc, p, playflags);
}

static int load_enter_string(tr7_engine_t tsc, uint8_t *start, uint8_t *end, tr7_play_t playflags)
{
   tr7_t p = port_from_string(tsc, TR7_NIL, start, end);
   return TR7_IS_FALSE(p) ? 0 : load_enter(tsc, p, playflags);
}

static int load_enter_search(
   tr7_engine_t tsc,
   const char *basename, unsigned baselength,
   tr7_strid_t idpath, tr7_play_t playflags)
{
   char path[PATH_MAX + 1];
   unsigned found = search_access_file(tsc,
            basename, baselength ? baselength : (unsigned)strlen(basename),
            path, sizeof path,
            suffixes_scheme, (int)(sizeof suffixes_scheme / sizeof *suffixes_scheme),
            tsc->strings[idpath]);
   return found ? load_enter_for_file(tsc, NULL, path, playflags) : 0;
}

static int load_enter_search_import(tr7_engine_t tsc, const char *basename, unsigned baselength)
{
   return load_enter_search(tsc, basename, baselength, Tr7_StrID_Library_Path, 0);
}

static int load_enter_search_load(tr7_engine_t tsc, FILE * f, const char *fn, tr7_play_t playflags)
{
   return f == NULL
             ? load_enter_search(tsc, fn, 0, Tr7_StrID_Path, playflags)
             : load_enter_for_file(tsc, f, fn, playflags);
}

static int load_enter_search_include(tr7_engine_t tsc, const char *basename, tr7_play_t playflags)
{
   return load_enter_search(tsc, basename, 0, Tr7_StrID_Include_Path, playflags);
}

/*
**************************************************************************
*
*/
/* ========== atoms implementation  ========== */

static int string_to_int(const char *s, size_t length, tr7_int_t *resu, unsigned base)
{
   const char *end = &s[length];
   signed char c;
   tr7_int_t x = 0;
   while(s != end) {
      c = *(const signed char*)s++;
      if (c != '_') {
         c -= '0';
         if (c < 0)
            return 0;
         if (c > 9) {
            c -= 'a' - '0';
            if (c < 0 || c > 25) {
               c += 'a' - 'A';
               if (c < 0 || c > 25)
                  return -1;
            }
            c += 10;
         }
         if ((unsigned)c >= base
          || overflow_mul(x, (tr7_int_t)base, &x)
          || overflow_add(x, (tr7_int_t)c, &x))
            return -1;
      }
   }
   *resu = x;
   return 0;
}

static int decode_integer_string(tr7_engine_t tsc, const char *s, size_t length, tr7_t *result, unsigned base)
{
   tr7_int_t x;
   int rc = string_to_int(s, length, &x, base);
   *result = rc ? TR7_NIL : tr7_from_int(tsc, x); /* TODO TR7_NIL? */
   return rc;
}

static tr7_char_t decode_character(const char *s, size_t length)
{
   tr7_char_t c;
   char f = *s;
   unsigned lf = xtf8_length((uint8_t)f);
   if (lf == 0 || lf > length)
      c = TR7_CHAR_EOF;
   else if (lf == length)
      utf8_to_char((uint8_t*)s, &c);
   else if (f == 'x') {
      tr7_int_t x;
      int rc = string_to_int(s + 1, length - 1, &x, 16);
      c = rc < 0 || (tr7_uint_t)x > (tr7_uint_t)TR7_CHAR_MAX ? TR7_CHAR_EOF : (tr7_char_t)x;
   }
   else {
      switch(length) {
      case 3:
         if (!memcmp(s, "tab", 3))
            return '\t';
         break;
      case 4:
         if (!memcmp(s, "null", 4))
            return 0;
         break;
      case 5:
         if (!memcmp(s, "space", 5))
            return ' ';
         if (!memcmp(s, "alarm", 5))
            return '\x07';
         break;
      case 6:
         if (!memcmp(s, "escape", 6))
            return '\x1b';
         if (!memcmp(s, "return", 6))
            return '\r';
         if (!memcmp(s, "delete", 6))
            return '\x7f';
         break;
      case 7:
         if (!memcmp(s, "newline", 7))
            return '\n';
         break;
      case 9:
         if (!memcmp(s, "backspace", 9))
            return '\x08';
         break;
      }
      if (!get_control_code(s, &c, length))
         c = TR7_CHAR_EOF;
   }
   return c;
}

/* make symbol or number atom from string */
static tr7_t mk_atom(tr7_engine_t tsc, const char *q, size_t length)
{
   char c;
   int has_dec_point = 0;
   int has_fp_exp = 0;
   char low[STRBUFFSIZE];
   size_t off = 0;

   if (tsc->playflags & Tr7_Play_Fold_Case) {
      if (length == 0 || length >= STRBUFFSIZE)
         goto symbol; /* bad but ... */
      q = fold(low, q, &length);
   }

   c = q[off++];
   if ((c == '+') || (c == '-')) {
      if (length == off)
         goto symbol;
      if (length == 6 && strncmp(&q[1], "nan.0", 5) == 0)
         return tr7_from_double(tsc, NAN);
      if (length == 6 && strncmp(&q[1], "inf.0", 5) == 0)
         return tr7_from_double(tsc, c == '-' ? -INFINITY : INFINITY);
      c = q[off++];
   }
   if (c == '.') {
      if (length == off)
         goto symbol;
      has_dec_point = 1;
      c = q[off++];
   }

   if (!isdigit(c))
      goto symbol;

   while (off < length) {
      c = q[off++];
      if (!isdigit(c)) {
         if (c == '.') {
            if (has_dec_point)
               goto symbol;
            has_dec_point = 1;
         }
         else if ((c == 'e') || (c == 'E')) {
            if (has_fp_exp || length == off)
               goto symbol;
            c = q[off];
            if (c == '-' || c == '+') {
               if (length == ++off)
                  goto symbol;
            }
            has_fp_exp = 1;
         }
         else
            goto symbol;
      }
   }
   if (has_dec_point || has_fp_exp)
      return tr7_from_double(tsc, atof(q)); /* TODO: length ? */
   return tr7_from_int(tsc, atol(q)); /* TODO: length ? */

symbol:
   return tr7_get_symbol_length(tsc, q, length, 1);
}

/* make constant */
static int mk_sharp_const(tr7_engine_t tsc, tr7_t *resu, const char *name, size_t length)
{
   tr7_char_t c;
   long x;
   char low[STRBUFFSIZE];

   if (tsc->playflags & Tr7_Play_Fold_Case)
      name = fold(low, name, &length);

   switch (name[0]) {
   case 't':
      if (length == 1 || (length == 4 && memcmp(&name[1], "rue", 3) == 0)) {
         *resu = TR7_TRUE;
         return 0;
      }
      goto bad;
   case 'f':
      if (length == 1 || (length == 5 && memcmp(&name[1], "alse", 4) == 0)) {
         *resu = TR7_FALSE;
         return 0;
      }
      goto bad;
   case 'e': /* #e (exact) */
      if (name[1] == '#')
         return mk_sharp_const(tsc, resu, &name[2], length - 2);
      /* TODO: process decimals as real exacts */
      /* TODO: processe exponents 5e10 is exact no? */
      return decode_integer_string(tsc, name + 1, length - 1, resu, 10);
   case 'i': /* #i (inexact) */
      /* TODO: process integers as inexacts */
      if (name[1] == '#') {
         int s = mk_sharp_const(tsc, resu, &name[2], length - 2);
         if (s == 0)
            *resu = tr7_from_double(tsc, (double)TR7_TO_INT(*resu));
         return s;
      }
      *resu = tr7_from_double(tsc, atof(&name[1])); /* TODO: length ? */
      return 0;
   case 'o': /* #o (octal) */
      return decode_integer_string(tsc, name + 1, length - 1, resu, 8);
   case 'd': /* #d (decimal) */
      return decode_integer_string(tsc, name + 1, length - 1, resu, 10);
      break;
   case 'x': /* #x (hexadecimal) */
      return decode_integer_string(tsc, name + 1, length - 1, resu, 16);
   case 'b': /* #b (binary) */
      return decode_integer_string(tsc, name + 1, length - 1, resu, 2);
   case '\\': /* #\ (character) */
      c = decode_character(name + 1, length - 1);
      if (c != TR7_CHAR_EOF) {
         *resu = TR7_FROM_CHAR(c);
         return 0;
      }
      /*@fallthrough@*/
   default:
bad:  *resu = TR7_NIL;
      return -1;
   }
   *resu = tr7_from_int(tsc, x);
   return 0;
}
/*
**************************************************************************
* SECTION STRBUFF - strbuff is used for buffering characters in UTF8
* ---------------
*
* start buffering in strbuff
*/
static void strbuff_start(tr7_engine_t tsc)
{
   tsc->strbuff.length = 0;
   if (tsc->strbuff.head != tsc->strbuff.buffer) {
      free(tsc->strbuff.head);
      tsc->strbuff.head = tsc->strbuff.buffer;
      tsc->strbuff.size = (unsigned)sizeof tsc->strbuff.buffer;
   }
}
/*
* stop buffering of strbuff
* appends a zero at tail without increasing the length
* returns 1 if okay or zero otherwise
*/
static int strbuff_ensure(tr7_engine_t tsc, unsigned count)
{
   void *newhead;
   unsigned req = tsc->strbuff.length + count;
   unsigned sz = tsc->strbuff.size;
   if (req > sz) {
      do { sz <<= 1; } while (req > sz);
      newhead = memalloc(tsc, sz);
      if (newhead == NULL)
         return 0;
      memcpy(newhead, tsc->strbuff.head, tsc->strbuff.length);
      if (tsc->strbuff.head != tsc->strbuff.buffer)
         memfree(tsc, tsc->strbuff.head);
      tsc->strbuff.head = newhead;
      tsc->strbuff.size = sz;
   }
   return 1;
}
/*
* stop buffering of strbuff
* appends a zero at tail without increasing the length
* returns 1 if okay or zero otherwise
*/
static int strbuff_stop(tr7_engine_t tsc)
{
   int ret = strbuff_ensure(tsc, 1);
   if (ret)
      tsc->strbuff.head[tsc->strbuff.length] = 0;
   return ret;
}
/*
* removes the spaces at tail and then stops (see strbuff_stop)
*/
static int strbuff_stop_trim(tr7_engine_t tsc)
{
   while (tsc->strbuff.length && isspace(tsc->strbuff.head[tsc->strbuff.length - 1]))
      tsc->strbuff.length--;
   return strbuff_stop(tsc);
}
/*
* add the character 'car' to the current strbuff
* returns 1 if okay or zero otherwise
*/
static int strbuff_add(tr7_engine_t tsc, tr7_char_t car)
{
   int ret = strbuff_ensure(tsc, UTF8BUFFSIZE);
   if (ret) {
      unsigned len = tsc->strbuff.length;
      len += char_to_utf8(car, (uint8_t*)&tsc->strbuff.head[len]);
      tsc->strbuff.length = len;
   }
   return ret;
}
/*
* get head of the current strbuff
*/
static const char *strbuff_head(tr7_engine_t tsc)
{
   return tsc->strbuff.head;
}
/*
* get length in bytes (utf8) of the current strbuff (without terminating zero)
*/
static unsigned strbuff_length(tr7_engine_t tsc)
{
   return tsc->strbuff.length;
}
/*
* get a new TR7 string for the current strbuff
*/
static tr7_t strbuff_string(tr7_engine_t tsc)
{
   tr7_t res;
   if (tsc->strbuff.head == tsc->strbuff.buffer)
      res = tr7_make_string_copy_length(tsc, tsc->strbuff.head, tsc->strbuff.length);
   else {
      res = tr7_make_string_take_length(tsc, tsc->strbuff.head, tsc->strbuff.length);
      tsc->strbuff.head = tsc->strbuff.buffer;
      tsc->strbuff.size = (unsigned)sizeof tsc->strbuff.buffer;
   }
   return res;
}
/*
**************************************************************************
* SECTION LINE_TRACK - line tracking
* ------------------
*
* Line tracking is done using a-list of starts of lines.
*
* Example: if the parser encountered the following s-expr:
*
*   10: (define (square x)
*   11:    (* x x))
*
* the line tracking will be:
*
*   (((define (square x) #1=(* x x)) . 10) (#1# . 11))
*
* This coding is chosen for its short use of memory
* in soite of its low performance on queries.
*/
#if USE_TR7_DEBUG && DEBUG_LINES
/*
* An item can be tagged only if it is a pointeur and that
* this pointer is not shared. Symbols are shared by nature.
*/
#define LINE_START_ACCEPT(item) (TR7_IS_PTR(item) && !TR7_IS_SYMBOL(item))
/*
* This is the forward process, mark an item as soon as it is on
* a new line: mark it as beginning the line.
*/
static void line_starts_update(tr7_engine_t tsc, port_t *pt, tr7_t item)
{
   if (LINE_START_ACCEPT(item) && pt->line != tsc->last_line) {
      tr7_t pair = tr7_cons(tsc, item, TR7_FROM_INT(tsc->last_line = pt->line));
      tsc->line_starts = tr7_cons(tsc, pair, tsc->line_starts);
   }
}
/*
* The forward process above is not enougth. This function allows to
* reserve a place for storing the line if later item is created.
* The holder furtherly set using line_starts_holder_set or dropped
* using line_starts_holder_drop.
*/
static tr7_t line_starts_holder_get(tr7_engine_t tsc, port_t *pt)
{
   tr7_t pair = TR7_VOID;
   if (pt->line != tsc->last_line) {
      pair = tr7_cons(tsc, TR7_VOID, TR7_FROM_INT(tsc->last_line = pt->line));
      tsc->line_starts = tr7_cons(tsc, pair, tsc->line_starts);
   }
   return pair;
}
/*
* Drop the previously reserved holder
*/
static void line_starts_holder_drop(tr7_engine_t tsc, tr7_t holder)
{
   if (TR7_IS_PAIR(holder)) {
      tr7_t *iter = &tsc->line_starts;
      for(;;) {
         tr7_t pair = *iter;
         if (!TR7_IS_PAIR(pair))
            break;
         if (TR7EQ(holder, TR7_CAR(pair))) {
            *iter = TR7_CDR(pair);
            break;
         }
         iter = &TR7_CDR(pair);
      }
   }
}
/*
* Set the previously reserved holder with the given item
*/
static void line_starts_holder_set(tr7_engine_t tsc, tr7_t holder, tr7_t item)
{
   if (TR7_IS_PAIR(holder)) {
      if (LINE_START_ACCEPT(item))
         TR7_CAR(holder) = item;
      else
         line_starts_holder_drop(tsc, holder);
   }
}
#endif


/*
**************************************************************************
*
*/
/* read string like expression (")xxx...xxx" or (|)xxx...xxx| */
static int read_str_like_exp_port(tr7_engine_t tsc, port_t *pt, tr7_char_t term)
{
   tr7_char_t car;
   int num;

   strbuff_start(tsc);
   for (;;) {
      car = port_read_char(tsc, pt);
      if (car == TR7_CHAR_EOF)
         return 0;
      if (car == term)
         return strbuff_stop(tsc);
      if (car == '\\') {
escaping:
         car = port_read_char(tsc, pt);
         if (car == TR7_CHAR_EOF)
            return 0;
         switch (car) {
         case ' ':
         case '\t':
         case '\r':
            do {
               car = port_read_char(tsc, pt);
            } while(car == ' ' || car == '\t' || car == '\r');
            if (car != '\n')
               return 0;
            /*@fallthrough@*/
         case '\n':
#if USE_TR7_DEBUG && DEBUG_LINES
            if (car == '\n')
               pt->line++;
#endif
            do {
               car = port_read_char(tsc, pt);
            } while(car == ' ' || car == '\t' || car == '\r');
            if (car == TR7_CHAR_EOF)
               return 0;
            if (car == term)
               return strbuff_stop(tsc);
            if (car == '\\')
               goto escaping;
            break;
         case 'n':
            car = '\n';
            break;
         case 't':
            car = '\t';
            break;
         case 'r':
            car = '\r';
            break;
         case 'x':
         case 'X':
            num = 0;
            do {
               car = port_read_char(tsc, pt);
               if (car <= '9' && car >= '0')
                  num = (num << 4) + car - '0';
               else if (car <= 'F' && car >= 'A')
                  num = (num << 4) + car - 'A' + 10;
               else if (car <= 'f' && car >= 'a')
                  num = (num << 4) + car - 'a' + 10;
               else if (car != ';')
                  return 0;
            } while(car != ';');
            car = (tr7_char_t)num;
            break;
         default:
            break;
         }
      }
      if (!strbuff_add(tsc, car))
         return 0;
   }
}

/*
 * read started (#!) directives
 * return -1 if error or if OK, 1 if terminated with '\n' or 0 if terminated with EOF
 */
static int read_directive(tr7_engine_t tsc, port_t *pt)
{
   tr7_char_t car;

   strbuff_start(tsc);
   if (strbuff_add(tsc, '#') && strbuff_add(tsc, '!'))
      do {
         car = port_read_char(tsc, pt);
         if (car == '\n' || car == TR7_CHAR_EOF) {
            if (!strbuff_stop_trim(tsc))
               break;
#if USE_TR7_DEBUG && DEBUG_LINES
            if (car == '\n')
               pt->line++;
#endif
            return car == '\n';
         }
      } while(strbuff_add(tsc, car));
   return -1;
}

/* read string expression "xxx...xxx" */

static int skip_block_comment_port(tr7_engine_t tsc, port_t *pt)
{
   tr7_char_t car;
   int state = 0, nest = 1;
   for(;;) {
      car = port_read_char(tsc, pt);
      switch (car) {
      case TR7_CHAR_EOF:
         return 1;
      case '|':
         if (state != '#')
            state = car;
         else {
            nest++;
            state = 0;
         }
         break;
      case '#':
         if (state != '|')
            state = car;
         else {
            if (0 == --nest)
               return 0;
            state = 0;
         }
         break;
      default:
#if USE_TR7_DEBUG && DEBUG_LINES
         if (car == '\n')
            pt->line++;
#endif
         state = 0;
         break;
      }
   }
}

static int is_a_delimiter(tr7_char_t car)
{
   switch (car) {
   case '(':
   case ')':
   case '"':
   case ';':
   case '\f':
   case '\t':
   case '\v':
   case '\n':
   case '\r':
   case ' ':
      return 1;
   default:
      return 0;
   }
}
/*
* read non standard sharp expressions
*/
static token_type_t read_other_sharp_cont(tr7_engine_t tsc, port_t *pt, tr7_char_t car)
{
   for(;;) {
      if (car == TR7_CHAR_EOF || is_a_delimiter(car)) {
         if (!strbuff_stop(tsc))
            break;
         if (car != TR7_CHAR_EOF)
            port_unread_char(tsc, pt, car);
         tsc->read_value = strbuff_string(tsc);
         return Token_Sharp;
      }
      if (!strbuff_add(tsc, car))
         break;
      car = port_read_char(tsc, pt);
   }
   return Token_Error; /* oom */
}
static int read_other_sharp_begin(tr7_engine_t tsc)
{
   strbuff_start(tsc);
   return strbuff_add(tsc, '#');
}
static token_type_t read_other_sharp(tr7_engine_t tsc, port_t *pt, tr7_char_t car)
{
   return read_other_sharp_begin(tsc)
               ? read_other_sharp_cont(tsc, pt, car) : Token_Error;
}

static token_type_t skip_directive_port(tr7_engine_t tsc, port_t *pt)
{
   static const char nfc[] = "no-fold-case";
#if HAS_GREEDY_SYNTAX
   static const char nsg[] = "no-greedy-syntax";
#endif
   int sts = read_directive(tsc, pt);
   if (sts < 0)
      return Token_Error;
   if (!strcmp(&strbuff_head(tsc)[2], nfc))
      tsc->playflags &= ~(unsigned)Tr7_Play_Fold_Case;
   else if (!strcmp(&strbuff_head(tsc)[2], &nfc[3]))
      tsc->playflags |= Tr7_Play_Fold_Case;
#if HAS_GREEDY_SYNTAX
   else if (!strcmp(&strbuff_head(tsc)[2], nsg))
      tsc->no_greedy_syntax = 1;
   else if (!strcmp(&strbuff_head(tsc)[2], &nsg[3]))
      tsc->no_greedy_syntax = 0;
#endif
   else {
      tsc->read_value = strbuff_string(tsc);
      return Token_Sharp;
   }
   return sts ? Token_Comment : Token_EOF;
}

/* get token */
static token_type_t read_token(tr7_engine_t tsc, port_t *pt)
{
   tr7_char_t car;
   int idx, res;
   token_type_t toktyp;
   for(;;) {
      car = port_read_char(tsc, pt);
      switch (car) {
      case TR7_CHAR_EOF:
         return Token_EOF;
      case '\n':
#if USE_TR7_DEBUG && DEBUG_LINES
         pt->line++;
         /*@fallthrough@*/
#endif
      case '\r':
      case '\t':
      case ' ':
         break;
      case ';':
         while ((car = port_read_char(tsc, pt)) != '\n')
            if (car == TR7_CHAR_EOF)
               return Token_EOF;
#if USE_TR7_DEBUG && DEBUG_LINES
         pt->line++;
#endif
         break;
      case '(':
         return Token_Left_Par;
      case ')':
         return Token_Right_Par;
      case '\'':
         return Token_Quote;
      case '`':
         return Token_Back_Quote;
      case ',':
         if ((car = port_read_char(tsc, pt)) == '@')
            return Token_At;
         port_unread_char(tsc, pt, car);
         return Token_Comma;
      case '|':
      case '"':
         if (!read_str_like_exp_port(tsc, pt, car))
            return Token_Error;
         if (car == '"')
            tsc->read_value = strbuff_string(tsc);
         else
            tsc->read_value = tr7_get_symbol_length(tsc, strbuff_head(tsc), strbuff_length(tsc), 1);
#if USE_TR7_DEBUG && DEBUG_LINES
            line_starts_update(tsc, pt, tsc->read_value);
#endif
         return Token_Value;
      case '#':
         car = port_read_char(tsc, pt);
         switch (car) {
         case ';':
            return Token_Comment_Datum;
         case '(':
            return Token_Vector;
         case '|':
            if (skip_block_comment_port(tsc, pt))
               return Token_EOF;
            break;
         case '!':
            toktyp = skip_directive_port(tsc, pt);
            if (toktyp != Token_Comment)
               return toktyp;
            break;
         case 'u':
            idx = 0;
            car = port_read_char(tsc, pt);
            if (car == '8') {
               car = port_read_char(tsc, pt);
               if (car == '(')
                  return Token_Byte_Vector;
               idx = 1;
            }
            return read_other_sharp_begin(tsc) && (!idx || strbuff_add(tsc, '8'))
                     ? read_other_sharp_cont(tsc, pt, car) : Token_Error;
         case 'e':
         case 'i':
         case 't':
         case 'f':
         case 'o':
         case 'd':
         case 'x':
         case 'b':
         case '\\':
            strbuff_start(tsc);
            while (car != TR7_CHAR_EOF && !is_a_delimiter(car)) {
               if (!strbuff_add(tsc, car))
                  return Token_Error;
               car = port_read_char(tsc, pt);
            }
            if (strbuff_head(tsc)[0] != '\\' || strbuff_length(tsc) != 1)
               port_unread_char(tsc, pt, car);
            else if (car == TR7_CHAR_EOF || !strbuff_add(tsc, car))
               return Token_Error;
            if (!strbuff_stop(tsc))
               return Token_Error;
            res = mk_sharp_const(tsc, &tsc->read_value, strbuff_head(tsc), strbuff_length(tsc));
#if USE_TR7_DEBUG && DEBUG_LINES
            line_starts_update(tsc, pt, tsc->read_value);
#endif
            return res < 0 ? Token_Error : Token_Value;
         default:
            if (car < '0' || car > '9')
               return read_other_sharp(tsc, pt, car);
            idx = (int)(car - '0');
            for(;;) {
               car = port_read_char(tsc, pt);
               if (car >= '0' && car <= '9')
                  idx = 10 * idx + (int)(car - '0');
               else if (car != '=' && car != '#')
                  return Token_Error;
               else if (car == '=' || car == '#') {
                  tsc->read_value = TR7_FROM_INT(idx);
                  return car == '=' ? Token_Datum_Set : Token_Datum_Ref;
               }
            }
         }
         break;
      default:
         strbuff_start(tsc);
         do {
            if (!strbuff_add(tsc, car))
               return Token_Error;
            car = port_read_char(tsc, pt);
         } while (car != TR7_CHAR_EOF && !is_a_delimiter(car));
         port_unread_char(tsc, pt, car);
         if (!strbuff_stop(tsc))
            return Token_Error;
         if (strbuff_length(tsc) == 1 && *strbuff_head(tsc) == '.')
            return Token_Dot;
         tsc->read_value = mk_atom(tsc, strbuff_head(tsc), strbuff_length(tsc));
#if USE_TR7_DEBUG && DEBUG_LINES
         line_starts_update(tsc, pt, tsc->read_value);
#endif
         return Token_Value;
      }
   }
}

/*
**************************************************************************
* Basic number formatting to be improved if needed (avoid copy, big nums)
*/

static unsigned format_int(tr7_engine_t tsc, char *buffer, unsigned length, unsigned fbase, tr7_int_t num)
{
   char car;
   char buf[140]; /* enough for 128 bits */
   char *str;
   int sig, dig;
   unsigned base, lenstr;
   tr7_uint_t val;

   base = fbase < 2 || fbase > 36 ? 10 : fbase;
   if (num < 0) {
      sig = 1;
      val = (tr7_uint_t)-num;
   }
   else {
      sig = 0;
      val = (tr7_uint_t)num;
   }
   str = &buf[sizeof buf];
   *--str = 0;
   switch(base) {
   case 2:
      do {
         car = (char)('0' + (int)(val & 1));
         val >>= 1;
         *--str = car;
      } while(val);
      break;
   case 8:
      do {
         car = (char)('0' + (int)(val & 7));
         val >>= 3;
         *--str = car;
      } while(val);
      break;
   case 16:
      do {
         dig = (int)(val & 15);
         val >>= 4;
         car = DIGIT2CHAR(dig);
         *--str = car;
      } while(val);
      break;
   default:
      do {
         dig = (int)(val % base);
         val /= base;
         car = DIGIT2CHAR(dig);
         *--str = car;
      } while(val);
      break;
   }
   if (sig)
         *--str = '-';
   lenstr = (unsigned)(sizeof buf - 1 - (unsigned)(str - buf));
   if (length)
      memcpy(buffer, str, lenstr < length ? lenstr + 1 : length);
   return lenstr;
}

static unsigned format_double(tr7_engine_t tsc, char *buffer, unsigned length, unsigned fbase, double num)
{
   char buf[20];
   const char *str;
   unsigned lenstr;

   if (fbase != 10 && fbase > 1 && fbase <= 36)
      return format_int(tsc, buffer, length, fbase, (tr7_int_t)num); /* hum, TODO checks */

   if (isnan(num)) {
      str = "+nan.0";
      lenstr = 6;
   }
   else if (!isfinite(num)) {
      str = num < 0 ? "-inf.0" : "+inf.0";
      lenstr = 6;
   }
   else {
      str = buf;
      lenstr = (unsigned)snprintf(buf, sizeof buf, "%.16g", num);
      /* r5rs says there must be a '.' (unless 'e'?) */
      if (lenstr == (unsigned)strcspn(buf, ".e")) {
         buf[lenstr++] = '.';      /* not found, so add '.0' at the end */
         buf[lenstr++] = '0';
         buf[lenstr] = 0;
      }
   }
   if (length)
      memcpy(buffer, str, lenstr < length ? lenstr + 1 : length);
   return lenstr;
}

static unsigned format_number(tr7_engine_t tsc, char *buffer, unsigned length, unsigned fbase, tr7_t num)
{
   switch (TR7_TAG(num)) {
   case TR7_TAG_EINT:
   case TR7_TAG_OINT:
      return format_int(tsc, buffer, length, fbase, TR7_TO_INT(num));
   case TR7_TAG_DOUBLE:
      return format_double(tsc, buffer, length, fbase, *TR7_TO_DOUBLE(num));
   default:
      return 0;
   }
}
/*
**************************************************************************
* SECTION WRITING
* ===============
*
* scan items for its loops and fulfil the set of anchored values
* accordingly to the exploration trail
*/
static tr7_t do_scan_loops(tr7_engine_t tsc, tr7_t item, tr7_t anchors, tr7_t trail)
{
   tr7_vector_t vec;
   tr7_uint_t idx, cnt;

   /* is item to be checked? */
   if (TR7_IS_PAIR(item)
#if DUMP_LAMBDAS
    || TR7_IS_LAMBDA(item)
#if USE_SCHEME_CASE_LAMBDA
    || TR7_IS_CASE_LAMBDA(item)
#endif
#endif
    /* TODO RECORDS */
    || TR7_IS_VECTOR(item)) {
      /* yes, to be checked.
       * is it in the trail? */
      if (tr7_memq_pair(item, trail) != NULL) {
         /* in the trail.
          * add to detected anchors if not already done */
         if (tr7_assq_pair(item, anchors) == NULL)
            anchors = tr7_cons(tsc, tr7_cons(tsc, item, TR7_FROM_INT(0)), anchors);
      }
      else {
         /* not in the trail.
          * update the trail to include this item */
#if INTPTR_MAX == INT32_MAX
         tr7_t here[3];
         tr7_pair_t hpair = (tr7_pair_t)&here[0];
         if (!TR7_IS_PAIR(TR7_FROM_PAIR(hpair)))
            hpair = (tr7_pair_t)&here[1];
#else
         struct tr7_pair here;
         tr7_pair_t hpair = &here;
#endif
         TR7_PAIR_CAR(hpair) = item;
         TR7_PAIR_CDR(hpair) = trail;
         trail = TR7_FROM_PAIR(hpair);
         /* recursive scan of content of item with current trail */
         if (TR7_IS_PAIR(item)) {
            anchors = do_scan_loops(tsc, TR7_CAR(item), anchors, trail);
            anchors = do_scan_loops(tsc, TR7_CDR(item), anchors, trail);
         }
#if DUMP_LAMBDAS
#if USE_SCHEME_CASE_LAMBDA
         else if (TR7_IS_LAMBDA(item) || TR7_IS_CASE_LAMBDA(item)) {
#else
         else if (TR7_IS_LAMBDA(item)) {
#endif
            anchors = do_scan_loops(tsc, TR7_TO_CLOSURE(item)->description, anchors, trail);
#if DUMP_CLOSURES
            anchors = do_scan_loops(tsc, TR7_TO_CLOSURE(item)->upperframes, anchors, trail);
#endif
         }
#endif
         else {
            vec = TR7_TO_VECTOR(item);
            cnt = TR7_HEAD_UVALUE(TR7_CELL_HEAD(vec));
            for(idx = 0 ; idx < cnt ; idx++)
               anchors = do_scan_loops(tsc, vec->items[idx], anchors, trail);
         }
      }
   }
   return anchors;
}
/*
* scan the the content of item and return a list made
* of pairs (x . 0) where x is a detected loops value.
*/
static tr7_t scan_loops(tr7_engine_t tsc, tr7_t item)
{
   return do_scan_loops(tsc, item, TR7_NIL, TR7_NIL);
}
/*
* scan the the content of item and fulfil the set of detected
* values as pairs of (x . occur) where x is a detected value
* and occur its count of detected occurence minus one
*/
static tr7_t do_scan_shareds(tr7_engine_t tsc, tr7_t item, tr7_t set)
{
   tr7_vector_t vec;
   tr7_uint_t idx, cnt;

   /* is item to be checked? */
   if (TR7_IS_PAIR(item)
    || TR7_IS_STRING(item)
    || TR7_IS_PORT(item)
    || TR7_IS_BYTEVECTOR(item)
#if DUMP_LAMBDAS
    || TR7_IS_LAMBDA(item)
#if USE_SCHEME_CASE_LAMBDA
    || TR7_IS_CASE_LAMBDA(item)
#endif
#endif
    || IS_BOX(item)
    || TR7_IS_RECORD(item)
    || TR7_IS_VECTOR(item)) {
      /* yes, to be checked.
       * search it in the set */
      tr7_pair_t p = tr7_assq_pair(item, set);
      if (p != NULL)
         /* found in the set, increase its count */
         TR7_PAIR_CDR(p) = TR7_FROM_INT(TR7_TO_INT(TR7_PAIR_CDR(p)) + 1);
      else {
         /* not found, add to the set */
         set = tr7_cons(tsc, tr7_cons(tsc, item, TR7_FROM_INT(0)), set);
         /* recursive scan of content of item with current set */
         if (TR7_IS_PAIR(item)) {
            set = do_scan_shareds(tsc, TR7_CAR(item), set);
            set = do_scan_shareds(tsc, TR7_CDR(item), set);
         }
#if DUMP_LAMBDAS
#if USE_SCHEME_CASE_LAMBDA
         else if (TR7_IS_LAMBDA(item) || TR7_IS_CASE_LAMBDA(item)) {
#else
         else if (TR7_IS_LAMBDA(item)) {
#endif
            set = do_scan_shareds(tsc, TR7_TO_CLOSURE(item)->description, set);
#if DUMP_CLOSURES
            set = do_scan_shareds(tsc, TR7_TO_CLOSURE(item)->upperframes, set);
#endif
         }
#endif
         else if (IS_BOX(item)
              || TR7_IS_RECORD(item)
              || TR7_IS_VECTOR(item)) {
            vec = TR7_TO_VECTOR(item);
            cnt = TR7_HEAD_UVALUE(TR7_CELL_HEAD(vec));
            idx = TR7_IS_RECORD(item) ? 1 : 0;
            for( ; idx < cnt ; idx++)
               set = do_scan_shareds(tsc, vec->items[idx], set);
         }
      }
   }
   return set;
}
/*
* scan the the content of item and return a list made
* of pairs (x . 0) where x is a detected shared value.
*/
static tr7_t scan_shareds(tr7_engine_t tsc, tr7_t item)
{
   tr7_t entries = do_scan_shareds(tsc, item, TR7_NIL);
   tr7_t anchors = TR7_NIL;
   /* removes the entries with single occurency */
   while (!TR7_IS_NIL(entries)) {
      tr7_t next = TR7_CDR(entries);
      if (TR7_TO_INT(TR7_CDAR(entries)) != 0) {
         TR7_CDAR(entries) = TR7_FROM_INT(0);
         TR7_CDR(entries) = anchors;
         anchors = entries;
      }
      entries = next;
   }
   return anchors;
}

static int is_simple_symbol(const char *str, unsigned len)
{
   int sta = 0;
   unsigned idx = 0;
   while (idx < len) {
      tr7_char_t c;
      unsigned n = utf8_to_char((const uint8_t *)&str[idx], &c);
      if (n == 0)
         break;
      if (n == 1) {
         if (isalpha(c))
            sta = 2;
         else if (c == '.')
            sta++;
         else if (c < '0' || c > '9') {
            if (!strchr("!$%&*+-/:<=>?@^_~", c))
               return 0;
            sta = 2;
         }
      } else if (iswalpha((wint_t)c))
         sta = 2;
      else
         break;
      idx += n;
   }
   return sta > 1;
}

static void print_esc_string(tr7_engine_t tsc, port_t *pt, const char *p, unsigned len, char delim)
{
   unsigned i, n;
   char d, c, buf[5];

   buf[0] = '\\';
   port_write_char(tsc, pt, delim);
   while (len) {
      n = i = 0;
      while(i < len && n == 0) {
         c = p[i];
         switch (c) {
         case '\n':
            buf[1] = 'n';
            n = 2;
            break;
         case '\t':
            buf[1] = 't';
            n = 2;
            break;
         case '\r':
            buf[1] = 'r';
            n = 2;
            break;
         default:
            if (delim != c) {
               if ((c >= ' ' && c < 0x7f) || c < 0)
                  i++;
               else {
                  buf[1] = 'x';
                  d = (c >> 4) & 15;
                  buf[2] = (char)((d > 9 ? ('A' - 10) : '0') + d);
                  d = c & 15;
                  buf[3] = (char)((d > 9 ? ('A' - 10) : '0') + d);
                  buf[4] = ';';
                  n = 5;
               }
               break;
            }
            /*@fallthrough@*/
         case '\\':
            buf[1] = c;
            n = 2;
            break;
         }
      }
      if (i)
         port_write_utf8_length(tsc, pt, p, i);
      if (n) {
         port_write_utf8_length(tsc, pt, buf, n);
         i++;
      }
      p += i;
      len -= i;
   }
   port_write_char(tsc, pt, delim);
}

static void write_character(tr7_engine_t tsc, port_t *pt, tr7_char_t car)
{
   char buf[UTF8BUFFSIZE + 2];
   const char *str;
   unsigned lenstr;
   switch (car) {
   case ' ':
      str = "#\\space";
      lenstr = 7;
      break;
   case '\n':
      str = "#\\newline";
      lenstr = 9;
      break;
   case '\r':
      str = "#\\return";
      lenstr = 8;
      break;
   case '\t':
      str = "#\\tab";
      lenstr = 5;
      break;
   case '\x07':
      str = "#\\alarm";
      lenstr = 7;
      break;
   case '\x08':
      str = "#\\backspace";
      lenstr = 11;
      break;
   case '\x7f':
      str = "#\\delete";
      lenstr = 8;
      break;
   case '\x1b':
      str = "#\\escape";
      lenstr = 8;
      break;
   case 0:
      str = "#\\null";
      lenstr = 6;
      break;
   default:
      str = buf;
      if (IS_CONTROL_CODE(car))
         lenstr = get_control_name(buf, sizeof buf, car);
      else {
         buf[0] = '#';
         buf[1] = '\\';
         lenstr = 2 + char_to_utf8(car, (uint8_t*)&buf[2]);
      }
      break;
   }
   port_write_utf8_length(tsc, pt, str, lenstr);
}

static void print_int(tr7_engine_t tsc, port_t *pt, unsigned pflags, tr7_int_t num)
{
   char buf[140]; /* enough for 128 bits */
   unsigned lenstr;

   lenstr = format_int(tsc, buf, sizeof buf, pflags, num);
   port_write_utf8_length(tsc, pt, buf, lenstr);
}

static void print_double(tr7_engine_t tsc, port_t *pt, unsigned pflags, double num)
{
   char buf[140]; /* enough for 128 bits */
   unsigned lenstr;

   lenstr = format_double(tsc, buf, sizeof buf, pflags, num);
   port_write_utf8_length(tsc, pt, buf, lenstr);
}


static void print_vector(tr7_engine_t tsc, port_t *pt, unsigned pflags, tr7_vector_t vector, tr7_t anchors)
{
   tr7_uint_t idx, cnt = TR7_HEAD_UVALUE(TR7_CELL_HEAD(vector)); /* TODO: MACRO */
   port_write_utf8_length(tsc, pt, "#(", 2);
   for(idx = 0 ; idx < cnt ; idx++) {
      if (idx)
         port_write_utf8_length(tsc, pt, " ", 1);
      print_item(tsc, pt, pflags, vector->items[idx], anchors);
   }
   port_write_utf8_length(tsc, pt, ")", 1);
}

static void print_bytevector(tr7_engine_t tsc, port_t *pt, tr7_buffer_t bytevector)
{
   unsigned off = 1, cnt;
   char buf[4];
   uint8_t val;
   uint8_t *ptr = bytevector->content;
   uint8_t *end = ptr + TR7_BUFFER_LENGTH(bytevector);
   port_write_utf8_length(tsc, pt, "#u8(", 4);
   buf[0] = ' ';
   for(; ptr != end; off = 0) {
      val = *ptr++;
      if (val > 99) {
         buf[1] = (char)('0' + (val / 100));
         buf[2] = (char)('0' + ((val % 100) / 10));
         buf[3] = (char)('0' + (val % 10));
         cnt = 4;
      }
      else if (val > 9) {
         buf[1] = (char)('0' + (val / 10));
         buf[2] = (char)('0' + (val % 10));
         cnt = 3;
      }
      else {
         buf[1] = (char)('0' + val);
         cnt = 2;
      }
      port_write_utf8_length(tsc, pt, &buf[off], cnt - off);
   }
   port_write_utf8_length(tsc, pt, ")", 1);
}

static int print_anchor(tr7_engine_t tsc, port_t *pt, tr7_t item, tr7_t anchors)
{
   char buf[60]; /* enougth */
   unsigned len, idx = 1;
   while (!TR7_IS_NIL(anchors)) {
      tr7_t head = TR7_CAR(anchors);
      if (TR7EQ(item, TR7_CAR(head))) {
         int set = !!TR7_TO_INT(TR7_CDR(head));
         buf[0] = '#';
         len = format_int(tsc, &buf[1], sizeof buf - 2, 10, idx);
         if (set)
            buf[len + 1] = '#';
         else {
            TR7_CDAR(anchors) = TR7_FROM_INT(idx);
            buf[len + 1] = '=';
         }
         port_write_utf8_length(tsc, pt, buf, len + 2);
         return set;
      }
      anchors = TR7_CDR(anchors);
      idx++;
   }
   return 0;
}

static void print_pair(tr7_engine_t tsc, port_t *pt, unsigned pflags, tr7_pair_t head, tr7_t anchors)
{
   tr7_t car = TR7_PAIR_CAR(head);
   tr7_t cdr = TR7_PAIR_CDR(head);

#define PRINT_ABBREV(sym, bref, len) \
   if (TR7EQ(car, SYMBOL(sym)) && TR7_IS_PAIR(cdr) && TR7_IS_NIL(TR7_CDR(cdr))) { \
      port_write_utf8_length(tsc, pt, bref, len); \
      print_item(tsc, pt, pflags, TR7_CAR(cdr), anchors); \
      return; \
   }
   PRINT_ABBREV(QUOTE, "'", 1)
   PRINT_ABBREV(QUASIQUOTE, "`", 1)
   PRINT_ABBREV(UNQUOTE, ",", 1)
   PRINT_ABBREV(UNQUOTE_SPLICING, ",@", 2)
#undef PRINT_ABBREV

   port_write_utf8_length(tsc, pt, "(", 1);
   print_item(tsc, pt, pflags, car, anchors);
   while (TR7_IS_PAIR(cdr) && !tr7_assq_pair(cdr, anchors)) {
      car = TR7_CAR(cdr);
      cdr = TR7_CDR(cdr);
      port_write_utf8_length(tsc, pt, " ", 1);
      print_item(tsc, pt, pflags, car, anchors);
   }
   if (!TR7_IS_NIL(cdr)) {
      port_write_utf8_length(tsc, pt, " . ", 3);
      print_item(tsc, pt, pflags, cdr, anchors);
   }
   port_write_utf8_length(tsc, pt, ")", 1);
}

static void print_item(tr7_engine_t tsc, port_t *pt, unsigned pflags, tr7_t item, tr7_t anchors)
{
   const char *str = "#<?>";
   tr7_cell_t cell;
   unsigned lenstr;

   switch (TR7_TAG(item)) {

   /* case of integers */
   case TR7_TAG_EINT:
   case TR7_TAG_OINT:
      print_int(tsc, pt, 10, TR7_TO_INT(item));
      return;

   /* operation */
   case TR7_TAG_DOUBLE:
      print_double(tsc, pt, 10, *TR7_TO_DOUBLE(item));
      return;

   /* case of specials */
   case TR7_TAG_SPECIAL:
      switch (TR7_VSP_TAG(item)) {

      /* predefined */
      case TR7_TAG_VSP(TR7_VSP_CONSTANT):
         switch (item) {
         case TR7_NIL:   str = "()"; break;
         case TR7_FALSE: str = "#f"; break;
         case TR7_TRUE:  str = "#t"; break;
         case TR7_EOF:   str = "#<EOF>";  break;
         case TR7_VOID:  str = pflags ? "#<VOID>" : ""; break;
         }
         break;

      /* operator */
      case TR7_TAG_VSP(TR7_VSP_INTERNAL):
         switch (INTERNAL_TAG(item)) {
         case TAG_INTERNAL(INTERNAL_PROC):
            {
            /* operator */
            const char *opname = get_proc_name(TO_PROC(item));
            port_write_utf8(tsc, pt, "#<");
            if (opname != NULL)
               port_write_utf8(tsc, pt, opname);
            else {
               port_write_utf8(tsc, pt, "PROC ");
               print_int(tsc, pt, 10, TO_PROC(item));
            }
            port_write_utf8(tsc, pt, ">");
            }
            return;
         case TAG_INTERNAL(INTERNAL_SYNTAX):
            {
            /* syntax */
            const char *syname = get_syn_name(TO_SYNTAX(item));
            port_write_utf8(tsc, pt, "#<");
            if (syname != NULL)
               port_write_utf8(tsc, pt, syname);
            else {
               port_write_utf8(tsc, pt, "SYNTAX ");
               print_int(tsc, pt, 10, TO_SYNTAX(item));
            }
            port_write_utf8(tsc, pt, ">");
            }
            return;
#if SHOW_OPCODES
         case TAG_INTERNAL(INTERNAL_OPER):
            if (TO_OPER(item) < OPERID(MAXDEFINED))
               str = operator_names[TO_OPER(item)];
            break;
#endif
         }
         break;

      /* character */
      case TR7_TAG_VSP(TR7_VSP_CHARACTER):
         if (pflags)
            write_character(tsc, pt, TR7_TO_CHAR(item));
         else
            port_write_char(tsc, pt, TR7_TO_CHAR(item));
         return;

      default:
         break;
      }
      break;

   /* case of pairs */
   case TR7_TAG_PAIR:
      if (TR7_IS_VOID(item)) {
         if (!pflags)
            return;
         str = "#<VOID>";
         break;
      }
      if (print_anchor(tsc, pt, item, anchors))
         return;
      print_pair(tsc, pt, pflags, TR7_TO_PAIR(item), anchors);
      return;

   /* case of cells */
   case TR7_TAG_CELL:
      if (print_anchor(tsc, pt, item, anchors))
         return;
      cell = TR7_TO_CELL(item);
      switch (TR7_CELL_KIND(cell)) {

      /* strings */
      case Tr7_Head_Kind_String:
         str = (char*)TR7_CELL_CONTENT_STRING(cell);
         lenstr = TR7_CELL_SIZE_STRING(cell);
         if (pflags)
            print_esc_string(tsc, pt, str, lenstr, '"');
         else
            port_write_utf8_length(tsc, pt, str, lenstr);
         return;

      /* symbols */
      case Tr7_Head_Kind_Symbol:
         str = (char*)TR7_CELL_CONTENT_SYMBOL(cell);
         lenstr = TR7_CELL_SIZE_SYMBOL(cell);
         if (!is_simple_symbol(str, lenstr))
            print_esc_string(tsc, pt, str, lenstr, '|');
         else
            port_write_utf8_length(tsc, pt, str, lenstr);
         return;

      /* bytevector */
      case Tr7_Head_Kind_Byte_Vector:
         print_bytevector(tsc, pt, (tr7_buffer_t)cell);
         return;

      case Tr7_Head_Kind_Port:
         str = "#<PORT>";
         break;

      case Tr7_Head_Kind_CFunction:
         port_write_utf8(tsc, pt, "#<FOREIGN PROCEDURE ");
         print_int(tsc, pt, 16, (intptr_t)((tr7_cfunc_t)cell)->definition->func);
         port_write_utf8(tsc, pt, ">");
         return;

      case Tr7_Head_Kind_CPointer:
         port_write_utf8(tsc, pt, "#<FOREIGN POINTER ");
         print_int(tsc, pt, 16, (intptr_t)((tr7_cptr_t)cell)->value);
         port_write_utf8(tsc, pt, ">");
         return;

      case Tr7_Head_Kind_Continuation:
         str = "#<CONTINUATION>";
         break;

      case Tr7_Head_Kind_Rational:
         break;

      case Tr7_Head_Kind_Complex:
         break;

      case Tr7_Head_Kind_Lambda:
#if DUMP_LAMBDAS
         port_write_utf8(tsc, pt, "#<LAMBDA ");
         print_item(tsc, pt, PRTFLG_ESCAPE|PRTFLG_SHAREDS, TR7_TO_CLOSURE(item)->description, anchors);
#if DUMP_CLOSURES
         port_write_char(tsc, pt, ' ');
         print_item(tsc, pt, PRTFLG_ESCAPE|PRTFLG_SHAREDS, TR7_TO_CLOSURE(item)->upperframes, anchors);
#endif
         port_write_char(tsc, pt, '>');
         return;
#else
         str = "#<LAMBDA>";
         break;
#endif

#if USE_SCHEME_CASE_LAMBDA
      case Tr7_Head_Kind_Case_Lambda:
#if DUMP_LAMBDAS
         port_write_utf8(tsc, pt, "#<CASE-LAMBDA ");
         print_item(tsc, pt, PRTFLG_ESCAPE|PRTFLG_SHAREDS, TR7_TO_CLOSURE(item)->description, anchors);
#if DUMP_CLOSURES
         port_write_char(tsc, pt, ' ');
         print_item(tsc, pt, PRTFLG_ESCAPE|PRTFLG_SHAREDS, TR7_TO_CLOSURE(item)->upperframes, anchors);
#endif
         port_write_char(tsc, pt, '>');
         return;
#else
         str = "#<CASE-LAMBDA>";
         break;
#endif
#endif

      case Tr7_Head_Kind_Promise:
         str = "#<PROMISE>";
         break;

      case Tr7_Head_Kind_Parameter:
         str = "#<PARAMETER>";
         break;

      case Tr7_Head_Kind_Transform:
         port_write_utf8(tsc, pt, "#<TRANSFORM ");
         port_write_utf8(tsc, pt, transform_name(item));
         port_write_utf8(tsc, pt, ">");
         return;

      case Tr7_Head_Kind_Environment:
         str = "#<ENVIRONMENT>";
         break;

      case Tr7_Head_Kind_Box:
         if (!TR7_CELL_IS_IMMUTABLE(cell)) {
            port_write_utf8(tsc, pt, "#<BOX ");
            print_item(tsc, pt, pflags, CELL_BOX_GET(cell), anchors);
            port_write_utf8(tsc, pt, ">");
            return;
         }
#if USE_TR7_DEBUG
         port_write_utf8(tsc, pt, "#<LOC ");
         print_item(tsc, pt, pflags, CELL_BOX_ITEM(cell, 1), anchors);
         port_write_utf8(tsc, pt, ">");
         return;
#else
         str = "#<LOC>";
         break;
#endif

      case Tr7_Head_Kind_Record:
         if (tr7_is_record_desc(item)) {
            port_write_utf8(tsc, pt, "#<RECORD-DESC ");
            port_write_utf8(tsc, pt, tr7_record_desc_name_string(item));
         }
         else {
            port_write_utf8(tsc, pt, "#<RECORD ");
            port_write_utf8(tsc, pt, tr7_record_typename_string(item));
            for (tr7_uint_t i = 1 ; i < TR7_LENGTH_RECORD(item) ; i++) {
               port_write_utf8(tsc, pt, " ");
               print_item(tsc, pt, pflags, TR7_ITEM_RECORD(item, i), anchors);
            }
         }
         port_write_utf8(tsc, pt, ">");
         return;

      case Tr7_Head_Kind_Vector:
         print_vector(tsc, pt, pflags, (tr7_vector_t)cell, anchors);
         return;

      case Tr7_Head_Kind_Big_Int:
         break;

      case Tr7_Head_Kind_Program:
#if USE_TR7_DEBUG
         if (!TR7_IS_VOID(TR7_ITEM_VECTOR(item, Program_Idx_Name))) {
            port_write_utf8(tsc, pt, "#<PROGRAM ");
            print_item(tsc, pt, pflags, TR7_ITEM_VECTOR(item, Program_Idx_Name), anchors);
            port_write_char(tsc, pt, '>');
            return;
         }
         str = "#<PROGRAM ?>";
#else
         str = "#<PROGRAM>";
#endif
         break;

      default: /*TODO*/
         break;
      }
      break;

   default:
      break;
   }
   port_write_utf8(tsc, pt, str);
}

static void do_print(tr7_engine_t tsc, port_t *pt, int pflags, tr7_t obj)
{
   tr7_t  anchors;
   if (pflags & PRTFLG_SHAREDS)
      anchors = scan_shareds(tsc, obj);
   else if (pflags & PRTFLG_LOOPS)
      anchors = scan_loops(tsc, obj);
   else
      anchors = TR7_NIL;
   print_item(tsc, pt, pflags & PRTFLG_ESCAPE, obj, anchors);
}

/* print items */
int tr7_display_string(tr7_engine_t tsc, const char *s)
{
   port_t *pt = TR7__PORT__PORT(get_stdport(tsc,IDX_STDOUT));
   return port_write_utf8(tsc, pt, s);
}

void tr7_flush(tr7_engine_t tsc)
{
   port_t *pt = TR7__PORT__PORT(get_stdport(tsc,IDX_STDOUT));
   port_flush(tsc, pt);
}

static void print_to(tr7_engine_t tsc, tr7_t item, int pflags, int idxport)
{
   port_t *pt = TR7__PORT__PORT(get_stdport(tsc, idxport));
   do_print(tsc, pt, pflags, item);
}

void tr7_write(tr7_engine_t tsc, tr7_t item)
{
   print_to(tsc, item, PRTFLG_ESCAPE | PRTFLG_LOOPS, IDX_STDOUT);
}

void tr7_write_simple(tr7_engine_t tsc, tr7_t item)
{
   print_to(tsc, item, PRTFLG_ESCAPE, IDX_STDOUT);
}

void tr7_write_shared(tr7_engine_t tsc, tr7_t item)
{
   print_to(tsc, item, PRTFLG_ESCAPE | PRTFLG_SHAREDS, IDX_STDOUT);
}

void tr7_display(tr7_engine_t tsc, tr7_t item)
{
   print_to(tsc, item, PRTFLG_LOOPS, IDX_STDOUT);
}

static void log_str(tr7_engine_t tsc, const char *string)
{
   port_t *pt = TR7__PORT__PORT(get_stdport(tsc, IDX_STDERR));
   port_write_utf8(tsc, pt, string);
}

static void log_item(tr7_engine_t tsc, tr7_t item)
{
   print_to(tsc, item, PRTFLG_ESCAPE | PRTFLG_SHAREDS, IDX_STDERR);
}

static void log_item_string(tr7_engine_t tsc, tr7_t item)
{
   int pflags = TR7_IS_STRING(item) ? 0 : PRTFLG_ESCAPE | PRTFLG_SHAREDS;
   print_to(tsc, item, pflags, IDX_STDERR);
}

/*
*************************************************************************
* SECTION DISASSEMBLY
* -------------------
*/
#if USE_TR7_DEBUG
#if DEBUG_LINES
static unsigned line_of_pos(const uint8_t *lines, unsigned szlines, unsigned pos)
{
   unsigned ili = 0, lif, po;
   /* detect extra bytes */
   while(szlines && lines[szlines - 1] == 0xff)
      szlines--;
   /* the line at start */
   lif = (unsigned)(lines[ili] & 127);
   while (lines[ili++] & 128)
      lif = (lif << 7) | (unsigned)(lines[ili] & 127);
   /* search line of the position */
   while(ili < szlines) {
      /* get position */
      po = (unsigned)(lines[ili] & 127);
      while (lines[ili++] & 128)
         po = (po << 7) | (unsigned)(lines[ili] & 127);
      /* leave when pos found */
      if (po > pos)
         break;
      /* get line */
      lif = (unsigned)(lines[ili] & 127);
      while (lines[ili++] & 128)
         lif = (lif << 7) | (unsigned)(lines[ili] & 127);
   }
   return lif;
}
#endif
static unsigned disassemble_instruction(
               tr7_engine_t tsc,
               port_t *pt,
               const uint16_t *code,
               unsigned poscode,
               tr7_t *holders_array,
               tr7_t holders_list
#if DEBUG_LINES
               , const uint8_t *lines
               , unsigned szlines
#endif
) {
   char buffer[20];
   unsigned pos = poscode;
   uint16_t op, val;
   uint8_t mod, cnt, idx;
   tr7_t held;

   /* line in source */
#if DEBUG_LINES
   sprintf(buffer, "L%-4u ", line_of_pos(lines, szlines, pos));
   port_write_utf8(tsc, pt, buffer);
#endif

   /* code address */
   sprintf(buffer, "%4u. ", pos);
   port_write_utf8(tsc, pt, buffer);

   /* get opcode and its mode */
   op = code[pos++];
   mod = idx = op >= INSTRID(MAXDEFINED) ? 0 : decode_instr_modes[op];

   /* dump binary code */
   for (cnt = 1 ; idx ; cnt++, idx >>= DECODE_TAG_SHIFT);
   for (idx = 0 ; idx < 4 ; idx++)
      if (idx < cnt)
         sprintf(&buffer[idx * 3], " %02X", code[pos + idx - 1]);
      else
         sprintf(&buffer[idx * 3], "   ");
   port_write_utf8(tsc, pt, buffer);

   /* dump assembly operation */
   if (op >= INSTRID(MAXDEFINED)) {
      sprintf(buffer, " ???");
      port_write_utf8(tsc, pt, buffer);
   }
   else {
      sprintf(buffer, mod ? " %-15s" : " %s", decode_instr_names[op]);
      port_write_utf8(tsc, pt, buffer);

      /* dump arguments */
      for(; mod ; mod >>= DECODE_TAG_SHIFT) {
         val = code[pos++];
         switch(mod & DECODE_TAG_MASK) {
         case DECODE_TAG_UINT:
            sprintf(buffer, " %u", val);
            port_write_utf8(tsc, pt, buffer);
            break;
         case DECODE_TAG_VALUE:
            sprintf(buffer, " %u=", val);
            port_write_utf8(tsc, pt, buffer);
            if (holders_array != NULL)
               held = holders_array[val];
            else {
               for (held = holders_list ; val-- && TR7_IS_PAIR(held) ; held = TR7_CDR(held));
               held = TR7_IS_PAIR(held) ? TR7_CAR(held) : TR7_VOID;
            }
            do_print(tsc, pt, PRTFLG_ESCAPE | PRTFLG_SHAREDS, held);
            break;
         case DECODE_TAG_RELOC:
            val = pos - 1 + val;
            sprintf(buffer, " @%u", val);
            port_write_utf8(tsc, pt, buffer);
            break;
         case DECODE_TAG_PROC:
            port_write_utf8(tsc, pt, " ");
            do_print(tsc, pt, PRTFLG_ESCAPE | PRTFLG_SHAREDS, FROM_PROC(val));
            sprintf(buffer, ":%u", val);
            port_write_utf8(tsc, pt, buffer);
            break;
         case DECODE_TAG_SINT:
            sprintf(buffer, " %d", (int)(int16_t)val);
            port_write_utf8(tsc, pt, buffer);
            break;
         case DECODE_TAG_DATA:
            port_write_utf8(tsc, pt, " ");
            do_print(tsc, pt, PRTFLG_ESCAPE | PRTFLG_SHAREDS, I2TR7((int16_t)val));
            break;
         }
      }
   }
   return pos;
}

static void disassemble_program(tr7_engine_t tsc, tr7_t prog, port_t *pt, int depth);

static void disassemble(
               tr7_engine_t tsc,
               port_t *pt,
               int depth,
               const uint16_t *code,
               unsigned szcode,
               tr7_t *holders_array,
               unsigned szholders_array,
               tr7_t holders_list
#if DEBUG_LINES
               , const uint8_t *lines
               , unsigned szlines
#endif
) {
   int i;
   unsigned pos = 0;
   char buffer[30], indent[] = { ' ', ' ', ' ' };

   if (pt == NULL)
      pt = TR7__PORT__PORT(get_stdport(tsc, IDX_STDOUT));

   /* detect extra words */
   while(szcode && code[szcode - 1] == 0xffff)
      szcode--;

   /* disassemble */
   while (pos < szcode) {
      for (i = 0 ; i < depth ; i++)
         port_write_utf8_length(tsc, pt, indent, sizeof indent);
      pos = disassemble_instruction(tsc, pt, code, pos, holders_array, holders_list
#if DEBUG_LINES
               , lines, szlines
#endif
         );
      port_write_utf8(tsc, pt, "\n");
   }

   if (holders_array != NULL) {
      for(pos = 0 ; pos < szholders_array ; pos++)
         if (TR7_IS_CELL_KIND(holders_array[pos + Program_Idx_Quote0], Tr7_Head_Kind_Program)) {
            for (i = 0 ; i < depth ; i++)
               port_write_utf8_length(tsc, pt, indent, sizeof indent);
            sprintf(buffer, "*** PROG=%d\n", pos + Program_Idx_Quote0);
            port_write_utf8(tsc, pt, buffer);
            disassemble_program(tsc, holders_array[pos + Program_Idx_Quote0], pt, depth + 1);
         }
   }
   else {
      for(pos = 0 ; TR7_IS_PAIR(holders_list) ; pos++) {
         if (TR7_IS_CELL_KIND(TR7_CAR(holders_list), Tr7_Head_Kind_Program)) {
            for (i = 0 ; i < depth ; i++)
               port_write_utf8_length(tsc, pt, indent, sizeof indent);
            sprintf(buffer, "*** PROG=%d\n", pos);
            port_write_utf8(tsc, pt, buffer);
            disassemble_program(tsc, TR7_CAR(holders_list), pt, depth + 1);
         }
         holders_list = TR7_CDR(holders_list);
      }
   }
}

static void disassemble_program(tr7_engine_t tsc, tr7_t prog, port_t *pt, int depth)
{
   tr7_vector_t vprog = TR7_TO_VECTOR(prog);
   unsigned ncells = TR7_VECTOR_LENGTH(vprog);
   unsigned poscode = (unsigned)TR7_TO_UINT(TR7_VECTOR_ITEM(vprog, Program_Idx_Code));
   tr7_t *holders = &TR7_VECTOR_ITEM(vprog, 0);
   unsigned nholders = poscode - Program_Idx_Quote0;
   const uint16_t *code = (uint16_t*)&TR7_VECTOR_ITEM(vprog, poscode);
   unsigned szcode = (unsigned)((ncells - poscode) * (sizeof(tr7_t) / sizeof(uint16_t)));
#if DEBUG_LINES
   unsigned poslines = (unsigned)TR7_TO_UINT(TR7_VECTOR_ITEM(vprog, Program_Idx_Lines));
   const uint8_t *lines = (uint8_t*)&TR7_VECTOR_ITEM(vprog, poslines);
   unsigned szlines = (unsigned)((ncells - poslines) * sizeof(tr7_t));
   szcode -= (unsigned)((ncells - poslines) * (sizeof(tr7_t) / sizeof(uint16_t)));
#endif

   if (pt == NULL)
      pt = TR7__PORT__PORT(get_stdport(tsc, IDX_STDOUT));

   port_write_utf8(tsc, pt, "PROG ");
   if (TR7_IS_VOID(TR7_VECTOR_ITEM(vprog, Program_Idx_Name)))
      port_write_utf8(tsc, pt, "?");
   else
      print_item(tsc, pt, 0, TR7_VECTOR_ITEM(vprog, Program_Idx_Name), TR7_NIL);
   port_write_utf8(tsc, pt, " n.params=");
   print_item(tsc, pt, 0, TR7_VECTOR_ITEM(vprog, Program_Idx_nParams), TR7_NIL);
   port_write_utf8(tsc, pt, " n.locals=");
   print_item(tsc, pt, 0, TR7_VECTOR_ITEM(vprog, Program_Idx_nLocals), TR7_NIL);
   port_write_utf8(tsc, pt, "\n");

   disassemble(tsc, pt, depth, code, szcode, holders, nholders, TR7_NIL
#if DEBUG_LINES
               , lines, szlines
#endif
         );
}

static void disassemble_any(tr7_engine_t tsc, tr7_t item, port_t *pt)
{
   tr7_cell_t cell;

   if (TR7_IS_CELL(item)) {

      cell = TR7_TO_CELL(item);
      switch (TR7_CELL_KIND(cell)) {

      case Tr7_Head_Kind_Lambda: {
            tr7_closure_t closure = (tr7_closure_t)cell;
            tr7_t prog = closure->description;
            disassemble_program(tsc, prog, pt, 0);
            return;
         }
#if 0 && USE_SCHEME_CASE_LAMBDA
      case Tr7_Head_Kind_Case_Lambda:
         closure = (tr7_closure_t)cell;
         list = closure->description;
         while(TR7_IS_PAIR(list)) {
            desc = TR7_CAR(list);
            idx = (int)TR7_TO_INT(TR7_ITEM_VECTOR(desc, Program_Idx_nParams));
            if (idx >= 0 ? idx == nargs : -(1 + idx) <= nargs)
               return execute_program(tsc, closure->upperframes, nargs, desc);
            list = TR7_CDR(list);
         }
         return raise_error_msg(tsc, "unbound case-lambda");
#endif
      case Tr7_Head_Kind_Program:
            disassemble_program(tsc, item, pt, 0);
            return;

      default:
         break;
      }

   }
   do_print(tsc, pt, PRTFLG_ESCAPE | PRTFLG_SHAREDS, item);
   port_write_utf8(tsc, pt, "\n");
}
#endif
/*
**************************************************************************
*
*/

/* equivalence of atoms */
int tr7_eq(tr7_t a, tr7_t b)
{
   return TR7EQ(a, b);
}

int tr7_eqv(tr7_t a, tr7_t b)
{
   if (TR7EQ(a, b))
      return 1; /* symbols, integers, specials, identity */

   return tr7_cmp_num(a, b) == Tr7_Cmp_Equal; /* TODO: avoid equality if inexacts */
}

/* protection against infinite equality search */
struct eqguard { tr7_t a, b; struct eqguard *prv; };
static int equal_unchecked(tr7_t a, tr7_t b, struct eqguard *prv);

/* compares 2 lists for equality */
static int listequal_checked(tr7_t a, tr7_t b, struct eqguard *prv)
{
   int i = 0;
   tr7_t slowa, fasta;
   tr7_t slowb, fastb;

   slowa = fasta = a;
   slowb = fastb = b;
   for (;;) {
      if (!equal_unchecked(TR7_CAR(fasta), TR7_CAR(fastb), prv))
         return 0;
      fasta = TR7_CDR(fasta);
      fastb = TR7_CDR(fastb);
      if (TR7_IS_NIL(fasta))
         return TR7_IS_NIL(fastb);
      if (!TR7_IS_PAIR(fasta))
         return tr7_equal(fasta, fastb);
      if (!TR7_IS_PAIR(fastb))
         return 0;
      i = i ^ 1;
      if ((i & 1) == 0) {
         slowa = TR7_CDR(slowa);
         slowb = TR7_CDR(slowb);
         if (TR7EQ(fasta, slowa))
            i |= 2;
         if (TR7EQ(fastb, slowb))
            i |= 4;
         if (i == 8 + 4 + 2)
            return 1;
         if (i == 4 + 2) {
            i = 8;
            slowa = fasta;
            slowb = fastb;
         }
      }
   }
}

/* compares 2 cells for equality */
static int cellequal_checked(tr7_t a, tr7_t b, struct eqguard *prv)
{
   tr7_cell_t cella, cellb;
   tr7_head_t heada, headb;
   tr7_uint_t length;
   tr7_t *itemsa, *itemsb;

   /* CAUTION: next is optimized!!! be aware!!! */
   cella = TR7_TO_CELL(a);
   cellb = TR7_TO_CELL(b);
   heada = TR7_CELL_HEAD(cella);
   headb = TR7_CELL_HEAD(cellb);

   if (TR7_HEAD_KIND(heada ^ headb) == 0)
      switch (TR7_HEAD_KIND(heada)) {
      case Tr7_Head_Kind_String:
      case Tr7_Head_Kind_Byte_Vector:
         length = TR7_HEAD_UVALUE(heada);
         return length == TR7_HEAD_UVALUE(headb)
            && !memcmp(((tr7_buffer_t)cella)->content, ((tr7_buffer_t)cellb)->content, length);
      case Tr7_Head_Kind_Box:
      case Tr7_Head_Kind_Record:
      case Tr7_Head_Kind_Vector:
         length = TR7_HEAD_UVALUE(heada);
         if (length == TR7_HEAD_UVALUE(headb)) {
            itemsa = (tr7_t*)cella;
            itemsb = (tr7_t*)cellb;
            while (length) {
               if (!equal_unchecked(*++itemsa, *++itemsb, prv))
                  return 0;
               length--;
            }
            return 1;
         }
         break;
      }

   return 0;
}

/* unchecked recursive equality */
static int equal_unchecked(tr7_t a, tr7_t b, struct eqguard *prv)
{
   struct eqguard guard;

   /* check pure equality */
   if (TR7EQ(a, b))
      return 1; /* symbols, integers, specials, identity */

   /* init the guard */
   if ((intptr_t)a < (intptr_t)b) {
      guard.a = a;
      guard.b = b;
   } else {
      guard.a = b;
      guard.b = a;
   }
   guard.prv = prv;
   while (prv != NULL) {
      if (prv->a == guard.a && prv->b == guard.b)
         return 1; /* equals? yes, doesn't avoid to fail later */
      prv = prv->prv;
   }

   /* recurse safely (if needed) now */
   if (TR7_IS_PAIR(guard.a))
      return TR7_IS_PAIR(guard.b) && listequal_checked(guard.a, guard.b, &guard);

   if (TR7_IS_CELL(guard.a))
      return TR7_IS_CELL(guard.b) && cellequal_checked(guard.a, guard.b, &guard);

   return tr7_cmp_num(guard.a, guard.b) == Tr7_Cmp_Equal; /* TODO: avoid equality if inexacts */
}

int tr7_equal(tr7_t a, tr7_t b)
{
   return equal_unchecked(a, b, NULL);
}

/* ========== Evaluation Cycle: handling stack ========== */

#if EXTRA_TRACING && USE_TR7_TRACE
/*
* Dumps the data stack on a given length
*/
static void data_stack_dump(tr7_engine_t tsc, int count)
{
   char buffer[30];
   int i, n = (int)(tsc->stack.tail - tsc->stack.data);
   if (n > count)
      n = count;
   for(i = 0 ; i < n ; i++) {
      snprintf(buffer, sizeof buffer, "**[%d]** ", i);
      log_str(tsc, buffer);
      log_item(tsc, DATA(tsc, i));
      log_str(tsc, "\n");
   }
}
/*
* Dumps the oper stack on a given length
*/
static void oper_stack_dump(tr7_engine_t tsc, int count)
{
   char buffer[30];
   int i, n = (int)(tsc->stack.oper - tsc->stack.head);
   if (n > count)
      n = count;
   for(i = 0 ; i < n ; i++) {
      snprintf(buffer, sizeof buffer, "++[%d]++ ", i);
      log_str(tsc, buffer);
      log_item(tsc, OPER_AT(tsc, i));
      log_str(tsc, "\n");
   }
}
#endif

static int stack_ensure(tr7_engine_t tsc, unsigned count)
{
   if (tsc->stack.head + count > tsc->stack.tail) {
      unsigned nrdata = (unsigned)(tsc->stack.tail - tsc->stack.data);
      unsigned nroper = (unsigned)(tsc->stack.oper - tsc->stack.head);
      tr7_t *head = (tr7_t*)get_cells(tsc, count, 0);
      if (head == NULL)
         return -1;
      memcpy(&head[0], tsc->stack.head, nroper * sizeof *head);
      memcpy(&head[count - nrdata], tsc->stack.data, nrdata * sizeof *head);
      tsc->stack.head = head;
      tsc->stack.data = &head[count - nrdata];
      tsc->stack.oper = &head[nroper];
      tsc->stack.tail = &head[count];
   }
   return 0;
}

static int stack_grow(tr7_engine_t tsc)
{
   unsigned prvsz, nxtsz;

   if (tsc->no_stack)
      return -1;

   prvsz = (unsigned)(tsc->stack.tail - tsc->stack.head);
   nxtsz = prvsz == 0 ? STACK_INITIAL_SIZE
                      : ((prvsz * STACK_GROW_MUL) / STACK_GROW_DIV) + STACK_GROW_INC;

   if (nxtsz > tsc->stack_size_max) {
      nxtsz = tsc->stack_size_max + 200;
      tsc->no_stack = 1;
   }
   return stack_ensure(tsc, nxtsz);
}

static int stack_safe(tr7_engine_t tsc, unsigned count)
{
#if GLOBAL_STACK_SAFETY
   count += tsc->stack.safegap;
#endif
   while (tsc->stack.oper + count >= tsc->stack.data)
      if (stack_grow(tsc) < 0)
         return -1;
   return 0;
}

static void data_stack_push_rescue(tr7_engine_t tsc, tr7_t item)
{
   if (!stack_grow(tsc))
      *--tsc->stack.data = item;
}

static tr7_t *data_stack_enter_safe(tr7_engine_t tsc, unsigned count)
{
   if (stack_safe(tsc, count) < 0)
      return NULL;
   return tsc->stack.data -= count;
}

static void data_push_safe_1(tr7_engine_t tsc, tr7_t x1)
{
   tr7_t *s = data_stack_enter_safe(tsc, 1);
   if (s != NULL)
      s[0] = x1;
}

static void data_push_safe_2(tr7_engine_t tsc, tr7_t x1, tr7_t x2)
{
   tr7_t *s = data_stack_enter_safe(tsc, 2);
   if (s != NULL) {
      s[0] = x1;
      s[1] = x2;
   }
}

static void data_push_safe_3(tr7_engine_t tsc, tr7_t x1, tr7_t x2, tr7_t x3)
{
   tr7_t *s = data_stack_enter_safe(tsc, 3);
   if (s != NULL) {
      s[0] = x1;
      s[1] = x2;
      s[2] = x3;
   }
}

static int data_stack_push_list(tr7_engine_t tsc, tr7_t list)
{
   /* TODO: improve, count sz, allocate, iterate directly */
   int r = 0;
   if (TR7_IS_PAIR(list)) {
      r = 1 + data_stack_push_list(tsc, TR7_CDR(list));
      DATA_PUSH_SAFE(tsc, TR7_CAR(list));
   }
   return r;
}

static unsigned data_stack_push_values(tr7_engine_t tsc)
{
   unsigned nvalues = tsc->nvalues;
   if (nvalues != 0) {
      tr7_t *s = data_stack_enter_safe(tsc, nvalues);
      if (s != NULL)
         memcpy(s, tsc->values, nvalues * sizeof *s);
      tsc->nvalues = 0;
   }
   return nvalues;
}

static void data_stack_push_values_first(tr7_engine_t tsc)
{
   unsigned nvalues = tsc->nvalues;
   if (nvalues == 0)
      DATA_PUSH(tsc, TR7_VOID);
   else {
      DATA_PUSH(tsc, tsc->values[0]);
      tsc->nvalues = 0;
   }
}






/***********************************************************/


static void oper_push_safe_1(tr7_engine_t tsc, tr7_t x1)
{
   if (stack_safe(tsc, 1) >= 0)
      *tsc->stack.oper++ = x1;
}

static void oper_push_safe_2(tr7_engine_t tsc, tr7_t x1, tr7_t x2)
{
   if (stack_safe(tsc, 2) >= 0) {
      *tsc->stack.oper++ = x2;
      *tsc->stack.oper++ = x1;
   }
}

static void oper_push_safe_3(tr7_engine_t tsc, tr7_t x1, tr7_t x2, tr7_t x3)
{
   if (stack_safe(tsc, 3) >= 0) {
      *tsc->stack.oper++ = x3;
      *tsc->stack.oper++ = x2;
      *tsc->stack.oper++ = x1;
   }
}

static void oper_push_safe_4(tr7_engine_t tsc, tr7_t x1, tr7_t x2, tr7_t x3, tr7_t x4)
{
   if (stack_safe(tsc, 4) >= 0) {
      *tsc->stack.oper++ = x4;
      *tsc->stack.oper++ = x3;
      *tsc->stack.oper++ = x2;
      *tsc->stack.oper++ = x1;
   }
}














/***********************************************************/



static void drop_values(tr7_engine_t tsc)
{
   tsc->values[0] = TR7_VOID;
   tsc->nvalues = 0;
}

static void set_values_single(tr7_engine_t tsc, tr7_t val)
{
   tsc->values[0] = val;
   tsc->nvalues = 1;
}

static void set_values_multi(tr7_engine_t tsc, unsigned nvalues, tr7_t values[])
{
   tsc->nvalues = nvalues;
   memcpy(tsc->values, values, nvalues * sizeof(tr7_t));
}

static void set_values_pop(tr7_engine_t tsc, unsigned nvalues)
{
   tsc->nvalues = nvalues;
   memcpy(tsc->values, &DATA(tsc, 0), nvalues * sizeof(tr7_t));
   DATA_POP_N(tsc, nvalues);
}

/***********************************************************/

static eval_status_t return_single(tr7_engine_t tsc, tr7_t val, eval_status_t status)
{
   set_values_single(tsc, val);
   return status;
}

static eval_status_t return_void(tr7_engine_t tsc, eval_status_t status)
{
   drop_values(tsc);
   return status;
}

static eval_status_t do_pop_status(tr7_engine_t tsc, int nargs, eval_status_t status)
{
   DATA_POP_N(tsc, nargs);
   return status;
}

static eval_status_t do_pop_status_void(tr7_engine_t tsc, int nargs, eval_status_t status)
{
   DATA_POP_N(tsc, nargs);
   return return_void(tsc, status);
}

static eval_status_t do_pop_status_single(tr7_engine_t tsc, int nargs, tr7_t a, eval_status_t status)
{
   DATA_POP_N(tsc, nargs);
   return return_single(tsc, a, status);
}

static eval_status_t do_pop_status_pop(tr7_engine_t tsc, int nargs, eval_status_t status)
{
   set_values_pop(tsc, (unsigned)nargs);
   return status;
}

/***********************************************************/

static eval_status_t do_pop_continue_single(tr7_engine_t tsc, int nargs, tr7_t a)
{
   return do_pop_status_single(tsc, nargs, a, Cycle_Continue);
}

static eval_status_t do_pop_continue_single_alloc(tr7_engine_t tsc, int nargs, tr7_t a)
{
   return do_pop_continue_single(tsc, nargs, a); /* TODO check a!=NIL */
}

static eval_status_t do_pop_continue_2_values(tr7_engine_t tsc, int nargs, tr7_t a, tr7_t b)
{
   tsc->nvalues = 2;
   tsc->values[0] = a;
   tsc->values[1] = b;
   return do_pop_status(tsc, nargs, Cycle_Continue);
}

static eval_status_t do_pop_continue_integer(tr7_engine_t tsc, int nargs, tr7_int_t i)
{
   return do_pop_continue_single(tsc, nargs, tr7_from_int(tsc, i));
}

static eval_status_t do_pop_continue_double(tr7_engine_t tsc, int nargs, double d)
{
   return do_pop_continue_single(tsc, nargs, tr7_from_double(tsc, d));
}

static eval_status_t do_pop_continue_void(tr7_engine_t tsc, int nargs)
{
   return do_pop_status_void(tsc, nargs, Cycle_Continue);
}

static eval_status_t do_pop_continue_false(tr7_engine_t tsc, int nargs)
{
   return do_pop_continue_single(tsc, nargs, TR7_FALSE);
}

static eval_status_t do_pop_continue_EOF(tr7_engine_t tsc, int nargs)
{
   return do_pop_continue_single(tsc, nargs, TR7_EOF);
}

static eval_status_t do_pop_continue_boolean(tr7_engine_t tsc, int nargs, int value)
{
   return do_pop_continue_single(tsc, nargs, value ? TR7_TRUE : TR7_FALSE);
}

static eval_status_t do_pop_continue_char_or_EOF(tr7_engine_t tsc, int nargs, tr7_char_t car)
{
   return do_pop_continue_single(tsc, nargs, car == TR7_CHAR_EOF ? TR7_EOF : TR7_FROM_CHAR(car));
}

static eval_status_t do_pop_continue_u8_or_EOF(tr7_engine_t tsc, int nargs, int byte)
{
   return do_pop_continue_single(tsc, nargs, byte == EOF ? TR7_EOF : TR7_FROM_INT(byte));
}

/*
**************************************************************************
* SECTION CALL_STACK
* ------------------
*
*/
#if USE_TR7_DEBUG
/*
* Returns the list of frames (name args file line)
*/
static tr7_t call_stack(tr7_engine_t tsc)
{
   tr7_t result = TR7_NIL;
   unsigned idx = 0, count = (unsigned)(tsc->stack.oper - tsc->stack.head);
   while (idx < count) {
      if (!TR7EQ(OPER_AT(tsc, idx),OPER(XRUN)))
         idx++;
      else {
         tr7_vector_t vprog = TR7_TO_VECTOR(OPER_AT(tsc, idx + XRUN_Idx_Program));
         tr7_vector_t vframe = TR7_TO_VECTOR(OPER_AT(tsc, idx + XRUN_Idx_Frame));
         int nparams = TR7_TO_INT(TR7_VECTOR_ITEM(vprog, Program_Idx_nParams));
         tr7_t name = TR7_VECTOR_ITEM(vprog, Program_Idx_Name);
         int nargs = nparams < 0 ? -nparams : nparams;
         tr7_t *arg0 = &TR7_VECTOR_ITEM(vframe, Frame_Idx_Arg0);
         tr7_t args = tr7_cons_n(tsc, nargs, arg0, TR7_NIL);
#if DEBUG_LINES
         tr7_t file = TR7_VECTOR_ITEM(vprog, Program_Idx_Filename);
         unsigned ncells = TR7_VECTOR_LENGTH(vprog);
         unsigned poslines = (unsigned)TR7_TO_UINT(TR7_VECTOR_ITEM(vprog, Program_Idx_Lines));
         const uint8_t *lines = (uint8_t*)&TR7_VECTOR_ITEM(vprog, poslines);
         unsigned szlines = (unsigned)((ncells - poslines) * sizeof(tr7_t));
         unsigned pcx = (unsigned)TR7_TO_UINT(OPER_AT(tsc, idx + XRUN_Idx_PC));
         unsigned pc0 = (unsigned)((uint16_t*)&TR7_VECTOR_ITEM(vprog, TR7_TO_UINT(TR7_VECTOR_ITEM(vprog, Program_Idx_Code))) - (uint16_t*)vprog);
         unsigned pc = pcx - pc0;
         unsigned line = line_of_pos(lines, szlines, pc ? pc - 1 : pc);
         tr7_t cur = TR7_LIST4(tsc, name, args, file, TR7_FROM_UINT(line));
#else
         tr7_t cur = TR7_LIST2(tsc, name, args);
#endif
         result = TR7_CONS2(tsc, cur, result);
         idx += _XRUN_Idx_Count_;
      }
   }
   return tr7_reverse_in_place(result, TR7_NIL);
}
#endif
/*
**************************************************************************
* SECTION ERROR
* =============
* Errors are instances of a record type. SRFI 136 is used for inheritance
* of record types: read-error and file-error both inherit error.
*
* test if item is an error
*/
int tr7_is_error(tr7_t item)
{
   return tr7_is_record_type(item, RECORD_DESC(error));
}
/*
* test if item is a read error
*/
int tr7_is_read_error(tr7_t item)
{
   return tr7_is_record_type(item, RECORD_DESC(read_error));
}
/*
* test if item is a file error
*/
int tr7_is_file_error(tr7_t item)
{
   return tr7_is_record_type(item, RECORD_DESC(file_error));
}
/*
* helper for accessing error-object items
*/
static tr7_t error_item(tr7_t errobj, int index)
{
   tr7_record_t rec = tr7_as_record_cond(errobj, RECORD_DESC(error));
   return rec == NULL ? TR7_VOID : rec->items[index];
}
/*
* access error message
*/
tr7_t tr7_error_message(tr7_t errobj)
{
   return error_item(errobj, Error_Idx_Message);
}
/*
* access error irritants
*/
tr7_t tr7_error_irritants(tr7_t errobj)
{
   return error_item(errobj, Error_Idx_Irritants);
}
/*
* access error call stack
*/
tr7_t tr7_error_stack(tr7_t errobj)
{
#if USE_TR7_DEBUG
   return error_item(errobj, Error_Idx_Stack);
#else
   return TR7_VOID;
#endif
}













/*
* create an instance of error
*/
static tr7_t make_error(tr7_engine_t tsc, tr7_t errdsc, tr7_t msg, tr7_t irritants)
{
#if USE_TR7_DEBUG
   tr7_t data = TR7_LIST3(tsc, msg, irritants, call_stack(tsc));
#else
   tr7_t data = TR7_LIST2(tsc, msg, irritants);
#endif
   return mk_record_instance(tsc, errdsc, data);
}
/*
* create an instance of error for a string
*/
static tr7_t make_error_msg_irr(tr7_engine_t tsc, tr7_t errdsc, const char *msg, tr7_t irr, int copy)
{
   tr7_t t = tr7_make_string(tsc, msg, copy);
   return make_error(tsc, errdsc, t, irr);
}
/*
* converts an obj to an irritant
*/
static tr7_t obj2irr(tr7_engine_t tsc, tr7_t obj)
{
   return TR7_IS_VOID(obj) ? TR7_NIL : TR7_LIST1(tsc, obj);
}
/*
* setting error on result stack
*/
static int set_error(tr7_engine_t tsc, tr7_t error)
{
   set_values_single(tsc, error);
   return -1;
}

static int set_error_msg_irr(tr7_engine_t tsc, const char *msg, tr7_t irr, int copy)
{
   tr7_t error = make_error_msg_irr(tsc, RECORD_DESC(error), msg, irr, copy);
   return set_error(tsc, error);
}

static int set_error_msg_obj(tr7_engine_t tsc, const char *msg, tr7_t obj, int copy)
{
   tr7_t irr = obj2irr(tsc, obj);
   return set_error_msg_irr(tsc, msg, irr, copy);
}

static int set_error_oom(tr7_engine_t tsc)
{
   return set_error_msg_obj(tsc, "out of memory", TR7_VOID, 0);
}

/*
* setting and raising error
*/
static eval_status_t raise_error(tr7_engine_t tsc, tr7_t error)
{
   return return_single(tsc, error, Cycle_Raise);
}

static eval_status_t raise_error_msg_irr_desc(tr7_engine_t tsc, const char *msg, tr7_t irr, tr7_t errdsc, int copy)
{
   tr7_t error = make_error_msg_irr(tsc, errdsc, msg, irr, copy);
   return raise_error(tsc, error);
}

static eval_status_t raise_error_msg_irr(tr7_engine_t tsc, const char *msg, tr7_t irr)
{
   if (TR7_IS_VOID(irr))
      irr = TR7_NIL;
   else if (!TR7_IS_PAIR(irr))
      irr = TR7_LIST1(tsc, irr);
   tr7_t error = make_error_msg_irr(tsc, RECORD_DESC(error), msg, irr, 0);
   return raise_error(tsc, error);
}

static eval_status_t raise_error_msg_obj_desc(tr7_engine_t tsc, const char *msg, tr7_t obj, tr7_t errdsc, int copy)
{
   tr7_t irr = obj2irr(tsc, obj);
   return raise_error_msg_irr_desc(tsc, msg, irr, errdsc, copy);
}

static eval_status_t raise_error_msg_obj(tr7_engine_t tsc, const char *msg, tr7_t obj)
{
   return raise_error_msg_obj_desc(tsc, msg, obj, RECORD_DESC(error), 0);
}

static eval_status_t raise_error_msg(tr7_engine_t tsc, const char *msg)
{
   return raise_error_msg_obj(tsc, msg, TR7_VOID);
}

static eval_status_t raise_out_of_memory_error(tr7_engine_t tsc)
{
   return raise_error_msg(tsc, "out of memory");
}

static eval_status_t raise_out_of_bound_error(tr7_engine_t tsc, tr7_t bound)
{
   return raise_error_msg_irr(tsc, "out of bounds", bound);
}

static eval_status_t raise_invalid_argument_error(tr7_engine_t tsc)
{
   return raise_error_msg(tsc, "invalid argument");
}

static eval_status_t raise_immutable_error(tr7_engine_t tsc)
{
   return raise_error_msg(tsc, "can not set immutable");
}

static eval_status_t raise_division_by_zero_error(tr7_engine_t tsc)
{
   return raise_error_msg(tsc, "division by zero");
}

static eval_status_t raise_file_error(tr7_engine_t tsc, const char *msg, tr7_t obj)
{
   return raise_error_msg_obj_desc(tsc, msg, obj, RECORD_DESC(file_error), 0);
}
/***********************************************************/

/*
* execute the compiled 'code'
*/
static eval_status_t s_prog(tr7_engine_t tsc, tr7_t prog)
{
   if (TR7_IS_VOID(prog))
      return return_void(tsc, Cycle_Return);
   return execute_program(tsc, TR7_VOID, 0, prog);
}
/*
* execute a procedure
*/
static eval_status_t s_exec(tr7_engine_t tsc, tr7_t proc, unsigned nargs)
{
   oper_push_safe_3(tsc, OPER(XCALL), TR7_FROM_UINT(nargs), proc);
   return Cycle_Goto;
}

static eval_status_t s_exec_0(tr7_engine_t tsc, tr7_t proc)
{
   return s_exec(tsc, proc, 0);
}

static eval_status_t s_exec_1(tr7_engine_t tsc, tr7_t proc, tr7_t arg1)
{
   data_push_safe_1(tsc, arg1);
   return s_exec(tsc, proc, 1);
}

static eval_status_t s_exec_2(tr7_engine_t tsc, tr7_t proc, tr7_t arg1, tr7_t arg2)
{
   data_push_safe_2(tsc, arg1, arg2);
   return s_exec(tsc, proc, 2);
}
/*
**************************************************************************
* SECTION DYNAMIC_WIND
* --------------------
* add a new dynamic-wind frame for thunks 'before' and 'after' in the environment
*/
static int dynawind_push(tr7_engine_t tsc, tr7_t before, tr7_t after)
{
   tr7_int_t depth = 1;
   /* allocates the dynamic-wind vector */
   tr7_t item = alloc_vector(tsc, DynaWind_Count_Idx);
   if (TR7_IS_NIL(item))
      return -1;
   /* compute the depth */
   if (TR7_IS_VECTOR(tsc->stof_dynawinds))
      depth += TR7_TO_INT(TR7_ITEM_VECTOR(tsc->stof_dynawinds, DynaWind_Idx_Depth));
   /* initialize */
   TR7_ITEM_VECTOR(item, DynaWind_Idx_Previous) = tsc->stof_dynawinds; /* link */
   TR7_ITEM_VECTOR(item, DynaWind_Idx_Depth) = TR7_FROM_INT(depth); /* depth */
   TR7_ITEM_VECTOR(item, DynaWind_Idx_Before) = before;    /* guard */
   TR7_ITEM_VECTOR(item, DynaWind_Idx_After) = after;    /* guard */
   TR7_ITEM_VECTOR(item, DynaWind_Idx_Params) = tsc->stof_params; /* params */
   /* push */
   tsc->stof_dynawinds = item;
   return 0;
}
/*
* pushes action to perform when changing of dynamic environment from 'fromdw'
* to 'todw'.
* TODO (or not?) break recursion
*/
static void dynawind_compute_actions(tr7_engine_t tsc, tr7_t fromdw, tr7_t todw)
{
   /* check if somethign is to be done */
   if (!TR7EQ(fromdw, todw)) {
      /* yes, then get depths */
      tr7_uint_t fromdepth = TR7_IS_NIL(fromdw) ? 0
                     : TR7_TO_UINT(TR7_ITEM_VECTOR(fromdw, DynaWind_Idx_Depth));
      tr7_uint_t todepth = TR7_IS_NIL(todw) ? 0
                     : TR7_TO_UINT(TR7_ITEM_VECTOR(todw, DynaWind_Idx_Depth));
      /* deduce action from differences in depths */
      if (todepth > fromdepth) {
         /* queue entering destination */
         oper_push_safe_2(tsc, OPER(DWBEFORE), todw);
         /* change from 'fromdw' to previous of 'todw' */
         dynawind_compute_actions(tsc, fromdw, TR7_ITEM_VECTOR(todw, DynaWind_Idx_Previous));
      }
      else if (todepth < fromdepth) {
         /* change from previous of 'fromdw' to 'todw' */
         dynawind_compute_actions(tsc, TR7_ITEM_VECTOR(fromdw, DynaWind_Idx_Previous), todw);
         /* queue leaving current 'fromdw' */
         oper_push_safe_2(tsc, OPER(DWAFTER), fromdw);
      }
      else {
         /* queue entering destination */
         oper_push_safe_2(tsc, OPER(DWBEFORE), todw);
         /* change from previous of 'fromdw' to previous of 'todw' */
         dynawind_compute_actions(tsc, TR7_ITEM_VECTOR(fromdw, DynaWind_Idx_Previous), TR7_ITEM_VECTOR(todw, DynaWind_Idx_Previous));
         /* queue leaving current 'fromdw' */
         oper_push_safe_2(tsc, OPER(DWAFTER), fromdw);
      }
   }
}
/*
**************************************************************************
* SECTION CONTINUATION
* --------------------
*
* make continuation
*/
static tr7_t mk_continuation(tr7_engine_t tsc)
{
   size_t ndata = (size_t)(tsc->stack.tail - tsc->stack.data);
   size_t noper = (size_t)(tsc->stack.oper - tsc->stack.head);
   size_t size = noper + ndata;
   tr7_continuation_t x = get_cells(tsc, size + NCELL_OF_PTR(x), 0);
   TR7_CELL_HEAD(x) = TR7_MAKE_HEAD(size, Tr7_Head_Kind_Continuation);
   x->dynawind = tsc->stof_dynawinds;
   x->params = tsc->stof_params;
   x->noper = TR7_FROM_UINT(noper);
   memcpy(&x->stack, tsc->stack.head, noper * sizeof(tr7_t));
   memcpy(&x->stack[noper], tsc->stack.data, ndata * sizeof(tr7_t));
   return push_recent_cell(tsc, x);
}
/*
*/
static eval_status_t call_continuation(tr7_engine_t tsc, tr7_continuation_t cont, int nargs)
{
   tr7_t *prv;
   tr7_uint_t size = TR7_HEAD_UVALUE(TR7_CELL_HEAD(cont));
   tr7_uint_t noper = TR7_TO_UINT(cont->noper);
   tr7_uint_t ndata = size - noper;
#if GLOBAL_STACK_SAFETY
   size += tsc->stack.safegap;
#endif
   if (stack_ensure(tsc, size + (unsigned)nargs) < 0)
      return raise_out_of_memory_error(tsc);
   prv = tsc->stack.data;
   tsc->stack.data = &tsc->stack.tail[-((tr7_int_t)ndata + nargs)];
   tsc->stack.oper = &tsc->stack.head[noper];
   memmove(tsc->stack.data, prv, (unsigned)nargs * sizeof(tr7_t));
   memcpy(tsc->stack.head, &cont->stack[0], noper * sizeof(tr7_t));
   memcpy(&tsc->stack.data[nargs], &cont->stack[noper], ndata * sizeof(tr7_t));
   oper_push_safe_3(tsc, OPER(CONT), TR7_FROM_CONTINUATION(cont), TR7_FROM_INT(nargs));
   dynawind_compute_actions(tsc, tsc->stof_dynawinds, cont->dynawind);
   return Cycle_Goto;
}
/*
**************************************************************************
* SECTION STDPORTS
* ----------------
*
*
*/
static tr7_t get_stdport(tr7_engine_t tsc, int num)
{
   return parameter_get(tsc, tsc->stdports[num]);
}

static void set_stdport(tr7_engine_t tsc, tr7_t value, int num)
{
   parameter_set(tsc, tsc->stdports[num], value);
}

#if USE_SCHEME_FILE
static void push_stdport(tr7_engine_t tsc, tr7_t value, int num)
{
   parameter_push(tsc, tsc->stdports[num], value);
   oper_push_safe_1(tsc, OPER(PARAMPOP1));
}
#endif

static void init_stdports(tr7_engine_t tsc)
{
   tsc->stdports[IDX_STDIN] = mk_parameter(tsc, TR7_NIL, TR7_NIL);
   tsc->stdports[IDX_STDOUT] = mk_parameter(tsc, TR7_NIL, TR7_NIL);
   tsc->stdports[IDX_STDERR] = mk_parameter(tsc, TR7_NIL, TR7_NIL);
}

/*
**************************************************************************
* SECTION LIBRARY
* ---------------
*
* search the library of utf8 'name' of 'length' bytes
* and return its environment in 'libenv' if not NULL
* returns 1 if found or 0 if not found.
*/
static int searchlib(tr7_engine_t tsc, const char *name, unsigned length, tr7_t *libenv)
{
   tr7_t it;
   for (it = tsc->libraries ; !TR7_IS_NIL(it) ; it = TR7_CDR(it)) {
      tr7_buffer_t iname = TR7_TO_STRING(TR7_CAAR(it));
      if (TR7_STRING_SIZE(iname) == length
       && memcmp(name, TR7_STRING_CONTENT(iname), length) == 0) {
         if (libenv != NULL)
            *libenv = TR7_CDAR(it);
         return 1;
      }
   }
   return 0;
}
/*
*/
int tr7_has_lib(tr7_engine_t tsc, const char *name)
{
   return tr7_has_lib_length(tsc, name, strlen(name));
}
int tr7_has_lib_length(tr7_engine_t tsc, const char *name, unsigned length)
{
   return searchlib(tsc, name, length, NULL);
}
/*
* record the library defined by its exports in 'libenv'
*/
static void addlib(tr7_engine_t tsc, const char *name, unsigned length, tr7_t libenv)
{
   tr7_t libname = tr7_make_string_copy_length(tsc, name, length);
   tr7_t libdsc = tr7_cons(tsc, libname, libenv);
   tsc->libraries = tr7_cons(tsc, libdsc, tsc->libraries);
}
/*
*/
static int lib_enumerate(tr7_engine_t tsc, tr7_t env, env_enum_cb_t fun, void *closure)
{
   if (!TR7_IS_INT(env))
      return environment_enumerate(tsc, env, fun, closure);
   builtin_lib_enum(tsc, TR7_TO_INT(env), fun, closure);
   return 0;
}
/*
* search the library of 'name' (of 'length' bytes), try to load it if needed,
* and return its environment in 'libenv' if not NULL
* returns 0 on success or a negtive error code getlib_error_t
*/
static int getlib(tr7_engine_t tsc, const char *name, unsigned length, tr7_t *lib)
{
   int istd, rc;

   /* check if already here */
   if (searchlib(tsc, name, length, lib))
      return 0;

   /* search a standard library */
   istd = search_builtin_lib(name, length);
   if (istd >= 0) {
      if (lib != NULL)
         *lib = TR7_FROM_INT(istd);
   }
   else {
      /* push open the load the file */
      if (!load_enter_search_import(tsc, name, length))
         return -Tr7_GetLib_Error_Not_Found;

      /* evaluate the found library file */
      save_from_C_call(tsc);
      rc = play(tsc);
      restore_from_C_call(tsc);
      if (!rc)
         return -Tr7_GetLib_Error_Eval;

      /* check if loaded file declared the library */
      if (!searchlib(tsc, name, length, lib))
         return -Tr7_GetLib_Error_Name_Mismatch;
   }
   return 0;
}
/*
*/
int tr7_load_lib(tr7_engine_t tsc, const char *name)
{
   return tr7_load_lib_length(tsc, name, strlen(name));
}
int tr7_load_lib_length(tr7_engine_t tsc, const char *name, unsigned length)
{
   return getlib(tsc, name, length, NULL);
}
/*
* translate a valid libname to filename ex: (hi guy 1) -> hi/guy/1
* return the count of bytes needed by the name but don't fill more
* that size
*/
static unsigned make_libname(tr7_t libname, char buffer[], unsigned size)
{
   tr7_t x;
   char bufint[30];
   const char *ptr;
   unsigned pos = 0, len;

   while (TR7_IS_PAIR(libname)) {
      /* get the car, should be a symbol or a non negative integer */
      x = TR7_CAR(libname);
      if (TR7_IS_SYMBOL(x)) {
         len = (unsigned)TR7_SIZE_SYMBOL(x);
         ptr = (char*)TR7_CONTENT_SYMBOL(x);
         /* ignore leading colon (SRFI 97) */
         if (len && *ptr == ':')
            len--, ptr++;
      }
      else if (TR7_IS_INT(x) && TR7_TO_INT(x) >= 0) {
         len = (unsigned)snprintf(bufint, sizeof bufint, "%lld", (long long)TR7_TO_INT(x));
         ptr = bufint;
      }
      else
         break;

      /* append the value found */
      if (pos) {
         if (pos < size)
            buffer[pos] = LIB_SEP_CHAR;
         pos++;
      }
      if (pos < size)
         memcpy(&buffer[pos], ptr, pos + len <= size ? len : size - pos);
      pos += len;

      /* next of the list */
      libname = TR7_CDR(libname);
      if (TR7_IS_NIL(libname))
         return pos;
   }
   return 0; /* invalid */
}
/*
* search the library of name 'libname' (a list), loading it if needed
* and return its environment in 'libenv'
* returns 0 on success or a negtive error code getlib_error_t
*/
static int get_library(tr7_engine_t tsc, tr7_t libname, tr7_t *lib)
{
   char basename[LIBNAME_MAXSZ + 1];
   unsigned len;

   /* convert libname to basename */
   len = make_libname(libname, basename, sizeof basename);
   if (len == 0)
      return -Tr7_GetLib_Error_Invalid_Name;
   if (len >= sizeof basename)
      return -Tr7_GetLib_Error_Name_Too_Long;
   basename[len] = 0;

   return getlib(tsc, basename, len, lib);
}
/*
**************************************************************************
* SECTION CFUNCTION
* -----------------
*
*/
static tr7_t get_libenv(tr7_engine_t tsc, const char *libname)
{
   tr7_t libenv;
   const char *name = libname != NULL ? libname : TR7_FOREIGNS_LIBNAME;
   unsigned length = strlen(name);

   /* not for builtins */
   if (search_builtin_lib(name, length) >= 0)
      return TR7_VOID;

   /* get or create the lib */
   if (!searchlib(tsc, name, length, &libenv)) {
      libenv = mk_environment(tsc, TR7_NIL, DEFAULT_ENV_SIZE);
      addlib(tsc, name, length, libenv);
   }
   return libenv;
}

static tr7_t add_C_func(tr7_engine_t tsc, tr7_t libenv, const tr7_C_func_def_t *funcdef)
{
   tr7_t symbol, cfunc = TR7_VOID;
   if (!TR7_IS_VOID(libenv)) {
      symbol = tr7_get_symbol(tsc, funcdef->name, 1);
      cfunc = tr7_make_C_func(tsc, funcdef);
      if (!TR7_IS_SYMBOL(symbol) || !TR7_IS_CFUNC(cfunc))
         cfunc = TR7_FALSE;
      else
         tr7_define(tsc, libenv, symbol, cfunc);
   }
   return cfunc;
}

void tr7_lib_register_C_func_list(tr7_engine_t tsc, const char *libname, const tr7_C_func_def_t *functions)
{
   tr7_t libenv = get_libenv(tsc, libname);
   while (functions->name != NULL)
      add_C_func(tsc, libenv, functions++);
}

void tr7_lib_register_C_functions(tr7_engine_t tsc, const char *libname, const tr7_C_func_def_t *functions, unsigned count)
{
   tr7_t libenv = get_libenv(tsc, libname);
   while (count)
      add_C_func(tsc, libenv, &functions[--count]);
}

tr7_t tr7_lib_register_C_func(tr7_engine_t tsc, const char *libname, const tr7_C_func_def_t *funcdef)
{
   return add_C_func(tsc, get_libenv(tsc, libname), funcdef);
}

void tr7_register_C_func_list(tr7_engine_t tsc, const tr7_C_func_def_t *functions)
{
   tr7_lib_register_C_func_list(tsc, NULL, functions);
}

void tr7_register_C_functions(tr7_engine_t tsc, const tr7_C_func_def_t *functions, unsigned count)
{
   tr7_lib_register_C_functions(tsc, NULL, functions, count);
}

tr7_t tr7_register_C_func(tr7_engine_t tsc, const tr7_C_func_def_t *funcdef)
{
   return tr7_lib_register_C_func(tsc, NULL, funcdef);
}

tr7_t tr7_make_C_func(tr7_engine_t tsc, const tr7_C_func_def_t *funcdef)
{
   tr7_cfunc_t ff;

   if (funcdef->min_args < 0
    || (funcdef->max_args >= 0 && funcdef->max_args < funcdef->min_args))
      return TR7_FALSE;

   ff = GET_CELLS(tsc, ff, 0);
   if (!ff)
      return TR7_FALSE;

   TR7_CELL_HEAD(ff) = TR7_MAKE_HEAD(0, Tr7_Head_Kind_CFunction);
   ff->definition = funcdef;
   return push_recent_cell(tsc, ff);
}

tr7_C_return_t tr7_C_return_single(tr7_engine_t tsc, tr7_t value)
{
   return (tr7_C_return_t)return_single(tsc, value, (eval_status_t)Tr7_C_Return_Ok);
}

tr7_C_return_t tr7_C_return_values(tr7_engine_t tsc, unsigned count, tr7_t values[])
{
   set_values_multi(tsc, count, values);
   return Tr7_C_Return_Ok;
}

tr7_C_return_t tr7_C_raise_error(tr7_engine_t tsc, const char *utf8msg, tr7_t irritants, int copy)
{
   set_error_msg_irr(tsc,  utf8msg, irritants, copy);
   return Tr7_C_Return_Raise;
}

tr7_C_return_t tr7_C_raise_single(tr7_engine_t tsc, tr7_t value)
{
   return (tr7_C_return_t)return_single(tsc, value, (eval_status_t)Tr7_C_Return_Raise);
}

/*
**************************************************************************
* SECTION CPOINTER
* ----------------
*
*/
tr7_t tr7_make_foreign_pointer(tr7_engine_t tsc, void *value, tr7_cptr_vtable_t *vtable)
{
   tr7_cptr_t fp = GET_CELLS(tsc, fp, vtable && vtable->disposer);
   if (!fp)
      return TR7_FALSE;

   TR7_CELL_HEAD(fp) = TR7_MAKE_HEAD(0, Tr7_Head_Kind_CPointer);
   fp->value = value;
   fp->vtable = vtable;
   return push_recent_cell(tsc, fp);
}

void *tr7_get_foreign_pointer(tr7_t value)
{
   return TR7_IS_CPTR(value) ? TR7_TO_CPTR(value)->value : NULL;
}

tr7_cptr_vtable_t *tr7_get_foreign_pointer_vtable(tr7_t value)
{
   return TR7_IS_CPTR(value) ? TR7_TO_CPTR(value)->vtable : NULL;
}

tr7_foreign_t tr7_get_foreign(tr7_t value)
{
   return TR7_IS_CPTR(value)
            ? (tr7_foreign_t){ TR7_TO_CPTR(value)->value, TR7_TO_CPTR(value)->vtable }
            : (tr7_foreign_t){ NULL, NULL };
}
/*
**************************************************************************
* SECTION FEATURING
* -----------------
*
* The feature list
*/
static const char *feature_list[] = {
   "r7rs",        /* always first  */
   "tr7",         /* always second */
   ("tr7-"VERSION),  /* always third, see tr7_get_id */
   ("tr7-"MINVERSION)
#if USE_RATIOS
   ,"ratios"
#endif
#if USE_MATH
   ,"tr7-use-math"
#endif
#if HAS_GREEDY_SYNTAX
   ,"tr7-greedy-syntax"
#endif
};
/*
* get the tr7 identifier string
*/
const char *tr7_get_id(void)
{
   return feature_list[2];
}
/*
* get the tr7 version string
*/
const char *tr7_get_version(void)
{
   return &tr7_get_id()[4];
}
/*
* get feature's list
*/
static tr7_t get_features_list(tr7_engine_t tsc)
{
   tr7_t sym, res = TR7_NIL;
   unsigned n = sizeof feature_list / sizeof *feature_list;
   while (n) {
      sym = tr7_get_symbol(tsc, feature_list[--n], 0);
      res = tr7_cons(tsc, sym, res);
   }
   return res;
}
/*
* check a feature
*/
static int has_feature(tr7_t sym)
{
   int res = 0;
   if (TR7_IS_SYMBOL(sym)) {
      const char *str = tr7_symbol_string(sym);
      unsigned n = sizeof feature_list / sizeof *feature_list;
      while (n > 0 && !res)
         res = !strcmp(str, feature_list[--n]);
   }
   return res;
}
/*
**************************************************************************
* SECTION IMPORT
* --------------
*
* import symbol value to environment
*/
static int import_env_cb(tr7_engine_t tsc, tr7_t sym, tr7_t val, void *closure)
{
   tr7_t envir = (tr7_t)closure;
   int rc = environment_import(tsc, envir, sym, val);
   return rc ? 0 : set_error_oom(tsc);
}
/*
* import possibly renamed symbol
*/
static int import_rename_cb(tr7_engine_t tsc, tr7_t sym, tr7_t val, void *closure)
{
   import_list_t *s = (import_list_t*)closure;
   tr7_pair_t ren = tr7_assq_pair(sym, s->list);
   if (ren != NULL) {
      s->done = TR7_CONS2(tsc, sym, s->done);
      sym = TR7_CAR(TR7_PAIR_CDR(ren));
   }
   return s->callback(tsc, sym, val, s->closure);
}
/*
* import only symbols
*/
static int import_only_cb(tr7_engine_t tsc, tr7_t sym, tr7_t val, void *closure)
{
   import_list_t *s = (import_list_t*)closure;
   if (tr7_memq_pair(sym, s->list) == NULL)
      return 0;
   s->done = TR7_CONS2(tsc, sym, s->done);
   return s->callback(tsc, sym, val, s->closure);
}
/*
* import except symbols
*/
static int import_except_cb(tr7_engine_t tsc, tr7_t sym, tr7_t val, void *closure)
{
   import_list_t *s = (import_list_t*)closure;
   if (tr7_memq_pair(sym, s->list) == NULL)
      return s->callback(tsc, sym, val, s->closure);
   s->done = TR7_CONS2(tsc, sym, s->done);
   return 0;
}
/*
* import prefixed symbol
*/
static int import_prefix_cb(tr7_engine_t tsc, tr7_t sym, tr7_t val, void *closure)
{
   tr7_t nsym;
   import_prefix_t *s = (import_prefix_t*)closure;
   const char *post = tr7_symbol_string(sym);
   size_t length = tr7_symbol_length(sym);
   if (length + s->offset >= sizeof s->buffer)
      return -1; //tsc_error_validity(tsc, "prefixed too long", sym);
   memcpy(&s->buffer[s->offset], post, length);
   nsym = tr7_get_symbol_length(tsc, s->buffer, length + s->offset, 1);
   if (TR7_IS_NIL(nsym))
      return -1; //tsc_oom(tsc);
   return s->callback(tsc, nsym, val, s->closure);
}
/*
* common processing for import only, except, rename
*/
static int import_list(tr7_engine_t tsc, tr7_t set, env_enum_cb_t importer, void *closure, env_enum_cb_t process, int isrename)
{
   int rc;
   tr7_t args, subset, iter, nfound, sym;
   import_list_t s;

   /* from set==(key subset args) extracts subset and args */
   args = TR7_CDR(set);
   if (!TR7_IS_PAIR(args))
      return set_error_msg_obj(tsc, "bad import set", set, 0);
   subset = TR7_CAR(args);
   args = TR7_CDR(args);

   /* check arguments */
   for (iter = args ; TR7_IS_PAIR(iter) ; iter = TR7_CDR(iter)) {
      sym = TR7_CAR(iter);
      if (isrename) {
         if (!TR7_IS_PAIR(sym) || !TR7_IS_PAIR(TR7_CDR(sym))
          || !TR7_IS_NIL(TR7_CDDR(sym)) || !TR7_IS_SYMBOL(TR7_CADR(sym)))
            return set_error_msg_obj(tsc, "bad rename spec", sym, 0);
         sym = TR7_CAR(sym);
      }
      if (!TR7_IS_SYMBOL(sym))
         return set_error_msg_obj(tsc, "not a symbol", sym, 0);
   }
   if (!TR7_IS_NIL(iter))
         return set_error_msg_obj(tsc, "improper import", args, 0);

   /* process */
   s.callback = importer;
   s.closure = closure;
   s.list = args;
   s.done = TR7_NIL;
   rc = import_importset(tsc, subset, process, &s);
   if (rc < 0 || tr7_list_length(s.done) == tr7_list_length(s.list))
      return rc;

   /* some symbol are not found */
   for (nfound = TR7_NIL, iter = args ; TR7_IS_PAIR(iter) ; iter = TR7_CDR(iter)) {
      sym = isrename ? TR7_CAAR(iter) : TR7_CAR(iter);
      if (tr7_memq_pair(sym, s.done) == NULL)
         nfound = TR7_CONS2(tsc, sym, nfound);
   }
   return -1; //tsc_error_validity(tsc, "imported symbol(s) not found", nfound);
}
/*
* prepare importing prefixed symbols
*/
static int import_prefix(tr7_engine_t tsc,  tr7_t set, env_enum_cb_t importer, void *closure)
{
   import_prefix_t s;
   const char *prestr;
   size_t prelen;
   tr7_t pre, subset;

   /* from set==(prefix subset pre) extracts subset and pre */
   pre = TR7_CDR(set);
   if (!TR7_IS_PAIR(pre))
      return set_error_msg_obj(tsc, "bad import set", set, 0);
   subset = TR7_CAR(pre);
   pre = TR7_CDR(pre);
   if (!TR7_IS_PAIR(pre) || !TR7_IS_NIL(TR7_CDR(pre)))
      return set_error_msg_obj(tsc, "bad import set", set, 0);
   pre = TR7_CAR(pre);
   if (!TR7_IS_SYMBOL(pre))
      return set_error_msg_obj(tsc, "bad import set", set, 0);

   /* extract and copy prefix string */
   prestr = tr7_symbol_string(pre);
   prelen = tr7_symbol_length(pre);
   if (prelen >= sizeof s.buffer)
      return set_error_msg_obj(tsc, "prefix too long", pre, 0);

   s.offset = (unsigned)prelen;
   memcpy(s.buffer, prestr, prelen);
   s.callback = importer;
   s.closure = closure;
   return import_importset(tsc, subset, import_prefix_cb, &s);
}
/*
* import any import set
*/
static int import_importset(tr7_engine_t tsc,  tr7_t set, env_enum_cb_t importer, void *closure)
{
   int rc;
   tr7_t head, env;

   /* check if pair */
   if (!TR7_IS_PAIR(set))
      return set_error_msg_obj(tsc, "bad import set", set, 0);
   head = TR7_CAR(set);

   /* check for only, except, rename and prefix */
   if (TR7EQ(head, SYMBOL(ONLY)))
      return import_list(tsc, set, importer, closure, import_only_cb, 0);
   if (TR7EQ(head, SYMBOL(EXCEPT)))
      return import_list(tsc, set, importer, closure, import_except_cb, 0);
   if (TR7EQ(head, SYMBOL(RENAME)))
      return import_list(tsc, set, importer, closure, import_rename_cb, 1);
   if (TR7EQ(head, SYMBOL(PREFIX)))
      return import_prefix(tsc, set, importer, closure);

   /* get/load the library environment in 'env' */
   rc = get_library(tsc, set, &env);
   if (rc < 0) {
      switch (-rc) {
      case Tr7_GetLib_Error_Invalid_Name:
         return set_error_msg_obj(tsc, "invalid library name", set, 0);
      case Tr7_GetLib_Error_Name_Too_Long:
         return set_error_msg_obj(tsc, "library name too long", set, 0);
      case Tr7_GetLib_Error_Not_Found:
         return set_error_msg_obj(tsc, "unable to locate library", set, 0);
      case Tr7_GetLib_Error_Name_Mismatch:
         return set_error_msg_obj(tsc, "found file doesn't export library", set, 0);
      case Tr7_GetLib_Error_Eval:
      default:
         return set_error_msg_obj(tsc, "failed to eval library", set, 0);
      }
   }

   /* process import of of environment */
   return lib_enumerate(tsc, env, importer, closure);
}
/*
* import the list of importset given by 'args' in the environment 'envir'
* does not produce code but add symbols to current given environment
*/
static int import(tr7_engine_t tsc, tr7_t args, tr7_t envir)
{
   int rc;
   tr7_t iter, set;

   /* import all importset of the list */
   for (iter = args ; TR7_IS_PAIR(iter) ; iter = TR7_CDR(iter)) {
      set = TR7_CAR(iter);
      rc = import_importset(tsc, set, import_env_cb, (void*)envir);
      if (rc < 0)
         return rc;
   }
   if (!TR7_IS_NIL(iter))
      return set_error_msg_obj(tsc, "bad import set", iter, 0);
   if (iter == args)
      return set_error_msg_obj(tsc, "empty import set", args, 0);
   return 0;
}
/*
*/
static int importlib(tr7_engine_t tsc, const char *name, unsigned length, tr7_t env)
{
   tr7_t lib;
   int rc = getlib(tsc, name, length, &lib);
   return rc < 0 ? rc : lib_enumerate(tsc, lib, import_env_cb, (void*)env);
}
/*
*/
int tr7_import_lib(tr7_engine_t tsc, const char *name)
{
   return tr7_import_lib_length(tsc, name, strlen(name));
}
int tr7_import_lib_length(tr7_engine_t tsc, const char *name, unsigned length)
{
   return importlib(tsc, name, length, tsc->curenv);
}
/*
************************************************************************
* SECTION: DEFENVS
*/
static void add_null_env(tr7_engine_t tsc, tr7_t env)
{
   builtin_lib_enum_syntax(tsc, 0, import_env_cb, (void*)env);
}
static void add_base_env(tr7_engine_t tsc, tr7_t env)
{
   builtin_lib_enum(tsc, 0, import_env_cb, (void*)env);
}
#if COMMON_ROOT_ENV
static tr7_t defenv_null(tr7_engine_t tsc)
{
   tr7_t env = tsc->null_env;
   if (TR7_IS_NIL(env)) {
      env = tsc->null_env = mk_environment(tsc, TR7_NIL, builtins_lib[0].proc_last);
      add_null_env(tsc, env);
   }
   return env;
}
static tr7_t defenv_base(tr7_engine_t tsc)
{
   tr7_t env = tsc->base_env;
   if (TR7_IS_NIL(env)) {
      env = tsc->base_env = mk_environment(tsc, TR7_NIL, builtins_lib[0].proc_last);
      builtin_lib_enum_proc(tsc, 0, import_env_cb, (void*)env);
   }
   return env;
}
#endif
static tr7_t make_null_environment(tr7_engine_t tsc, unsigned size)
{
#if COMMON_ROOT_ENV
   tr7_t env = mk_environment(tsc, defenv_null(tsc), size);
#else
   tr7_t env = mk_environment(tsc, TR7_NIL, size);
   add_null_env(tsc, env);
#endif
   return env;
}
static tr7_t make_base_environment(tr7_engine_t tsc, unsigned size)
{
#if COMMON_ROOT_ENV
   tr7_t env = mk_environment(tsc, defenv_base(tsc), size);
#else
   tr7_t env = mk_environment(tsc, TR7_NIL, size);
   add_base_env(tsc, env);
#endif
   return env;
}
static tr7_t make_interaction_environment(tr7_engine_t tsc, unsigned size)
{
   tr7_t env = make_base_environment(tsc, size);
#define ADDLIB(n) importlib(tsc, n, (unsigned)strlen(n), env)
#if USE_SCHEME_FILE
   ADDLIB("scheme/file");
#endif
#if USE_SCHEME_LOAD
   ADDLIB("scheme/load");
#endif
#if USE_SCHEME_PROCESS_CONTEXT
   ADDLIB("scheme/process-context");
#endif
#if USE_SCHEME_READ
   ADDLIB("scheme/read");
#endif
#if USE_SCHEME_WRITE
   ADDLIB("scheme/write");
#endif
   return env;
}
/*
************************************************************************
* SECTION GUARDS
* --------------
*
* remove the top guard object
*/
static void guard_pop(tr7_engine_t tsc)
{
   tsc->stof_guards = TR7_ITEM_VECTOR(tsc->stof_guards, Guard_Idx_Previous);
}
/*
* create a new guard objet for the 'handler' of 'type'
* capture the current state for restoring it
*/
static int guard_push(tr7_engine_t tsc, tr7_t handler, guard_type_t type)
{
   tr7_t item = alloc_vector(tsc, Guard_Count_Idx);
   if (TR7_IS_NIL(item))
      return -1;
   TR7_ITEM_VECTOR(item, Guard_Idx_Previous) = tsc->stof_guards; /* link */
   TR7_ITEM_VECTOR(item, Guard_Idx_Handler) = handler;    /* guard */
   TR7_ITEM_VECTOR(item, Guard_Idx_Type) = TR7_FROM_INT(type);    /* type */
   TR7_ITEM_VECTOR(item, Guard_Idx_nData) = TR7_FROM_UINT(tsc->stack.tail - tsc->stack.data);
   TR7_ITEM_VECTOR(item, Guard_Idx_nOper) = TR7_FROM_UINT(tsc->stack.oper - tsc->stack.head);
   TR7_ITEM_VECTOR(item, Guard_Idx_Params) = tsc->stof_params; /* params */
   TR7_ITEM_VECTOR(item, Guard_Idx_DynWind) = tsc->stof_dynawinds; /* dyn-wind */
   tsc->stof_guards = item;
   return 0;
}
/*
* raise the error in current value, use current guard to process it.
*/
static eval_status_t do_raise(tr7_engine_t tsc, int continuable)
{
   guard_type_t type;
   tr7_t handler, guard, errobj = tsc->values[0];

   for(;;) {
      /* get current guard */
      guard = tsc->stof_guards;
      if (TR7_IS_NIL(guard))
         return return_single(tsc, errobj, Cycle_Leave_Error);
      /* pop current guard and ensure it is not discarded before used */
      push_recent_alloc(tsc, guard);
      tsc->stof_guards = TR7_ITEM_VECTOR(guard, Guard_Idx_Previous);
      /* restore the found guard environment */
      handler = TR7_ITEM_VECTOR(guard, Guard_Idx_Handler);
      type = (guard_type_t)TR7_TO_INT(TR7_ITEM_VECTOR(guard, Guard_Idx_Type));
      if (!continuable || type != Guard_Type_Handler) {
         tsc->no_stack = 0;
         tsc->stack.data = tsc->stack.tail - TR7_TO_INT(TR7_ITEM_VECTOR(guard, Guard_Idx_nData));
         tsc->stack.oper = tsc->stack.head + TR7_TO_INT(TR7_ITEM_VECTOR(guard, Guard_Idx_nOper));
         tsc->stof_params = TR7_ITEM_VECTOR(guard, Guard_Idx_Params);
      }
      tsc->stof_dynawinds = TR7_ITEM_VECTOR(guard, Guard_Idx_DynWind);
      /* inspect the found guard type */
      switch (type) {
      case Guard_Type_Guard:
         OPER_AT(tsc, XRUN_Idx_PC) = handler;
         return return_single(tsc, errobj, Cycle_Return);
      case Guard_Type_Handler:
         guard_push(tsc, tsc->stof_guards, Guard_Type_Repeat);
         if (!continuable)
            oper_push_safe_2(tsc, OPER(RERAISE), errobj);
         return s_exec_1(tsc, handler, errobj);
      case Guard_Type_Root:
         oper_push_safe_2(tsc, OPER(REPL_GUARD), errobj);
         return Cycle_Goto;
      case Guard_Type_Leave:
         return return_single(tsc, errobj, Cycle_Leave_Error);
      case Guard_Type_Repeat:
         tsc->stof_guards = handler;
         break;
      }
   }
}
/*
************************************************************************
* SECTION OPERATORS
* -----------------
* operators act at very low level, on stack and on execution status
*
* implement OPERID(IEVAL)
* compile arg0 and run it on success or throw the error
*/
static eval_status_t _oper_ieval(tr7_engine_t tsc)
{
   tr7_t prog = OPER_AT(tsc, 1);
#if USE_TR7_DEBUG && DEBUG_LINES
   int rc = main_compile(tsc, prog, TR7_VOID, TR7_NIL);
#else
   int rc = main_compile(tsc, prog);
#endif
   OPER_POP(tsc, 2);
   return rc >= 0 ? s_prog(tsc, tsc->values[0]) : Cycle_Raise;
}
/*
* implement OPERID(SENV)
* set the environment from arg0
*/
static eval_status_t _oper_senv(tr7_engine_t tsc)
{
   tsc->curenv = OPER_AT(tsc, 1);
   OPER_POP(tsc, 2);
   return Cycle_Goto;
}
/*
* implement OPERID(XCALL) NARGS PROC ARGS...
* calls arg1 with the arg0 count of arguments starting at arg2
*/
static eval_status_t _oper_xcall(tr7_engine_t tsc)
{
   int nargs = (int)TR7_TO_INT(OPER_AT(tsc,1));
   tr7_t proc = OPER_AT(tsc,2);
   OPER_POP(tsc, 3);
   return execute_call(tsc, proc, nargs);
}
/*
* implement OPERID(PROG) LAMBDA
* make an initial closure and and call it
*/
static eval_status_t _oper_prog(tr7_engine_t tsc)
{
   tr7_t prog = OPER_AT(tsc, 1);
   OPER_POP(tsc, 2);
   return s_prog(tsc, prog);
}
/*
* implement OPERID(LEAVE)
* leave the current REPL
*/
static eval_status_t _oper_leave(tr7_engine_t tsc)
{
   OPER_POP(tsc, 1);
   guard_pop(tsc);
   return Cycle_Leave;
}
/*
* implement OPERID(CONT)
* after processing dynamic winds, activates the continuation
* given at arg0 with arg1 count of args starting at arg2
*/
static eval_status_t _oper_cont(tr7_engine_t tsc)
{
   tr7_continuation_t item = TR7_TO_CONTINUATION(OPER_AT(tsc, 1));
   int nargs = (int)TR7_TO_INT(OPER_AT(tsc, 2));
   OPER_POP(tsc, 3);
   tsc->stof_dynawinds = item->dynawind;
   tsc->stof_params = item->params;
   return do_pop_status_pop(tsc, nargs, Cycle_Return);
}
/*
* helper for implementation of dynamic wind OPERID(DWBEFORE) and OPERID(DWAFTER)
*/
static eval_status_t _oper_dwhandle(tr7_engine_t tsc, unsigned index)
{
   tr7_t handler, dw = OPER_AT(tsc, 1);
   OPER_POP(tsc, 2);
   handler = TR7_ITEM_VECTOR(dw, index);
   tsc->stof_dynawinds = TR7_ITEM_VECTOR(dw, DynaWind_Idx_Previous);
   tsc->stof_params = TR7_ITEM_VECTOR(dw, DynaWind_Idx_Params);
   return execute_call(tsc, handler, 0);
}
/*
* implement OPERID(DWBEFORE)
*/
static eval_status_t _oper_dwbefore(tr7_engine_t tsc)
{
   return _oper_dwhandle(tsc, DynaWind_Idx_Before);
}
/*
* implement OPERID(DWAFTER)
*/
static eval_status_t _oper_dwafter(tr7_engine_t tsc)
{
   return _oper_dwhandle(tsc, DynaWind_Idx_After);
}
/*
* implement OPERID(DWPOP)
*/
static eval_status_t _oper_dwpop(tr7_engine_t tsc)
{
   OPER_POP(tsc, 1);
   tsc->stof_dynawinds = TR7_ITEM_VECTOR(tsc->stof_dynawinds, DynaWind_Idx_Previous);
   return Cycle_Return;
}
/*
* implement OPERID(MKPARAMCVT)
*/
static eval_status_t _oper_mkparamcvt(tr7_engine_t tsc)
{
   tr7_t par = OPER_AT(tsc, 1);
   OPER_POP(tsc, 2);
   TR7_TO_PARAMETER(par)->value = tsc->values[0];
   return return_single(tsc, par, Cycle_Return);
}
/*
* implement OPERID(PARAMCVT)
*/
static eval_status_t _oper_paramcvt(tr7_engine_t tsc)
{
   tr7_t par = OPER_AT(tsc, 1);
   OPER_POP(tsc, 2);
   parameter_set(tsc, par, tsc->values[0]);
   return Cycle_Return;
}
/*
* implement OPERID(PARAMPOP1)
*/
static eval_status_t _oper_parampop1(tr7_engine_t tsc)
{
   OPER_POP(tsc, 1);
   parameter_pop(tsc, 1);
   return Cycle_Continue;
}
/*
* implement OPERID(RERAISE)
*/
static eval_status_t _oper_reraise(tr7_engine_t tsc)
{
   tr7_t err = OPER_AT(tsc, 1);
   OPER_POP(tsc, 2);
   return raise_error(tsc, err);
}
/*************************************************************************
* SECTION SCHEME_BASE
* -------------------
*/

/* implement 'make-parameter' */
static eval_status_t proc_mkparam(tr7_engine_t tsc, int nargs)
{
   tr7_t param, ini, cvt;

   ini = DATA(tsc, 0);
   param = mk_parameter(tsc, TR7_NIL, TR7_NIL);
   if (nargs == 1) {
      /* no converter, fast path */
      TR7_TO_PARAMETER(param)->value = ini;
      return do_pop_continue_single(tsc, 1, param);
   }
   /* with converter */
   cvt = DATA(tsc, 1);
   TR7_TO_PARAMETER(param)->converter = cvt;
   DATA_POP_N(tsc, 2);
   oper_push_safe_2(tsc, OPER(MKPARAMCVT), param);
   return s_exec_1(tsc, cvt, ini);
}

static eval_status_t proc_with_exception_handler(tr7_engine_t tsc, int nargs)
{
   set_values_multi(tsc, 2, &DATA(tsc, 0));/*for GC*/
   DATA_POP_N(tsc, 2);
   guard_push(tsc, tsc->values[0], Guard_Type_Handler);
   return s_exec_0(tsc, tsc->values[1]);
}

/* implement 'raise' */
static eval_status_t proc_raise(tr7_engine_t tsc, int nargs)
{
   return raise_error(tsc, DATA_POP(tsc));
}

/* implement 'raise-continuable' */
static eval_status_t proc_raise_continuable(tr7_engine_t tsc, int nargs)
{
   return do_pop_status_pop(tsc, nargs, Cycle_Raise_Cont);
}

/* implement 'error' */
static eval_status_t proc_error(tr7_engine_t tsc, int nargs)
{
   tr7_t irr = tr7_cons_n(tsc, nargs - 1, &DATA(tsc, 1), TR7_NIL);
   tr7_t err = make_error(tsc, RECORD_DESC(error), DATA(tsc, 0), irr);
   return raise_error(tsc, err);
}

/* implement 'error-object?' */
static eval_status_t proc_is_error_object(tr7_engine_t tsc, int nargs)
{
   return do_pop_continue_boolean(tsc, 1, tr7_is_error(DATA(tsc, 0)));
}

/* implement 'error-object-message' */
static eval_status_t proc_error_msg(tr7_engine_t tsc, int nargs)
{
   return do_pop_continue_single(tsc, 1, tr7_error_message(DATA(tsc, 0)));
}

/* implement 'error-object-irritants' */
static eval_status_t proc_error_irritants(tr7_engine_t tsc, int nargs)
{
   return do_pop_continue_single(tsc, 1, tr7_error_irritants(DATA(tsc, 0)));
}

/* implement 'read-error?' */
static eval_status_t proc_is_read_error(tr7_engine_t tsc, int nargs)
{
   return do_pop_continue_boolean(tsc, 1, tr7_is_read_error(DATA(tsc, 0)));
}

/* implement 'file-error?' */
static eval_status_t proc_is_file_error(tr7_engine_t tsc, int nargs)
{
   return do_pop_continue_boolean(tsc, 1, tr7_is_file_error(DATA(tsc, 0)));
}
/*************************************************************************
* SECTION SCHEME_LAZY
* -------------------
*/
#if USE_SCHEME_LAZY
/*
* operator receive the evaluation of a 'force'
*/
static eval_status_t _oper_save_forced(tr7_engine_t tsc)
{
   tr7_t x = tsc->values[0];
   tr7_promise_t pro = TR7_TO_PROMISE(OPER_AT(tsc, 1));
   OPER_POP(tsc, 2);
   pro->head = TR7_PROMISE_HEAD_VALUE;
   pro->item = x;
   return Cycle_Return;
}
/*
* implement evaluation of forced promise
*/
static eval_status_t do_force(tr7_engine_t tsc, tr7_promise_t pro)
{
   /* prepare continuation after evaluation */
   tr7_t op = pro->head == TR7_PROMISE_HEAD_DELAY ? OPER(SAVE_FORCED) : OPER(FORCE_DELAYED);
   oper_push_safe_2(tsc, op, TR7_FROM_PROMISE(pro));
   /* trig evaluation */
   return execute_call(tsc, pro->item, 0);
}
/*
* operator receive the evaluation of a 'delay-force'
*/
static eval_status_t _oper_force_delayed(tr7_engine_t tsc)
{
   tr7_t x, promise = OPER_AT(tsc, 1);
   tr7_promise_t pro = TR7_TO_PROMISE(promise);
   OPER_POP(tsc, 2);

   x = tsc->values[0];

   if (TR7_IS_PROMISE(x)) {
      tr7_promise_t subpro = TR7_TO_PROMISE(x);
      if (subpro->head != TR7_PROMISE_HEAD_VALUE) {
         /* prepare setting of promise */
         oper_push_safe_2(tsc, OPER(SAVE_FORCED), promise);
         /* force x */
         return do_force(tsc, subpro);
      }
      tsc->values[0] = x = subpro->item;
   }

   pro->head = TR7_PROMISE_HEAD_VALUE;
   pro->item = x;
   return Cycle_Return;
}
/*
* implement 'force'
*/
static eval_status_t proc_force(tr7_engine_t tsc, int nargs)
{
   /* check if promise */
   tr7_t x = DATA(tsc, 0);
   if (TR7_IS_PROMISE(x)) {
      /* check if promise has its value */
      tr7_promise_t pro = TR7_TO_PROMISE(x);
      if (pro->head == TR7_PROMISE_HEAD_VALUE)
         x = pro->item; /* yes */
      else {
         /* effective force */
         DATA_POP_N(tsc, 1);
         return do_force(tsc, pro);
      }
   }
   return do_pop_continue_single(tsc, 1, x);
}
/*
* implement 'promise?'
*/
static eval_status_t proc_is_promise(tr7_engine_t tsc, int nargs)
{
   return do_pop_continue_boolean(tsc, 1, TR7_IS_PROMISE(DATA(tsc, 0)));
}
/*
* implement 'make-promise'
*/
static eval_status_t proc_make_promise(tr7_engine_t tsc, int nargs)
{
   tr7_t x = DATA(tsc, 0);
   if (!TR7_IS_PROMISE(x))
      x = mk_promise(tsc, TR7_PROMISE_HEAD_VALUE, x);
   return do_pop_continue_single(tsc, 1, x);
}
#endif
/*************************************************************************
* SECTION SRFI_136_PROCS
* ----------------------
* Definition of record and record type
*/
#if USE_SRFI_136
/*
* implement 'record?'
*/
static eval_status_t proc_is_record(tr7_engine_t tsc, int nargs)
{
   return do_pop_continue_boolean(tsc, 1, tr7_is_record(DATA(tsc, 0)));
}
/*
* implement 'record-type-descriptor?'
*/
static eval_status_t proc_is_record_desc(tr7_engine_t tsc, int nargs)
{
   return do_pop_continue_boolean(tsc, 1, tr7_is_record_desc(DATA(tsc, 0)));
}
/*
* implement 'record-type-descriptor'
*/
static eval_status_t proc_record_desc(tr7_engine_t tsc, int nargs)
{
   return do_pop_continue_single(tsc, 1, tr7_record_desc(DATA(tsc, 0)));
}
/*
* implement 'record-type-predicate'
*/
static eval_status_t proc_record_desc_pred(tr7_engine_t tsc, int nargs)
{
   return do_pop_continue_single(tsc, 1, mk_record_predicate(tsc, DATA(tsc, 0)));
}
/*
* implement 'record-type-name'
*/
static eval_status_t proc_record_desc_name(tr7_engine_t tsc, int nargs)
{
   return do_pop_continue_single(tsc, 1, tr7_record_desc_name(DATA(tsc, 0)));
}
/*
* implement 'record-type-parent'
*/
static eval_status_t proc_record_desc_parent(tr7_engine_t tsc, int nargs)
{
   return do_pop_continue_single(tsc, 1, tr7_record_desc_parent(DATA(tsc, 0)));
}

/* implement 'record-type-fields' */
static eval_status_t proc_record_desc_fields(tr7_engine_t tsc, int nargs)
{
   return do_pop_continue_single(tsc, 1, record_desc_fields_srfi136(tsc, DATA(tsc, 0)));
}
/*
* implement 'make-record-type-descriptor'
*/
static eval_status_t proc_make_record_desc(tr7_engine_t tsc, int nargs)
{
   tr7_t resu, name = DATA(tsc, 0);
   tr7_t fields = DATA(tsc, 1);
   tr7_t parent = nargs > 2 ? DATA(tsc, 2) : TR7_FALSE;
   if (!TR7_IS_FALSE(parent) && !tr7_is_record_desc(parent))
      return raise_error_msg_obj(tsc, "bad parent", parent);
   resu = make_record_type_srfi136(tsc, name, parent, fields);
   if (TR7_IS_VOID(resu))
      return raise_error_msg(tsc, "error type definition");
   return do_pop_continue_single(tsc, nargs, resu);
}
/*
* implement 'make-record'
*/
static eval_status_t proc_make_record(tr7_engine_t tsc, int nargs)
{
   tr7_t re = mk_record_instance(tsc, DATA(tsc, 0), DATA(tsc, 1));
   if (TR7_IS_VOID(re))
      return raise_error_msg(tsc, "creation of record failed");
   return do_pop_continue_single(tsc, 2, re);
}
#endif
/*************************************************************************
* SECTION EQUIVALENC_PROCS
* ----------------
*
* implement 'eq?'
*/
static eval_status_t proc_eq(tr7_engine_t tsc, int nargs)
{
   int res = TR7EQ(DATA(tsc, 0), DATA(tsc, 1));
   return do_pop_continue_boolean(tsc, 2, res);
}
/*
* implement 'eqv?'
*/
static eval_status_t proc_eqv(tr7_engine_t tsc, int nargs)
{
   int res = tr7_eqv(DATA(tsc, 0), DATA(tsc, 1));
   return do_pop_continue_boolean(tsc, 2, res);
}
/*
* implement 'equal?'
*/
static eval_status_t proc_equal(tr7_engine_t tsc, int nargs)
{
   int res = tr7_equal(DATA(tsc, 0), DATA(tsc, 1));
   return do_pop_continue_boolean(tsc, 2, res);
}
/*
* helper for any hash procedure
*/
static eval_status_t hash_bounding(tr7_engine_t tsc, int nargs, tr7_uint_t h)
{
   if (nargs > 1)
      h = h % TR7_TO_UINT(DATA(tsc, 1));
   else
      h = h & TR7_INT_MAX_VAL;
   return do_pop_continue_integer(tsc, nargs, (tr7_int_t)h);
}
/*
* implement 'hash'
*/
static eval_status_t proc_hash(tr7_engine_t tsc, int nargs)
{
   tr7_uint_t h = hash_any(DATA(tsc, 0));
   return hash_bounding(tsc, nargs, h);
}
/*
* implement 'hash-by-identity'
*/
static eval_status_t proc_hash_by_identity(tr7_engine_t tsc, int nargs)
{
   tr7_uint_t h = TR7_TO_UINT(DATA(tsc, 0));
   return hash_bounding(tsc, nargs, h);
}
/*************************************************************************
* SECTION NUMBER_PROCS
* --------------------
*
* implement 'number?'
*/
static eval_status_t proc_is_number(tr7_engine_t tsc, int nargs)
{
   return do_pop_continue_boolean(tsc, 1, tr7_is_number(DATA(tsc, 0)));
}

/* implement 'integer?' */
static eval_status_t proc_is_integer(tr7_engine_t tsc, int nargs)
{
   return do_pop_continue_boolean(tsc, 1, tr7_is_integer(DATA(tsc, 0)));
}

/* implement 'real?' */
static eval_status_t proc_is_real(tr7_engine_t tsc, int nargs)
{
   return do_pop_continue_boolean(tsc, 1, tr7_is_number(DATA(tsc, 0)));      /* All numbers are real */
}

/* implement 'complex?' */
static eval_status_t proc_is_complex(tr7_engine_t tsc, int nargs)
{
   return do_pop_continue_boolean(tsc, 1, tr7_is_number(DATA(tsc, 0)));
}

/* implement 'rational?' */
static eval_status_t proc_is_rational(tr7_engine_t tsc, int nargs)
{
   return do_pop_continue_boolean(tsc, 1, tr7_is_integer(DATA(tsc, 0)));
}

/* implement 'exact?' */
static eval_status_t proc_is_exact(tr7_engine_t tsc, int nargs)
{
   return do_pop_continue_boolean(tsc, 1, tr7_is_exact(DATA(tsc, 0)));
}

/* implement 'inexact?' */
static eval_status_t proc_is_inexact(tr7_engine_t tsc, int nargs)
{
   return do_pop_continue_boolean(tsc, 1, !tr7_is_exact(DATA(tsc, 0)));
}

/* implement 'exact-integer?' */
static eval_status_t proc_is_exact_int(tr7_engine_t tsc, int nargs)
{
   return do_pop_continue_boolean(tsc, 1, tr7_is_exact_integer(DATA(tsc, 0)));
}

/* implement 'zero?' */
static eval_status_t proc_is_zero(tr7_engine_t tsc, int nargs)
{
   tr7_t x = DATA(tsc, 0);
   int res = TR7_IS_INT(x) ? TR7_TO_INT(x) == 0 : *TR7_TO_DOUBLE(x) == 0;
   return do_pop_continue_boolean(tsc, 1, res);
}

/* implement 'positive?' */
static eval_status_t proc_is_positive(tr7_engine_t tsc, int nargs)
{
   tr7_t x = DATA(tsc, 0);
   int res = TR7_IS_INT(x) ? TR7_TO_INT(x) > 0 : *TR7_TO_DOUBLE(x) > 0;
   return do_pop_continue_boolean(tsc, 1, res);
}

/* implement 'negative?' */
static eval_status_t proc_is_negative(tr7_engine_t tsc, int nargs)
{
   tr7_t x = DATA(tsc, 0);
   int res = TR7_IS_INT(x) ? TR7_TO_INT(x) < 0 : *TR7_TO_DOUBLE(x) < 0;
   return do_pop_continue_boolean(tsc, 1, res);
}

/* implement 'odd?' */
static eval_status_t proc_is_odd(tr7_engine_t tsc, int nargs)
{
   tr7_t x = DATA(tsc, 0);
   return do_pop_continue_boolean(tsc, 1, 0 != (TR7_TO_INT(x) & 1));
}

/* implement 'even?' */
static eval_status_t proc_is_even(tr7_engine_t tsc, int nargs)
{
   tr7_t x = DATA(tsc, 0);
   return do_pop_continue_boolean(tsc, 1, 0 == (TR7_TO_INT(x) & 1));
}

/* operate number comparison */
static eval_status_t do_number_compare_from(tr7_engine_t tsc, int nargs, tr7_compare_t comp, int idx, tr7_t a)
{
   tr7_t r = TR7_TRUE;
   if (idx < nargs)
      for (;;) {
         tr7_t b = DATA(tsc, idx);
         if (0 == (comp & tr7_cmp_num(a, b)))
            r = TR7_FALSE;
         else if (++idx < nargs) {
            a = b;
            continue;
         }
         break;
      }
   return do_pop_continue_single(tsc, nargs, r);
}

static eval_status_t do_number_compare(tr7_engine_t tsc, int nargs, tr7_compare_t comp)
{
   return do_number_compare_from(tsc, nargs, comp, 1, DATA(tsc, 0));
}

/* implement '=' */
static eval_status_t proc_number_eq(tr7_engine_t tsc, int nargs)
{
   int idx;
   tr7_t r, b, a = DATA(tsc, 0);
   if (!TR7_IS_INT(a))
      return do_number_compare(tsc, nargs, Tr7_Cmp_Equal);
   for (r = TR7_TRUE, idx = 1 ;;) {
      b = DATA(tsc, idx);
      if (!TR7_IS_INT(b))
         return do_number_compare_from(tsc, nargs, Tr7_Cmp_Equal, idx, a);
      if (((tr7_int_t)a) != ((tr7_int_t)b)) /* tr7_t and tr7_int_t: same order */
         r = TR7_FALSE;
      else if (++idx < nargs)
         continue;
      break;
   }
   return do_pop_continue_single(tsc, nargs, r);
}

/* implement '<' */
static eval_status_t proc_number_lt(tr7_engine_t tsc, int nargs)
{
   int idx;
   tr7_t r, b, a = DATA(tsc, 0);
   if (!TR7_IS_INT(a))
      return do_number_compare(tsc, nargs, Tr7_Cmp_Lesser);
   for (r = TR7_TRUE, idx = 1 ;;) {
      b = DATA(tsc, idx);
      if (!TR7_IS_INT(b))
         return do_number_compare_from(tsc, nargs, Tr7_Cmp_Lesser, idx, a);
      if (((tr7_int_t)a) >= ((tr7_int_t)b)) /* tr7_t and tr7_int_t: same order */
         r = TR7_FALSE;
      else if (++idx < nargs) {
         a = b;
         continue;
      }
      break;
   }
   return do_pop_continue_single(tsc, nargs, r);
}

/* implement '>' */
static eval_status_t proc_number_gt(tr7_engine_t tsc, int nargs)
{
   int idx;
   tr7_t r, b, a = DATA(tsc, 0);
   if (!TR7_IS_INT(a))
      return do_number_compare(tsc, nargs, Tr7_Cmp_Greater);
   for (r = TR7_TRUE, idx = 1 ;;) {
      b = DATA(tsc, idx);
      if (!TR7_IS_INT(b))
         return do_number_compare_from(tsc, nargs, Tr7_Cmp_Greater, idx, a);
      if (((tr7_int_t)a) <= ((tr7_int_t)b)) /* tr7_t and tr7_int_t: same order */
         r = TR7_FALSE;
      else if (++idx < nargs) {
         a = b;
         continue;
      }
      break;
   }
   return do_pop_continue_single(tsc, nargs, r);
}

/* implement '<=' */
static eval_status_t proc_number_le(tr7_engine_t tsc, int nargs)
{
   int idx;
   tr7_t r, b, a = DATA(tsc, 0);
   if (!TR7_IS_INT(a))
      return do_number_compare(tsc, nargs, Tr7_Cmp_Lesser_Or_Equal);
   for (r = TR7_TRUE, idx = 1 ;;) {
      b = DATA(tsc, idx);
      if (!TR7_IS_INT(b))
         return do_number_compare_from(tsc, nargs, Tr7_Cmp_Lesser_Or_Equal, idx, a);
      if (((tr7_int_t)a) > ((tr7_int_t)b)) /* tr7_t and tr7_int_t: same order */
         r = TR7_FALSE;
      else if (++idx < nargs) {
         a = b;
         continue;
      }
      break;
   }
   return do_pop_continue_single(tsc, nargs, r);
}

/* implement '>=' */
static eval_status_t proc_number_ge(tr7_engine_t tsc, int nargs)
{
   int idx;
   tr7_t r, b, a = DATA(tsc, 0);
   if (!TR7_IS_INT(a))
      return do_number_compare(tsc, nargs, Tr7_Cmp_Greater_Or_Equal);
   for (r = TR7_TRUE, idx = 1 ;;) {
      b = DATA(tsc, idx);
      if (!TR7_IS_INT(b))
         return do_number_compare_from(tsc, nargs, Tr7_Cmp_Greater_Or_Equal, idx, a);
      if (((tr7_int_t)a) < ((tr7_int_t)b)) /* tr7_t and tr7_int_t: same order */
         r = TR7_FALSE;
      else if (++idx < nargs) {
         a = b;
         continue;
      }
      break;
   }
   return do_pop_continue_single(tsc, nargs, r);
}

static eval_status_t do_math_fun_from(tr7_engine_t tsc, int nargs, void (*func)(any_num_t*, tr7_t), int idx, any_num_t *n)
{
   while (idx < nargs)
      func(n,  DATA(tsc, idx++));
   return do_pop_continue_single(tsc, nargs, any_num_get(n));
}

static eval_status_t do_math_fun_noint(tr7_engine_t tsc, int nargs, void (*func)(any_num_t*, tr7_t), int idx, tr7_int_t val)
{
   any_num_t n;
   any_num_make_double(tsc, &n, (double)val); /* !!! TODO use big int instead of entering doubles */
   return do_math_fun_from(tsc, nargs, func, idx, &n);
}

static eval_status_t do_math_fun(tr7_engine_t tsc, int nargs, void (*func)(any_num_t*, tr7_t))
{
   any_num_t n;
   any_num_make(tsc, &n, DATA(tsc, 0));
   return do_math_fun_from(tsc, nargs, func, 1, &n);
}

static eval_status_t do_math_opp(tr7_engine_t tsc, int init, void (*func)(any_num_t*, tr7_t))
{
   any_num_t n;
   any_num_make_double(tsc, &n, init);
   func(&n,  DATA(tsc, 0));
   return do_pop_continue_single(tsc, 1, any_num_get(&n));
}

/* implement 'max' */
static eval_status_t proc_max(tr7_engine_t tsc, int nargs)
{
   int idx;
   tr7_t b, r = DATA(tsc, 0);
   if (!TR7_IS_INT(r))
      return do_math_fun(tsc, nargs, any_num_max);
   for (idx = 1 ;; idx++) {
      if (idx >= nargs)
         return do_pop_continue_single(tsc, nargs, r);
      b = DATA(tsc, idx);
      if (!TR7_IS_INT(b))
         return do_math_fun_noint(tsc, nargs, any_num_max, idx, r);
      if (((tr7_int_t)r) < ((tr7_int_t)b)) /* tr7_t and tr7_int_t: same order */
         r = b;
   }
}

/* implement 'min' */
static eval_status_t proc_min(tr7_engine_t tsc, int nargs)
{
   int idx;
   tr7_t b, r = DATA(tsc, 0);
   if (!TR7_IS_INT(r))
      return do_math_fun(tsc, nargs, any_num_min);
   for (idx = 1 ;; idx++) {
      if (idx >= nargs)
         return do_pop_continue_single(tsc, nargs, r);
      b = DATA(tsc, idx);
      if (!TR7_IS_INT(b))
         return do_math_fun_noint(tsc, nargs, any_num_min, idx, r);
      if (((tr7_int_t)r) > ((tr7_int_t)b)) /* tr7_t and tr7_int_t: same order */
         r = b;
   }
}

/* implement '+' */
static eval_status_t proc_add(tr7_engine_t tsc, int nargs)
{
   int idx;
   tr7_t t;
   tr7_int_t r, a;
   if (nargs == 0)
      t = TR7_FROM_INT(0);
   else {
      t = DATA(tsc, 0);
      if (!TR7_IS_INT(t))
         return do_math_fun(tsc, nargs, any_num_add);
      r = TR7_TO_INT(t);
      for (idx = 1 ; idx < nargs ; idx++) {
         t = DATA(tsc, idx);
         if (TR7_IS_INT(t)) {
            a = TR7_TO_INT(t);
            if (!overflow_add(r, a, &r))
               continue;
         }
         return do_math_fun_noint(tsc, nargs, any_num_add, idx, r);
      }
      t = TR7_FROM_INT_OVERFLOW(tsc, r);
   }
   return do_pop_continue_single(tsc, nargs, t);
}

/* implement '-' */
static eval_status_t proc_sub(tr7_engine_t tsc, int nargs)
{
   int idx, done;
   tr7_t t;
   tr7_int_t r, a;
   t = DATA(tsc, 0);
   if (nargs == 1) {
      if (!TR7_IS_INT(t))
         done = 0;
      else {
         r = -TR7_TO_INT(t);
         done = r == 0 || t != TR7_FROM_INT(r);
      }
      if (!done)
         return do_math_opp(tsc, 0, any_num_sub);
   }
   else {
      t = DATA(tsc, 0);
      if (!TR7_IS_INT(t))
         return do_math_fun(tsc, nargs, any_num_sub);
      r = TR7_TO_INT(t);
      for (idx = 1 ; idx < nargs ; idx++) {
         t = DATA(tsc, idx);
         if (TR7_IS_INT(t)) {
            a = TR7_TO_INT(t);
            if (!overflow_sub(r, a, &r))
               continue;
         }
         return do_math_fun_noint(tsc, nargs, any_num_sub, idx, r);
      }
   }
   t = TR7_FROM_INT_OVERFLOW(tsc, r);
   return do_pop_continue_single(tsc, nargs, t);
}

/* implement '*' */
static eval_status_t proc_mul(tr7_engine_t tsc, int nargs)
{
   int idx;
   tr7_t t;
   tr7_int_t r, a;
   if (nargs == 0)
      t = TR7_FROM_INT(1);
   else {
      t = DATA(tsc, 0);
      if (!TR7_IS_INT(t))
         return do_math_fun(tsc, nargs, any_num_mul);
      r = TR7_TO_INT(t);
      for (idx = 1 ; idx < nargs ; idx++) {
         t = DATA(tsc, idx);
         if (TR7_IS_INT(t)) {
            a = TR7_TO_INT(t);
            if (!overflow_mul(r, a, &r))
               continue;
         }
         return do_math_fun_noint(tsc, nargs, any_num_mul, idx, r);
      }
      t = TR7_FROM_INT_OVERFLOW(tsc, r);
   }
   return do_pop_continue_single(tsc, nargs, t);
}

/* implement '/' */
static eval_status_t proc_div(tr7_engine_t tsc, int nargs)
{
   any_num_t n;
   int idx = 0;
   if (nargs == 1)
      any_num_make_int(tsc, &n, 1);
   else
      any_num_make(tsc, &n, DATA(tsc, idx++));
   while (idx < nargs)
      if (!any_num_div(&n,  DATA(tsc, idx++)))
         return raise_division_by_zero_error(tsc); /* TODO NEW-STYLE-ERROR */
   return do_pop_continue_single(tsc, nargs, any_num_get(&n));
}

/* implement 'abs' */
static eval_status_t proc_abs(tr7_engine_t tsc, int nargs)
{
   any_num_t n;
   any_num_make(tsc, &n, DATA(tsc, 0));
   any_num_abs(&n);
   return do_pop_continue_single(tsc, nargs, any_num_get(&n));
}

/* implement 'floor/' */
static eval_status_t proc_floor_div(tr7_engine_t tsc, int nargs)
{
   any_num_t n, nn;
   any_num_make(tsc, &n, DATA(tsc, 0));
   if (!any_num_div_floor(&n, &nn, DATA(tsc, 1)))
      return raise_division_by_zero_error(tsc); /* TODO NEW-STYLE-ERROR */
   return do_pop_continue_2_values(tsc, nargs, any_num_get(&n), any_num_get(&nn));
}

/* implement 'floor-quotient' */
static eval_status_t proc_floor_quotient(tr7_engine_t tsc, int nargs)
{
   any_num_t n, nn;
   any_num_make(tsc, &n, DATA(tsc, 0));
   if (!any_num_div_floor(&n, &nn, DATA(tsc, 1)))
      return raise_division_by_zero_error(tsc);
   return do_pop_continue_single(tsc, nargs, any_num_get(&n));
}

/* implement 'floor-remainder' / 'modulo' */
static eval_status_t proc_floor_rem(tr7_engine_t tsc, int nargs)
{
   any_num_t n, nn;
   any_num_make(tsc, &n, DATA(tsc, 0));
   if (!any_num_div_floor(&n, &nn, DATA(tsc, 1)))
      return raise_division_by_zero_error(tsc);
   return do_pop_continue_single(tsc, nargs, any_num_get(&nn));
}

/* implement 'truncate/' */
static eval_status_t proc_truncate_div(tr7_engine_t tsc, int nargs)
{
   any_num_t n, nn;
   any_num_make(tsc, &n, DATA(tsc, 0));
   if (!any_num_div_trunc(&n, &nn, DATA(tsc, 1)))
      return raise_division_by_zero_error(tsc); /* TODO NEW-STYLE-ERROR */
   return do_pop_continue_2_values(tsc, nargs, any_num_get(&n), any_num_get(&nn));
}

/* implement 'truncate-quotient' / 'quotient' */
static eval_status_t proc_truncate_quotient(tr7_engine_t tsc, int nargs)
{
   any_num_t n, nn;
   any_num_make(tsc, &n, DATA(tsc, 0));
   if (!any_num_div_trunc(&n, &nn, DATA(tsc, 1)))
      return raise_division_by_zero_error(tsc); /* TODO NEW-STYLE-ERROR */
   return do_pop_continue_single(tsc, nargs, any_num_get(&n));
}

/* implement 'truncate-remainder' / 'remainder' */
static eval_status_t proc_truncate_rem(tr7_engine_t tsc, int nargs)
{
   any_num_t n, nn;
   any_num_make(tsc, &n, DATA(tsc, 0));
   if (!any_num_div_trunc(&n, &nn, DATA(tsc, 1)))
      return raise_division_by_zero_error(tsc); /* TODO NEW-STYLE-ERROR */
   return do_pop_continue_single(tsc, nargs, any_num_get(&nn));
}

/* implement 'gcd' */
static eval_status_t proc_gcd(tr7_engine_t tsc, int nargs)
{
   any_num_t n;
   int idx = 0;
   if (nargs == 0)
      return do_pop_continue_single(tsc, 0, TR7_FROM_INT(0));
   any_num_make(tsc, &n, DATA(tsc, idx));
   while (++idx < nargs)
      any_num_gcd(&n,  DATA(tsc, idx));
   return do_pop_continue_single(tsc, nargs, any_num_get(&n));
}

/* implement 'lcm' */
static eval_status_t proc_lcm(tr7_engine_t tsc, int nargs)
{
   any_num_t n;
   int idx = 0;
   if (nargs == 0)
      return do_pop_continue_single(tsc, 0, TR7_FROM_INT(1));
   any_num_make(tsc, &n, DATA(tsc, idx));
   while (++idx < nargs)
      any_num_lcm(&n,  DATA(tsc, idx));
   return do_pop_continue_single(tsc, nargs, any_num_get(&n));
}

/* implement 'square' */
static eval_status_t proc_square(tr7_engine_t tsc, int nargs)
{
   any_num_t n;
   tr7_t x = DATA(tsc, 0);
   any_num_make(tsc, &n, x);
   any_num_mul(&n, x);
   return do_pop_continue_single(tsc, 1, any_num_get(&n));
}

/* implement 'exact-integer-sqrt' */
static eval_status_t proc_int_sqrt(tr7_engine_t tsc, int nargs)
{
   any_num_t n, nn;
   any_num_make(tsc, &n, DATA(tsc, 0));
   if (!any_num_exact_sqrt(&n, &nn))
      return raise_error_msg(tsc, "imaginary"); /* TODO NEW-STYLE-ERROR */
   return do_pop_continue_2_values(tsc, nargs, any_num_get(&n), any_num_get(&nn));
}

/* implement 'number->string' */
static eval_status_t proc_num2str(tr7_engine_t tsc, int nargs)
{
   char buf[140]; /* enough for 128 bits */
   unsigned len;
   tr7_int_t pf = 10;
   tr7_t x = DATA(tsc, 0);
   if (nargs == 2) {
      pf = tr7_to_int(DATA(tsc, 1));
      if (pf != 16 && pf != 10 && pf != 8 && pf != 2)
         return raise_error_msg_obj(tsc, "bad radix", DATA(tsc, 1)); /* TODO NEW-STYLE-ERROR */
   }
   len = format_number(tsc, buf, sizeof buf, (unsigned)pf, x);
   return do_pop_continue_single(tsc, nargs, tr7_make_string_copy_length(tsc, buf, len));
}

/* implement 'string->number' */
static eval_status_t proc_str2num(tr7_engine_t tsc, int nargs)
{
   tr7_t r;
   long long iv;
   tr7_int_t pf = 0;
   char *ep, *s = (char*)TR7_CONTENT_STRING(DATA(tsc, 0));
   size_t length = TR7_SIZE_STRING(DATA(tsc, 0));
   if (length == 0)
      return do_pop_continue_single(tsc, nargs, TR7_FALSE);
   if (nargs == 2) {
      pf = tr7_to_int(DATA(tsc, 1));
      if (pf != 16 && pf != 10 && pf != 8 && pf != 2)
         return raise_error_msg_obj(tsc, "bad radix", DATA(tsc, 1)); /* TODO NEW-STYLE-ERROR */
   }
   if (*s == '#' && length > 1)   /* no use of base! assume zero terminated string */
      mk_sharp_const(tsc, &r, s + 1, length - 1);
   else if (pf == 0 || pf == 10)
      r = mk_atom(tsc, s, length);
   else {
      iv = strtoll(s, &ep, (int) pf);
      r = *ep ? TR7_FALSE : TR7_FROM_INT(iv);
   }
   return do_pop_continue_single(tsc, nargs, tr7_is_number(r) ? r : TR7_FALSE);
}

#if USE_MATH

/* implement 'floor' */
static eval_status_t proc_floor(tr7_engine_t tsc, int nargs)
{
   tr7_t x = DATA(tsc, 0);
   return do_pop_continue_double(tsc, 1, floor(tr7_to_double(x)));
}

/* implement 'ceiling' */
static eval_status_t proc_ceiling(tr7_engine_t tsc, int nargs)
{
   tr7_t x = DATA(tsc, 0);
   return do_pop_continue_double(tsc, 1, ceil(tr7_to_double(x)));
}

/* implement 'truncate' */
static eval_status_t proc_truncate(tr7_engine_t tsc, int nargs)
{
   tr7_t x = DATA(tsc, 0);
   double r = tr7_to_double(x);
   r = r > 0 ? floor(r) : ceil(r);
   return do_pop_continue_double(tsc, 1, r);
}

/* implement 'round' */
static eval_status_t proc_round(tr7_engine_t tsc, int nargs)
{
   tr7_t x = DATA(tsc, 0);
   if (TR7_IS_INT(x))
      return do_pop_continue_single(tsc, 1, x);
   return do_pop_continue_double(tsc, 1, rint(tr7_to_double(x)));
}

/* implement 'expt' */
static eval_status_t proc_expt(tr7_engine_t tsc, int nargs)
{
   double result = 0;
   int real_result = 1;
   tr7_t x = DATA(tsc, 0);
   tr7_t y = DATA(tsc, 1);
   if (TR7_IS_INT(x) && TR7_IS_INT(y))
      real_result = 0;
   /* This 'if' is an R5RS compatibility fix. */
   /* NOTE: Remove this 'if' fix for R6RS.    */
   if (tr7_to_double(x) != 0 || tr7_to_double(y) >= 0)
      result = pow(tr7_to_double(x), tr7_to_double(y));
   /* Before returning integer result make sure we can. */
   /* If the test fails, result is too big for integer. */
   if (!real_result) {
      tr7_int_t result_as_int = (tr7_int_t) result;
      if (result == (double) result_as_int)
         return do_pop_continue_integer(tsc, 2, result_as_int);
   }
   return do_pop_continue_double(tsc, 2, result);
}

/* implement 'exact' */
static eval_status_t proc_exact(tr7_engine_t tsc, int nargs)
{
   double dd;
   tr7_t x = DATA(tsc, 0);
   if (TR7_IS_DOUBLE(x)) {
      dd = *TR7_TO_DOUBLE(x);
      if (modf(dd, &dd) != 0.0)
         return raise_error_msg_obj(tsc, "not integral:", x); /* TODO NEW-STYLE-ERROR */
      if (dd > (double)TR7_INT_MAX_VAL || dd < (double)TR7_INT_MIN_VAL)
         return raise_error_msg_obj(tsc, "out of range:", x); /* TODO NEW-STYLE-ERROR */
      x = TR7_FROM_INT((tr7_int_t)dd);
   }
   return do_pop_continue_single(tsc, 1, x);
}

/* implement 'inexact' */
static eval_status_t proc_inexact(tr7_engine_t tsc, int nargs)
{
   tr7_t x = DATA(tsc, 0);
   if (!TR7_IS_INT(x))
      return do_pop_continue_single(tsc, 1, x);
   return do_pop_continue_double(tsc, 1, (double)TR7_TO_INT(x));
}
#endif

/*************************************************************************
* SECTION SCHEME_INEXACT
* ----------------------
*/
#if USE_SCHEME_INEXACT
/* implement 'finite?' */
static eval_status_t proc_is_finite(tr7_engine_t tsc, int nargs)
{
   return do_pop_continue_boolean(tsc, 1, tr7_is_finite(DATA(tsc, 0)));
}

/* implement 'infinite?' */
static eval_status_t proc_is_infinite(tr7_engine_t tsc, int nargs)
{
   return do_pop_continue_boolean(tsc, 1, tr7_is_infinite(DATA(tsc, 0)));
}

/* implement 'nan?' */
static eval_status_t proc_is_nan(tr7_engine_t tsc, int nargs)
{
   return do_pop_continue_boolean(tsc, 1, tr7_is_NaN(DATA(tsc, 0)));
}

#if USE_MATH
/* implement 'exp' */
static eval_status_t proc_exp(tr7_engine_t tsc, int nargs)
{
   tr7_t x = DATA(tsc, 0);
   return do_pop_continue_double(tsc, 1, exp(tr7_to_double(x)));
}

/* implement 'log' */
static eval_status_t proc_log(tr7_engine_t tsc, int nargs)
{
   double dd =  log(tr7_to_double(DATA(tsc, 0)));
   if (nargs == 2)
      dd /=  log(tr7_to_double(DATA(tsc, 1)));
   return do_pop_continue_double(tsc, nargs, dd);
}

/* implement 'sin' */
static eval_status_t proc_sin(tr7_engine_t tsc, int nargs)
{
   tr7_t x = DATA(tsc, 0);
   return do_pop_continue_double(tsc, 1, sin(tr7_to_double(x)));
}

/* implement 'cos' */
static eval_status_t proc_cos(tr7_engine_t tsc, int nargs)
{
   tr7_t x = DATA(tsc, 0);
   return do_pop_continue_double(tsc, 1, cos(tr7_to_double(x)));
}

/* implement 'tan' */
static eval_status_t proc_tan(tr7_engine_t tsc, int nargs)
{
   tr7_t x = DATA(tsc, 0);
   return do_pop_continue_double(tsc, 1, tan(tr7_to_double(x)));
}

/* implement 'asin' */
static eval_status_t proc_asin(tr7_engine_t tsc, int nargs)
{
   tr7_t x = DATA(tsc, 0);
   return do_pop_continue_double(tsc, 1, asin(tr7_to_double(x)));
}

/* implement 'acos' */
static eval_status_t proc_acos(tr7_engine_t tsc, int nargs)
{
   tr7_t x = DATA(tsc, 0);
   return do_pop_continue_double(tsc, 1, acos(tr7_to_double(x)));
}

/* implement 'atan' */
static eval_status_t proc_atan(tr7_engine_t tsc, int nargs)
{
   double dd;
   if (nargs == 2)
      dd = atan2(tr7_to_double(DATA(tsc, 0)), tr7_to_double(DATA(tsc, 1)));
   else
      dd = atan(tr7_to_double(DATA(tsc, 0)));
   return do_pop_continue_double(tsc, nargs, dd);
}

/* implement 'sqrt' */
static eval_status_t proc_sqrt(tr7_engine_t tsc, int nargs)
{
   tr7_t x = DATA(tsc, 0);
   return do_pop_continue_double(tsc, 1, sqrt(tr7_to_double(x)));
}
#endif
#endif

/*************************************************************************
* SECTION PROC_BOOLEAN
* --------------------
*
* implement 'boolean?'
*/
static eval_status_t proc_is_boolean(tr7_engine_t tsc, int nargs)
{
   return do_pop_continue_boolean(tsc, 1, TR7_IS_BOOLEAN(DATA(tsc, 0)));
}
/*
* implement 'not'
*/
static eval_status_t proc_not(tr7_engine_t tsc, int nargs)
{
   return do_pop_continue_boolean(tsc, 1, TR7_IS_FALSE(DATA(tsc, 0)));
}
/*
* implement 'boolean=?'
*/
static eval_status_t proc_boolean_eq(tr7_engine_t tsc, int nargs)
{
   int idx = 0;
   tr7_t x = DATA(tsc, idx);
   int res = TR7_IS_BOOLEAN(x);
   while (res && ++idx < nargs)
      res = TR7EQ(x,DATA(tsc, idx));
   return do_pop_continue_boolean(tsc, nargs, res);
}
/*************************************************************************
* SECTION PROC_LIST
* --------------------
*
* implement 'pair?'
*/
static eval_status_t proc_is_pair(tr7_engine_t tsc, int nargs)
{
   return do_pop_continue_boolean(tsc, 1, TR7_IS_PAIR(DATA(tsc, 0)));
}

/* implement 'cons' */
static eval_status_t proc_cons(tr7_engine_t tsc, int nargs)
{
   return do_pop_continue_single(tsc, 2, TR7_CONS2(tsc, DATA(tsc, 0), DATA(tsc, 1)));
}

/* implement 'car' */
static eval_status_t proc_car(tr7_engine_t tsc, int nargs)
{
   return do_pop_continue_single(tsc, 1, TR7_CAR(DATA(tsc, 0)));
}

/* implement 'cdr' */
static eval_status_t proc_cdr(tr7_engine_t tsc, int nargs)
{
   return do_pop_continue_single(tsc, 1, TR7_CDR(DATA(tsc, 0)));
}

/* implement 'set-car!' */
static eval_status_t proc_set_car(tr7_engine_t tsc, int nargs)
{
   tr7_t x = DATA(tsc, 0);
   if (tr7_is_immutable(x))
      return raise_immutable_error(tsc); /* TODO NEW-STYLE-ERROR */
   TR7_CAR(x) = DATA(tsc, 1);
   return do_pop_continue_single(tsc, 2, x);
}

/* implement 'set-cdr!' */
static eval_status_t proc_set_cdr(tr7_engine_t tsc, int nargs)
{
   tr7_t x = DATA(tsc, 0);
   if (tr7_is_immutable(x))
      return raise_immutable_error(tsc); /* TODO NEW-STYLE-ERROR */
   TR7_CDR(x) = DATA(tsc, 1);
   return do_pop_continue_single(tsc, 2, x);
}

static eval_status_t do_cxr(tr7_engine_t tsc, tr7_t (*cxr)(tr7_t))
{
   tr7_t x = cxr(DATA(tsc, 0));
   if (TR7_IS_VOID(x))
      return raise_invalid_argument_error(tsc); /* TODO NEW-STYLE-ERROR */
   return do_pop_continue_single(tsc, 1, x);
}

/* implement 'caar' */
static eval_status_t proc_caar(tr7_engine_t tsc, int nargs)
{
   return do_cxr(tsc, tr7_caar_or_void);
}

/* implement 'cadr' */
static eval_status_t proc_cadr(tr7_engine_t tsc, int nargs)
{
   return do_cxr(tsc, tr7_cadr_or_void);
}

/* implement 'cdar' */
static eval_status_t proc_cdar(tr7_engine_t tsc, int nargs)
{
   return do_cxr(tsc, tr7_cdar_or_void);
}

/* implement 'cddr' */
static eval_status_t proc_cddr(tr7_engine_t tsc, int nargs)
{
   return do_cxr(tsc, tr7_cddr_or_void);
}

/* implement 'null?' */
static eval_status_t proc_is_null(tr7_engine_t tsc, int nargs)
{
   return do_pop_continue_boolean(tsc, 1, TR7_IS_NIL(DATA(tsc, 0)));
}

/* implement 'list?' */
static eval_status_t proc_is_list(tr7_engine_t tsc, int nargs)
{
   return do_pop_continue_boolean(tsc, 1, tr7_list_length(DATA(tsc, 0)) >= 0);
}

/* implement 'make-list' */
static eval_status_t proc_make_list(tr7_engine_t tsc, int nargs)
{
   tr7_t head = TR7_NIL, last, x = nargs > 1 ?  DATA(tsc, 1) : TR7_NIL;
   tr7_int_t n = TR7_TO_INT(DATA(tsc, 0));
   if (n > 0) {
      head = last = tr7_cons(tsc, x, TR7_NIL);
      while (--n)
         last = TR7_CDR(last) = tr7_cons(tsc, x, TR7_NIL);
   }
   return do_pop_continue_single(tsc, nargs, head);
}

/* implement 'list' */
static eval_status_t proc_list(tr7_engine_t tsc, int nargs)
{
   tr7_t r = TR7_LIST_N(tsc, nargs, &DATA(tsc, 0));
   return do_pop_continue_single(tsc, nargs, r);
}

/* implement 'length' */
static eval_status_t proc_length(tr7_engine_t tsc, int nargs)
{
   tr7_int_t n = (tr7_int_t)tr7_list_length(DATA(tsc, 0));
   if (n < 0)
      return raise_invalid_argument_error(tsc); /* TODO NEW-STYLE-ERROR */
   return do_pop_continue_integer(tsc, 1, n);
}

/* implement 'append' */
static eval_status_t proc_append(tr7_engine_t tsc, int nargs)
{
   tr7_t r = tr7_append(tsc, nargs, &DATA(tsc, 0));
   if (TR7_IS_FALSE(r))
      return raise_invalid_argument_error(tsc);
   return do_pop_continue_single(tsc, nargs, r);
}

/* implement 'reverse' */
static eval_status_t proc_reverse(tr7_engine_t tsc, int nargs)
{
   tr7_t r = tr7_reverse(tsc, DATA(tsc, 0), TR7_NIL);
   return do_pop_continue_single(tsc, 1, r);
}

/* implement 'list-tail' */
static eval_status_t proc_list_tail(tr7_engine_t tsc, int nargs)
{
   tr7_t v = DATA(tsc, 0);
   tr7_int_t n = TR7_TO_INT(DATA(tsc, 1));
   for ( ; n > 0 ; n--, v = TR7_CDR(v))
      if (!TR7_IS_PAIR(v))
         return raise_invalid_argument_error(tsc); /* TODO NEW-STYLE-ERROR */
   return do_pop_continue_single(tsc, 2, v);
}

/* implement 'list-ref' */
static eval_status_t proc_list_ref(tr7_engine_t tsc, int nargs)
{
   tr7_t v = DATA(tsc, 0);
   tr7_int_t n = TR7_TO_INT(DATA(tsc, 1));
   for ( ; n > 0 && TR7_IS_PAIR(v) ; n--)
      v = TR7_CDR(v);
   if (!TR7_IS_PAIR(v))
      return raise_invalid_argument_error(tsc); /* TODO NEW-STYLE-ERROR */
   return do_pop_continue_single(tsc, 2, TR7_CAR(v));
}

/* implement 'list-set!' */
static eval_status_t proc_list_set(tr7_engine_t tsc, int nargs)
{
   tr7_t v = DATA(tsc, 0);
   tr7_int_t n = TR7_TO_INT(DATA(tsc, 1));
   for ( ; n > 0 && TR7_IS_PAIR(v) ; n--)
      v = TR7_CDR(v);
   if (!TR7_IS_PAIR(v))
      return raise_invalid_argument_error(tsc); /* TODO NEW-STYLE-ERROR */
   TR7_CAR(v) = DATA(tsc, 2);
   return do_pop_continue_void(tsc, 3);
}

/* implement 'list-copy' */
static eval_status_t proc_list_copy(tr7_engine_t tsc, int nargs)
{
   tr7_t v, x = DATA(tsc, 0);
   if (!TR7_IS_PAIR(x))
      v = x;
   else {
      if (!list_copy(tsc, x, &v, &x))
         return raise_invalid_argument_error(tsc); /* TODO NEW-STYLE-ERROR */
   }
   return do_pop_continue_single(tsc, 1, v);
}

#if USE_TR7_EXTRA
/* implement 'car+cdr' */
static eval_status_t proc_car_cdr(tr7_engine_t tsc, int nargs)
{
   tr7_t x = DATA(tsc, 0);
   return do_pop_continue_2_values(tsc, 1, TR7_CAR(x), TR7_CDR(x));
}

/* implement 'length*' */
static eval_status_t proc_length_star(tr7_engine_t tsc, int nargs)
{
   tr7_int_t n = (tr7_int_t)tr7_list_length(DATA(tsc, 0));
   return do_pop_continue_integer(tsc, 1, n);
}

/* implement 'append-reverse' */
static eval_status_t proc_append_reverse(tr7_engine_t tsc, int nargs)
{
   tr7_t r = tr7_reverse(tsc, DATA(tsc, 0), DATA(tsc, 1));
   return do_pop_continue_single(tsc, 2, r);
}

/* implement 'append-reverse!' */
static eval_status_t proc_append_reverse_in_place(tr7_engine_t tsc, int nargs)
{
   tr7_t r = tr7_reverse_in_place(DATA(tsc, 0), DATA(tsc, 1));
   return do_pop_continue_single(tsc, 2, r);
}

/* implement 'cons*' */
static eval_status_t proc_cons_star(tr7_engine_t tsc, int nargs)
{
   tr7_t r = tr7_cons_n(tsc, nargs - 1, &DATA(tsc, 0), DATA(tsc, nargs - 1));
   return do_pop_continue_single(tsc, nargs, r);
}

/* implement 'list-copy*' */
static eval_status_t proc_list_copy_star(tr7_engine_t tsc, int nargs)
{
   if (nargs > 1) {
      tr7_t r, x = DATA(tsc, 0);
      tr7_int_t cnt, start = TR7_TO_INT(DATA(tsc, 1));
      for ( cnt = start ; ; x = TR7_CDR(x), cnt--) {
         if (cnt == 0) {
            if (nargs == 2) {
               if (!list_copy(tsc, x, &r, &x))
                  break;
            }
            else {
               tr7_t *p = &r;
               cnt = TR7_TO_INT(DATA(tsc, 2)) - start;
               if (cnt < 0)
                  break;
               for (r = TR7_NIL ; cnt > 0 && TR7_IS_PAIR(x) ; x = TR7_CDR(x), cnt--) {
                  tr7_pair_t copy = GET_CELLS(tsc, copy, 0);
                  if (copy == NULL)
                     break;
                  TR7_PAIR_CAR(copy) = TR7_CAR(x);
                  *p = TR7_FROM_PAIR(copy);
                  if (p == &r)
                     push_recent_alloc(tsc, r);
                  p = &TR7_PAIR_CDR(copy);
               }
               if (cnt) {
                  if (nargs < 4)
                     break;
                  do {
                     tr7_pair_t copy = GET_CELLS(tsc, copy, 0);
                     if (copy == NULL)
                        break;
                     TR7_PAIR_CAR(copy) = DATA(tsc, 3);
                     *p = TR7_FROM_PAIR(copy);
                     if (p == &r)
                        push_recent_alloc(tsc, r);
                     p = &TR7_PAIR_CDR(copy);
                  } while(--cnt);
                  if (cnt)
                     break;
               }
               *p = TR7_NIL;
            }
            return do_pop_continue_single(tsc, nargs, r);
         }
         if (!TR7_IS_PAIR(x))
            break;
      }
      return raise_invalid_argument_error(tsc); /* TODO NEW-STYLE-ERROR */
   }
   return proc_list_copy(tsc, nargs);
}

#endif
/*
* helper for implementing member or assoc family
*/
static eval_status_t do_memass(tr7_engine_t tsc, int nargs, tr7_t (*fun)(tr7_t, tr7_t))
{
   tr7_t obj = DATA(tsc, 0);
   tr7_t lst = DATA(tsc, 1);
   return do_pop_continue_single(tsc, nargs, fun(obj, lst));
}
/*
* implement 'memq'
*/
static eval_status_t proc_memq(tr7_engine_t tsc, int nargs)
{
   return do_memass(tsc, nargs, tr7_memq);
}
/*
* implement 'memv'
*/
static eval_status_t proc_memv(tr7_engine_t tsc, int nargs)
{
   return do_memass(tsc, nargs, tr7_memv);
}
/*
* helper to implement 'member'
* gets: idx slow obj list proc
*/
static eval_status_t member_test(tr7_engine_t tsc)
{
   /*
   * STACK: CPT SLOW OBJ LIST CMP
   *         0    1   2   3    4
   */
   tr7_t list, slow, obj, cmp;
   tr7_int_t idx;

   /* list must be a pair */
   list = DATA(tsc, 3);
   if (!TR7_IS_PAIR(list)) {
      OPER_POP(tsc, 1);
      return do_pop_status_single(tsc, 5, TR7_FALSE, Cycle_Return);
   }

   /* guarding from circular lists */
   idx = TR7_TO_INT(DATA(tsc, 0));
   if (idx & 1) {
      slow = DATA(tsc, 1);
      slow = TR7_CDR(slow);
      if (idx > 2 && TR7EQ(slow, list)) {
         OPER_POP(tsc, 1);
         return do_pop_status_single(tsc, 5, TR7_FALSE, Cycle_Return);
      }
      DATA(tsc, 1) = slow;
   }
   DATA(tsc, 0) = TR7_FROM_INT(++idx);

   /* call comparison for helper handler */
   obj = DATA(tsc, 2);
   cmp = DATA(tsc, 4);
   return s_exec_2(tsc, cmp, obj, TR7_CAR(list));
}
/*
* implement 'member'
*/
static eval_status_t proc_member(tr7_engine_t tsc, int nargs)
{
   /* predefined equality function */
   tr7_t cmp;
   if (nargs == 2)
      return do_memass(tsc, 2, tr7_meme);
   cmp = DATA(tsc, 2);
   if (TR7EQ(cmp, PROC(EQUAL)))
      return do_memass(tsc, nargs, tr7_meme);
   if (TR7EQ(cmp, PROC(EQV)))
      return do_memass(tsc, nargs, tr7_memv);
   if (TR7EQ(cmp, PROC(EQ)))
      return do_memass(tsc, nargs, tr7_memq);

   /* user defined equality function */
   data_push_safe_2(tsc, TR7_FROM_INT(0), DATA(tsc, 1));
   oper_push_safe_1(tsc, OPER(MEMBER_THEN));
   return member_test(tsc);
}
/*
* operator helping to implement 'member'
*/
static eval_status_t _oper_member_then(tr7_engine_t tsc)
{
   tr7_t res = tsc->values[0];
   tr7_t list = DATA(tsc, 3);
   /* if test to true, return the list */
   if (!TR7_IS_FALSE(res)) {
      OPER_POP(tsc, 1);
      return do_pop_status_single(tsc, 5, list, Cycle_Return);
   }
   /* continue with cdr of the list */
   DATA(tsc, 3) = TR7_CDR(list);
   return member_test(tsc);
}
/*
* implement 'assq'
*/
static eval_status_t proc_assq(tr7_engine_t tsc, int nargs)
{
   return do_memass(tsc, nargs, tr7_assq);
}
/*
* implement 'assv'
*/
static eval_status_t proc_assv(tr7_engine_t tsc, int nargs)
{
   return do_memass(tsc, nargs, tr7_assv);
}
/*
* helper to implement 'assoc'
* gets: idx slow obj list proc
*/
static eval_status_t assoc_test(tr7_engine_t tsc)
{
   /*
   * STACK: CPT SLOW OBJ LIST CMP
   *         0    1   2   3    4
   */
   tr7_t list, slow, obj, cmp, head;
   tr7_int_t idx;

   /* search in list first car being a pair */
   idx = TR7_TO_INT(DATA(tsc, 0));
   slow = DATA(tsc, 1);
   list = DATA(tsc, 3);
   for ( ; TR7_IS_PAIR(list) ; list = TR7_CDR(list)) {
      if (idx++ & 1) {
         slow = TR7_CDR(slow);
         if (idx > 2 && TR7EQ(slow, list))
            break;
      }
      head = TR7_CAR(list);
      if (TR7_IS_PAIR(head)) {
         DATA(tsc, 0) = TR7_FROM_INT(idx);
         DATA(tsc, 1) = slow;
         obj = DATA(tsc, 2);
         cmp = DATA(tsc, 4);
         return s_exec_2(tsc, cmp, obj, TR7_CAR(head));
      }
   }
   OPER_POP(tsc, 1);
   return do_pop_status_single(tsc, 5, TR7_FALSE, Cycle_Return);
}
/*
* implement 'assoc'
*/
static eval_status_t proc_assoc(tr7_engine_t tsc, int nargs)
{
   /* predefined equality function */
   tr7_t cmp;
   if (nargs == 2)
      return do_memass(tsc, 2, tr7_asse);
   cmp = DATA(tsc, 2);
   if (TR7EQ(cmp, PROC(EQUAL)))
      return do_memass(tsc, nargs, tr7_asse);
   if (TR7EQ(cmp, PROC(EQV)))
      return do_memass(tsc, nargs, tr7_assv);
   if (TR7EQ(cmp, PROC(EQ)))
      return do_memass(tsc, nargs, tr7_assq);

   /* user defined equality function */
   data_push_safe_2(tsc, TR7_FROM_INT(0), DATA(tsc, 1));
   oper_push_safe_1(tsc, OPER(ASSOC_THEN));
   return assoc_test(tsc);
}
/*
* operator helping to implement 'assoc'
*/
static eval_status_t _oper_assoc_then(tr7_engine_t tsc)
{
   tr7_t res = tsc->values[0];
   tr7_t list = DATA(tsc, 3);
   /* if test to true, return the car of list */
   if (!TR7_IS_FALSE(res)) {
      OPER_POP(tsc, 1);
      return do_pop_status_single(tsc, 5, TR7_CAR(list), Cycle_Return);
   }
   /* continue with cdr of the list */
   DATA(tsc, 3) = TR7_CDR(list);
   return assoc_test(tsc);
}

/*************************************************************************
* SECTION SCHEME_CXR
* ------------------
*/
#if USE_SCHEME_CXR

static eval_status_t proc_caaar(tr7_engine_t tsc, int nargs)
{
   return do_cxr(tsc, tr7_caaar_or_void);
}

static eval_status_t proc_caadr(tr7_engine_t tsc, int nargs)
{
   return do_cxr(tsc, tr7_caadr_or_void);
}

static eval_status_t proc_cadar(tr7_engine_t tsc, int nargs)
{
   return do_cxr(tsc, tr7_cadar_or_void);
}

static eval_status_t proc_caddr(tr7_engine_t tsc, int nargs)
{
   return do_cxr(tsc, tr7_caddr_or_void);
}

static eval_status_t proc_cdaar(tr7_engine_t tsc, int nargs)
{
   return do_cxr(tsc, tr7_cdaar_or_void);
}

static eval_status_t proc_cdadr(tr7_engine_t tsc, int nargs)
{
   return do_cxr(tsc, tr7_cdadr_or_void);
}

static eval_status_t proc_cddar(tr7_engine_t tsc, int nargs)
{
   return do_cxr(tsc, tr7_cddar_or_void);
}

static eval_status_t proc_cdddr(tr7_engine_t tsc, int nargs)
{
   return do_cxr(tsc, tr7_cdddr_or_void);
}

static eval_status_t proc_caaaar(tr7_engine_t tsc, int nargs)
{
   return do_cxr(tsc, tr7_caaaar_or_void);
}

static eval_status_t proc_caaadr(tr7_engine_t tsc, int nargs)
{
   return do_cxr(tsc, tr7_caaadr_or_void);
}

static eval_status_t proc_caadar(tr7_engine_t tsc, int nargs)
{
   return do_cxr(tsc, tr7_caadar_or_void);
}

static eval_status_t proc_caaddr(tr7_engine_t tsc, int nargs)
{
   return do_cxr(tsc, tr7_caaddr_or_void);
}

static eval_status_t proc_cadaar(tr7_engine_t tsc, int nargs)
{
   return do_cxr(tsc, tr7_cadaar_or_void);
}

static eval_status_t proc_cadadr(tr7_engine_t tsc, int nargs)
{
   return do_cxr(tsc, tr7_cadadr_or_void);
}

static eval_status_t proc_caddar(tr7_engine_t tsc, int nargs)
{
   return do_cxr(tsc, tr7_caddar_or_void);
}

static eval_status_t proc_cadddr(tr7_engine_t tsc, int nargs)
{
   return do_cxr(tsc, tr7_cadddr_or_void);
}

static eval_status_t proc_cdaaar(tr7_engine_t tsc, int nargs)
{
   return do_cxr(tsc, tr7_cdaaar_or_void);
}

static eval_status_t proc_cdaadr(tr7_engine_t tsc, int nargs)
{
   return do_cxr(tsc, tr7_cdaadr_or_void);
}

static eval_status_t proc_cdadar(tr7_engine_t tsc, int nargs)
{
   return do_cxr(tsc, tr7_cdadar_or_void);
}

static eval_status_t proc_cdaddr(tr7_engine_t tsc, int nargs)
{
   return do_cxr(tsc, tr7_cdaddr_or_void);
}

static eval_status_t proc_cddaar(tr7_engine_t tsc, int nargs)
{
   return do_cxr(tsc, tr7_cddaar_or_void);
}

static eval_status_t proc_cddadr(tr7_engine_t tsc, int nargs)
{
   return do_cxr(tsc, tr7_cddadr_or_void);
}

static eval_status_t proc_cdddar(tr7_engine_t tsc, int nargs)
{
   return do_cxr(tsc, tr7_cdddar_or_void);
}

static eval_status_t proc_cddddr(tr7_engine_t tsc, int nargs)
{
   return do_cxr(tsc, tr7_cddddr_or_void);
}
#endif
/* ========== symbol ========== */

/* implement 'symbol?' */
static eval_status_t proc_is_symbol(tr7_engine_t tsc, int nargs)
{
   return do_pop_continue_boolean(tsc, 1, TR7_IS_SYMBOL(DATA(tsc, 0)));
}

/* implement 'symbol=?' */
static eval_status_t proc_symbol_eq(tr7_engine_t tsc, int nargs)
{
   int idx = 0;
   tr7_t x = DATA(tsc, idx);
   int res = TR7_IS_SYMBOL(x);
   while (res && ++idx < nargs)
      res = TR7EQ(x, DATA(tsc, idx));
   return do_pop_continue_boolean(tsc, nargs, res);
}

/* implement 'string->symbol' */
static eval_status_t proc_str2sym(tr7_engine_t tsc, int nargs)
{
   tr7_t s = DATA(tsc, 0);
   const char *v = (char *)TR7_CONTENT_STRING(s);
   size_t l = TR7_SIZE_STRING(s);
   return do_pop_continue_single(tsc, 1, tr7_get_symbol_length(tsc, v, l, 1));
}

/* implement 'symbol->string' */
static eval_status_t proc_sym2str(tr7_engine_t tsc, int nargs)
{
   tr7_t s = DATA(tsc, 0);
   const char *v = (char *)TR7_CONTENT_SYMBOL(s);
   tr7_t x = tr7_make_string_copy(tsc, v);
   tr7_set_immutable(x);
   return do_pop_continue_single(tsc, 1, x);
}

/* implement 'symbol-hash' */
static eval_status_t proc_symbol_hash(tr7_engine_t tsc, int nargs)
{
   tr7_t item = DATA(tsc, 0);
   tr7_uint_t h = hash_utf8str(TR7_CONTENT_SYMBOL(item), TR7_SIZE_SYMBOL(item), 0);
   return hash_bounding(tsc, nargs, h);
}

/*************************************************************************
* SECTION PROC_CHAR
* -----------------
*/

/* implement 'char?' */
static eval_status_t proc_is_char(tr7_engine_t tsc, int nargs)
{
   return do_pop_continue_boolean(tsc, 1, TR7_IS_CHAR(DATA(tsc, 0)));
}

static eval_status_t do_char_cmp_fun(tr7_engine_t tsc, int nargs, tr7_compare_t cmpmsk, tr7_compare_t (*cmpfun)(tr7_char_t,tr7_char_t))
{
   int idx = 0;
   tr7_char_t c, pc = TR7_TO_CHAR(DATA(tsc, idx));
   tr7_compare_t res = 1;
   while (res && ++idx < nargs) {
      c = TR7_TO_CHAR(DATA(tsc, idx));
      res = cmpfun(pc, c) & cmpmsk;
      pc = c;
   }
   return do_pop_continue_boolean(tsc, nargs, (int)res);
}

static eval_status_t do_char_cmp(tr7_engine_t tsc, int nargs, tr7_compare_t cmpmsk)
{
   return do_char_cmp_fun(tsc, nargs, cmpmsk, char_cmp);
}

/* implement 'char=?' */
static eval_status_t proc_char_eq(tr7_engine_t tsc, int nargs)
{
   return do_char_cmp(tsc, nargs, Tr7_Cmp_Equal);
}

/* implement 'char<?' */
static eval_status_t proc_char_lt(tr7_engine_t tsc, int nargs)
{
   return do_char_cmp(tsc, nargs, Tr7_Cmp_Lesser);
}

/* implement 'char>?' */
static eval_status_t proc_char_gt(tr7_engine_t tsc, int nargs)
{
   return do_char_cmp(tsc, nargs, Tr7_Cmp_Greater);
}

/* implement 'char<=?' */
static eval_status_t proc_char_le(tr7_engine_t tsc, int nargs)
{
   return do_char_cmp(tsc, nargs, Tr7_Cmp_Lesser_Or_Equal);
}

/* implement 'char>=?' */
static eval_status_t proc_char_ge(tr7_engine_t tsc, int nargs)
{
   return do_char_cmp(tsc, nargs, Tr7_Cmp_Greater_Or_Equal);
}

/* implement 'char->integer' */
static eval_status_t proc_char2int(tr7_engine_t tsc, int nargs)
{
   tr7_char_t c = TR7_TO_CHAR(DATA(tsc, 0));
   return do_pop_continue_integer(tsc, 1, c);
}

/* implement 'integer->char' */
static eval_status_t proc_int2char(tr7_engine_t tsc, int nargs)
{
   int c = tr7_to_int(DATA(tsc, 0));
   return do_pop_continue_single(tsc, 1, TR7_FROM_CHAR(c));
}

/*************************************************************************
* SECTION PROC_STRING
* -------------------
*/
/* implement 'string?' */
static eval_status_t proc_is_string(tr7_engine_t tsc, int nargs)
{
   return do_pop_continue_boolean(tsc, 1, TR7_IS_STRING(DATA(tsc, 0)));
}

/* implement 'make-string' */
static eval_status_t proc_make_string(tr7_engine_t tsc, int nargs)
{
   tr7_uint_t len = (tr7_uint_t)tr7_to_int(DATA(tsc, 0));
   tr7_char_t car = nargs == 2 ? TR7_TO_CHAR(DATA(tsc, 1)) : ' ';
   return do_pop_continue_single_alloc(tsc, nargs, tr7_make_string_fill(tsc, car, len));
}

/* implement 'string' */
static eval_status_t proc_string(tr7_engine_t tsc, int nargs)
{
   uint8_t *str;
   tr7_t res, c;
   unsigned len = 0;
   int idx;
   for (idx = 0 ; idx < nargs ; idx ++) {
      c = DATA(tsc, idx);
      if (!TR7_IS_CHAR(c))
         return raise_error_msg_obj(tsc, "expected char but got ", c);
      len += char_utf8_length(TR7_TO_CHAR(c));
   }
   res = make_string_noinit(tsc, len);
   if (!TR7_IS_NIL(res)) {
      str = TR7_CONTENT_STRING(res);
      for (idx = 0 ; idx < nargs ; idx++)
         str += char_to_utf8(TR7_TO_CHAR(DATA(tsc, idx)), str);
   }
   return do_pop_continue_single_alloc(tsc, nargs, res);
}

/* implement 'string-length' */
static eval_status_t proc_string_length(tr7_engine_t tsc, int nargs)
{
   return do_pop_continue_integer(tsc, 1, (tr7_int_t)tr7_string_length(DATA(tsc, 0)));
}

/* implement 'string-ref' */
static eval_status_t proc_string_ref(tr7_engine_t tsc, int nargs)
{
   tr7_t string = DATA(tsc, 0);
   size_t index = (tr7_uint_t)tr7_to_int(DATA(tsc, 1));
   tr7_char_t car = tr7_string_ref(string, index);
   if (car == TR7_CHAR_EOF)
      return raise_out_of_bound_error(tsc, DATA(tsc, 1));
   return do_pop_continue_single(tsc, 2, TR7_FROM_CHAR(car));
}

/* implement 'string-set!' */
static eval_status_t proc_string_set(tr7_engine_t tsc, int nargs)
{
   tr7_t string = DATA(tsc, 0);
   size_t index = (tr7_uint_t)tr7_to_int(DATA(tsc, 1));
   tr7_char_t car = TR7_TO_CHAR(DATA(tsc, 2));
   if (tr7_is_immutable(string))
      return raise_immutable_error(tsc);
   if (!tr7_string_set(tsc, string, index, car))
      return raise_out_of_bound_error(tsc, DATA(tsc, 1));
   return do_pop_continue_void(tsc, 3);
}

static eval_status_t do_string_cmp(tr7_engine_t tsc, int nargs, tr7_compare_t cmp, tr7_compare_t (*cmpfun)(tr7_char_t,tr7_char_t))
{
   int iarg = 0;
   tr7_compare_t c, res = 1;
   tr7_char_t car, car2;
   tr7_t x = DATA(tsc, iarg);
   const uint8_t *str2, *str = TR7_CONTENT_STRING(x);
   tr7_uint_t idx, idx2, len2, len = TR7_SIZE_STRING(x);
   while (res && ++iarg < nargs) {
      x = DATA(tsc, iarg);
      str2 = TR7_CONTENT_STRING(x);
      len2 = TR7_SIZE_STRING(x);
      for (idx = idx2 = 0; ; ) {
         if (idx >= len)
            c = idx2 >= len2 ? Tr7_Cmp_Equal : Tr7_Cmp_Lesser;
         else if (idx2 >= len2)
            c = Tr7_Cmp_Greater;
         else {
            idx += utf8_to_char(&str[idx], &car);
            idx2 += utf8_to_char(&str2[idx2], &car2);
            c = cmpfun(car, car2);
            if (c == Tr7_Cmp_Equal)
               continue;
         }
         break;
      }
      res = c & cmp;
      str = str2;
      len = len2;
   }
   return do_pop_continue_boolean(tsc, nargs, (int)res);
}

static eval_status_t do_string_compare(tr7_engine_t tsc, int nargs, tr7_compare_t cmp)
{
      return do_string_cmp(tsc, nargs, cmp, char_cmp);
}

/* implement 'string=?' */
static eval_status_t proc_string_eq(tr7_engine_t tsc, int nargs)
{
   return do_string_compare(tsc, nargs, Tr7_Cmp_Equal);
}

/* implement 'string<?' */
static eval_status_t proc_string_lt(tr7_engine_t tsc, int nargs)
{
   return do_string_compare(tsc, nargs, Tr7_Cmp_Lesser);
}

/* implement 'string>?' */
static eval_status_t proc_string_gt(tr7_engine_t tsc, int nargs)
{
   return do_string_compare(tsc, nargs, Tr7_Cmp_Greater);
}

/* implement 'string<=?' */
static eval_status_t proc_string_le(tr7_engine_t tsc, int nargs)
{
   return do_string_compare(tsc, nargs, Tr7_Cmp_Lesser_Or_Equal);
}

/* implement 'string>=?' */
static eval_status_t proc_string_ge(tr7_engine_t tsc, int nargs)
{
   return do_string_compare(tsc, nargs, Tr7_Cmp_Greater_Or_Equal);
}

/* implement 'string-append' */
static eval_status_t proc_string_append(tr7_engine_t tsc, int nargs)
{
   int iarg;
   uint8_t *str;
   tr7_t x, r;
   tr7_uint_t len;
   /* compute needed length for new string */
   for (len = 0, iarg = 0 ; iarg < nargs ; iarg++)
      len += TR7_SIZE_STRING(DATA(tsc, iarg));
   /* allocate the result */
   r = make_string_noinit(tsc, len);
   if (TR7_IS_STRING(r)) {
      str = TR7_CONTENT_STRING(r);
      for (iarg = 0 ; iarg < nargs ; iarg++) {
         x = DATA(tsc, iarg);
         len = TR7_SIZE_STRING(x);
         memcpy(str, TR7_CONTENT_STRING(x), len);
         str += len;
      }
      *str = 0;
   }
   return do_pop_continue_single_alloc(tsc, nargs, r);
}













/*
* gets zero, one or two indexes from args
* returns the count of indexes gotten
*/
static int get_start_end(tr7_t args[], int nargs, tr7_uint_t indexes[2])
{
   int r = nargs;
   if (nargs <= 0)
      r = 0;
   else {
      tr7_int_t val = tr7_to_int(args[0]);
      if (val < 0)
         r = -1;
      else {
         indexes[0] = (tr7_uint_t)val;
         if (nargs > 1) {
            val = tr7_to_int(args[1]);
            if (val < 0 || (indexes[1] = (tr7_uint_t)val) < indexes[0])
               r = -1;
         }
      }
   }
   return r;
}

struct substring_desc {
   uint8_t *string;
   tr7_uint_t length;
   tr7_uint_t indexes[2];
   tr7_uint_t offsets[2];
};

static int make_substring_desc(tr7_t args[], int nargs, struct substring_desc *subd, tr7_t string)
{
   int n;
   ssize_t ssz;
   tr7_uint_t len;

   /* get string */
   subd->string = TR7_CONTENT_STRING(string);
   subd->length = len = TR7_SIZE_STRING(string);

   /* get bounds */
   n = get_start_end(args, nargs, subd->indexes);
   if (n < 0)
      return 0;
   if (n == 0) {
      subd->indexes[0] = 0;
      subd->offsets[0] = 0;
   }
   else {
      ssz = utf8str_offset_end((uint8_t*)subd->string, len, subd->indexes[0]);
      if (ssz < 0)
         return 0;
      subd->offsets[0] = (tr7_uint_t)ssz;
   }
   if (n <= 1) {
      subd->indexes[1] = subd->indexes[0] + utf8str_nchars(&subd->string[subd->indexes[0]], len - subd->indexes[0]);
      subd->offsets[1] = len;
   }
   else {
      ssz = utf8str_offset_end((uint8_t*)&subd->string[subd->offsets[0]], len - subd->offsets[0], (tr7_uint_t)(subd->indexes[1] - subd->indexes[0]));
      if (ssz < 0)
         return 0;
      subd->offsets[1] = subd->offsets[0] + (tr7_uint_t)ssz;
   }
   return 1;
}

static int get_substring_desc(tr7_t args[], int nargs, struct substring_desc *subd)
{
   return make_substring_desc(&args[1], nargs - 1, subd, args[0]);
}

/* implement 'list->string' */
static eval_status_t proc_list_to_string(tr7_engine_t tsc, int nargs)
{
   uint8_t *str;
   tr7_t it, res;
   unsigned len;
   tr7_t lst = DATA(tsc, 0);
   for (len = 0, it = lst ; TR7_IS_PAIR(it) ; it = TR7_CDR(it)) {
      if (!TR7_IS_CHAR(TR7_CAR(it)))
         return raise_error_msg_obj(tsc, "expected char but got ", TR7_CAR(lst));
      len += char_utf8_length(TR7_TO_CHAR(TR7_CAR(it)));
   }
   res = make_string_noinit(tsc, len);
   if (!TR7_IS_NIL(res)) {
      str = TR7_CONTENT_STRING(res);
      for (it = lst ; TR7_IS_PAIR(it) ; it = TR7_CDR(it))
         str += char_to_utf8(TR7_TO_CHAR(TR7_CAR(it)), str);
      *str = 0;
   }
   return do_pop_continue_single_alloc(tsc, 1, res);
}

/* implement 'string->list' */
static eval_status_t proc_string_to_list(tr7_engine_t tsc, int nargs)
{
   tr7_char_t car;
   tr7_t *l, res, x;
   struct substring_desc subd;

   if (!get_substring_desc(&DATA(tsc, 0), nargs, &subd))
      return raise_out_of_bound_error(tsc, tr7_cons_n(tsc, nargs - 1, &DATA(tsc, 1), TR7_NIL));

   /* make list */
   res = TR7_NIL;
   l = &res;
   while(subd.offsets[0] < subd.offsets[1]) {
      subd.offsets[0] += utf8_to_char((uint8_t*)&subd.string[subd.offsets[0]], &car);
      *l = x = tr7_cons(tsc, TR7_FROM_CHAR(car), TR7_NIL);
      l = &TR7_CDR(x);
   }
   return do_pop_continue_single_alloc(tsc, nargs, res);
}

/* implement 'substring' and 'string-copy' */
static eval_status_t proc_string_copy(tr7_engine_t tsc, int nargs)
{
   tr7_t res;
   struct substring_desc subd;
   if (!get_substring_desc(&DATA(tsc, 0), nargs, &subd))
      return raise_out_of_bound_error(tsc, tr7_cons_n(tsc, nargs - 1, &DATA(tsc, 1), TR7_NIL));
   res = tr7_make_string_copy_length(tsc, (char*)&subd.string[subd.offsets[0]], subd.offsets[1] - subd.offsets[0]);
   return do_pop_continue_single_alloc(tsc, nargs, res);
}

/* implement 'string-copy!' */
static eval_status_t proc_string_copy_to(tr7_engine_t tsc, int nargs)
{
   tr7_t to;
   uint8_t *str, *cpy;
   ssize_t ssz;
   size_t at, len, start, stop, slen, dlen;
   struct substring_desc subd;

   /* get destination data */
   to = DATA(tsc, 0);
   str = TR7_CONTENT_STRING(to);
   len = TR7_SIZE_STRING(to);
   at = TR7_TO_UINT(DATA(tsc, 1));
   ssz = utf8str_offset_end(str, len, at);
   if (ssz < 0)
      goto bound_error;

   /* get source data */
   if (!get_substring_desc(&DATA(tsc, 2), nargs - 2, &subd))
      goto bound_error;

   /* replaced destination length */
   start = (size_t)ssz;
   ssz = utf8str_offset_end(&str[start], len - start, subd.indexes[1] - subd.indexes[0]);
   if (ssz < 0)
      goto bound_error;
   dlen = (size_t)ssz;
   stop = start + dlen;

   /* copy */
   slen = subd.offsets[1] - subd.offsets[0];
   if (dlen >= slen) {
      /* replaced length greater than replacement's one */
      memmove(&str[start], &subd.string[subd.offsets[0]], slen);
      if (dlen > slen) {
         memmove(&str[start + slen], &str[start + dlen], len - stop);
         len += slen - dlen;
         TR7_SET_SIZE_STRING(to, len);
         str[len] = 0;
      }
   }
   else {
      cpy = memalloc(tsc, len + slen - dlen + 1);
      if (!cpy)
         return raise_out_of_memory_error(tsc);
      TR7_SET_SIZE_STRING(to, len + slen - dlen);
      cpy[len + slen - dlen] = 0;
      TR7_CONTENT_STRING(to) = cpy;
      set_final_flag(tsc, TR7_TO_CELL(to));
      memcpy(cpy, str, start);
      memcpy(&cpy[start], &subd.string[subd.offsets[0]], slen);
      memcpy(&cpy[start + slen], &str[start + dlen], len - stop);
      memfree(tsc, str);
   }
   return do_pop_continue_single(tsc, nargs, to);

bound_error:
   return raise_out_of_bound_error(tsc,
               tr7_cons(tsc, DATA(tsc, 1),
                              tr7_cons_n(tsc, nargs - 3, &DATA(tsc, 3), TR7_NIL)));
}

/* implement 'string-fill!' */
static eval_status_t proc_string_fill(tr7_engine_t tsc, int nargs)
{
   uint8_t buf[UTF8BUFFSIZE];
   tr7_char_t car;
   tr7_t to;
   uint8_t *cpy;
   size_t szc, nlen, olen, nc;
   struct substring_desc subd;

   /* get parameters */
   to = DATA(tsc, 0);
   car = TR7_TO_CHAR(DATA(tsc, 1));
   if (!make_substring_desc(&DATA(tsc, 2), nargs - 2, &subd, to))
      return raise_out_of_bound_error(tsc, tr7_cons_n(tsc, nargs - 2, &DATA(tsc, 2), TR7_NIL));

   /* lengths */
   szc = char_to_utf8(car, buf);
   nc = subd.indexes[1] - subd.indexes[0];
   nlen = szc * nc;
   olen = subd.offsets[1] - subd.offsets[0];

   /* set */
   if (olen >= nlen) {
      /* replaced length greater than replacement's one */
      while(nc) {
         memcpy(&subd.string[subd.offsets[0]], buf, szc);
         subd.offsets[0] += szc;
         nc--;
      }
      if (olen > nlen) {
         memmove(&subd.string[subd.offsets[0]], &subd.string[subd.offsets[1]], subd.length - subd.offsets[1]);
         subd.length += nlen - olen;
         TR7_SET_SIZE_STRING(to, subd.length);
         subd.string[subd.length] = 0;
      }
   }
   else {
      cpy = memalloc(tsc, subd.length + nlen - olen + 1);
      if (!cpy)
         return raise_out_of_memory_error(tsc);
      TR7_SET_SIZE_STRING(to, subd.length + nlen - olen);
      cpy[subd.length + nlen - olen] = 0;
      TR7_CONTENT_STRING(to) = cpy;
      set_final_flag(tsc, TR7_TO_CELL(to));
      memcpy(cpy, subd.string, subd.offsets[0]);
      while(nc) {
         memcpy(&cpy[subd.offsets[0]], buf, szc);
         subd.offsets[0] += szc;
         nc--;
      }
      memcpy(&cpy[subd.offsets[0]], &subd.string[subd.offsets[1]], subd.length - subd.offsets[1]);
      memfree(tsc, subd.string);
   }
   return do_pop_continue_single(tsc, nargs, to);
}

/* implement 'string-hash' */
static eval_status_t proc_string_hash(tr7_engine_t tsc, int nargs)
{
   tr7_t item = DATA(tsc, 0);
   tr7_uint_t h = hash_utf8str(TR7_CONTENT_STRING(item), TR7_SIZE_STRING(item), 0);
   return hash_bounding(tsc, nargs, h);
}

/*************************************************************************
* SECTION PROC_SCHEME_CHAR
* ------------------------
*/
#if USE_SCHEME_CHAR
static eval_status_t do_char_cmp_ci(tr7_engine_t tsc, int nargs, tr7_compare_t cmpmsk)
{
   return do_char_cmp_fun(tsc, nargs, cmpmsk, char_cmp_ci);
}

/* implement 'char-ci=?' */
static eval_status_t proc_char_eq_ci(tr7_engine_t tsc, int nargs)
{
   return do_char_cmp_ci(tsc, nargs, Tr7_Cmp_Equal);
}

/* implement 'char-ci<?' */
static eval_status_t proc_char_lt_ci(tr7_engine_t tsc, int nargs)
{
   return do_char_cmp_ci(tsc, nargs, Tr7_Cmp_Lesser);
}

/* implement 'char-ci>?' */
static eval_status_t proc_char_gt_ci(tr7_engine_t tsc, int nargs)
{
   return do_char_cmp_ci(tsc, nargs, Tr7_Cmp_Greater);
}

/* implement 'char-ci<=?' */
static eval_status_t proc_char_le_ci(tr7_engine_t tsc, int nargs)
{
   return do_char_cmp_ci(tsc, nargs, Tr7_Cmp_Lesser_Or_Equal);
}

/* implement 'char-ci>=?' */
static eval_status_t proc_char_ge_ci(tr7_engine_t tsc, int nargs)
{
   return do_char_cmp_ci(tsc, nargs, Tr7_Cmp_Greater_Or_Equal);
}

/* implement 'char-alphabetic?' */
static eval_status_t proc_char_is_alpha(tr7_engine_t tsc, int nargs)
{
   return do_pop_continue_boolean(tsc, 1, iswalpha((wint_t)TR7_TO_CHAR(DATA(tsc, 0))));
}

/* implement 'char-numeric?' */
static eval_status_t proc_char_is_num(tr7_engine_t tsc, int nargs)
{
   return do_pop_continue_boolean(tsc, 1, iswdigit((wint_t)TR7_TO_CHAR(DATA(tsc, 0))));
}

/* implement 'char-whitespace?' */
static eval_status_t proc_char_is_space(tr7_engine_t tsc, int nargs)
{
   return do_pop_continue_boolean(tsc, 1, iswspace((wint_t)TR7_TO_CHAR(DATA(tsc, 0))));
}

/* implement 'char-upper-case?' */
static eval_status_t proc_char_is_upper(tr7_engine_t tsc, int nargs)
{
   return do_pop_continue_boolean(tsc, 1, iswupper((wint_t)TR7_TO_CHAR(DATA(tsc, 0))));
}

/* implement 'char-lower-case?' */
static eval_status_t proc_char_is_lower(tr7_engine_t tsc, int nargs)
{
   return do_pop_continue_boolean(tsc, 1, iswlower((wint_t)TR7_TO_CHAR(DATA(tsc, 0))));
}

/* implement 'char-unicode?' */
static eval_status_t proc_char_is_unicode(tr7_engine_t tsc, int nargs)
{
   return do_pop_continue_boolean(tsc, 1, tr7_is_char_unicode(TR7_TO_CHAR(DATA(tsc, 0))));
}

/* implement 'char-upcase' */
static eval_status_t proc_char_upcase(tr7_engine_t tsc, int nargs)
{
   tr7_char_t c = TR7_TO_CHAR(DATA(tsc, 0));
   c = (tr7_char_t)towupper((wint_t)c);
   return do_pop_continue_single(tsc, 1, TR7_FROM_CHAR(c));
}

/* implement 'char-downcase' and 'char-foldcase' */
static eval_status_t proc_char_downcase(tr7_engine_t tsc, int nargs)
{
   tr7_char_t c = TR7_TO_CHAR(DATA(tsc, 0));
   c = towlower(c);
   return do_pop_continue_single(tsc, 1, TR7_FROM_CHAR(c));
}

/* implement 'digit-value' */
static eval_status_t proc_char_digit_value(tr7_engine_t tsc, int nargs)
{
   tr7_char_t c = TR7_TO_CHAR(DATA(tsc, 0));
   int value = (int)(c - '0');
   if (value < 0 || value > 9)
      return do_pop_continue_false(tsc, 1);
   return do_pop_continue_integer(tsc, 1, value);
}

static eval_status_t do_string_compare_ci(tr7_engine_t tsc, int nargs, tr7_compare_t cmp)
{
      return do_string_cmp(tsc, nargs, cmp, char_cmp_ci);
}

/* implement 'string-ci=?' */
static eval_status_t proc_string_eq_ci(tr7_engine_t tsc, int nargs)
{
   return do_string_compare_ci(tsc, nargs, Tr7_Cmp_Equal);
}

/* implement 'string-ci<?' */
static eval_status_t proc_string_lt_ci(tr7_engine_t tsc, int nargs)
{
   return do_string_compare_ci(tsc, nargs, Tr7_Cmp_Lesser);
}

/* implement 'string-ci>?' */
static eval_status_t proc_string_gt_ci(tr7_engine_t tsc, int nargs)
{
   return do_string_compare_ci(tsc, nargs, Tr7_Cmp_Greater);
}

/* implement 'string-ci<=?' */
static eval_status_t proc_string_le_ci(tr7_engine_t tsc, int nargs)
{
   return do_string_compare_ci(tsc, nargs, Tr7_Cmp_Lesser_Or_Equal);
}

/* implement 'string-ci>=?' */
static eval_status_t proc_string_ge_ci(tr7_engine_t tsc, int nargs)
{
   return do_string_compare_ci(tsc, nargs, Tr7_Cmp_Greater_Or_Equal);
}

static eval_status_t do_change_case(tr7_engine_t tsc, int nargs, wint_t (*cvtfun)(wint_t))
{
   uint8_t *rstr;
   tr7_char_t car;
   tr7_t r, x = DATA(tsc, 0);
   const uint8_t *str = TR7_CONTENT_STRING(x);
   size_t index, len2, len = TR7_SIZE_STRING(x);
   for (len2 = 0, index = 0 ; index < len ;) {
      index += utf8_to_char(&str[index], &car);
      len2 += char_utf8_length((tr7_char_t)cvtfun((wint_t)car));
   }
   r = make_string_noinit(tsc, len2);
   if (TR7_IS_STRING(r)) {
      rstr = TR7_CONTENT_STRING(r);
      for (index = 0 ; index < len ; ) {
         index += utf8_to_char(&str[index], &car);
         rstr += char_to_utf8((tr7_char_t)cvtfun((wint_t)car), rstr);
      }
   }
   return do_pop_continue_single(tsc, 1, r);
}

/* implement 'string-upcase' */
static eval_status_t proc_string_upcase(tr7_engine_t tsc, int nargs)
{
   return do_change_case(tsc, nargs, towupper);
}

/* implement 'string-downcase' and 'string-foldcase' */
static eval_status_t proc_string_downcase(tr7_engine_t tsc, int nargs)
{
   return do_change_case(tsc, nargs, towlower);
}

/* implement 'string-ci-hash' */
static eval_status_t proc_string_ci_hash(tr7_engine_t tsc, int nargs)
{
   tr7_t item = DATA(tsc, 0);
   tr7_uint_t h = hash_utf8str(TR7_CONTENT_STRING(item), TR7_SIZE_STRING(item), 1);
   return hash_bounding(tsc, nargs, h);
}

#endif
/*************************************************************************
* SECTION PROC_VECTOR
* -------------------
*
* implement 'vector?'
*/
static eval_status_t proc_is_vector(tr7_engine_t tsc, int nargs)
{
   return do_pop_continue_boolean(tsc, 1, TR7_IS_VECTOR(DATA(tsc, 0)));
}
/*
* implement 'vector'
*/
static eval_status_t proc_vector(tr7_engine_t tsc, int nargs)
{
   tr7_t res = tr7_make_vector_copy(tsc, (size_t)nargs, &DATA(tsc, 0));
   return do_pop_continue_single_alloc(tsc, nargs, res);
}
/*
* implement 'make-vector'
*/
static eval_status_t proc_make_vector(tr7_engine_t tsc, int nargs)
{
   tr7_uint_t len = (tr7_uint_t)tr7_to_int(DATA(tsc, 0));
   tr7_t item = nargs == 2 ? DATA(tsc, 1) : TR7_NIL;
   tr7_t res = tr7_make_vector_fill(tsc, len, item);
   return do_pop_continue_single_alloc(tsc, nargs, res);
}
/*
* implement 'vector-length'
*/
static eval_status_t proc_vector_length(tr7_engine_t tsc, int nargs)
{
   return do_pop_continue_integer(tsc, 1, (tr7_int_t)TR7_LENGTH_VECTOR(DATA(tsc, 0)));
}
/*
* implement 'vector-ref'
*/
static eval_status_t proc_vector_ref(tr7_engine_t tsc, int nargs)
{
   tr7_t vec = DATA(tsc, 0);
   tr7_uint_t idx = (tr7_uint_t)tr7_to_int(DATA(tsc, 1));
   if (idx >= TR7_LENGTH_VECTOR(vec))
      return raise_out_of_bound_error(tsc, DATA(tsc, 1));
   return do_pop_continue_single(tsc, 2, TR7_ITEM_VECTOR(vec, idx));
}

/* implement 'vector-set!' */
static eval_status_t proc_vector_set(tr7_engine_t tsc, int nargs)
{
   tr7_t vec = DATA(tsc, 0);
   tr7_uint_t idx = (tr7_uint_t)tr7_to_int(DATA(tsc, 1));
   tr7_t val = DATA(tsc, 2);
   if (tr7_is_immutable(vec))
      return raise_immutable_error(tsc);
   if (idx >= TR7_LENGTH_VECTOR(vec))
      return raise_out_of_bound_error(tsc, DATA(tsc, 1));
   TR7_ITEM_VECTOR(vec, idx) = val;
   return do_pop_continue_single(tsc, 3, vec);
}

struct subvector_desc {
   tr7_t *items;
   tr7_uint_t length;
   tr7_uint_t indexes[2];
};

static int make_subvector_desc(tr7_t args[], int nargs, struct subvector_desc *subd, tr7_t vector)
{
   int n;
   tr7_vector_t vec = TR7_TO_VECTOR(vector);

   subd->items = vec->items;
   subd->length = TR7_VECTOR_LENGTH(vec);

   /* get bounds */
   n = get_start_end(args, nargs, subd->indexes);
   if (n < 0)
      return 0;
   if (n == 0)
      subd->indexes[0] = 0;
   else if (subd->indexes[0] > subd->length)
      return 0;
   if (n <= 1)
      subd->indexes[1] = subd->length;
   else if (subd->indexes[1] < subd->indexes[0] || subd->indexes[1] > subd->length)
      return 0;
   return 1;
}

static int get_subvector_desc(tr7_t args[], int nargs, struct subvector_desc *subd)
{
   return make_subvector_desc(&args[1], nargs - 1, subd, args[0]);
}

/* implement 'vector->list' */
static eval_status_t proc_vector_to_list(tr7_engine_t tsc, int nargs)
{
   tr7_t res;
   struct subvector_desc subd;

   if (!get_subvector_desc(&DATA(tsc, 0), nargs, &subd))
      return raise_out_of_bound_error(tsc, tr7_cons_n(tsc, nargs - 1, &DATA(tsc, 1), TR7_NIL));

   res = tr7_cons_n(tsc, subd.indexes[1] - subd.indexes[0], &subd.items[subd.indexes[0]], TR7_NIL);
   return do_pop_continue_single_alloc(tsc, nargs, res);
}
/*
* implement 'list->vector'
*/
static eval_status_t proc_list_to_vector(tr7_engine_t tsc, int nargs)
{
   tr7_t res = tr7_list_to_vector(tsc, DATA(tsc, 0));
   return do_pop_continue_single_alloc(tsc, 1, res);
}

/* implement 'vector->string' */
static eval_status_t proc_vector_to_string(tr7_engine_t tsc, int nargs)
{
   struct subvector_desc subd;
   tr7_t res, car;
   tr7_uint_t idx, len;
   uint8_t *str;

   if (!get_subvector_desc(&DATA(tsc, 0), nargs, &subd))
      return raise_out_of_bound_error(tsc, tr7_cons_n(tsc, nargs - 1, &DATA(tsc, 1), TR7_NIL));

   for (len = 0, idx = subd.indexes[0] ; idx < subd.indexes[1] ; idx++) {
      car = subd.items[idx];
      if (!TR7_IS_CHAR(car))
         return raise_error_msg_obj(tsc, "expected char but got ", car);
      len += char_utf8_length(TR7_TO_CHAR(car));
   }

   res = make_string_noinit(tsc, len);
   if (TR7_IS_NIL(res))
      return raise_out_of_memory_error(tsc);

   str = TR7_CONTENT_STRING(res);
   for (len = 0, idx = subd.indexes[0] ; idx < subd.indexes[1] ; idx++) {
      car = subd.items[idx];
      len += char_to_utf8(TR7_TO_CHAR(car), &str[len]);
   }
   return do_pop_continue_single(tsc, nargs, res);
}

/* implement 'string->vector' */
static eval_status_t proc_string_to_vector(tr7_engine_t tsc, int nargs)
{
   tr7_char_t car;
   struct substring_desc subd;
   tr7_t res, *items;

   if (!get_substring_desc(&DATA(tsc, 0), nargs, &subd))
      return raise_out_of_bound_error(tsc, tr7_cons_n(tsc, nargs - 1, &DATA(tsc, 1), TR7_NIL));

   /* allocate */
   res = alloc_vector(tsc, subd.indexes[1] - subd.indexes[0]);
   if (TR7_IS_NIL(res))
      return raise_out_of_memory_error(tsc);

   items = TR7_ITEMS_VECTOR(res);
   while(subd.offsets[0] < subd.offsets[1]) {
      subd.offsets[0] += utf8_to_char((uint8_t*)&subd.string[subd.offsets[0]], &car);
      *items++ = TR7_FROM_CHAR(car);
   }
   return do_pop_continue_single(tsc, nargs, res);
}

/* implement 'vector-copy' */
static eval_status_t proc_vector_copy(tr7_engine_t tsc, int nargs)
{
   struct subvector_desc subd;
   tr7_t res;

   if (!get_subvector_desc(&DATA(tsc, 0), nargs, &subd))
      return raise_out_of_bound_error(tsc, tr7_cons_n(tsc, nargs - 1, &DATA(tsc, 1), TR7_NIL));

   res = tr7_make_vector_copy(tsc, (subd.indexes[1] - subd.indexes[0]), &subd.items[subd.indexes[0]]);
   return do_pop_continue_single_alloc(tsc, nargs, res);
}

/* implement 'vector-copy!' */
static eval_status_t proc_vector_copy_to(tr7_engine_t tsc, int nargs)
{
   struct subvector_desc subd;

   /* get destination data */
   tr7_t to = DATA(tsc, 0);
   tr7_vector_t vecto = TR7_TO_VECTOR(to);
   tr7_int_t at = tr7_to_int(DATA(tsc, 1));

   /* get source data */
   if (!get_subvector_desc(&DATA(tsc, 2), nargs - 2, &subd)
     || at < 0
     || (tr7_uint_t)at + subd.indexes[1] - subd.indexes[0] > TR7_VECTOR_LENGTH(vecto))
      return raise_out_of_bound_error(tsc,
                  tr7_cons(tsc, DATA(tsc, 1),
                              tr7_cons_n(tsc, nargs - 3, &DATA(tsc, 3), TR7_NIL)));

   memmove(&vecto->items[at], &subd.items[subd.indexes[0]], (subd.indexes[1] - subd.indexes[0]) * sizeof *vecto->items);

   return do_pop_continue_single(tsc, nargs, to);
}

/* implement 'vector-append' */
static eval_status_t proc_vector_append(tr7_engine_t tsc, int nargs)
{
   tr7_t *to, *from, x, res;
   tr7_uint_t len;
   int iarg;

   /* compute needed length for new vector */
   for (len = 0, iarg = 0; iarg < nargs; iarg++)
      len += TR7_LENGTH_VECTOR(DATA(tsc, iarg));

   /* allocate */
   res = alloc_vector(tsc, len);
   if (TR7_IS_NIL(res))
      return raise_out_of_memory_error(tsc);

   /* store the contents of the argument vectors into the new vector */
   to = TR7_ITEMS_VECTOR(res);
   for (iarg = 0; iarg < nargs; iarg++) {
      x = DATA(tsc, iarg);
      from = TR7_ITEMS_VECTOR(x);
      len = TR7_LENGTH_VECTOR(x);
      memcpy(to, from, len * sizeof *to);
      to += len;
   }
   return do_pop_continue_single(tsc, nargs, res);
}

/* implement 'vector-fill!' */
static eval_status_t proc_vector_fill(tr7_engine_t tsc, int nargs)
{
   struct subvector_desc subd;
   tr7_t vec = DATA(tsc, 0);
   tr7_t fill = DATA(tsc, 1);
   if (!make_subvector_desc(&DATA(tsc, 2), nargs - 2, &subd, vec))
      return raise_out_of_bound_error(tsc, tr7_cons_n(tsc, nargs - 1, &DATA(tsc, 1), TR7_NIL));

   while (subd.indexes[0] < subd.indexes[1])
      subd.items[subd.indexes[0]++] = fill;

   return do_pop_continue_single(tsc, nargs, vec);
}

/*************************************************************************
* SECTION PROC_BYTEVECTOR
* -----------------------
*/
/* implement 'bytevector?' */
static eval_status_t proc_is_bytevector(tr7_engine_t tsc, int nargs)
{
   return do_pop_continue_boolean(tsc, 1, TR7_IS_BYTEVECTOR(DATA(tsc, 0)));
}

/*
* implement 'make-bytevector'
*/
static eval_status_t proc_make_bytevector(tr7_engine_t tsc, int nargs)
{
   tr7_uint_t len = (tr7_uint_t)tr7_to_int(DATA(tsc, 0));
   uint8_t ini = nargs == 2 ? (uint8_t)tr7_to_int(DATA(tsc, 1)) : 0;
   tr7_t vec = tr7_make_bytevector_fill(tsc, ini, len);
   return do_pop_continue_single_alloc(tsc, nargs, vec);
}
/*
*/
static tr7_t list_to_bytevector(tr7_engine_t tsc, tr7_t list)
{
   tr7_int_t ival;
   tr7_buffer_t bv;
   tr7_t res = TR7_NIL, item;
   int i, listlen = tr7_list_length(list);
   if (listlen >= 0) {
      res = tr7_make_bytevector(tsc, (unsigned)listlen);
      if (!TR7_IS_NIL(res)) {
         bv = TR7_TO_BYTEVECTOR(res);
         for (i = 0; TR7_IS_PAIR(list); list = TR7_CDR(list)) {
            item = TR7_CAR(list);
            ival = TR7_IS_INT(item) ? TR7_TO_INT(item) : -1;
            if (ival < 0 || ival > 255) {
               res = TR7_NIL;
               break;
            }
            bv->content[i++] = (uint8_t)ival;
         }
      }
   }
   return res;
}
/*
* implement 'bytevector'
*/
static eval_status_t proc_bytevector(tr7_engine_t tsc, int nargs)
{
   int i;
   tr7_buffer_t bv;
   tr7_t res = tr7_make_bytevector(tsc, (unsigned)nargs);
   if (TR7_IS_NIL(res))
      return raise_out_of_memory_error(tsc);
   bv = TR7_TO_BYTEVECTOR(res);
   for (i = 0; i < nargs ; i++)
      bv->content[i] = (uint8_t)tr7_to_int(DATA(tsc, i));
   return do_pop_continue_single(tsc, nargs, res);
}
/* implement 'bytevector-length' */
static eval_status_t proc_bytevector_length(tr7_engine_t tsc, int nargs)
{
   tr7_buffer_t bv = TR7_TO_BYTEVECTOR(DATA(tsc, 0));
   return do_pop_continue_integer(tsc, 1, (tr7_int_t)TR7_BUFFER_LENGTH(bv));
}
/*
* implement 'bytevector-u8-ref'
*/
static eval_status_t proc_bytevector_u8_ref(tr7_engine_t tsc, int nargs)
{
   tr7_buffer_t bv = TR7_TO_BYTEVECTOR(DATA(tsc, 0));
   tr7_uint_t idx = (tr7_uint_t)tr7_to_int(DATA(tsc, 1));
   if (idx >= TR7_BYTEVECTOR_LENGTH(bv))
      return raise_out_of_bound_error(tsc, DATA(tsc, 1));
   return do_pop_continue_single(tsc, 2, TR7_FROM_INT(bv->content[idx]));
}

/* implement 'bytevector-u8-set!' */
static eval_status_t proc_bytevector_u8_set(tr7_engine_t tsc, int nargs)
{
   tr7_t vec = DATA(tsc, 0);
   tr7_buffer_t bv = TR7_TO_BYTEVECTOR(vec);
   tr7_uint_t idx = (tr7_uint_t)tr7_to_int(DATA(tsc, 1));
   if (tr7_is_immutable(vec))
      return raise_immutable_error(tsc);
   if (idx >= TR7_BYTEVECTOR_LENGTH(bv))
      return raise_out_of_bound_error(tsc, DATA(tsc, 1));
   bv->content[idx] = (uint8_t)tr7_to_int(DATA(tsc, 2));
   return do_pop_continue_single(tsc, 3, vec);
}

struct subbytevector_desc {
   uint8_t *content;
   tr7_uint_t length;
   tr7_uint_t indexes[2];
};

static int make_subbytevector_desc(tr7_t args[], int nargs, struct subbytevector_desc *subd, tr7_t bytevector)
{
   int n;
   tr7_buffer_t bv = TR7_TO_BYTEVECTOR(bytevector);

   subd->content = bv->content;
   subd->length = TR7_BYTEVECTOR_LENGTH(bv);

   /* get bounds */
   n = get_start_end(args, nargs, subd->indexes);
   if (n < 0)
      return 0;
   if (n == 0)
      subd->indexes[0] = 0;
   else if (subd->indexes[0] > subd->length)
      return 0;
   if (n <= 1)
      subd->indexes[1] = subd->length;
   else if (subd->indexes[1] < subd->indexes[0] || subd->indexes[1] > subd->length)
      return 0;
   return 1;
}

static int get_subbytevector_desc(tr7_t args[], int nargs, struct subbytevector_desc *subd)
{
   return make_subbytevector_desc(&args[1], nargs - 1, subd, args[0]);
}

/* implement 'bytevector-copy' */
static eval_status_t proc_bytevector_copy(tr7_engine_t tsc, int nargs)
{
   struct subbytevector_desc subd;
   tr7_t res;

   if (!get_subbytevector_desc(&DATA(tsc, 0), nargs, &subd))
      return raise_out_of_bound_error(tsc, tr7_cons_n(tsc, nargs - 1, &DATA(tsc, 1), TR7_NIL));

   res = tr7_make_bytevector_copy(tsc, (uint8_t*)&subd.content[subd.indexes[0]], subd.indexes[1] - subd.indexes[0]);
   return do_pop_continue_single_alloc(tsc, nargs, res);
}

/* implement 'bytevector-copy!' */
static eval_status_t proc_bytevector_copy_to(tr7_engine_t tsc, int nargs)
{
   struct subbytevector_desc subd;

   /* get destination data */
   tr7_t to = DATA(tsc, 0);
   tr7_buffer_t vecto = TR7_TO_BYTEVECTOR(to);
   tr7_int_t at = tr7_to_int(DATA(tsc, 1));

   /* get source data */
   if (!get_subbytevector_desc(&DATA(tsc, 2), nargs - 2, &subd)
     || at < 0
     || (tr7_uint_t)at + subd.indexes[1] - subd.indexes[0] > TR7_BYTEVECTOR_LENGTH(vecto))
      return raise_out_of_bound_error(tsc, tr7_cons(tsc, TR7_FROM_INT(at), tr7_cons_n(tsc, nargs - 3, &DATA(tsc, 3), TR7_NIL)));

   memmove(&vecto->content[at], &subd.content[subd.indexes[0]], (subd.indexes[1] - subd.indexes[0]) * sizeof(char));

   return do_pop_continue_single(tsc, nargs, to);
}

/* implement 'bytevector-append' */
static eval_status_t proc_bytevector_append(tr7_engine_t tsc, int nargs)
{
   tr7_buffer_t vec, from;
   tr7_t res;
   tr7_uint_t pos, len;
   int iarg;

   /* compute needed length for new bytevector */
   for (len = 0, iarg = 0; iarg < nargs; iarg++)
      len += TR7_LENGTH_BYTEVECTOR(DATA(tsc, iarg));

   /* allocate */
   res = tr7_make_bytevector(tsc, len);
   if (TR7_IS_NIL(res))
      return raise_out_of_memory_error(tsc);
   vec = TR7_TO_BYTEVECTOR(res);

   /* store the contents of the argument bytevectors into the new bytevector */
   for (pos = 0, iarg = 0; iarg < nargs; iarg++) {
      from = TR7_TO_BYTEVECTOR(DATA(tsc, iarg));
      len = TR7_BYTEVECTOR_LENGTH(from);
      memmove(&vec->content[pos], from->content, len * sizeof(char));
      pos += len;
   }
   return do_pop_continue_single(tsc, nargs, res);
}

/* implement 'bytevector-fill!' */
static eval_status_t proc_bytevector_fill(tr7_engine_t tsc, int nargs)
{
   struct subbytevector_desc subd;
   tr7_t vec = DATA(tsc, 0);
   char fill = (char)tr7_to_int(DATA(tsc, 1));

   if (!make_subbytevector_desc(&DATA(tsc, 2), nargs - 2, &subd, vec))
      return raise_out_of_bound_error(tsc, tr7_cons_n(tsc, nargs - 2, &DATA(tsc, 2), TR7_NIL));

   memset(&subd.content[subd.indexes[0]], fill, subd.indexes[1] - subd.indexes[0]);

   return do_pop_continue_single(tsc, nargs, vec);
}


/* implement 'utf8->string' */
static eval_status_t proc_utf8_to_string(tr7_engine_t tsc, int nargs)
{
   tr7_t res;
   struct subbytevector_desc subd;

   if (!get_subbytevector_desc(&DATA(tsc, 0), nargs, &subd))
      return raise_out_of_bound_error(tsc, tr7_cons_n(tsc, nargs - 2, &DATA(tsc, 2), TR7_NIL));

   if (!utf8str_is_valid((uint8_t*)&subd.content[subd.indexes[0]], subd.indexes[1] - subd.indexes[0]))
      return raise_invalid_argument_error(tsc);

   res = tr7_make_string_copy_length(tsc, (char*)&subd.content[subd.indexes[0]], subd.indexes[1] - subd.indexes[0]);
   return do_pop_continue_single_alloc(tsc, nargs, res);
}

/* implement 'string->utf8' */
static eval_status_t proc_string_to_utf8(tr7_engine_t tsc, int nargs)
{
   tr7_t res;
   struct substring_desc subd;

   if (!get_substring_desc(&DATA(tsc, 0), nargs, &subd))
      return raise_out_of_bound_error(tsc, tr7_cons_n(tsc, nargs - 2, &DATA(tsc, 2), TR7_NIL));

   res = tr7_make_bytevector_copy(tsc, (uint8_t*)&subd.string[subd.offsets[0]], subd.offsets[1] - subd.offsets[0]);
   return do_pop_continue_single_alloc(tsc, nargs, res);
}

/*************************************************************************
* SECTION PROC_BOXES
* ------------------
*/
#if USE_SCHEME_BOX
static tr7_t make_box(tr7_engine_t tsc, tr7_t value)
{
   tr7_box_t box = get_cells(tsc, 2, 0);
   if (box == NULL)
      return TR7_NIL;
   TR7_CELL_HEAD(box) = TR7_MAKE_HEAD(1, Tr7_Head_Kind_Box);
   BOX_SET(box, value);
   return push_recent_cell(tsc, box);
}
/* implement 'box' */
static eval_status_t proc_box(tr7_engine_t tsc, int nargs)
{
   return do_pop_continue_single(tsc, 1, make_box(tsc, DATA(tsc, 0)));
}

/* implement 'box?' */
static eval_status_t proc_is_box(tr7_engine_t tsc, int nargs)
{
   return do_pop_continue_boolean(tsc, 1, IS_BOX(DATA(tsc, 0)));
}

/* implement 'unbox' */
static eval_status_t proc_unbox(tr7_engine_t tsc, int nargs)
{
   return do_pop_continue_single(tsc, 1, GET_BOX(DATA(tsc, 0)));
}

/* implement 'set-box!' */
static eval_status_t proc_set_box(tr7_engine_t tsc, int nargs)
{
   SET_BOX(DATA(tsc, 0), DATA(tsc, 1));
   return do_pop_continue_void(tsc, 2);
}
#endif
/*************************************************************************
* SECTION PROC_TR7_TAGGED_CLOSURES
* --------------------------------
*/
#if USE_TR7_TAGGED_CLOSURES
/*
* implement 'closure?'
*/
static eval_status_t proc_is_closure(tr7_engine_t tsc, int nargs)
{
   return do_pop_continue_boolean(tsc, 1, tr7_is_closure(DATA(tsc, 0)));
}
/*
* implement 'closure-copy'
*/
static eval_status_t proc_closure_copy(tr7_engine_t tsc, int nargs)
{
   tr7_closure_t closure;
   tr7_t copy, x = DATA(tsc, 0);

   if (!tr7_is_closure(x))
      return raise_error_msg_obj(tsc, "not a closure", x);

   closure = TR7_TO_CLOSURE(x);
   copy = mk_closure(tsc, closure->description, closure->upperframes,
                     TR7_CELL_HEAD(closure));
   tr7_closure_set_tag(copy, closure->tag);
   return do_pop_continue_single(tsc, 1, copy);
}
/*
* implement 'closure-set-tag!'
*/
static eval_status_t proc_closure_set_tag(tr7_engine_t tsc, int nargs)
{
   tr7_t x = DATA(tsc, 0);

   if (!tr7_is_closure(x))
      return raise_error_msg_obj(tsc, "not a closure", x);

   tr7_closure_set_tag(x, DATA(tsc, 1));
   return do_pop_continue_void(tsc, 2);
}
/*
* implement 'closure-get-tag'
*/
static eval_status_t proc_closure_get_tag(tr7_engine_t tsc, int nargs)
{
   tr7_t x = DATA(tsc, 0);

   if (!tr7_is_closure(x))
      return raise_error_msg_obj(tsc, "not a closure", x);

   return do_pop_continue_single(tsc, 1, tr7_closure_get_tag(x));
}
#endif
/*************************************************************************
* SECTION PROC_CONTROL
* --------------------
*/
/* implement 'procedure?' */
static eval_status_t proc_procedure(tr7_engine_t tsc, int nargs)
{
   return do_pop_continue_boolean(tsc, 1, tr7_is_procedure(DATA(tsc, 0)));
}

/* implement 'apply' */
static eval_status_t proc_apply(tr7_engine_t tsc, int nargs)
{
   int llen, idx, cargs;
   tr7_t list, *s, proc = DATA(tsc, 0);

   if (nargs == 1) {
      DATA_POP_N(tsc, nargs);
      cargs = 0;
   }
   else {
      list = DATA(tsc, nargs - 1);
      llen = tr7_list_length(list);
      if (llen < 0)
         return raise_error_msg_obj(tsc, "improper list", list);

      if (llen == 0) {
         s = &DATA(tsc, 0);
         for (idx = nargs - 2 ; idx ; idx--)
            s[idx + 1] = s[idx];
         DATA_POP_N(tsc, 2);
      }
      else if (llen == 1) {
         DATA(tsc, nargs - 1) = TR7_CAR(list);
         DATA_POP_N(tsc, 1);
      }
      else {
         s = data_stack_enter_safe(tsc, (unsigned)llen - 2);
         for (idx = nargs - 1; --idx ; s++)
            *s = s[llen - 1];
         for (; !TR7_IS_NIL(list) ; list = TR7_CDR(list))
            *s++ = TR7_CAR(list);
      }
      cargs = nargs + llen - 2;
   }
   return s_exec(tsc, proc, (unsigned)cargs);
}

/* implement 'call-with-current-continuation' */
static eval_status_t proc_callcc(tr7_engine_t tsc, int nargs)
{
   set_values_single(tsc, DATA_POP(tsc));/*GC*/
   return s_exec_1(tsc, tsc->values[0], mk_continuation(tsc));
}

/* implement 'values' */
static eval_status_t proc_values(tr7_engine_t tsc, int nargs)
{
   return do_pop_status_pop(tsc, nargs, Cycle_Continue);
}
/*
* implement 'call-with-values' (main)
*/
static eval_status_t proc_callvals(tr7_engine_t tsc, int nargs)
{
   tr7_t procfrom = DATA_POP(tsc);
   tr7_t procto = DATA_POP(tsc);
   oper_push_safe_2(tsc, OPER(CALLVALS_THEN), procto);
   return s_exec_0(tsc, procfrom);
}
/*
* implement 'call-with-values' (then)
*/
static eval_status_t _oper_callvals_then(tr7_engine_t tsc)
{
   unsigned nvals = data_stack_push_values(tsc);
   tr7_t proc = OPER_AT(tsc, 1);
   OPER_POP(tsc, 2);
   return s_exec(tsc, proc, nvals);
}
/*
* helper for implementation of 'map'
*/
static eval_status_t map_aux(tr7_engine_t tsc)
{
   /*
   * STACK: NARG CPT SLOW RESU TAIL PROC ARGS...
   *         0    1   2    3    4    5    6
   */
   tr7_t item, *args, *params = &DATA(tsc, 0);
   unsigned nargs = TR7_TO_UINT(params[0]);
   unsigned idx = TR7_TO_UINT(params[1]);

   /* detection of circular lists */
   if (idx & 1) {
      tr7_t slow = params[2];
      if (TR7EQ(slow, params[5]))
         return raise_error_msg(tsc, "improper list");
      params[2] = TR7_CDR(slow);
   }
   params[1] = TR7_FROM_INT(idx + 1);

   /* prepare call to proc */
   args = data_stack_enter_safe(tsc, nargs);
   for (idx = 0 ; idx < nargs ; idx++) {
      item = params[6 + idx];
      if (!TR7_IS_PAIR(item)) {
         OPER_POP(tsc, 1);
         return do_pop_status_single(tsc, (int)(nargs + nargs) + 6, params[3], Cycle_Return);
      }
      args[idx] = TR7_CAR(item);
      params[6 + idx] = TR7_CDR(item);
   }

   /* call now */
   return s_exec(tsc, params[5], nargs);
}
/*
* implement 'map'
*/
static eval_status_t proc_map(tr7_engine_t tsc, int nargs)
{
   stack_safe(tsc, (unsigned)nargs + 5 /*data*/ + 4 /*oper*/);
   data_push_safe_2(tsc, TR7_NIL, TR7_NIL);
   data_push_safe_3(tsc, TR7_FROM_INT(nargs - 1), TR7_FROM_INT(0), DATA(tsc, 3));
   oper_push_safe_1(tsc, OPER(MAP_THEN));
   return map_aux(tsc);
}
/*
* operator helping for implementation of 'map'
*/
static eval_status_t _oper_map_then(tr7_engine_t tsc)
{
   tr7_t last, *params, tail;
   last = TR7_CONS2(tsc, tsc->values[0], TR7_NIL);
   /*
   * STACK: NARG CPT SLOW RESU TAIL PROC ARGS...
   *         0    1   2    3    4    5    6
   */
   params = &DATA(tsc, 0);
   tail = params[4];
   params[4] = last;
   if (TR7_IS_NIL(tail))
      params[3] = last;
   else
      TR7_CDR(tail) = last;
   return map_aux(tsc);
}
/*
* helper of 'string-map'
*/
static eval_status_t strmap_aux(tr7_engine_t tsc)
{
   /*
   * STACK: IDX PORT NARGS PROC ARGS...
   *         0   1    2     3    4
   */
   tr7_char_t car;
   tr7_t item, *args, *params = &DATA(tsc, 0);
   tr7_uint_t iitm = TR7_TO_UINT(params[0]);
   unsigned idx, nargs = TR7_TO_UINT(params[2]);

   /* prepare call to proc */
   args = data_stack_enter_safe(tsc, nargs);
   for (idx = 0 ; idx < nargs ; idx++) {
      item = params[4 + idx];
      car = tr7_string_ref(item, iitm);
      if (car == TR7_CHAR_EOF) {
         item = port_get_string(tsc, params[1]);
         port_close(tsc, params[1], port_output);
         OPER_POP(tsc, 1);
         return do_pop_status_single(tsc, (int)(nargs + nargs) + 4, item, Cycle_Return);
      }
      item = TR7_FROM_CHAR(car);
      args[idx] = item;
   }

   /* call now */
   params[0] = TR7_FROM_UINT(iitm + 1);
   return s_exec(tsc, params[3], nargs);
}
/*
* implement 'string-map'
*/
static eval_status_t proc_strmap(tr7_engine_t tsc, int nargs)
{
   tr7_t port = port_from_scratch(tsc, port_string | port_textual);
   stack_safe(tsc, (unsigned)nargs + 3 /*data*/ + 4 /*oper*/);
   data_push_safe_3(tsc, TR7_FROM_UINT(0), port, TR7_FROM_INT(nargs - 1));
   oper_push_safe_1(tsc, OPER(STRMAP_THEN));
   return strmap_aux(tsc);
}
/*
* operator helping for implementation of 'string-map'
*/
static eval_status_t _oper_strmap_then(tr7_engine_t tsc)
{
   tr7_t port, car = tsc->values[0];
   /*
   * STACK: IDX PORT NARGS PROC ARGS...
   *         0   1    2     3    4
   */
   port = DATA(tsc, 1);
   if (!TR7_IS_CHAR(car))
      return raise_error_msg(tsc, "character expected as result of string-map proc");
   port_write_char(tsc, TR7__PORT__PORT(port), to_char_unicode(TR7_TO_CHAR(car)));
   return strmap_aux(tsc);
}
/*
* helper of 'vector-map'
*/
static eval_status_t vecmap_aux(tr7_engine_t tsc, tr7_uint_t iitm)
{
   /*
   * STACK: IDX RESU NARGS PROC ARGS...
   *         0   1    2     3    4
   */
   tr7_t item, *args, *params = &DATA(tsc, 0);
   unsigned idx, nargs = TR7_TO_UINT(params[2]);

   /* check end */
   if (iitm == 0) {
      OPER_POP(tsc, 1);
      return do_pop_status_single(tsc, 4 + (int)nargs, params[1], Cycle_Return);
   }
   iitm--;

   /* prepare call to proc */
   args = data_stack_enter_safe(tsc, nargs);
   for (idx = 0 ; idx < nargs ; idx++) {
      item = params[idx + 4];
      item = TR7_ITEM_VECTOR(item, iitm);
      args[idx] = item;
   }

   /* call now */
   params[0] = TR7_FROM_UINT(iitm);
   return s_exec(tsc, params[3], nargs);
}
/*
* implementation of 'vector-map'
*/
static eval_status_t proc_vecmap(tr7_engine_t tsc, int nargs)
{
   int idx;
   tr7_t vec;
   tr7_uint_t len, minlen = TR7_LENGTH_VECTOR(DATA(tsc, 1));
   for (idx = 2 ; idx < nargs ; idx++) {
      len = TR7_LENGTH_VECTOR(DATA(tsc, idx));
      if (len < minlen)
         minlen = len;
   }
   stack_safe(tsc, (unsigned)nargs + 3 /*data*/ + 4 /*oper*/);
   vec = tr7_make_vector_fill(tsc, minlen, TR7_VOID);
   data_push_safe_3(tsc, TR7_FROM_UINT(minlen), vec, TR7_FROM_INT(nargs - 1));
   oper_push_safe_1(tsc, OPER(VECMAP_THEN));
   return vecmap_aux(tsc, minlen);
}
/*
* operator helping for implementation of 'vector-map'
*/
static eval_status_t _oper_vecmap_then(tr7_engine_t tsc)
{
   tr7_uint_t idx;
   tr7_t vec, val = tsc->values[0];
   /*
   * STACK: IDX RESU NARGS PROC ARGS...
   *         0   1    2     3    4
   */
   idx = TR7_TO_UINT(DATA(tsc, 0));
   vec = DATA(tsc, 1);
   TR7_ITEM_VECTOR(vec, idx) = val;
   return vecmap_aux(tsc, idx);
}
/*
* helper for implementing 'for-each'
*/
static eval_status_t _oper_foreach_then(tr7_engine_t tsc)
{
   tr7_t item, slow, *args, *params;
   unsigned nargs, cpt, idx;

   /*
   * STACK: NARG CPT SLOW PROC ARGS...
   *         0    1   2    3    4
   */
   params = &DATA(tsc, 0);
   nargs = TR7_TO_UINT(params[0]);
   cpt = TR7_TO_UINT(params[1]);

   /* avoiding infinite loop */
   if (cpt & 1) {
      slow = params[2];
      if (TR7EQ(slow, DATA(tsc, 4)))
         return raise_error_msg(tsc, "improper list");
      params[2] = TR7_CDR(slow);
   }

   /* prepare call to proc */
   args = data_stack_enter_safe(tsc, nargs);
   for (idx = 0 ; idx < nargs ; idx++) {
      item = params[4 + idx];
      if (!TR7_IS_PAIR(item)) {
         OPER_POP(tsc, 1);
         return do_pop_status_void(tsc, (int)(nargs + nargs) + 4, Cycle_Goto);
      }
      params[4 + idx] = TR7_CDR(item);
      args[idx] = TR7_CAR(item);
   }

   /* call now */
   params[1] = TR7_FROM_INT(cpt + 1);
   return s_exec(tsc, params[3], nargs);
}
/*
* implementation of 'for-each'
*/
static eval_status_t proc_foreach(tr7_engine_t tsc, int nargs)
{
   stack_safe(tsc, (unsigned)nargs + 3 /*data*/ + 4 /*oper*/);
   data_push_safe_3(tsc, TR7_FROM_INT(nargs - 1), TR7_FROM_INT(0), DATA(tsc, 1));
   oper_push_safe_1(tsc, OPER(FOREACH_THEN));
   return _oper_foreach_then(tsc);
}
/*
* helper for implementing 'string-for-each'
*/
static eval_status_t _oper_strforeach_then(tr7_engine_t tsc)
{
   tr7_t item, *args, *params;
   unsigned idx, nargs;
   tr7_uint_t icar;
   tr7_char_t car;

   /*
   * STACK: NARG IDX PROC ARGS...
   *         0    1   2    3
   */
   params = &DATA(tsc, 0);
   nargs = TR7_TO_INT(params[0]);
   icar = TR7_TO_UINT(params[1]);

   /* prepare call to proc */
   args = data_stack_enter_safe(tsc, nargs);
   for (idx = 0 ; idx < nargs ; idx++) {
      item = params[3 + idx];
      car = tr7_string_ref(item, icar);
      if (car == TR7_CHAR_EOF) {
         OPER_POP(tsc, 1);
         return do_pop_status_void(tsc, (int)(nargs + nargs) + 3, Cycle_Goto);
      }
      args[idx] = TR7_FROM_CHAR(car);
   }

   /* call now */
   params[1] = TR7_FROM_UINT(icar + 1);
   return s_exec(tsc, params[2], nargs);
}
/*
* implementation of 'string-for-each'
*/
static eval_status_t proc_strforeach(tr7_engine_t tsc, int nargs)
{
   stack_safe(tsc, (unsigned)nargs + 2 /*data*/ + 4 /*oper*/);
   data_push_safe_2(tsc, TR7_FROM_INT(nargs - 1), TR7_FROM_UINT(0));
   oper_push_safe_1(tsc, OPER(STRFOREACH_THEN));
   return _oper_strforeach_then(tsc);
}

/*
* helper for implementing 'vector-for-each'
*/
static eval_status_t _oper_vecforeach_then(tr7_engine_t tsc)
{
   tr7_t item, *args, *params;
   unsigned idx, nargs;
   tr7_uint_t iitm;

   /*
   * STACK: NARG IDX PROC ARGS...
   *         0    1   2    3
   */
   params = &DATA(tsc, 0);
   nargs = TR7_TO_INT(params[0]);
   iitm = TR7_TO_UINT(params[1]);

   /* prepare call to proc */
   args = data_stack_enter_safe(tsc, nargs);
   for (idx = 0 ; idx < nargs ; idx++) {
      item = params[3 + idx];
      if (iitm >= TR7_LENGTH_VECTOR(item)) {
         OPER_POP(tsc, 1);
         return do_pop_status_void(tsc, (int)(nargs + nargs) + 3, Cycle_Goto);
      }
      args[idx] = TR7_ITEM_VECTOR(item, iitm);
   }

   /* call now */
   params[1] = TR7_FROM_UINT(iitm + 1);
   return s_exec(tsc, params[2], nargs);
}
/*
* implementation of 'vector-for-each'
*/
static eval_status_t proc_vecforeach(tr7_engine_t tsc, int nargs)
{
   stack_safe(tsc, (unsigned)nargs + 2 /*data*/ + 4 /*oper*/);
   data_push_safe_2(tsc, TR7_FROM_INT(nargs - 1), TR7_FROM_UINT(0));
   oper_push_safe_1(tsc, OPER(VECFOREACH_THEN));
   return _oper_vecforeach_then(tsc);
}

/* implement 'dynamic-wind' */
static eval_status_t proc_dynamic_wind(tr7_engine_t tsc, int nargs)
{
   set_values_pop(tsc, 3);/*GC*/
   dynawind_push(tsc, tsc->values[0], tsc->values[2]);
   oper_push_safe_3(tsc, OPER(XCALL), TR7_FROM_INT(0), tsc->values[2]);
   oper_push_safe_1(tsc, OPER(DWPOP));
   oper_push_safe_3(tsc, OPER(XCALL), TR7_FROM_INT(0), tsc->values[1]);
   return s_exec_0(tsc, tsc->values[0]);
}



/* helper for calling with a port */
static eval_status_t do_call_with_port(tr7_engine_t tsc, tr7_t port)
{
   if (!TR7_IS_PORT(port))
      return raise_error(tsc, port);
   tr7_t proc = DATA(tsc, 1);
   DATA_POP_N(tsc, 2);
   oper_push_safe_2(tsc, OPER(CLOPORT), port);
   return s_exec_1(tsc, proc, port);
}

/* implementation of 'call-with-port' */
static eval_status_t proc_call_with_port(tr7_engine_t tsc, int nargs)
{
   tr7_t port = DATA(tsc, 0);
   return do_call_with_port(tsc, port);
}

/* implementation of 'input-port?' */
static eval_status_t proc_is_input_port(tr7_engine_t tsc, int nargs)
{
   return do_pop_continue_boolean(tsc, 1, tr7_is_input_port(DATA(tsc, 0)));
}

/* implementation of 'output-port?' */
static eval_status_t proc_is_output_port(tr7_engine_t tsc, int nargs)
{
   return do_pop_continue_boolean(tsc, 1, tr7_is_output_port(DATA(tsc, 0)));
}

/* implementation of 'textual-port?' */
static eval_status_t proc_is_textual_port(tr7_engine_t tsc, int nargs)
{
   return do_pop_continue_boolean(tsc, 1, tr7_is_textual_port(DATA(tsc, 0)));
}

/* implementation of 'binary-port?' */
static eval_status_t proc_is_binary_port(tr7_engine_t tsc, int nargs)
{
   return do_pop_continue_boolean(tsc, 1, tr7_is_binary_port(DATA(tsc, 0)));
}

/* implementation of 'port?' */
static eval_status_t proc_is_port(tr7_engine_t tsc, int nargs)
{
   return do_pop_continue_boolean(tsc, 1, TR7_IS_PORT(DATA(tsc, 0)));
}

/* operator for closing port */
static eval_status_t _oper_close_port(tr7_engine_t tsc)
{
   tr7_t port = OPER_AT(tsc, 1);
   OPER_POP(tsc, 2);
   port_close(tsc, port, port_input|port_output);
   return Cycle_Goto;
}

/* implementation of 'close-port' */
static eval_status_t proc_close_port(tr7_engine_t tsc, int nargs)
{
   port_close(tsc, DATA(tsc, 0), port_input|port_output);
   return do_pop_continue_void(tsc, 1);
}

/* implementation of 'close-input-port' */
static eval_status_t proc_close_input_port(tr7_engine_t tsc, int nargs)
{
   port_close(tsc, DATA(tsc, 0), port_input);
   return do_pop_continue_void(tsc, 1);
}

/* implementation of 'close-output-port' */
static eval_status_t proc_close_output_port(tr7_engine_t tsc, int nargs)
{
   port_close(tsc, DATA(tsc, 0), port_output);
   return do_pop_continue_void(tsc, 1);
}

/* implementation of 'open-input-string' */
static eval_status_t proc_open_input_string(tr7_engine_t tsc, int args)
{
   tr7_t string = DATA(tsc, 0);
   tr7_t port = port_from_string(tsc, string, TR7_CONTENT_STRING(string), NULL);
   return do_pop_continue_single(tsc, 1, port);
}

/* implementation of 'open-output-string' */
static eval_status_t proc_open_output_string(tr7_engine_t tsc, int nargs)
{
   return do_pop_continue_single(tsc, 0, port_from_scratch(tsc, port_string | port_textual));
}

/* implementation of 'get-output-string' */
static eval_status_t proc_get_output_string(tr7_engine_t tsc, int nargs)
{
   return do_pop_continue_single(tsc, 1, port_get_string(tsc, DATA(tsc, 0)));
}

/* implementation of 'open-input-bytevector' */
static eval_status_t proc_open_input_bytevector(tr7_engine_t tsc, int nargs)
{
   tr7_t bytevector = DATA(tsc, 0);
   tr7_t port = port_from_bytevector(tsc, bytevector);
   return do_pop_continue_single(tsc, 1, port);
}

/* implementation of 'open-output-bytevector' */
static eval_status_t proc_open_output_bytevector(tr7_engine_t tsc, int nargs)
{
   return do_pop_continue_single(tsc, 0, port_from_scratch(tsc, port_bytevector | port_binary));
}

/* implementation of 'get-output-bytevector' */
static eval_status_t proc_get_output_bytevector(tr7_engine_t tsc, int nargs)
{
   return do_pop_continue_single(tsc, 1, port_get_bytevector(tsc, DATA(tsc, 0)));
}

static port_t *get_optional_port(tr7_engine_t tsc, int nargs, int iarg, int stdidx)
{
   tr7_t port = iarg < nargs ? DATA(tsc, iarg) : get_stdport(tsc, stdidx);
   return TR7__PORT__PORT(port);
}

static port_t *get_optional_inport(tr7_engine_t tsc, int nargs, int iarg)
{
   return get_optional_port(tsc, nargs, iarg, IDX_STDIN);
}

static port_t *get_optional_outport(tr7_engine_t tsc, int nargs, int iarg)
{
   return get_optional_port(tsc, nargs, iarg, IDX_STDOUT);
}

/* implementation of 'read-char' */
static eval_status_t proc_read_char(tr7_engine_t tsc, int nargs)
{
   port_t *pt = get_optional_inport(tsc, nargs, 0);
   tr7_char_t car = port_read_char(tsc, pt);
   return do_pop_continue_char_or_EOF(tsc, nargs, car);
}

/* implementation of 'peek-char' */
static eval_status_t proc_peek_char(tr7_engine_t tsc, int nargs)
{
   port_t *pt = get_optional_inport(tsc, nargs, 0);
   tr7_char_t car = port_read_char(tsc, pt);
   port_unread_char(tsc, pt, car);
   return do_pop_continue_char_or_EOF(tsc, nargs, car);
}

/* implementation of 'read-line' */
static eval_status_t proc_read_line(tr7_engine_t tsc, int nargs)
{
   tr7_t res;
   tr7_char_t car;
   int cr = 0;
   port_t *pt = get_optional_inport(tsc, nargs, 0);

   strbuff_start(tsc);
   for(;;) {
      car = port_read_char(tsc, pt);
      if (car == TR7_CHAR_EOF && strbuff_length(tsc) == 0)
         return do_pop_continue_EOF(tsc, nargs);
      if (car == TR7_CHAR_EOF || car == '\n') {
         if (!strbuff_stop(tsc))
            break;
         res = strbuff_string(tsc);
         return do_pop_continue_single_alloc(tsc, nargs, res);
      }
      if (cr && !strbuff_add(tsc, '\r'))
         break;
      if (car == '\r')
         cr = 1;
      else if (!strbuff_add(tsc, car))
         break;
   }
   return raise_file_error(tsc, "line too long", TR7_NIL);
}

/* implementation of 'eof-object?' */
static eval_status_t proc_is_eof_object(tr7_engine_t tsc, int nargs)
{
   return do_pop_continue_boolean(tsc, 1, TR7_IS_EOF(DATA(tsc, 0)));
}

/* implementation of 'eof-object' */
static eval_status_t proc_eof_object(tr7_engine_t tsc, int nargs)
{
   return do_pop_continue_EOF(tsc, 0);
}

/* implementation of 'char-ready?' */
static eval_status_t proc_is_char_ready(tr7_engine_t tsc, int nargs)
{
   port_t *pt = get_optional_inport(tsc, nargs, 0);
   int ready = port_has_char(tsc, pt);
   return do_pop_continue_boolean(tsc, 1, ready);
}

/* implementation of 'read-string' */
static eval_status_t proc_read_string(tr7_engine_t tsc, int nargs)
{
   tr7_t res;
   tr7_char_t car;
   port_t *pt = get_optional_inport(tsc, nargs, 1);
   tr7_int_t idx, len = tr7_to_int(DATA(tsc, 0));
   strbuff_start(tsc);
   for (idx = 0 ; idx < len ; idx++) {
      car = port_read_char(tsc, pt);
      if (car == TR7_CHAR_EOF)
         break;
      if (!strbuff_add(tsc, car))
         return raise_out_of_memory_error(tsc);
   }
   res = strbuff_string(tsc);
   return do_pop_continue_single_alloc(tsc, nargs, res);
}

/* implementation of 'read-u8' */
static eval_status_t proc_read_u8(tr7_engine_t tsc, int nargs)
{
   port_t *pt = get_optional_inport(tsc, nargs, 0);
   int byte = port_read_byte(tsc, pt);
   return do_pop_continue_u8_or_EOF(tsc, nargs, byte);
}

/* implementation of 'peek-u8' */
static eval_status_t proc_peek_u8(tr7_engine_t tsc, int nargs)
{
   port_t *pt = get_optional_inport(tsc, nargs, 0);
   int byte = port_read_byte(tsc, pt);
   port_unread_byte(tsc, pt, byte);
   return do_pop_continue_u8_or_EOF(tsc, nargs, byte);
}

/* implementation of 'u8-ready?' */
static eval_status_t proc_is_u8_ready(tr7_engine_t tsc, int nargs)
{
   port_t *pt = get_optional_inport(tsc, nargs, 0);
   int ready = port_has_byte(tsc, pt);
   return do_pop_continue_boolean(tsc, 1, ready);
}

/* implementation of 'read-bytevector' */
static eval_status_t proc_read_bytevector(tr7_engine_t tsc, int nargs)
{
   uint8_t *content;
   port_t *pt = get_optional_inport(tsc, nargs, 1);
   tr7_int_t nread;
   tr7_uint_t len = TR7_TO_UINT(DATA(tsc, 0));
   tr7_t res = tr7_make_bytevector(tsc, len);
   if (TR7_IS_NIL(res))
      return raise_out_of_memory_error(tsc);
   content = TR7_CONTENT_BYTEVECTOR(res);
   nread = port_read_bytes(tsc, pt, content, len);
   if (nread == EOF)
      return do_pop_continue_EOF(tsc, nargs);
   if ((tr7_uint_t)nread < len)
      TR7_SET_LENGTH_BYTEVECTOR(res, nread);
   return do_pop_continue_single(tsc, nargs, res);
}

/* implementation of 'read-bytevector!' */
static eval_status_t proc_read_bytevector_in(tr7_engine_t tsc, int nargs)
{
   struct subbytevector_desc subd;
   tr7_int_t nread;
   tr7_t bv = DATA(tsc, 0);
   port_t *pt = get_optional_inport(tsc, nargs, 1);

   if (!make_subbytevector_desc(&DATA(tsc, 2), nargs - 2, &subd, bv))
      return raise_out_of_bound_error(tsc, tr7_cons_n(tsc, nargs - 2, &DATA(tsc, 2), TR7_NIL));

   nread = port_read_bytes(tsc, pt, &subd.content[subd.indexes[0]], subd.indexes[1] - subd.indexes[0]);
   return do_pop_continue_single(tsc, nargs, nread == EOF ? TR7_EOF : TR7_FROM_INT(nread));
}

/*************************************************************************
* SECTION SCHEME_WRITE
* --------------------
*/
#if USE_SCHEME_WRITE
static eval_status_t do_write(tr7_engine_t tsc, int nargs, int pflags)
{
   port_t *pt = get_optional_outport(tsc, nargs, 1);
   tr7_t obj = DATA(tsc, 0);
   do_print(tsc, pt, pflags, obj);
   return do_pop_continue_void(tsc, nargs);
}

/* implementation of 'write' */
static eval_status_t proc_write(tr7_engine_t tsc, int nargs)
{
   return do_write(tsc, nargs, PRTFLG_LOOPS | PRTFLG_ESCAPE);
}

/* implementation of 'write-simple' */
static eval_status_t proc_write_simple(tr7_engine_t tsc, int nargs)
{
   return do_write(tsc, nargs, PRTFLG_ESCAPE);
}

/* implementation of 'write-shared' */
static eval_status_t proc_write_shared(tr7_engine_t tsc, int nargs)
{
   return do_write(tsc, nargs, PRTFLG_SHAREDS | PRTFLG_ESCAPE);
}

/* implementation of 'display' */
static eval_status_t proc_display(tr7_engine_t tsc, int nargs)
{
   return do_write(tsc, nargs, PRTFLG_LOOPS);
}
#endif

/* implementation of 'write-char' */
static eval_status_t proc_write_char(tr7_engine_t tsc, int nargs)
{
   tr7_char_t car = TR7_TO_CHAR(DATA(tsc, 0));
   port_t *pt = get_optional_outport(tsc, nargs, 1);
   port_write_char(tsc, pt, to_char_unicode(car));
   return do_pop_continue_void(tsc, nargs);
}

/* implementation of 'newline' */
static eval_status_t proc_write_newline(tr7_engine_t tsc, int nargs)
{
   port_t *pt = get_optional_outport(tsc, nargs, 0);
   port_write_char(tsc, pt, '\n');
   return do_pop_continue_void(tsc, nargs);
}

/* implementation of 'write-string' */
static eval_status_t proc_write_string(tr7_engine_t tsc, int nargs)
{
   struct substring_desc subd;
   tr7_t str = DATA(tsc, 0);
   port_t *pt = get_optional_outport(tsc, nargs, 1);
   if (!make_substring_desc(&DATA(tsc, 2), nargs - 2, &subd, str))
      return raise_out_of_bound_error(tsc, tr7_cons_n(tsc, nargs - 2, &DATA(tsc, 2), TR7_NIL));
   port_write_utf8_length(tsc, pt, (char*)&subd.string[subd.offsets[0]], subd.offsets[1] - subd.offsets[0]);
   return do_pop_continue_void(tsc, nargs);
}

/* implementation of 'write-u8' */
static eval_status_t proc_write_u8(tr7_engine_t tsc, int nargs)
{
   uint8_t byte = (uint8_t)TR7_TO_INT(DATA(tsc, 0));
   port_t *pt = get_optional_outport(tsc, nargs, 1);
   port_write_bytes(tsc, pt, &byte, 1);
   return do_pop_continue_void(tsc, nargs);
}

/* implementation of 'write-bytevector' */
static eval_status_t proc_write_bytevector(tr7_engine_t tsc, int nargs)
{
   struct subbytevector_desc subd;
   tr7_t bv = DATA(tsc, 0);
   port_t *pt = get_optional_outport(tsc, nargs, 1);
   if (!make_subbytevector_desc(&DATA(tsc, 2), nargs - 2, &subd, bv))
      return raise_out_of_bound_error(tsc, tr7_cons_n(tsc, nargs - 2, &DATA(tsc, 2), TR7_NIL));
   port_write_bytes(tsc, pt, &subd.content[subd.indexes[0]], subd.indexes[1] - subd.indexes[0]);
   return do_pop_continue_void(tsc, nargs);
}

/* implementation of 'flush-output-port' */
static eval_status_t proc_flush_output_port(tr7_engine_t tsc, int nargs)
{
   port_t *pt = get_optional_outport(tsc, nargs, 0);
   port_flush(tsc, pt);
   return do_pop_continue_void(tsc, nargs);
}
/*
**************************************************************************
* SECTION SCHEME_PROCESS_CONTEXT
* ------------------------------
*
*
*/
#if USE_SCHEME_PROCESS_CONTEXT
extern char **environ;
static char **memo_argv = NULL;

void tr7_set_argv(char **argv)
{
   memo_argv = argv;
}

tr7_t tr7_command_line(tr7_engine_t tsc)
{
   tr7_t result, *prev, item;
   char **argv;

   result = TR7_NIL;
   argv = memo_argv;
   if (argv) {
      prev = &result;
      while (*argv) {
         item = tr7_make_string_static(tsc, *argv++);
         *prev = tr7_cons(tsc, item, TR7_NIL);
         prev = &TR7_CDR(*prev);
      }
   }
   return result;
}

/* implementation of 'command-line' */
static eval_status_t proc_command_line(tr7_engine_t tsc, int nargs)
{
   return do_pop_continue_single(tsc, 0, tr7_command_line(tsc));
}

/* realization of exits */
static eval_status_t do_exit(tr7_engine_t tsc, int nargs, int urge)
{
   int rc = EXIT_SUCCESS;
   if (nargs == 1) {
      tr7_t x = DATA(tsc, 0);
      if (!TR7_IS_TRUE(x)) {
         if (TR7_IS_INT(x))
            rc = (int)TR7_TO_INT(x);
         else
            rc = EXIT_FAILURE;
      }
   }
   if (urge)
      _exit(rc);
   else {
      /* TODO unwind afters */
      exit(rc);
   }
   return raise_error_msg(tsc, "exit failed?");
}

/* implementation of 'exit' */
static eval_status_t proc_exit(tr7_engine_t tsc, int nargs)
{
   return do_exit(tsc, nargs, 0);
}

/* implementation of 'emergency-exit' */
static eval_status_t proc_emergency_exit(tr7_engine_t tsc, int nargs)
{
   return do_exit(tsc, nargs, 1);
}

/* implementation of 'get-environment-variable' */
static eval_status_t proc_get_env_var(tr7_engine_t tsc, int nargs)
{
   /* TODO: avoid copy and make immutable */
   char *str = getenv((const char*)TR7_CONTENT_STRING(DATA(tsc, 0)));
   tr7_t val = str ? tr7_make_string_copy(tsc, str) : TR7_FALSE;
   return do_pop_continue_single(tsc, 1, val);
}

/* implementation of 'get-environment-variables' */
static eval_status_t proc_get_env_vars(tr7_engine_t tsc, int nargs)
{
   /* TODO: avoid copy and make immutable */
   tr7_t tn, tv, res = TR7_NIL;
   char **it, *name, *val;
   for (it = environ; it && (name = *it) ; it++) {
      val = strchr(name, '=');
      if (val == NULL) {
         tn = tr7_make_string_copy(tsc, name);
         tv = tr7_make_string_copy(tsc, "");
      } else {
         tn = tr7_make_string_copy_length(tsc, name, (unsigned)(val - name));
         tv = tr7_make_string_copy(tsc, &val[1]);
      }
      res = tr7_cons(tsc, tr7_cons(tsc, tn, tv), res);
   }
   return do_pop_continue_single(tsc, 0, res);
}
#else
void tr7_set_argv(char **argv) {}
tr7_t tr7_command_line(tr7_engine_t tsc) { return TR7_NIL; }
#endif
/*
**************************************************************************
* SECTION SCHEME_TIME
* -------------------
*
*
*/
#if USE_SCHEME_TIME
#if _POSIX_C_SOURCE >= 199309L && !defined(_WIN32)
#ifndef CLOCK_TAI
#define CLOCK_TAI CLOCK_REALTIME
#endif
/* implement 'current-second' */
static eval_status_t proc_current_second(tr7_engine_t tsc, int nargs)
{
   struct timespec ts;
   double s, ns;
   clock_gettime(CLOCK_TAI, &ts);
   s = (double)ts.tv_sec;
   ns = (double)ts.tv_nsec * 1.0e-9;
   return do_pop_continue_double(tsc, 0, s + ns);
}

/* implement 'current-jiffy' */
static eval_status_t proc_current_jiffy(tr7_engine_t tsc, int nargs)
{
   static struct timespec base = { 0, 0};
   struct timespec ts;
   tr7_int_t r;
   clock_gettime(CLOCK_TAI, &ts);
   if (base.tv_sec == 0)
      base = ts;
   r = (tr7_int_t)(ts.tv_sec - base.tv_sec) * 1000;
   r += ((tr7_int_t)ts.tv_nsec - (tr7_int_t)base.tv_nsec) / 1000000;
   return do_pop_continue_integer(tsc, 0, r);
}

/* implement 'jiffies-per-second' */
static eval_status_t proc_jiffies_per_second(tr7_engine_t tsc, int nargs)
{
   return do_pop_continue_integer(tsc, 0, 1000);
}
#else
/* implement 'current-second' */
static eval_status_t proc_current_second(tr7_engine_t tsc, int nargs)
{
   return do_pop_continue_double(tsc, 0, (double)time(NULL));
}

/* implement 'current-jiffy' */
static eval_status_t proc_current_jiffy(tr7_engine_t tsc, int nargs)
{
   static time_t base = 0;
   time_t t = time(NULL);
   if (base == 0)
      base = t;
   return do_pop_continue_integer(tsc, 0, (int)(t - base));
}

/* implement 'jiffies-per-second' */
static eval_status_t proc_jiffies_per_second(tr7_engine_t tsc, int nargs)
{
   return do_pop_continue_integer(tsc, 0, 1);
}
#endif
#endif

/*************************************************************************
* SECTION SCHEME_LOAD
* -------------------
*/
#if USE_SCHEME_LOAD
/* implement 'load' */
static eval_status_t proc_load(tr7_engine_t tsc, int nargs)
{
   tr7_t file = DATA(tsc, 0);
   const char *base = (const char*)TR7_CONTENT_STRING(file);
   tr7_t env = nargs == 2 ? DATA(tsc, 1)
                : make_interaction_environment(tsc, SYMBOL_SET_SIZE);
   if (!load_enter_search_load(tsc, NULL, base, 0))
      return raise_error_msg_obj(tsc, "unable to open", file);
   DATA_POP_N(tsc, nargs);
   oper_push_safe_2(tsc, OPER(SENV), tsc->curenv);
   oper_push_safe_1(tsc, OPER(REPL_READ));
   tsc->curenv = env;
   return Cycle_Goto;
}
#endif

/*************************************************************************
* SECTION SCHEME_FILE
* -------------------
*/
#if USE_SCHEME_FILE
static eval_status_t do_call_with_file(tr7_engine_t tsc, unsigned prop)
{
   tr7_t file = DATA(tsc, 0);
   tr7_t proc = DATA(tsc, 1);
   tr7_t port = port_from_file(tsc, NULL, (char*)TR7_CONTENT_STRING(file), prop);
   if (!TR7_IS_PORT(port))
      return raise_file_error(tsc, "can't open", file);
   oper_push_safe_2(tsc, OPER(CLOPORT), port);
   DATA_POP_N(tsc, 2);
   return s_exec_1(tsc, proc, port);
}

/* helper for calling with a port */
static eval_status_t do_with_file(tr7_engine_t tsc, unsigned prop, int idx)
{
   tr7_t file = DATA(tsc, 0);
   tr7_t thunk = DATA(tsc, 1);
   tr7_t port = port_from_file(tsc, NULL, (char*)TR7_CONTENT_STRING(file), prop);
   if (!TR7_IS_PORT(port))
      return raise_file_error(tsc, "can't open", file);
   push_stdport(tsc, port, idx);
   oper_push_safe_2(tsc, OPER(CLOPORT), port);
   DATA_POP_N(tsc, 2);
   return s_exec_0(tsc, thunk);
}

/* helper for opening file */
static eval_status_t do_open_file(tr7_engine_t tsc, unsigned prop)
{
   tr7_t p = port_from_file(tsc, NULL, (char*)TR7_CONTENT_STRING(DATA(tsc, 0)), prop);
   if (TR7_IS_FALSE(p))
      return raise_file_error(tsc, "can't open", DATA(tsc, 0));
   return do_pop_continue_single(tsc, 1, p);
}

/* implement 'file-exists?' */
static eval_status_t proc_file_exists(tr7_engine_t tsc, int nargs)
{
   const char *filename = (const char*)TR7_CONTENT_STRING(DATA(tsc, 0));
   return do_pop_continue_boolean(tsc, 1, access(filename, F_OK) == 0);
}

/* implement 'delete-file' */
static eval_status_t proc_delete_file(tr7_engine_t tsc, int nargs)
{
   const char *filename = (const char*)TR7_CONTENT_STRING(DATA(tsc, 0));
   if (unlink(filename) < 0) /* TODO: file-error */
      return raise_error_msg_obj(tsc, "can't delete", DATA(tsc, 0));
   return do_pop_continue_void(tsc, 1);
}

/* implementation of 'open-input-file' */
static eval_status_t proc_open_input_file(tr7_engine_t tsc, int nargs)
{
   return do_open_file(tsc, port_input | port_textual);
}

/* implementation of 'open-binary-input-file' */
static eval_status_t proc_open_binary_input_file(tr7_engine_t tsc, int nargs)
{
   return do_open_file(tsc, port_input | port_binary);
}

/* implementation of 'open-output-file' */
static eval_status_t proc_open_output_file(tr7_engine_t tsc, int nargs)
{
   return do_open_file(tsc, port_output | port_textual);
}

/* implementation of 'open-binary-output-file' */
static eval_status_t proc_open_binary_output_file(tr7_engine_t tsc, int nargs)
{
   return do_open_file(tsc, port_output | port_binary);
}

/* implementation of 'call-with-input-file' */
static eval_status_t proc_call_with_input_file(tr7_engine_t tsc, int nargs)
{
   return do_call_with_file(tsc, port_input | port_textual);
}

/* implementation of 'call-with-output-file' */
static eval_status_t proc_call_with_output_file(tr7_engine_t tsc, int nargs)
{
   return do_call_with_file(tsc, port_output | port_textual);
}

/* implementation of 'with-input-from-file' */
static eval_status_t proc_with_input_file(tr7_engine_t tsc, int nargs)
{
   return do_with_file(tsc, port_input | port_textual, IDX_STDIN);
}

/* implementation of 'with-output-to-file' */
static eval_status_t proc_with_output_file(tr7_engine_t tsc, int nargs)
{
   return do_with_file(tsc, port_output | port_textual, IDX_STDOUT);
}
#endif

/*************************************************************************
* SECTION FEATURES
* ----------------
*/
/* implement 'features' */
static eval_status_t proc_features(tr7_engine_t tsc, int nargs)
{
   return do_pop_continue_single(tsc, 0, get_features_list(tsc));
}

/*************************************************************************
* SECTION SCHEME_EVAL
* -------------------
*/
#if USE_SCHEME_EVAL
/*
* implement 'eval'
*/
static eval_status_t proc_eval(tr7_engine_t tsc, int nargs)
{
   if (nargs == 2) {
      oper_push_safe_2(tsc, OPER(SENV), tsc->curenv);
      tsc->curenv = DATA(tsc, 1);
   }
   oper_push_safe_2(tsc, OPER(IEVAL), DATA(tsc, 0));
   return do_pop_status(tsc, nargs, Cycle_Goto);
}
/*
* implement 'environment'
*/
static eval_status_t proc_environment(tr7_engine_t tsc, int nargs)
{
   tr7_t env = mk_environment(tsc, TR7_NIL, DEFAULT_ENV_SIZE);
   tr7_t list = tr7_cons_n(tsc, nargs, &DATA(tsc, 0), TR7_NIL);
   int rc = import(tsc, list, env);
   if (rc < 0)
      return Cycle_Raise;
   return do_pop_continue_single(tsc, nargs, env);
}
#endif

/*************************************************************************
* SECTION SCHEME_REPL
* -------------------
*/
#if USE_SCHEME_REPL
/* implement 'interaction-environment' */
static eval_status_t proc_interaction_environment(tr7_engine_t tsc, int nargs)
{
   tr7_t env = make_interaction_environment(tsc, SYMBOL_SET_SIZE);
   return do_pop_continue_single(tsc, 0, env);
}
#endif

#if USE_TR7_EXTENSION
/* procedure 'load-extension' */
static eval_status_t proc_load_extension(tr7_engine_t tsc, int nargs)
{
   const char *libname = NULL;
   if (nargs == 2)
      libname = (const char*)TR7_CONTENT_STRING(DATA(tsc, 1));
   if (!dl_load_ext(tsc, (const char*)TR7_CONTENT_STRING(DATA(tsc, 0)), libname))
      return raise_error_msg_obj(tsc, "unable to load extension", DATA(tsc, 0));
   return do_pop_continue_void(tsc, 1);
}
#endif

/*************************************************************************
* SECTION READ
* ------------
*/

static int do_read_set(tr7_engine_t tsc, tr7_t item, int stsok)
{
   tsc->read_value = item;
   return TR7_IS_NIL(item) ? -read_error_oom : stsok;
}

static int do_read_set_list2(tr7_engine_t tsc, tr7_t item, int stsok)
{
   return do_read_set(tsc, TR7_LIST2(tsc, item, tsc->read_value), stsok);
}

static int do_read_set_list3(tr7_engine_t tsc, tr7_t item, tr7_t item2, int stsok)
{
   return do_read_set(tsc, TR7_LIST3(tsc, item, item2, tsc->read_value), stsok);
}

static int do_read_mix_status(int prvsts, int newsts)
{
   return prvsts < 0 ? prvsts : newsts < 0 ? newsts : prvsts > newsts ? prvsts : newsts;
}

static int do_read(tr7_engine_t tsc, port_t *pt, int funq);
static int do_read_true_token(tr7_engine_t tsc, port_t *pt, int funq, token_type_t *token)
{
   for (;;) {
      token_type_t tok = *token = read_token(tsc, pt);
      if (tok == Token_Comment_Datum) {
         int sts = do_read(tsc, pt, funq);
         if (sts < 0)
            return sts;
      }
      else if (tok != Token_Comment)
         return 0;
   }
}

static int do_read(tr7_engine_t tsc, port_t *pt, int funq)
{
   token_type_t token;
   int sts = do_read_true_token(tsc, pt, funq, &token);
   return sts < 0 ? sts : do_read_with_token(tsc, pt, funq, token);
}

/*
* read a scheme started list, vector or bytevector (will be stopped by a closing par)
* returns a negative value on error or 0 if okay or 1 if okay but with an unquoting
*/
static int do_read_list(tr7_engine_t tsc, port_t *pt, int funq)
{
   tr7_t pair, head = TR7_NIL;
   tr7_t *prev = &head;
   int sts = 0;
   int insts;
   token_type_t tok;
#if USE_TR7_DEBUG && DEBUG_LINES
   tr7_t holder = line_starts_holder_get(tsc, pt);
#endif
   sts = do_read_true_token(tsc, pt, funq, &tok);
   if (sts >= 0 && tok != Token_Right_Par) {
      /* treat special cases of quoting */
      if (tok == Token_Value) {
         if (TR7EQ(tsc->read_value, SYMBOL(QUASIQUOTE)))
            funq++;
         else if (TR7EQ(tsc->read_value, SYMBOL(UNQUOTE))
               || TR7EQ(tsc->read_value, SYMBOL(UNQUOTE_SPLICING)))
            sts = 1;
      }
      while(tok != Token_Right_Par && tok != Token_EOF && tok != Token_Dot) {
         insts = do_read_with_token(tsc, pt, funq, tok);
         if (insts >= 0 && sts >= 0) {
            pair = tr7_cons(tsc, tsc->read_value, TR7_NIL);
#if USE_TR7_DEBUG && DEBUG_LINES
            line_starts_holder_set(tsc, holder, pair);
#endif
            if (!TR7_IS_PAIR(pair))
               sts = -read_error_oom;
            else {
               *prev = pair;
               prev = &TR7_CDR(pair);
            }
         }
#if USE_TR7_DEBUG && DEBUG_LINES
         else
            line_starts_holder_drop(tsc, holder);
         holder = line_starts_holder_get(tsc, pt);
#endif
         sts = do_read_mix_status(sts, insts);
         insts = do_read_true_token(tsc, pt, funq, &tok);
         sts = do_read_mix_status(sts, insts);
      }
      if (tok == Token_Dot) {
         if (TR7_IS_NIL(head))
            sts = -read_error_dot_at_begin;
         insts = do_read_true_token(tsc, pt, funq, &tok);
         sts = do_read_mix_status(sts, insts);
         if (tok == Token_Right_Par)
            sts = do_read_mix_status(sts, -read_error_dot_at_end);
         else if (tok != Token_EOF) {
            insts = do_read_with_token(tsc, pt, funq, tok);
            *prev = tsc->read_value;
            sts = do_read_mix_status(sts, insts);
            insts = do_read_true_token(tsc, pt, funq, &tok);
            sts = do_read_mix_status(sts, insts);
            while (tok != Token_Right_Par && tok != Token_EOF) {
               sts = do_read_mix_status(sts, -read_error_dot_at_middle);
               do_read_with_token(tsc, pt, funq, tok);
               insts = do_read_true_token(tsc, pt, funq, &tok);
               sts = do_read_mix_status(sts, insts);
            }
         }
      }
      if (tok != Token_Right_Par)
         sts = do_read_mix_status(sts, -read_error_unclosed_parenthesis);
   }
#if USE_TR7_DEBUG && DEBUG_LINES
   line_starts_holder_drop(tsc, holder);
#endif
   tsc->read_value = sts < 0 ? TR7_NIL : head;
   return sts;
}

/*
* read a scheme expression starting with token 'tok'
* returns a negative value on error or 0 if okay or 1 if okay but with an unquoting
*/
static int do_read_with_token(tr7_engine_t tsc, port_t *pt, int funq, token_type_t tok)
{
   int sts;

   switch (tok) {
   case Token_Byte_Vector: /* #u8( */
      sts = do_read_list(tsc, pt, funq);
      if (sts == 0)
         sts = do_read_set(tsc, list_to_bytevector(tsc, tsc->read_value), 0);
      else if (sts > 0) {
         sts = do_read_set_list2(tsc, SYMBOL(QUASIQUOTE), sts);
         sts = do_read_set_list3(tsc, SYMBOL(APPLY), SYMBOL(BYTEVECTOR), sts);
         sts = do_read_set_list2(tsc, SYMBOL(UNQUOTE), sts);
      }
      break;

   case Token_Vector: /* #( */
      sts = do_read_list(tsc, pt, funq);
      if (sts == 0)
         sts = do_read_set(tsc, tr7_list_to_vector(tsc, tsc->read_value), 0);
      else if (sts > 0) {
         sts = do_read_set_list2(tsc, SYMBOL(QUASIQUOTE), sts);
         sts = do_read_set_list3(tsc, SYMBOL(APPLY), SYMBOL(VECTOR), sts);
         sts = do_read_set_list2(tsc, SYMBOL(UNQUOTE), sts);
      }
      break;

   case Token_Right_Par: /* ) */
      sts = -read_error_unopened_parenthesis;
      break;

   case Token_Left_Par: /* ( */
      sts = do_read_list(tsc, pt, funq);
      break;

   case Token_Quote: /* ' */
      sts = do_read(tsc, pt, funq);
      if (sts >= 0)
         sts = do_read_set_list2(tsc, SYMBOL(QUOTE), sts);
      break;

   case Token_Back_Quote: /* ` */
      sts = do_read_true_token(tsc, pt, funq, &tok);
      if (sts >= 0)
         switch(tok) {
         case Token_Byte_Vector: /* `#u8( */
            sts = do_read_list(tsc, pt, funq + 1);
            if (sts == 0)
               sts = do_read_set(tsc, list_to_bytevector(tsc, tsc->read_value), 0);
            else if (sts > 0) {
               sts = do_read_set_list2(tsc, SYMBOL(QUASIQUOTE), sts - 1);
               sts = do_read_set_list3(tsc, SYMBOL(APPLY), SYMBOL(BYTEVECTOR), sts);
            }
            break;
         case Token_Vector: /* `#( */
            sts = do_read_list(tsc, pt, funq + 1);
            if (sts == 0)
               sts = do_read_set(tsc, tr7_list_to_vector(tsc, tsc->read_value), 0);
            else if (sts > 0) {
               sts = do_read_set_list2(tsc, SYMBOL(QUASIQUOTE), sts - 1);
               sts = do_read_set_list3(tsc, SYMBOL(APPLY), SYMBOL(VECTOR), sts);
            }
            break;
         default: /* ` ? */
            sts = do_read_with_token(tsc, pt, funq + 1, tok);
            if (sts >= 0)
               sts = do_read_set_list2(tsc, SYMBOL(QUASIQUOTE), 0);
            break;
         }
      break;

   case Token_Comma: /* , */
      sts = do_read(tsc, pt, funq - !!funq);
      if (sts >= 0)
         sts = do_read_set_list2(tsc, SYMBOL(UNQUOTE), sts + 1);
      break;

   case Token_At: /* @, */
      sts = do_read(tsc, pt, funq - !!funq);
      if (sts >= 0)
         sts = do_read_set_list2(tsc, SYMBOL(UNQUOTE_SPLICING), sts + 1);
      break;

   case Token_Value: /* ? */
      sts = 0;
      break;

   case Token_Comment_Datum: /* #; */
      sts = do_read(tsc, pt, funq);
      sts = do_read_mix_status(sts, do_read(tsc, pt, funq));
      break;

   case Token_Sharp: /* #... */
      sts = do_read_set_list2(tsc, SYMBOL(SHARP), 0);
      break;

   case Token_Datum_Set: /* #N= */
      if (tr7_assq_pair(tsc->read_value, tsc->datums))
         sts = do_read_mix_status(do_read(tsc, pt, funq), -read_error_duplicated_datum);
      else {
         tr7_t pair = tr7_cons(tsc, tsc->read_value, TR7_VOID);
         tsc->datums = tr7_cons(tsc, pair, tsc->datums);
         sts = do_read(tsc, pt, funq);
         if (sts >= 0)
            TR7_CDR(pair) = tsc->read_value;
      }
      break;

   case Token_Datum_Ref: /* #N# */
      tsc->read_value = tr7_assq(tsc->read_value, tsc->datums);
      sts = TR7_IS_NIL(tsc->read_value) ? -read_error_unbound_datum : 0;
      break;

   case Token_EOF:
      sts = -read_error_unexpected_end;
      break;

   default:
   case Token_Error:
      sts = -read_error_illegal_token;
      break;
   }
   return sts;
}

static tr7_t solve_datums(tr7_t item, tr7_t datums)
{
   tr7_vector_t vec;
   tr7_uint_t idx, cnt;

   if (TR7_IS_PAIR(item)) {
      if (tr7_memq_pair(item, datums))
         item = TR7_CDR(item);
      else {
         TR7_CAR(item) = solve_datums(TR7_CAR(item), datums);
         TR7_CDR(item) = solve_datums(TR7_CDR(item), datums);
      }
   }
   else if (TR7_IS_VECTOR(item)) {
      vec = TR7_TO_VECTOR(item);
      cnt = TR7_HEAD_UVALUE(TR7_CELL_HEAD(vec));
      for(idx = 0 ; idx < cnt ; idx++)
         vec->items[idx] = solve_datums(vec->items[idx], datums);
   }
   return item;
}
/*
* Read from the port 'pt', either just one s-expr when 'all'==0
* or a list of all s-expr until end of file when 'all'!=0.
* Solves the internal datum references.
* When no debug is required, set the single value with the read
* expression.
* When debug (USE_TR7_DEBUG!=0) is required, returns 3 values:
* the readen s-expr, the filename (string) and line indications
* Returns 1 (one) if a value is returned without error.
* Returns 0 (zero) if no value is returned (end-of-input) without error.
* Returns a negative value from enum read_status.
*/
static int do_read_with_datum(tr7_engine_t tsc, port_t *pt, int all)
{
   int sts;
   token_type_t tok;
   tr7_t result = TR7_NIL;
#if USE_TR7_DEBUG && DEBUG_LINES
   tr7_t filename = pt->flags & port_file ? pt->rep.stdio.filename : TR7_VOID;
   int line = pt->line;
   tsc->last_line = line;
   tsc->line_starts = TR7_NIL;
#endif
   tsc->datums = TR7_NIL;
   do {
      sts = do_read_true_token(tsc, pt, 0, &tok);
      if (sts >= 0 && tok == Token_EOF) {
         if (all) {
            result = tr7_reverse_in_place(result, TR7_NIL);
            sts = !TR7_IS_NIL(result);
            all = 0;
         }
         else {
            result = TR7_EOF;
            sts = 0;
         }
      }
      else {
         if (sts >= 0)
            sts = do_read_with_token(tsc, pt, 0, tok);
         if (sts >= 0) {
            if (!TR7_IS_NIL(tsc->datums)) {
               tsc->read_value = solve_datums(tsc->read_value, tsc->datums);
               tsc->datums = TR7_NIL;
            }
            if (all)
               result = TR7_CONS2(tsc, tsc->read_value, result);
            else {
               result = tsc->read_value;
               sts = !TR7_IS_NIL(result);
            }
         }
         else {
            char sbuf[STRBUFFSIZE];
            const char *msg;
            switch (-sts) {
            case read_error_oom:                  msg = "out of memory"; break;
            case read_error_unexpected_end:       msg = "unexpected end"; break;
            case read_error_dot_at_begin:         msg = "dot not following any value"; break;
            case read_error_dot_at_middle:        msg = "dot followed by more than one value"; break;
            case read_error_dot_at_end:           msg = "dot not followed by a value"; break;
            case read_error_unclosed_parenthesis: msg = "closing parenthesis is missing"; break;
            case read_error_unopened_parenthesis: msg = "opening parenthesis is missing"; break;
            case read_error_unbound_datum:        msg = "invalid datum reference"; break;
            case read_error_duplicated_datum:     msg = "already set datum"; break;
            case read_error_illegal_token:        msg = "invalid token"; break;
            default:                              msg = "unexpected error"; break;
            }
#if USE_TR7_DEBUG && DEBUG_LINES
            int lino = tsc->last_line;
            if (TR7_IS_VOID(filename))
               snprintf(sbuf, sizeof sbuf, "line %d: read error: %s", lino, msg);
            else {
               const char *fname = (const char*)TR7_CONTENT_STRING(filename);
               snprintf(sbuf, sizeof sbuf, "%s:%d: read error: %s", fname, lino, msg);
            }
#else
            snprintf(sbuf, sizeof sbuf, "read error: %s", msg);
#endif
            memcpy(&sbuf[sizeof sbuf - 4], "...", 4);
            result = make_error_msg_irr(tsc, RECORD_DESC(read_error), sbuf, TR7_NIL, 1);
            all = 0;
         }
      }
   }
   while (all);
   tsc->read_value = result;
#if USE_TR7_DEBUG && DEBUG_LINES
   if (TR7_IS_NIL(tsc->line_starts))
      tsc->line_starts = TR7_FROM_INT(pt->line);
   else
      tsc->line_starts = tr7_reverse_in_place(tsc->line_starts, TR7_NIL);
   tsc->read_file = filename;
   tsc->read_lines = tsc->line_starts;
   tsc->last_line = line;
#endif
   return sts;
}

/*************************************************************************
* SECTION
* ------------
*/
static eval_status_t proc_sharp(tr7_engine_t tsc, int nargs)
{
#if IGNORE_UNKNOWN_SHARP
   return do_pop_continue_void(tsc, 1);
#elif AUTO_SHARP_TO_SYMBOL
   tr7_t expr = DATA(tsc, 0);
   return do_pop_continue_single_alloc(tsc, 1, tr7_get_symbol_length(tsc,
            (const char*)TR7_CONTENT_STRING(expr),
            TR7_SIZE_STRING(expr), 1));
#else
   tr7_t expr = DATA(tsc, 0);
   return raise_error_msg_obj(tsc, "undefined sharp expression", expr);
#endif
}

static eval_status_t read_port(tr7_engine_t tsc, port_t *pt)
{
   int sts = do_read_with_datum(tsc, pt, 0);
   if (sts < 0)
      return raise_error(tsc, tsc->read_value);
   return Cycle_Return;
}

#if USE_SCHEME_READ
static eval_status_t proc_read(tr7_engine_t tsc, int nargs)
{
   port_t *pt = get_optional_inport(tsc, nargs, 0);
   eval_status_t rc = read_port(tsc, pt);
   tr7_t res = tsc->read_value;

#if USE_TR7_DEBUG && DEBUG_LINES
   tsc->read_file = tsc->read_lines =
#endif
   tsc->read_value = TR7_VOID;
   return do_pop_status_single(tsc, nargs, res, rc);
}
#endif

/* implement repl, part read */
static eval_status_t _oper_repl_read(tr7_engine_t tsc)
{
   OPER_POP(tsc, 1);
   /* If interactive, be nice to user. */
   if (tsc->playflags & Tr7_Play_Show_Prompt) {
      if (tsc->strings[Tr7_StrID_Prompt]) {
         tr7_display_string(tsc, tsc->strings[Tr7_StrID_Prompt]);
         tr7_flush(tsc);
      }
   }

   if (tsc->playflags & Tr7_Play_Show_Errors)
      guard_push(tsc, OPER(REPL_GUARD), Guard_Type_Root);

   /* Set up another iteration of REPL */
   oper_push_safe_1(tsc, OPER(REPL_COMPILE));
   return read_port(tsc, TR7__PORT__PORT(tsc->loadport));
}

/* implement repl, part compile */
static eval_status_t _oper_repl_compile(tr7_engine_t tsc)
{
   tr7_t expr = tsc->read_value;
#if USE_TR7_DEBUG && DEBUG_LINES
   tr7_t filename = tsc->read_file;
   tr7_t linetrack = tsc->read_lines;
#endif
   OPER_POP(tsc, 1);
   /* If we reached the end of file, this loop is done. */
   if (TR7_IS_EOF(expr)) {
      load_leave(tsc);
      return Cycle_Leave;
   }
   if (tsc->playflags & Tr7_Play_Show_Eval) {
      tr7_write(tsc, expr);
      tr7_display_string(tsc, "\n");
      tr7_flush(tsc);
   }
   oper_push_safe_1(tsc, OPER(REPL_EVAL));
#if USE_TR7_DEBUG && DEBUG_LINES
   return do_compile(tsc, expr, filename, linetrack);
#else
   return do_compile(tsc, expr);
#endif
}

/* implement repl, part eval */
static eval_status_t _oper_repl_eval(tr7_engine_t tsc)
{
   tr7_t x = tsc->values[0];

   OPER_POP(tsc, 1);
   /* If we reached the end of file, this loop is done. */
   if (TR7_IS_EOF(x))
      return Cycle_Return;
   if (tsc->playflags & Tr7_Play_Show_Compile) {
      tr7_write(tsc, x);
      tr7_display_string(tsc, "\n");
      tr7_flush(tsc);
   }
   oper_push_safe_1(tsc, OPER(REPL_READ));
   /* OPERID(REPL_PRINT) is always pushed, because when changing from
      non-interactive to interactive mode, it needs to be
      already on the stack */
   oper_push_safe_1(tsc, OPER(REPL_PRINT));
   return s_prog(tsc, x);
}

/* implement repl, part print */
static eval_status_t _oper_repl_print(tr7_engine_t tsc)
{
   OPER_POP(tsc, 1);
   if (!TR7_IS_VOID(tsc->values[0])) {
      if (tsc->tracing || (tsc->playflags & Tr7_Play_Show_Result)) {
         unsigned i;
         if (tsc->tracing)
            log_str(tsc, "\nGives:\n");
         for(i = 0 ; i  < tsc->nvalues ; i++) {
            tr7_write_shared(tsc, tsc->values[i]);
            tr7_display_string(tsc, "\n");
         }
         tr7_flush(tsc);
      }
   }
   return Cycle_Return;
}

/* implement repl, part guard */
static eval_status_t _oper_repl_guard(tr7_engine_t tsc)
{
   tr7_t x, i;
   int n;

   /* no guard found, print the error */
   x = OPER_AT(tsc, 1);
   OPER_POP(tsc, 2);
   if (tr7_is_error(x)) {
      log_str(tsc, "Error: ");
      log_item_string(tsc, tr7_error_message(x));
      i = tr7_error_irritants(x);
      if (TR7_IS_PAIR(i)) {
         if (TR7_IS_NIL(TR7_CDR(i))) {
            log_str(tsc, ": ");
            log_item(tsc, TR7_CAR(i));
         }
         else {
            for (n = 1; TR7_IS_PAIR(i) ; n++, i = TR7_CDR(i)) {
               log_str(tsc, "\n   irritant ");
               log_item(tsc, TR7_FROM_INT(n));
               log_str(tsc, ": ");
               log_item_string(tsc, TR7_CAR(i));
            }
         }
      }
#if USE_TR7_DEBUG
      i = tr7_error_stack(x);
      for (n = 0; TR7_IS_PAIR(i) ; n++, i = TR7_CDR(i)) {
         tr7_t s = TR7_CAR(i);
         log_str(tsc, "\n   #");
         log_item(tsc, TR7_FROM_INT(n));
         if (!TR7_IS_VOID(TR7_CAR(s))) {
            log_str(tsc, " in ");
            log_item_string(tsc, TR7_CAR(s));
         }
#if DEBUG_LINES
         log_str(tsc, " at ");
         log_item_string(tsc, TR7_CADDR(s));
         log_str(tsc, ":");
         log_item(tsc, TR7_CAR(TR7_CDDDR(s)));
#endif
         log_str(tsc, " args ");
         log_item(tsc, TR7_CADR(s));
      }
#endif
   }
   else {
      log_str(tsc, "Exception: ");
      log_item(tsc, x);
   }
   log_str(tsc, "\n");

   /* should continue? */
   if (!(tsc->playflags & Tr7_Play_Keep_Playing))
      return Cycle_Leave_Error; /* no stop with error */

   /* yes continue */
   oper_push_safe_1(tsc, OPER(REPL_READ));
   return Cycle_Goto;
}
/*
*************************************************************************
* LIBRARY_TR7_MISC
* ----------------
*/
#if USE_TR7_MISC
/*
* procedure 'tr7-id'
*/
static eval_status_t proc_tr7_id(tr7_engine_t tsc, int nargs)
{
   return do_pop_continue_single(tsc, 0, tr7_make_string_static(tsc, tr7_get_id()));
}
/*
* procedure 'tr7-version'
*/
static eval_status_t proc_tr7_version(tr7_engine_t tsc, int nargs)
{
   return do_pop_continue_single(tsc, 0, tr7_make_string_static(tsc, tr7_get_version()));
}
/*
* auxilary procedure for making a list from a path
*/
static tr7_t path_to_list(tr7_engine_t tsc, const char *paths)
{
   size_t len;
   tr7_t nxt, cur;

   if (paths == NULL)
      return TR7_NIL;
   for(; *paths == PATH_SEP_CHAR ; paths++);
   for(len = 0 ; paths[len] != 0 && paths[len] != PATH_SEP_CHAR ; len++);
   if (len == 0)
      return TR7_NIL;
   cur = tr7_make_string_copy_length(tsc, paths, len);
   nxt = path_to_list(tsc, &paths[len]);
   return tr7_cons(tsc, cur, nxt);
}
/*
* procedure 'scheme-paths'
*/
static eval_status_t proc_scheme_paths(tr7_engine_t tsc, int nargs)
{
   const char *paths = tsc->strings[Tr7_StrID_Library_Path];
   tr7_t list = path_to_list(tsc, paths);
   return do_pop_continue_single(tsc, 0, list);
}
#endif
/*
*************************************************************************
* LIBRARY_TR7_TRACE
* --------------
*/
#if USE_TR7_TRACE
/*
* procedure 'tr7-tracing'
*/
static eval_status_t proc_tracing(tr7_engine_t tsc, int nargs)
{
   tr7_t x = TR7_FROM_INT(tsc->tracing);
   unsigned v = (unsigned)TR7_TO_INT(DATA(tsc, 0));
   tsc->tracing = v < 32 ? v : 31;
   return do_pop_continue_single(tsc, nargs, x);
}
/* implement show */
static eval_status_t proc_play_option(tr7_engine_t tsc, int nargs, unsigned flag)
{
   int was = (tsc->playflags & flag) != 0;
   if (nargs) {
      if (TR7_IS_FALSE(DATA(tsc, 0)))
         tsc->playflags &= ~flag;
      else
         tsc->playflags |= flag;
   }
   return do_pop_continue_boolean(tsc, nargs, was);
}

/* procedure 'tr7-show-prompt */
static eval_status_t proc_show_prompt(tr7_engine_t tsc, int nargs)
{
   return proc_play_option(tsc, nargs, Tr7_Play_Show_Prompt);
}

/* procedure 'tr7-show-eval */
static eval_status_t proc_show_eval(tr7_engine_t tsc, int nargs)
{
   return proc_play_option(tsc, nargs, Tr7_Play_Show_Eval);
}

/* procedure 'tr7-show-compile */
static eval_status_t proc_show_compile(tr7_engine_t tsc, int nargs)
{
   return proc_play_option(tsc, nargs, Tr7_Play_Show_Compile);
}

/* procedure 'tr7-show-result */
static eval_status_t proc_show_result(tr7_engine_t tsc, int nargs)
{
   return proc_play_option(tsc, nargs, Tr7_Play_Show_Result);
}

/* procedure 'tr7-keep-playing */
static eval_status_t proc_keep_playing(tr7_engine_t tsc, int nargs)
{
   return proc_play_option(tsc, nargs, Tr7_Play_Keep_Playing);
}
#endif
/*
*************************************************************************
* LIBRARY_TR7_GC
* --------------
*/
#if USE_TR7_GC
/*
* procedure 'tr7-gc'
*/
static eval_status_t proc_gc(tr7_engine_t tsc, int nargs)
{
   unsigned was = tsc->gc_verbose;
   if (nargs)
      tsc->gc_verbose = !TR7_IS_FALSE(DATA(tsc, 0));
   collect_garbage(tsc);
   tsc->gc_verbose = was;
   return do_pop_continue_void(tsc, nargs);
}
/*
* procedure 'tr7-gc-verbose'
*/
static eval_status_t proc_gc_verbose(tr7_engine_t tsc, int nargs)
{
   int was = tsc->gc_verbose;
   if (nargs)
      tsc->gc_verbose = !TR7_IS_FALSE(DATA(tsc, 0));
   return do_pop_continue_boolean(tsc, nargs, was);
}
/*
* procedure 'new-segment'
*/
static eval_status_t proc_new_segment(tr7_engine_t tsc, int nargs)
{
   unsigned nr = nargs ? (unsigned)tr7_to_int(DATA(tsc, 0)) : 1;
   memseg_multi_alloc(tsc, nr, ITEM_SEGSIZE);
   return do_pop_continue_void(tsc, nargs);
}
#endif
/*
*************************************************************************
* LIBRARY_TR7_ENVIRONMENT
* -----------------------
*/
#if USE_TR7_ENVIRONMENT
/*
* import symbol value to environment
*/
static int env2list_cb(tr7_engine_t tsc, tr7_t sym, tr7_t val, void *closure)
{
   tr7_t *pl = (tr7_t*)closure;
   *pl = tr7_cons(tsc, sym, *pl);
   return 0;
}
/* implement 'tr7-environment->list' */
static eval_status_t proc_environment_list(tr7_engine_t tsc, int nargs)
{
   tr7_t env = nargs > 0 ? DATA(tsc, 0) : tsc->curenv;
   tr7_int_t d = nargs > 1 ? TR7_TO_INT(DATA(tsc, 1)) : 0;
   int depth = 0 < d && d <= INT_MAX ? (int)d : INT_MAX;
   tr7_t list = TR7_NIL;
   environment_enumerate_depth(tsc, env, env2list_cb, &list, depth);
   return do_pop_continue_single(tsc, nargs, list);
}
/* procedure 'environment?' */
static eval_status_t proc_is_environment(tr7_engine_t tsc, int nargs)
{
   return do_pop_continue_boolean(tsc, 1, TR7_IS_ENVIRONMENT(DATA(tsc, 0)));
}

/* procedure 'defined?' */
static eval_status_t proc_is_defined(tr7_engine_t tsc, int nargs)
{
   tr7_t env = nargs > 1 ? DATA(tsc, 1) : tsc->curenv;
   tr7_pair_t envit = environment_find_item(env, DATA(tsc, 0));
   return do_pop_continue_boolean(tsc, nargs, NULL != envit);
}

/* procedure 'symbols-set' */
static eval_status_t proc_symbols_set(tr7_engine_t tsc, int nargs)
{
   return do_pop_continue_single(tsc, 0, symbols_set_all_symbols(tsc));
}

/* procedure 'current-environment' */
static eval_status_t proc_current_environment(tr7_engine_t tsc, int nargs)
{
   return do_pop_continue_single(tsc, 0, tsc->curenv);
}

#endif
/*
*************************************************************************
* LIBRARY_TR7_DEBUG
* -----------------------
*/
#if USE_TR7_DEBUG

/*
* procedure 'tr7-call-stack'
*/
static eval_status_t proc_call_stack(tr7_engine_t tsc, int nargs)
{
   tr7_t cs = call_stack(tsc);
   return do_pop_continue_single(tsc, 0, cs);
}
/*
* procedure 'tr7-exec-stack'
*/
static eval_status_t proc_exec_stack(tr7_engine_t tsc, int nargs)
{
   tr7_t es = tr7_cons_n(tsc, (int)(tsc->stack.tail - tsc->stack.data), tsc->stack.data, TR7_NIL);
   return do_pop_continue_single(tsc, 0, es);
}
/*
* procedure 'error-object-stack'
*/
static eval_status_t proc_error_stack(tr7_engine_t tsc, int nargs)
{
   tr7_t st = error_item(DATA(tsc, 0), Error_Idx_Stack);
   return do_pop_continue_single(tsc, 1, st);
}
/*
* procedure 'compile'
*/
static eval_status_t proc_compile(tr7_engine_t tsc, int nargs)
{
#if USE_TR7_DEBUG && DEBUG_LINES
   int rc = main_compile(tsc, DATA(tsc, 0), TR7_VOID, TR7_NIL);
#else
   int rc = main_compile(tsc, DATA(tsc, 0));
#endif
   return do_pop_status(tsc, 1 /*nargs==1*/, rc >= 0 ? Cycle_Continue : Cycle_Raise);
}
/*
* procedure 'disass'
*/
static eval_status_t proc_disass(tr7_engine_t tsc, int nargs)
{
   tr7_t item = DATA(tsc, 0);
   port_t *pt = get_optional_outport(tsc, nargs, 1);
   disassemble_any(tsc, item, pt);
   return do_pop_continue_void(tsc, nargs);
}
#endif
/*
**************************************************************************
* SECTION CHECK_ARGUMENTS
* -----------------------
*
*/
#define CHECK_ARG_OK          0
#define CHECK_ARG_BAD_COUNT   1
#define CHECK_ARG_AT_LEAST    2
#define CHECK_ARG_AT_MOST     3
#define CHECK_ARG_AT_ARG      4
#define CHECK_ARG_TYPE_MASK   7
#define CHECK_ARG_SHIFT       3
#define CHECK_ARG_MAKE(arg,typ) (((arg)<<CHECK_ARG_SHIFT)|(typ))
#define CHECK_ARG_TYPE(cksts)   ((cksts) & CHECK_ARG_TYPE_MASK)
#define CHECK_ARG_VALUE(cksts)  ((cksts) >> CHECK_ARG_SHIFT)

static eval_status_t raise_check_args_error(tr7_engine_t tsc, int cksts, int nargs, tr7_t symbol, const char *tststr)
{
   static const char *precisions[3] = { "", " at least", " at most" };
   static const char *descriptions[] = {
      "string",
      "symbol",
      "port",
      "input port",
      "output port",
      "environment",
      "pair",
      "pair or '()",
      "character",
      "vector",
      "number",
      "integer",
      "non-negative integer",
      "bytevector",
      "proc",
      "error-object",
      "byte",
      "record",
      "record type descriptor",
      "proper list",
      "box",
      "textual input port",
      "textual output port",
      "binary input port",
      "binary output port"
   };
   char msg[STRBUFFSIZE];
   int nxttst, curtst, off;
   int i, typ = CHECK_ARG_TYPE(cksts), val = CHECK_ARG_VALUE(cksts);
   unsigned offset = 0, remain;

   if (TR7_IS_SYMBOL(symbol)) {
      off = snprintf(msg, sizeof msg, "when calling %s, ", TR7_CONTENT_SYMBOL(symbol));
      offset = off < 0 ? 0 : (unsigned)off;
   }
   remain = sizeof msg - offset;

   switch (typ) {
   case CHECK_ARG_BAD_COUNT:
   case CHECK_ARG_AT_LEAST:
   case CHECK_ARG_AT_MOST:
      snprintf(&msg[offset], remain,
                  "needs%s %d argument(s)",
                  precisions[typ - CHECK_ARG_BAD_COUNT], val);
      break;
   case CHECK_ARG_AT_ARG:
      curtst = *tststr;
      if (curtst) {
         nxttst = *++tststr;
         for(i = 0 ; nxttst && i < val ; i++, nxttst = *++tststr)
            curtst = nxttst;
         curtst -= _TR7ARGNUM_ANY_ + 1;
         if (curtst >= 0
          && curtst < (int)(sizeof descriptions / sizeof *descriptions)) {
            snprintf(&msg[offset], remain,
                           "argument %d must be %s",
                           val + 1, descriptions[curtst]);
            break;
         }
      }
      /*@fallthrough@*/
   default:
      snprintf(&msg[offset], remain, "unexpected situation");
      break;
   }

   return raise_error_msg_obj_desc(tsc, msg, TR7_LIST_N(tsc, nargs, tsc->stack.data), RECORD_DESC(error), 1);
}
static int check_args(tr7_engine_t tsc, int nargs, int min_args, int max_args, const char *tststr)
{
   tr7_t arg;
   char nxttst, curtst;
   int i = 0;

   if (nargs < min_args)
      return CHECK_ARG_MAKE(min_args, min_args == max_args ? CHECK_ARG_BAD_COUNT : CHECK_ARG_AT_LEAST);

   if (nargs > max_args && max_args >= 0)
      return CHECK_ARG_MAKE(max_args, min_args == max_args ? CHECK_ARG_BAD_COUNT : CHECK_ARG_AT_MOST);

   /* test arguments */
   if (tststr == NULL || (curtst = *tststr) == 0)
      return CHECK_ARG_OK;

   nxttst = *++tststr;
   /* count of scanned arguments */
   for( ; i < nargs ; i++) {
      /* check the type of the argument */
      arg = DATA(tsc, i);
      switch (curtst) {
      case _TR7ARGNUM_STRING_: if (!(TR7_IS_STRING(arg))) goto badarg; break;
      case _TR7ARGNUM_SYMBOL_: if (!(TR7_IS_SYMBOL(arg))) goto badarg; break;
      case _TR7ARGNUM_PORT_: if (!(TR7_IS_PORT(arg))) goto badarg; break;
      case _TR7ARGNUM_INPORT_: if (!(tr7_is_input_port(arg))) goto badarg; break;
      case _TR7ARGNUM_OUTPORT_: if (!(tr7_is_output_port(arg))) goto badarg; break;
      case _TR7ARGNUM_ENVIRONMENT_: if (!(TR7_IS_ENVIRONMENT(arg))) goto badarg; break;
      case _TR7ARGNUM_PAIR_: if (!(TR7_IS_PAIR(arg))) goto badarg; break;
      case _TR7ARGNUM_ANY_LIST_: if (!(TR7_IS_PAIR(arg) || TR7_IS_NIL(arg))) goto badarg; break;
      case _TR7ARGNUM_CHAR_: if (!(TR7_IS_CHAR(arg))) goto badarg; break;
      case _TR7ARGNUM_VECTOR_: if (!(TR7_IS_VECTOR(arg))) goto badarg; break;
      case _TR7ARGNUM_NUMBER_: if (!(tr7_is_number(arg))) goto badarg; break;
      case _TR7ARGNUM_INTEGER_: if (!(tr7_is_integer(arg))) goto badarg; break;
      case _TR7ARGNUM_NATURAL_: if (!(TR7_IS_INT(arg) && TR7_TO_INT(arg) >= 0)) goto badarg; break;
      case _TR7ARGNUM_BYTEVEC_: if (!(TR7_IS_BYTEVECTOR(arg))) goto badarg; break;
      case _TR7ARGNUM_PROC_: if (!(tr7_is_procedure(arg))) goto badarg; break;
      case _TR7ARGNUM_ERROBJ_: if (!tr7_is_error(arg)) goto badarg; break;
      case _TR7ARGNUM_BYTE_: if (!(TR7_IS_INT(arg) && TR7_TO_INT(arg) >= 0 && TR7_TO_INT(arg) <= 255)) goto badarg; break;
      case _TR7ARGNUM_RECORD_: if (!tr7_is_record(arg)) goto badarg; break;
      case _TR7ARGNUM_RECORD_DESC_: if (!tr7_is_record_desc(arg)) goto badarg; break;
      case _TR7ARGNUM_PROPER_LIST_: if (!(tr7_list_length(arg) >= 0)) goto badarg; break;
      case _TR7ARGNUM_BOX_: if (!(IS_BOX(arg))) goto badarg; break;
      case _TR7ARGNUM_TXT_INPORT_: if (!(tr7_is_textual_input_port(arg))) goto badarg; break;
      case _TR7ARGNUM_TXT_OUTPORT_: if (!(tr7_is_textual_output_port(arg))) goto badarg; break;
      case _TR7ARGNUM_BIN_INPORT_: if (!(tr7_is_binary_input_port(arg))) goto badarg; break;
      case _TR7ARGNUM_BIN_OUTPORT_: if (!(tr7_is_binary_output_port(arg))) goto badarg; break;
      default: break;
      }
      if (nxttst) {
         curtst = nxttst;
         nxttst = *++tststr;
      }
   }
   return CHECK_ARG_OK;
badarg:
   return CHECK_ARG_MAKE(i, CHECK_ARG_AT_ARG);
}
/*
**************************************************************************
* SECTION EXECUTE
* ---------------
*
*/
#if USE_TR7_TRACE
static void trace_invoke_proc(tr7_engine_t tsc, tr7_t proc, int nargs)
{
   int idx;
   log_str(tsc, "\nApply/");
   log_item(tsc, TR7_FROM_INT(nargs));
   log_str(tsc, " ");
   log_item(tsc, proc);
   log_str(tsc, " to:\n");
   for (idx = 0 ; idx < nargs ; idx++) {
      log_str(tsc, "   ");
      log_item(tsc, TR7_FROM_INT(idx));
      log_str(tsc, ": ");
      log_item(tsc, DATA(tsc, idx));
      log_str(tsc, "\n");
   }
}
static void trace_return_proc(tr7_engine_t tsc)
{
   unsigned idx;
   log_str(tsc, "\nReturns:\n");
   for (idx = 0 ; idx < tsc->nvalues ; idx++) {
      log_str(tsc, "  - ");
      log_item(tsc, tsc->values[idx]);
      log_str(tsc, "\n");
   }
   log_str(tsc, "\n");
}
#endif
/*
*/
static eval_status_t run_proc(tr7_engine_t tsc, const proc_desc_t *pcd, int nargs)
{
   eval_status_t sts = pcd->proc(tsc, nargs);
#if USE_TR7_TRACE
   if (tsc->tracing && sts == Cycle_Continue)
      trace_return_proc(tsc);
#endif
   return sts;
}
#if HAS_CHECK_TYPES_NO
/*
*/
static eval_status_t execute_proc_unsafe(tr7_engine_t tsc, procid_t proc, int nargs)
{
   const proc_desc_t *pcd;
#if USE_TR7_TRACE
   if (tsc->tracing)
      trace_invoke_proc(tsc, FROM_PROC(proc), nargs);
#endif
   pcd = &procedures[proc];
   return run_proc(tsc, pcd, nargs);
}
#endif
/*
*/
static eval_status_t execute_proc(tr7_engine_t tsc, procid_t proc, int nargs)
{
   const proc_desc_t *pcd;
   int cksts;
#if USE_TR7_TRACE
   if (tsc->tracing)
      trace_invoke_proc(tsc, FROM_PROC(proc), nargs);
#endif
   pcd = &procedures[proc];
   cksts = check_args(tsc, nargs, pcd->min_arity, pcd->max_arity, pcd->argtypes);
   if (cksts != CHECK_ARG_OK)
      return raise_check_args_error(tsc, cksts, nargs, SYMBOL_AT(pcd->symbolid), pcd->argtypes);
   return run_proc(tsc, pcd, nargs);
}
/*
*/
static eval_status_t prepare_frame(tr7_engine_t tsc, tr7_vector_t vfra, tr7_t link, int nargs, int nparams, int nlocals)
{
   tr7_t x;

   /* set link */
   TR7_VECTOR_ITEM(vfra, Frame_Idx_Link) = link;

   /* get argument count and dotted status */
   if (nparams >= 0) {
      /* not dotted */
      if (nargs > nparams)
         return raise_error_msg_obj(tsc, "too much arguments", TR7_FROM_INT(nargs));
      if (nargs < nparams)
         return raise_error_msg_obj(tsc, "not enough arguments", TR7_FROM_INT(nargs));
      /* copy the arguments */
      memcpy(&TR7_VECTOR_ITEM(vfra, Frame_Idx_Arg0), &DATA(tsc, 0), (unsigned)nparams * sizeof(tr7_t));
   }
   else {
      /* dotted */
      nparams = -(1 + nparams);
      if (nargs < nparams)
         return raise_error_msg_obj(tsc, "not enough arguments", TR7_FROM_INT(nargs));
      /* copy the arguments */
      memcpy(&TR7_VECTOR_ITEM(vfra, Frame_Idx_Arg0), &DATA(tsc, 0), (unsigned)nparams * sizeof(tr7_t));
      if (nargs == nparams)
         x = TR7_NIL;
      else {
         x = tr7_cons_n(tsc, nargs - nparams, &DATA(tsc, nparams), TR7_NIL);
         if (TR7_IS_NIL(x))
            return raise_out_of_memory_error(tsc);
      }
      TR7_VECTOR_ITEM(vfra, Frame_Idx_Arg0 + nparams++) = x;
   }

   /* copy the arguments */
   while (nlocals > nparams)
      TR7_VECTOR_ITEM(vfra, Frame_Idx_Arg0 + nparams++) = TR7_VOID;
   DATA_POP_N(tsc, nargs);

   /* continue */
   return Cycle_Continue;
}
/*
*/
static tr7_t pc0(tr7_vector_t vprog)
{
   tr7_uint_t offset = TR7_TO_UINT(TR7_VECTOR_ITEM(vprog, Program_Idx_Code));
   uint16_t *base = (uint16_t*)&TR7_VECTOR_ITEM(vprog, offset);
   tr7_uint_t pc = (tr7_uint_t)(base - (uint16_t*)vprog);
   return TR7_FROM_UINT(pc);
}
/*
*/
static eval_status_t execute_program(tr7_engine_t tsc, tr7_t link, int nargs, tr7_t prog)
{
   eval_status_t es;
   int nparams, nlocals;
   tr7_t frame;
   tr7_vector_t vfra, vprog;

   /* the program as a vector */
   vprog = TR7_TO_VECTOR(prog);
   nlocals = (int)TR7_TO_INT(TR7_VECTOR_ITEM(vprog, Program_Idx_nLocals));
   nparams = (int)TR7_TO_INT(TR7_VECTOR_ITEM(vprog, Program_Idx_nParams));

   /* allocate the frame */
   frame = alloc_vector(tsc, Frame_Idx_Arg0 + (unsigned)nlocals);
   if (TR7_IS_NIL(frame))
      return raise_out_of_memory_error(tsc);
   vfra = TR7_TO_VECTOR(frame);

   /* prepare the frame */
   es = prepare_frame(tsc, vfra, link, nargs, nparams, nlocals);
   if (es != Cycle_Continue)
      return es;

   /* run now */
   oper_push_safe_4(tsc, OPER(XRUN), prog, pc0(vprog), frame);
   return Cycle_Goto;
}
/*
*/
static eval_status_t execute_ff(tr7_engine_t tsc, const tr7_C_func_def_t *ff, int nargs)
{
   tr7_C_return_t r;
   tr7_t s;
   int ilarg, cntargs = nargs, cksts = check_args(tsc, nargs, ff->min_args, ff->max_args, ff->typargs);
   if (cksts != CHECK_ARG_OK) {
      s = ff->name == NULL ? TR7_FALSE : tr7_get_symbol(tsc, ff->name, 0);
      return raise_check_args_error(tsc, cksts, nargs, s, ff->typargs);
   }
   if (nargs && ff->max_args < 0) {
         ilarg = -1 - ff->max_args;
         if (nargs >= ilarg) {
            DATA(tsc, ilarg) = TR7_LIST_N(tsc, nargs - ilarg, &DATA(tsc, ilarg));
            cntargs = ilarg + 1;
         }
   }
   drop_values(tsc); /* not returning something by default */
   r = ff->func(tsc, cntargs, &DATA(tsc, 0), ff->closure);
   return do_pop_status(tsc, nargs, r ? Cycle_Continue : Cycle_Raise);
}
/*
*/
static eval_status_t execute_record_function(tr7_engine_t tsc, tr7_recfun_t recfun, int nargs)
{
   tr7_record_t rec;
   tr7_uint_t opterm = TR7_TO_UINT(recfun->opterm);
   tr7_t val, recdesc = recfun->recdesc;
   unsigned idx, count;
   int iarg;

   switch (opterm & _RecFun_Op_Mask_) {

   case RecFun_Op_Create:
      if (nargs != (int)(opterm >> _RecFun_Op_Shift_))
         return raise_error_msg(tsc, "wrong argument count");
      /* allocate */
      rec = mk_record(tsc, recdesc);
      /* init the record */
      count = (unsigned)(TR7_HEAD_UVALUE(recfun->head) - NCELL_OF_PTR(recfun) + 1);
      for(idx = 0 ; idx < count ; idx++) {
         iarg = (int)TR7_TO_INT(recfun->args[idx]);
         val = iarg >= 0 ? DATA(tsc, iarg) : TR7_VOID;
         TR7_RECORD_ITEM(rec, idx + Record_Idx_First) = val;
      }
      return do_pop_continue_single(tsc, nargs, push_recent_cell(tsc, rec));

   case RecFun_Op_Test:
         if (nargs != 1)
            return raise_error_msg(tsc, "one argument only expected");
         val = DATA(tsc, 0);
         return do_pop_continue_boolean(tsc, 1, tr7_is_record_type(val, recdesc));

   case RecFun_Op_Get:
         if (nargs != 1)
            return raise_error_msg(tsc, "one argument only expected");
         val = DATA(tsc, 0);
         rec = tr7_as_record_cond(val, recdesc);
         if (rec == NULL)
            return raise_error_msg_obj(tsc, "bad type", val);
         val = rec->items[opterm >> _RecFun_Op_Shift_];
         return do_pop_continue_single(tsc, 1, val);

   case RecFun_Op_Set:
         if (nargs != 2)
            return raise_error_msg(tsc, "two arguments only expected");
         val = DATA(tsc, 0);
         rec = tr7_as_record_cond(val, recdesc);
         if (rec == NULL)
            return raise_error_msg_obj(tsc, "bad type", val);
         rec->items[opterm >> _RecFun_Op_Shift_] = DATA(tsc, 1);
         return do_pop_continue_void(tsc, 2);
   }
   return raise_error_msg_obj(tsc, "invalid callable", TR7_FROM_VECTOR(recfun));
}
/*
*/
static eval_status_t execute_call(tr7_engine_t tsc, tr7_t oper, int nargs)
{
   int idx;
   tr7_t list, desc, value, converter;
   tr7_cell_t cell;
   tr7_closure_t closure;

   /* PROCEDURE ? */
   if (IS_PROC(oper))
      return execute_proc(tsc, TO_PROC(oper), nargs);

#if USE_TR7_TRACE
   if (tsc->tracing)
      trace_invoke_proc(tsc, oper, nargs);
#endif

   if (TR7_IS_CELL(oper)) {
      cell = TR7_TO_CELL(oper);
      switch (TR7_CELL_KIND(cell)) {

      case Tr7_Head_Kind_Lambda:
         closure = (tr7_closure_t)cell;
         return execute_program(tsc, closure->upperframes, nargs, closure->description);

#if USE_SCHEME_CASE_LAMBDA
      case Tr7_Head_Kind_Case_Lambda:
         closure = (tr7_closure_t)cell;
         list = closure->description;
         while(TR7_IS_PAIR(list)) {
            desc = TR7_CAR(list);
            idx = (int)TR7_TO_INT(TR7_ITEM_VECTOR(desc, Program_Idx_nParams));
            if (idx >= 0 ? idx == nargs : -(1 + idx) <= nargs)
               return execute_program(tsc, closure->upperframes, nargs, desc);
            list = TR7_CDR(list);
         }
         return raise_error_msg(tsc, "unbound case-lambda");
#endif

      /* FOREIGN FUNCTION */
      case Tr7_Head_Kind_CFunction:
         return execute_ff(tsc, TR7_TO_CFUNC(oper)->definition, nargs);

      /* CONTINUATION */
      case Tr7_Head_Kind_Continuation:
         return call_continuation(tsc, (tr7_continuation_t)cell, nargs);

      /* PARAMETER */
      case Tr7_Head_Kind_Parameter:
         if (nargs == 0)
            return return_single(tsc, parameter_get(tsc, oper), Cycle_Continue);
         if (nargs != 1)
            return raise_error_msg(tsc, "too much parameter argument");
         converter = TR7_TO_PARAMETER(oper)->converter;
         value = DATA_POP(tsc);
         if (TR7_IS_NIL(converter)) {
            parameter_set(tsc, oper, value);
            return return_single(tsc, value, Cycle_Continue);
         }
         oper_push_safe_2(tsc, OPER(PARAMCVT), oper);
         return s_exec_1(tsc, converter, value);

      /* RECORD */
      case Tr7_Head_Kind_RecFun:
         return execute_record_function(tsc, TR7_CELL_TO_RECFUN(cell), nargs);

      /* ELSE */
      default:
         break;
      }
   }

   return raise_error_msg_obj(tsc, "invalid callable", oper);
}

/*
* implement OPERID(XRUN)
* this operator restore from arg0
* and then ensure the code arg1 is executed
*/
static eval_status_t _oper_xrun(tr7_engine_t tsc)
{
   tr7_t val, val2, *pvals;
   eval_status_t sts;
   tr7_vector_t subclos, vprog, frame;
   uint16_t *pcode;

   vprog = TR7_TO_VECTOR(OPER_AT(tsc, XRUN_Idx_Program));
   pcode = &(((uint16_t*)vprog)[TR7_TO_UINT(OPER_AT(tsc, XRUN_Idx_PC))]);
   frame = TR7_AS_VECTOR(OPER_AT(tsc, XRUN_Idx_Frame));

   for(;;) {
      uint16_t a1, a2, op;

#if USE_TR7_TRACE
      if (tsc->tracing) {
         port_t *pt = TR7__PORT__PORT(get_stdport(tsc, IDX_STDERR));
         tr7_t *holders = &TR7_VECTOR_ITEM(vprog, 0);
         uint16_t *base = (uint16_t*)&TR7_VECTOR_ITEM(vprog, TR7_TO_UINT(TR7_VECTOR_ITEM(vprog, Program_Idx_Code)));
#if USE_TR7_DEBUG && DEBUG_LINES
         unsigned poslines = (unsigned)TR7_TO_UINT(TR7_VECTOR_ITEM(vprog, Program_Idx_Lines));
         const uint8_t *lines = (uint8_t*)&TR7_VECTOR_ITEM(vprog, poslines);
         unsigned szlines = (unsigned)((TR7_VECTOR_LENGTH(vprog) - poslines) * sizeof(tr7_t));
#endif
#if EXTRA_TRACING
         log_str(tsc, "\n************\n");
         oper_stack_dump(tsc, tsc->tracing - 1);
         log_str(tsc, "   ---\n");
         data_stack_dump(tsc, tsc->tracing - 1);
         for (int idx = 0 ; idx < (int)tsc->nvalues ; idx++) {
            log_str(tsc, "   *= ");
            log_item(tsc, tsc->values[idx]);
            log_str(tsc, "\n");
         }
#endif
         port_write_utf8(tsc, pt, "execute: ");
         disassemble_instruction(tsc, pt, base, pcode - base, holders, TR7_NIL
#if USE_TR7_DEBUG && DEBUG_LINES
                                 , lines, szlines
#endif
                     );
         port_write_utf8(tsc, pt, "\n");
#if EXTRA_TRACING && DUMP_CLOSURES
         log_str(tsc, "   FRAME ");
         log_item(tsc, OPER_AT(tsc, XRUN_Idx_Frame));
         log_str(tsc, "\n");
#endif
      }
#endif

      op = *pcode++;
      switch(op) {

      case INSTRID(END):

         OPER_POP(tsc, _XRUN_Idx_Count_);
         return Cycle_Return;

      case INSTRID(GOTO):

         pcode += (int16_t)*pcode;
         break;

      case INSTRID(IFTRUE):

         pcode += TR7_IS_FALSE(tsc->values[0]) ? 1 : (int16_t)*pcode;
         break;

      case INSTRID(IFFALSE):

         pcode += TR7_IS_FALSE(tsc->values[0]) ? (int16_t)*pcode : 1;
         break;

      case INSTRID(ARG):

         if (tsc->nvalues == 0)
            return raise_error_msg(tsc, "undefined value returned");
         data_stack_push_values_first(tsc);
         break;

      case INSTRID(CALLSELF):

         if (tsc->no_stack)
            return raise_error_msg(tsc, "stack overflow");

         a1 = *pcode++;
         val = TR7_VECTOR_ITEM(frame, Frame_Idx_Link);
         val2 = OPER_AT(tsc, XRUN_Idx_Program);

         if (*pcode == INSTRID(END)) {
            OPER_POP(tsc, _XRUN_Idx_Count_);
#if USE_TR7_TRACE
            if (tsc->tracing)
               trace_invoke_proc(tsc, val2, a1);
#endif
            return execute_program(tsc, val, a1, val2);
         }

         OPER_AT(tsc, XRUN_Idx_PC) = TR7_FROM_UINT(pcode - (uint16_t*)vprog);
#if USE_TR7_TRACE
         if (tsc->tracing)
            trace_invoke_proc(tsc, val2, a1);
#endif
         sts = execute_program(tsc, val, a1, val2);
         return sts; /* TODO dont return */
         break;

      case INSTRID(CALLG):

         a1 = *pcode++;
         a2 = *pcode++;
         val = TR7_VECTOR_ITEM(vprog, a2);
         if (IS_BOX(val))
            val = GET_BOX(val);
         if (TR7_IS_VOID(val))
#if USE_TR7_DEBUG
            return raise_error_msg_obj(tsc, "unbound variable", ITEM_BOX(TR7_VECTOR_ITEM(vprog, a2), 1));
#else
            return raise_error_msg(tsc, "unbound variable");
#endif
         goto call;

      case INSTRID(CALL):

         a1 = *pcode++;
         val = tsc->values[0];

call:
         if (tsc->no_stack)
            return raise_error_msg(tsc, "stack overflow");

         if (*pcode == INSTRID(END)) {
            OPER_POP(tsc, _XRUN_Idx_Count_);
            return execute_call(tsc, val, (int16_t)a1);
         }

         OPER_AT(tsc, XRUN_Idx_PC) = TR7_FROM_UINT(pcode - (uint16_t*)vprog);
         sts = execute_call(tsc, val, (int16_t)a1);
         if (sts != Cycle_Continue)
            return sts;
         break;

      case INSTRID(PROC):

         a1 = *pcode++;
         a2 = *pcode++;

         if (tsc->no_stack)
            return raise_error_msg(tsc, "stack overflow");

         if (*pcode == INSTRID(END)) {
            OPER_POP(tsc, _XRUN_Idx_Count_);
            return execute_proc(tsc, a2, a1);
         }

         OPER_AT(tsc, XRUN_Idx_PC) = TR7_FROM_UINT(pcode - (uint16_t*)vprog);
         sts = execute_proc(tsc, a2, a1);
         if (sts != Cycle_Continue)
            return sts;
         break;

      case INSTRID(GETG):

         a1 = *pcode++;
         val = TR7_VECTOR_ITEM(vprog, a1);
         if (IS_BOX(val))
            val = GET_BOX(val);
         if (TR7_IS_VOID(val))
#if USE_TR7_DEBUG
            return raise_error_msg_obj(tsc, "unbound variable", ITEM_BOX(TR7_VECTOR_ITEM(vprog, a1), 1));
#else
            return raise_error_msg(tsc, "unbound variable");
#endif
         set_values_single(tsc, val);
         break;

      case INSTRID(GETGA):

         a1 = *pcode++;
         val = TR7_VECTOR_ITEM(vprog, a1);
         if (IS_BOX(val))
            val = GET_BOX(val);
         if (TR7_IS_VOID(val))
#if USE_TR7_DEBUG
            return raise_error_msg_obj(tsc, "unbound variable", ITEM_BOX(TR7_VECTOR_ITEM(vprog, a1), 1));
#else
            return raise_error_msg(tsc, "unbound variable");
#endif
         DATA_PUSH(tsc, val);
         break;

      case INSTRID(SETG):

         a1 = *pcode++;
         val = TR7_VECTOR_ITEM(vprog, a1);
         SET_BOX(val, tsc->values[0]);
         drop_values(tsc);
         break;

      case INSTRID(GETC):

         a1 = *pcode++;
         a2 = *pcode++;
         subclos = TR7_TO_VECTOR(TR7_VECTOR_ITEM(frame, Frame_Idx_Link));
         while (--a2)
            subclos = TR7_TO_VECTOR(TR7_VECTOR_ITEM(subclos, Frame_Idx_Link));
         val = TR7_VECTOR_ITEM(subclos, a1);
         set_values_single(tsc, val);
         break;

      case INSTRID(GETL):

         a1 = *pcode++;
         val = TR7_VECTOR_ITEM(frame, a1);
         set_values_single(tsc, val);
         break;

      case INSTRID(GETCA):

         a1 = *pcode++;
         a2 = *pcode++;
         subclos = TR7_TO_VECTOR(TR7_VECTOR_ITEM(frame, Frame_Idx_Link));
         while (--a2)
            subclos = TR7_TO_VECTOR(TR7_VECTOR_ITEM(subclos, Frame_Idx_Link));
         val = TR7_VECTOR_ITEM(subclos, a1);
         DATA_PUSH(tsc, val);
         break;

      case INSTRID(GETLA):

         a1 = *pcode++;
         val = TR7_VECTOR_ITEM(frame, a1);
         DATA_PUSH(tsc, val);
         break;

      case INSTRID(SETC):

         a1 = *pcode++;
         a2 = *pcode++;
         subclos = TR7_TO_VECTOR(TR7_VECTOR_ITEM(frame, Frame_Idx_Link));
         while (--a2)
            subclos = TR7_TO_VECTOR(TR7_VECTOR_ITEM(subclos, Frame_Idx_Link));
         val = tsc->values[0];
         TR7_VECTOR_ITEM(subclos, a1) = val;
         break;

      case INSTRID(SETL):

         a1 = *pcode++;
         val = tsc->values[0];
         TR7_VECTOR_ITEM(frame, a1) = val;
         break;

      case INSTRID(QUOTE):

         a1 = *pcode++;
         val = TR7_VECTOR_ITEM(vprog, a1);
         set_values_single(tsc, val);
         break;

      case INSTRID(QUOTA):

         a1 = *pcode++;
         val = TR7_VECTOR_ITEM(vprog, a1);
         DATA_PUSH(tsc, val);
         break;

      case INSTRID(IMM):

         a1 = *pcode++;
         val = I2TR7((int16_t)a1);
         set_values_single(tsc, val);
         break;

      case INSTRID(IMMA):

         a1 = *pcode++;
         val = I2TR7((int16_t)a1);
         DATA_PUSH(tsc, val);
         break;

      case INSTRID(MVAL):

         a1 = *pcode++;
         if (tsc->nvalues < a1)
            return raise_error_msg(tsc, "missing values");
         pvals = tsc->values;
         a2 = 0;
         goto multiset;

      case INSTRID(MVALD):

         a1 = *pcode++;
         a2 = a1 - 1;
         if (tsc->nvalues < a2)
            return raise_error_msg(tsc, "missing values");
         val = tr7_cons_n(tsc, (int)(tsc->nvalues - a2), &tsc->values[a2], TR7_NIL);
         pvals = tsc->values;
         pvals[a2] = val;
         a2 = 0;
         goto multiset;

      case INSTRID(MSET):

         a2 = a1 = *pcode++;
         pvals = &DATA(tsc, 0);
multiset:
         for ( ; a1 != 0 ; a1--) {
            uint16_t xp, x1, x2;
            xp = *pcode++;
            x1 = *pcode++;
            switch (xp) {
            case INSTRID(SETG):
               val = TR7_VECTOR_ITEM(vprog, x1);
               SET_BOX(val, *pvals++);
               break;
            case INSTRID(SETL):
               TR7_VECTOR_ITEM(frame, x1) = *pvals++;
               break;
            case INSTRID(SETC):
               x2 = *pcode++;
               subclos = TR7_TO_VECTOR(TR7_VECTOR_ITEM(frame, Frame_Idx_Link));
               while (--x2)
                  subclos = TR7_TO_VECTOR(TR7_VECTOR_ITEM(subclos, Frame_Idx_Link));
               TR7_VECTOR_ITEM(subclos, x1) = *pvals++;
               break;
            default:
               break;
            }
         }
         DATA_POP_N(tsc, a2);
         drop_values(tsc);
         break;

      case INSTRID(LAMBDA):

         a1 = *pcode++;
         val = TR7_VECTOR_ITEM(vprog, a1);
         val = mk_closure(tsc, val, OPER_AT(tsc, XRUN_Idx_Frame), Tr7_Head_Kind_Lambda);
         set_values_single(tsc, val);
         break;

      case INSTRID(CASE):

         a1 = pcode[1];
         val = TR7_VECTOR_ITEM(vprog, a1);
         pcode += tr7_unsafe_memv_pair(tsc->values[0], val) != NULL
                           ? 2
                           : (int16_t)*pcode;
         break;

      case INSTRID(GUARD):

         a1 = *pcode;
         guard_push(tsc, TR7_FROM_UINT((pcode + (int16_t)a1) - (uint16_t*)vprog), Guard_Type_Guard);
         pcode++;
         break;

      case INSTRID(UNGUARD):

         guard_pop(tsc);
         pcode += (int16_t)*pcode;
         break;

      case INSTRID(PARAMETER):

         val = tsc->values[0];
         if (!TR7_IS_PARAMETER(val))
            return raise_error_msg_obj(tsc, "not a parameter", val);
         parameter_push(tsc, val, TR7_TO_PARAMETER(val)->value);
         OPER_AT(tsc, XRUN_Idx_PC) = TR7_FROM_UINT(pcode - (uint16_t*)vprog);
         sts = execute_call(tsc, val, 1);
         if (sts != Cycle_Continue)
            return sts;
         break;

      case INSTRID(ENDPARAMETERIZE):

         a1 = *pcode++;
         parameter_pop(tsc, a1);
         break;

      case INSTRID(DEFRECORD):

         a1 = *pcode++;
         a2 = *pcode++;
         val = TR7_VECTOR_ITEM(vprog, a2);
         if (make_record_type_compiled(tsc, val, tsc->values[0], a1) < 0)
            return raise_out_of_memory_error(tsc);
         a2 = a1;
         pvals = &DATA(tsc, 0);
         goto multiset;

#if USE_SCHEME_CASE_LAMBDA
      case INSTRID(CASE_LAMBDA):

         a1 = *pcode++;
         val = TR7_VECTOR_ITEM(vprog, a1);
         val = mk_closure(tsc, val, OPER_AT(tsc, XRUN_Idx_Frame), Tr7_Head_Kind_Case_Lambda);
         set_values_single(tsc, val);
         break;
#endif

#if USE_SCHEME_LAZY
      case INSTRID(DELAY):

         a1 = *pcode++;
         val = TR7_VECTOR_ITEM(vprog, a1);
         val = mk_closure(tsc, val, OPER_AT(tsc, XRUN_Idx_Frame), Tr7_Head_Kind_Lambda);
         val = mk_promise(tsc, TR7_PROMISE_HEAD_DELAY, val);
         set_values_single(tsc, val);
         break;

      case INSTRID(DELAYFORCE):

         a1 = *pcode++;
         val = TR7_VECTOR_ITEM(vprog, a1);
         val = mk_closure(tsc, val, OPER_AT(tsc, XRUN_Idx_Frame), Tr7_Head_Kind_Lambda);
         val = mk_promise(tsc, TR7_PROMISE_HEAD_DELAY_FORCE, val);
         set_values_single(tsc, val);
         break;
#endif

#if HAS_CHECK_TYPES_NO
      case INSTRID(PROCUNSAFE):

         a1 = *pcode++;
         a2 = *pcode++;

         if (tsc->no_stack)
            return raise_error_msg(tsc, "stack overflow");

         if (*pcode == INSTRID(END)) {
            OPER_POP(tsc, _XRUN_Idx_Count_);
            return execute_proc_unsafe(tsc, a2, a1);
         }

         OPER_AT(tsc, XRUN_Idx_PC) = TR7_FROM_UINT(pcode - (uint16_t*)vprog);
         sts = execute_proc_unsafe(tsc, a2, a1);
         if (sts != Cycle_Continue)
            return sts;
         break;
#endif

      default:
         /* what to do? ignore */
         break;
      }
      ok_to_freely_gc(tsc);
   }
   return Cycle_Return;
}
/*
* execute the stacked program until end
* the possibly returned evaluation status are
* - Cycle_OOM: out of memory reached
* - Cycle_Leave: normal leave
* - Cycle_Leave_Error: leave with error
*/
static eval_status_t main_loop(tr7_engine_t tsc)
{
   eval_status_t es;
   while(HAS_OPER(tsc)) {
      /* get current operator */
      tr7_t oper = OPER_AT(tsc, 0);

      /* tracing or not tracing? */
#if USE_TR7_TRACE
      if (tsc->tracing) {
#if EXTRA_TRACING
         log_str(tsc, "\n************\n");
         oper_stack_dump(tsc, tsc->tracing - 1);
         log_str(tsc, "   ---\n");
         data_stack_dump(tsc, tsc->tracing - 1);
#endif
         log_str(tsc, "\nloop exec ");
         log_item(tsc, oper);
         log_str(tsc, "\n");
      }
#endif

      /* action */
#if SWITCHED_OPERATORS
      /* using switch+case */
      switch(TO_OPER(oper)) {
#        define ___OPER_(FUNC,CODE)  case OPERID(CODE): es = FUNC(tsc); break;
#        include __FILE__
      default:
#else
      /* using vector */
      if (IS_OPER(oper)) {
         static tr7_oper_t operators[] = {
#           define ___OPER_(FUNC,CODE)       FUNC,
#           include __FILE__
         };
         es = operators[TO_OPER(oper)](tsc);
      }
      else {
#endif
         log_str(tsc, "Unexpected operator!\n");
         es = Cycle_Leave_Error;
         break;
      }

      /* safe memory */
      if (tsc->no_memory) {
         log_str(tsc, "No memory!\n");
         es = Cycle_OOM;
      }

      /* retracing or not */
#if USE_TR7_TRACE
      if (tsc->tracing) {
         log_str(tsc, "\n---loop exec -> ");
         switch(es) {
         case Cycle_Goto:    log_str(tsc, "GOTO\n"); break;
         case Cycle_Return:  log_str(tsc, "RETURN"); if (tsc->tracing) trace_return_proc(tsc); break;
         case Cycle_Raise:   log_str(tsc, "RAISE\n"); break;
         case Cycle_Raise_Cont: log_str(tsc, "RAISECONT\n"); break;
         case Cycle_OOM:     log_str(tsc, "OOM\n"); break;
         case Cycle_Leave:   log_str(tsc, "LEAVE\n"); break;
         case Cycle_Leave_Error: log_str(tsc, "LEAVEERROR\n"); break;
         default:            log_str(tsc, "?\n"); break;
         }
         log_str(tsc, "\n");
      }
#endif

      /* okay for gc */
      ok_to_freely_gc(tsc);

      /* */
      if (es == Cycle_Raise)
         es = do_raise(tsc, 0);
      else if(es == Cycle_Raise_Cont)
         es = do_raise(tsc, 1);

      /* continue */
      switch(es) {
      default:
      case Cycle_Return:
         break;
      case Cycle_Goto:
         tsc->nvalues = 0;
         break;
      case Cycle_OOM:
      case Cycle_Leave:
      case Cycle_Leave_Error:
         return es;
      }
   }
   return Cycle_Leave;
}

static void prepare_loop_exec(tr7_engine_t tsc)
{
   guard_push(tsc, TR7_NIL, Guard_Type_Leave);
   oper_push_safe_1(tsc, OPER(LEAVE));
}

static eval_status_t execute_prog(tr7_engine_t tsc, tr7_t prog)
{
   prepare_loop_exec(tsc);
   oper_push_safe_2(tsc, OPER(PROG), prog);
   return main_loop(tsc);
}

/*
**************************************************************************
* SECTION COMPILE
* ---------------
*
* Predefinitions for compiling:
*/
static void cpl_common_init(cpl_t cpl, tr7_engine_t tsc, cpl_error_t *error)
{
   cpl->tsc = tsc;
   cpl->error = error;

   cpl->varcount = 0;
   cpl->vars = TR7_NIL;
   cpl->vsyn = TR7_NIL;

   cpl->poscode = 0;
   cpl->lastopos = 0;
   cpl->holders = TR7_NIL;

#if USE_TR7_DEBUG && DEBUG_LINES
   cpl->poslines = 0;
   cpl->curline = 0;
#endif
}
/*
* Initialisation of a compiling context for a given engine and upper.
*/
#if USE_TR7_DEBUG && DEBUG_LINES
static void cpl_init(cpl_t cpl, tr7_engine_t tsc, cpl_error_t *error, tr7_t filename, tr7_t linetrack)
#else
static void cpl_init(cpl_t cpl, tr7_engine_t tsc, cpl_error_t *error)
#endif
{
   cpl_common_init(cpl, tsc, error);

   cpl->upper = NULL;
   cpl->inlet = 0;

   error->text = NULL;
   error->args = TR7_VOID;
   error->expr = TR7_VOID;
   error->error = TR7_VOID;
   error->kind = Cpl_No_Error;

   cpl->self = NULL;

   cpl->szcode = 0;
   cpl->code = NULL;

#if USE_TR7_DEBUG && DEBUG_LINES
   cpl->linetrack = linetrack;
   if (TR7_IS_PAIR(linetrack))
      cpl->cur_line = TR7_CDAR(linetrack);
   else if (TR7_IS_INT(linetrack))
      cpl->cur_line = linetrack;
   else
      cpl->cur_line = TR7_FROM_INT(0);
   cpl->filename = filename;
   error->line = 0;

   cpl->szlines = 0;
   cpl->lines = NULL;
#endif
}
/*
* Initialisation of a compiling context for a given engine
* and upper compiling context.
*/
static void cpl_enter(cpl_t cpl, cpl_t upper)
{
   cpl_common_init(cpl, upper->tsc, upper->error);

   cpl->upper = upper;
   cpl->inlet = 1;

   cpl->self = upper->self;

   cpl->szcode = upper->szcode - upper->poscode;
   cpl->code = &upper->code[upper->poscode];

#if USE_TR7_DEBUG && DEBUG_LINES
   cpl->linetrack = upper->linetrack;
   cpl->cur_line = upper->cur_line;
   cpl->filename = upper->filename;

   cpl->szlines = upper->szlines - upper->poslines;
   cpl->lines = &upper->lines[upper->poslines];
#endif
}
/*
* leave an entered cpl context
*/
static int cpl_leave(cpl_t cpl, int rc)
{
   if (cpl->upper == NULL) {
      free(cpl->code);
#if USE_TR7_DEBUG && DEBUG_LINES
      free(cpl->lines);
#endif
   }
   return rc;
}
/*
* save the current state of variables of cpl in savars
*/
static void cpl_vars_save(cpl_t cpl, cpl_vars_t *savars)
{
   savars->vars = cpl->vars;
   savars->vsyn = cpl->vsyn;
   savars->inlet = cpl->inlet;
   cpl->inlet = 1;
}
/*
* restore state of variables of cpl from savars
*/
static void cpl_vars_restore(cpl_t cpl, cpl_vars_t *savars)
{
   cpl->vars = savars->vars;
   cpl->vsyn = savars->vsyn;
   cpl->inlet = savars->inlet;
}
/*
* report a compilation error
*/
static int cpl_error(cpl_t cpl, const char *text, tr7_t args, cpl_error_kind_t kind)
{
   cpl_error_t *error = cpl->error;
   error->kind = kind;
#if USE_TR7_DEBUG && DEBUG_LINES
   error->line = TR7_TO_INT(cpl->cur_line);
#endif
   error->text = text;
   error->args = args;
   return -1;
}
/*
* report a compilation error during evaluation
*/
static int cpl_error_eval(cpl_t cpl, tr7_t errobj)
{
   cpl_error_t *error = cpl->error;
   error->kind = Cpl_Error_Eval;
#if USE_TR7_DEBUG && DEBUG_LINES
   error->line = TR7_TO_INT(cpl->cur_line);
#endif
   error->error = errobj;
   return -1;
}
/*
* report a syntax error
*/
static int cpl_error_syntax(cpl_t cpl, const char *text, tr7_t args)
{
   return cpl_error(cpl, text, args, Cpl_Error_Syntax);
}
/*
* report a validity error
*/
static int cpl_error_validity(cpl_t cpl, const char *text, tr7_t args)
{
   return cpl_error(cpl, text, args, Cpl_Error_Validity);
}
/*
* report an internal error
*/
static int cpl_error_internal(cpl_t cpl, const char *text, tr7_t args)
{
   return cpl_error(cpl, text, args, Cpl_Error_Internal);
}
/*
* report a out of memory error
*/
static int cpl_oom(cpl_t cpl)
{
   return cpl_error_internal(cpl, "out of memory", TR7_VOID);
}
/*
* report improper list
*/
static int cpl_error_improper_list(cpl_t cpl, tr7_t args)
{
   return cpl_error_syntax(cpl, "improper list", args);
}
/*
* report not a pair
*/
static int cpl_error_not_a_pair(cpl_t cpl, tr7_t args)
{
   return cpl_error_syntax(cpl, "not a pair", args);
}
/*
* report not a symbol
*/
static int cpl_error_not_a_symbol(cpl_t cpl, tr7_t args)
{
   return cpl_error_syntax(cpl, "not a symbol", args);
}
/*
* check that a list is proper and has enougth elements
* returns the length of the list if valid or else a negtive number
*/
static int cpl_check_list(cpl_t cpl, tr7_t item, int min, int max)
{
   int len = tr7_list_length(item);
   if (len < 0)
      return cpl_error_improper_list(cpl, item);
   if (len < min)
      return cpl_error_syntax(cpl, "incomplete list", item);
   if (max > 0 && len > max)
      return cpl_error_syntax(cpl, "list too long", item);
   return len;
}
/*
* report the compiling error to the evaluator loop
*/
static void cpl_report_error(cpl_t cpl)
{
   tr7_engine_t tsc = cpl->tsc;
   cpl_error_t *error = cpl->error;
   tr7_t obj = error->error;

   /* already a runtime error */
   if (!TR7_IS_VOID(obj))
      set_error(tsc, obj);
   else {
      /* no, build the runtime error */
      const char *kind;
      switch (error->kind) {
      case Cpl_Error_Syntax:
         kind = "syntax";
         break;
      case Cpl_Error_Validity:
         kind = "validity";
         break;
      case Cpl_Error_Internal:
         kind = "internal";
         break;
      case Cpl_No_Error:
      case Cpl_Error_Eval:
      default:
         kind = NULL;
         break;
      }
      if (kind == NULL)
         set_error_msg_irr(tsc, "undocumented error", TR7_NIL, 0);
      else {
         char sbuf[STRBUFFSIZE];
         const char *fname = NULL;
         int lino = 0;
#if USE_TR7_DEBUG && DEBUG_LINES
         if (TR7_IS_STRING(cpl->filename)) {
            fname = (const char*)TR7_CONTENT_STRING(cpl->filename);
            lino = error->line;
         }
#endif
         if (fname == NULL)
            snprintf(sbuf, sizeof sbuf, "%s error: %s", kind, error->text);
         else
            snprintf(sbuf, sizeof sbuf, "%s:%d: %s error: %s", fname, lino, kind, error->text);
         memcpy(&sbuf[sizeof sbuf - 4], "...", 4);
         set_error_msg_obj(tsc, sbuf, error->args, 1);
      }
   }
}
/*
* extend memory used for creation of code
*/
static int cpl_extend_code(cpl_t cpl)
{
   cpl_t upper = cpl->upper;
   if (upper == NULL) {
      /* code memory is managed at top level */
      unsigned sz = cpl->szcode ? cpl->szcode + cpl->szcode : 1024;
      uint16_t *code = realloc(cpl->code, sz * sizeof(uint16_t)); /* TODO use an allocator */
      if (code == NULL)
         return cpl_oom(cpl);
      cpl->code = code;
      cpl->szcode = sz;
   }
   else {
      /* extend upper code memory and adapt */
      int rc = cpl_extend_code(upper);
      if (rc < 0)
         return rc;
      cpl->code = &upper->code[upper->poscode];
      cpl->szcode = upper->szcode - upper->poscode;
   }
   return 0;
}
#if USE_TR7_DEBUG && DEBUG_LINES
/*
* extend memory used for creation of lines
*/
static int cpl_extend_lines(cpl_t cpl)
{
   cpl_t upper = cpl->upper;
   if (upper == NULL) {
      /* line memory is managed at top level */
      unsigned sz = cpl->szlines ? cpl->szlines + cpl->szlines : 1024;
      uint8_t *lines = realloc(cpl->lines, sz * sizeof(uint8_t));
      if (lines == NULL)
         return cpl_oom(cpl);
      cpl->lines = lines;
      cpl->szlines = sz;
   }
   else {
      /* extend upper line memory and adapt */
      int rc = cpl_extend_lines(upper);
      if (rc < 0)
         return rc;
      cpl->lines = &upper->lines[upper->poslines];
      cpl->szlines = upper->szlines - upper->poslines;
   }
   return 0;
}
/*
* add the value in line buffer encode the value using KIM
*/
static int cpl_encode_line_put(cpl_t cpl, unsigned value, uint8_t mask)
{
   int rc = value <= 127 ? 0 : cpl_encode_line_put(cpl, value >> 7, 128);
   if (rc >= 0 && cpl->szlines == cpl->poslines)
      rc = cpl_extend_lines(cpl);
   if (rc >= 0)
      cpl->lines[cpl->poslines++] = mask | (uint8_t)(value & 127);
   return rc;
}
/*
* encode current line position if needed
*/
static int cpl_encode_line(cpl_t cpl)
{
   int rc = 0;
   unsigned li = (unsigned)TR7_TO_UINT(cpl->cur_line);

   /* is begin ? */
   if (cpl->poslines == 0)
      rc = cpl_encode_line_put(cpl, li, 0);
   /* is a new valid line ? */
   else if (li != cpl->curline) {
      /* encode the position in code except first time */
      rc = cpl_encode_line_put(cpl, cpl->poscode, 0);
      /* encode line number */
      if (rc >= 0)
         rc = cpl_encode_line_put(cpl, li, 0);
   }

   /* record the line */
   cpl->curline = li;
   return rc;
}
#else
static int cpl_encode_line(cpl_t cpl)
{
   return 0;
}
#endif








/* TODO comment */
typedef
struct {
   tr7_t value;
}
   label_t;


static int cpl_make_label(cpl_t cpl, label_t *label)
{
   tr7_t nxt = TR7_CONS2(cpl->tsc, TR7_VOID, TR7_NIL);
   label->value = nxt;
   if (TR7_IS_NIL(nxt))
      return cpl_oom(cpl);
   return 0;
}

static int cpl_set_label(cpl_t cpl, label_t label)
{
   tr7_t it, nxt = label.value;
   if (!TR7_IS_VOID(TR7_CAR(nxt)))
      return cpl_error_internal(cpl, "redefined (reset) label", TR7_VOID);
   cpl->lastopos = cpl->poscode; /* invalidate lastopos to avoid ARG optimisation */
   TR7_CAR(nxt) = TR7_FROM_UINT(cpl->poscode);
   it = TR7_CDR(nxt);
   TR7_CDR(nxt) = TR7_NIL;
   while (!TR7_IS_NIL(it)) {
      cpl->code[TR7_TO_UINT(TR7_CAR(it))] = cpl->poscode;
      it = TR7_CDR(it);
   }
   return 0;
}

static int cpl_label_get(cpl_t cpl, label_t label)
{
   int rc = 0;
   tr7_t nxt = label.value;
   if (!TR7_IS_VOID(TR7_CAR(nxt)))
      rc = (int)TR7_TO_INT(TR7_CAR(nxt));
   else {
      tr7_t head = TR7_CONS2(cpl->tsc, TR7_FROM_UINT(cpl->poscode), TR7_CDR(nxt));
      if (TR7_IS_NIL(head))
         rc = cpl_oom(cpl);
      else
         TR7_CDR(nxt) = head;
   }
   return rc;
}



/*
* Get the numerical index of a quoted value
*/
static int cpl_hold_value(cpl_t cpl, tr7_t value)
{
   /* search existing same value in holders */
   tr7_pair_t pos;
#if HOLD_UNIQUE_INSTANCE
   pos = tr7_unsafe_member_pair(value, cpl->holders);
   if (pos == NULL)
#endif
   {
      /* not found, add it */
      tr7_t head = tr7_cons(cpl->tsc, value, cpl->holders);
      if (!TR7_IS_PAIR(head))
         return cpl_oom(cpl);
      cpl->holders = head;
      pos = TR7_TO_PAIR(head);
   }
   /* remaining list length is the index in the reversed list */
   return tr7_unsafe_list_length(TR7_PAIR_CDR(pos));
}
static int cpl_encode_uint16(cpl_t cpl, uint16_t value)
{
   if (cpl->szcode == cpl->poscode) {
      int rc = cpl_extend_code(cpl);
      if (rc < 0)
         return rc;
   }
   cpl->code[cpl->poscode++] = value;
   return 0;
}

static int cpl_encode_uint(cpl_t cpl, unsigned value)
{
/*
   if (value >> 16)
      return cpl_error_internal(cpl, "can't encode value", TR7_FROM_INT(value));
*/
   return cpl_encode_uint16(cpl, (uint16_t)value);
}

static int cpl_encode_value(cpl_t cpl, tr7_t value)
{
   int rc = cpl_hold_value(cpl, value);
   if (rc >= 0)
      rc = cpl_encode_uint(cpl, (unsigned)rc);
   return rc;
}

static int cpl_encode_instr(cpl_t cpl, instrid_t instr)
{
   int rc = cpl_encode_line(cpl);
   cpl->lastopos = cpl->poscode;
   if (rc >= 0)
      rc = cpl_encode_uint(cpl, (unsigned)instr);
   return rc;
}

static int cpl_encode_instr_uint(cpl_t cpl, instrid_t instr, unsigned value)
{
   int rc = cpl_encode_instr(cpl, instr);
   if (rc >= 0)
      rc = cpl_encode_uint(cpl, value);
   return rc;
}

static int cpl_encode_instr_int(cpl_t cpl, instrid_t instr, int value)
{
   int rc = cpl_encode_instr(cpl, instr);
   if (rc >= 0)
      rc = cpl_encode_uint(cpl, value & UINT16_MAX);
   return rc;
}

static int cpl_encode_instr_uint_uint(cpl_t cpl, instrid_t instr, unsigned value1, unsigned value2)
{
   int rc = cpl_encode_instr(cpl, instr);
   if (rc >= 0)
      rc = cpl_encode_uint(cpl, value1);
   if (rc >= 0)
      rc = cpl_encode_uint(cpl, value2);
   return rc;
}

static int cpl_encode_instr_value(cpl_t cpl, instrid_t instr, tr7_t value)
{
   int rc = cpl_encode_instr(cpl, instr);
   if (rc >= 0)
      rc = cpl_encode_value(cpl, value);
   return rc;
}

static int cpl_encode_instr_uint_value(cpl_t cpl, instrid_t instr, unsigned uval, tr7_t value)
{
   int rc = cpl_encode_instr_uint(cpl, instr, uval);
   if (rc >= 0)
      rc = cpl_encode_value(cpl, value);
   return rc;
}

static int cpl_encode_label(cpl_t cpl, label_t label)
{
   int rc = cpl_label_get(cpl, label);
   if (rc >= 0)
      rc = cpl_encode_uint(cpl, (unsigned)rc);
   return rc;
}

static int cpl_encode_instr_label(cpl_t cpl, instrid_t instr, label_t label)
{
   int rc = cpl_encode_instr(cpl, instr);
   if (rc >= 0)
      rc = cpl_encode_label(cpl, label);
   return rc;
}

static int cpl_encode_instr_label_value(cpl_t cpl, instrid_t instr, label_t label, tr7_t value)
{
   int rc = cpl_encode_instr_label(cpl, instr, label);
   if (rc >= 0)
      rc = cpl_encode_value(cpl, value);
   return rc;
}

static int cpl_emit_quote(cpl_t cpl, tr7_t value)
{
   int rc;
   if (!TR7_IS_PTR(value) && INT16_MIN <= TR72I(value) && TR72I(value) <= INT16_MAX)
      rc = cpl_encode_instr_uint(cpl, INSTRID(IMM), (unsigned)TR72I(value));
   else
      rc = cpl_encode_instr_value(cpl, INSTRID(QUOTE), value);
   return rc;
}

static int cpl_emit_set_var_local(cpl_t cpl, int num, int depth)
{
   if (depth == 0)
      return cpl_encode_instr_uint(cpl, INSTRID(SETL), (unsigned)num);
   return cpl_encode_instr_uint_uint(cpl, INSTRID(SETC), (unsigned)num, (unsigned)depth);
}

static int cpl_emit_get_var_local(cpl_t cpl, int num, int depth)
{
   if (depth == 0)
      return cpl_encode_instr_uint(cpl, INSTRID(GETL), (unsigned)num);
   return cpl_encode_instr_uint_uint(cpl, INSTRID(GETC), (unsigned)num, (unsigned)depth);
}

static int cpl_emit_set_var_global(cpl_t cpl, tr7_t box)
{
   return cpl_encode_instr_value(cpl, INSTRID(SETG), box);
}

static int cpl_emit_get_var_global(cpl_t cpl, tr7_t box)
{
   return cpl_encode_instr_value(cpl, INSTRID(GETG), box);
}

static int cpl_emit_push_arg(cpl_t cpl)
{
   uint16_t code = INSTRID(ARG);
   if (cpl->lastopos < cpl->poscode) {
      switch (cpl->code[cpl->lastopos]) {
      case INSTRID(GETC):  code = INSTRID(GETCA); break;
      case INSTRID(GETL): code = INSTRID(GETLA); break;
      case INSTRID(GETG):  code = INSTRID(GETGA); break;
      case INSTRID(QUOTE): code = INSTRID(QUOTA); break;
      case INSTRID(IMM):   code = INSTRID(IMMA); break;
      }
   }
   if (code == INSTRID(ARG))
      return cpl_encode_instr(cpl, INSTRID(ARG));
   cpl->code[cpl->lastopos] = code;
   return 0;
}

static int cpl_emit_lambda(cpl_t cpl, tr7_t proc)
{
   return cpl_encode_instr_value(cpl, INSTRID(LAMBDA), proc);
}

static int cpl_emit_call(cpl_t cpl, int nargs)
{
   uint16_t glob;
   if (cpl->lastopos + 2 != cpl->poscode || cpl->code[cpl->lastopos] != INSTRID(GETG))
      return cpl_encode_instr_uint(cpl, INSTRID(CALL), (unsigned)nargs);
   cpl->code[cpl->lastopos] = INSTRID(CALLG);
   glob = cpl->code[cpl->lastopos + 1];
   cpl->code[cpl->lastopos + 1] = (uint16_t)nargs;
   return cpl_encode_uint(cpl, glob);
}

static int cpl_emit_call_self(cpl_t cpl, int nargs)
{
   return cpl_encode_instr_uint(cpl, INSTRID(CALLSELF), (unsigned)nargs);
}

static int cpl_emit_proc(cpl_t cpl, procid_t procid, int nargs)
{
   instrid_t instrid = INSTRID(PROC);
#if HAS_CHECK_TYPES_NO
   if (cpl->tsc->no_check_types)
      instrid = INSTRID(PROCUNSAFE);
#endif
   return cpl_encode_instr_uint_uint(cpl,
                     instrid, (unsigned)nargs, (unsigned)procid);
}

static int cpl_emit_if_true(cpl_t cpl, label_t label)
{
   return cpl_encode_instr_label(cpl, INSTRID(IFTRUE), label);
}

static int cpl_emit_if(cpl_t cpl, label_t label, int neg)
{
   instrid_t instrid = neg ? INSTRID(IFFALSE) : INSTRID(IFTRUE);
   return cpl_encode_instr_label(cpl, instrid, label);
}

static int cpl_emit_test(cpl_t cpl, label_t label, int neg)
{
   return cpl_emit_if(cpl, label, neg);
}

static int cpl_emit_cond(cpl_t cpl, label_t label, int feedto)
{
   instrid_t instrid = INSTRID(IFFALSE);
   return cpl_encode_instr_label(cpl, instrid, label);
}

static int cpl_emit_goto(cpl_t cpl, label_t label)
{
   return cpl_encode_instr_label(cpl, INSTRID(GOTO), label);
}

static int cpl_emit_not_case(cpl_t cpl, tr7_t items, label_t label)
{
   return cpl_encode_instr_label_value(cpl, INSTRID(CASE), label, items);
}

static int cpl_emit_mvalues(cpl_t cpl, int count, int dotted)
{
   instrid_t instr = dotted ? INSTRID(MVALD) : INSTRID(MVAL);
   return cpl_encode_instr_int(cpl, instr, count);
}

static int cpl_emit_mset(cpl_t cpl, int count)
{
   return cpl_encode_instr_uint(cpl, INSTRID(MSET), (unsigned)count);
}

#if USE_SCHEME_CASE_LAMBDA
static int cpl_emit_case_lambda(cpl_t cpl, tr7_t cases)
{
   return cpl_encode_instr_value(cpl, INSTRID(CASE_LAMBDA), cases);
}
#endif

static int cpl_emit_parameterize(cpl_t cpl)
{
   return cpl_encode_instr(cpl, INSTRID(PARAMETER));
}

static int cpl_emit_end_parameterize(cpl_t cpl, int count)
{
   return cpl_encode_instr_uint(cpl, INSTRID(ENDPARAMETERIZE), (unsigned)count);
}

static int cpl_emit_guard(cpl_t cpl, label_t label)
{
   return cpl_encode_instr_label(cpl, INSTRID(GUARD), label);
}

static int cpl_emit_end_guard(cpl_t cpl, label_t label)
{
   return cpl_encode_instr_label(cpl, INSTRID(UNGUARD), label);
}

static int cpl_emit_define_record(cpl_t cpl, tr7_t fspec, int count)
{
   return cpl_encode_instr_uint_value(cpl, INSTRID(DEFRECORD), (unsigned)count, fspec);
}

#if USE_SCHEME_LAZY
static int cpl_emit_delay(cpl_t cpl, int force, tr7_t proc)
{
   instrid_t instr = force ? INSTRID(DELAYFORCE) : INSTRID(DELAY);
   return cpl_encode_instr_value(cpl, instr, proc);
}
#endif

static uint16_t ungoto(uint16_t *code, uint16_t off)
{
   while (code[off] == INSTRID(GOTO))
      off = code[off + 1];
   return off;
}

#if USE_TR7_DEBUG && DEBUG_LINES
static unsigned KIM_put(uint8_t *dest, unsigned value)
{
   unsigned i = 7, n = 1;
   while (value >= (1U << i) && (1 << i) != 0)
      n++, i += 7;
   for (i -= 7 ; i > 0 ; i -= 7)
      *dest++ = 128 | (127 & (value >> i));
   *dest = (uint8_t)(value & 127);
   return n;
}
#endif

static int cpl_finalize_code(cpl_t cpl, int nargs, int dotted)
{
   unsigned idx, lopa;
   uint16_t instr, off;
   uint8_t  mode;
   uint16_t *code = cpl->code;
   unsigned lencode = cpl->poscode;
   unsigned upperlencode = lencode + (unsigned)nargs * 3;
   int16_t  *sdepth = (int16_t *)TR7_ALLOCA(lencode * sizeof(int16_t));
   uint16_t *recode = (uint16_t *)TR7_ALLOCA(upperlencode * sizeof(uint16_t));
   uint16_t *renum = (uint16_t *)TR7_ALLOCA(upperlencode * sizeof(uint16_t));
   unsigned rpos, wpos;
   int      curdep, maxdep;
#if USE_TR7_DEBUG && DEBUG_LINES
   unsigned lenlines = cpl->poslines;
   uint8_t  *lines = cpl->lines;
   uint8_t  *relines = (uint8_t *)TR7_ALLOCA(lencode * sizeof(uint8_t));
#endif

   /* init of stack depths */
   for (idx = 0 ; idx < lencode ; idx++)
      sdepth[idx] = 0;

   /* first pass */
   curdep = maxdep = 0;
   for (rpos = wpos = 0 ; rpos < lencode ; ) {
      if (curdep < 0)
         curdep = sdepth[rpos];
      renum[rpos] = wpos;
      instr = code[rpos];
      mode = decode_instr_modes[instr];
      lopa = (unsigned)DECODE_LENGTH(mode);
      switch(instr) {
      case INSTRID(GOTO):
         sdepth[code[rpos + 1]] = curdep;
         curdep = -1;
         off = ungoto(code, code[rpos + 1]);
         if (code[off] == INSTRID(END)) {
            recode[wpos++] = INSTRID(END);
            rpos += 2;
            continue;
         }
         code[rpos + 1] = off;
         break;
      case INSTRID(CASE):
         code[rpos + 2] += Program_Idx_Quote0;
         /*@fallthrough@*/
      case INSTRID(IFFALSE):
      case INSTRID(IFTRUE):
      case INSTRID(GUARD):
      case INSTRID(UNGUARD):
         sdepth[code[rpos + 1]] = curdep;
         code[rpos + 1] = ungoto(code, code[rpos + 1]);
         break;
      case INSTRID(GETCA):
      case INSTRID(GETLA):
         code[rpos + 1] += Frame_Idx_Arg0;
         /*@fallthrough@*/
      case INSTRID(ARG):
      case INSTRID(IMMA):
         if (++curdep > maxdep)
            maxdep = curdep;
         break;
      case INSTRID(CALLG):
         code[rpos + 2] += Program_Idx_Quote0;
         /*@fallthrough@*/
      case INSTRID(CALLSELF):
      case INSTRID(CALL):
      case INSTRID(PROC):
#if HAS_CHECK_TYPES_NO
      case INSTRID(PROCUNSAFE):
#endif
         curdep -= code[rpos + 1];
         break;
      case INSTRID(DEFRECORD):
         code[rpos + 2] += Program_Idx_Quote0;
         if (curdep + code[rpos + 1] > maxdep)
            maxdep = curdep + code[rpos + 1];
         break;
      case INSTRID(MSET):
         curdep -= code[rpos + 1];
         break;
      case INSTRID(PARAMETER):
         curdep--;
         break;
      case INSTRID(GETC):
      case INSTRID(SETC):
      case INSTRID(GETL):
      case INSTRID(SETL):
         code[rpos + 1] += Frame_Idx_Arg0;
         break;
      case INSTRID(GETGA):
      case INSTRID(QUOTA):
         if (++curdep > maxdep)
            maxdep = curdep;
         /*@fallthrough@*/
      case INSTRID(GETG):
      case INSTRID(SETG):
      case INSTRID(QUOTE):
      case INSTRID(LAMBDA):
#if USE_SCHEME_CASE_LAMBDA
      case INSTRID(CASE_LAMBDA):
#endif
#if USE_SCHEME_LAZY
      case INSTRID(DELAY):
      case INSTRID(DELAYFORCE):
#endif
         code[rpos + 1] += Program_Idx_Quote0;
         break;
      case INSTRID(IMM):
      case INSTRID(MVAL):
      case INSTRID(ENDPARAMETERIZE):
      case INSTRID(END):
         break;
      }
      /* copy of the instruction */
      for (idx = 0 ; idx <= lopa ; idx++)
         recode[wpos + idx] = code[rpos + idx];
      wpos += idx;
      rpos += 1 + lopa;
   }

   /* second pass */
   for (rpos = 0 ; rpos < wpos ; ) {
      instr = recode[rpos];
      mode  = decode_instr_modes[instr];
      lopa = (unsigned)DECODE_LENGTH(mode);
      switch(instr) {
      case INSTRID(GOTO):
      case INSTRID(IFTRUE):
      case INSTRID(IFFALSE):
      case INSTRID(UNGUARD):
      case INSTRID(CASE):
      case INSTRID(GUARD):
         recode[rpos + 1] = renum[recode[rpos + 1]] - rpos - 1;
         break;
      default:
         break;
      }
      rpos += 1 + lopa;
   }
   memcpy(code, recode, wpos * sizeof *code);
   cpl->poscode = wpos;
#if GLOBAL_STACK_SAFETY
   if ((unsigned)maxdep > cpl->tsc->stack.safegap)
      cpl->tsc->stack.safegap = (unsigned)maxdep;
#endif

#if USE_TR7_DEBUG && DEBUG_LINES
   for(rpos = wpos = 0 ;;) {
      unsigned x = (unsigned)(lines[rpos] & 127);
      while (lines[rpos++] & 128)
         x = (x << 7) | (unsigned)(lines[rpos] & 127);
      wpos += KIM_put(&relines[wpos], x);
      if (rpos >= lenlines)
         break;
      x = (unsigned)(lines[rpos] & 127);
      while (lines[rpos++] & 128)
         x = (x << 7) | (unsigned)(lines[rpos] & 127);
      x = renum[x];
      wpos += KIM_put(&relines[wpos], x);
   }
   memcpy(lines, relines, wpos);
   cpl->poslines = wpos;
#endif

   /*  pass: change target addresses */
   return 0;
}

/*
* make a program
*/
static tr7_t make_prog(
   tr7_engine_t tsc,
   int nparams, int dotted,
   int nlocals,
   const uint16_t *code, unsigned szcode, tr7_t quoteds
#if USE_TR7_DEBUG
   , tr7_t name
#if DEBUG_LINES
   , tr7_t filename, const uint8_t *lines, unsigned szlines
#endif
#endif
) {
   unsigned idx;
   tr7_t prog;
   tr7_vector_t vprog;
   unsigned nrval = Program_Idx_Quote0 + (size_t)tr7_unsafe_list_length(quoteds);
   unsigned nrbytescode = szcode * sizeof(uint16_t);
   unsigned ncellcode = NCELL_OF_SIZE(nrbytescode);
   unsigned ncells = nrval + ncellcode;
#if USE_TR7_DEBUG && DEBUG_LINES
   size_t ncelllines = NCELL_OF_SIZE(szlines);
   ncells += ncelllines;
#endif
   vprog = get_cells(tsc, 1 + ncells, 0);
   if (vprog == NULL)
      prog = TR7_NIL;
   else {
      TR7_CELL_HEAD(vprog) = TR7_MAKE_HEAD(ncells, Tr7_Head_Kind_Program);
      prog = push_recent_cell(tsc, vprog);
      TR7_VECTOR_ITEM(vprog, Program_Idx_Code) = TR7_FROM_UINT(nrval);
      TR7_VECTOR_ITEM(vprog, Program_Idx_nParams) = TR7_FROM_INT(dotted ? -nparams : nparams);
      TR7_VECTOR_ITEM(vprog, Program_Idx_nLocals) = TR7_FROM_INT(nlocals);
      for (idx = Program_Idx_Quote0 ; !TR7_IS_NIL(quoteds) ; quoteds = TR7_CDR(quoteds), idx++)
         TR7_VECTOR_ITEM(vprog, idx) = TR7_CAR(quoteds);
      memset(&TR7_VECTOR_ITEM(vprog, nrval + ncellcode - 1), 0xff, sizeof(tr7_t));
      memcpy(&TR7_VECTOR_ITEM(vprog, nrval), code, nrbytescode);
#if USE_TR7_DEBUG
      TR7_VECTOR_ITEM(vprog, Program_Idx_Name) = name;
#if DEBUG_LINES
      TR7_VECTOR_ITEM(vprog, Program_Idx_Filename) = filename;
      TR7_VECTOR_ITEM(vprog, Program_Idx_Lines) = TR7_FROM_UINT(nrval + ncellcode);
      if (szlines) {
         memset(&TR7_VECTOR_ITEM(vprog, ncells - 1), 0xff, sizeof(tr7_t));
         memcpy(&TR7_VECTOR_ITEM(vprog, nrval + ncellcode), lines, szlines);
      }
#endif
#endif
   }
   return prog;
}
/*
* make lambda intro from current data
*/
static int cpl_make_prog(cpl_t cpl, tr7_t name, int nargs, int dotted, tr7_t *result)
{
   /* set the end at end */
   cpl_encode_instr(cpl, INSTRID(END));

   /* fix holders order */
   cpl->holders = tr7_reverse_in_place(cpl->holders, TR7_NIL);

#if 0
   printf("\n\n\nBEFORE CPL-FINALIZE\n");
   disassemble(cpl->tsc, NULL, 0, cpl->code, cpl->poscode, NULL, 0, cpl->holders
#if DEBUG_LINES
         , cpl->lines, cpl->poslines
#endif
         );
#endif
   cpl_finalize_code(cpl, nargs, dotted);

   /* create the program */
   *result = make_prog(cpl->tsc,
         nargs, dotted, cpl->varcount,
         cpl->code, cpl->poscode,
         cpl->holders
#if USE_TR7_DEBUG
         , name
#if DEBUG_LINES
         , cpl->filename, cpl->lines, cpl->poslines
#endif
#endif
      );

#if 0
   printf("\nAFTER CPL-FINALIZE\n");
   disassemble_program(cpl->tsc, *result, NULL, 0);
#endif

   /* reset surrent state */
   cpl->holders = TR7_NIL;
   cpl->poscode = 0;
#if USE_TR7_DEBUG && DEBUG_LINES
   cpl->poslines = 0;
#endif
   return TR7_IS_NIL(*result) ? cpl_oom(cpl) : 0;
}
/*
* handling compilation environment
*/
#define MAKE_SYNVAR(tsc,name,envit) \
                  mk_record_instance((tsc), RECORD_DESC(synvar), \
                        TR7_LIST2((tsc), name, envit))
#define IS_SYNVAR(tsc,item)   tr7_is_record_type((item), RECORD_DESC(synvar))
#define SYNVAR_NAME(item)     TR7_ITEM_RECORD((item),Synvar_Idx_Name)
#define SYNVAR_ENVIT(item)    TR7_ITEM_RECORD((item),Synvar_Idx_Envit)
/*
* get the name of the possibly pseudo symbol
*/
static tr7_t cpl_get_pseudo_symbol_name(cpl_t cpl, tr7_t item)
{
   while (!TR7_IS_SYMBOL(item)) {
      if (!IS_SYNVAR(cpl->tsc, item))
         return TR7_VOID;
      item = SYNVAR_NAME(item);
   }
   return item;
}
/*
* check if an item is a symbol for compiling.
* natural symbols are symbols obviously but also
* are members of instances of syntaxic expansions
*/
static int cpl_is_symbol(cpl_t cpl, tr7_t item)
{
   return TR7_IS_SYMBOL(item) || IS_SYNVAR(cpl->tsc, item);
}
/*
* check wether an item represents the given keyword
*/
static int cpl_is_the_keyword(cpl_t cpl, tr7_t item, tr7_t keyword)
{
   return TR7EQ(cpl_get_pseudo_symbol_name(cpl, item), keyword);
}
/*
* find the compiling environment item of symbol in a given environment
* if not found, either return NULL or create it if create isn't null
* return its value if found and its level as below:
*   0 means global level
*   1 means current compilation level
*   2 means upper compilation level
*   3 means upper upper compilation level
*  ...
* requires: cpl_is_symbol(symbol) != 0
*/
static int compile_get_cplenvit(cpl_t cpl, tr7_t symbol, tr7_pair_t *cplenvit, tr7_t env, int create)
{
   int depth;
   cpl_t it;
   tr7_pair_t envit;
   tr7_t plo;

   /* search locally bound symbol */
   for (it = cpl, depth = 1 ; it != NULL ; it = it->upper, depth++) {
      envit = tr7_assq_pair(symbol, it->vars);
      if (envit != NULL) {
         /* found local, return its compiling environment item and depth */
         *cplenvit = envit;
         return depth;
      }
   }

   /* is it a real symbol? */
   if (TR7_IS_SYMBOL(symbol)) {
      /* yes, get its envit */
      envit = environment_search_item(env, symbol, INT_MAX);
      if (envit == NULL && create)
         envit = environment_make_item(cpl->tsc, env, symbol, TR7_VOID, 1);
      *cplenvit = envit;
      return envit != NULL ? 0
                  : create ? cpl_oom(cpl)
                           : cpl_error_validity(cpl, "undefined variable", symbol);
   }

   /* comes from syntax expansion, get its compiling environment item */
   plo = SYNVAR_ENVIT(symbol);
   *cplenvit = TR7_TO_PAIR(plo);
   /* search if locally bound */
   for (it = cpl, depth = 1 ; it != NULL ; it = it->upper, depth++) {
      envit = tr7_memq_pair(plo, it->vars);
      if (envit != NULL)
         return depth;
   }
   return 0;
}
/*
* find the compiling environment item of symbol in a given environment
* return its value in found and its level as below:
*   0 means global level
*   1 means current compilation level
*   2 means upper compilation level
*   3 means upper upper compilation level
*  ...
* requires: cpl_is_symbol(symbol) != 0
*/
static int compile_search_cplenvit_env(cpl_t cpl, tr7_t symbol, tr7_pair_t *cplenvit, tr7_t env, int create)
{
   return compile_get_cplenvit(cpl, symbol, cplenvit, env, create);
}
/*
* find the compiling environment item of symbol in the compiling environment
* return its value in found and its level as below:
*   0 means global level
*   1 means current compilation level
*   2 means upper compilation level
*   3 means upper upper compilation level
*  ...
* requires: cpl_is_symbol(symbol) != 0
*/
static int compile_search_cplenvit(cpl_t cpl, tr7_t symbol, tr7_pair_t *cplenvit, int create)
{
   return compile_search_cplenvit_env(cpl, symbol, cplenvit, cpl->tsc->curenv, create);
}
/*
* find the symbol item in the compiling environment
* return its value in found and its level as below:
*   0 means global level
*   1 means current compilation level
*   2 means upper
*   3 means upper upper
*  ...
* requires: cpl_is_symbol(symbol) != 0
*/
static int compile_search_value(cpl_t cpl, tr7_t symbol, tr7_t *value)
{
   tr7_pair_t envit;
   int rc = compile_search_cplenvit(cpl, symbol, &envit, 1);
   if (rc >= 0)
      *value = TR7_PAIR_CDR(envit);
   return rc;
}
/*
* find the symbol item in the compiling environment
* compare its current value with the expected one
* returns 1 if the values matches or 0 otherwise
* requires: cpl_is_symbol(symbol) != 0
*/
static int compile_is_value(cpl_t cpl, tr7_t symbol, tr7_t expected)
{
   tr7_t value;
   int rc = compile_search_value(cpl, symbol, &value);
   return rc >= 0 && TR7EQ(value, expected);
}
/*
* declare a local variable of the given name
*/
static int compile_declare_var_local(cpl_t cpl, tr7_t symbol)
{
   int ivar = cpl->varcount++;
   tr7_t vardef = TR7_CONS2(cpl->tsc, symbol, TR7_FROM_INT(ivar));
   cpl->vars = TR7_CONS2(cpl->tsc, vardef, cpl->vars);
   return 0;
}
/*
* declare a global variable of the given name
*/
static int compile_declare_var_global(cpl_t cpl, tr7_t symbol)
{
   tr7_t value, env = cpl->tsc->curenv;
   tr7_pair_t envit = environment_search_item(env, symbol, 1);
   if (envit == NULL)
      envit = environment_create_item(cpl->tsc, env, symbol, TR7_VOID);
   if (envit != NULL) {
      value = TR7_PAIR_CDR(envit);
      if (!IS_BOX(value))
         value = environment_make_location(cpl->tsc, TR7_VOID, symbol);
      if (IS_BOX(value)) {
         TR7_PAIR_CDR(envit) = value;
         return 0;
      }
   }
   return cpl_oom(cpl);
}
/*
* declare a variable of the given name
*/
static int compile_declare_var(cpl_t cpl, tr7_t name)
{
   return (cpl->inlet ? compile_declare_var_local : compile_declare_var_global)(cpl, name);
}
/*
* compile 'quote'
*/
/*
* get the quoted value of item by resolving syntax variables
*/
static tr7_t cpl_get_quoted(cpl_t cpl, tr7_t item, tr7_pair_t guard)
{
   struct tr7_pair *iter, current = { .car = item, .cdr = (tr7_t)guard };

   /* naive loop guard TODO improve it */
   for (iter = guard ; iter != NULL ; iter = (tr7_pair_t)iter->cdr)
      if (TR7EQ(iter->car, item))
         return item;

   /* recursive resolution of variable captured by syntax */
   if (IS_SYNVAR(cpl->tsc, item))
      return cpl_get_quoted(cpl, SYNVAR_NAME(item), &current);

   if (TR7_IS_PAIR(item)) {
      /* resolution of pairs */
      tr7_t ocar = cpl_get_quoted(cpl, TR7_CAR(item), &current);
      tr7_t ocdr = cpl_get_quoted(cpl, TR7_CDR(item), &current);
      if (!TR7EQ(ocar, TR7_CAR(item)) || !TR7EQ(ocdr, TR7_CDR(item)))
         return tr7_cons(cpl->tsc, ocar, ocdr);
   }

   else if (TR7_IS_VECTOR(item)) {
      /* resolution of vectors */
      tr7_vector_t vre, vec = TR7_TO_VECTOR(item);
      tr7_uint_t i, j, n = TR7_VECTOR_LENGTH(vec);
      for (i = 0 ; i < n ; i++) {
         tr7_t a = TR7_VECTOR_ITEM(vec, i);
         tr7_t b = cpl_get_quoted(cpl, a, &current);
         if (!TR7EQ(a, b)) {
            /* an item of the vector changed, copy the vector */
            tr7_t resu = alloc_vector(cpl->tsc, n);
            if (TR7_IS_NIL(resu))
               break;
            vre = TR7_TO_VECTOR(resu);
            for (j = 0 ; j < i ; j++)
               TR7_VECTOR_ITEM(vre, j) = TR7_VECTOR_ITEM(vec, j);
            TR7_VECTOR_ITEM(vre, j++) = b;
            for ( ; j < n ; j++) {
               a = TR7_VECTOR_ITEM(vec, j);
               b = cpl_get_quoted(cpl, a, &current);
               TR7_VECTOR_ITEM(vre, j) = b;
            }
            return resu;
         }
      }
   }

   /* default resolution */
   return item;
}

static int compile_push_arg(cpl_t cpl)
{
   return cpl_emit_push_arg(cpl);
}

static int compile_quote(cpl_t cpl, tr7_t args)
{
   args = cpl_get_quoted(cpl, args, NULL);
   return cpl_emit_quote(cpl, args);
}

static int compile_quote_arg(cpl_t cpl, tr7_t args)
{
   int rc = compile_quote(cpl, args);
   if (rc >= 0)
      rc = compile_push_arg(cpl);
   return rc;
}

/*
* geting and setting variable
*/
static int compile_set_var(cpl_t cpl, tr7_t name, int create)
{
   int rc;
   tr7_t value;
   tr7_pair_t envit;

   if (!cpl_is_symbol(cpl, name))
      rc = cpl_error_not_a_symbol(cpl, name);
   else {
      rc = compile_search_cplenvit(cpl, name, &envit, create);
      if (rc >= 0) {
         value = TR7_PAIR_CDR(envit);
         if (rc > 0)
            rc = cpl_emit_set_var_local(cpl, TR7_TO_INT(value), rc - 1);
         else if (IS_BOX(value))
            rc = cpl_emit_set_var_global(cpl, value);
         else
            rc = cpl_error_validity(cpl, "can't set!", name);
      }
   }
   return rc;
}

static int compile_get_var(cpl_t cpl, tr7_t name)
{
   int rc;
   tr7_t value;
   tr7_pair_t envit;

   if (!cpl_is_symbol(cpl, name))
      rc = cpl_error_not_a_symbol(cpl, name);
   else {
      rc = compile_search_cplenvit(cpl, name, &envit, 1);
      if (rc >= 0) {
         value = TR7_PAIR_CDR(envit);
         if (IS_SYNTAX(value) || TR7_IS_TRANSFORM(value))
            rc = cpl_error_validity(cpl, "symbol is a syntax", name);
         else if (rc > 0)
            rc = cpl_emit_get_var_local(cpl, TR7_TO_INT(value), rc - 1);
         else if (IS_BOX(value))
            rc = cpl_emit_get_var_global(cpl, value);
         else
            rc = compile_quote(cpl, value);
      }
   }
   return rc;
}

static int compile_get_var_arg(cpl_t cpl, tr7_t name)
{
   int rc = compile_get_var(cpl, name);
   if (rc >= 0)
      rc = compile_push_arg(cpl);
   return rc;
}

static int cpl_sequence(cpl_t cpl, tr7_t args, int predecl)
{
   int rc;
   tr7_t expr, cdr;

   if (!TR7_IS_PAIR(args))
      return cpl_error_syntax(cpl, "not a list of expressions", args);
   expr = TR7_CAR(args);
   cdr = TR7_CDR(args);
   if (TR7_IS_NIL(cdr))
      return compile_expr(cpl, expr, predecl);

   for (;;) {
      rc = compile_expr(cpl, expr, predecl);
      if (rc < 0 || TR7_IS_NIL(cdr))
         return rc;
      if (!TR7_IS_PAIR(cdr))
         return cpl_error_syntax(cpl, "invalid list of expressions", args);
      expr = TR7_CAR(cdr);
      cdr = TR7_CDR(cdr);
   }
}

static int check_no_declare_sequence(cpl_t cpl, tr7_t seq)
{
   int rc = 0;
   tr7_t expr, symb, vvalue, trf;
   for ( ; rc >= 0 && TR7_IS_PAIR(seq) ; seq = TR7_CDR(seq)) {
      expr = TR7_CAR(seq);
again:
      if (!TR7_IS_PAIR(expr))
         break;
      symb = TR7_CAR(expr);
      if (!cpl_is_symbol(cpl, symb))
         continue;
      if (compile_search_value(cpl, symb, &vvalue) < 0)
         continue;
      if (TR7_IS_TRANSFORM(vvalue)) {
         rc = eval_syntax_rules_transform(cpl, vvalue, expr, &trf);
         if (rc < 0)
            break;
         expr = trf;
         goto again;
      }
      if (IS_SYNTAX(vvalue)) {
         expr = TR7_CDR(expr);
         switch(TO_SYNTAX(vvalue)) {
         case SYNTAXID(DEFINE):
         case SYNTAXID(DEFVAL):
         case SYNTAXID(DEFSYN):
         case SYNTAXID(DEFREC):
            rc = cpl_error_validity(cpl, "misplaced definition", TR7_CAR(seq));
            break;
         case SYNTAXID(BEGIN):
            if (!TR7_IS_PAIR(expr))
               rc = cpl_error_validity(cpl, "bad body", TR7_CAR(seq));
            else
               rc = check_no_declare_sequence(cpl, expr);
            break;
            break;
         default:
            break;
         }
      }
   }
   return rc;
}

static int cpl_sequence_nodecl(cpl_t cpl, tr7_t args)
{
   int rc = check_no_declare_sequence(cpl, args);
   return rc < 0 ? rc : cpl_sequence(cpl, args, 0);
}

static int compile_optional_sequence(cpl_t cpl, tr7_t args)
{
   return TR7_IS_NIL(args) ? 0 : cpl_sequence_nodecl(cpl, args);
}

static int compile_sequence(cpl_t cpl, tr7_t args)
{
   return TR7_IS_NIL(args)
      ? cpl_error_syntax(cpl, "empty sequence", args)
      : cpl_sequence_nodecl(cpl, args);
}

/*
*
*/
static int compile_lambda_body(cpl_t upper, tr7_t procname, tr7_t formals, tr7_t body, tr7_t *proc)
{
   int rc, nfor, dot;
   struct cpl_s lcpl;
   tr7_t name;

   /* initialize context */
   cpl_enter(&lcpl, upper);
   if (TR7_IS_VOID(procname) || compile_search_cplenvit(upper, procname, &lcpl.self, 0) <= 0)
      lcpl.self = NULL;

   /* declare formals */
   for (nfor = 0 ; TR7_IS_PAIR(formals) ; formals = TR7_CDR(formals)) {
      name = TR7_CAR(formals);
      if (!cpl_is_symbol(&lcpl, name))
         return cpl_leave(&lcpl, cpl_error_not_a_symbol(&lcpl, name));
      rc = compile_declare_var_local(&lcpl, name);
      if (rc < 0)
         return cpl_leave(&lcpl, rc);
      nfor++;
   }
   if (cpl_is_symbol(&lcpl, formals)) {
      dot = 1;
      rc = compile_declare_var_local(&lcpl, formals);
      if (rc < 0)
         return cpl_leave(&lcpl, rc);
      nfor++;
   }
   else if (TR7_IS_NIL(formals))
      dot = 0;
   else
      return cpl_leave(&lcpl,
               cpl_error_syntax(&lcpl, "invalid formals", formals));

   /* compile the body */
   rc = compile_body(&lcpl, body);
   if (rc >= 0)
      /* make the prog */
      rc = cpl_make_prog(&lcpl, procname, nfor, dot, proc);

   /* done */
   return cpl_leave(&lcpl, rc);
}
/*
*
*/
static int compile_lambda(cpl_t cpl, tr7_t name, tr7_t formals, tr7_t body)
{
   /* compile the body */
   tr7_t proc;
   int rc = compile_lambda_body(cpl, name, formals, body, &proc);
   return rc < 0 ? rc : cpl_emit_lambda(cpl, proc);
}
/*
* compile call expression list (expr ....)
*/
static int compile_exprlist0_args(cpl_t cpl, tr7_t exprlist)
{
   if (TR7_IS_NIL(exprlist))
      return 0;
   else if (!TR7_IS_PAIR(exprlist))
      return cpl_error_syntax(cpl, "improper expression list", exprlist);
   else {
      int n = compile_exprlist0_args(cpl, TR7_CDR(exprlist));
      int rc = n < 0 ? n : compile_expression_arg(cpl, TR7_CAR(exprlist));
      return rc < 0 ? rc : n + 1;
   }
}
/*
* compile call expression list ((_ expr ....) ... )
*/
static int compile_exprlist2_args(cpl_t cpl, tr7_t exprlist)
{
   if (TR7_IS_NIL(exprlist))
      return 0;
   else if (!TR7_IS_PAIR(exprlist))
      return cpl_error_syntax(cpl, "improper expression list", exprlist);
   else {
      int n = compile_exprlist2_args(cpl, TR7_CDR(exprlist));
      int rc = n;
      if (rc >= 0) {
         tr7_t expr = tr7_cadr_or_void(TR7_CAR(exprlist));
         if (TR7_IS_VOID(expr))
            rc = cpl_error_syntax(cpl, "expression expected", TR7_CAR(exprlist));
         else
            rc = compile_expression_arg(cpl, expr);
      }
      return rc < 0 ? rc : n + 1;
   }
}
/*
* compile call expression list ((_ _ [expr] ....) ... )
*/
static int compile_opt_exprlist3_args(cpl_t cpl, tr7_t exprlist)
{
   if (TR7_IS_NIL(exprlist))
      return 0;
   else if (!TR7_IS_PAIR(exprlist))
      return cpl_error_syntax(cpl, "improper expression list", exprlist);
   else {
      int n = compile_opt_exprlist3_args(cpl, TR7_CDR(exprlist));
      int rc = n;
      if (rc >= 0) {
         tr7_t expr = tr7_caddr_or_void(TR7_CAR(exprlist));
         if (!TR7_IS_VOID(expr)) {
            n++;
            rc = compile_expression_arg(cpl, expr);
         }
      }
      if (rc < 0)
         return rc;
      return n;
   }
}
/*
* compile '(let name (...) ...)' form
*/
static int compile_let_lambda(cpl_t cpl, tr7_t args)
{
   int rc, nargs;
   tr7_t procname, formals, body, decls, iter, bind, *pfor;
   cpl_vars_t savars;

   /* get the name of the lambda */
   procname = TR7_CAR(args);
   iter = TR7_CDR(args);
   if (!TR7_IS_PAIR(iter))
      return cpl_error_syntax(cpl, "no binding spec in let", args);

   /* get bindings of the lambda */
   decls = TR7_CAR(iter);
   if (!TR7_IS_PAIR(decls) && !TR7_IS_NIL(decls))
      return cpl_error_syntax(cpl, "invalid binding spec in let", args);

   /* get body of the lambda */
   body = TR7_CDR(iter);

   /* extract formals */
   rc = nargs = 0;
   formals = TR7_NIL;
   pfor = &formals;
   for (iter = decls ; rc >= 0 && TR7_IS_PAIR(iter) ; iter = TR7_CDR(iter)) {
      bind = TR7_CAR(iter);
      if (!TR7_IS_PAIR(bind))
         rc = cpl_error_syntax(cpl, "not a binding list", bind);
      else if (!cpl_is_symbol(cpl, TR7_CAR(bind)))
         rc = cpl_error_syntax(cpl, "not a symbol in binding", bind);
      else if (!TR7_IS_PAIR(TR7_CDR(bind)))
         rc = cpl_error_syntax(cpl, "no definition in binding", bind);
      else {
         nargs++;
         *pfor = TR7_CONS2(cpl->tsc, TR7_CAR(bind), TR7_NIL);
         pfor = &TR7_CDR(*pfor);
         if (!TR7_IS_NIL(TR7_CDDR(bind)))
            rc = cpl_error_syntax(cpl, "extra expression in binding", bind);
      }
   }
   if (rc >= 0 && !TR7_IS_NIL(iter))
      rc = cpl_error_syntax(cpl, "invalid binding end", iter);

   /* start compiling */
   cpl_vars_save(cpl, &savars);

   /* compile the arguments of the call to the lambda */
   if (rc >= 0)
      rc = compile_exprlist2_args(cpl, decls);

   /* declare, compile and set the lambda by its name */
   if (rc >= 0)
      rc = compile_declare_var_local(cpl, procname);
   if (rc >= 0)
      rc = compile_lambda(cpl, procname, formals, body);
   if (rc >= 0)
      rc = compile_set_var(cpl, procname, 0);

   /* call the lambda */
   if (rc >= 0)
      rc = cpl_emit_call(cpl, nargs);

   /* done */
   cpl_vars_restore(cpl, &savars);
   return rc;
}
/*
* compile (quote item)
*/
static int syn_quote(cpl_t cpl, tr7_t args)
{
   if (!TR7_IS_PAIR(args) || !TR7_IS_NIL(TR7_CDR(args)))
      return cpl_error_syntax(cpl, "invalid quote", args);
   return compile_quote(cpl, TR7_CAR(args));
}
/*
* type of item for quasiquoting
*/
typedef enum {
   cpl_qq_Other,
   cpl_qq_List,
   cpl_qq_Vector,
   cpl_qq_QuasiQuote,
   cpl_qq_Unquote,
   cpl_qq_UnquoteSplicing
} cpl_qq_type_t;
/*
* inspect the type of item
*/
static cpl_qq_type_t cpl_qq_get_type(cpl_t cpl, tr7_t item)
{
   /* is it a vector? */
   if (TR7_IS_VECTOR(item))
      return cpl_qq_Vector;

   /* is it a list? */
   if (!TR7_IS_PAIR(item))
      return cpl_qq_Other; /* not a list */

   /* is it a list of type (X Y) ? */
   if (TR7_IS_PAIR(TR7_CDR(item)) && TR7_IS_NIL(TR7_CDDR(item))) {
      item = TR7_CAR(item);

      /* is it UNQUOTE auxiliary keyword */
      if (cpl_is_the_keyword(cpl, item, SYMBOL(UNQUOTE)))
         return cpl_qq_Unquote;

      /* is it UNQUOTE_SPLICING auxiliary keyword */
      if (cpl_is_the_keyword(cpl, item, SYMBOL(UNQUOTE_SPLICING)))
         return cpl_qq_UnquoteSplicing;

      /* is it QUASIQUOTE syntax */
      if (cpl_is_symbol(cpl, item)) {
         if (compile_is_value(cpl, item, SYNTAX(QUASIQUOTE)))
            return cpl_qq_QuasiQuote;
      }
   }
   /* any list */
   return cpl_qq_List;
}
/*
* checks if item has unquoting for depth
* returns 1 if no unquoting is needed and item can be safely quoted
* returns 0 otherwise
*/
static int cpl_qq_can_quote(cpl_t cpl, tr7_t item, int depth)
{
   tr7_uint_t idx;
   switch (cpl_qq_get_type(cpl, item)) {
   default:
   case cpl_qq_Other:
      return 1;

   case cpl_qq_List:
      do {
         if (!cpl_qq_can_quote(cpl, TR7_CAR(item), depth))
            return 0;
         item = TR7_CDR(item);
      }
      while (cpl_qq_get_type(cpl, item) == cpl_qq_List);
      return cpl_qq_can_quote(cpl, item, depth);

   case cpl_qq_Vector:
      idx = TR7_LENGTH_VECTOR(item);
      while (idx) {
         idx--;
         if (!cpl_qq_can_quote(cpl, TR7_ITEM_VECTOR(item, idx), depth))
            return 0;
      }
      return 1;

   case cpl_qq_QuasiQuote:
      return cpl_qq_can_quote(cpl, TR7_CADR(item), depth + 1);

   case cpl_qq_Unquote:
   case cpl_qq_UnquoteSplicing:
      return depth ? cpl_qq_can_quote(cpl, TR7_CADR(item), depth - 1) : 0;
   }
}

static int compile_quasiquote_arg(cpl_t cpl, tr7_t item, int depth);

/*
* compile quasiquoting of a vector
*/
static int compile_qq_vector(cpl_t cpl, tr7_t item, int depth)
{
   int rc;
   tr7_uint_t idx, cnt;

   cnt = TR7_LENGTH_VECTOR(item);
   if (cnt == 0)
      rc = compile_quote(cpl, item);
   else {
      for (rc = 0, idx = cnt ; rc >= 0 && idx > 0 ;) {
         rc = compile_quasiquote_arg(cpl, TR7_ITEM_VECTOR(item, --idx), depth);
      }
      if (rc >= 0)
         rc = cpl_emit_proc(cpl, PROCID(VECTOR), cnt);
   }
   return rc;
}
/*
* compile quasiquoting of a list
*/
static int compile_qq_list(cpl_t cpl, tr7_t item, int depth)
{
   int rc;

   /* push arg tail as list */
   rc = compile_quasiquote_arg(cpl, TR7_CDR(item), depth);

   /* push the head */
   if (rc >= 0)
      rc = compile_quasiquote_arg(cpl, TR7_CAR(item), depth);

   /* emit the build operation */
   if (depth == 0 && cpl_qq_get_type(cpl, TR7_CAR(item)) == cpl_qq_UnquoteSplicing)
      rc = cpl_emit_proc(cpl, PROCID(APPEND), 2);
   else
      rc = cpl_emit_proc(cpl, PROCID(CONS), 2);
   return rc;
}
/*
* compile quasiquoting of an expression
*/
static int compile_qq(cpl_t cpl, tr7_t item, int depth)
{
   switch (cpl_qq_get_type(cpl, item)) {
   default:
   case cpl_qq_Other:
      return cpl_error_internal(cpl, "unexpected qq type", item); /* should not hapen */

   case cpl_qq_List:
      break;

   case cpl_qq_Vector:
      return compile_qq_vector(cpl, item, depth);

   case cpl_qq_QuasiQuote:
      depth++;
      break;

   case cpl_qq_Unquote:
   case cpl_qq_UnquoteSplicing:
      if (depth == 0)
         return compile_expression(cpl, TR7_CADR(item));
      depth--;
      break;
   }
   return compile_qq_list(cpl, item, depth);
}

static int compile_quasiquote_arg(cpl_t cpl, tr7_t item, int depth)
{
   int rc;
   if (cpl_qq_can_quote(cpl, item, depth))
      return compile_quote_arg(cpl, item);
   rc = compile_qq(cpl, item, depth);
   if (rc >= 0)
      rc = compile_push_arg(cpl);
   return rc;
}
/*
* compile (quasiquote ...)
*/
static int syn_quasiquote(cpl_t cpl, tr7_t args)
{
   tr7_t item = TR7_CAR(args);
   return cpl_qq_can_quote(cpl, item, 0)
      ? compile_quote(cpl, item)
      : compile_qq(cpl, item, 0);
}
/*
* hold unexpected unquotings
*/
static int syn_unquote(cpl_t cpl, tr7_t args)
{
   return cpl_error_syntax(cpl, "unexpected unquote", args);
}
static int syn_unquote_splicing(cpl_t cpl, tr7_t args)
{
   return cpl_error_syntax(cpl, "unexpected unquote-splicing", args);
}
/*
* compile (check-types ...)
*/
#if USE_TR7_EXTRA
static int syn_check_types(cpl_t cpl, tr7_t args)
{
#if HAS_CHECK_TYPES_NO
   cpl->tsc->no_check_types = TR7_IS_PAIR(args) && TR7_IS_FALSE(TR7_CAR(args));
#endif
   return 0;
}
#endif
/*
* compile (if cond then [else])
*/
static int syn_if(cpl_t cpl, tr7_t args)
{
   int rc, neg;
   tr7_t cond, blocthen, blocelse, cdr;
   label_t lblend, lblelse;

   /* get the condition */
   if (TR7_IS_NIL(args))
      /* bad if */
      return cpl_error_syntax(cpl, "empty if", args);
   cond = TR7_CAR(args);
   cdr = TR7_CDR(args);

   /* get then term */
   if (TR7_IS_NIL(cdr))
      /* bad if */
      return cpl_error_syntax(cpl, "if without consequent", args);
   blocthen = TR7_CAR(cdr);

   /* get optional else term */
   cdr = TR7_CDR(cdr);
   rc = cpl_make_label(cpl, &lblend);
   if (rc < 0)
      return rc;
   if (TR7_IS_NIL(cdr)) {
      blocelse = TR7_NIL;
      lblelse = lblend;
   }
   else {
      blocelse = TR7_CAR(cdr);
      if (!TR7_IS_NIL(TR7_CDR(cdr)))
         return cpl_error_syntax(cpl, "if with more than 2 consequents", args);
      rc = cpl_make_label(cpl, &lblelse);
      if (rc < 0)
         return rc;
   }
   /* optimize if not ... */
   neg = 1;
   while (TR7_IS_PAIR(cond)
      &&  cpl_is_symbol(cpl, TR7_CAR(cond))
      &&  compile_is_value(cpl, TR7_CAR(cond),  PROC(NOT))) {
      cond = TR7_CADR(cond);
      neg = 1 - neg;
   }
   /* compile condition */
   rc = compile_expression(cpl, cond);
   if (rc >= 0)
      rc = cpl_emit_if(cpl, lblelse, neg);

   /* compile then term */
   if (rc >= 0)
      rc = compile_expression(cpl, blocthen);

   /* compile optional else term */
   if (rc >= 0) {
      if (!TR7_IS_NIL(blocelse)) {
         rc = cpl_emit_goto(cpl, lblend);
         if (rc >= 0)
            rc = cpl_set_label(cpl, lblelse);
         if (rc >= 0)
            rc = compile_expression(cpl, blocelse);
      }
   }

   /* join execution threads */
   if (rc >= 0)
      rc = cpl_set_label(cpl, lblend);
   return rc;
}
/*
* when 'is_and' == 0 compile (or ...) else compile (and ... )
*/
static int syn_and_or(cpl_t cpl, tr7_t args, int is_and)
{
   int rc;
   tr7_t expr, cdr;
   label_t end;

   /* process empty case */
   if (TR7_IS_NIL(args))
      return compile_quote(cpl, is_and ? TR7_TRUE : TR7_FALSE);

   /* get first expression */
   if (!TR7_IS_PAIR(args))
      return cpl_error_syntax(cpl, is_and ? "bad and" : "bad or", args);
   expr = TR7_CAR(args);
   cdr = TR7_CDR(args);

   /* simple expression optimisation */
   if (TR7_IS_NIL(cdr))
      return compile_expression(cpl, expr);

   /* begin sequence of tests */
   rc = cpl_make_label(cpl, &end);

   /* compile expressions except last one */
   while (rc >= 0 && TR7_IS_PAIR(cdr)) {
      /* test one expression */
      rc = compile_expression(cpl, expr);
      if (rc >= 0)
         rc = cpl_emit_test(cpl, end, is_and);

      /* next expression */
      expr = TR7_CAR(cdr);
      cdr = TR7_CDR(cdr);
   }
   if (rc >= 0 && !TR7_IS_NIL(cdr))
      rc = cpl_error_syntax(cpl, is_and ? "bad and" : "bad or", args);

   /* compile last expression */
   if (rc >= 0)
      rc = compile_expression(cpl, expr);
   if (rc >= 0)
      rc = cpl_set_label(cpl, end);
   return rc;
}
/*
* compile (and ...)
*/
static int syn_and(cpl_t cpl, tr7_t args)
{
   return syn_and_or(cpl, args, 1);
}
/*
* compile (or ...)
*/
static int syn_or(cpl_t cpl, tr7_t args)
{
   return syn_and_or(cpl, args, 0);
}

static int syn_when_unless(cpl_t cpl, tr7_t args, int is_when)
{
   label_t end;
   int rc = compile_expression(cpl, TR7_CAR(args));
   if (rc >= 0)
      rc = cpl_make_label(cpl, &end);
   if (rc >= 0)
      rc = cpl_emit_if(cpl, end, is_when);
   if (rc >= 0)
      rc = compile_sequence(cpl, TR7_CDR(args));
   if (rc >= 0)
      rc = cpl_set_label(cpl, end);
   return rc;
}

static int syn_when(cpl_t cpl, tr7_t args)
{
   return syn_when_unless(cpl, args, 1);
}

static int syn_unless(cpl_t cpl, tr7_t args)
{
   return syn_when_unless(cpl, args, 0);
}

/*
* compiling either 'cond' or 'guard' handler
* when gvar is TR7_NIL, this is for compiling (cond ...)
* when gvar isn't TR7_NIL it represents the variable handling the exception
*/
static int compile_cond_or_guard(cpl_t cpl, tr7_t args, tr7_t gvar)
{
   tr7_t clause, cond, action, iter = args;
   label_t lblend, lblelse;
   int feedto, rc = cpl_make_label(cpl, &lblend);
   while (rc >= 0) {

      /* end of condition clauses without else? */
      if (TR7_IS_NIL(iter)) {
         /* yes, end of conditions without else */
         /* is for a guard? */
         if (!TR7_IS_VOID(gvar)) {
            /* yes, then rethrow */
            rc = compile_get_var_arg(cpl, gvar);
            if (rc >= 0)
               rc = cpl_emit_proc(cpl, PROCID(RAISECON), 1);
         }
         /* end of loop */
         break;
      }

      /* check if valid pair */
      if (!TR7_IS_PAIR(iter))
         return cpl_error_not_a_pair(cpl, iter);

      /* extract the clause and check it */
      clause = TR7_CAR(iter);
      rc = syntaxic_expansion(cpl, clause, &clause);
      if (rc < 0)
         return rc;
      if (!TR7_IS_PAIR(clause))
         return cpl_error_not_a_pair(cpl, clause);
      cond = TR7_CAR(clause);
      action = TR7_CDR(clause);

      /* compute else label */
      iter = TR7_CDR(iter);
      if (TR7_IS_NIL(iter) && TR7_IS_VOID(gvar))
         lblelse = lblend;
      else {
         rc = cpl_make_label(cpl, &lblelse);
         if (rc < 0)
            return rc;
      }

      /* feeds to => ? */
      feedto = TR7_IS_PAIR(action)
          && cpl_is_the_keyword(cpl, TR7_CAR(action), SYMBOL(FEED_TO));

      /* is it a else condition? */
      if (cpl_is_the_keyword(cpl, cond, SYMBOL(ELSE))) {
         /* yes, else clause, check it's the last */
         if (!TR7_IS_NIL(iter))
            return cpl_error_syntax(cpl, "else isn't last clause", args);
         if (feedto)
            return cpl_error_syntax(cpl, "else should not be followed by => (feed to)", args);
      }
      else {
         /* standard condition, compile it */
         if (rc >= 0)
            rc = compile_expression(cpl, cond);
         if (rc >= 0)
            rc = cpl_emit_cond(cpl, lblelse, feedto);
         if (rc < 0)
            return rc;
      }

      /* compile the if true sequence */
      if (!feedto)
         /* not feeding to */
         rc = compile_optional_sequence(cpl, action);
      else {
         /* with feed-to => */
         rc = compile_push_arg(cpl);
         if (rc >= 0)
            rc = compile_expression(cpl, TR7_CADR(action));
         if (rc >= 0)
            rc = cpl_emit_call(cpl, 1);
      }
      if (!TR7_IS_NIL(iter) || !TR7_IS_VOID(gvar)) {
         if (rc >= 0)
            rc = cpl_emit_goto(cpl, lblend);
         if (rc >= 0)
            rc = cpl_set_label(cpl, lblelse);
      }
   }
   if (rc >= 0)
      rc = cpl_set_label(cpl, lblend);
   return rc;
}
/*
* compile (cond clause ...)
*/
static int syn_cond(cpl_t cpl, tr7_t args)
{
   return compile_cond_or_guard(cpl, args, TR7_VOID);
}
/*
* implement compilation of (case EXPR (MATCH {=> EXPR | SEQ})...)
*/
static int syn_case(cpl_t cpl, tr7_t args)
{
   int rc;
   tr7_t expr, cases, clause, match, action;
   label_t lblend, lblelse;

   /* extract expr and cases */
   if (!TR7_IS_PAIR(args))
      return cpl_error_syntax(cpl, "bad case body", args);
   expr = TR7_CAR(args);
   cases = TR7_CDR(args);

   /* enter the block */
   rc = cpl_make_label(cpl, &lblend);

   /* compile expression */
   if (rc >= 0)
      rc = compile_expression(cpl, expr);

   while (rc >= 0 && !TR7_IS_NIL(cases)) {

      /* check if valid pair */
      if (!TR7_IS_PAIR(cases))
         return cpl_error_not_a_pair(cpl, cases);

      /* extract, expand and check the clause */
      clause = TR7_CAR(cases);
      rc = syntaxic_expansion(cpl, clause, &clause);
      if (rc < 0)
         return rc;
      if (!TR7_IS_PAIR(clause))
         return cpl_error_not_a_pair(cpl, clause);
      match = TR7_CAR(clause);
      action = TR7_CDR(clause);

      /* compute else label */
      cases = TR7_CDR(cases);
      if (TR7_IS_NIL(cases))
         lblelse = lblend;
      else {
         rc = cpl_make_label(cpl, &lblelse);
         if (rc < 0)
            return rc;
      }
      /* is it a else case? */
      if (cpl_is_the_keyword(cpl, match, SYMBOL(ELSE))) {
         /* yes, else case, check it's the last */
         if (!TR7_IS_NIL(cases))
            return cpl_error_syntax(cpl, "else isn't last case", args);
      }
      else
         /* standard case, compile it */
         rc = cpl_emit_not_case(cpl, match, lblelse);

      /* compile the action */
      if (rc >= 0) {
         if (!TR7_IS_PAIR(action)
          || !cpl_is_the_keyword(cpl, TR7_CAR(action), SYMBOL(FEED_TO))) {
            /* not feeding to */
            rc = compile_optional_sequence(cpl, action);
         }
         else if (!TR7_IS_PAIR(TR7_CDR(action)))
            rc = cpl_error_not_a_pair(cpl, TR7_CDR(action));
         else {
            /* with feed-to => */
            rc = compile_push_arg(cpl);
            if (rc >= 0)
               rc = compile_expression(cpl, TR7_CADR(action));
            if (rc >= 0)
               rc = cpl_emit_call(cpl, 1);
         }
         if (!TR7_IS_NIL(cases)) {
            /* compile going to end */
            if (rc >= 0)
               rc = cpl_emit_goto(cpl, lblend);
            if (rc >= 0)
               rc = cpl_set_label(cpl, lblelse);
         }
      }
   }
   /* end of case */
   if (rc >= 0)
      rc = cpl_set_label(cpl, lblend);
   return rc;
}
/*
* compile (set! name expr)
*/
static int syn_set(cpl_t cpl, tr7_t args)
{
   int rc;
   tr7_t name, expr, cdr;

   /* get name and expression */
   if (!TR7_IS_PAIR(args))
      return cpl_error_syntax(cpl, "empty set body", args);
   name = TR7_CAR(args);
   cdr = TR7_CDR(args);
   if (!TR7_IS_PAIR(cdr))
      return cpl_error_syntax(cpl, "bad set expression", args);
   expr = TR7_CAR(cdr);

   /* compile */
   rc =  compile_expression(cpl, expr);
   if (rc >= 0)
      rc = compile_set_var(cpl, name, 1);
   return rc;
}
/*
* declare the variables in order
*/
static int cpl_decl_vars(cpl_t cpl, tr7_t vars)
{
   int rc = 0;
   while (rc >= 0 && TR7_IS_PAIR(vars)) {
      tr7_t name = TR7_CAR(vars);
      vars = TR7_CDR(vars);
      rc = compile_declare_var(cpl, name);
   }
   if (rc >= 0 && !TR7_IS_NIL(vars))
      rc = compile_declare_var(cpl, vars);
   return rc;
}
/*
* set the variables in order
*/
static int cpl_set_vars(cpl_t cpl, tr7_t vars, int values)
{
   int rc = 0, len = tr7_unsafe_list_length(vars);
   if (values && len != 1) {
      if (len >= 0)
         rc = cpl_emit_mvalues(cpl, len, 0);
      else
         rc = cpl_emit_mvalues(cpl, -len, 1);
   }
   while (rc >= 0 && TR7_IS_PAIR(vars)) {
      tr7_t name = TR7_CAR(vars);
      vars = TR7_CDR(vars);
      rc = compile_set_var(cpl, name, 0);
   }
   if (rc >= 0 && !TR7_IS_NIL(vars))
      rc = compile_set_var(cpl, vars, 0);
   return rc;
}
/*
* compile many kinds of let: let, let*, letrec, letrec*, let-values, let*-values
* depending on value of 'options' (see CPL_LET_XXX constants)
*/
static int cpl_let(cpl_t cpl, tr7_t args, int options)
{
   tr7_t iter, term, epart, bindings, body;
   int rc = 0;
   cpl_vars_t oldvars, evalvars, setvars;

   /* get bindings, body and check them */
   if (!TR7_IS_PAIR(args))
      return cpl_error_syntax(cpl, "no binding spec in let", args);
   bindings = TR7_CAR(args);
   if (tr7_list_length(bindings) < 0)
      return cpl_error_syntax(cpl, "invalid binding spec in let", args);
   body = TR7_CDR(args);

   /* create the vars */
   cpl_vars_save(cpl, &oldvars);
   cpl_vars_save(cpl, &evalvars);
   if (!(options & CPL_LET_STAR))
      for (iter = bindings ; TR7_IS_PAIR(iter) ; iter = TR7_CDR(iter)) {
         term = TR7_CAR(iter);
         if (!TR7_IS_PAIR(term))
            return cpl_error_syntax(cpl, "bad binding", term);
         rc = cpl_decl_vars(cpl, TR7_CAR(term));
         if (rc < 0)
            return rc;
      }
   cpl_vars_save(cpl, &setvars);

   /* evaluate bindings */
   for (iter = bindings ; rc >= 0 && TR7_IS_PAIR(iter) ; iter = TR7_CDR(iter)) {
      /* get binding term */
      term = TR7_CAR(iter);
      if (!TR7_IS_PAIR(term))
         return cpl_error_syntax(cpl, "bad binding", term);
      epart = TR7_CDR(term);
      if (!TR7_IS_PAIR(epart))
         return cpl_error_syntax(cpl, "bad binding expression", epart);
      /* compile the expression */
      if ((options & (CPL_LET_REC | CPL_LET_STAR)) == 0)
         cpl_vars_restore(cpl, &evalvars);
      rc = compile_expression(cpl, TR7_CAR(epart));
      /* assign */
      if (rc >= 0) {
         switch (options & (CPL_LET_REC | CPL_LET_STAR)) {
         case CPL_LET_STAR: rc = cpl_decl_vars(cpl, TR7_CAR(term)); break;
         case 0: cpl_vars_restore(cpl, &setvars); break;
         }
         if (rc >= 0)
            rc = cpl_set_vars(cpl, TR7_CAR(term), options & CPL_LET_VALUES);
      }
   }

   /* compile body */
   if (rc >= 0)
      rc = compile_body(cpl, body);

   cpl_vars_restore(cpl, &oldvars);
   return rc < 0 ? rc : 0;
}

static int syn_let(cpl_t cpl, tr7_t args)
{
   if (TR7_IS_PAIR(args) && cpl_is_symbol(cpl, TR7_CAR(args)))
      return compile_let_lambda(cpl, args);

   return cpl_let(cpl, args, 0);
}

static int syn_letstar(cpl_t cpl, tr7_t args)
{
   return cpl_let(cpl, args, CPL_LET_STAR);
}

static int syn_letrec(cpl_t cpl, tr7_t args)
{
   return cpl_let(cpl, args, CPL_LET_REC);
}

static int syn_letrecstar(cpl_t cpl, tr7_t args)
{
   return syn_letrec(cpl, args);
}

static int syn_let_values(cpl_t cpl, tr7_t args)
{
   return cpl_let(cpl, args, CPL_LET_VALUES);
}

static int syn_letstarval(cpl_t cpl, tr7_t args)
{
   return cpl_let(cpl, args, CPL_LET_VALUES | CPL_LET_STAR);
}

static int syn_lambda(cpl_t cpl, tr7_t args)
{
   if (!TR7_IS_PAIR(args))
      return cpl_error_syntax(cpl, "bad lambda", args);
   return compile_lambda(cpl, TR7_VOID, TR7_CAR(args), TR7_CDR(args));
}

static int declare_define(cpl_t cpl, tr7_t args)
{
   tr7_t head, expr, name;

   if (!TR7_IS_PAIR(args))
      return cpl_error_syntax(cpl, "bad define term", args);

   head = TR7_CAR(args);
   expr = TR7_CDR(args);
   if (!TR7_IS_PAIR(expr))
      return cpl_error_syntax(cpl, "bad define expression", args);

   if (cpl_is_symbol(cpl, head))
      /* declare a variable */
      name = head;
   else {
      /* declare a lambda */
      if (!TR7_IS_PAIR(head))
         return cpl_error_validity(cpl, "bad define symbol", head);

      name = TR7_CAR(head);
      if (!cpl_is_symbol(cpl, name))
         return cpl_error_validity(cpl, "bad define symbol for lambda", name);
   }

   return compile_declare_var(cpl, name);
}

static int compile_define(cpl_t cpl, tr7_t args)
{
   int rc;
   tr7_t head, expr, name;

   head = TR7_CAR(args);
   expr = TR7_CDR(args);

   if (cpl_is_symbol(cpl, head)) {
      /* define a variable */
      name = head;
      rc = compile_expression(cpl, TR7_CAR(expr));
   }
   else {
      /* define a lambda */
      name = TR7_CAR(head);
      rc = compile_lambda(cpl, name, TR7_CDR(head), expr);
   }
   /* set the defined variable */
   if (rc >= 0)
      rc = compile_set_var(cpl, name, 0);
   return rc;
}

static int syn_define(cpl_t cpl, tr7_t args)
{
   int rc = declare_define(cpl, args);
   return rc < 0 ? rc : compile_define(cpl, args);
}

static int declare_define_values(cpl_t cpl, tr7_t args)
{
   int rc = 0;
   tr7_t vpart, epart, name;

   /* check */
   if (!TR7_IS_PAIR(args))
      return cpl_error_syntax(cpl, "bad binding", args);
   /* get the names */
   vpart = TR7_CAR(args);
   /* get the expression */
   epart = TR7_CDR(args);
   if (!TR7_IS_PAIR(epart))
      return cpl_error_syntax(cpl, "bad binding expression", epart);

   do {
      if (cpl_is_symbol(cpl, vpart)) {
         name = vpart;
         vpart = TR7_NIL;
      }
      else if (TR7_IS_PAIR(vpart) && cpl_is_symbol(cpl, TR7_CAR(vpart))) {
         name = TR7_CAR(vpart);
         vpart = TR7_CDR(vpart);
      }
      else
         return cpl_error_syntax(cpl, "malformed binding", TR7_CAR(args));
      rc = compile_declare_var(cpl, name);
   } while(rc >= 0 && !TR7_IS_NIL(vpart));
   return rc;
}

/*
* compile (define-values (vars ...) expr)
*/
static int compile_define_values(cpl_t cpl, tr7_t args)
{
   int rc, count, dotted;
   tr7_t vars, expr, iter;

   /* extract list of vars and expression */
   vars = TR7_CAR(args);
   expr = TR7_CADR(args); /* TODO: check CDDR */

   /* count variables */
   count = dotted = 0;
   for (iter = vars; TR7_IS_PAIR(iter); iter = TR7_CDR(iter))
      count++;
   if (!TR7_IS_NIL(iter)) {
      dotted = 1;
      count++;
   }

   /* compile expression */
   rc = compile_expression(cpl, expr);

   /* set multi-values */
   if (rc >= 0)
      rc = cpl_emit_mvalues(cpl, count, dotted);

   /* set values */
   for (iter = vars; rc >= 0 && TR7_IS_PAIR(iter); iter = TR7_CDR(iter))
      rc = compile_set_var(cpl, TR7_CAR(iter), 0);
   if (rc >= 0 && !TR7_IS_NIL(iter))
      rc = compile_set_var(cpl, iter, 0);
   return rc;
}

static int syn_define_values(cpl_t cpl, tr7_t args)
{
   int rc = declare_define_values(cpl, args);
   return rc < 0 ? rc : compile_define_values(cpl, args);
}

#if USE_SCHEME_CASE_LAMBDA
/*
* cpl of case-lambda
*/
static int syn_case_lambda(cpl_t cpl, tr7_t args)
{
   int rc;
   tr7_t cases, proc, *last, formals, body, clause;

   /* basic check */
   if (!TR7_IS_PAIR(args))
      return cpl_error_syntax(cpl, "malformed case-lambda", args);

   /* iterate over cases to build the list in order */
   last = &cases;
   cases = TR7_NIL;
   do {
      /* get one entry */
      clause = TR7_CAR(args);
      if (!TR7_IS_PAIR(clause))
         return cpl_error_syntax(cpl, "malformed case-lambda clause", clause);

      /* extract formal spec and lambda case body */
      formals = TR7_CAR(clause);
      body = TR7_CDR(clause);

      /* get in proc the corresponding program */
      rc = compile_lambda_body(cpl, TR7_VOID, formals, body, &proc);
      if (rc < 0)
         return rc;

      /* store the case */
      *last = TR7_CONS2(cpl->tsc, proc, TR7_NIL);
      last = &TR7_CDR(*last);

      /* iterate */
      args = TR7_CDR(args);
   } while(TR7_IS_PAIR(args));

   /* check end */
   if (!TR7_IS_NIL(args))
      return cpl_error_syntax(cpl, "ill terminated case-lambda", args);

   /* done */
   return cpl_emit_case_lambda(cpl, cases);
}
#endif
/*
* compile (do bindinds (test resu) commands ...)
(compile '(do ((i 0 (+ i 1))) ((>= i 10) i) (display i)))
tr7i -c "(display (compile '(do ((i 0 (+ i 1))) ((>= i 10) i) (display i))))(newline)"
*/
static int syn_do(cpl_t cpl, tr7_t args)
{
   int rc, nvars, nvars3;
   tr7_t it, bindings, commands, test, resu, name;
   label_t lblagain, lblend;
   cpl_vars_t savar;

   /* extract bindings specs */
   it = args;
   if (!TR7_IS_PAIR(it))
      return cpl_error_syntax(cpl, "ill formed do", args);
   bindings = TR7_CAR(it);

   /* extract commands */
   it = TR7_CDR(it);
   if (!TR7_IS_PAIR(it))
      return cpl_error_syntax(cpl, "ill formed do", args);
   commands = TR7_CDR(it);
   it = TR7_CAR(it);

   /* extract test and resu */
   if (!TR7_IS_PAIR(it))
      return cpl_error_syntax(cpl, "ill formed do", args);
   test = TR7_CAR(it);
   resu = TR7_CDR(it);

   /* init */
   cpl_vars_save(cpl, &savar);

   /* make the labels */
   rc = cpl_make_label(cpl, &lblagain);
   if (rc >= 0)
      rc = cpl_make_label(cpl, &lblend);

   /* compile initial values */
   if (rc >= 0)
      rc = nvars = compile_exprlist2_args(cpl, bindings);

   /* set the values */
   if (rc >= 0)
      rc = cpl_emit_mset(cpl, nvars);
   for (it = bindings ; rc >= 0 && TR7_IS_PAIR(it) ; it = TR7_CDR(it)) {
      name = TR7_CAAR(it);
      if (!cpl_is_symbol(cpl, name))
         return cpl_error_syntax(cpl, "do binding should be symbol", name);
      compile_declare_var_local(cpl, name);
      rc = compile_set_var(cpl, name, 0);
   }

   /* test */
   if (rc >= 0)
      rc = cpl_set_label(cpl, lblagain);
   if (rc >= 0)
      rc = compile_expression(cpl, test);
   if (rc >= 0)
      rc = cpl_emit_if_true(cpl, lblend);

   /* commands */
   if (rc >= 0)
      rc = compile_optional_sequence(cpl, commands);

   /* compile update values */
   if (rc >= 0)
      rc = nvars3 = compile_opt_exprlist3_args(cpl, bindings);

   /* update values */
   if (rc >= 0)
      rc = cpl_emit_mset(cpl, nvars3);
   for (it = bindings ; rc >= 0 && TR7_IS_PAIR(it) ; it = TR7_CDR(it))
      if (!TR7_IS_VOID(tr7_caddr_or_void(TR7_CAR(it))))
         rc = compile_set_var(cpl, TR7_CAAR(it), 0);

   /* loop */
   if (rc >= 0)
      rc = cpl_emit_goto(cpl, lblagain);

   /* result */
   if (rc >= 0)
      rc = cpl_set_label(cpl, lblend);
   if (rc >= 0)
      rc = compile_optional_sequence(cpl, resu);

   /* restore vars */
   cpl_vars_restore(cpl, &savar);
   return rc;
}
/*
* compile ((parameterize ((param value) ...) body)
*/
static int syn_parameterize(cpl_t cpl, tr7_t args)
{
   int rc, npars;
   tr7_t parlist, body, it, param, value, sub;

   /* get parameter list and body */
   if (!TR7_IS_PAIR(args))
      return cpl_error_syntax(cpl, "illformed parameterize", args);
   parlist = TR7_CAR(args);
   body = TR7_CDR(args);

   /* set the parameters */
   rc = npars = 0;
   for (it = parlist ; rc >= 0 && TR7_IS_PAIR(it) ; it = TR7_CDR(it)) {
      sub = TR7_CAR(it);
      if (!TR7_IS_PAIR(sub) || !TR7_IS_PAIR(TR7_CDR(sub)) || !TR7_IS_NIL(TR7_CDDR(sub)))
         return cpl_error_syntax(cpl, "illformed parameter set", sub);
      param = TR7_CAR(sub);
      value = TR7_CADR(sub);
      rc = compile_expression_arg(cpl, value);
      if (rc >= 0)
         rc = compile_expression(cpl, param);
      if (rc >= 0)
         rc = cpl_emit_parameterize(cpl);
      npars++;
   }
   if (rc >= 0 && !TR7_IS_NIL(it))
      return cpl_error_syntax(cpl, "illformed parameter list", parlist);

   /* execute the body with given parameters */
   if (rc >= 0)
      rc = compile_body(cpl, body);

   /* restore previous values */
   if (rc >= 0)
      rc = cpl_emit_end_parameterize(cpl, npars);
   return rc;
}
/*
* compiling 'guard'
*/
static int syn_guard(cpl_t cpl, tr7_t args)
{
   tr7_t handler, body, gvar, clauses;
   label_t lblend, lblguard;
   cpl_vars_t savars;
   int rc;

   /* get the handler and the body */
   if (!TR7_IS_PAIR(args))
      return cpl_error_not_a_pair(cpl, args);
   handler = TR7_CAR(args);
   body = TR7_CDR(args);

   /* get the variable and the clauses */
   if (!TR7_IS_PAIR(handler))
      return cpl_error_not_a_pair(cpl, handler);
   gvar = TR7_CAR(handler);
   clauses = TR7_CDR(handler);
   if (!cpl_is_symbol(cpl, gvar))
      return cpl_error_not_a_symbol(cpl, gvar);

   /* enter context of the handler */
   cpl_vars_save(cpl, &savars);

   /* creates the labels */
   rc = cpl_make_label(cpl, &lblend);
   if (rc >= 0)
      rc = cpl_make_label(cpl, &lblguard);

   /* compile the body within guard */
   if (rc >= 0)
      rc = cpl_emit_guard(cpl, lblguard);
   if (rc >= 0)
      rc = compile_body(cpl, body);
   if (rc >= 0)
      rc = cpl_emit_end_guard(cpl, lblend);

   /* compile the handler */
   if (rc >= 0)
      rc = cpl_set_label(cpl, lblguard);
   if (rc >= 0)
      rc = compile_declare_var_local(cpl, gvar);
   if (rc >= 0)
      rc = compile_set_var(cpl, gvar, 0);
   if (rc >= 0)
      rc = compile_cond_or_guard(cpl, clauses, gvar);
   if (rc >= 0)
      rc = cpl_set_label(cpl, lblend);

   /* restore vars */
   cpl_vars_restore(cpl, &savars);
   return rc;
}
#if USE_SCHEME_LAZY
/*
* common compilation of 'delay' and 'delay-force'
*/
static int compile_delay(cpl_t cpl, tr7_t args, int force)
{
   int rc;
   tr7_t proc;
   struct cpl_s lcpl;

   if (!TR7_IS_PAIR(args))
      return cpl_error_not_a_pair(cpl, args);

   cpl_enter(&lcpl, cpl);
   rc = compile_expression(&lcpl, TR7_CAR(args));
   if (rc >= 0)
      rc = cpl_make_prog(&lcpl, TR7_VOID, 0, 0, &proc);
   if (rc >= 0)
      rc = cpl_leave(&lcpl, rc);
   if (rc >= 0)
      rc = cpl_emit_delay(cpl, force, proc);
   return rc;
}

static int syn_delay(cpl_t cpl, tr7_t args)
{
   return compile_delay(cpl, args, 0);
}

static int syn_delay_force(cpl_t cpl, tr7_t args)
{
   return compile_delay(cpl, args, 1);
}
#endif

/*
* validates and declare variables of (define-record-type ...)
*/
static int declare_define_record_type(cpl_t cpl, tr7_t args)
{
   int rc, len, cons_count, fld_count;
   tr7_t iter;
   tr7_t type, typname;
   tr7_t constr, const_name, const_args;
   tr7_t predname;
   tr7_t fields, flddef, fname, faccname, fmutname, ofld;

   /** extract name, constr, predname and fields */
   len = cpl_check_list(cpl, args, 3, 0);
   if (len < 0)
      return len;
   type = TR7_CAR(args);
   iter = TR7_CDR(args);
   constr = TR7_CAR(iter);
   iter = TR7_CDR(iter);
   predname = TR7_CAR(iter);
   fields = TR7_CDR(iter);
   fld_count = len - 3;

   /* scan type to get typname and parent */
   if (!TR7_IS_PAIR(type))
      typname = type;
   else {
      len = cpl_check_list(cpl, type, 2, 2);
      if (len < 0)
         return len;
      typname = TR7_CAR(type);
   }

   /* scan the constructor */
   const_args = TR7_NIL;
   cons_count = 0;
   if (TR7_IS_FALSE(constr))
      const_name = TR7_FALSE;
   else if (!TR7_IS_PAIR(constr))
      const_name = constr;
   else {
      len = cpl_check_list(cpl, constr, 1, 0);
      if (len < 0)
         return len;
      const_name = TR7_CAR(constr);
      const_args = TR7_CDR(constr);
      cons_count = len - 1;
   }

   /* validate and declare symbols */
   if (!cpl_is_symbol(cpl, typname))
      return cpl_error_not_a_symbol(cpl, typname);
   rc = compile_declare_var(cpl, typname);
   if (rc < 0)
      return rc;
   if (!TR7_IS_FALSE(const_name)) {
      if (!cpl_is_symbol(cpl, const_name))
         return cpl_error_not_a_symbol(cpl, const_name);
      rc = compile_declare_var(cpl, const_name);
      if (rc < 0)
         return rc;
   }
   if (!TR7_IS_FALSE(predname)) {
      if (!cpl_is_symbol(cpl, predname))
         return cpl_error_not_a_symbol(cpl, predname);
      rc = compile_declare_var(cpl, predname);
      if (rc < 0)
         return rc;
   }

   /* validate and declare fields */
   if (fld_count > 0) {
      for (iter = fields ; TR7_IS_PAIR(iter) ; ) {
         flddef = TR7_CAR(iter);
         iter = TR7_CDR(iter);

         len = cpl_check_list(cpl, flddef, 2, 3);
         if (len < 0)
            return len;

         fname =  TR7_CAR(flddef);
         if (!cpl_is_symbol(cpl, fname))
            return cpl_error_not_a_symbol(cpl, fname);
         ofld = tr7_assq(fname, iter);
         if (!TR7_IS_FALSE(ofld))
            return cpl_error_validity(cpl, "duplicated field name", ofld);

         flddef = TR7_CDR(flddef);
         faccname =  TR7_CAR(flddef);
         if (!cpl_is_symbol(cpl, faccname))
            return cpl_error_not_a_symbol(cpl, faccname);
         rc = compile_declare_var(cpl, faccname);
         if (rc < 0)
            return rc;

         if (len == 3) {
            fmutname = TR7_CADR(flddef);
            if (!cpl_is_symbol(cpl, fmutname))
               return cpl_error_not_a_symbol(cpl, fmutname);
            rc = compile_declare_var(cpl, fmutname);
            if (rc < 0)
               return rc;
         }
      }
   }

   /* validate constructor arguments */
   if (cons_count > 0) {
      for (iter = const_args ; TR7_IS_PAIR(iter) ;) {
         fname = TR7_CAR(iter);
         iter = TR7_CDR(iter);

         if (!cpl_is_symbol(cpl, fname))
            return cpl_error_not_a_symbol(cpl, fname);

         if (tr7_memq_pair(fname, iter) != NULL)
            return cpl_error_validity(cpl, "duplicated name in constructor", fname);
      }
   }
   return 0;
}
/*
* compile the symbols of define-record-type checked
*/
static int compile_define_record_type(cpl_t cpl, tr7_t args)
{
   int rc, valcount;
   tr7_t iter, flag;
   tr7_t type, typname, parent;
   tr7_t constr, const_name, const_args;
   tr7_t predname;
   tr7_t fields, flddef, fspec, fname;
   tr7_t *pend;


   /* scan type to get typname and parent */
   iter = args;
   type = TR7_CAR(iter);
   if (!TR7_IS_PAIR(type)) {
      typname = type;
      parent = TR7_FALSE;
   }
   else {
      typname = TR7_CAR(type);
      parent = TR7_CADR(type);
   }

   /* init record descriptor */
   fspec = TR7_CONS2(cpl->tsc, typname, TR7_NIL);
   pend = &TR7_CDR(fspec);
   valcount = 1;

   /* scan the constructor */
   const_args = TR7_FALSE;
   iter = TR7_CDR(iter);
   constr = TR7_CAR(iter);
   if (TR7_IS_FALSE(constr))
      const_name = TR7_FALSE;
   else {
      if (!TR7_IS_PAIR(constr)) {
         const_name = constr;
         const_args = TR7_TRUE;
      }
      else {
         const_name = TR7_CAR(constr);
         const_args = TR7_CDR(constr);
      }
      valcount++;
   }
   *pend = TR7_CONS2(cpl->tsc, const_args, TR7_NIL);
   pend = &TR7_CDR(*pend);

   /* scan the predicate */
   iter = TR7_CDR(iter);
   predname = TR7_CAR(iter);
   if (TR7_IS_FALSE(predname))
      flag = TR7_FALSE;
   else {
      flag = TR7_TRUE;
      valcount++;
   }
   *pend = TR7_CONS2(cpl->tsc, flag, TR7_NIL);
   pend = &TR7_CDR(*pend);

   /* scan the fields */
   fields = TR7_CDR(iter);
   iter = fields;

   while (!TR7_IS_NIL(iter)) {
      /* get the field descriptor */
      flddef = TR7_CAR(iter);
      iter = TR7_CDR(iter);

      /* extract field name and check if mutable */
      fname =  TR7_CAR(flddef);
      if (TR7_IS_NIL(TR7_CDDR(flddef))) {
         flag = TR7_FALSE;
         valcount++;
      }
      else {
         flag = TR7_TRUE;
         valcount += 2;
      }
      *pend = TR7_CONS2(cpl->tsc, TR7_CONS2(cpl->tsc, flag, fname), TR7_NIL);
      pend = &TR7_CDR(*pend);
   }

   rc = compile_expression(cpl, parent);
   if (rc >= 0)
      rc = cpl_emit_define_record(cpl, fspec, valcount);

   /* assign type */
   if (rc >= 0)
      rc = compile_set_var(cpl, typname, 1);

   /* assign constructor */
   if (rc >= 0 && !TR7_IS_FALSE(const_name))
      rc = compile_set_var(cpl, const_name, 1);

   /* assign predicate */
   if (rc >= 0 && !TR7_IS_FALSE(predname))
      rc = compile_set_var(cpl, predname, 1);

   /* assign fields */
   iter = TR7_CDDDR(fspec);
   while (rc >= 0 && !TR7_IS_NIL(iter)) {
      flddef = tr7_assq(TR7_CDAR(iter), fields);
      flddef = TR7_CDR(flddef);
      rc = compile_set_var(cpl, TR7_CAR(flddef), 1);
      if (rc >= 0 && !TR7_IS_NIL(TR7_CDR(flddef)))
         rc = compile_set_var(cpl, TR7_CADR(flddef), 1);
      iter = TR7_CDR(iter);
   }

   /* end */
   return rc;
}

static int syn_define_record_type(cpl_t cpl, tr7_t args)
{
   int rc = declare_define_record_type(cpl, args);
   return rc < 0 ? rc : compile_define_record_type(cpl, args);
}

/*
* The structure records items needed
* for expansion of the transformer
*/
struct trfmr {
   tr7_engine_t tsc;            /* the main scheme */
   cpl_t cpl;                   /* main compiling context */
   cpl_t cpltrf;                /* compiling context of the transform if any */
   tr7_t dvars;                 /* variables when definition occurs */
   tr7_transform_t trf;         /* original transform */
   tr7_t ellipsis;              /* given ellipsis */
   tr7_t literals;              /* given literals */
   tr7_t variables;             /* instantiation of matched variables */
   tr7_t instance;              /* local bindings */
#if HAS_GREEDY_SYNTAX
   uint8_t notgreedy;           /* not greedy */
#endif
   int indexes[TRANSFORM_DEPTH_MAX];              /* indexes */
};
/*
* search the binding of a definition
*/
static tr7_pair_t syntax_search_in_definition(struct trfmr *trf, tr7_t symbol)
{
   tr7_pair_t envit;
   tr7_t vars;
   if (trf->cpltrf == NULL)
      envit = environment_find_item(trf->trf->env, symbol);
   else {
      vars = trf->cpltrf->vars;
      trf->cpltrf->vars = trf->dvars;
      compile_get_cplenvit(trf->cpltrf, symbol, &envit, trf->cpltrf->tsc->curenv, 0);
      trf->cpltrf->vars = vars;
   }
   return envit;
}

static tr7_pair_t syntax_search_for_expression(struct trfmr *trf, tr7_t symbol)
{
   tr7_pair_t envit;
   compile_get_cplenvit(trf->cpl, symbol, &envit, trf->cpl->tsc->curenv, 0);
   return envit;
}
/*
* delivers for the given pseudo-symbol 'symbol' the pseudo-symbol
* representing it for the current syntax instantiation given by 'trf'
*/
static tr7_t syntax_get_symbol(struct trfmr *trf, tr7_t symbol)
{
   tr7_t resu, inst, vars, symb;
   tr7_pair_t pair;

   /* search in the syntax instantiation list */
   for (inst = trf->instance ; TR7_IS_PAIR(inst) ; inst = TR7_CDR(inst)) {
      resu = TR7_CAR(inst);
      symb = SYNVAR_NAME(resu);
      if (TR7EQ(symbol, symb))
         return resu;
   }
   /* not found, create the symbol, link it and return it */
   if (trf->cpltrf == NULL)
      compile_search_cplenvit_env(trf->cpl, symbol, &pair, trf->trf->env, 1);
   else {
      vars = trf->cpltrf->vars;
      trf->cpltrf->vars = trf->dvars;
      compile_search_cplenvit(trf->cpltrf, symbol, &pair, 1);
      trf->cpltrf->vars = vars;
   }
   /* create the pseudo-symbol and link it */
   resu = MAKE_SYNVAR(trf->tsc, symbol, TR7_FROM_PAIR(pair));
   inst = TR7_CONS2(trf->tsc, resu, trf->instance);
   trf->instance = inst;
   return resu;
}

static void syntax_capture(struct trfmr *trf, tr7_t symbol, tr7_t value, int capture_depth)
{
   tr7_pair_t pair;
   int depth;
   tr7_t x, y, i;

   /* get the capture data */
   pair = tr7_assq_pair(symbol, trf->variables);
   if (pair != NULL)
      x = TR7_PAIR_CDR(pair); /* skip recorded depth */
   else {
      /* not found, add it */
      x =  TR7_LIST1(trf->tsc, TR7_FROM_INT(capture_depth));
      y = TR7_CONS2(trf->tsc, symbol, x);
      trf->variables = tr7_cons(trf->tsc, y, trf->variables);
   }

   /* capture the value */
   for (depth = 0 ; depth < capture_depth - 1 ; depth++) {
      i = TR7_FROM_INT(trf->indexes[depth]);
      y = tr7_assq(i, TR7_CDR(x));
      if (TR7_IS_FALSE(y)) {
         y = TR7_LIST1(trf->tsc, i);
         TR7_CDR(x) = TR7_CONS2(trf->tsc, y, TR7_CDR(x));
      }
      x = y;
   }
   /* append capture at end */
   if (!TR7_IS_VOID(value)) {
      while(!TR7_IS_NIL(TR7_CDR(x)))
         x = TR7_CDR(x);
      TR7_CDR(x) = TR7_LIST1(trf->tsc, value);
   }
}

static int syntax_match_pattern(struct trfmr *trf, tr7_t pattern, tr7_t scan, int capture_depth);
static int syntax_match_symbol(struct trfmr *trf, tr7_t pattern, tr7_t scan, int capture_depth);
static int syntax_match_pair(struct trfmr *trf, tr7_t pattern, tr7_t scan, int capture_depth);
static int syntax_match_vector(struct trfmr *trf, tr7_t pattern, tr7_t scan, int capture_depth);

/*
* Matches a pattern with current scan, the capture depth is negative when no capture
* is required or is the posistive or nul count of ellipsis of the match.
* Returns 1 if matching, 0 if not matching, a negative code on error
*/
static int syntax_match_pattern(struct trfmr *trf, tr7_t pattern, tr7_t scan, int capture_depth)
{
   /* match a symbols */
   if (cpl_is_symbol(trf->cpl, pattern))
      return syntax_match_symbol(trf, pattern, scan, capture_depth);

   /* match a lists */
   if (TR7_IS_PAIR(pattern))
      return syntax_match_pair(trf, pattern, scan, capture_depth);

   /* match a vectors */
   if (TR7_IS_VECTOR(pattern))
      return syntax_match_vector(trf, pattern, scan, capture_depth);

   /* match others */
   return tr7_equal(pattern, scan);
}

#if TRACE_SYNTAX
static int hook_syntax_match_pattern(struct trfmr *trf, tr7_t pattern, tr7_t scan, int capture_depth)
{
   int r = syntax_match_pattern(trf, pattern, scan, capture_depth);
   log_str(trf->tsc, "\n---------------------   matching depth=");
   log_item(trf->tsc, TR7_FROM_INT(capture_depth));
   log_str(trf->tsc, "\n");
   log_str(trf->tsc, "      matching expr:    ");
   log_item(trf->tsc, scan);
   log_str(trf->tsc, "\n");
   log_str(trf->tsc, "      matching pattern: ");
   log_item(trf->tsc, pattern);
   log_str(trf->tsc, "\n");
   log_str(trf->tsc, r ? "      YES  " : "      NO\n");
   if (r) {
      log_item(trf->tsc, trf->variables);
      log_str(trf->tsc, "\n");
   }
   return r;
}
#define syntax_match_pattern hook_syntax_match_pattern
#endif

static int syntax_match_symbol(struct trfmr *trf, tr7_t pattern, tr7_t scan, int capture_depth)
{
   tr7_pair_t pair;

   /* case of underscore: always fit without capture */
   if (cpl_is_the_keyword(trf->cpl, pattern, SYMBOL(UNDERSCORE)))
      return 1;

   /* literal ? */
   pair = tr7_memq_pair(pattern, trf->literals); /* search in literals */
   if (pair != NULL) {
      /* if found, is a literal */
      tr7_pair_t locx = syntax_search_in_definition(trf, pattern);
      if (locx == NULL) {
         /* literal unbounded match only unbounded same symbol */
         return TR7EQ(pattern, scan) /* if true, scan is a symbol */
             && NULL == syntax_search_for_expression(trf, scan);
      }
      if (cpl_is_symbol(trf->cpl, scan)) {
         tr7_pair_t locy = syntax_search_for_expression(trf, scan);
         return locx == locy
             || (locy != NULL && TR7_PAIR_CDR(locx) == TR7_PAIR_CDR(locy));
      }
      return 0;
   }

   /* matches a non literal */
   if (capture_depth >= 0)
      syntax_capture(trf, pattern, scan, capture_depth);
   return 1;
}

static void syntax_match_capture_void(struct trfmr *trf, tr7_t pattern, int capture_depth)
{
   /* match a vectors */
   if (TR7_IS_VECTOR(pattern))
      pattern = tr7_vector_to_list(trf->tsc, pattern);

   /* match a lists */
   if (TR7_IS_PAIR(pattern)) {
      while (TR7_IS_PAIR(pattern)) {
         if (TR7_IS_PAIR(TR7_CDR(pattern)) && TR7EQ(TR7_CADR(pattern), trf->ellipsis)) {
            trf->indexes[capture_depth] = 0;
            syntax_match_capture_void(trf, TR7_CAR(pattern), capture_depth + 1);
            pattern = TR7_CDDR(pattern);
         }
         else {
            syntax_match_capture_void(trf, TR7_CAR(pattern), capture_depth);
            pattern = TR7_CDR(pattern);
         }
      }
   }

   /* match a symbols */
   if (!cpl_is_symbol(trf->cpl, pattern))
      return;

   /* case of underscore */
   if (cpl_is_the_keyword(trf->cpl, pattern, SYMBOL(UNDERSCORE)))
      return;

   /* literal ? */
   if (NULL != tr7_memq_pair(pattern, trf->literals))
      return;

   /* get the capture data */
   syntax_capture(trf, pattern, TR7_VOID, capture_depth);
}

static int syntax_match_pair(struct trfmr *trf, tr7_t pattern, tr7_t scan, int capture_depth)
{
   int found;
   tr7_t oriscan, iscan, pat;

   /* match list */
   do {
      /* get the pattern head */
      pat = TR7_CAR(pattern);
      pattern = TR7_CDR(pattern);
      /* check if ellipsis follows */
      if (!TR7_IS_PAIR(pattern) || !TR7EQ(TR7_CAR(pattern), trf->ellipsis)) {
         /* no ellipsis, check if pair matching */
         if (!TR7_IS_PAIR(scan) || !syntax_match_pattern(trf, pat, TR7_CAR(scan), capture_depth))
            return 0;
         scan = TR7_CDR(scan);
      } else {
         /* ellipsis... advance pattern */
         pattern = TR7_CDR(pattern);
         /* match longest prefix, without capturing */
         iscan = oriscan = scan;
         found = syntax_match_pattern(trf, pattern, iscan, -1);
#if HAS_GREEDY_SYNTAX
         if (!found || !trf->notgreedy)
#endif
         while (TR7_IS_PAIR(iscan) && syntax_match_pattern(trf, pat, TR7_CAR(iscan), -1)) {
            iscan = TR7_CDR(iscan);
            if (syntax_match_pattern(trf, pattern, iscan, -1) > 0) {
               found = 1;
               scan = iscan;
#if HAS_GREEDY_SYNTAX
               if (trf->notgreedy)
                  break;
#endif
            }
         }
         if (!found)
            return 0;
         if (capture_depth >= 0) {
            /* check depth */
            if (capture_depth >= (int)(sizeof trf->indexes / sizeof *trf->indexes))
               return 0;
            /* capturing... */
            trf->indexes[capture_depth] = 0;
            if (TR7EQ(oriscan, scan)) {
               /* empty case */
               syntax_match_capture_void(trf, pat, capture_depth + 1);
            } else {
               for (iscan = oriscan ; !TR7EQ(iscan, scan) ; iscan = TR7_CDR(iscan)) {
                  syntax_match_pattern(trf, pat, TR7_CAR(iscan), capture_depth + 1);
                  trf->indexes[capture_depth]++;
               }
            }
         }
      }
   } while (TR7_IS_PAIR(pattern));
   return syntax_match_pattern(trf, pattern, scan, capture_depth);
}

static int syntax_match_vector(struct trfmr *trf, tr7_t pattern, tr7_t scan, int capture_depth)
{
   if (!TR7_IS_VECTOR(scan))
      return 0;
   else {
      tr7_t lpat = tr7_vector_to_list(trf->tsc, pattern);
      tr7_t lscan = tr7_vector_to_list(trf->tsc, scan);
      return syntax_match_pattern(trf, lpat, lscan, capture_depth);
   }
}

static int syntax_apply_symbol(struct trfmr *trf, tr7_t template, int depth, tr7_t *result);
static int syntax_apply_pair(struct trfmr *trf, tr7_t template, int depth, tr7_t *result);
static int syntax_apply_vector(struct trfmr *trf, tr7_t template, int depth, tr7_t *result);

/*
* expansion status of applying templates are:
*  TR7_APPLY_NONE      no expansion was possible
*  TR7_APPLY_ANY       expansion was made but not on capture
*  TR7_APPLY_VARIABLE  expansion was made on a capture
*  TR7_APPLY_MIXTE     expansion was made with both capture and not capture
*/
#define TR7_APPLY_NONE       0
#define TR7_APPLY_ANY        1
#define TR7_APPLY_VARIABLE   2
#define TR7_APPLY_MIXTE      3

/*
* apply the syntax captures handled in 'trf' to the given template expression.
* depth is the ellipsis depth
* the resulting expansion is put in result.
* returns negative value on error or expansion status
*/
static int syntax_apply_template(struct trfmr *trf, tr7_t template, int depth, tr7_t *result)
{
   *result = TR7_NIL;

   if (cpl_is_symbol(trf->cpl, template))
      return syntax_apply_symbol(trf, template, depth, result);

   if (TR7_IS_PAIR(template))
      return syntax_apply_pair(trf, template, depth, result);

   if (TR7_IS_VECTOR(template))
      return syntax_apply_vector(trf, template, depth, result);

   /* transform of others is just a "copy" */
   *result = template;
   return TR7_APPLY_ANY;
}

#if TRACE_SYNTAX
static int hook_syntax_apply_template(struct trfmr *trf, tr7_t template, int depth, tr7_t *result)
{
   int r = syntax_apply_template(trf, template, depth, result);
   log_str(trf->tsc, "\n---------------------   transform depth=");
   log_item(trf->tsc, TR7_FROM_INT(depth));
   log_str(trf->tsc, "\n      from: ");
   log_item(trf->tsc, template);
   log_str(trf->tsc, "\n");
   log_str(trf->tsc, "      to:   ");
   log_item(trf->tsc, *result);
   log_str(trf->tsc, "\n");
   log_str(trf->tsc, r > 1 ? "      CAPTURE " : "      STOPPED ");
   log_item(trf->tsc, TR7_FROM_INT(r));
   log_str(trf->tsc, "\n");
   return r;
}
#define syntax_apply_template hook_syntax_apply_template
#endif

/*
* apply the syntax captures handled in 'trf' to the given symbol.
* the resulting expansion is put in result.
* returns negative value on error or expansion status
*/
static int syntax_apply_symbol(struct trfmr *trf, tr7_t symbol, int depth, tr7_t *result)
{
   tr7_pair_t pair;
   int d, j, idth;
   tr7_t x;

   /* captured item? variable? */
   pair = tr7_assq_pair(symbol, trf->variables);
   if (pair == NULL) {
      /* not a capture. literal ? */
      pair = tr7_memq_pair(symbol, trf->literals); /* search in literals */
      if (pair != NULL)
         x = symbol; /* yes -> the literal */
      else
         x = syntax_get_symbol(trf, symbol);
      *result = x; /* yes -> its value */
      return TR7_APPLY_ANY;
   }

   /* the captured item */
   x = TR7_PAIR_CDR(pair);
   idth = TR7_TO_INT(TR7_CAR(x));
   x = TR7_CDR(x);
   for(d = 0; d < idth - 1; d++) {
      if (d >= depth)
         return TR7_APPLY_NONE;
      j = trf->indexes[d];
      pair = tr7_assq_pair(TR7_FROM_INT(j), x);
      if (pair == NULL)
         return TR7_APPLY_NONE;
      x = TR7_PAIR_CDR(pair);
   }
   j = idth ? trf->indexes[d] : 0;
   while (j && TR7_IS_PAIR(x)) {
      x = TR7_CDR(x);
      j--;
   }
   if (!TR7_IS_PAIR(x))
      return TR7_APPLY_NONE;
   *result = TR7_CAR(x);
   return idth == depth ? TR7_APPLY_VARIABLE : TR7_APPLY_ANY;
}

/*
* apply the syntax captures handled in 'trf' to the given pair.
* the resulting expansion is put in result.
* returns negative value on error or expansion status
*/
static int syntax_apply_pair(struct trfmr *trf, tr7_t pair, int depth, tr7_t *result)
{
   int rc, d, substatus, status, updepth, trig;
   tr7_t x, y, item, next, head, *tail;

   /* special case of an ellipsis at start */
   item = TR7_CAR(pair);
   next = TR7_CDR(pair);
   if (TR7EQ(item, trf->ellipsis)) {
      if (!TR7_IS_PAIR(next) || !TR7_IS_NIL(TR7_CDR(next)))
         return cpl_error_validity(trf->cpl, "wrong ellipsis escape", pair);
      *result = TR7_CAR(next);
      return TR7_APPLY_ANY;
   }
   /* make a list starting with head */
   head = TR7_NIL;
   tail = &head;
   status = TR7_APPLY_NONE;
   for (;;) {
      /* advance depth of ellipsis */
      updepth = depth;
      while (TR7_IS_PAIR(next) && TR7EQ(TR7_CAR(next), trf->ellipsis)) {
         updepth++;
         next = TR7_CDR(next);
      }
      /* init iteration */
      trig = updepth > depth ? TR7_APPLY_VARIABLE : TR7_APPLY_ANY;
      for (d = depth ; d < updepth ; d++)
         trf->indexes[d] = 0;
      /* iterate for depth */
      substatus = TR7_APPLY_NONE;
      for (;;) {
         /* get a template instance */
         rc = syntax_apply_template(trf, item, updepth, &x);
         if (rc < 0)
            return rc;
         if (rc >= trig) {
            /* record instance in the list */
            y = tr7_cons(trf->tsc, x, TR7_NIL);
            *tail = y;
            tail = &TR7_CDR(y);
            /* next */
            substatus |= rc;
            if (depth == updepth)
               break;
            trf->indexes[updepth - 1]++;
         }
         else {
            if (depth == updepth)
               return 0;
            /* got none, iterate over next depths */
            d = updepth - 1;
            while (d > depth && trf->indexes[d] == 0)
               d--;
            if (d <= depth)
               break; /* next, continue */
            trf->indexes[d] = 0;
            trf->indexes[d - 1]++;
         }
      }
      status |= substatus;
      /* next ? */
      if (TR7_IS_PAIR(next)) {
         /* yes, iterate */
         item = TR7_CAR(next);
         next = TR7_CDR(next);
      }
      else {
         /* no, check for dotted end */
         if (!TR7_IS_NIL(next)) {
            /* dotted end, expands the tail */
            rc = syntax_apply_template(trf, next, depth, &x);
            if (rc <= 0)
               return rc;
            status |= rc;
            *tail = x;
         }
         *result = head;
         return status | TR7_APPLY_ANY;
      }
   }
}

/*
* apply the syntax captures handled in 'trf' to the given vector.
* the resulting expansion is put in result.
* returns negative value on error or expansion status
*/
static int syntax_apply_vector(struct trfmr *trf, tr7_t vector, int depth, tr7_t *result)
{
   tr7_t res, ltemp = tr7_vector_to_list(trf->tsc, vector);
   int rc = syntax_apply_template(trf, ltemp, depth, &res);
   if (rc > 0)
      *result = tr7_list_to_vector(trf->tsc, res);
   return rc;
}

static int eval_syntax_rules_transform(cpl_t cpl, tr7_t transformer, tr7_t expr, tr7_t *result)
{
   int rc;
   tr7_transform_t t;
   tr7_t pattern, rules, scanned, template;
   struct trfmr trf;
   tr7_pair_t pair = NULL;

   /* retrieve the rules */
   trf.tsc = cpl->tsc;
   trf.cpl = cpl;
   do {
      pair = tr7_assq_pair(transformer, cpl->vsyn);
      if (pair == NULL)
         cpl = cpl->upper;
   } while (pair == NULL && cpl != NULL);
   trf.cpltrf = cpl;
   trf.dvars = pair == NULL ? TR7_NIL : TR7_PAIR_CDR(pair);
   t = TR7_TO_TRANSFORM(transformer);
   trf.trf = t;
   trf.ellipsis = t->ellipsis;
   trf.literals = TR7_CDR(t->literals);
#if HAS_GREEDY_SYNTAX
   trf.notgreedy = !!TR7_HEAD_VALUE(TR7_CELL_HEAD(t));
#endif
   rules = t->rules;
   scanned = TR7_CDR(expr);
   while (!TR7_IS_NIL(rules)) {
      /* inspect the rule */
      pattern = TR7_CAAR(rules);
      /* skip heads */
      pattern = TR7_CDR(pattern);
      /* init match */
      trf.variables = TR7_NIL;
      if (syntax_match_pattern(&trf, pattern, scanned, -1)) {
#if DEBUG_SYNTAX
         log_str(trf.tsc, "\nmatching\n");
         log_str(trf.tsc, "      syntax match expr:    ");
         log_item(trf.tsc, expr);
         log_str(trf.tsc, "\n");
         log_str(trf.tsc, "      syntax match pattern: ");
         log_item(trf.tsc, pattern);
         log_str(trf.tsc, "\n");
         log_str(trf.tsc, "      MATCHES\n");
#endif
         syntax_match_pattern(&trf, pattern, scanned, 0);
#if DEBUG_SYNTAX
         log_str(trf.tsc, "\n      CAPTURES: ");
         log_item(trf.tsc, trf.variables);
         log_str(trf.tsc, "\n\n");
#endif
         template = TR7_CADAR(rules);
#if DEBUG_SYNTAX
         log_str(trf.tsc, "      transform from ");
         log_item(trf.tsc, template);
         log_str(trf.tsc, "\n");
#endif
         trf.instance = TR7_NIL;
         rc = syntax_apply_template(&trf, template, 0, result);
#if DEBUG_SYNTAX
         log_str(trf.tsc, "      transform to ");
         log_item(trf.tsc, *result);
         log_str(trf.tsc, "\n\n");
#endif
         return rc;
      }
#if DEBUG_SYNTAX > 1
      log_str(trf.tsc, "\nmatching\n");
      log_str(trf.tsc, "      syntax match expr:    ");
      log_item(trf.tsc, expr);
      log_str(trf.tsc, "\n");
      log_str(trf.tsc, "      syntax match pattern: ");
      log_item(trf.tsc, pattern);
      log_str(trf.tsc, "\n");
      log_str(trf.tsc, "      NO MATCH\n");
#endif
      /* no match try next */
      rules = TR7_CDR(rules);
   }
   return cpl_error_validity(trf.cpl, "expansion of syntax failed", expr);
}

static int declare_transformer_spec(cpl_t cpl, tr7_t trfspec, tr7_t keyword, tr7_t *result)
{
   tr7_t ellipsis;
   tr7_t literals;
   tr7_t iter;
   tr7_t rules;
   tr7_t pattern;
   tr7_t x;

   /* check it is syntax-rules */
   if (!TR7_IS_PAIR(trfspec))
      return -1;
   x = TR7_CAR(trfspec);
   if (!cpl_is_the_keyword(cpl, x, SYMBOL(SYNRULES)))
      return -1;

   /* get ellipsis */
   iter = TR7_CDR(trfspec);
   if (!TR7_IS_PAIR(iter))
      return -1;
   x = TR7_CAR(iter);
   if (!cpl_is_symbol(cpl, x))
      ellipsis = SYMBOL(ELLIPSIS);
   else {
      ellipsis = x;
      iter = TR7_CDR(iter);
      if (!TR7_IS_PAIR(iter))
         return -1;
      x = TR7_CAR(iter);
   }

   /* get and check literals */
   literals = x;
   while (TR7_IS_PAIR(x)) {
      if (!cpl_is_symbol(cpl, TR7_CAR(x)))
         return -1;
      x = TR7_CDR(x);
   }
   if (!TR7_IS_NIL(x))
      return -1;

   /* check disjoint ellispsis and literals */
   if (tr7_memq_pair(ellipsis, literals) != NULL)
      return -1;

   /* get the rules */
   iter = TR7_CDR(iter);
   rules = iter;

   /* check the rules */
   for (; TR7_IS_PAIR(iter) ; iter = TR7_CDR(iter)) {
      x = TR7_CAR(iter);
      if (!TR7_IS_PAIR(x))
         return -1;
      pattern = TR7_CAR(x);
      if (!TR7_IS_PAIR(pattern))
         return -1;
      x = TR7_CDR(x);
      if (!TR7_IS_PAIR(x))
         return -1;
      x = TR7_CDR(x);
      if (!TR7_IS_NIL(x))
         return -1;
   }
   if (!TR7_IS_NIL(iter))
      return -1;

   /* record the rule */
   *result = x = mk_transform(cpl->tsc, keyword, ellipsis, literals, rules, cpl->tsc->curenv);
   return TR7_IS_NIL(x) ? -1 : 0;
}

static int declare_define_syntax(cpl_t cpl, tr7_t args)
{
   tr7_t trfspec;
   tr7_t keyword;

   /* get the keyword */
   if (!TR7_IS_PAIR(args))
      return cpl_error_syntax(cpl, "argument expected", args);
   keyword = TR7_CAR(args);
   if (!cpl_is_symbol(cpl, keyword))
      return cpl_error_syntax(cpl, "name expected", args);

   /* check the transformer spec */
   trfspec = TR7_CDR(args);
   if (!TR7_IS_NIL(TR7_CDR(trfspec)))
      return cpl_error_syntax(cpl, "transformer expected", args);

   /* declare it */
   if (cpl->inlet) {
      tr7_t vardef = TR7_CONS2(cpl->tsc, keyword, TR7_VOID);
      cpl->vars = TR7_CONS2(cpl->tsc, vardef, cpl->vars);
   }
   else {
      if (!environment_define_void(cpl->tsc, cpl->tsc->curenv, keyword))
         return cpl_oom(cpl);
   }
   return 0;
}

static int compile_define_syntax_wvars(cpl_t cpl, tr7_t args, tr7_t wvars)
{
   int rc;
   tr7_t trfspec;
   tr7_t trf;
   tr7_t keyword;
   tr7_pair_t envit;

   /* build the syntax transformer */
   keyword = TR7_CAR(args);
   trfspec = TR7_CADR(args);
   rc = declare_transformer_spec(cpl, trfspec, keyword, &trf);
   if (rc < 0)
      return rc;

   /* record it now */
   if (cpl->inlet) {
      tr7_t vsyn = TR7_CONS2(cpl->tsc, trf, cpl->vars);
      cpl->vsyn = TR7_CONS2(cpl->tsc, vsyn, cpl->vsyn);
      envit = tr7_assq_pair(keyword, wvars);
      if (envit == NULL)
         return cpl_oom(cpl);
      TR7_PAIR_CDR(envit) = trf;
   }
   else {
      rc = environment_set(cpl->tsc, cpl->tsc->curenv, keyword, trf);
      if (!rc)
         return cpl_oom(cpl);
   }
   return 0;
}

static int compile_define_syntax(cpl_t cpl, tr7_t args)
{
   return compile_define_syntax_wvars(cpl, args, cpl->vars);
}

static int cpl_let_syntax(cpl_t cpl, tr7_t args, int isrec)
{
   int rc;
   tr7_t bindings, it, svars, nvars;
   cpl_vars_t oldvars;

   /* get the bindings */
   if (!TR7_IS_PAIR(args))
      return -1;
   bindings = TR7_CAR(args);
   if (!TR7_IS_PAIR(bindings))
      return -1;

   /* enter locals */
   cpl_vars_save(cpl, &oldvars);

   /* declare syntaxes */
   svars = cpl->vars;
   for (it = bindings ; !TR7_IS_NIL(it) ; it = TR7_CDR(it)) {
      rc = declare_define_syntax(cpl, TR7_CAR(it));
      if (rc < 0)
         return rc;
   }

   /* is recursive ? */
   nvars = cpl->vars;
   if (!isrec)
      cpl->vars = svars;

   /* compile syntaxes */
   for (it = bindings ; !TR7_IS_NIL(it) ; it = TR7_CDR(it)) {
      rc = compile_define_syntax_wvars(cpl, TR7_CAR(it), nvars);
      if (rc < 0)
         return rc;
   }
   cpl->vars = nvars;

   /* compile body */
   rc = compile_body(cpl, TR7_CDR(args));
   cpl_vars_restore(cpl, &oldvars);
   return rc;
}

static int syn_letrec_syntax(cpl_t cpl, tr7_t args)
{
   return cpl_let_syntax(cpl, args, 1);
}

static int syn_let_syntax(cpl_t cpl, tr7_t args)
{
   return cpl_let_syntax(cpl, args, 0);
}

static int syn_define_syntax(cpl_t cpl, tr7_t args)
{
   int rc = declare_define_syntax(cpl, args);
   if (rc >= 0)
      rc = compile_define_syntax(cpl, args);
   return rc;
}

static int syn_syntax_error(cpl_t cpl, tr7_t args)
{
   return cpl_error_syntax(cpl, "syntax-error reporting", args);
}

/*
* compile import: does not produce code but add symbols to current global environment
*/
static int syn_import(cpl_t cpl, tr7_t args)
{
   int rc = import(cpl->tsc, args, cpl->tsc->curenv);
   if (rc < 0)
      rc = cpl_error_eval(cpl, cpl->tsc->values[0]);
   return rc;
}

static int cpl_eval_cond_expand(cpl_t cpl, tr7_t req)
{
   int rc, rcs;
   tr7_t head, rest;

   if (TR7_IS_NIL(req))
      return 0;
   if (!TR7_IS_PAIR(req)) {
      if (!cpl_is_symbol(cpl, req))
         return -1;
      if (TR7EQ(req, SYMBOL(ELSE)))
         return 1;
      return has_feature(req);
   }

   head = TR7_CAR(req);
   rest = TR7_CDR(req);
   if (TR7EQ(head, SYMBOL(NOT))) {
      if (!TR7_IS_PAIR(rest))
         return -1;
      rc = cpl_eval_cond_expand(cpl, TR7_CAR(rest));
      return rc < 0 ? rc : !rc;
   }

   if (TR7EQ(head, SYMBOL(LIBRARY))) {
      if (!TR7_IS_PAIR(rest))
         return -1;
      return get_library(cpl->tsc, TR7_CAR(rest), NULL) >= 0;
   }

   if (TR7EQ(head, SYMBOL(OR)))
      rc = 0;
   else if (TR7EQ(head, SYMBOL(AND)))
      rc = 1;
   else
      return -1;

   while(TR7_IS_PAIR(rest)) {
      rcs = cpl_eval_cond_expand(cpl, TR7_CAR(rest));
      if (rcs != rc)
         return rcs;
      rest = TR7_CDR(rest);
   }
   return rc;
}

static int cpl_get_cond_expand(cpl_t cpl, tr7_t args, tr7_t *found)
{
   tr7_t clause;
   for(;; args = TR7_CDR(args) ) {
      if (TR7_IS_NIL(args))
         return 0;
      if (!TR7_IS_PAIR(args))
         return cpl_error_improper_list(cpl, args);
      clause = TR7_CAR(args);
      if (!TR7_IS_PAIR(clause))
         return cpl_error_syntax(cpl, "invalid cond-expand clause", clause);
      if (cpl_eval_cond_expand(cpl, TR7_CAR(clause)) > 0) {
         *found = TR7_CDR(clause);
         return 1;
      }
   }
}

static int syn_cond_expand(cpl_t cpl, tr7_t args)
{
   tr7_t exprs;
   int rc = cpl_get_cond_expand(cpl, args, &exprs);
   return rc <= 0 ? rc : TR7_IS_PAIR(exprs) ? compile_body(cpl, exprs) : 0;
}

static int cpl_apply_include(cpl_t cpl, tr7_t args, unsigned ci_loadflag, int (*fun)(cpl_t,tr7_t,void*), void *closure)
{
   const char *base;
   int rc;
   tr7_t iter, head;
   port_t *pt;
#if USE_TR7_DEBUG && DEBUG_LINES
   tr7_t cur_line, filename, linetrack;
#endif

   /* iterate over file list */
   for(iter = args ; TR7_IS_PAIR(iter) ; iter = TR7_CDR(iter)) {

      /* get filename */
      head = TR7_CAR(iter);
      if (!TR7_IS_STRING(head))
         return cpl_error_validity(cpl, "not a string", head);
      base = (const char*)TR7_CONTENT_STRING(head);

      /* open the file */
      if (cpl->tsc->playflags & Tr7_Play_Show_Load) {
         log_str(cpl->tsc, "Loading ");
         log_str(cpl->tsc, base);
         log_str(cpl->tsc, "\n");
      }
      if (!load_enter_search_include(cpl->tsc, base, ci_loadflag))
         return cpl_error_validity(cpl, "unable to open", head);

      /* read all the content */
      pt = TR7__PORT__PORT(cpl->tsc->loadport);
      rc = do_read_with_datum(cpl->tsc, pt, 1);
      load_leave(cpl->tsc);

      if (rc < 0)
         return cpl_error_eval(cpl, cpl->tsc->read_value);

#if USE_TR7_DEBUG && DEBUG_LINES
      linetrack = cpl->linetrack;
      cur_line = cpl->cur_line;
      filename = cpl->filename;
      cpl->cur_line = cpl->tsc->last_line;
      cpl->filename = cpl->tsc->read_file;
      cpl->linetrack = cpl->tsc->read_lines;
#endif

      /* process the content */
      if (rc > 0) {
         rc = fun(cpl, cpl->tsc->read_value, closure);
         if (rc < 0)
            return rc;
      }

#if USE_TR7_DEBUG && DEBUG_LINES
      cpl->cur_line = cur_line;
      cpl->filename = filename;
      cpl->linetrack = linetrack;
#endif
   }
   if (!TR7_IS_NIL(iter))
      return cpl_error_validity(cpl, "improper list", iter);
   return 0;
}

/* helpers to implement 'include' and 'include-ci' */
static int cpl_do_include_cb(cpl_t cpl, tr7_t list, void *closure)
{
   return compile_body(cpl, list);
}
static int cpl_do_include(cpl_t cpl, tr7_t args, unsigned flags)
{
   return cpl_apply_include(cpl, args, flags, cpl_do_include_cb, NULL);
}

/* helpers to implement 'include-library-declarations' */
static int cpl_deflib_splice_expand(cpl_t cpl, tr7_t indecls, tr7_t **pptail);
static int cpl_do_include_library_declarations_cb(cpl_t cpl, tr7_t list, void *closure)
{
   return cpl_deflib_splice_expand(cpl, list, closure);
}
static int cpl_do_include_library_declarations(cpl_t cpl, tr7_t args, tr7_t **pptail)
{
   return cpl_apply_include(cpl, args, 0, cpl_do_include_library_declarations_cb, pptail);
}

/*
* splice expand of cond-expand and include-library-declarations
* in define-library
*/
static int cpl_deflib_splice_expand(cpl_t cpl, tr7_t indecls, tr7_t **pptail)
{
   int rc, repl = 0;
   tr7_t head, next, first;

   while (TR7_IS_PAIR(indecls)) {

      /* pair */
      head = TR7_CAR(indecls);
      next = TR7_CDR(indecls);

      /* check if (cond-expand ...) or (include-library-declarations ...) */
      if (TR7_IS_PAIR(head)) {
         first = TR7_CAR(head);

         /* cond-expand ? */
         if (TR7EQ(first, SYMBOL(COND_EXPAND))) {
            rc = cpl_get_cond_expand(cpl, TR7_CDR(head), &head);
            if (rc > 0)
               rc = cpl_deflib_splice_expand(cpl, head, pptail);
            if (rc < 0)
               return rc;
            repl = 1;
         }

         /* include-library-declarations ? */
         else if (TR7EQ(first, SYMBOL(INCLUDE_LIB_DECL))) {
            rc = cpl_do_include_library_declarations(cpl, TR7_CDR(head), pptail);
            if (rc < 0)
               return rc;
            repl = 1;
         }

         /* check for errors */
         if (repl && rc < 0)
            return rc;
      }

      /* is replacement done? */
      if (repl)
         repl = 0; /* yes */
      else {
         /* no */
         **pptail = indecls;
         *pptail = &TR7_CDR(indecls);
      }
      indecls = next;
   }

   if (!TR7_IS_NIL(indecls))
      return cpl_error_syntax(cpl, "improper library definition", indecls);
   return 0;
}
/*
* process the library definition expungned of its
* includes and cond-expands
*/
static int cpl_deflib_defines(cpl_t cpl, tr7_t decls)
{
   int rc = 0;
   tr7_t iter, first, head, args, prog;
   struct cpl_s lcpl;
   eval_status_t es;

   /* iterate over the list */
   for (iter = decls; rc >= 0 && TR7_IS_PAIR(iter); iter = TR7_CDR(iter)) {
      head = TR7_CAR(iter);
      if (!TR7_IS_PAIR(head))
         return cpl_error_syntax(cpl, "unexpected content in library definition", iter);

      first = TR7_CAR(head);
      args = TR7_CDR(head);

#if USE_TR7_DEBUG && DEBUG_LINES
      cpl_init(&lcpl, cpl->tsc, cpl->error, cpl->filename, cpl->linetrack);
#else
      cpl_init(&lcpl, cpl->tsc, cpl->error);
#endif

      /* importing */
      if (TR7EQ(first, SYMBOL(IMPORT)))
         rc = syn_import(&lcpl, args);

      /* definitions */
      else if (TR7EQ(first, SYMBOL(BEGIN)))
         rc = compile_body(&lcpl, args);

      /* includes */
      else if (TR7EQ(first, SYMBOL(INCLUDE)))
         rc = cpl_do_include(&lcpl, args, 0);
      else if (TR7EQ(first, SYMBOL(INCLUDE_CI)))
         rc = cpl_do_include(&lcpl, args, Tr7_Play_Fold_Case);

      /* deferred export */
      else if (TR7EQ(first, SYMBOL(EXPORT)))
         rc = 0;

      /* unexpected */
      else
         rc = cpl_error_syntax(&lcpl, "unexpected define library item", head);

      /* execution ? */
      if (rc >= 0 && lcpl.poscode > 0) {
         rc = compile_make_prog(&lcpl, &prog);
         if (rc >= 0 && !TR7_IS_VOID(prog)) {
            es = execute_prog(cpl->tsc, prog);
            switch (es) {
            case Cycle_OOM:
               rc = cpl_oom(cpl);
               break;
            case Cycle_Leave_Error:
               rc = cpl_error_eval(cpl, cpl->tsc->values[0]);
               break;
            case Cycle_Leave:
            default:
               rc = 0;
               break;
            }
         }
      }
      rc = cpl_leave(&lcpl, rc);
   }
   return rc;
}

/* helper for define library exports */
static int cpl_export(cpl_t cpl, tr7_t args, tr7_t fromenv, tr7_t toenv)
{
   tr7_t exports[3]; /* for ('rename from to) */
   tr7_t value, it;
   tr7_pair_t envit;
   int rc;

   for(it = args ; TR7_IS_PAIR(it) ; it = TR7_CDR(it)) {
      /* get from/to in exports[1]/exports[2] */
      exports[1] = TR7_CAR(it);
      if (cpl_is_symbol(cpl, exports[1]))
         exports[2] = exports[1];
      else {
         rc = tr7_get_list_cars(exports[1], 3, exports, NULL);
         if (rc != 3 || !TR7EQ(exports[0], SYMBOL(RENAME))
            || !cpl_is_symbol(cpl, exports[1]) ||   !cpl_is_symbol(cpl, exports[2]))
            return cpl_error_syntax(cpl, "invalid export", TR7_CAR(it));
      }
      /* find the exported symbol */
      envit = environment_find_item(fromenv, exports[1]);
      if (envit == NULL)
         return cpl_error_validity(cpl, "exported symbol not found", exports[1]);
      /* check the exported value */
      value = TR7_PAIR_CDR(envit);
      if (TR7_IS_VOID(value) || (IS_BOX(value) && TR7_IS_VOID(GET_BOX(value))))
         return cpl_error_validity(cpl, "exported symbol not defined", exports[1]);
      /* fill the exporting environment */
      rc = environment_import(cpl->tsc, toenv, exports[2], value);
      if (!rc)
         return cpl_oom(cpl);
   }
   if (!TR7_IS_NIL(it))
      return cpl_error_syntax(cpl, "improper export", it);
   return 0;
}

static int cpl_deflib_export(cpl_t cpl, tr7_t decls, tr7_t fromenv, tr7_t toenv)
{
   int rc = 0;
   tr7_t iter, first, head;

   /* iterate over the list for processing exports */
   for (iter = decls; rc >= 0 && TR7_IS_PAIR(iter); iter = TR7_CDR(iter)) {
      head = TR7_CAR(iter);
      first = TR7_CAR(head);
      if (TR7EQ(first, SYMBOL(EXPORT)))
         rc = cpl_export(cpl, TR7_CDR(head), fromenv, toenv);
   }
   return rc;
}

static int syn_define_library(cpl_t cpl, tr7_t args)
{
   char basename[LIBNAME_MAXSZ + 1];
   unsigned len;
   tr7_t libname, libdcls, libexp, libenv, expenv, *pexp;
   int rc;

   /* get the library name */
   if (!TR7_IS_PAIR(args))
      return cpl_error_syntax(cpl, "missing library name", args);
   libname = TR7_CAR(args);

   /* get library name */
   len = make_libname(libname, basename, sizeof basename);
   if (len == 0)
      return cpl_error_validity(cpl, "bad library name", libname);
   if (len >= sizeof basename)
      return cpl_error_validity(cpl, "library name too long", libname);
   basename[len] = 0;

   /* check if already defined */
   if (searchlib(cpl->tsc, basename, len, NULL))
      return cpl_error_validity(cpl, "already defined library", libname);

   /* retrieves the specifications of the library */
   libdcls = TR7_CDR(args);
   rc = tr7_list_length(libdcls);
   if (rc <= 0)
      return cpl_error_validity(cpl, "invalid library specification", args);

   /* expands cond expands and include */
   libexp = TR7_NIL;
   pexp = &libexp;
   rc = cpl_deflib_splice_expand(cpl, libdcls, &pexp);
   if (rc < 0)
      return rc;

   /* creates the evaluation environment */
   libenv = make_null_environment(cpl->tsc, DEFAULT_ENV_SIZE);

   /* process definitions of expanded content and restore environment */
   save_from_C_call(cpl->tsc);
   cpl->tsc->curenv = libenv;
   rc = cpl_deflib_defines(cpl, libexp);
   restore_from_C_call(cpl->tsc);

   /* export the environment */
   if (rc >= 0) {
      expenv = mk_environment(cpl->tsc, TR7_NIL, DEFAULT_ENV_SIZE);
      rc = cpl_deflib_export(cpl, libexp, libenv, expenv);
      if (rc >= 0)
         addlib(cpl->tsc,  basename, len, expenv);
   }
   return rc;
}

/* implement 'include' */
static int syn_include(cpl_t cpl, tr7_t args)
{
   return cpl_do_include(cpl, args, 0);
}

/* implement 'include-ci' */
static int syn_include_ci(cpl_t cpl, tr7_t args)
{
   return cpl_do_include(cpl, args, Tr7_Play_Fold_Case);
}

static int cpl_call(cpl_t cpl, tr7_t proc, tr7_t args, tr7_pair_t envit)
{
   int rc = compile_exprlist0_args(cpl, args);
   if (rc >= 0) {
      int nargs = rc;
      if (envit == NULL) {
         /* compile the lambda expression */
         rc = compile_expression(cpl, proc);
         if (rc >= 0)
            rc = cpl_emit_call(cpl, nargs);
      }
      else if (envit == cpl->self) {
            rc = cpl_emit_call_self(cpl, nargs);
      }
      else if (IS_PROC(TR7_PAIR_CDR(envit))) {
         /* compile the proc */
         rc = cpl_emit_proc(cpl, TO_PROC(TR7_PAIR_CDR(envit)), nargs);
      }
      else {
         /* compile the symbol */
         rc = compile_get_var(cpl, proc);
         if (rc >= 0)
            rc = cpl_emit_call(cpl, nargs);
      }
   }
   return rc;
}

static int compile_syntax(cpl_t cpl, tr7_t args, tr7_t syn, int predeclared)
{
   cplcb_t cb;
   if (predeclared) {
      switch(TO_SYNTAX(syn)) {
      case SYNTAXID(DEFINE):
         return compile_define(cpl, args);
      case SYNTAXID(DEFVAL):
         return compile_define_values(cpl, args);
      case SYNTAXID(DEFSYN):
         return 0;
      case SYNTAXID(BEGIN):
         return cpl_sequence(cpl, args, 1);
      case SYNTAXID(DEFREC):
         return compile_define_record_type(cpl, args);
      default:
         break;
      }
   }
   cb = syncbs[TO_SYNTAX(syn)];
   return cb(cpl, args);
}

static int declare_body_items(cpl_t cpl, tr7_t args)
{
   int rc = 0;
   tr7_t expr, symb, vvalue, trf;
   for ( ; rc >= 0 && TR7_IS_PAIR(args) ; args = TR7_CDR(args)) {
      expr = TR7_CAR(args);
again:
      if (!TR7_IS_PAIR(expr))
         break;
      symb = TR7_CAR(expr);
      if (!cpl_is_symbol(cpl, symb))
         break;
      if (compile_search_value(cpl, symb, &vvalue) < 0)
         break;
      if (TR7_IS_TRANSFORM(vvalue)) {
         rc = eval_syntax_rules_transform(cpl, vvalue, expr, &trf);
         if (rc < 0)
            break;
         expr = trf;
         goto again;
      }
      if (!IS_SYNTAX(vvalue)) {
         args = TR7_CDR(args);
         break;
      }
      expr = TR7_CDR(expr);
      switch(TO_SYNTAX(vvalue)) {
      case SYNTAXID(DEFINE):
         rc = declare_define(cpl, expr);
         break;
      case SYNTAXID(DEFVAL):
         rc = declare_define_values(cpl, expr);
         break;
      case SYNTAXID(DEFSYN):
         /* early definition of syntaxes */
         rc = syn_define_syntax(cpl, expr);
         break;
      case SYNTAXID(BEGIN):
         if (!TR7_IS_PAIR(expr))
            rc = cpl_error_validity(cpl, "bad body", TR7_CAR(args));
         else
            rc = declare_body_items(cpl, expr);
         break;
      case SYNTAXID(DEFREC):
         rc = declare_define_record_type(cpl, expr);
         break;
      default:
         break;
      }
   }
   if (rc >= 0 && TR7_IS_PAIR(args))
      rc = check_no_declare_sequence(cpl, args);
   return rc;
}

/*
* compile (begin ...)
*/
static int syn_begin(cpl_t cpl, tr7_t args)
{
   return compile_body(cpl, args);
}

static int compile_body(cpl_t cpl, tr7_t args)
{
   int rc;
   if (!TR7_IS_PAIR(args))
      rc = cpl_error_syntax(cpl, "invalid body", args);
   else if (!cpl->inlet)
      rc = cpl_sequence(cpl, args, 0);
   else {
      rc = declare_body_items(cpl, args);
      if (rc >= 0)
         rc = cpl_sequence(cpl, args, 1);
   }
   return rc;
}

static int syntaxic_expansion(cpl_t cpl, tr7_t expr, tr7_t *result)
{
   tr7_pair_t pair, envit;
   tr7_t head, value;
   int rc;

   /* is it a pair? */
   if (TR7_IS_PAIR(expr)) {
      /* yes, get the head of the pair */
      pair = TR7_TO_PAIR(expr);
      head = TR7_PAIR_CAR(pair);
      envit = NULL;
      if (cpl_is_symbol(cpl, head)) {
         /* when symbol, look at it, create it globally if not existing */
         rc = compile_search_cplenvit(cpl, head, &envit, 1);
         if (rc < 0)
            return rc;
         value = TR7_PAIR_CDR(envit);
         /* is it a program syntax? */
         if (TR7_IS_TRANSFORM(value)) {
            rc = eval_syntax_rules_transform(cpl, value, expr, result);
            if (rc < 0)
               return rc;
            return syntaxic_expansion(cpl, *result, result);
         }
      }
   }
   /* the value for itself */
   *result = expr;
   return 0;
}

static int compile_expr_checked(cpl_t cpl, tr7_t expr, int predeclared)
{
   tr7_pair_t pair, envit;
   tr7_t head, value, trf;
   int rc;

   /* is it a pair? */
   if (TR7_IS_PAIR(expr)) {
      /* yes, get the head of the pair */
      pair = TR7_TO_PAIR(expr);
      head = TR7_PAIR_CAR(pair);
      envit = NULL;
      if (cpl_is_symbol(cpl, head)) {
         /* when symbol, look at it, create it globally if not existing */
         rc = compile_search_cplenvit(cpl, head, &envit, 1);
         if (rc < 0)
            return rc;
         value = TR7_PAIR_CDR(envit);
         /* is it a predefined syntax? */
         if (IS_SYNTAX(value))
            return compile_syntax(cpl, TR7_PAIR_CDR(pair), value, predeclared);
         /* is it a program syntax? */
         if (TR7_IS_TRANSFORM(value)) {
            rc = eval_syntax_rules_transform(cpl, value, expr, &trf);
            if (rc < 0)
               return rc;
            return compile_expr(cpl, trf, predeclared);
         }
      }
      else if (!TR7_IS_PAIR(head)) {
         /* if not pair and not symbol, it can't be a procedure */
         return cpl_error_validity(cpl, "invalid expression", expr);
      }

      /* compile a call */
      return cpl_call(cpl, head, TR7_PAIR_CDR(pair), envit);
   }

   /* is it a symbol? */
   if (cpl_is_symbol(cpl, expr))
      return compile_get_var(cpl, expr);

   /* the value for itself */
   return compile_quote(cpl, expr);
}

static int compile_expr(cpl_t cpl, tr7_t expr, int predeclared)
{
   int rc;
#if USE_TR7_DEBUG && DEBUG_LINES
   if (TR7_IS_PAIR(cpl->linetrack)) {
      tr7_pair_t pos = tr7_assq_pair(expr, cpl->linetrack);
      if (pos != NULL)
         cpl->cur_line = TR7_PAIR_CDR(pos);
   }
#endif
   rc = compile_expr_checked(cpl, expr, predeclared);
   if (rc < 0 && TR7_IS_VOID(cpl->error->expr))
      cpl->error->expr = expr;
   return rc;
}

static int compile_expression(cpl_t cpl, tr7_t expr)
{
   int rc = compile_expr(cpl, expr, 0);
   if (rc < 0 && TR7_IS_VOID(cpl->error->expr))
      cpl->error->expr = expr;
   return rc;
}

static int compile_expression_arg(cpl_t cpl, tr7_t expr)
{
   int rc = compile_expression(cpl, expr);
   if (rc >= 0)
      rc = compile_push_arg(cpl);
   return rc;
}

static int compile_make_prog(cpl_t cpl, tr7_t *prog)
{
   int rc;
   *prog = TR7_VOID;
   if (cpl->varcount <= 0 && cpl->poscode == 0)
      rc = 0;
   else
      rc = cpl_make_prog(cpl, TR7_VOID, 0, 0, prog);
   return rc;
}

/* procedure 'main_compile' */
#if USE_TR7_DEBUG && DEBUG_LINES
static int main_compile(tr7_engine_t tsc, tr7_t expr, tr7_t filename, tr7_t linetrack)
#else
static int main_compile(tr7_engine_t tsc, tr7_t expr)
#endif
{
   int rc;
   struct cpl_s cpl;
   cpl_error_t error;
   tr7_t prog;

   push_recent_alloc(tsc, expr); /*TODO remove that line when possible */

#if USE_TR7_DEBUG && DEBUG_LINES
   push_recent_alloc(tsc, filename); /*TODO remove that line when possible */
   cpl_init(&cpl, tsc, &error, filename, linetrack);
#else
   cpl_init(&cpl, tsc, &error);
#endif

   rc = compile_expression(&cpl, expr);
   /* create a context for locals if any */
   if (rc >= 0)
      rc = compile_make_prog(&cpl, &prog);
   if (rc >= 0)
      set_values_single(tsc, prog);
   else
      cpl_report_error(&cpl);
   return cpl_leave(&cpl, rc);
}

/* procedure 'compile' */
#if USE_TR7_DEBUG && DEBUG_LINES
static eval_status_t do_compile(tr7_engine_t tsc, tr7_t expr, tr7_t filename, tr7_t linetrack)
{
   int rc = main_compile(tsc, expr, filename, linetrack);
#else
static eval_status_t do_compile(tr7_engine_t tsc, tr7_t expr)
{
   int rc = main_compile(tsc, expr);
#endif
   return rc < 0 ? Cycle_Raise : Cycle_Return;
}
/* ========== Initialization of internals ========== */

/* initialization of TR7 */

void tr7_config_init_default(tr7_config_t *config)
{
   config->main_dictionary_size = SYMBOL_SET_SIZE;
   config->stack_size_max = STACK_SIZE_MAX;
   config->malloc = malloc;
   config->free = free;
}

static int scheme_init(tr7_engine_t tsc, tr7_config_t *config)
{
   /* init sink */
   tsc->malloc = config->malloc;
   tsc->free = config->free;
   tsc->stack_size_max = config->stack_size_max ? config->stack_size_max : UINT_MAX;
   tsc->no_memory = 0;
   tsc->no_stack = 0;
#if STRESS_GC_RESILIENCE
   tsc->gc_resilience = 0;
#endif
#if HAS_GREEDY_SYNTAX
   tsc->no_greedy_syntax = 0;
#endif
   tsc->stdports[IDX_STDIN] = TR7_NIL;
   tsc->stdports[IDX_STDOUT] = TR7_NIL;
   tsc->stdports[IDX_STDERR] = TR7_NIL;
   tsc->loadport = TR7_NIL;
   tsc->loadenv = TR7_NIL;
   tsc->playflags = 0;
   tsc->curenv = TR7_NIL;
#if COMMON_ROOT_ENV
   tsc->null_env = TR7_NIL;
   tsc->base_env = TR7_NIL;
#endif
   tsc->read_value = TR7_VOID;
#if USE_TR7_DEBUG && DEBUG_LINES
   tsc->read_file = TR7_VOID;
   tsc->read_lines = TR7_VOID;
#endif
   tsc->libraries = TR7_NIL;
   tsc->stof_guards = TR7_NIL;
   tsc->stof_params = TR7_NIL;
   tsc->stof_dynawinds = TR7_NIL;
   tsc->stack.data = NULL;
   tsc->stack.oper = NULL;
   tsc->stack.head = NULL;
   tsc->stack.tail = NULL;
#if GLOBAL_STACK_SAFETY
   tsc->stack.safegap = STACK_SAFEGAP_INIT;  /* psnd: was left uninitialized */
#endif
   tsc->nvalues = 0;
   tsc->symbols_set = TR7_NIL;
   tsc->c_nest = TR7_NIL;
   tsc->c_holds = TR7_NIL;
   tsc->datums = TR7_NIL;
#if USE_TR7_DEBUG && DEBUG_LINES
   tsc->last_line = 0;
   tsc->line_starts = TR7_NIL;
#endif
   tsc->strings[Tr7_StrID_Prompt] = NULL;
   tsc->strings[Tr7_StrID_Path] = NULL;
   tsc->strings[Tr7_StrID_Library_Path] = NULL;
   tsc->strings[Tr7_StrID_Include_Path] = NULL;
   tsc->strings[Tr7_StrID_Extension_Path] = NULL;

   tsc->free_cells = 0;
   tsc->nmemseg = 0;
   tsc->nsuccfrees = 0;
   tsc->firstfree = NULL;
   tsc->freeshead = NULL;
   tsc->freestail = NULL;

   tsc->strbuff.length = 0;
   tsc->strbuff.size = (unsigned)sizeof tsc->strbuff.buffer;
   tsc->strbuff.head = tsc->strbuff.buffer;

   if (memseg_multi_alloc(tsc, NSEGMENT_INITIAL, ITEM_SEGSIZE) != NSEGMENT_INITIAL) {
      tsc->no_memory = 1;
      return 0;
   }

   tsc->no_recent = 1;           /* dont record recents during init */
   tsc->recent_count = 0;
   tsc->gc_verbose = 0;
   tsc->tracing = 0;

   /* init port parameters */
   init_stdports(tsc);
   tr7_set_standard_ports(tsc);

   /* init symbols */
   tsc->symbols_set = symbols_set_initial_value(tsc, config->main_dictionary_size);
   symbols_set_add_predefined_symbols(tsc);

   /* init default environments */
   tsc->curenv = make_null_environment(tsc, config->main_dictionary_size);

   /* init else */
   stack_grow(tsc);

   tsc->no_recent = 0;           /* init done, record recents */
#if STRESS_GC_RESILIENCE
   tsc->gc_resilience = 1;
#endif
   return !tsc->no_memory;
}

static void scheme_deinit(tr7_engine_t tsc)
{
   unsigned i;

   strbuff_start(tsc);
   tsc->symbols_set = TR7_NIL;
   tsc->curenv = TR7_NIL;
#if COMMON_ROOT_ENV
   tsc->null_env = TR7_NIL;
   tsc->base_env = TR7_NIL;
#endif
   tsc->read_value = TR7_VOID;
#if USE_TR7_DEBUG && DEBUG_LINES
   tsc->read_file = TR7_VOID;
   tsc->read_lines = TR7_VOID;
#endif
   tsc->libraries = TR7_NIL;
   tsc->stof_guards = TR7_NIL;
   tsc->stack.data = NULL;
   tsc->stack.oper = NULL;
   tsc->stack.head = NULL;
   tsc->stack.tail = NULL;
#if GLOBAL_STACK_SAFETY
   tsc->stack.safegap = STACK_SAFEGAP_INIT;
#endif
   tsc->nvalues = 0;
   tsc->loadport = TR7_NIL;
   tsc->stdports[IDX_STDIN] = TR7_NIL;
   tsc->stdports[IDX_STDOUT] = TR7_NIL;
   tsc->stdports[IDX_STDERR] = TR7_NIL;
   tsc->loadenv = TR7_NIL;
   tsc->datums = TR7_NIL;
   tsc->c_nest = TR7_NIL;
   tsc->c_holds = TR7_NIL;
#if USE_TR7_DEBUG && DEBUG_LINES
   tsc->last_line = 0;
   tsc->line_starts = TR7_NIL;
#endif
   tsc->gc_verbose = 0;
   ok_to_freely_gc(tsc);  /* psnd: recents are GC roots; ports left there were never finalized */
   collect_garbage(tsc);

   for (i = 0; i < tsc->nmemseg; i++)
      tsc->free(tsc->memsegs[i]);
}

tr7_engine_t tr7_engine_create(tr7_config_t *config)
{
   tr7_engine_t tsc;
   tr7_config_t cfg;

   if (!config)
      tr7_config_init_default(&cfg);
   else {
      cfg = *config;
      if (!cfg.malloc || !cfg.free || !cfg.main_dictionary_size)
         return NULL;
   }
   tsc = (tr7_engine_t)cfg.malloc(sizeof(*tsc));
   if (scheme_init(tsc, &cfg))
      return tsc;
   cfg.free(tsc);
   return NULL;
}

void tr7_engine_destroy(tr7_engine_t tsc)
{
   scheme_deinit(tsc);
   tsc->free(tsc);
}

void tr7_set_standard_ports(tr7_engine_t tsc)
{
   tr7_set_input_port_file(tsc, stdin, "stdin");
   tr7_set_output_port_file(tsc, stdout, "stdout");
   tr7_set_error_port_file(tsc, stderr, "stderr");
}

void tr7_set_input_port_file(tr7_engine_t tsc, FILE * fin, const char *fname)
{
   set_stdport(tsc, port_from_file(tsc, fin, fname, port_input | port_textual), IDX_STDIN);
}

void tr7_set_input_port_string(tr7_engine_t tsc, char *start, char *end)
{
   set_stdport(tsc, port_from_string(tsc, TR7_NIL, (uint8_t*)start, (uint8_t*)end), IDX_STDIN);
}

void tr7_set_output_port_file(tr7_engine_t tsc, FILE * fout, const char *fname)
{
   set_stdport(tsc, port_from_file(tsc, fout, fname, port_output | port_textual), IDX_STDOUT);
}

void tr7_set_error_port_file(tr7_engine_t tsc, FILE * ferr, const char *fname)
{
   set_stdport(tsc, port_from_file(tsc, ferr, fname, port_output | port_textual), IDX_STDERR);
}

/*
* load and eval inputs
* return 1 in case of success or 0 on error
*/
static int play(tr7_engine_t tsc)
{
   eval_status_t sts;
   oper_push_safe_1(tsc, OPER(REPL_READ));
   sts = main_loop(tsc);
   return sts == Cycle_Leave;
}

int tr7_play_file(tr7_engine_t tsc, FILE * fin, const char *filename, tr7_play_t options)
{
   if (load_enter_search_load(tsc, fin, filename, options))
      return play(tsc);
   set_error_msg_obj(tsc, "can't open file", tr7_make_string_copy(tsc, filename), 0);
   return 0; /* not found */
}

int tr7_play_string(tr7_engine_t tsc, const char *cmd, tr7_play_t options)
{
   load_enter_string(tsc, (uint8_t*)cmd, NULL, options);
   return play(tsc);
}

int tr7_load_file(tr7_engine_t tsc, FILE * fin, const char *filename)
{
   tr7_play_t options = fin == stdin ? Tr7_Play_Interactive : Tr7_Play_Trap_Errors;
   return tr7_play_file(tsc, fin, filename, options);
}

int tr7_load_string(tr7_engine_t tsc, const char *cmd)
{
   return tr7_play_string(tsc, cmd, Tr7_Play_Trap_Errors);
}

int  tr7_run_file(tr7_engine_t tsc, FILE * fin, const char *filename)
{
   return tr7_play_file(tsc, fin, filename, Tr7_Play_No_Options);
}

int  tr7_run_string(tr7_engine_t tsc, const char *cmd)
{
   return tr7_play_string(tsc, cmd, Tr7_Play_No_Options);
}

static tr7_t do_read_pt(tr7_engine_t tsc, port_t *pt)
{
   do_read_with_datum(tsc, pt, 0);
#if USE_TR7_DEBUG && DEBUG_LINES
   tsc->read_file = tsc->read_lines = TR7_VOID;
#endif
   return tsc->read_value;
}

tr7_t tr7_read(tr7_engine_t tsc)
{
   tr7_t port = get_stdport(tsc, IDX_STDIN);
   return do_read_pt(tsc, TR7__PORT__PORT(port));
}

tr7_t tr7_from_utf8_length(tr7_engine_t tsc, const char *expr, size_t length)
{
   tr7_t result = TR7_VOID;
   port_t *pt = port_rep_from_string(tsc, TR7_NIL, (uint8_t*)expr, (uint8_t*)&expr[length]);
   if (pt != NULL) {
      result = do_read_pt(tsc, pt);
      port_rep_free(tsc, pt);
   }
   return result;
}

tr7_t tr7_from_utf8(tr7_engine_t tsc, const char *expr)
{
   return tr7_from_utf8_length(tsc, expr, strlen(expr));
}

void tr7_define(tr7_engine_t tsc, tr7_t envir, tr7_t symbol, tr7_t value)
{
   if (!TR7_IS_ENVIRONMENT(envir))
      envir = tsc->curenv;
   environment_define(tsc, envir, symbol, value);
}

int tr7_set(tr7_engine_t tsc, tr7_t envir, tr7_t symbol, tr7_t value)
{
   if (!TR7_IS_ENVIRONMENT(envir))
      envir = tsc->curenv;
   return environment_set(tsc, envir, symbol, value);
}

void tr7_hold(tr7_engine_t tsc, tr7_t value)
{
   tsc->c_holds = tr7_cons(tsc, value, tsc->c_holds);
}

void tr7_unhold(tr7_engine_t tsc, tr7_t value)
{
   tr7_t *p = &tsc->c_holds, c = tsc->c_holds;
   while (!TR7_IS_NIL(c))
      if (!TR7EQ(value, TR7_CAR(c)))
         c = *(p = &TR7_CDR(c));
      else {
         *p = TR7_CDR(c);
         break;
      }
}

void tr7_set_string(tr7_engine_t tsc, tr7_strid_t strid, const char *value)
{
   int idx = (int)strid;
   if (idx >= 0 && idx < (int)(sizeof tsc->strings / sizeof *tsc->strings))
      tsc->strings[idx] = value;
}

static void save_from_C_call(tr7_engine_t tsc)
{
   tr7_vector_t vec;
   tr7_t v, recents = wrap_recent_allocs(tsc);
   tsc->recents[0] = recents;
   tsc->recent_count = 1;
   v = alloc_vector(tsc, 6);
   vec = TR7_TO_VECTOR(v);
   vec->items[1] = tsc->curenv;
   vec->items[2] = recents;
   vec->items[3] = tsc->stof_guards;
   vec->items[4] = tsc->stof_params;
   vec->items[5] = tsc->stof_dynawinds;
   vec->items[0] = tsc->c_nest;
   tsc->c_nest = v;
   tsc->recent_count = 0;
   /* Truncate the dump stack so TS will return here when done, not
      directly resume pre-C-call. */
}

static void restore_from_C_call(tr7_engine_t tsc)
{
   tr7_vector_t vec = TR7_TO_VECTOR(tsc->c_nest);
   tsc->curenv = vec->items[1];
   tr7_t recents = vec->items[2];
   tsc->stof_guards = vec->items[3];
   tsc->stof_params = vec->items[4];
   tsc->stof_dynawinds = vec->items[5];
   tsc->recent_count = 0;
   if (!TR7_IS_FALSE(recents))
      push_recent_alloc(tsc, recents);
   tsc->c_nest = vec->items[0];
}

/* "func" and "args" are assumed to be already eval'ed. */
tr7_t tr7_call(tr7_engine_t tsc, tr7_t func, tr7_t args)
{
   int nr;
   tr7_uint_t old_loadflags = tsc->playflags;
   tsc->playflags = 0;
   save_from_C_call(tsc);
   prepare_loop_exec(tsc);
   nr = data_stack_push_list(tsc, args);
   s_exec(tsc, func, (unsigned)nr);
   main_loop(tsc);
   tsc->playflags = old_loadflags;
   restore_from_C_call(tsc);
   return tsc->values[0];
}

tr7_t tr7_eval(tr7_engine_t tsc, tr7_t expr)
{
   int rc;
   tr7_uint_t old_loadflags = tsc->playflags;
   tsc->playflags = 0;
   save_from_C_call(tsc);
#if USE_TR7_DEBUG && DEBUG_LINES
   rc = main_compile(tsc, expr, TR7_VOID, TR7_NIL);
#else
   rc = main_compile(tsc, expr);
#endif
   if (rc >= 0)
      execute_prog(tsc, tsc->values[0]);
   tsc->playflags = old_loadflags;
   restore_from_C_call(tsc);
   return tsc->values[0];
}

tr7_t tr7_apply0(tr7_engine_t tsc, const char *procname)
{
   return tr7_eval(tsc, tr7_cons(tsc, tr7_get_symbol(tsc, procname, 1), TR7_NIL));
}

unsigned tr7_get_values_count(tr7_engine_t tsc)
{
   return tsc->nvalues;
}

unsigned tr7_get_values(tr7_engine_t tsc, unsigned count, tr7_t *values)
{
   unsigned idx = count;
   while (idx > tsc->nvalues)
      values[--idx] = TR7_VOID;
   memcpy(values, &tsc->values[0], idx * sizeof *values);
   return tsc->nvalues;
}

tr7_t tr7_get_value(tr7_engine_t tsc, unsigned index)
{
   return index < tsc->nvalues ? tsc->values[index] : TR7_VOID;
}

tr7_t tr7_get_last_value(tr7_engine_t tsc)
{
   return tr7_get_value(tsc, 0);
}

#ifdef TR7_EXTRA_CODE
#undef _WANT_DECLARATIONS_
#include TR7_EXTRA_CODE
#endif
/*
* END OF PROGRAM SECTION
*
**************************************************************************
*/
#else /* _WANT_DECLARATIONS_ */
/*
**************************************************************************
*
* BEGIN OF DECLARATIVE SECTION
*
* Reset as ignored undefined macros
*/
#ifndef _BEGIN_LIBRARY_
# define _BEGIN_LIBRARY_(ID,NAME)
#endif
#ifndef _END_LIBRARY_
# define _END_LIBRARY_(ID)
#endif
#ifndef _SYMBOL_
# define _SYMBOL_(NAME,CODE)
#endif
#ifndef _SYNTAX_
# define _SYNTAX_(FUNC,NAME,CODE)
#endif
#ifndef _PROC___
# define _PROC___(FUNC,NAME,MIN,MAX,TYP,CODE)
#endif
#ifndef ___OPER_
# define ___OPER_(FUNC,CODE)
#endif
#ifndef ___OPER_
# define ___OPER_(FUNC,CODE)
#endif
#ifndef _INSTR__
# define _INSTR__(CODE,MOD1,MOD2)
#endif
/*
**************************************************************************
*
* Definition of symbols
*/
   _SYMBOL_("call/cc", CALL_CC)
   _SYMBOL_("λ", LAMBDA_CHAR)
   _SYMBOL_("else", ELSE)
   _SYMBOL_("=>", FEED_TO)
   _SYMBOL_("...", ELLIPSIS)
   _SYMBOL_("_", UNDERSCORE)
   _SYMBOL_("current-input-port", CURR_INPORT)
   _SYMBOL_("current-output-port", CURR_OUTPORT)
   _SYMBOL_("current-error-port", CURR_ERRPORT)
   _SYMBOL_("scheme", SCHEME)
   _SYMBOL_("only", ONLY)
   _SYMBOL_("except", EXCEPT)
   _SYMBOL_("prefix", PREFIX)
   _SYMBOL_("rename", RENAME)
   _SYMBOL_("library", LIBRARY)
   _SYMBOL_("export", EXPORT)
   _SYMBOL_("include-library-declarations", INCLUDE_LIB_DECL)
   _SYMBOL_("*compile-hook*", COMPILE_HOOK)
#if USE_SRFI_136
   _SYMBOL_("mutable", MUTABLE)
   _SYMBOL_("immutable", IMMUTABLE)
#endif

   _SYMBOL_("<synvar>", SYNVAR)
   _SYMBOL_("syntax-rules", SYNRULES)
/*
**************************************************************************
* Definition of instructions
*/
   _INSTR__(END,             NONE,  NONE)  /* (END)                  end of proc           */
   _INSTR__(GOTO,            RELOC, NONE)  /* (GOTO ...)             do nothing but go     */
   _INSTR__(IFTRUE,          RELOC, NONE)  /* (IFTRUE THEN . ELSE)   if VAL THEN ELSE      */
   _INSTR__(IFFALSE,         RELOC, NONE)  /* (IFFALSE THEN . ELSE)  if NOT(VAL) THEN ELSE */
   _INSTR__(ARG,             NONE,  NONE)  /* (ARG ...)              push VAL              */
   _INSTR__(CALLSELF,        UINT,  NONE)  /* (CALLSELF NARGS ...)   call VAL with NARGS   */
   _INSTR__(CALLG,           UINT,  VALUE) /* (CALLG NARGS LOC ...)  call (LOC) with NARGS */
   _INSTR__(CALL,            UINT,  NONE)  /* (CALL NARGS ...)       call VAL with NARGS   */
   _INSTR__(PROC,            UINT,  PROC)  /* (PROC NARGS P ...)     call P with NARGS     */
   _INSTR__(GETG,            VALUE, NONE)  /* (GETG LOC ...)         set VAL with [LOC]    */
   _INSTR__(GETGA,           VALUE, NONE)  /* (GETGA LOC ...)        push [LOC]            */
   _INSTR__(SETG,            VALUE, NONE)  /* (SETG LOC ...)         set [LOC] with VAL    */
   _INSTR__(GETC,            UINT,  UINT)  /* (GETC I N ...)         set VAL with C[N/I]   */
   _INSTR__(GETL,            UINT,  NONE)  /* (GETL I ...)           set VAL with L[I] !! must be just after GETC !! */
   _INSTR__(GETCA,           UINT,  UINT)  /* (GETCA I N ...)        push C[N/I]            */
   _INSTR__(GETLA,           UINT,  NONE)  /* (GETLA I ...)          push L[I]         !! must be just after GETCA !! */
   _INSTR__(SETC,            UINT,  UINT)  /* (SETC I N ...)         set C[N/I] with VAL    */
   _INSTR__(SETL,            UINT,  NONE)  /* (SETL I ...)           set L[I] with VAL !! must be just after SETC !! */
   _INSTR__(QUOTE,           VALUE, NONE)  /* (QUOTE X ...)          set VAL with X        */
   _INSTR__(QUOTA,           VALUE, NONE)  /* (QUOTA X ...)          push X                */
   _INSTR__(IMM,             DATA,  NONE)  /* (IMM X ...)            set VAL with TR7(X)   */
   _INSTR__(IMMA,            DATA,  NONE)  /* (IMMA X ...)           push TR7(X)           */
   _INSTR__(MVAL,            SINT,  NONE)  /* (MVAL S... ...)        multi set VALUES      */
   _INSTR__(MVALD,           UINT,  NONE)  /* (MVALD S... ...)       multi set VALUES dotted */
   _INSTR__(MSET,            UINT,  NONE)  /* (MSET CNT S... ...)    multi set CNT S from ARGS */
   _INSTR__(LAMBDA,          VALUE, NONE)  /* (LAMBDA FUN ...)       set VAL with lambda(FUN) */
   _INSTR__(CASE,            RELOC, VALUE) /* (CASE (V...) IFIN . ELSE...) if VAL in V... IFIN ELSE */
   _INSTR__(GUARD,           RELOC, NONE)  /* (GUARD CLAUSE ...)     push guard CLAUSE     */
   _INSTR__(UNGUARD,         RELOC, NONE)  /* (UNGUARD ...)          pop last guard        */
   _INSTR__(PARAMETER,       NONE,  NONE)  /* (PARAMETER PAR ...)    push PAR parameter and call it with ARGS/1 */
   _INSTR__(ENDPARAMETERIZE, UINT,  NONE)  /* (ENDPARAMETERIZE NPARS ...) pop NPARS parameters */
   _INSTR__(DEFRECORD,       UINT,  VALUE) /* (DEFRECORD DESC S... ...) create record of DESC and parent VAL and store artifacts in S... */
#if USE_SCHEME_CASE_LAMBDA
   _INSTR__(CASE_LAMBDA,     VALUE, NONE)  /* (CASE_LAMBDA FUNS ...)  set VAL with lambdas(FUNS) */
#endif
#if USE_SCHEME_LAZY
   _INSTR__(DELAY,           VALUE, NONE)  /* (DELAY EXPR ...)   set VAL with delay(expr) */
   _INSTR__(DELAYFORCE,      VALUE, NONE)  /* (DELAYFORCE EXPR ...)   set VAL with delayforce(expr) */
#endif
#if HAS_CHECK_TYPES_NO
   _INSTR__(PROCUNSAFE,      UINT,  PROC)  /* (PROCUNSAFE NARGS P ...)   call P with NARGS */
#endif
/*
**************************************************************************
* Definition of operators
*/
   ___OPER_(_oper_xrun, XRUN)                 /* (XRUN PROG PC CLOSURES) run CONt for frame */

   ___OPER_(_oper_ieval, IEVAL)
   ___OPER_(_oper_senv, SENV)
   ___OPER_(_oper_xcall, XCALL)
   ___OPER_(_oper_prog, PROG)
   ___OPER_(_oper_leave, LEAVE)

   ___OPER_(_oper_cont, CONT)
   ___OPER_(_oper_dwbefore, DWBEFORE)
   ___OPER_(_oper_dwafter, DWAFTER)
   ___OPER_(_oper_dwpop, DWPOP)

   ___OPER_(_oper_repl_read, REPL_READ)
   ___OPER_(_oper_repl_compile, REPL_COMPILE)
   ___OPER_(_oper_repl_eval, REPL_EVAL)
   ___OPER_(_oper_repl_print, REPL_PRINT)
   ___OPER_(_oper_repl_guard, REPL_GUARD)

   ___OPER_(_oper_mkparamcvt, MKPARAMCVT)
   ___OPER_(_oper_paramcvt, PARAMCVT)
   ___OPER_(_oper_parampop1, PARAMPOP1)

   ___OPER_(_oper_member_then, MEMBER_THEN)
   ___OPER_(_oper_assoc_then, ASSOC_THEN)

   ___OPER_(_oper_map_then, MAP_THEN)
   ___OPER_(_oper_strmap_then, STRMAP_THEN)
   ___OPER_(_oper_vecmap_then, VECMAP_THEN)
   ___OPER_(_oper_foreach_then, FOREACH_THEN)
   ___OPER_(_oper_strforeach_then, STRFOREACH_THEN)
   ___OPER_(_oper_vecforeach_then, VECFOREACH_THEN)
   ___OPER_(_oper_callvals_then, CALLVALS_THEN)

   ___OPER_(_oper_reraise, RERAISE)

   ___OPER_(_oper_close_port, CLOPORT)

#if USE_SCHEME_LAZY
   ___OPER_(_oper_save_forced, SAVE_FORCED)
   ___OPER_(_oper_force_delayed, FORCE_DELAYED)
#endif
/*
**************************************************************************
* SECTION procedures
* ------------------
*/

/*================= (scheme base) ====================*/
_BEGIN_LIBRARY_(base, "scheme/base")

   /*------- primitive (4.1) ----------*/
   _SYNTAX_(syn_quote, "quote", QUOTE)
   _SYNTAX_(syn_lambda, "lambda", LAMBDA)
   _SYNTAX_(syn_if, "if", IFTRUE)
   _SYNTAX_(syn_set, "set!", SET)
   _SYNTAX_(syn_include, "include", INCLUDE)
   _SYNTAX_(syn_include_ci, "include-ci", INCLUDE_CI)

   /*------- derived (4.2) ----------*/

   _SYNTAX_(syn_cond, "cond", COND)
   _SYNTAX_(syn_case, "case", CASE)

   _SYNTAX_(syn_and, "and", AND)
   _SYNTAX_(syn_or, "or", OR)

   _SYNTAX_(syn_when, "when", WHEN)
   _SYNTAX_(syn_unless, "unless", UNLESS)

   _SYNTAX_(syn_cond_expand, "cond-expand", COND_EXPAND)

   _SYNTAX_(syn_let, "let", LET0)
   _SYNTAX_(syn_letrec, "letrec", LET0REC)
   _SYNTAX_(syn_let_values, "let-values", LET0VAL)

   _SYNTAX_(syn_letstar, "let*", LET0STAR)
   _SYNTAX_(syn_letrecstar, "letrec*", LET0STARREC)
   _SYNTAX_(syn_letstarval, "let*-values", LET0STARVAL)

   _SYNTAX_(syn_begin, "begin", BEGIN)
   _SYNTAX_(syn_do, "do", DO)

   _PROC___(proc_mkparam, "make-parameter", 1, 2, TR7ARG_ANY TR7ARG_PROC, MKPARAM)
   _SYNTAX_(syn_parameterize, "parameterize", PARAM0)

   _SYNTAX_(syn_guard, "guard", GUARD)

   _SYNTAX_(syn_quasiquote, "quasiquote", QUASIQUOTE)
   _SYNTAX_(syn_unquote, "unquote", UNQUOTE)
   _SYNTAX_(syn_unquote_splicing, "unquote-splicing", UNQUOTE_SPLICING)

   /*------- macro (4.3) ----------*/
   _SYNTAX_(syn_let_syntax, "let-syntax", LET0SYN)
   _SYNTAX_(syn_letrec_syntax, "letrec-syntax", LET0SYNREC)
   _SYNTAX_(syn_syntax_error, "syntax-error", SYNERR)

   _SYNTAX_(syn_import, "import", IMPORT)

   _SYNTAX_(syn_define, "define", DEFINE)

   _SYNTAX_(syn_define_values, "define-values", DEFVAL)

   _SYNTAX_(syn_define_syntax, "define-syntax", DEFSYN)

   _SYNTAX_(syn_define_library, "define-library", DEFLIB)

   /*------- record ----------*/
   _SYNTAX_(syn_define_record_type, "define-record-type", DEFREC)

   /*------- equivalence ----------*/
   _PROC___(proc_eq, "eq?", 2, 2, TR7ARG_ANY, EQ)
   _PROC___(proc_eqv, "eqv?", 2, 2, TR7ARG_ANY, EQV)
   _PROC___(proc_equal, "equal?", 2, 2, TR7ARG_ANY, EQUAL)

   /*------- hashing ----------*/
   _PROC___(proc_hash, "hash", 1, 2, TR7ARG_ANY TR7ARG_NATURAL, HASH)
   _PROC___(proc_hash_by_identity, "hash-by-identity", 1, 2, TR7ARG_ANY TR7ARG_NATURAL, IDHASH)

   /*------- number ----------*/
   _PROC___(proc_is_number, "number?", 1, 1, TR7ARG_ANY, NUMBERP)
   _PROC___(proc_is_complex, "complex?", 1, 1, TR7ARG_ANY, COMPLEXP)
   _PROC___(proc_is_real, "real?", 1, 1, TR7ARG_ANY, REALP)
   _PROC___(proc_is_rational, "rational?", 1, 1, TR7ARG_ANY, RATIONALP)
   _PROC___(proc_is_integer, "integer?", 1, 1, TR7ARG_ANY, INTEGERP)

   _PROC___(proc_is_exact, "exact?", 1, 1, TR7ARG_NUMBER, EXACTP)
   _PROC___(proc_is_inexact, "inexact?", 1, 1, TR7ARG_NUMBER, INEXACTP)
   _PROC___(proc_is_exact_int, "exact-integer?", 1, 1, TR7ARG_NUMBER, EXACTINTP)

   _PROC___(proc_is_zero, "zero?", 1, 1, TR7ARG_NUMBER, ZEROP)
   _PROC___(proc_is_positive, "positive?", 1, 1, TR7ARG_NUMBER, POSITIVEP)
   _PROC___(proc_is_negative, "negative?", 1, 1, TR7ARG_NUMBER, NEGATIVEP)
   _PROC___(proc_is_odd, "odd?", 1, 1, TR7ARG_INTEGER, ODDP)
   _PROC___(proc_is_even, "even?", 1, 1, TR7ARG_INTEGER, EVENP)

   _PROC___(proc_number_eq, "=", 2, INF_ARG, TR7ARG_NUMBER, NUMEQ)
   _PROC___(proc_number_lt, "<", 2, INF_ARG, TR7ARG_NUMBER, LESS)
   _PROC___(proc_number_gt, ">", 2, INF_ARG, TR7ARG_NUMBER, GRE)
   _PROC___(proc_number_le, "<=", 2, INF_ARG, TR7ARG_NUMBER, LEQ)
   _PROC___(proc_number_ge, ">=", 2, INF_ARG, TR7ARG_NUMBER, GEQ)

   _PROC___(proc_max, "max", 1, INF_ARG, TR7ARG_NUMBER, MAX)
   _PROC___(proc_min, "min", 1, INF_ARG, TR7ARG_NUMBER, MIN)

   _PROC___(proc_add, "+", 0, INF_ARG, TR7ARG_NUMBER, ADD)
   _PROC___(proc_mul, "*", 0, INF_ARG, TR7ARG_NUMBER, MUL)
   _PROC___(proc_sub, "-", 1, INF_ARG, TR7ARG_NUMBER, SUB)
   _PROC___(proc_div, "/", 1, INF_ARG, TR7ARG_NUMBER, DIV)

   _PROC___(proc_abs, "abs", 1, 1, TR7ARG_NUMBER, ABS)

   _PROC___(proc_floor_div, "floor/", 2, 2, TR7ARG_INTEGER, FLOOR_DIV)
   _PROC___(proc_floor_quotient, "floor-quotient", 2, 2, TR7ARG_INTEGER, FLOOR_QUO)
   _PROC___(proc_floor_rem, "floor-remainder", 2, 2, TR7ARG_INTEGER, FLOOR_REM)
   _PROC___(proc_floor_rem, "modulo", 2, 2, TR7ARG_INTEGER, MOD) /* TODO ALIAS? */
   _PROC___(proc_truncate_div, "truncate/", 2, 2, TR7ARG_INTEGER, TRUNC_DIV)
   _PROC___(proc_truncate_quotient, "truncate-quotient", 2, 2, TR7ARG_INTEGER, TRUNC_QUO)
   _PROC___(proc_truncate_quotient, "quotient", 2, 2, TR7ARG_INTEGER, INTDIV) /* TODO ALIAS? */
   _PROC___(proc_truncate_rem, "truncate-remainder", 2, 2, TR7ARG_INTEGER, TRUNC_REM)
   _PROC___(proc_truncate_rem, "remainder", 2, 2, TR7ARG_INTEGER, REM) /* TODO ALIAS? */

   _PROC___(proc_gcd, "gcd", 0, INF_ARG, TR7ARG_INTEGER, GCD)
   _PROC___(proc_lcm, "lcm", 0, INF_ARG, TR7ARG_INTEGER, LCM)

   _PROC___(proc_square, "square", 1, 1, TR7ARG_NUMBER, SQUARE)

   _PROC___(proc_int_sqrt, "exact-integer-sqrt", 1, 1, TR7ARG_NUMBER, EXACT_SQRT)

#if USE_RATIOS
/*   _PROC___(_oper_numerator, "numerator", 1, 1, TR7ARG_NUMBER, NUMERATOR) */
/*   _PROC___(_oper_denominator, "denominator", 1, 1, TR7ARG_NUMBER, DENOMINATOR) */
/*   _PROC___(_oper_rationalize, "rationalize", 2, 2, TR7ARG_NUMBER, RATIONALIZE) */
#endif

   _PROC___(proc_num2str, "number->string", 1, 2, TR7ARG_NUMBER TR7ARG_INTEGER, NUM2STR)
   _PROC___(proc_str2num, "string->number", 1, 2, TR7ARG_STRING TR7ARG_INTEGER, STR2NUM)

#if USE_MATH
   _PROC___(proc_floor, "floor", 1, 1, TR7ARG_NUMBER, FLOOR)
   _PROC___(proc_truncate, "truncate", 1, 1, TR7ARG_NUMBER, TRUNCATE)
   _PROC___(proc_ceiling, "ceiling", 1, 1, TR7ARG_NUMBER, CEILING)
   _PROC___(proc_round, "round", 1, 1, TR7ARG_NUMBER, ROUND)

   _PROC___(proc_expt, "expt", 2, 2, TR7ARG_NUMBER, EXPT)

   _PROC___(proc_exact, "exact", 1, 1, TR7ARG_NUMBER, INEX2EX)
   _PROC___(proc_inexact, "inexact", 1, 1, TR7ARG_NUMBER, EX2INEX)
#endif

   /*------- boolean ----------*/
   _PROC___(proc_is_boolean, "boolean?", 1, 1, NULL, BOOLP)
   _PROC___(proc_not, "not", 1, 1, NULL, NOT)
   _PROC___(proc_boolean_eq, "boolean=?", 1, INF_ARG, NULL, BOOLEQP)

   /*------- pair and list ----------*/
   _PROC___(proc_is_pair, "pair?", 1, 1, TR7ARG_ANY, PAIRP)
   _PROC___(proc_cons, "cons", 2, 2, NULL, CONS)
   _PROC___(proc_car, "car", 1, 1, TR7ARG_PAIR, CAR)
   _PROC___(proc_cdr, "cdr", 1, 1, TR7ARG_PAIR, CDR)
   _PROC___(proc_set_car, "set-car!", 2, 2, TR7ARG_PAIR TR7ARG_ANY, SETCAR)
   _PROC___(proc_set_cdr, "set-cdr!", 2, 2, TR7ARG_PAIR TR7ARG_ANY, SETCDR)
   _PROC___(proc_caar, "caar", 1, 1, TR7ARG_PAIR, CAAR)
   _PROC___(proc_cadr, "cadr", 1, 1, TR7ARG_PAIR, CADR)
   _PROC___(proc_cdar, "cdar", 1, 1, TR7ARG_PAIR, CDAR)
   _PROC___(proc_cddr, "cddr", 1, 1, TR7ARG_PAIR, CDDR)
   _PROC___(proc_is_null, "null?", 1, 1, NULL, NULLP)
   _PROC___(proc_is_list, "list?", 1, 1, TR7ARG_ANY, LISTP)
   _PROC___(proc_make_list, "make-list", 1, 2, TR7ARG_NATURAL TR7ARG_ANY, MKLIST)
   _PROC___(proc_list, "list", 0, INF_ARG, TR7ARG_ANY, LIST)
   _PROC___(proc_length, "length", 1, 1, TR7ARG_ANY_LIST, LIST_LENGTH)
   _PROC___(proc_append, "append", 0, INF_ARG, NULL, APPEND)
   _PROC___(proc_reverse, "reverse", 1, 1, TR7ARG_PROPER_LIST, REVERSE)
   _PROC___(proc_list_tail, "list-tail", 2, 2, TR7ARG_ANY_LIST TR7ARG_NATURAL, LISTTAIL)
   _PROC___(proc_list_ref, "list-ref", 2, 2, TR7ARG_ANY_LIST TR7ARG_NATURAL, LISTREF)
   _PROC___(proc_list_set, "list-set!", 3, 3, TR7ARG_ANY_LIST TR7ARG_NATURAL TR7ARG_ANY, LISTSET)
   _PROC___(proc_list_copy, "list-copy", 1, 1, TR7ARG_ANY, LISTCOPY)
   _PROC___(proc_memq, "memq", 2, 2, TR7ARG_ANY TR7ARG_ANY_LIST, MEMQ)
   _PROC___(proc_memv, "memv", 2, 2, TR7ARG_ANY TR7ARG_ANY_LIST, MEMV)
   _PROC___(proc_member, "member", 2, 3, TR7ARG_ANY TR7ARG_ANY_LIST TR7ARG_PROC, MEMBER)
   _PROC___(proc_assq, "assq", 2, 2, TR7ARG_ANY TR7ARG_ANY_LIST, ASSQ)
   _PROC___(proc_assv, "assv", 2, 2, TR7ARG_ANY TR7ARG_ANY_LIST, ASSV)
   _PROC___(proc_assoc, "assoc", 2, 3, TR7ARG_ANY TR7ARG_ANY_LIST TR7ARG_PROC, ASSOC)

   /*------- symbol ----------*/
   _PROC___(proc_is_symbol, "symbol?", 1, 1, TR7ARG_ANY, SYMBOLP)
   _PROC___(proc_symbol_eq, "symbol=?", 1, INF_ARG, TR7ARG_ANY, SYMBOLEQP)
   _PROC___(proc_sym2str, "symbol->string", 1, 1, TR7ARG_SYMBOL, SYM2STR)
   _PROC___(proc_str2sym, "string->symbol", 1, 1, TR7ARG_STRING, STR2SYM)
   _PROC___(proc_symbol_hash, "symbol-hash", 1, 2, TR7ARG_SYMBOL TR7ARG_NATURAL, SYMHASH)

   /*------- char ----------*/
   _PROC___(proc_is_char, "char?", 1, 1, TR7ARG_ANY, CHARP)
   _PROC___(proc_char_eq, "char=?", 2, INF_ARG, TR7ARG_CHAR, CHAREQP)
   _PROC___(proc_char_lt, "char<?", 2, INF_ARG, TR7ARG_CHAR, CHARLTP)
   _PROC___(proc_char_gt, "char>?", 2, INF_ARG, TR7ARG_CHAR, CHARGTP)
   _PROC___(proc_char_le, "char<=?", 2, INF_ARG, TR7ARG_CHAR, CHARLEP)
   _PROC___(proc_char_ge, "char>=?", 2, INF_ARG, TR7ARG_CHAR, CHARGEP)
   _PROC___(proc_char2int, "char->integer", 1, 1, TR7ARG_CHAR, CHAR2INT)
   _PROC___(proc_int2char, "integer->char", 1, 1, TR7ARG_NATURAL, INT2CHAR)

   /*------- string ----------*/
   _PROC___(proc_is_string, "string?", 1, 1, TR7ARG_ANY, STRINGP)
   _PROC___(proc_make_string, "make-string", 1, 2, TR7ARG_NATURAL TR7ARG_CHAR, MKSTRING)
   _PROC___(proc_string, "string", 0, INF_ARG, TR7ARG_CHAR, STRING)
   _PROC___(proc_string_length, "string-length", 1, 1, TR7ARG_STRING, STRLEN)
   _PROC___(proc_string_ref, "string-ref", 2, 2, TR7ARG_STRING TR7ARG_NATURAL, STRREF)
   _PROC___(proc_string_set, "string-set!", 3, 3, TR7ARG_STRING TR7ARG_NATURAL TR7ARG_CHAR, STRSET)
   _PROC___(proc_string_eq, "string=?", 2, INF_ARG, TR7ARG_STRING, STRING_EQP)
   _PROC___(proc_string_lt, "string<?", 2, INF_ARG, TR7ARG_STRING, STRING_LTP)
   _PROC___(proc_string_gt, "string>?", 2, INF_ARG, TR7ARG_STRING, STRING_GTP)
   _PROC___(proc_string_le, "string<=?", 2, INF_ARG, TR7ARG_STRING, STRING_LEP)
   _PROC___(proc_string_ge, "string>=?", 2, INF_ARG, TR7ARG_STRING, STRING_GEP)
   _PROC___(proc_string_append, "string-append", 0, INF_ARG, TR7ARG_STRING, STRAPPEND)
   _PROC___(proc_string_to_list, "string->list", 1, 3, TR7ARG_STRING TR7ARG_NATURAL TR7ARG_NATURAL, STR2LST)
   _PROC___(proc_list_to_string, "list->string", 1, 1, TR7ARG_PROPER_LIST, LST2STR)
   _PROC___(proc_string_copy, "substring", 1, 3, TR7ARG_STRING TR7ARG_NATURAL, SUBSTR)
   _PROC___(proc_string_copy, "string-copy", 1, 3, TR7ARG_STRING TR7ARG_NATURAL, STRCPY) /* TODO ALIAS? */
   _PROC___(proc_string_copy_to, "string-copy!", 3, 5, TR7ARG_STRING TR7ARG_NATURAL TR7ARG_STRING TR7ARG_NATURAL, STRCPYSET)
   _PROC___(proc_string_fill, "string-fill!", 2, 4, TR7ARG_STRING TR7ARG_CHAR TR7ARG_NATURAL, STRFILL)
   _PROC___(proc_string_hash, "string-hash", 1, 2, TR7ARG_STRING TR7ARG_NATURAL, STRHASH)

   /*------- vector ----------*/
   _PROC___(proc_is_vector, "vector?", 1, 1, TR7ARG_ANY, VECTORP)
   _PROC___(proc_make_vector, "make-vector", 1, 2, TR7ARG_NATURAL TR7ARG_ANY, MKVECTOR)
   _PROC___(proc_vector, "vector", 0, INF_ARG, NULL, VECTOR)
   _PROC___(proc_vector_length, "vector-length", 1, 1, TR7ARG_VECTOR, VECLEN)
   _PROC___(proc_vector_ref, "vector-ref", 2, 2, TR7ARG_VECTOR TR7ARG_NATURAL, VECREF)
   _PROC___(proc_vector_set, "vector-set!", 3, 3, TR7ARG_VECTOR TR7ARG_NATURAL TR7ARG_ANY, VECSET)
   _PROC___(proc_vector_to_list, "vector->list", 1, 3, TR7ARG_VECTOR TR7ARG_NATURAL, VEC2LST)
   _PROC___(proc_list_to_vector, "list->vector", 1, 1, TR7ARG_PROPER_LIST, LST2VEC)
   _PROC___(proc_vector_to_string, "vector->string", 1, 3, TR7ARG_VECTOR TR7ARG_NATURAL, VEC2STR)
   _PROC___(proc_string_to_vector, "string->vector", 1, 3, TR7ARG_STRING TR7ARG_NATURAL, STR2VEC)
   _PROC___(proc_vector_copy, "vector-copy", 1, 3, TR7ARG_VECTOR TR7ARG_NATURAL, VECCPY)
   _PROC___(proc_vector_copy_to, "vector-copy!", 3, 5, TR7ARG_VECTOR TR7ARG_NATURAL TR7ARG_VECTOR TR7ARG_NATURAL, VECCPYSET)
   _PROC___(proc_vector_append, "vector-append", 1, INF_ARG, TR7ARG_VECTOR, VECAPPEND)
   _PROC___(proc_vector_fill, "vector-fill!", 2, 4, TR7ARG_VECTOR TR7ARG_ANY TR7ARG_NATURAL, VECFILL)

   /*------- bytevector ----------*/
   _PROC___(proc_is_bytevector, "bytevector?", 1, 1, TR7ARG_ANY, BYTEVECP)
   _PROC___(proc_bytevector, "bytevector", 0, INF_ARG, TR7ARG_BYTE, BYTEVECTOR)
   _PROC___(proc_make_bytevector, "make-bytevector", 1, 2, TR7ARG_NATURAL TR7ARG_BYTE, MKBYTEVEC)
   _PROC___(proc_bytevector_length, "bytevector-length", 1, 1, TR7ARG_BYTEVEC, BYTEVECLEN)
   _PROC___(proc_bytevector_u8_ref, "bytevector-u8-ref", 2, 2, TR7ARG_BYTEVEC TR7ARG_NATURAL, BYTEVECREF)
   _PROC___(proc_bytevector_u8_set, "bytevector-u8-set!", 3, 3, TR7ARG_BYTEVEC TR7ARG_NATURAL TR7ARG_BYTE, BYTEVECSET)
   _PROC___(proc_bytevector_copy, "bytevector-copy", 1, 3, TR7ARG_BYTEVEC TR7ARG_NATURAL, BYTEVECCOPY)
   _PROC___(proc_bytevector_copy_to, "bytevector-copy!", 3, 5, TR7ARG_BYTEVEC TR7ARG_NATURAL TR7ARG_BYTEVEC TR7ARG_NATURAL, BYTEVECCOPYSET)
   _PROC___(proc_bytevector_append, "bytevector-append", 1, INF_ARG, TR7ARG_BYTEVEC, BYTEVECAPPEND)
   _PROC___(proc_bytevector_fill, "bytevector-fill!", 2, 4, TR7ARG_BYTEVEC TR7ARG_BYTE TR7ARG_NATURAL, BYTEVECFILL)
   _PROC___(proc_utf8_to_string, "utf8->string", 1, 3, TR7ARG_BYTEVEC TR7ARG_NATURAL, BYTEVECSTRING)
   _PROC___(proc_string_to_utf8, "string->utf8", 1, 3, TR7ARG_STRING TR7ARG_NATURAL, STRINGBYTEVEC)

   /*------- control ----------*/
   _PROC___(proc_procedure, "procedure?", 1, 1, TR7ARG_ANY, PROCP)
   _PROC___(proc_apply, "apply", 1, INF_ARG, TR7ARG_PROC TR7ARG_ANY, APPLY)
   _PROC___(proc_map, "map", 2, INF_ARG, TR7ARG_PROC TR7ARG_ANY_LIST, MAP)
   _PROC___(proc_strmap, "string-map", 2, INF_ARG, TR7ARG_PROC TR7ARG_STRING, STRMAP)
   _PROC___(proc_vecmap, "vector-map", 2, INF_ARG, TR7ARG_PROC TR7ARG_VECTOR, VECMAP)
   _PROC___(proc_foreach, "for-each", 2, INF_ARG, TR7ARG_PROC TR7ARG_ANY_LIST, FOREACH)
   _PROC___(proc_strforeach, "string-for-each", 2, INF_ARG, TR7ARG_PROC TR7ARG_STRING, STRFOREACH)
   _PROC___(proc_vecforeach, "vector-for-each", 2, INF_ARG, TR7ARG_PROC TR7ARG_VECTOR, VECFOREACH)
   _PROC___(proc_callcc, "call-with-current-continuation", 1, 1, TR7ARG_PROC, CALLCC)
   _PROC___(proc_values, "values", 0, INF_ARG, TR7ARG_ANY, VALUES)
   _PROC___(proc_callvals, "call-with-values", 2, 2, TR7ARG_ANY, CALLVALS)
   _PROC___(proc_dynamic_wind, "dynamic-wind", 3, 3, TR7ARG_PROC, DYNWIND)

   /*------- exception ----------*/
   _PROC___(proc_with_exception_handler, "with-exception-handler", 2, 2, TR7ARG_PROC, WITH_EXC_HNDL)
   _PROC___(proc_raise, "raise", 1, 1, NULL, RAISE)
   _PROC___(proc_raise_continuable, "raise-continuable", 1, 1, NULL, RAISECON)
   _PROC___(proc_error, "error", 1, INF_ARG, TR7ARG_STRING TR7ARG_ANY, ERROR)
   _PROC___(proc_is_error_object, "error-object?", 1, 1, NULL, ERROROBJP)
   _PROC___(proc_error_msg, "error-object-message", 1, 1, TR7ARG_ERROBJ, ERRORMSG)
   _PROC___(proc_error_irritants, "error-object-irritants", 1, 1, TR7ARG_ERROBJ, ERRORIRRIT)

   _PROC___(proc_is_read_error, "read-error?", 1, 1, NULL, READERRP)
   _PROC___(proc_is_file_error, "file-error?", 1, 1, NULL, FILEERRP)

   /*------- port ----------*/
   _PROC___(proc_call_with_port, "call-with-port", 2, 2, TR7ARG_PORT TR7ARG_PROC, CALLWPORT)

   _PROC___(proc_is_input_port, "input-port?", 1, 1, TR7ARG_ANY, INPORTP)
   _PROC___(proc_is_output_port, "output-port?", 1, 1, TR7ARG_ANY, OUTPORTP)
   _PROC___(proc_is_textual_port, "textual-port?", 1, 1, TR7ARG_ANY, TXTPORTP)
   _PROC___(proc_is_binary_port, "binary-port?", 1, 1, TR7ARG_ANY, BINPORTP)
   _PROC___(proc_is_port, "port?", 1, 1, TR7ARG_ANY, PORTP)

   _PROC___(proc_is_input_port, "input-port-open?", 1, 1, TR7ARG_PORT, INPORTOPENP) /* ALIAS */
   _PROC___(proc_is_output_port, "output-port-open?", 1, 1, TR7ARG_PORT, OUTPORTOPENP) /* ALIAS */

   _PROC___(proc_close_port, "close-port", 1, 1, TR7ARG_PORT, CLOSE_PORT)
   _PROC___(proc_close_input_port, "close-input-port", 1, 1, TR7ARG_INPORT, CLOSE_INPORT)
   _PROC___(proc_close_output_port, "close-output-port", 1, 1, TR7ARG_OUTPORT, CLOSE_OUTPORT)

   _PROC___(proc_open_input_string, "open-input-string", 1, 1, TR7ARG_STRING, OPEN_INSTRING)
   _PROC___(proc_open_output_string, "open-output-string", 0, 0, NULL, OPEN_OUTSTRING)
   _PROC___(proc_get_output_string, "get-output-string", 1, 1, TR7ARG_TXT_OUTPORT, GET_OUTSTRING)

   _PROC___(proc_open_input_bytevector, "open-input-bytevector", 1, 1, TR7ARG_BYTEVEC, OPEN_INBYTEVEC)
   _PROC___(proc_open_output_bytevector, "open-output-bytevector", 0, 0, NULL, OPEN_OUTBYTEVEC)
   _PROC___(proc_get_output_bytevector, "get-output-bytevector", 1, 1, TR7ARG_BIN_OUTPORT, GET_OUTBYTEVEC)

   /*------- input ----------*/
   _PROC___(proc_read_char, "read-char", 0, 1, TR7ARG_TXT_INPORT, READ_CHAR)
   _PROC___(proc_peek_char, "peek-char", 0, 1, TR7ARG_TXT_INPORT, PEEK_CHAR)
   _PROC___(proc_read_line, "read-line", 0, 1, TR7ARG_TXT_INPORT, READ_LINE)
   _PROC___(proc_is_eof_object, "eof-object?", 1, 1, NULL, EOFOBJP)
   _PROC___(proc_eof_object, "eof-object", 0, 0, NULL, EOFOBJ)
   _PROC___(proc_is_char_ready, "char-ready?", 0, 1, TR7ARG_TXT_INPORT, CHAR_READY)
   _PROC___(proc_read_string, "read-string", 1, 2, TR7ARG_NATURAL TR7ARG_TXT_INPORT, READ_STRING)
   _PROC___(proc_read_u8, "read-u8", 0, 1, TR7ARG_BIN_INPORT, READ_BYTE)
   _PROC___(proc_peek_u8, "peek-u8", 0, 1, TR7ARG_BIN_INPORT, PEEK_BYTE)
   _PROC___(proc_is_u8_ready, "u8-ready?", 0, 1, TR7ARG_BIN_INPORT, BYTE_READY)
   _PROC___(proc_read_bytevector, "read-bytevector", 1, 2, TR7ARG_NATURAL TR7ARG_BIN_INPORT, READ_BYTEVEC)
   _PROC___(proc_read_bytevector_in, "read-bytevector!", 1, 4, TR7ARG_BYTEVEC TR7ARG_BIN_INPORT TR7ARG_NATURAL, READ_BYTEVEC_IN)

   /*------- output ----------*/
   _PROC___(proc_write_char, "write-char", 1, 2, TR7ARG_CHAR TR7ARG_TXT_OUTPORT, WRITE_CHAR)
   _PROC___(proc_write_newline, "newline", 0, 1, TR7ARG_TXT_OUTPORT, NEWLINE)
   _PROC___(proc_write_string, "write-string", 1, 4, TR7ARG_STRING TR7ARG_TXT_OUTPORT TR7ARG_NATURAL, WRITE_STRING)
   _PROC___(proc_write_u8, "write-u8", 1, 2, TR7ARG_NATURAL TR7ARG_BIN_OUTPORT, WRITE_U8)
   _PROC___(proc_write_bytevector, "write-bytevector", 1, 4, TR7ARG_BYTEVEC TR7ARG_BIN_OUTPORT TR7ARG_NATURAL, WRITE_BYTEVEC)
   _PROC___(proc_flush_output_port, "flush-output-port", 0, 1, TR7ARG_OUTPORT, FLUSH_OUTPORT)

   /*------- feature ----------*/
   _PROC___(proc_features, "features", 0, 0, NULL, FEATURES)

   /*------- SHARP HOOKING ----------*/
   _PROC___(proc_sharp, "*sharp-hook*", 1, 1, NULL, SHARP)

_END_LIBRARY_(base)

/*================= (scheme case-lambda) ====================*/
#if USE_SCHEME_CASE_LAMBDA
_BEGIN_LIBRARY_(case_lambda, "scheme/case-lambda")
   _SYNTAX_(syn_case_lambda, "case-lambda", CASE_LAMBDA)
_END_LIBRARY_(case_lambda)
#endif

/*================= (scheme char) ====================*/
#if USE_SCHEME_CHAR
_BEGIN_LIBRARY_(char, "scheme/char")
   /* char */
   _PROC___(proc_char_eq_ci, "char-ci=?", 2, INF_ARG, TR7ARG_CHAR, CHARCIEQP)
   _PROC___(proc_char_lt_ci, "char-ci<?", 2, INF_ARG, TR7ARG_CHAR, CHARCILTP)
   _PROC___(proc_char_gt_ci, "char-ci>?", 2, INF_ARG, TR7ARG_CHAR, CHARCIGTP)
   _PROC___(proc_char_le_ci, "char-ci<=?", 2, INF_ARG, TR7ARG_CHAR, CHARCILEP)
   _PROC___(proc_char_ge_ci, "char-ci>=?", 2, INF_ARG, TR7ARG_CHAR, CHARCIGEP)
   _PROC___(proc_char_is_alpha, "char-alphabetic?", 1, 1, TR7ARG_CHAR, CHARALPHAP)
   _PROC___(proc_char_is_num, "char-numeric?", 1, 1, TR7ARG_CHAR, CHARNUMP)
   _PROC___(proc_char_is_space, "char-whitespace?", 1, 1, TR7ARG_CHAR, CHARWHITEP)
   _PROC___(proc_char_is_upper, "char-upper-case?", 1, 1, TR7ARG_CHAR, CHARUPPERP)
   _PROC___(proc_char_is_lower, "char-lower-case?", 1, 1, TR7ARG_CHAR, CHARLOWERP)
   _PROC___(proc_char_digit_value, "digit-value", 1, 1, TR7ARG_CHAR, DIGVAL)
   _PROC___(proc_char_upcase, "char-upcase", 1, 1, TR7ARG_CHAR, CHARUP)
   _PROC___(proc_char_downcase, "char-downcase", 1, 1, TR7ARG_CHAR, CHARDOWN)
   _PROC___(proc_char_downcase, "char-foldcase", 1, 1, TR7ARG_CHAR, CHARFOLD) /* ALIAS */
   /* string */
   _PROC___(proc_string_eq_ci, "string-ci=?", 2, INF_ARG, TR7ARG_STRING, STRINGCI_EQP)
   _PROC___(proc_string_lt_ci, "string-ci<?", 2, INF_ARG, TR7ARG_STRING, STRINGCI_LTP)
   _PROC___(proc_string_gt_ci, "string-ci>?", 2, INF_ARG, TR7ARG_STRING, STRINGCI_GTP)
   _PROC___(proc_string_le_ci, "string-ci<=?", 2, INF_ARG, TR7ARG_STRING, STRINGCI_LEP)
   _PROC___(proc_string_ge_ci, "string-ci>=?", 2, INF_ARG, TR7ARG_STRING, STRINGCI_GEP)
   _PROC___(proc_string_upcase, "string-upcase", 1, 1, TR7ARG_STRING, STRUP)
   _PROC___(proc_string_downcase, "string-downcase", 1, 1, TR7ARG_STRING, STRDOWN)
   _PROC___(proc_string_downcase, "string-foldcase", 1, 1, TR7ARG_STRING, STRFOLD) /* ALIAS */
   _PROC___(proc_char_is_unicode, "char-unicode?", 1, 1, TR7ARG_CHAR, CHARUNICODE)
   _PROC___(proc_string_ci_hash, "string-ci-hash", 1, 2, TR7ARG_STRING TR7ARG_NATURAL, STRCIHASH)
_END_LIBRARY_(char)
#endif

/*================= (scheme complex) ====================*/
#if USE_SCHEME_COMPLEX
_BEGIN_LIBRARY_(complex, "scheme/complex")
/*   _PROC___(_oper_make_rectangular, "make-rectangular", 2, 2, TR7ARG_NUMBER, CPLX_MKREC) */
/*   _PROC___(_oper_make_polar, "make-polar", 2, 2, TR7ARG_NUMBER, CPLX_MKPOL) */
/*   _PROC___(_oper_real_part, "real-part", 1, 1, TR7ARG_NUMBER, CPLX_REAL) */
/*   _PROC___(_oper_imag_part, "imag-part", 1, 1, TR7ARG_NUMBER, CPLX_IMAG) */
/*   _PROC___(_oper_magnitude, "magnitude", 1, 1, TR7ARG_NUMBER, CPLX_MAGN) */
/*   _PROC___(_oper_angle, "angle", 1, 1, TR7ARG_NUMBER, CPLX_ANGLE) */
_END_LIBRARY_(complex)
#endif

/*================= (scheme cxr) ====================*/
#if USE_SCHEME_CXR
_BEGIN_LIBRARY_(cxr, "scheme/cxr")
   _PROC___(proc_caaar, "caaar", 1, 1, TR7ARG_PAIR, CAAAR)
   _PROC___(proc_caadr, "caadr", 1, 1, TR7ARG_PAIR, CAADR)
   _PROC___(proc_cadar, "cadar", 1, 1, TR7ARG_PAIR, CADAR)
   _PROC___(proc_caddr, "caddr", 1, 1, TR7ARG_PAIR, CADDR)
   _PROC___(proc_cdaar, "cdaar", 1, 1, TR7ARG_PAIR, CDAAR)
   _PROC___(proc_cdadr, "cdadr", 1, 1, TR7ARG_PAIR, CDADR)
   _PROC___(proc_cddar, "cddar", 1, 1, TR7ARG_PAIR, CDDAR)
   _PROC___(proc_cdddr, "cdddr", 1, 1, TR7ARG_PAIR, CDDDR)
   _PROC___(proc_caaaar, "caaaar", 1, 1, TR7ARG_PAIR, CAAAAR)
   _PROC___(proc_caaadr, "caaadr", 1, 1, TR7ARG_PAIR, CAAADR)
   _PROC___(proc_caadar, "caadar", 1, 1, TR7ARG_PAIR, CAADAR)
   _PROC___(proc_caaddr, "caaddr", 1, 1, TR7ARG_PAIR, CAADDR)
   _PROC___(proc_cadaar, "cadaar", 1, 1, TR7ARG_PAIR, CADAAR)
   _PROC___(proc_cadadr, "cadadr", 1, 1, TR7ARG_PAIR, CADADR)
   _PROC___(proc_caddar, "caddar", 1, 1, TR7ARG_PAIR, CADDAR)
   _PROC___(proc_cadddr, "cadddr", 1, 1, TR7ARG_PAIR, CADDDR)
   _PROC___(proc_cdaaar, "cdaaar", 1, 1, TR7ARG_PAIR, CDAAAR)
   _PROC___(proc_cdaadr, "cdaadr", 1, 1, TR7ARG_PAIR, CDAADR)
   _PROC___(proc_cdadar, "cdadar", 1, 1, TR7ARG_PAIR, CDADAR)
   _PROC___(proc_cdaddr, "cdaddr", 1, 1, TR7ARG_PAIR, CDADDR)
   _PROC___(proc_cddaar, "cddaar", 1, 1, TR7ARG_PAIR, CDDAAR)
   _PROC___(proc_cddadr, "cddadr", 1, 1, TR7ARG_PAIR, CDDADR)
   _PROC___(proc_cdddar, "cdddar", 1, 1, TR7ARG_PAIR, CDDDAR)
   _PROC___(proc_cddddr, "cddddr", 1, 1, TR7ARG_PAIR, CDDDDR)
_END_LIBRARY_(cxr)
#endif

/*================= (scheme eval) ====================*/
#if USE_SCHEME_EVAL
_BEGIN_LIBRARY_(eval, "scheme/eval")
   _PROC___(proc_environment, "environment", 0, INF_ARG, NULL, ENV)
   _PROC___(proc_eval, "eval", 1, 2, TR7ARG_ANY TR7ARG_ENVIRONMENT, EVAL)
_END_LIBRARY_(eval)
#endif

/*================= (scheme file) ====================*/
#if USE_SCHEME_FILE
_BEGIN_LIBRARY_(file, "scheme/file")
   _PROC___(proc_call_with_input_file, "call-with-input-file", 2, 2, TR7ARG_STRING TR7ARG_PROC, CALLWINFILE)
   _PROC___(proc_call_with_output_file, "call-with-output-file", 2, 2, TR7ARG_STRING TR7ARG_PROC, CALLWOUTFILE)
   _PROC___(proc_delete_file, "delete-file", 1, 1, TR7ARG_STRING, DELETE_FILE)
   _PROC___(proc_file_exists, "file-exists?", 1, 1, TR7ARG_STRING, FILE_EXISTS_P)

   _PROC___(proc_open_binary_input_file, "open-binary-input-file", 1, 1, TR7ARG_STRING, OPEN_BININFILE)
   _PROC___(proc_open_binary_output_file, "open-binary-output-file", 1, 1, TR7ARG_STRING, OPEN_BINOUTFILE)
   _PROC___(proc_open_input_file, "open-input-file", 1, 1, TR7ARG_STRING, OPEN_INFILE)
   _PROC___(proc_open_output_file, "open-output-file", 1, 1, TR7ARG_STRING, OPEN_OUTFILE)

   _PROC___(proc_with_input_file, "with-input-from-file", 2, 2, TR7ARG_STRING TR7ARG_PROC, WITHINFILE)
   _PROC___(proc_with_output_file, "with-output-to-file", 2, 2, TR7ARG_STRING TR7ARG_PROC, WITHOUTFILE)
_END_LIBRARY_(file)
#endif

/*================= (scheme inexact) ====================*/
#if USE_SCHEME_INEXACT
_BEGIN_LIBRARY_(inexact, "scheme/inexact")
   _PROC___(proc_is_finite, "finite?", 1, 1, TR7ARG_NUMBER, FINITEP)
   _PROC___(proc_is_infinite, "infinite?", 1, 1, TR7ARG_NUMBER, INFINITEP)
   _PROC___(proc_is_nan, "nan?", 1, 1, TR7ARG_NUMBER, NANP)

#if USE_MATH
   _PROC___(proc_exp, "exp", 1, 1, TR7ARG_NUMBER, EXP)
   _PROC___(proc_log, "log", 1, 2, TR7ARG_NUMBER, LOG) /* 1, 2!! */
   _PROC___(proc_sin, "sin", 1, 1, TR7ARG_NUMBER, SIN)
   _PROC___(proc_cos, "cos", 1, 1, TR7ARG_NUMBER, COS)
   _PROC___(proc_tan, "tan", 1, 1, TR7ARG_NUMBER, TAN)
   _PROC___(proc_asin, "asin", 1, 1, TR7ARG_NUMBER, ASIN)
   _PROC___(proc_acos, "acos", 1, 1, TR7ARG_NUMBER, ACOS)
   _PROC___(proc_atan, "atan", 1, 2, TR7ARG_NUMBER, ATAN)
   _PROC___(proc_sqrt, "sqrt", 1, 1, TR7ARG_NUMBER, SQRT)
#endif
_END_LIBRARY_(inexact)
#endif

/*================= (scheme lazy) ====================*/
#if USE_SCHEME_LAZY
_BEGIN_LIBRARY_(lazy, "scheme/lazy")
   _SYNTAX_(syn_delay, "delay", DELAY)
   _PROC___(proc_is_promise, "promise?", 1, 1, TR7ARG_ANY, PROMISEP)
   _PROC___(proc_force, "force", 1, 1, TR7ARG_ANY, FORCE)
   _SYNTAX_(syn_delay_force, "delay-force", DELAYFORCE)
   _PROC___(proc_make_promise, "make-promise", 1, 1, TR7ARG_ANY, MKPROMISE)
_END_LIBRARY_(lazy)
#endif

/*================= (scheme load) ====================*/
#if USE_SCHEME_LOAD
_BEGIN_LIBRARY_(load, "scheme/load")
   _PROC___(proc_load, "load", 1, 2, TR7ARG_STRING TR7ARG_ENVIRONMENT, LOAD)
_END_LIBRARY_(load)
#endif

/*================= (scheme process-context) ====================*/
#if USE_SCHEME_PROCESS_CONTEXT
_BEGIN_LIBRARY_(process_context, "scheme/process-context")
   _PROC___(proc_command_line, "command-line", 0, 0, NULL, CMDLINE)
   _PROC___(proc_exit, "exit", 0, 1, NULL, EXIT)
   _PROC___(proc_emergency_exit, "emergency-exit", 0, 1, NULL, EMERGEXIT)
   _PROC___(proc_get_env_var, "get-environment-variable", 1, 1, TR7ARG_STRING, GETENVVAR)
   _PROC___(proc_get_env_vars, "get-environment-variables", 0, 0, NULL, GETENVVARS)
_END_LIBRARY_(process_context)
#endif

/*================= (scheme read) ====================*/
#if USE_SCHEME_READ
_BEGIN_LIBRARY_(read, "scheme/read")
   _PROC___(proc_read, "read", 0, 1, TR7ARG_TXT_INPORT, READ)
_END_LIBRARY_(read)
#endif

/*================= (scheme repl) ====================*/
#if USE_SCHEME_REPL
_BEGIN_LIBRARY_(repl, "scheme/repl")
   _PROC___(proc_interaction_environment, "interaction-environment", 0, 0, NULL, INT_ENV)
_END_LIBRARY_(repl)
#endif

/*================= (scheme time) ====================*/
#if USE_SCHEME_TIME
_BEGIN_LIBRARY_(time, "scheme/time")
   _PROC___(proc_current_second, "current-second", 0, 0, NULL, CURRENT_SECOND)
   _PROC___(proc_current_jiffy, "current-jiffy", 0, 0, NULL, CURRENT_JIFFY)
   _PROC___(proc_jiffies_per_second, "jiffies-per-second", 0, 0, NULL, JIFFIES_PER_SECOND)
_END_LIBRARY_(time)
#endif

/*================= (scheme write) ====================*/
#if USE_SCHEME_WRITE
_BEGIN_LIBRARY_(write, "scheme/write")
   _PROC___(proc_write, "write", 1, 2, TR7ARG_ANY TR7ARG_TXT_OUTPORT, WRITE)
   _PROC___(proc_write_shared, "write-shared", 1, 2, TR7ARG_ANY TR7ARG_TXT_OUTPORT, WRITE_SHARED)
   _PROC___(proc_write_simple, "write-simple", 1, 2, TR7ARG_ANY TR7ARG_TXT_OUTPORT, WRITE_SIMPLE)
   _PROC___(proc_display, "display", 1, 2, TR7ARG_ANY TR7ARG_TXT_OUTPORT, DISPLAY)
_END_LIBRARY_(write)
#endif

/*================= (scheme box) ====================*/
#if USE_SCHEME_BOX
_BEGIN_LIBRARY_(box, "scheme/box")
   _PROC___(proc_box, "box", 1, 1, TR7ARG_ANY, BOX)
   _PROC___(proc_is_box, "box?", 1, 1, TR7ARG_ANY, BOXP)
   _PROC___(proc_unbox, "unbox", 1, 1, TR7ARG_BOX, UNBOX)
   _PROC___(proc_set_box, "set-box!", 1, 2, TR7ARG_BOX TR7ARG_ANY, SETBOX)
_END_LIBRARY_(box)
#endif

/*================= (srfi 136) ====================*/
#if USE_SRFI_136
_BEGIN_LIBRARY_(srfi_136, "srfi/136")
   _PROC___(proc_is_record, "record?", 1, 1, TR7ARG_ANY, RECORDP)
   _PROC___(proc_is_record_desc, "record-type-descriptor?", 1, 1, TR7ARG_ANY, RECDESCP)
   _PROC___(proc_record_desc, "record-type-descriptor", 1, 1, TR7ARG_RECORD, RECDESC)
   _PROC___(proc_record_desc_pred, "record-type-predicate", 1, 1, TR7ARG_RECORD_DESC, RECDESCPRED)
   _PROC___(proc_record_desc_name, "record-type-name", 1, 1, TR7ARG_RECORD_DESC, RECDESCNAME)
   _PROC___(proc_record_desc_parent, "record-type-parent", 1, 1, TR7ARG_RECORD_DESC, RECDESCPARENT)
   _PROC___(proc_record_desc_fields, "record-type-fields", 1, 1, TR7ARG_RECORD_DESC, RECDESCFIELDS)
   _PROC___(proc_make_record_desc, "make-record-type-descriptor", 2, 3, TR7ARG_SYMBOL TR7ARG_PROPER_LIST TR7ARG_ANY, MKRECDESC)
   _PROC___(proc_make_record, "make-record", 2, 2, TR7ARG_RECORD_DESC TR7ARG_VECTOR, MAKERECORD)
_END_LIBRARY_(srfi_136)
#endif

/*================= (tr7 misc) ====================*/
#if USE_TR7_MISC
_BEGIN_LIBRARY_(tr7, "tr7/misc")
   _PROC___(proc_tr7_id, "tr7-id", 0, 0, NULL, GETID)
   _PROC___(proc_tr7_version, "tr7-version", 0, 0, NULL, GETVERSION)
   _PROC___(proc_scheme_paths, "scheme-paths", 0, 0, NULL, SCHEMEPATHS)
_END_LIBRARY_(tr7)
#endif

/*================= (tr7 extra) ====================*/
#if USE_TR7_EXTRA
_BEGIN_LIBRARY_(tr7_extra, "tr7/extra")
   _PROC___(proc_car_cdr, "car+cdr", 1, 1, TR7ARG_PAIR, CAR_CDR)
   _PROC___(proc_length_star, "length*", 1, 1, TR7ARG_ANY, LIST_LENGTH_STAR)
   _PROC___(proc_append_reverse, "append-reverse", 2, 2, TR7ARG_PROPER_LIST TR7ARG_ANY, APPEND_REVERSE)
   _PROC___(proc_append_reverse_in_place, "append-reverse!", 2, 2, TR7ARG_PROPER_LIST TR7ARG_ANY, APPEND_REVERSE_IN_PLACE)
   _PROC___(proc_cons_star, "cons*", 1, INF_ARG, TR7ARG_ANY, CONS_STAR)
   _PROC___(proc_list_copy_star, "list-copy*", 1, 4, TR7ARG_ANY_LIST TR7ARG_NATURAL TR7ARG_NATURAL TR7ARG_ANY, CONS_LIST_COPY_STAR)
   _SYNTAX_(syn_check_types, "check-types", CHKTYPS)
_END_LIBRARY_(tr7_extra)
#endif

/*================= (tr7 environment) ====================*/
#if USE_TR7_ENVIRONMENT
_BEGIN_LIBRARY_(tr7_environment, "tr7/environment")
   _PROC___(proc_is_environment, "environment?", 1, 1, TR7ARG_ANY, ENVP)
   _PROC___(proc_is_defined, "defined?", 1, 2, TR7ARG_SYMBOL TR7ARG_ENVIRONMENT, DEFP)
   _PROC___(proc_symbols_set, "symbols-set", 0, 0, NULL, SYMSET)
   _PROC___(proc_current_environment, "current-environment", 0, 0, NULL, CURR_ENV)
   _PROC___(proc_environment_list, "tr7-environment->list", 0, 2, TR7ARG_ENVIRONMENT TR7ARG_NATURAL, ENVIRONMENT_LIST)
_END_LIBRARY_(tr7_environment)
#endif

/*================= (tr7 extension) ====================*/
#if USE_TR7_EXTENSION
_BEGIN_LIBRARY_(tr7_extension, "tr7/extension")
   _PROC___(proc_load_extension, "load-extension", 1, 2, TR7ARG_STRING, LOADEXT)
_END_LIBRARY_(tr7_extension)
#endif

/*================= (tr7 gc) ====================*/
#if USE_TR7_GC
_BEGIN_LIBRARY_(tr7_gc, "tr7/gc")
   _PROC___(proc_gc, "tr7-gc", 0, 1, NULL, GC)
   _PROC___(proc_gc_verbose, "tr7-gc-verbose", 0, 1, NULL, GCVERB)
   _PROC___(proc_new_segment, "new-segment", 0, 1, TR7ARG_NUMBER, NEWSEGMENT)
_END_LIBRARY_(tr7_gc)
#endif

/*================= (tr7 trace) ====================*/
#if USE_TR7_TRACE
_BEGIN_LIBRARY_(tr7_trace, "tr7/trace")
   _PROC___(proc_tracing, "tr7-tracing", 1, 1, TR7ARG_NATURAL, TRACING)
   _PROC___(proc_show_prompt, "tr7-show-prompt", 0, 1, NULL, SHOW_PROMPT)
   _PROC___(proc_show_eval, "tr7-show-eval", 0, 1, NULL, SHOW_EVAL)
   _PROC___(proc_show_compile, "tr7-show-compile", 0, 1, NULL, SHOW_COMPILE)
   _PROC___(proc_show_result, "tr7-show-result", 0, 1, NULL, SHOW_RESULT)
   _PROC___(proc_keep_playing, "tr7-keep-playing", 0, 1, NULL, KEEP_RUNNING)
_END_LIBRARY_(tr7_trace)
#endif

/*================= (tr7 debug) ====================*/
#if USE_TR7_DEBUG
_BEGIN_LIBRARY_(tr7_debug, "tr7/debug")
   _PROC___(proc_call_stack, "tr7-call-stack", 0, 0, NULL, CALL_STACK)
   _PROC___(proc_exec_stack, "tr7-exec-stack", 0, 0, NULL, EXEC_STACK)
   _PROC___(proc_error_stack, "error-object-stack", 1, 1, TR7ARG_ERROBJ, ERRORSTACK)
   _PROC___(proc_compile, "compile", 1, 1, TR7ARG_ANY, COMPILE)
   _PROC___(proc_disass, "disass", 1, 2, TR7ARG_ANY TR7ARG_TXT_OUTPORT, DISASS)
_END_LIBRARY_(tr7_debug)
#endif

/*================= (tr7 tagged-closures) ====================*/
#if USE_TR7_TAGGED_CLOSURES
_BEGIN_LIBRARY_(tagged_closures, "tr7/tagged-closures")
   _PROC___(proc_is_closure, "closure?", 1, 1, TR7ARG_ANY, CLOSUREP)
   _PROC___(proc_closure_copy, "closure-copy", 1, 1, TR7ARG_PROC, COPYCLOSURE)
   _PROC___(proc_closure_set_tag, "closure-set-tag!", 2, 2, TR7ARG_PROC TR7ARG_ANY, SETCLOSURETAG)
   _PROC___(proc_closure_get_tag, "closure-get-tag", 1, 1, TR7ARG_PROC, GETCLOSURETAG)
_END_LIBRARY_(tagged_closures)
#endif

/*================= tuning ====================*/
#ifdef TR7_EXTRA_CODE
#include TR7_EXTRA_CODE
#endif
/*
**************************************************************************
*/
#undef _INSTR__
#undef ___OPER_
#undef _PROC___
#undef _SYNTAX_
#undef _SYMBOL_
#undef _BEGIN_LIBRARY_
#undef _END_LIBRARY_

#endif/* _WANT_DECLARATIONS_ */
/*
Local variables:
c-file-style: "k&r"
indent: -kr -nut -i3 -l200 -br -nce
End:
vim: noai ts=3 sw=3 expandtab
*/
