// (C) 2013 by Andreas Boerner
#pragma once

#include <functional> // function
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {
   const char ut_s_path[] = __BASE_FILE__;
};

// helpers
#define COMBINE1(x, y) x ## y
#define COMBINE(x, y) COMBINE1(x, y)
#define QUOT(x) #x
#define QUOTE(x) QUOT(x)
#define VAR(a) COMBINE(tc_, a)
#define VARS VAR(__LINE__)

// the real stuff
#define TEST_CASE(a,b,c) UT::Probe VARS (ut_s_path, __LINE__,a,b,c)
#define EXPECT(a) VARS.expect(a)

#define IS_EQUAL(a,b) try { VARS.equal(a,b, QUOTE(a), QUOTE(b) ); }\
   catch( const exception & e ) { VARS.except(e); }\
   catch(...) { VARS.undef_except(); }

#define IS_TRUE(a) try { VARS.isTrue(a, QUOTE(a)); }\
   catch( const exception & e ) { VARS.except(e); }\
   catch(...) { VARS.undef_except(); }

#define ASSERT(a) IS_TRUE(a)

#define IS_OK(a) VARS.isOK( QUOTE(a)); try { a; }       \
   catch( const exception & e ) { VARS.except(e); }\
   catch(...) { VARS.undef_except(); }

#define SET_TITLE(a) UT::Probe VARS ("setTitle", a)
#define EXEC []()

namespace UT
{
   struct Probe
   {
         Probe(const std::string &,          // file name where probe is located
               const int,                    // line number
               const std::string & tpath,    // p1: tcase path; unique KEY
               const std::string &,          // p2: description
               std::function< void (void)>   // p3: lambda test function
               );

         Probe( const std::string &,         // cmd-string
                const std::string &          // data
                );

         template< typename A, typename B>
         void equal( const A & a,
                     const B & b,
                     const std::string & expect,
                     const std::string & actual);

         void isTrue( const bool expr,
                      const std::string & strExpr);

         void isOK( const std::string & actual);

         void except(const std::exception & e);
         void expect(const std::string & e);
         void undef_except();

         std::string dump();

         // data
         std::string m_fname = "";
         int m_line = -1;
         std::string m_tpath = "";
         std::string m_desc = "";
         std::string m_msg = "";
         std::string m_expect = "";
         std::string m_actual = "";
         std::function<void (void)> m_func = {};

         bool m_is_EX_expected = false;
         bool m_is_EX_happened = false;
         std::string m_EX_expected = "";
         std::string m_EX_happened = "";
         std::string m_what = "";

         //derived data
         std::string m_suite = "";
         std::string m_tname = "";
         bool m_disabled = false;
   };
}

void step_passed(UT::Probe & probe);

void step_failed_eq(UT::Probe & probe,
                    const std::string & aVal,
                    const std::string & bVal,
                    const std::string & str_a,
                    const std::string & str_b);

template< typename A, typename B >
void UT::Probe::equal(const A & a,
                      const B & b,
                      const std::string & str_a,
                      const std::string & str_b)
{
   if(a==b)
   {
      step_passed(*this);
   } else {
      std::ostringstream ossA, ossB;
      ossA << std::setprecision(20) << a;
      ossB << std::setprecision(20) << b;
      step_failed_eq(*this, ossA.str(), ossB.str(), str_a, str_b);
   }
}
