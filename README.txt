cpp11ut
=======

CPP11UT is a simple but effective C++ unit test framework for Linux and g++ compiler.

Example output: http://www.x64it.com/files/results.html

hosted at

     https://github.com/aboerner/cpp11ut/wiki
and
     http://redmine.x64it.com/projects/cpp11ut1

Features
=======

    simple but effective C++ framework
    easy to use, no overhead
    no redundancy, all test cases auto register
    nice html output of test results and statistics
    can also generate pdf output of test results
    supports test suites, test cases and test steps
    accepts filter condition on CLI to run only a subset of the tests (or a single test case)
    allows to permanently disable testcases (but will generate a warning for those)
    framework consists of only two files, a header file and a cpp file
    all source code, no library

Limitations
===========

    works for Linux only
    needs C++11 compiler; g++ >= version 4.6 or clang++ v3.3
    need to link against standard Linux library librt.a/so
    pdf generation (optional) relies on external tool: http://code.google.com/p/wkhtmltopdf

Usage
=====

Write your test case file(s) (e.g. Tests.cc); see example below. include "unittest.h" and "unittest.cc" into your project compile/link project with

      g++ -std=gnu++11 -Wall -Weffc++ -o ut MyClass.cc Tests.cc unittest.cc -lrt

      (probably also add -ggdb -O0) run executable (ut)

   $ ./ut -h
     ------ CPP11UT:
     -d            : enable debug output
     -a            : details of all test steps, not just of the failed ones
     -f <filter>   : disable all testcases that do NOT contain <filter>
     -nopdf        : no pdf generation
     -h            : show available parameters
     --help        : show available parameters

view results.html and/or results.pdf

Concepts
========

    test cases can be put all in one file or distributed over many files.
    test cases can be organized into test suites. The name of the test case determines the test suite;
        e.g. "Advanced/DivideByZero/1.1" => creates a testcase with the name "DivideByZero/1.1" in the test suite "Advanced".
    if a test case name ends with "/DISABLE", then the testcase is disabled and will be skipped, but it generates a warning.
    Each test case can contain several test steps.
    Each test step is one of "IS_EQUAL(a,b)" or "ASSERT(a)"

Keywords
=======

SET_TITLE("MyClass") => set the title of your test
TEST_CASE("suite/testcasename", "short description", EXEC{ ... })

within EXEC:

IS_EQUAL( a,b );
        IS_TRUE( b );    or   ASSERT( b);

EXPECT("exception name");   // what exception do you expect to happen


Example
=======

see

MyClass.cc/h,    The class to be tested
Tests.cc         The test cases
unittest.cc/h    the framework

html example output:
     http://www.x64it.com/files/results.html

pdf example output:
    http://www.x64it.com/files/results.pdf


TODO
====

additional test steps might be useful; e.g.
IS_LESS(a,b) => expect a to be less than b
( ASSERT( a < b ) can handle this, but the diagnosis output is suboptimal)

Bugs:
====

1. () around EXEC in some cases

No really a bug, but:
see example /1-Basic/t04 in src/Tests.cc

If EXEC contains a C++ contruct with a "," that is not enclosed by (), then
the Macro expansion of EXEC (the lambda invocation) does not work as expected.
The compiler will generate an error about a wrong number of arguments in the
MACRO invocation.

The workaround is to put an additional pair of () around EXEC, or avoid the comma;
e.g. int a=1; b=2; instead of int a=1,b=2;


