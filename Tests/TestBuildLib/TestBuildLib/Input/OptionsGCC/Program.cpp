//TestBuildOptionEditsGCC's program, which the IDE's checks build too: a define the dialog gives it is seen to reach g++.
#ifdef FAIL_ON_PURPOSE
#error The define reached the compiler
#endif


int main(void) { return 0; }
