#include "Application/Application.h"

int main()
{
#ifndef DIST
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    Application application;
    application.run();
    
    return 0;
}