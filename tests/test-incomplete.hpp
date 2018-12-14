#pragma once
#include "../value_ptr_incomplete.hpp"

struct incomplete_foo;
bool use_incomplete_foo(smart_ptr::value_ptr_incomplete<incomplete_foo>&, int val);

struct Widget;

template<class T>
struct wrapped {
	smart_ptr::value_ptr_incomplete<T> m;
	int count;

	wrapped() = default;
	~wrapped() = default;
};

int get_wrapped_count(const wrapped<Widget>&);
void set_widget_i(wrapped<Widget>&, int);
int get_widget_i(const wrapped<Widget>&);
void reset_widget(wrapped<Widget>&);
void swap_widgets(wrapped<Widget>&, wrapped<Widget>&);