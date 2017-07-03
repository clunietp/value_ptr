#ifdef _DEBUG
#ifdef _WIN32
// http://msdn.microsoft.com/en-us/library/e5ewb1h3%28v=VS.90%29.aspx
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#endif
#endif

#define BOOST_TEST_MAIN
#include <boost/test/unit_test.hpp>
#include "../value_ptr.hpp"

BOOST_AUTO_TEST_CASE( test ) {

	// leak checking
#ifdef _WIN32
	_CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );	// dump memory leaks at termination
	_CrtSetReportMode( _CRT_ERROR, _CRTDBG_MODE_DEBUG );
	_CrtSetReportMode( _CRT_WARN, _CRTDBG_MODE_DEBUG );
#endif

	int* test_leak = new int( 5 );
	BOOST_CHECK( true );
}