// sora-helper.exe: los subprocesos de CEF (renderer, GPU, red, utilidades). No carga Qt.
#include <windows.h>

#include "engine/EngineApp.h"

// La GPU de Chromium va en este proceso: aquí deciden los controladores qué gráfica usar.
// Con la integrada, los directos de Discord desde la dedicada hacían parpadear las páginas.
extern "C" {
__declspec(dllexport) unsigned long NvOptimusEnablement = 1;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int)
{
    CefMainArgs args(instance);
    return CefExecuteProcess(args, new sora::EngineApp, nullptr);
}
