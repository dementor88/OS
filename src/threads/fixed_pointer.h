#include <stdint.h>
#include <debug.h>

int64_t load_avg = 0;
int current_max_priority = 0;
int64_t FIXED_POINT = 16384;

int64_t add(int64_t a, int64_t b){

	return a + b;

}

int64_t add_int(int64_t a, int b){

	return a + ((int64_t)b)*FIXED_POINT;

}

int64_t sub(int64_t a, int64_t b){

	return a - b;

}

int64_t mul(int64_t a, int64_t b ){

	return a * b / FIXED_POINT;

}

int64_t mul_int(int64_t a, int b ){

	return a * b;

}

int64_t div(int64_t a, int64_t b){

	ASSERT(b!=0);
	return a * FIXED_POINT / b;

}

int64_t div_int(int64_t a, int b){

	ASSERT(b!=0);
	return a / b;

}

int get_fixed_value(int64_t a){


	return a>0 ?
		(a*100 + FIXED_POINT/2)/FIXED_POINT :
		(a*100 - FIXED_POINT/2)/FIXED_POINT ;

}

int to_fixed_value(int a){

	return a * FIXED_POINT;

}
