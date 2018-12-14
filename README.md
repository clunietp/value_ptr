value_ptr ![Travis CI](https://travis-ci.org/clunietp/value_ptr.svg?branch=master)
===================
https://github.com/clunietp/value_ptr

Introduction
------------
value_ptr is a C++11 header only, deep-copying smart pointer that preserves value semantics for both polymorphic and undefined types.  
value_ptr aims to address the following issues by reducing/eliminating the boilerplate needed to facilitate value semantics:  The polymorphic copy problem, and the undefined type problem.

- The polymorphic copy problem.  Given a class heirarchy, preserve value semantics while preventing object slicing.

*Example:*

Without value_ptr:

    struct Base { virtual Base* clone() const { return new Base(*this); } };  
    struct Derived : Base { Base* clone() const { return new Derived(*this); };
    
    struct MyAwesomeClass {
        std::unique_ptr<Base> foo;
    };

    int main() {
        MyAwesomeClass a{};
        // lets make a copy of a
        auto b = a;	// ERROR.  Now we have to do a bunch of boilerplate to clone 'foo', etc.  Boo!
    }

With value_ptr:

    #include "value_ptr.hpp"
    struct Base { virtual Base* clone() const { return new Base(*this); } };  
    struct Derived : Base { Base* clone() const { return new Derived(*this); };
    
    struct MyAwesomeClass {
        smart_ptr::value_ptr<Base> foo;
    };

    int main() {
        MyAwesomeClass a{};
        // lets make a copy of a
        auto b = a;	// no boilerplate, no slicing, no problem.  yay!
    }

- The undefined type problem.  

Given a declared-but-not-yet defined type (which may or may not be polymorphic), preserve value semantics and prevent object slicing.  
This problem is often seen in the PIMPL idiom and often associated with forward declared classes.

*Example:*

Without value_ptr

    struct U;	// undefined type, defined in another file somewhere that we can't/don't want to include
    class MyAwesomeClass {
        std::unique_ptr<U> u; // unique_ptr doesn't really fit, but we don't want to manage a raw pointer either.
    };

    MyAwesomeClass a{};
    auto b = a;	// ERROR C2280!

with value_ptr:

	#include "value_ptr_incomplete.hpp"
    struct U;	// undefined type
    class MyAwesomeClass {
        smart_ptr::value_ptr_incomplete<U> u;
    };

    MyAwesomeClass a{};
    auto b = a;	// no problem!

For a working PIMPL example, see tests/test-pimpl.cpp


Features
------------
- Header only, single file, cross platform, no dependencies outside the STL
- Compatible interface/convertible to std::unique_ptr<T>
- Space efficient:  
    -  Utilizes empty base optimization to minimize memory footprint
    -  complete types:  `sizeof( value_ptr<T> ) == sizeof(T*) == sizeof(std::unique_ptr<T>)`
    -  incomplete types:  `sizeof( value_ptr_incomplete<T> ) == sizeof(T*)` + two function pointers
- Polymorphic copying:  
    -  Automatically detects/utilizes clone() member function
    -  Static assertion prevents object slicing if a user-defined copier not provided or clone member not found
- Support for stateful and stateless deleters and copiers, via functors or lambdas
- Unit tested, valgrind clean
- Permissive license (Boost)

Dependencies/Requirements
------------
- C++11, STL

Usage
-----------------------
- include "value_ptr.hpp" and/or "value_ptr_incomplete.hpp" in your project
- Use value_ptr just like a unique_ptr
- To leverage the automatic clone() detection feature, your base/derived classes should have a method with the signature:  `YourBaseClassHere* clone() const`
    - Alternatively, you can provide a functor or lambda which handles the copying.  See tests/main.cpp for examples

For additional examples/usage, see the unit tests in tests/main.cpp

Known limitations:
------------
- Support for arrays (a la unique_ptr) is not currently implemented

Tested compilers:
------------
- MSVC 2015 Update 3, MSVC 2017 15.8
- G++ 4.8, G++ 5
- Clang 3.5

Unit Tests ![Travis CI](https://travis-ci.org/clunietp/value_ptr.svg?branch=master)
-------------
- Compile and run the files in the 'tests' directory

Acknowledgements
---------
- http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2012/n3339.pdf , A Preliminary Proposal for a Deep-Copying Smart Pointer by Walter E Brown
- Quuxplusone on codereview.stackexchange.com

Issues/pull requests welcome
