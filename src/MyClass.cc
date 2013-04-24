#include "MyClass.h"

#include <stdexcept>

using namespace std;

class MyException : public exception {};

class MyOtherException {};

float MyClass::divide( const float a, const float b )
{
   if( b < 1e-6 && b > -1e-6)
   {
      throw runtime_error("division by zero");
   }

   if( b == 42 )
   {
      throw range_error("can't handle 42");
   }

   if( b == 43 )
   {
      throw 42;
   }

   if( b == 44 )
   {
      throw MyException();
   }

   if( b == 45 )
   {
      throw MyOtherException();
   }

   // this is a bug ;-)
   if( (b - a < 1e-6) && (a - b < 1e-6 ))
   {
      return a * b;
   }

   return a/b;
}
