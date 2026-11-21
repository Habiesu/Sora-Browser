#include <windows.h>

#include "include/cef_app.h"

// Proceso hijo de CEF (renderer, GPU, utilidad). No carga Qt.
int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int)
{
    CefMainArgs args(instance);
    return CefExecuteProcess(args, nullptr, nullptr);
}
