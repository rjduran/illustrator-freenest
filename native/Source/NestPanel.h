#pragma once
#include "NestSuites.h"
#include <functional>

// Illustrator owns the dockable frame; FreeNest owns only its child controls.
class NestPanel {
    SPPluginRef plugin=nullptr;
    AIPanelRef panel=nullptr;
    HWND content=nullptr,buttons[3]{};
    HFONT font=nullptr;
    HBRUSH background=nullptr;
    COLORREF back=RGB(240,240,240),text=RGB(20,20,20),border=RGB(120,120,120);
    std::function<void(int)> command;
    bool busy=false;
    int dpi=96;
    static LRESULT CALLBACK WindowProc(HWND,UINT,WPARAM,LPARAM);
    static void AIAPI SizeChanged(AIPanelRef);
    void Resize();
    void Layout();
    void Refresh();
    void DrawButton(const DRAWITEMSTRUCT&);
public:
    void Create(SPPluginRef,std::function<void(int)>);
    void Destroy();
    void Toggle();
    void SetBusy(bool);
    bool Visible() const;
};
