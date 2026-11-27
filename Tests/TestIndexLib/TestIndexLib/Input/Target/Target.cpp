//What TestWorkspaceIndexTarget indexes: a function for each macro a compiler defines itself.
#ifdef __LIBC65__
#include <Only65.h>
#endif


int Always(void) { return 0; }


#ifdef _WIN32
int OnHost(void) { return 1; }
#endif


#ifdef __linux__
int OnLinux(void) { return 5; }
#endif


//The project's own, in its GCC-x64 configuration alone.
#ifdef LINUX_GNU
int WithLinuxDefine(void) { return 6; }
#endif


#ifdef __W65C816__
int OnW65C816(void) { return 2; }
#endif


#ifdef __65C64__
int On65C64(void) { return 3; }
#endif


#ifdef __W65C816_ASSEMBLY__
int WithAssembly(void) { return 4; }
#endif

