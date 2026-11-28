//What TestWorkspaceIndexRuntime indexes: a system header that only the solution's Libc65 project has, and a header in
//that project's own include directory, which its build gives every unit.
#include <FromRows.h>
#include "SharedByRuntime.h"


int UsesRows(SFromRows* psRows, SSharedByRuntime* psShared) { return psRows->iRows + psShared->iShared; }
