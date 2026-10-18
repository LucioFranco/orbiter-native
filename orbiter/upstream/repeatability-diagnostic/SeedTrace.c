// External macOS diagnostic. Without DIAGNOSTIC_C_RNG_SEED it preserves every
// requested srand seed. Fixed seeds are explicit counterfactuals, not acceptance.
#include <stdlib.h>
#include <stdio.h>
#include <dlfcn.h>
#include <errno.h>
#include <time.h>
static void trace_srand(unsigned requested) {
    int saved_errno=errno;
    const char *fixed=getenv("DIAGNOSTIC_C_RNG_SEED");
    unsigned actual=fixed ? (unsigned)strtoul(fixed,0,10) : requested;
    // dyld excludes this image's own calls to the replacee.
    srand(actual);
    const char *path=getenv("RNG_TRACE");
    if(path){ FILE *f=fopen(path,"a");if(!f)abort();
        fprintf(f,"%u\t%u\t%ld\n",requested,actual,(long)time(NULL));fclose(f); }
    errno=saved_errno;
}
__attribute__((used)) static struct { const void *replacement; const void *replacee; }
interpose_srand __attribute__((section("__DATA,__interpose"))) = { (const void *)trace_srand,(const void *)srand };
