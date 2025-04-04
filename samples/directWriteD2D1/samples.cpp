#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

#include <iostream>

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

IDWriteFactory* pDWriteFactory_;
IDWriteTextFormat* pTextFormat_;

const wchar_t* wszText_;
UINT32 cTextLength_;

// Rendering with direct 2D
// ----------------------------------------------------------------------------
ID2D1Factory* pD2DFactory_;
ID2D1HwndRenderTarget* pRT_;
ID2D1SolidColorBrush* pBlackBrush_;

// Window stuff
// ----------------------------------------------------------------------------
HWND hwnd_;

float dpiScaleY_;
float dpiScaleX_;

template <class T> void SafeRelease(T **ppT)
{
    if (*ppT)
    {
        (*ppT)->Release();
        *ppT = NULL;
    }
}

HRESULT CreateDeviceResources()
{
    HRESULT hr = S_OK;
    
    
    RECT rc;
    GetClientRect(hwnd_, &rc);
    
    D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);
    
    if (!pRT_)
    {
        // Create a Direct2D render target.
        hr = pD2DFactory_->CreateHwndRenderTarget(
                                                  D2D1::RenderTargetProperties(),
                                                  D2D1::HwndRenderTargetProperties(
                                                                                   hwnd_,
                                                                                   size
                                                                                   ),
                                                  &pRT_
                                                  );
        
        // Create a black brush.
        if (SUCCEEDED(hr))
        {
            hr = pRT_->CreateSolidColorBrush(
                                             D2D1::ColorF(D2D1::ColorF::Black),
                                             &pBlackBrush_
                                             );
        }
    }
    
    
    return hr;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_CREATE)
    {
        return 1;
    }
    
    
    switch(message)
    {
        case WM_SIZE:
        {
            UINT width = LOWORD(lParam);
            UINT height = HIWORD(lParam);
            if (pRT_)
            {
                D2D1_SIZE_U size;
                size.width = width;
                size.height = height;
                pRT_->Resize(size);
            }
        }
        return 0;
        
        case WM_PAINT:
        case WM_DISPLAYCHANGE:
        {
            PAINTSTRUCT ps;
            BeginPaint(hwnd, &ps);
            {
                HRESULT hr;
                
                hr = CreateDeviceResources();
                
                if (!(pRT_->CheckWindowState() & D2D1_WINDOW_STATE_OCCLUDED))
                {
                    pRT_->BeginDraw();
                    
                    pRT_->SetTransform(D2D1::IdentityMatrix());
                    
                    pRT_->Clear(D2D1::ColorF(D2D1::ColorF::White));
                    
                    {
                        RECT rc;
                        
                        GetClientRect(
                                      hwnd_,
                                      &rc
                                      );
                        
                        // Create a D2D rect that is the same size as the window.
                        
                        D2D1_RECT_F layoutRect = D2D1::RectF(
                                                             static_cast<FLOAT>(rc.top) / dpiScaleY_,
                                                             static_cast<FLOAT>(rc.left) / dpiScaleX_,
                                                             static_cast<FLOAT>(rc.right - rc.left) / dpiScaleX_,
                                                             static_cast<FLOAT>(rc.bottom - rc.top) / dpiScaleY_
                                                             );
                        
                        
                        // Use the DrawText method of the D2D render target interface to draw.
                        
                        pRT_->DrawText(
                                       wszText_,        // The string to render.
                                       cTextLength_,    // The string's length.
                                       pTextFormat_,    // The text format.
                                       layoutRect,       // The region of the window where the text will be rendered.
                                       pBlackBrush_     // The brush used to draw the text.
                                       );
                        
                    }
                    
                    // Call the DrawText method of this class.
                    hr = S_OK;
                    
                    if (SUCCEEDED(hr))
                    {
                        hr = pRT_->EndDraw(
                                           );
                    }
                }
                
                if (FAILED(hr))
                {
                    SafeRelease(&pRT_);
                    SafeRelease(&pBlackBrush_);
                }
            }
            EndPaint(
                     hwnd,
                     &ps
                     );
        }
        return 0;
        
        case WM_DESTROY:
        {
            PostQuitMessage(0);
        }
        return 1;
    }
    
    return DefWindowProc(
                         hwnd,
                         message,
                         wParam,
                         lParam
                         );
}

int main() {
    // Initialize
    WNDCLASSEX wcex;
    
    HINSTANCE hInstance; 
    
    hInstance = GetModuleHandle(NULL);
    
    //get the dpi information
    HDC screen = GetDC(0);
    dpiScaleX_ = GetDeviceCaps(screen, LOGPIXELSX) / 96.0f;
    dpiScaleY_ = GetDeviceCaps(screen, LOGPIXELSY) / 96.0f;
    ReleaseDC(0, screen);
    
    HRESULT hr = S_OK;
    
    ATOM atom;
    
    // Register window class.
    wcex.cbSize        = sizeof(WNDCLASSEX);
    wcex.style         = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc   = WndProc;
    wcex.cbClsExtra    = 0;
    wcex.cbWndExtra    = sizeof(LONG_PTR);
    wcex.hInstance     = hInstance;
    wcex.hbrBackground = NULL;
    wcex.lpszMenuName  = NULL;
    wcex.hIcon         = LoadIcon(
                                  NULL,
                                  IDI_APPLICATION);
    wcex.hCursor       = LoadCursor(
                                    NULL,
                                    IDC_ARROW);
    wcex.lpszClassName = TEXT("D2DSimpleText");
    wcex.hIconSm       = LoadIcon(
                                  NULL,
                                  IDI_APPLICATION
                                  );
    
    atom = RegisterClassEx(
                           &wcex
                           );
    
    hr = atom ? S_OK : E_FAIL;
    
    // Create window.
    hwnd_ = CreateWindow(
                         TEXT("D2DSimpleText"),
                         TEXT(""),
                         WS_OVERLAPPEDWINDOW,
                         CW_USEDEFAULT,
                         CW_USEDEFAULT,
                         static_cast<int>(640.0f / dpiScaleX_),
                         static_cast<int>(480.0f / dpiScaleY_),
                         NULL,
                         NULL,
                         hInstance,
                         NULL
                         );
    if (!hwnd_)
    {
        DWORD error = GetLastError();
        std::wcerr << L"CreateWindow failed with error: " << error << std::endl;
        return -1;
    }
    
    if (SUCCEEDED(hr))
    {
        hr = hwnd_ ? S_OK : E_FAIL;
    }
    else {
        DWORD error = GetLastError();
        std::wcerr << L"Failed to create factory: " << error << std::endl;
        return -1;
    }
    
    
    hr =  D2D1CreateFactory(
                            D2D1_FACTORY_TYPE_SINGLE_THREADED,
                            &pD2DFactory_
                            );
    if( SUCCEEDED(hr) ) {
        hr = DWriteCreateFactory(
                                 DWRITE_FACTORY_TYPE_SHARED,
                                 __uuidof(IDWriteFactory),
                                 reinterpret_cast<IUnknown**>(&pDWriteFactory_)
                                 );
    }
    else {
        DWORD error = GetLastError();
        std::wcerr << L"Failed to create factory: " << error << std::endl;
        return -1;
    }
    
    wszText_ = L"Hello World using  DirectWrite!";
    cTextLength_ = (UINT32) wcslen(wszText_);
    
    if (SUCCEEDED(hr))
    {
        hr = pDWriteFactory_->CreateTextFormat(
                                               L"Gabriola",                // Font family name.
                                               NULL,                       // Font collection (NULL sets it to use the system font collection).
                                               DWRITE_FONT_WEIGHT_REGULAR,
                                               DWRITE_FONT_STYLE_NORMAL,
                                               DWRITE_FONT_STRETCH_NORMAL,
                                               72.0f,
                                               L"en-us",
                                               &pTextFormat_
                                               );
    }
    
    // Center align (horizontally) the text.
    if (SUCCEEDED(hr))
    {
        hr = pTextFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    }
    else {
        DWORD error = GetLastError();
        std::wcerr << L"Failed to create text format: " << error << std::endl;
        return -1;
    }
    
    if (SUCCEEDED(hr))
    {
        hr = pTextFormat_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }
    else {
        DWORD error = GetLastError();
        std::wcerr << L"Failed to create text format alignment: " << error << std::endl;
        return -1;
    }
    
    RECT rc;
    GetClientRect(hwnd_, &rc);
    
    D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);
    
    if (!pRT_)
    {
        // Create a Direct2D render target.
        hr = pD2DFactory_->CreateHwndRenderTarget(
                                                  D2D1::RenderTargetProperties(),
                                                  D2D1::HwndRenderTargetProperties(
                                                                                   hwnd_,
                                                                                   size
                                                                                   ),
                                                  &pRT_
                                                  );
        
        // Create a black brush.
        if (SUCCEEDED(hr))
        {
            hr = pRT_->CreateSolidColorBrush(
                                             D2D1::ColorF(D2D1::ColorF::Black),
                                             &pBlackBrush_
                                             );
        }else {
            DWORD error = GetLastError();
            std::wcerr << L"Failed to create solid color brush: " << error << std::endl;
            return -1;
        }
    }
    
    hr = CreateDeviceResources();
    
    if (SUCCEEDED(hr))
    {
        pRT_->BeginDraw();
        
        pRT_->SetTransform(D2D1::IdentityMatrix());
        
        pRT_->Clear(D2D1::ColorF(D2D1::ColorF::White));
        
        // Call the DrawText method of this class.
        {
            RECT rc;
            
            GetClientRect(
                          hwnd_,
                          &rc
                          );
            
            // Create a D2D rect that is the same size as the window.
            
            D2D1_RECT_F layoutRect = D2D1::RectF(
                                                 static_cast<FLOAT>(rc.top) / dpiScaleY_,
                                                 static_cast<FLOAT>(rc.left) / dpiScaleX_,
                                                 static_cast<FLOAT>(rc.right - rc.left) / dpiScaleX_,
                                                 static_cast<FLOAT>(rc.bottom - rc.top) / dpiScaleY_
                                                 );
            
            
            // Use the DrawText method of the D2D render target interface to draw.
            
            pRT_->DrawText(
                           wszText_,        // The string to render.
                           cTextLength_,    // The string's length.
                           pTextFormat_,    // The text format.
                           layoutRect,       // The region of the window where the text will be rendered.
                           pBlackBrush_     // The brush used to draw the text.
                           );
            
        }
        
        if (SUCCEEDED(hr))
        {
            hr = pRT_->EndDraw();
        }
    }
    else {
        DWORD error = GetLastError();
        std::wcerr << L"Failed to create text format alignment: " << error << std::endl;
        return -1;
    }
    
    if (FAILED(hr))
    {
        //DiscardDeviceResources();
        SafeRelease(&pRT_);
        SafeRelease(&pBlackBrush_);
    }
    
    ShowWindow(hwnd_, SW_SHOWNORMAL);
    UpdateWindow(hwnd_);
    
    MSG msg = { };
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    
    return 0;
}