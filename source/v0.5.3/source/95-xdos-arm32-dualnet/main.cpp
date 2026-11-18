#include "kernel.h"
#include <circle/startup.h>

int main(void)
{
    CKernel kernel;
    if (!kernel.Initialize()) { halt(); return EXIT_HALT; }
    kernel.Run();
    halt();
    return EXIT_HALT;
}
