#include "Base/DxRenderBase.h"
#include "ShapesApp.h"
#include "SimpleScreenApp.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance, PSTR cmdLine, int showCmd)
{
#if  defined(_DEBUG) || defined(DEBUG)
    _CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
#endif

    try
    {
        ShapesApp App(hInstance);
        if(!App.Initialize())
            return 0;
        return App.Run();
    }
    catch (DxException& e)
    {
        MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
        return 0;
    }
}