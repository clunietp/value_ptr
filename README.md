value_ptr ![Travis CI](https://travis-ci.org/trent33/value_ptr.svg?branch=master)
===================
https://github.com/trent33/value_ptr

Introduction
------------
value_ptr is a smart pointer that preserves value semantics for both polymorphic and undefined types.  
value_ptr aims to address the following issues by reducing/eliminating the boilerplate needed to facilitate value semantics.

- The polymorphic copy problem.  Given a class heirarchy, preserve value semantics while preventing object slicing.

*Example:*

Without value_ptr:

    struct Base { virtual Base* clone() const; base stuff... };  
    struct Derived : Base { Base* clone() const; derived stuff... };
    
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
    struct Base { virtual Base* clone() const; base stuff... };  
    struct Derived : Base { Base* clone() const; derived stuff... };
    
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

    struct U;	// undefined type
    class MyAwesomeClass {
        std::unique_ptr<U> u; // unique_ptr doesn't really fit, but we don't want to manage a raw pointer either.
    };

    MyAwesomeClass a{};
    auto b = a;	// ERROR C2280!

with value_ptr:

    struct U;	// undefined type
    class MyAwesomeClass {
        smart_ptr::value_ptr<U> u;
    };

    MyAwesomeClass a{};
    auto b = a;	// no problem!

For a full working PIMPL example, see tests/test-pimpl.cpp


Features
------------
- Header only, single file, cross platform, no dependencies outside the STL
- Compatible interface/convertible to std::unique_ptr<T>
- Space efficient:  
    -  known types:  sizeof( value_ptr<T> ) == sizeof(T*)
    -  unknown types:  sizeof( value_ptr<T> ) == sizeof(T*) + two function pointers
- Polymorphic copying:  
    -  Automatically detects/utilizes clone() member function
    -  Static assertion prevents object slicing if a user-defined copier not provided or clone member not found
- Support for stateful and stateless deleters and copiers, via functors or lambdas
- Unit tested, valgrind clean

Dependencies
------------
- None outside of the STL

Usage
-----------------------
- include "value_ptr.hpp" in your project
- Use value_ptr just like a unique_ptr
- To leverage the automatic clone() detection feature, your base/derived classes should have a method with the signature:  `YourBaseClassHere* clone() const`
    - Alternatively, you can provide a functor or lambda which handles the copying.  See tests/main.cpp for examples

For additional examples/usage, see the unit tests in tests/main.cpp

Tested compilers:
------------
- MSVC 2015 Update 3
- G++ 4.8, G++ 5
- Clang 3.5

Running Tests
-------------
- Compile and run the files in the 'tests' directory


Issues/pull requests welcome
