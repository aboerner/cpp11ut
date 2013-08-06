#include "unittest.h"

#include "MyClass.h"

using namespace std;

namespace
{
   SET_TITLE("MyClass");

   float tc(const float a, const float b)
   {
      MyClass my;
      return my.divide(a,b);
   }


   TEST_CASE("/1-Basic/0-ctor", "check c-tor",
             EXEC {
                MyClass my; my=my;
             });

   TEST_CASE("/1-Basic/t01", "10.0/5.0 => 2.0",
             EXEC {
                IS_EQUAL(tc(10.0,5.0), 2.0);
                IS_TRUE(tc(10,5) > 1);
             });


   TEST_CASE("/1-Basic/t02", "10/2 => 5",
             EXEC {
                IS_EQUAL(tc(10,2), 5);
                IS_TRUE(tc(10,2) < 10);
             });

   TEST_CASE("/1-Basic/t03", "7/2 => 3.5",
             EXEC {
                IS_EQUAL(tc(7,2), 3.5);
             });

   TEST_CASE("/1-Basic/t04", "4/4 => 1",
             EXEC {
                // int a=100, b=98; // this will require an additional () around EXEC
                int a=100;
                int b=98;
                IS_EQUAL(tc(6,3), a-b);
             });

   TEST_CASE("/2-Advanced/a-01", " (-) 5/5 => 1",
             EXEC {
               IS_EQUAL(tc(55,-5), -11);
               IS_EQUAL(tc(-55,5), -11);
               IS_EQUAL(tc(-55,-5), 11);
             });

   TEST_CASE("/2-Advanced/a-02", "100/.5 => 200",
             EXEC {
                IS_EQUAL(tc(100,.5), 200);
             });

   TEST_CASE("/3-Big/t01", "1000000/2 => 500000",
             EXEC {
                IS_EQUAL(tc(1000000,2), 500000);
             });

   TEST_CASE("/3-Big/t02", "some more big numbers /2",
             EXEC {
                IS_EQUAL(tc(1000000000,2), 500000000);
                IS_EQUAL(tc(10000000000,3), 5000000000);
                IS_EQUAL(tc(100000000000,2), 50000000000);
             });

   TEST_CASE("/3-Big/t03/DISABLE", "some even bigger numbers /2",
             EXEC {
                IS_EQUAL(tc(10000000000,2), 500000000);
                IS_EQUAL(tc(100000000000,3), 5000000000);
                IS_EQUAL(tc(1000000000000,2), 50000000000);
             });

   TEST_CASE("/4-Except/e-01", "test div zero exception",
             EXEC {
                EXPECT("std::runtime_error");
                IS_EQUAL(tc(5,0), 1);

                EXPECT("std::range_error");
                IS_EQUAL(tc(5,42), 1);
             });

   TEST_CASE("/4-Except/e-02", "div zero, don't expect exception",
             EXEC {
                 IS_EQUAL(tc(5,0), 1);
             });

   TEST_CASE("/4-Except/e-03", "test range exception",
             EXEC {
                IS_EQUAL(tc(5,42), 1);
             });

   TEST_CASE("/4-Except/e-04", "int exception",
             EXEC {
                IS_EQUAL(tc(5,43), 1);
             });

   TEST_CASE("/4-Except/e-05", "std::exception derived exception",
             EXEC {
                IS_EQUAL(tc(5,44), 1);
             });

   TEST_CASE("/4-Except/e-06", "proprietary exception",
             EXEC {
                IS_EQUAL(tc(5,45), 1);
             });

   TEST_CASE("/4-Except/e-07", "exception outside of test step",
             EXEC {
                IS_EQUAL(tc(12,3), 4);
                throw runtime_error("gone wrong");
             });

   TEST_CASE("/4-Except/e-08", "check for exception",
             EXEC {
                EXPECT("std::range_error");
                tc(3,42);
             });

   TEST_CASE("/4-Except/e-08a", "check for exception",
             EXEC {
                ASSERT(tc(42,3) > 13);
             });

   TEST_CASE("/4-Except/e-09", "exception does not match",
             EXEC {
                EXPECT("std::range_error");
                IS_EQUAL(tc(12,3), 4);
             });

   TEST_CASE("/4-Except/e-10", "good case",
             EXEC {
                IS_EQUAL(tc(12,3), 4);
             });

}

