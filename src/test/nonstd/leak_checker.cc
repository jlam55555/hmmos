#include "./leak_checker.h"

uint64_t LeakChecker::construct_count = 0;
uint64_t LeakChecker::destruct_count = 0;
