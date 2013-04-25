// (C) 2013 by Andreas Boerner
#include "unittest.h"

#include <algorithm>     // find
#include <cxxabi.h>      // abi
#include <fstream>       // ofstream
#include <iostream>      // cout, cerr
#include <map>
#include <unistd.h>      // pipe, fork, close, dup2, execlp, read

using namespace std;

/* There are 3 phases:

   1. subscription phase, Probe c'tor subscribes to the Manager
   2. Test case execution
   3. html result generation

*/

// framework stuff --------------------------------------------

namespace {

   template <typename T>
   class Singleton
   {
      public:
         static T & getInstance()
         {
            static T singleton;

            if( s_deleted )
            {
               throw std::runtime_error("Call of deleted Singleton detected!");
            }

            return singleton;
         }

         static const T & getConstInstance()
         {
            return getInstance();
         }

      private:
         static bool s_deleted;

         Singleton() = default;
         ~Singleton() { s_deleted = true; };
         explicit Singleton(const Singleton & s) = delete;
         Singleton & operator=(const Singleton &) = delete;
   };

   template <typename T>
   bool Singleton<T>::s_deleted(false);

   timespec diff(timespec start, timespec end)
   {
      timespec temp;
      if ((end.tv_nsec-start.tv_nsec) < 0) {
         temp.tv_sec = end.tv_sec - start.tv_sec - 1;
         temp.tv_nsec = 1000000000 + end.tv_nsec-start.tv_nsec;
      } else {
         temp.tv_sec = end.tv_sec-start.tv_sec;
         temp.tv_nsec = end.tv_nsec-start.tv_nsec;
      }
      return temp;
   }

   string printTime( double t )
   {
      ostringstream oss;

      oss << setprecision(3);

      if( t < 0.001 ) {
         oss << t * 1000000.0 << " &micro;s";
      } else if (t < 1) {
         oss << t * 1000.0 << " ms";
      } else {
         oss << t << " s";
      }

      return oss.str();
   }

   string typeIdName( const std::exception & e )
   {
      // this c-stuff is NOT exception safe (free()); hope it works anyway...
      int status;
      char * realname = abi::__cxa_demangle( typeid(e).name(), 0, 0, &status);
      ostringstream oss;
      oss << realname;
      free(realname);
      return oss.str();
   }

   // END: framework stuff ----------------------------------------

   void step_failed_isTrue(UT::Probe & probe,
                           const string & exprStr);

   template <typename T>
   struct myLambdaMap {
         typedef std::map<std::string, T> type;
   };

   struct Teststep
   {
         bool m_OK = false;

         int m_counter = -1;
         string m_desc = "";
         string m_msg = "";
         string m_EX_expected = "";
         string m_EX_happened = "";
         string m_what = "";

         string m_expect = "";
         string m_actual = "";

         string dump()
         {
            ostringstream oss;
            oss << "\t\t\t\tTeststep.m_OK          : " << boolalpha << m_OK << "\n"
                << "\t\t\t\tTeststep.m_counter     : " << m_counter << "\n"
                << "\t\t\t\tTeststep.m_desc        : " << m_desc << "\n"
                << "\t\t\t\tTeststep.m_msg         : " << m_msg << "\n"
                << "\t\t\t\tTeststep.m_expect      : " << m_expect << "\n"
                << "\t\t\t\tTeststep.m_actual      : " << m_actual << "\n";

            oss << "\t\t\t\tTeststep.m_EX_expected    : " << m_EX_expected << "\n";
            oss << "\t\t\t\tTeststep.m_EX_happened    : " << m_EX_happened << "\n";

            return oss.str();
         };
   };


   struct Testcase
   {
         string m_name = "";
         string m_tpath = "";
         int m_disabled = false;
         int m_ts_OK = 0;
         int m_ts_FAIL = 0;

         string m_expected = "";
         string m_actual = "";

         bool m_is_EX_expected = false;
         bool m_is_EX_happened = false;
         string m_EX_expected = "";
         string m_EX_happened = "";

         int m_ts_counter = 0;
         double m_time = 0.0;
         map<string, Teststep> m_tstep = {};

         string dump()
         {
            ostringstream oss;
            oss << "\t\tTestcase.m_name     : " << m_name << "\n"
                << "\t\tTestcase.m_tpath    : " << m_tpath << "\n"
                << "\t\tTestcase.m_disabled : " <<  m_disabled << "\n"
                << "\t\tTestcase.m_ts_OK    : " << m_ts_OK << "\n"
                << "\t\tTestcase.m_ts_FAIL  : " << m_ts_FAIL << "\n"
                << "\t\tTestcase.m_is_EX_expected : " << boolalpha << m_is_EX_expected << "\n"
                << "\t\tTestcase.m_is_EX_happened : " << boolalpha << m_is_EX_happened << "\n"
                << "\t\tTestcase.m_EX_expected: " << m_EX_expected << "\n"
                << "\t\tTestcase.m_EX_happened: " << m_EX_happened << "\n"
                << "\t\tTestcase.m_time     : " << m_time << "\n"
                << "\t\tTestcase.m_ts_counter  : " << m_ts_counter << "\n"
                << "\t\tTestcase.m_tstep[] =\n";
            int c = 0;
            for( auto & i : m_tstep )
            {
               oss << "\t\t\t[" << c++ << "]: " << i.first << "\n"
                   << i.second.dump();
            }

            return oss.str();
         };
   };

   struct Suite
   {
         string m_name = "";
         int m_tc_OK = 0;
         int m_tc_FAIL = 0;
         int m_tc_DISABLED = 0;
         map<string, Testcase> m_tcs = {};

         string dump()
         {
            ostringstream oss;
            oss << "Suite.m_name       : " << m_name << "\n"
                << "Suite.m_tc_OK      : " << m_tc_OK << "\n"
                << "Suite.m_tc_FAIL    : " << m_tc_FAIL << "\n"
                << "Suite.m_tc_DISABLED: " << m_tc_DISABLED << "\n"
                << "Suite.m_tcs[] =\n";

            int c = 0;
            for( auto & i : m_tcs )
            {
               oss << "\t[" << c++ << "]: " << i.first << "\n"
                   << i.second.dump() << "\n";
            }

            return oss.str();
         }
   };

   bool operator<(const Suite & lhs, const Suite & rhs)
   {
      return lhs.m_name < rhs.m_name;
   }

   bool operator<(const Testcase & lhs, const Testcase & rhs)
   {
      return lhs.m_name < rhs.m_name;
   }

   bool operator<(const Teststep & lhs, const Teststep & rhs)
   {
      return lhs.m_counter < rhs.m_counter;
   }

   class Manager
   {
      public:
         void subscribe( const UT::Probe & sig );
         void setFilter(const std::string & filter);
         void setTitle(const std::string & title);
         void init_phase_1();
         string dump();
         void exec();

         Testcase & findTC( const UT::Probe & probe );

         void setOK( const UT::Probe & sub);
         void setFAIL(const UT::Probe & sub);
         void setExpect(const UT::Probe & sub);
         void printSummary();
         void genHTML();
         void genXML();
         void genPDF();
         void genStatistics();

         bool m_PDF = true;
         bool m_debug = false;
         bool m_ts_all = false;

      private:
         void genGtime(const std::string::size_type pos, std::string & line);
         void genSummary(const std::string::size_type pos, std::string & line);
         void genSummary2(const std::string::size_type pos, std::string & line);
         void genSumSuites(const std::string::size_type pos, std::string & line);
         void genSuites(const std::string::size_type pos, std::string & line);
         void genTestResults(const std::string::size_type pos, std::string & line);

         void addTestStep(const UT::Probe & sub, bool okay);
         void tc_disabled(const UT::Probe & probe);

         string isWarn( int i );
         string isWarn( bool b );
         string isError( int i );
         string isError( bool b );
         string sumLine( const string & lab, // label
                         int a,     // total
                         int b,     // exec'd
                         int c,     // OK
                         float d,   // OK %
                         int e,     // FAIL
                         float f,   // FAIL %
                         int g,     // DISABLED
                         float h    // DISABLED %
                         );
      private:
         myLambdaMap<UT::Probe>::type m_fmap = {};

         std::string m_filter = "";
         std::string m_title = "";

         int m_su_OK = 0;
         int m_su_FAIL = 0;

         map<string, Suite> m_suites = {};
   };

   typedef Singleton<Manager> S_Manager;

   string Manager::dump()
   {
      ostringstream oss;
      oss << "m_filter : " << m_filter << "\n"
          << "m_title  : " << m_title << "\n"
          << "m_su_OK  : " << m_su_OK << "\n"
          << "m_su_FAIL: " << m_su_FAIL << "\n";

      oss << " === m_fmap ======================\n";

      int c = 0;
      for( auto & i : m_fmap )
      {
         oss << "[" << c++ << "] <" << i.first << "> -> Probe\n"
             << i.second.dump() << "\n\n";
      }

      oss << " === m_suites ====================\n";

      int s = 0;
      for( auto & i : m_suites )
      {
         oss << "[" << s++ << "] " << i.first << "\n";
         oss << i.second.dump() << "\n\n";
      }

      return oss.str();
   }

   // ===============================================================================
   // Probe code

   void checkForDisable(const string & hierName, bool & disabled, string & newName)
   {
      const string Disable = "/DISABLE";

      auto pos = hierName.find(Disable);

      if( pos == string::npos )
      {
         disabled = false;
         newName = hierName;
         return;
      }

      if( pos == hierName.size() - Disable.size() )
      {
         disabled = true;
         newName = hierName.substr(0, pos);
      } else {
         cerr << "WARNING: " << Disable << " NOT at end of <" << hierName
              << ">, so NOT disabled!" << endl;
         disabled = false;
         newName = hierName;
      }
   }

   void convertNames(const string & hierName,
                     string & suite,
                     string & name,
                     bool & disabled)
   {
      string newName;
      checkForDisable(hierName, disabled, newName);

      auto pos = newName.find("/");
      if( pos == string::npos )
      {
         suite = "default";
         name = newName;
         return;
      }

      if( pos == 0 )
      {
         pos = newName.find("/",1);

         // if there's no '/' in the name, then assume suite is 'default'
         if( pos == string::npos)
         {
            suite = "default";
            name = newName.substr(1);
            return;
         } else {
            suite = newName.substr(1,pos-1);
            name = newName.substr(pos+1);
            return;
         }
      } else {
         suite = newName.substr(0,pos);
         name = newName.substr(pos+1);
         return;
      }
   }

// ===============================================================================
// Manager code

   void Manager::subscribe( const UT::Probe & probe )
   {
      m_fmap.insert(make_pair(probe.m_tpath, probe));

      auto p = m_suites.find(probe.m_suite);
      if( p == m_suites.end() )
      {
         Suite suite;
         suite.m_name = probe.m_suite;
         // suite.m_tc_DISABLED = probe.m_disabled; // not yet
         p = m_suites.insert( make_pair(probe.m_suite, suite) ).first;
      }

      Testcase tc;
      tc.m_name = probe.m_tname;
      tc.m_disabled = probe.m_disabled;
      tc.m_tpath = probe.m_tpath;

      auto t = p->second.m_tcs.find( probe.m_tname );
      if( t != p->second.m_tcs.end() )
      {
         cerr << "ERROR: two testcases have the same name!\n"
              << "1st: " << p->second.m_name << "/" << t->second.m_name<< " in ";
         auto ftc = m_fmap.find( tc.m_tpath );
         if( ftc == m_fmap.end() )
         {
            throw logic_error("think I have already a duplicate tc, but can't find it!");
         }

         cerr << ftc->second.m_fname << ":" << ftc->second.m_line << "\n"
              << "2nd: " << probe.m_tpath << " in " << probe.m_fname << ":"
              << probe.m_line << endl;
         throw runtime_error("duplicate Testcase names !");
      }

      p->second.m_tcs.insert( make_pair( tc.m_name, tc ) );
   }

   void Manager::setTitle(const string & title)
   {
      if( m_title.size() > 0 )
      {
         cerr << "Title was already set to <" << m_title << ">\n"
              << " ignoring <" << title << ">" << endl;
         return;
      }

      m_title = title;
   }

   void Manager::setFilter(const string & filter)
   {
      m_filter = filter;
   }


   // ===  Manager end  =======================

   void step_failed_isTrue(UT::Probe & probe,
                           const string & exprStr)
   {
      probe.m_msg = "assert violation";

      ostringstream oss;
      oss << "[ " << exprStr << " ]";
      probe.m_expect = oss.str() ;

      probe.m_expect += " => true";
      probe.m_actual = "   => false";

      S_Manager::getInstance().setFAIL(probe);
   }

   void Manager::setOK(const UT::Probe & probe)
   {
      addTestStep(probe, true );
   }

   void Manager::setFAIL(const UT::Probe & probe)
   {
      addTestStep(probe, false );
   }

   Testcase & Manager::findTC( const UT::Probe & probe )
   {
      auto p = m_suites.find(probe.m_suite);
      if( p == m_suites.end() )
      {
         throw runtime_error("Suite not found!");
      }

      auto q = p->second.m_tcs.find(probe.m_tname);
      if( q == p->second.m_tcs.end() )
      {
         throw runtime_error("Testcase not found!");
      }

      return q->second;
   }

   void Manager::addTestStep( const UT::Probe & probe, bool okay)
   {
      Teststep ts;
      ts.m_OK = okay;
      ts.m_desc = probe.m_desc;
      ts.m_expect = probe.m_expect;
      ts.m_actual = probe.m_actual;
      ts.m_EX_expected = probe.m_EX_expected;
      ts.m_EX_happened = probe.m_EX_happened;
      ts.m_what = probe.m_what;
      ts.m_msg = probe.m_msg;

      if( probe.m_EX_expected != probe.m_EX_happened )
      {
         ts.m_OK = false;
         if( probe.m_EX_expected.size() == 0 )
         {
            ts.m_msg = "unexpected exception occured!";
         } else {
            ts.m_msg = "exceptions do NOT match";
         }

         if( probe.m_EX_happened == "(UNKNOWN)" )
         {
            ts.m_msg += " (UNKNOWN) means it was not derived from std::exception.";
         }
      }

      auto & p = findTC( probe );

      ts.m_counter = ++p.m_ts_counter;

      ostringstream oss;
      oss << "TS-" << setfill('0') << setw(3) << ts.m_counter;
      string tsName = oss.str();

      p.m_tstep.insert(make_pair( tsName, ts));

      if( ts.m_OK )
      {
         cout << "passed: ";
      } else {
         cout << "FAILED: ";
      }

      cout << "<" << p.m_name << " - " << tsName << endl;
   }

   void Manager::setExpect(const UT::Probe & probe)
   {
      auto & p = findTC( probe );

      p.m_is_EX_expected = true;
      p.m_EX_expected = probe.m_EX_expected;
   }


   void Manager::genStatistics()
   {
      int su_OK = 0;
      int su_FAIL = 0;

      for( auto & su : m_suites )
      {
         int tc_OK = 0;
         int tc_FAIL = 0;
         int tc_DISABLED = 0;

         for( auto & tc : su.second.m_tcs )
         {
            int ts_OK = 0;
            int ts_FAIL = 0;
            for( auto & ts : tc.second.m_tstep )
            {
               if( ts.second.m_OK )
               {
                  ts_OK++;
               } else {
                  ts_FAIL++;
               }
            }

            tc.second.m_ts_OK = ts_OK;
            tc.second.m_ts_FAIL = ts_FAIL;

            if( tc.second.m_ts_OK > 0 && tc.second.m_disabled )
            {
               throw logic_error("Testcase skipped, but teststeps exec'd OK ?!");
            }

            if( tc.second.m_disabled )
            {
               tc_DISABLED++;
            } else if (tc.second.m_ts_FAIL == 0)
            {
               tc_OK++;
            } else {
               tc_FAIL++;
            }
         }

         su.second.m_tc_OK = tc_OK;
         su.second.m_tc_FAIL = tc_FAIL;
         su.second.m_tc_DISABLED = tc_DISABLED;

         if( tc_FAIL > 0 )
         {
            su_FAIL++;
         } else {
            su_OK++;
         }
      }

      m_su_OK = su_OK;
      m_su_FAIL = su_FAIL;
   }


   void Manager::printSummary()
   {
      cout << "\nSUMMARY: ==========================\n";
      cout << "  test suites:\n";

      if( m_suites.size() < 1 )
      {
         cout << "BAD: NO suites registered..." << endl;
         return;
      }

      cout << "    exec'd  : \t" << m_suites.size() << endl;
      cout << "     OK     : \t" << m_su_OK << "\t" << m_su_OK * 100 / m_suites.size() << "%" << endl;
      cout << "     FAILED : \t" << m_su_FAIL << "\t" << m_su_FAIL * 100 / m_suites.size() << "%" << endl;
      cout << "\n  test cases:\n";

      int tc_OK = 0;
      int tc_FAIL = 0;
      int tc_DISABLE = 0;

      for( auto & i : m_suites )
      {
         tc_OK += i.second.m_tc_OK;
         tc_FAIL += i.second.m_tc_FAIL;
         tc_DISABLE += i.second.m_tc_DISABLED;
      }

      int tc_exec = tc_OK + tc_FAIL;

      if( tc_exec < 1 )
      {
         cout << "BAD: ALL testcases were DISABLED..." << endl;
         return;
      }

      if( m_fmap.size() < 1 )
      {
         cout << "BAD: NO testcases were registed..." << endl;
         return;
      }

      cout << "    exec'd  : \t" << tc_exec << "\t" << tc_exec * 100 / (tc_exec + tc_DISABLE) << "%" << endl;
      cout << "    disabled: \t" << tc_DISABLE << "\t" << tc_DISABLE * 100 / m_fmap.size() << "%" << endl;
      cout << "     OK     : \t" << tc_OK << "\t" << tc_OK * 100 / tc_exec << "%" << endl;
      cout << "     FAILED : \t" << tc_FAIL << "\t" << tc_FAIL * 100 / tc_exec << "%" << endl;
      cout << "\n  test steps:\n";

      int ts_OK = 0;
      int ts_FAIL = 0;

      for( auto & i : m_suites )
      {
         for( auto & tc : i.second.m_tcs )
         {
            if( !tc.second.m_disabled )
            {
               ts_OK += tc.second.m_ts_OK;
               ts_FAIL += tc.second.m_ts_FAIL;
            }
         }
      }

      int ts_exec = ts_OK + ts_FAIL;

      if( ts_exec < 1 )
      {
         cout << "BAD: NO teststeps were exec'd..." << endl;
         return;
      }

      cout << "    exec'd  : \t" << ts_exec << endl;
      cout << "     OK     : \t" << ts_OK << "\t" <<  ts_OK * 100 / ts_exec  << "%" << endl;
      cout << "     FAILED : \t" << ts_FAIL << "\t" <<  ts_FAIL * 100 / ts_exec << "%" << endl;
      if( tc_FAIL > 0 )
      {
         cout << "=====================================\n";
         cout << "======  B A D  ======================\n";
         cout << "=====================================";
      } else {
         cout << "======  O K  ========================";
      }
      cout << endl;
   }

   const char * const s_html[] = {


      "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Strict//EN\" \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd\">",
      "<html xmlns=\"http://www.w3.org/1999/xhtml\" xml:lang=\"en\" lang=\"en\">",
      "<head>",
      "  <meta http-equiv=\"content-type\" content=\"text/html; charset=utf-8\" />",
      "  <meta name=\"generator\" content=\"CPP11UT - http://redmine.x64it.com/projects/cpp11ut1\" />",
      "  <title>$TITLE$ - UT Results</title>",
      "",
      "  <style type=\"text/css\" media=\"screen\">",
      "    <!--",
      "    hr  {",
      "      width: 99%;",
      "      border-width: 0px;",
      "      height: 3px;",
      "      color: #ccbbcc;",
      "      background-color: #cc33cc;",
      "      padding: 0px;",
      "    }",
      "",
      "    table {",
      "      width:100%;",
      "      border-collapse:separate;",
      "      border-spacing: 2px;",
      "      border:0px;",
      "    }",
      "    tr {",
      "      margin:0px;",
      "      padding:0px;",
      "    }",
      "    td {",
      "      margin:0px;",
      "      padding:1px;",
      "    }",
      "    .table_summary {",
      "      background-color: #b0b0b0;",
      "    }",
      "    .table_suites {",
      "      background-color: #b0b0b0;",
      "    }",
      "    .table_suite {",
      "      background-color: #b0b0b0;",
      "    }",
      "    .table_result {",
      "      background-color: #b0b0b0;",
      "      margin: 0px 0px 1em 0px;",
      "    }",
      "    .tablecell_title {",
      "      background-color: #a5cef7;",
      "      font-weight: bold;",
      "      text-align: center;",
      "    }",
      "",
      "    .tablecell_success {",
      "      background-color: #efefe7;",
      "      text-align: center;",
      "    }",
      "",
      "    .tablecell_left {",
      "      background-color: #efefe7;",
      "      text-align: left;",
      "    }",
      "",
      "    .tablecell_warn {",
      "      background-color: #ffff80;",
      "      text-align: center;",
      "    }",
      "",
      "    .tablecell_error {",
      "      color: #ff0808;",
      "      background-color: #efbfe7;",
      "      font-weight: bold;",
      "      text-align: center;",
      "    }",
      "    p.spaced {",
      "      margin: 0px;",
      "      padding: 3px 0px 3px 0px;",
      "    }",
      "    p.unspaced {",
      "      margin: 0px;",
      "      padding: 0px 0px 2em 0px;",
      "    }",
      "    -->",
      "  </style>",
      "</head>",
      "",
      "<body>",
      "",
      "<table>",
      " <tr>",
      "  <td>",
      "   <h1><a name=\"top\"></a>$TITLE$ - Unit Tests Results</h1>",
      "  </td>",
      "  <td>",
      "   <div style=\"text-align:right\">",
      "    <a href=\"http://redmine.x64it.com/projects/cpp11ut1\">CPP11UT</a> v0.1 &copy; 2013",
      "   </div>",
      "   <div style=\"text-align:right\">",
      "    by Andreas Boerner",
      "   </div>",
      "  </td>",
      " </tr>",
      "</table>",
      "$GTIME$",
      "<hr />",
      "",
      "<h2>Summary</h2>",

      "$SUMMARY$",

      "<table style=\"table-layout: fixed;\" summary=\"Summary of test results\" class=\"table_summary\">",
      " <col width=\"120px\" />",
      "  <tr>",
      "    <td style=\"width: 400px;\" class=\"tablecell_title\"></td>",
      "    <td class=\"tablecell_title\">Total</td>",
      "    <td class=\"tablecell_title\">Exec'd</td>",
      "    <td colspan = \"2\" style=\"text-align:center;\" class=\"tablecell_title\">OK</td>",
      "    <td colspan = \"2\" style=\"text-align:center;\" class=\"tablecell_title\">DISABLED</td>",
      "    <td colspan = \"2\" style=\"text-align:center;\" class=\"tablecell_title\">FAIL</td>",
      "  </tr>",
      "  <tr>",
      "    <td style=\"\" class=\"tablecell_title\"></td>",
      "    <td style=\"\" class=\"tablecell_title\"></td>",
      "    <td style=\"\" class=\"tablecell_title\"></td>",
      "    <td style=\"text-align:center;\" class=\"tablecell_title\">#</td>",
      "    <td style=\"text-align:center;\" class=\"tablecell_title\">%</td>",
      "    <td style=\"text-align:center;\" class=\"tablecell_title\">#</td>",
      "    <td style=\"text-align:center;\" class=\"tablecell_title\">%</td>",
      "    <td style=\"text-align:center;\" class=\"tablecell_title\">#</td>",
      "    <td style=\"text-align:center;\" class=\"tablecell_title\">%</td>",
      "  </tr>",
      "$SUMMARY-2$",
      "<hr />",
      "",
      "<h2>Test Suites</h2>",
      "<h3>Summary</h3>",
      "<table style=\"table-layout: fixed;\" summary=\"Summary of test suites\" class=\"table_summary\">",
      " <col width=\"200px\" />",
      "  <tr>",
      "    <td style=\"text-align: left;\" class=\"tablecell_title\">Suite</td>",
      "    <td class=\"tablecell_title\">Total</td>",
      "    <td class=\"tablecell_title\">Exec'd</td>",
      "    <td colspan = \"2\" style=\"text-align:center;\" class=\"tablecell_title\">OK</td>",
      "    <td colspan = \"2\" style=\"text-align:center;\" class=\"tablecell_title\">DISABLED</td>",
      "    <td colspan = \"2\" style=\"text-align:center;\" class=\"tablecell_title\">FAIL</td>",
      "    <td class=\"tablecell_title\">Time</td>",
      "  </tr>",
      "  <tr>",
      "    <td style=\"\" class=\"tablecell_title\"></td>",
      "    <td style=\"\" class=\"tablecell_title\">testcases</td>",
      "    <td style=\"\" class=\"tablecell_title\">testcases</td>",
      "    <td style=\"text-align:center;\" class=\"tablecell_title\">#</td>",
      "    <td style=\"text-align:center;\" class=\"tablecell_title\">%</td>",
      "    <td style=\"text-align:center;\" class=\"tablecell_title\">#</td>",
      "    <td style=\"text-align:center;\" class=\"tablecell_title\">%</td>",
      "    <td style=\"text-align:center;\" class=\"tablecell_title\">#</td>",
      "    <td style=\"text-align:center;\" class=\"tablecell_title\">%</td>",
      "    <td style=\"text-align:center;\" class=\"tablecell_title\">s</td>",
      "  </tr>",
      "$SUM-SUITES$",
      "</table>",
      "$SUITES$",
      "",
      "$TESTRESULTS$",
      "<hr />",
      "</body>",
      "</html>"
   };

   void Manager::genSuites(const std::string::size_type pos, std::string & line)
   {
      ostringstream oss;
      bool disabled = false;

      for( auto & s : m_suites )
      {
         oss << "<h3><a name=\"" << s.second.m_name << "\"></a>Suite: " <<  s.second.m_name << "</h3>\n";
         oss << "<table style=\"table-layout: fixed;\" summary=\"Details for suite " <<  s.second.m_name << "\" class=\"table_suite\">\n";
         oss << " <col width=\"200px\" />\n";
         oss << "  <tr>\n";
         oss << "    <td style=\"text-align: left;\" class=\"tablecell_title\">Test Case Name</td>\n";
         oss << "    <td style=\"width:40%\" class=\"tablecell_title\">description</td>\n";
         oss << "    <td class=\"tablecell_title\">exec'd</td>\n";
         oss << "    <td colspan=\"2\" class=\"tablecell_title\">OK</td>\n";
         oss << "    <td colspan=\"2\" class=\"tablecell_title\">FAIL</td>\n";
         oss << "    <td class=\"tablecell_title\">Time</td>\n";
         oss << "  </tr>\n";
         oss << "  <tr>\n";
         oss << "    <td style=\"text-align: left;\" class=\"tablecell_title\"> </td>\n";
         oss << "    <td style=\"width:40%\" class=\"tablecell_title\"> </td>\n";
         oss << "    <td class=\"tablecell_title\">test steps</td>\n";
         oss << "    <td class=\"tablecell_title\">#</td>\n";
         oss << "    <td class=\"tablecell_title\">%</td>\n";
         oss << "    <td class=\"tablecell_title\">#</td>\n";
         oss << "    <td class=\"tablecell_title\">%</td>\n";
         oss << "    <td class=\"tablecell_title\">s</td>\n";
         oss << "  </tr>\n";

         disabled = false;
         for( auto & tc : s.second.m_tcs )
         {
            auto p = m_fmap.find(tc.second.m_tpath);
            if( p == m_fmap.end() )
            {
               throw runtime_error("can't find tpath in m_fmap !");
            }

            if( tc.second.m_disabled )
            {
               oss << "  <tr>\n";
               oss << "    <td style=\"text-align:left;\" class=\"tablecell_warn\">" << tc.second.m_name << "</td>\n";
               oss << "    <td style=\"text-align:left;\" class=\"tablecell_warn\">" << p->second.m_desc << "</td>\n";
               oss << "  </tr>\n";
               disabled = true;
               continue;
            }

            oss << "  <tr>\n";
            oss << "    <td style=\"text-align:left;\" class=\"tablecell_"
                << isError(tc.second.m_ts_FAIL) << "\">";

            if( tc.second.m_ts_FAIL > 0 )
            {
               oss << "<a href=\"#" << s.second.m_name << "_" << tc.second.m_name << "\">";
            }

            oss << tc.second.m_name;

            if( tc.second.m_ts_FAIL > 0 )
            {
               oss << "</a>";
            }

            oss << "</td>\n";

            oss << "    <td style=\"text-align:left;\" class=\"tablecell_" << isError(tc.second.m_ts_FAIL)
                << "\">" << p->second.m_desc << "</td>\n";

            oss << "    <td class=\"tablecell_" << isError(tc.second.m_ts_FAIL) << "\">"
                << tc.second.m_ts_OK + tc.second.m_ts_FAIL << "</td>\n";

            oss << "    <td class=\"tablecell_" << isError(tc.second.m_ts_FAIL) << "\">" << tc.second.m_ts_OK << "</td>\n";

            oss << "    <td class=\"tablecell_" << isError(tc.second.m_ts_FAIL) << "\">"
                << setprecision(3) << tc.second.m_ts_OK * 100.0 / (tc.second.m_ts_OK + tc.second.m_ts_FAIL) << "</td>\n";

            oss << "    <td class=\"tablecell_" << isError(tc.second.m_ts_FAIL) << "\">" << tc.second.m_ts_FAIL << "</td>\n";

            oss << "    <td class=\"tablecell_" << isError(tc.second.m_ts_FAIL) << "\">"
                << setprecision(3) << tc.second.m_ts_FAIL * 100.0 / (tc.second.m_ts_OK + tc.second.m_ts_FAIL) << "</td>\n";

            oss << "    <td class=\"tablecell_" << isError(tc.second.m_ts_FAIL) << "\">"
                << printTime( tc.second.m_time ) << "</td>\n" << "  </tr>\n";
         }
         oss << "</table>\n";

         if( disabled )
         {
            oss << "  <br />\n<table>\n"
                << "  <tr>\n  <td style=\"text-align:left;\" class=\"tablecell_warn\"> test cases marked yellow were DISABLED !</td>\n</tr>\n</table>\n";
         }
         oss << "<p class=\"spaced\"><a href=\"#top\">Back to top</a></p>\n";
      }

      line.replace(pos, 8, oss.str());
   }

   void Manager::genSummary(const string::size_type pos, string & line)
   {
      string insHtml;
      insHtml.reserve(2000);

      int tc_OK = 0;
      int tc_FAIL = 0;
      int tc_DISABLE = 0;

      for( auto & i : m_suites )
      {
         tc_OK += i.second.m_tc_OK;
         tc_FAIL += i.second.m_tc_FAIL;
         tc_DISABLE += i.second.m_tc_DISABLED;
      }

      // handle OK/FAIL
      if( tc_FAIL > 0 )
      {
         ostringstream oss;
         double percent = tc_FAIL * 100 / m_fmap.size();
         oss << "<h3 style=\"color: red; background-color: yellow;\">FAILED testcases: "
             << tc_FAIL << " (" << setprecision(3) << percent << "%)</h3>";

         insHtml = oss.str();

      } else {
         ostringstream oss;

         oss << "<h3 style=\"color: green;\">OK testcases: " << tc_OK << " (100%)</h3>";
         insHtml = oss.str();
      }

      // handle disabled testcases
      if( tc_DISABLE > 0 )
      {
         ostringstream oss;
         double percent = tc_DISABLE * 100 / m_fmap.size();
         oss << "<h3 style=\"color: black; background-color: yellow;\">DISABLED testcases: "
             << tc_DISABLE << " (" << setprecision(3) << percent << "%)</h3>";

         insHtml += oss.str();

      } else {
         insHtml += "<h3 style=\"color: green;\">EXEC'd: 100%</h3>";
      }

      line.replace(pos,9, insHtml);
   }

   string Manager::isWarn( int i )
   {
      if( i > 0 ) return "warn";
      return "success";
   }

   string Manager::isWarn( bool b )
   {
      if( b ) return "warn";
      return "success";
   }

   string Manager::isError( int i )
   {
      if( i > 0 ) return "error";
      return "success";
   }

   string Manager::isError( bool b )
   {
      if( b ) return "error";
      return "success";
   }

   string Manager::sumLine( const string & lab, // label
                            int a,     // total
                            int b,     // exec'd
                            int c,     // OK
                            float d,   // OK %
                            int e,     // FAIL
                            float f,   // FAIL %
                            int g,     // DISABLED
                            float h    // DISABLED %
                            )
   {
      ostringstream oss;

      // first cell (label)
      oss << "  <tr>\n" << "    <td style=\"text-align: left;\" class=\"tablecell_";
      if( e > 0 )
      {
         oss << "error";
      }  else if (g > 0)
      {
         oss << "warn";
      } else {
         oss << "success";
      }

      oss << "\">" << lab << "</td>\n";

      // 2. cell (total)
      if( a < 0 )
      {
         oss << "    <td class=\"tablecell_success";
         oss << "\"> " << "</td>\n";
      } else {
         oss << "    <td class=\"tablecell_success";
         oss << "\">" << a << "</td>\n";
      }

      // 3. cell (exec'd)
      oss << "    <td class=\"tablecell_" << isWarn( b<a )
          << "\">" << b << "</td>\n";

      // 4. cell # OK
      oss << "    <td class=\"tablecell_success"
          << "\">" << c << "</td>\n";

      // 5. cell % OK
      oss << "    <td class=\"tablecell_success"
          << "\">" << setprecision(3) << d << "</td>\n";

      // 6. cell # DISABLED
      if( g < 0 )
      {
         oss << "    <td class=\"tablecell_success"
             << "\"> " << "</td>\n";
      } else {
         oss << "    <td class=\"tablecell_" << isWarn(g)
             << "\">" << g << "</td>\n";
      }

      // 7. cell % DISABLED
      if( g < 0 )
      {
         oss << "    <td class=\"tablecell_success"
             << "\"> " << "</td>\n";
      } else {
         oss << "    <td class=\"tablecell_" << isWarn(g)
             << "\">" << setprecision(3) << h << "</td>\n";
      }

      // 8. cell # FAIL
      oss << "    <td class=\"tablecell_" << isError(e)
          << "\">" << e << "</td>\n";

      // 9. cell % FAIL
      oss << "    <td class=\"tablecell_" << isError(e)
          << "\">" << setprecision(3) << f << "</td>\n";

      oss << "  </tr>\n";

      return oss.str();
   }

   void Manager::genSummary2(const string::size_type pos, string & line)
   {
      string insHtml;
      insHtml.reserve(2000);

      ostringstream oss;

      insHtml += sumLine("Test Suites",       // label in first column
                         m_suites.size(),     // total number of test suites
                         m_su_OK + m_su_FAIL, // test suites exec'd
                         m_su_OK,             // test suites OK
                         m_su_OK * 100.0 / (m_su_OK + m_su_FAIL), // percent
                         m_su_FAIL,           // test suites failed
                         m_su_FAIL * 100.0 / (m_su_OK + m_su_FAIL),
                         -1,
                         0.0 );

      {
         int tc_OK = 0;
         int tc_FAIL = 0;
         int tc_DISABLE = 0;

         for( auto & i : m_suites )
         {
            tc_OK += i.second.m_tc_OK;
            tc_FAIL += i.second.m_tc_FAIL;
            tc_DISABLE += i.second.m_tc_DISABLED;
         }

         insHtml += sumLine("Test Cases",     // label in first column
                            m_fmap.size(),    // total number of test cases
                            tc_OK + tc_FAIL,  // test cases exec'd
                            tc_OK ,           // test cases OK
                            tc_OK * 100.0 / (tc_OK + tc_FAIL), // percent
                            tc_FAIL,          // test cases failed
                            tc_FAIL * 100.0 / (tc_OK + tc_FAIL),
                            tc_DISABLE,
                            tc_DISABLE * 100.0 / m_fmap.size()
                            );
      }

      {
         int ts_OK = 0;
         int ts_FAIL = 0;

         for( auto & s : m_suites )
         {
            for( auto & tc : s.second.m_tcs )
            {
               ts_OK += tc.second.m_ts_OK;
               ts_FAIL += tc.second.m_ts_FAIL;
            }
         }

         insHtml += sumLine("Test Steps",  // label in first column
                            -1,                 // total number of test cases
                            ts_OK + ts_FAIL,    // test cases exec'd
                            ts_OK ,             // test cases OK
                            ts_OK * 100.0 / ( ts_OK + ts_FAIL), // percent
                            ts_FAIL, // test suites failed
                            ts_FAIL * 100.0 / (ts_OK + ts_FAIL),
                            -1,
                            0.0 );
      }

      {
         double t = 0;
         for( auto & s : m_suites )
         {
            for( auto & tc : s.second.m_tcs )
            {
               t += tc.second.m_time;
            }
         }

         ostringstream oss;
         oss << "</table>\n" << "<p>Total time elapsed: "
             << printTime(t) << "</p>\n";

         insHtml += oss.str();
      }

      line.replace(pos,11, insHtml);
   }


   void Manager::genSumSuites(const string::size_type pos, string & line)
   {
      string name;

      ostringstream oss;

      for( auto & i : m_suites )
      {
         oss << "  <tr>\n";

         oss << "    <td style=\"text-align:left;\" class=\"tablecell_" << isError(i.second.m_tc_FAIL) << "\"><a href=\"#"
             << i.second.m_name << "\">" << i.second.m_name << "</a></td>\n";

         oss << "    <td class=\"tablecell_success\">"
             <<  i.second.m_tc_OK + i.second.m_tc_FAIL + i.second.m_tc_DISABLED << "</td>\n";

         oss << "    <td class=\"tablecell_" << isWarn( i.second.m_tc_DISABLED ) << "\">"
             <<  i.second.m_tc_OK + i.second.m_tc_FAIL << "</td>\n";

         oss << "    <td class=\"tablecell_success\">"
             <<  i.second.m_tc_OK << "</td>\n";

         oss << "    <td class=\"tablecell_success\">"
             <<  setprecision(3);
         if( i.second.m_tc_OK + i.second.m_tc_FAIL < 1 )
         {
            oss << 0.0;
         } else {
            oss << i.second.m_tc_OK * 100.0 / ( i.second.m_tc_OK + i.second.m_tc_FAIL );
         }
         oss << "</td>\n";

         oss << "    <td class=\"tablecell_" << isWarn( i.second.m_tc_DISABLED ) << "\">"
             <<  i.second.m_tc_DISABLED << "</td>\n";

         oss << "    <td class=\"tablecell_" << isWarn( i.second.m_tc_DISABLED ) << "\">"
             <<  setprecision(3) << i.second.m_tc_DISABLED * 100.0 / ( i.second.m_tc_OK + i.second.m_tc_FAIL + i.second.m_tc_DISABLED ) << "</td>\n";

         oss << "    <td class=\"tablecell_" << isError( i.second.m_tc_FAIL ) << "\">"
             <<  i.second.m_tc_FAIL << "</td>\n";

         oss << "    <td class=\"tablecell_" << isError( i.second.m_tc_FAIL ) << "\">"
             <<  setprecision(3);

         if(  i.second.m_tc_OK + i.second.m_tc_FAIL < 1 )
         {
            oss << 0.0;
         } else {
            oss << i.second.m_tc_FAIL * 100.0 / ( i.second.m_tc_OK + i.second.m_tc_FAIL );
         }
         oss << "</td>\n";

         double t=0;
         for( auto & tc : i.second.m_tcs )
         {
            t += tc.second.m_time;
         }

         oss << "    <td class=\"tablecell_success\">"
             << printTime(t) << "</td>\n" << "  </tr>\n";
      }

      line.replace(pos, 12, oss.str());
   }

   void Manager::genGtime(const string::size_type pos, string & line)
   {
      ostringstream oss;

      time_t rawtime;
      struct tm * timeinfo;
      char buffer[200];

      time(&rawtime);
      timeinfo = localtime(&rawtime);
      strftime( buffer, 80, "%Y-%m-%d  %H:%M:%S  %Z", timeinfo);

      oss << "<div style=\"border: 0px;font-size: 13px; font-family: monospace; text-align: center;\">generation time: "
          << buffer << "</div>\n";

      getcwd(buffer,200);

      oss << "<div style=\"border: 0px;font-size: 13px; font-family: monospace; text-align: center;\">"
          << buffer << "</div>\n";

      line.replace(pos, 7, oss.str());
   }

   void Manager::genTestResults(const string::size_type pos, string & line)
   {
      string insHtml;
      insHtml.reserve(10000);
      bool failFound = false;

      string name;
      ostringstream oss;

      for( auto & s : m_suites )
      {
         for( auto & tc : s.second.m_tcs )
         {
            if( tc.second.m_ts_FAIL > 0 or m_ts_all )
            {
               if( !failFound)
               {
                  oss << "<h2>Test result details - Failed test steps</h2>\n";
                  failFound = true;
               }

               auto p = m_fmap.find(tc.second.m_tpath);

               for( auto & ts : tc.second.m_tstep )
               {
                  if( !ts.second.m_OK or m_ts_all )
                  {
                     oss << "<h3><a name=\"" << s.second.m_name << "_" << tc.second.m_name << "\">"
                         << s.second.m_name << "::" << tc.second.m_name << "</a></h3>\n";

                     oss << "<table style=\"table-layout: fixed;\" summary=\"Test Failure\" class=\"table_result\">\n"
                         << " <col width=\"100px\" />\n"
                         << "<tr>\n" << "<td style=\"text-align:left;\" class=\"tablecell_title\">Testcase</td>\n"
                         << "<td class=\"tablecell_left\">" <<  s.second.m_name << "::" << tc.second.m_name
                         << "</td>\n</tr>\n";

                     oss << "<tr>\n<td style=\"text-align:left;\" class=\"tablecell_title\">Descript.</td>"
                         << "    <td class=\"tablecell_left\">";

                     //oss << p->second.m_desc <<  "</td>\n  </tr>";
                     oss <<ts.second.m_desc <<  "</td>\n  </tr>";


                     oss << "<tr>\n<td style=\"text-align:left;\" class=\"tablecell_title\">File</td>"
                         << "    <td class=\"tablecell_left\">";
                     oss << p->second.m_fname << ":" << p->second.m_line << "</td>\n  </tr>\n";


                     oss << "<tr>\n<td style=\"text-align:left;\" class=\"tablecell_title\">Test Step</td>"
                         << "    <td class=\"tablecell_left\">" << ts.first << "</td></tr>\n";

                     oss << "<tr>\n<td style=\"text-align:left;\" class=\"tablecell_title\">Message</td>"
                         << "    <td class=\"tablecell_left\">" << ts.second.m_msg <<  "</td>\n  </tr>";

                     oss << "<tr>\n<td style=\"text-align:left;\" class=\"tablecell_title\">Expected</td>"
                         << "    <td class=\"tablecell_left\">";

                     if( ts.second.m_EX_expected.size() > 0 )
                     {
                        oss << ts.second.m_EX_expected;
                     } else {
                        oss << ts.second.m_expect;
                     }
                     oss <<  "</td>\n  </tr>";


                     oss << "<tr>\n<td style=\"text-align:left;\" class=\"tablecell_title\">Actual</td>"
                         << "    <td class=\"tablecell_left\">";

                     if( ts.second.m_EX_happened.size() > 0 )
                     {
                        oss << ts.second.m_EX_happened;
                     } else {
                        oss << ts.second.m_actual;
                     }
                     oss <<  "</td>\n  </tr>\n";

                     // what
                     if( ts.second.m_what.size() > 0 )
                     {
                        oss << "<tr>\n<td style=\"text-align:left;\" class=\"tablecell_title\">What</td>"
                            << "    <td class=\"tablecell_left\">";

                        oss << ts.second.m_what;
                        oss <<  "</td>\n  </tr>\n";
                     }

                     oss << "</table>\n";
                     oss << "<p class=\"spaced\"><a href=\"#top\">Back to top</a></p>\n";
                     oss << "<hr />\n";
                  }
               }
               insHtml += oss.str();
               oss.str("");
               oss.clear();
            }
         }
      }

      line.replace(pos, 13, insHtml);
   }

   void Manager::genHTML()
   {
      ofstream of("results.html");
      string::size_type pos;

      for( auto & i : s_html )
      {
         string line(i);
         pos = 10;

         while( pos != string::npos)
         {
            pos = line.find("$GTIME$");
            if( pos != string::npos )
            {
               genGtime(pos, line);
               continue;
            }

            pos = line.find("$TITLE$");
            if( pos != string::npos )
            {
               line.replace(pos, 7, m_title);
               continue;
            }

            pos = line.find("$SUMMARY$");
            if( pos != string::npos )
            {
               genSummary(pos, line);
               continue;
            }

            pos = line.find("$SUMMARY-2$");
            if( pos != string::npos )
            {
               genSummary2(pos, line);
               continue;
            }

            pos = line.find("$SUM-SUITES$");
            if( pos != string::npos )
            {
               genSumSuites(pos, line);
               continue;
            }

            pos = line.find("$SUITES$");
            if( pos != string::npos )
            {
               genSuites(pos, line);
               continue;
            }

            pos = line.find("$TESTRESULTS$");
            if( pos != string::npos )
            {
               genTestResults(pos, line);
               continue;
            }

         };

         of << line << "\n";
      }
      cout << "<results.html> generated." << endl;
   }

   void Manager::tc_disabled(const UT::Probe & probe)
   {
      cout << "INFO: Test " << probe.m_tpath
           << " skipped because of filter condition <"
           << m_filter << ">" << endl;

      auto p = m_suites.find(probe.m_suite);
      if( p == m_suites.end() )
      {
         throw runtime_error("Suite not found !");
      }

      Suite & suite = p->second;

      auto p_tc = suite.m_tcs.find(probe.m_tname);
      if( p_tc == suite.m_tcs.end() )
      {
         throw runtime_error("Testcase not found !");
      }

      p_tc->second.m_disabled = true;
   }

   void Manager::exec()
   {
      string suite;
      string what;

      timespec t1,t2, tdiff = {0};

      for( auto & i : m_fmap )
      {
         if( i.first.find(m_filter) == string::npos )
         {
            tc_disabled(i.second);
            continue;
         }

         auto & tc = findTC(i.second);

         if( !i.second.m_disabled )
         {
            try
            {
               tc.m_is_EX_happened = false;
               clock_gettime(CLOCK_MONOTONIC, &t1);
               i.second.m_func();
               clock_gettime(CLOCK_MONOTONIC, &t2);
            }
            catch( const exception & e )
            {
               clock_gettime(CLOCK_MONOTONIC, &t2);
               tc.m_is_EX_happened = true;
               tc.m_EX_happened = typeIdName(e);
               what = e.what();
            }
            catch(...)
            {
               clock_gettime(CLOCK_MONOTONIC, &t2);
               tc.m_is_EX_happened = true;
               tc.m_EX_happened = "(UNKNOWN)>";
            }

            tdiff = diff(t1, t2);
            tc.m_time = tdiff.tv_sec + tdiff.tv_nsec / 1000000000.0;

            if( tc.m_is_EX_happened )
            {
               Teststep ts;
               ts.m_OK = false;
               ts.m_desc = "<kbd><b>internally generated Teststep</b> for Testcase exception failure</kbd>";
               ts.m_EX_expected = "NO exception";
               ts.m_EX_happened = tc.m_EX_happened;
               ts.m_what = what;
               what = "";
               tc.m_ts_FAIL++;

               tc.m_tstep.insert(make_pair( "TC-intern", ts));
            }
         }
      }
   }

   void Manager::genXML()
   {
      cerr << "genXML() not implemented." << endl;
   }

   void Manager::genPDF()
   {
      if( !m_PDF )
      {
         return;
      }

      string path;

      int fd[2];
      if(pipe(fd) < 0)           /*create pipe */
      {
         throw runtime_error("no pipe");
      }

      switch( fork() )
      {
         case -1:
            throw runtime_error("can not fork.");
         case 0: // child
            close(fd[0]);
            dup2(fd[1], fileno(stdout));
            execlp( "which", "which", "wkhtml2pdf", (char *)0 );
            close(fd[0]);
            exit(0);
      }

      close( fd[1] );
      char line[200];
      int n = read(fd[0], line, 199);
      line[n] = (char)0;
      close(fd[0]);
      path = string(line);

      if( path.size() == 0)
      {
         cout << "INFO: wkhtml2pdf not found, can't generate pdf file!\n"
              << "  check http://code.google.com/p/wkhtmltopdf/" << endl;
      } else {
         const string cmd = "wkhtml2pdf -b --zoom 0.7 -s Letter results.html results.pdf";
         system(cmd.c_str());
         cout << "<results.pdf> created." << endl;
      }

   }

   void usage()
   {
      cout << "------ CPP11UT:\n";
      cout << "-d            : enable debug output\n";
      cout << "-a            : details of all test steps, not just the failed ones\n";
      cout << "-f <filter>   : disable all testcases that do NOT contain <filter>\n";
      cout << "-nopdf        : no pdf generation\n";

      cout << "-h            : show available parameters\n";
      cout << "--help        : show available parameters\n";
      cout << endl;
   }

   void setParas(const vector<string> & args)
   {
      Manager & mgr = S_Manager::getInstance();

      auto p = find(args.begin(), args.end(), "-d");
      if( p != args.end() )
      {
         mgr.m_debug = true;
      }

      p = find(args.begin(), args.end(),"-a");
      if( p != args.end() )
      {
         mgr.m_ts_all = true;
      }

      p = find(args.begin(), args.end(),"-nopdf");
      if( p != args.end() )
      {
         mgr.m_PDF = false;
      }

      p = find(args.begin(), args.end(),"-f");
      if( p != args.end() )
      {
         p++;
         if( p == args.end() )
         {
            cerr << "-f needs a filter, see --help" << endl;
         }

         mgr.setFilter(*p);
         cout << "INFO: set filter to <" << *p << ">" << endl;

      }

      p = find(args.begin(), args.end(),"-h");
      if( p != args.end() )
      {
         usage();
         exit(0);
      }

      p = find(args.begin(), args.end(),"--help");
      if( p != args.end() )
      {
         usage();
         exit(0);
      }
   }

   void getArgs( int argc, char *argv[] )
   {
      vector<string> args;
      for( int i = 0; i< argc; ++i )
      {
         args.push_back(argv[i]);
      }

      setParas(args);
   }
}

// === namespace UT =============================================================
namespace UT {

   void step_passed(UT::Probe & probe);
   void step_failed_eq(UT::Probe & probe,
                       const string & aVal,
                       const string & bVal,
                       const std::string & str_a,
                       const std::string & str_b);

   Probe::Probe(
                const std::string & fname,          // file name where probe is located
                const int lineNumber,               // __LINE__
                const std::string & tpath,          // p1: tcase path; unique KEY
                const std::string & desc,           // p2: description
                std::function< void (void)> tfunc   // p3: lambda test function
                ) :
      m_fname(fname),
      m_line(lineNumber),
      m_tpath(tpath),
      m_desc(desc),
      m_func(tfunc),

      m_suite(""),
      m_tname(""),
      m_disabled(false)
   {
      convertNames(tpath, m_suite, m_tname, m_disabled);
      S_Manager::getInstance().subscribe(*this);
   }

   Probe::Probe( const std::string & cmdString,   // cmd-string
                 const std::string & data         // data
                 )
   {
      if( cmdString == "setTitle" )
      {
         S_Manager::getInstance().setTitle(data);
         return;
      }

      cerr << "ERROR: invalid cmd found: <" << cmdString << "> (ignored)." << endl;
   }

   void Probe::isTrue( const bool expr, const string & exprStr )
   {
      if(expr)
      {
         step_passed(*this);
      } else {
         step_failed_isTrue( *this, exprStr );
      }
   }

   void Probe::isOK( const std::string & actual )
   {
      step_passed(*this);
   }

   void Probe::except( const exception & e)
   {
      ostringstream oss;

      m_is_EX_happened = true;
      m_EX_happened = typeIdName(e);

      if( (m_is_EX_expected == m_is_EX_happened) and (m_EX_happened == m_EX_expected ))
      {
         m_actual = "";
         m_expect = "";
         m_msg = "exception happened (or not), as expected";
         step_passed(*this);
      } else {
         m_what = e.what();
         S_Manager::getInstance().setFAIL(*this);
      }
   }

   void Probe::expect( const string & e )
   {
      m_is_EX_expected = true;
      m_EX_expected = e;

      S_Manager::getInstance().setExpect(*this);
   }

   // undefined exception _inside_ test step
   void Probe::undef_except()
   {
      ostringstream oss;
      m_EX_happened = "(UNKNOWN)";
      S_Manager::getInstance().setFAIL(*this);
   }

   string Probe::dump()
   {
      ostringstream oss;
      oss << "m_fname: " << m_fname << "\n"
          << "m_line : " << m_line << "\n"
          << "m_tpath: " << m_tpath << "\n"
          << "m_desc : " << m_desc << "\n"
          << "m_func : func()\n"
          << "m_suite: " << m_suite << "\n"
          << "m_tname: " << m_tname << "\n"
          << "m_disabled: " << boolalpha << m_disabled;

      return oss.str();
   }

   void step_passed(UT::Probe & probe)
   {
      probe.m_actual = "";
      probe.m_expect = "";
      probe.m_msg = "";

      S_Manager::getInstance().setOK(probe);
   };

   void step_failed_eq(UT::Probe & probe,
                       const string & aVal,
                       const string & bVal,
                       const std::string & str_a,
                       const std::string & str_b)
   {
      std::ostringstream oss;
      std::ostringstream oss_expect;
      std::string value = bVal;

      oss_expect << str_b;

      if( bVal != str_b )
      {
         oss_expect << " => " << bVal;
      }

      const std::string expStr = oss_expect.str();
      oss << str_a << " expected to be: " << expStr
          << ", but evaluated to " << aVal;

      probe.m_msg = oss.str();

      // expect field
      probe.m_expect = expStr;
      std::ostringstream os2;
      os2 << str_a;

      if( aVal != str_a )
      {
         os2 << " => " << aVal;
      }

      probe.m_actual = os2.str();

      S_Manager::getInstance().setFAIL(probe);
   };

}

int main( int argc, char *argv[])
{
   getArgs(argc, argv);

   Manager & mgr = S_Manager::getInstance();

   mgr.exec();
   mgr.genStatistics();
   mgr.printSummary();

   if( mgr.m_debug )
   {
      cout << mgr.dump() << endl;
   }

   mgr.genHTML();
   mgr.genXML();
   mgr.genPDF();

   return 0;
}
