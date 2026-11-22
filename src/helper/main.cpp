// sora-helper.exe: los subprocesos de CEF (renderer, GPU, red, utilidades). No carga Qt.
#include <windows.h>

#include "engine/EngineApp.h"

// La GPU de Chromium va en este proceso: aquí deciden los controladores qué gráfica usar.
// Con Qt WebEngine (GPU en el mismo proceso) la integrada parpadeaba en los directos de Discord;
// con CEF se usa la integrada como Chrome. Si vuelve el parpadeo: compilar con /DSORA_DEDICATED_GPU.
#ifdef SORA_DEDICATED_GPU // opcional: por defecto la integrada, como Chrome (ver bench/results/2026-11-21-cef.md)
extern "C" {
__declspec(dllexport) unsigned long NvOptimusEnablement = 1;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int)
{
    CefMainArgs args(instance);
    return CefExecuteProcess(args, new sora::EngineApp, nullptr);
}
