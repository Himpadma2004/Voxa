#include "core/Application.h"

int main()
{
    VOXA::Application app;

    if (!app.initialize())
    {
        return -1;
    }

    app.run();
    return 0;
}
