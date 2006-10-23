/* Unit tests for treeview.
 *
 * Copyright 2005 Krzysztof Foltman
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include <assert.h>
#include <stdarg.h>

#include "windef.h"
#include "winbase.h"
#include "wingdi.h"
#include "winuser.h"
#include "winnls.h"
#include "winreg.h"
#include "commctrl.h" 

#include "resources.h"

#include "wine/test.h"

HWND hMainWnd;
BOOL g_fBlockHotItemChange;
BOOL g_fReceivedHotItemChange;
BOOL g_fExpectedHotItemOld;
BOOL g_fExpectedHotItemNew;
 
#define compare(val, exp, format) ok((val) == (exp), #val " value " format " expected " format "\n", (val), (exp));

static void MakeButton(TBBUTTON *p, int idCommand, int fsStyle, int nString) {
  p->iBitmap = -2;
  p->idCommand = idCommand;
  p->fsState = TBSTATE_ENABLED;
  p->fsStyle = fsStyle;
  p->iString = nString;
}

static LRESULT MyWnd_Notify(LPARAM lParam)
{
    NMHDR *hdr = (NMHDR *)lParam;
    NMTBHOTITEM *nmhi;
    switch (hdr->code)
    {
        case TBN_HOTITEMCHANGE:
            nmhi = (NMTBHOTITEM *)lParam;
            g_fReceivedHotItemChange = TRUE;
            if (g_fExpectedHotItemOld != g_fExpectedHotItemNew)
            {
                compare(nmhi->idOld, g_fExpectedHotItemOld, "%d");
                compare(nmhi->idNew, g_fExpectedHotItemNew, "%d");
            }
            if (g_fBlockHotItemChange)
                return 1;
            break;
    }
    return 0;
}

static LRESULT CALLBACK MyWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
        case WM_NOTIFY:
            return MyWnd_Notify(lParam);
    }
    return DefWindowProcA(hWnd, msg, wParam, lParam);
}

static void basic_test(void)
{
    TBBUTTON buttons[9];
    HWND hToolbar;
    int i;
    for (i=0; i<9; i++)
        MakeButton(buttons+i, 1000+i, TBSTYLE_CHECKGROUP, 0);
    MakeButton(buttons+3, 1003, TBSTYLE_SEP|TBSTYLE_GROUP, 0);
    MakeButton(buttons+6, 1006, TBSTYLE_SEP, 0);

    hToolbar = CreateToolbarEx(hMainWnd,
        WS_VISIBLE | WS_CLIPCHILDREN | CCS_TOP |
        WS_CHILD | TBSTYLE_LIST,
        100,
        0, NULL, (UINT)0,
        buttons, sizeof(buttons)/sizeof(buttons[0]),
        0, 0, 20, 16, sizeof(TBBUTTON));
    ok(hToolbar != NULL, "Toolbar creation\n");
    SendMessage(hToolbar, TB_ADDSTRINGA, 0, (LPARAM)"test\000");

    /* test for exclusion working inside a separator-separated :-) group */
    SendMessage(hToolbar, TB_CHECKBUTTON, 1000, 1); /* press A1 */
    ok(SendMessage(hToolbar, TB_ISBUTTONCHECKED, 1000, 0), "A1 pressed\n");
    ok(!SendMessage(hToolbar, TB_ISBUTTONCHECKED, 1001, 0), "A2 not pressed\n");

    SendMessage(hToolbar, TB_CHECKBUTTON, 1004, 1); /* press A5, release A1 */
    ok(SendMessage(hToolbar, TB_ISBUTTONCHECKED, 1004, 0), "A5 pressed\n");
    ok(!SendMessage(hToolbar, TB_ISBUTTONCHECKED, 1000, 0), "A1 not pressed anymore\n");

    SendMessage(hToolbar, TB_CHECKBUTTON, 1005, 1); /* press A6, release A5 */
    ok(SendMessage(hToolbar, TB_ISBUTTONCHECKED, 1005, 0), "A6 pressed\n");
    ok(!SendMessage(hToolbar, TB_ISBUTTONCHECKED, 1004, 0), "A5 not pressed anymore\n");

    /* test for inter-group crosstalk, ie. two radio groups interfering with each other */
    SendMessage(hToolbar, TB_CHECKBUTTON, 1007, 1); /* press B2 */
    ok(SendMessage(hToolbar, TB_ISBUTTONCHECKED, 1005, 0), "A6 still pressed, no inter-group crosstalk\n");
    ok(!SendMessage(hToolbar, TB_ISBUTTONCHECKED, 1000, 0), "A1 still not pressed\n");
    ok(SendMessage(hToolbar, TB_ISBUTTONCHECKED, 1007, 0), "B2 pressed\n");

    SendMessage(hToolbar, TB_CHECKBUTTON, 1000, 1); /* press A1 and ensure B group didn't suffer */
    ok(!SendMessage(hToolbar, TB_ISBUTTONCHECKED, 1005, 0), "A6 not pressed anymore\n");
    ok(SendMessage(hToolbar, TB_ISBUTTONCHECKED, 1000, 0), "A1 pressed\n");
    ok(SendMessage(hToolbar, TB_ISBUTTONCHECKED, 1007, 0), "B2 still pressed\n");

    SendMessage(hToolbar, TB_CHECKBUTTON, 1008, 1); /* press B3, and ensure A group didn't suffer */
    ok(!SendMessage(hToolbar, TB_ISBUTTONCHECKED, 1005, 0), "A6 pressed\n");
    ok(SendMessage(hToolbar, TB_ISBUTTONCHECKED, 1000, 0), "A1 pressed\n");
    ok(!SendMessage(hToolbar, TB_ISBUTTONCHECKED, 1007, 0), "B2 not pressed\n");
    ok(SendMessage(hToolbar, TB_ISBUTTONCHECKED, 1008, 0), "B3 pressed\n");
    DestroyWindow(hToolbar);
}

static void rebuild_toolbar(HWND *hToolbar)
{
    if (*hToolbar != NULL)
        DestroyWindow(*hToolbar);
    *hToolbar = CreateWindowEx(0, TOOLBARCLASSNAME, NULL, WS_CHILD | WS_VISIBLE, 0, 0, 0, 0,
        hMainWnd, (HMENU)5, GetModuleHandle(NULL), NULL);
    ok(*hToolbar != NULL, "Toolbar creation problem\n");
    ok(SendMessage(*hToolbar, TB_BUTTONSTRUCTSIZE, (WPARAM)sizeof(TBBUTTON), 0) == 0, "TB_BUTTONSTRUCTSIZE failed\n");
    ok(SendMessage(*hToolbar, TB_AUTOSIZE, 0, 0) == 0, "TB_AUTOSIZE failed\n");
}

void rebuild_toolbar_with_buttons(HWND *hToolbar)
{
    TBBUTTON buttons[5];
    rebuild_toolbar(hToolbar);
    
    ZeroMemory(&buttons, sizeof(buttons));
    buttons[0].idCommand = 1;
    buttons[0].fsStyle = BTNS_BUTTON;
    buttons[0].fsState = TBSTATE_ENABLED;
    buttons[1].idCommand = 3;
    buttons[1].fsStyle = BTNS_BUTTON;
    buttons[1].fsState = TBSTATE_ENABLED;
    buttons[2].idCommand = 5;
    buttons[2].fsStyle = BTNS_SEP;
    buttons[2].fsState = TBSTATE_ENABLED;
    buttons[3].idCommand = 7;
    buttons[3].fsStyle = BTNS_BUTTON;
    buttons[3].fsState = TBSTATE_ENABLED;
    buttons[4].idCommand = 9;
    buttons[4].fsStyle = BTNS_BUTTON;
    buttons[4].fsState = 0;  /* disabled */
    ok(SendMessage(*hToolbar, TB_ADDBUTTONS, 5, (LPARAM)buttons) == 1, "TB_ADDBUTTONS failed\n");
    ok(SendMessage(*hToolbar, TB_AUTOSIZE, 0, 0) == 0, "TB_AUTOSIZE failed\n");
}


#define CHECK_IMAGELIST(count, dx, dy) { \
    int cx, cy; \
    HIMAGELIST himl = (HIMAGELIST)SendMessageA(hToolbar, TB_GETIMAGELIST, 0, 0); \
    ok(himl != NULL, "No image list\n"); \
    if (himl != NULL) {\
        ok(ImageList_GetImageCount(himl) == count, "Images count mismatch - %d vs %d\n", count, ImageList_GetImageCount(himl)); \
        ImageList_GetIconSize(himl, &cx, &cy); \
        ok(cx == dx && cy == cy, "Icon size mismatch - %dx%d vs %dx%d\n", dx, dy, cx, cy); \
    } \
}

#define CHECK_IMAGELIST_TODO_COUNT(count, dx, dy) { \
    int cx, cy; \
    HIMAGELIST himl = (HIMAGELIST)SendMessageA(hToolbar, TB_GETIMAGELIST, 0, 0); \
    ok(himl != NULL, "No image list\n"); \
    if (himl != NULL) {\
        todo_wine ok(ImageList_GetImageCount(himl) == count, "Images count mismatch - %d vs %d\n", count, ImageList_GetImageCount(himl)); \
        ImageList_GetIconSize(himl, &cx, &cy); \
        ok(cx == dx && cy == cy, "Icon size mismatch - %dx%d vs %dx%d\n", dx, dy, cx, cy); \
    } \
}

#define CHECK_IMAGELIST_TODO_COUNT_SIZE(count, dx, dy) { \
    int cx, cy; \
    HIMAGELIST himl = (HIMAGELIST)SendMessageA(hToolbar, TB_GETIMAGELIST, 0, 0); \
    ok(himl != NULL, "No image list\n"); \
    if (himl != NULL) {\
        todo_wine ok(ImageList_GetImageCount(himl) == count, "Images count mismatch - %d vs %d\n", count, ImageList_GetImageCount(himl)); \
        ImageList_GetIconSize(himl, &cx, &cy); \
        todo_wine ok(cx == dx && cy == cy, "Icon size mismatch - %dx%d vs %dx%d\n", dx, dy, cx, cy); \
    } \
}

static void test_add_bitmap(void)
{
    HWND hToolbar = NULL;
    TBADDBITMAP bmp128;
    TBADDBITMAP bmp80;
    TBADDBITMAP stdsmall;
    TBADDBITMAP addbmp;
    HIMAGELIST himl;
    INT ret;

    /* empty 128x15 bitmap */
    bmp128.hInst = GetModuleHandle(NULL);
    bmp128.nID = IDB_BITMAP_128x15;

    /* empty 80x15 bitmap */
    bmp80.hInst = GetModuleHandle(NULL);
    bmp80.nID = IDB_BITMAP_80x15;

    /* standard bitmap - 240x15 pixels */
    stdsmall.hInst = HINST_COMMCTRL;
    stdsmall.nID = IDB_STD_SMALL_COLOR;

    rebuild_toolbar(&hToolbar);
    ok(SendMessageA(hToolbar, TB_ADDBITMAP, 8, (LPARAM)&bmp128) == 0, "TB_ADDBITMAP - unexpected return\n");
    CHECK_IMAGELIST(8, 16, 15);
    
    /* adding more bitmaps */
    ok(SendMessageA(hToolbar, TB_ADDBITMAP, 5, (LPARAM)&bmp80) == 8, "TB_ADDBITMAP - unexpected return\n");
    CHECK_IMAGELIST(13, 16, 15);
    /* adding the same bitmap will simply return the index of the already loaded block */
    ret = SendMessageA(hToolbar, TB_ADDBITMAP, 8, (LPARAM)&bmp128);
    ok(ret == 0, "TB_ADDBITMAP - unexpected return %d\n", ret);
    CHECK_IMAGELIST(13, 16, 15);
    ret = SendMessageA(hToolbar, TB_ADDBITMAP, 5, (LPARAM)&bmp80);
    ok(ret == 8, "TB_ADDBITMAP - unexpected return %d\n", ret);
    CHECK_IMAGELIST(13, 16, 15);
    /* even if we increase the wParam */
    ret = SendMessageA(hToolbar, TB_ADDBITMAP, 55, (LPARAM)&bmp80);
    ok(ret == 8, "TB_ADDBITMAP - unexpected return %d\n", ret);
    CHECK_IMAGELIST(13, 16, 15);

    /* when the wParam is smaller than the bitmaps count but non-zero, all the bitmaps will be added*/
    rebuild_toolbar(&hToolbar);
    ok(SendMessageA(hToolbar, TB_ADDBITMAP, 3, (LPARAM)&bmp128) == 0, "TB_ADDBITMAP - unexpected return\n");
    CHECK_IMAGELIST(8, 16, 15);
    ret = SendMessageA(hToolbar, TB_ADDBITMAP, 5, (LPARAM)&bmp80);
    ok(ret == 3, "TB_ADDBITMAP - unexpected return %d\n", ret);
    /* the returned value is misleading - id 8 is the id of the first icon from bmp80 */
    CHECK_IMAGELIST(13, 16, 15);

    /* the same for negative wParam */
    rebuild_toolbar(&hToolbar);
    ret = SendMessageA(hToolbar, TB_ADDBITMAP, -143, (LPARAM)&bmp128);
    ok(ret == 0, "TB_ADDBITMAP - unexpected return %d\n", ret);
    CHECK_IMAGELIST(8, 16, 15);
    ret = SendMessageA(hToolbar, TB_ADDBITMAP, 1, (LPARAM)&bmp80);
    ok(ret == -143, "TB_ADDBITMAP - unexpected return %d\n", ret);
    CHECK_IMAGELIST(13, 16, 15);

    /* for zero only one bitmap will be added */
    rebuild_toolbar(&hToolbar);
    ret = SendMessageA(hToolbar, TB_ADDBITMAP, 0, (LPARAM)&bmp80);
    ok(ret == 0, "TB_ADDBITMAP - unexpected return %d\n", ret);
    CHECK_IMAGELIST(1, 16, 15);

    /* if wParam is larger than the amount of icons, the list is grown */
    rebuild_toolbar(&hToolbar);
    ok(SendMessageA(hToolbar, TB_ADDBITMAP, 100, (LPARAM)&bmp80) == 0, "TB_ADDBITMAP - unexpected return\n");
    CHECK_IMAGELIST(100, 16, 15);
    ret = SendMessageA(hToolbar, TB_ADDBITMAP, 100, (LPARAM)&bmp128);
    ok(ret == 100, "TB_ADDBITMAP - unexpected return %d\n", ret);
    CHECK_IMAGELIST(200, 16, 15);

    /* adding built-in items - the wParam is ignored */
    rebuild_toolbar(&hToolbar);
    ok(SendMessageA(hToolbar, TB_ADDBITMAP, 5, (LPARAM)&bmp80) == 0, "TB_ADDBITMAP - unexpected return\n");
    CHECK_IMAGELIST(5, 16, 15);
    ok(SendMessageA(hToolbar, TB_ADDBITMAP, 0, (LPARAM)&stdsmall) == 5, "TB_ADDBITMAP - unexpected return\n");
    CHECK_IMAGELIST(20, 16, 15);
    ok(SendMessageA(hToolbar, TB_ADDBITMAP, 5, (LPARAM)&bmp128) == 20, "TB_ADDBITMAP - unexpected return\n");
    CHECK_IMAGELIST(28, 16, 15);

    /* when we increase the bitmap size, less icons will be created */
    rebuild_toolbar(&hToolbar);
    ok(SendMessageA(hToolbar, TB_SETBITMAPSIZE, 0, MAKELONG(20, 20)) == TRUE, "TB_SETBITMAPSIZE failed\n");
    ok(SendMessageA(hToolbar, TB_ADDBITMAP, 1, (LPARAM)&bmp128) == 0, "TB_ADDBITMAP - unexpected return\n");
    CHECK_IMAGELIST(6, 20, 20);
    ret = SendMessageA(hToolbar, TB_ADDBITMAP, 1, (LPARAM)&bmp80);
    ok(ret == 1, "TB_ADDBITMAP - unexpected return %d\n", ret);
    CHECK_IMAGELIST(10, 20, 20);
    /* the icons can be resized - an UpdateWindow is needed as this probably happens during WM_PAINT */
    ok(SendMessageA(hToolbar, TB_SETBITMAPSIZE, 0, MAKELONG(8, 8)) == TRUE, "TB_SETBITMAPSIZE failed\n");
    UpdateWindow(hToolbar);
    CHECK_IMAGELIST(26, 8, 8);
    /* loading a standard bitmaps automatically resizes the icons */
    ok(SendMessageA(hToolbar, TB_ADDBITMAP, 1, (LPARAM)&stdsmall) == 2, "TB_ADDBITMAP - unexpected return\n");
    UpdateWindow(hToolbar);
    CHECK_IMAGELIST(28, 16, 15);

    /* two more SETBITMAPSIZE tests */
    rebuild_toolbar(&hToolbar);
    ok(SendMessageA(hToolbar, TB_ADDBITMAP, 100, (LPARAM)&bmp128) == 0, "TB_ADDBITMAP - unexpected return\n");
    CHECK_IMAGELIST(100, 16, 15);
    ok(SendMessageA(hToolbar, TB_ADDBITMAP, 100, (LPARAM)&bmp80) == 100, "TB_ADDBITMAP - unexpected return\n");
    CHECK_IMAGELIST(200, 16, 15);
    ok(SendMessageA(hToolbar, TB_SETBITMAPSIZE, 0, MAKELONG(8, 8)) == TRUE, "TB_SETBITMAPSIZE failed\n");
    UpdateWindow(hToolbar);
    CHECK_IMAGELIST(200, 8, 8);
    ok(SendMessageA(hToolbar, TB_SETBITMAPSIZE, 0, MAKELONG(30, 30)) == TRUE, "TB_SETBITMAPSIZE failed\n");
    UpdateWindow(hToolbar);
    CHECK_IMAGELIST(200, 30, 30);
    rebuild_toolbar(&hToolbar);
    ok(SendMessageA(hToolbar, TB_ADDBITMAP, 5, (LPARAM)&bmp128) == 0, "TB_ADDBITMAP - unexpected return\n");
    CHECK_IMAGELIST(8, 16, 15);
    ok(SendMessageA(hToolbar, TB_ADDBITMAP, 3, (LPARAM)&bmp80) == 5, "TB_ADDBITMAP - unexpected return\n");
    CHECK_IMAGELIST(13, 16, 15);
    ok(SendMessageA(hToolbar, TB_SETBITMAPSIZE, 0, MAKELONG(30, 30)) == TRUE, "TB_SETBITMAPSIZE failed\n");
    UpdateWindow(hToolbar);
    CHECK_IMAGELIST(8, 30, 30);

    /* the control can add bitmaps to an existing image list */
    rebuild_toolbar(&hToolbar);
    himl = ImageList_LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(IDB_BITMAP_80x15), 20, 2, CLR_NONE, IMAGE_BITMAP, LR_DEFAULTCOLOR);
    ok(himl != NULL, "failed to create imagelist\n");
    ok(SendMessageA(hToolbar, TB_SETIMAGELIST, 0, (LPARAM)himl) == 0, "TB_SETIMAGELIST failed\n");
    CHECK_IMAGELIST(4, 20, 15);
    ok(SendMessageA(hToolbar, TB_ADDBITMAP, 1, (LPARAM)&bmp128) == 0, "TB_ADDBITMAP - unexpected return\n");
    CHECK_IMAGELIST(10, 20, 15);
    /* however TB_SETBITMAPSIZE/add std bitmap won't change the image size (the button size does change!) */
    ok(SendMessageA(hToolbar, TB_SETBITMAPSIZE, 0, MAKELONG(8, 8)) == TRUE, "TB_SETBITMAPSIZE failed\n");
    UpdateWindow(hToolbar);
    CHECK_IMAGELIST(10, 20, 15);
    ok(SendMessageA(hToolbar, TB_ADDBITMAP, 0, (LPARAM)&stdsmall) == 1, "TB_SETBITMAPSIZE failed\n");
    UpdateWindow(hToolbar);
    CHECK_IMAGELIST_TODO_COUNT(22, 20, 15);

    /* check standard bitmaps */
    addbmp.hInst = HINST_COMMCTRL;
    addbmp.nID = IDB_STD_SMALL_COLOR;
    rebuild_toolbar(&hToolbar);
    ok(SendMessageA(hToolbar, TB_ADDBITMAP, 1, (LPARAM)&addbmp) == 0, "TB_ADDBITMAP - unexpected return\n");
    CHECK_IMAGELIST(15, 16, 15);
    addbmp.nID = IDB_STD_LARGE_COLOR;
    rebuild_toolbar(&hToolbar);
    ok(SendMessageA(hToolbar, TB_ADDBITMAP, 1, (LPARAM)&addbmp) == 0, "TB_ADDBITMAP - unexpected return\n");
    CHECK_IMAGELIST(15, 24, 24);

    addbmp.nID = IDB_VIEW_SMALL_COLOR;
    rebuild_toolbar(&hToolbar);
    ok(SendMessageA(hToolbar, TB_ADDBITMAP, 1, (LPARAM)&addbmp) == 0, "TB_ADDBITMAP - unexpected return\n");
    CHECK_IMAGELIST(12, 16, 15);
    addbmp.nID = IDB_VIEW_LARGE_COLOR;
    rebuild_toolbar(&hToolbar);
    ok(SendMessageA(hToolbar, TB_ADDBITMAP, 1, (LPARAM)&addbmp) == 0, "TB_ADDBITMAP - unexpected return\n");
    CHECK_IMAGELIST(12, 24, 24);

    addbmp.nID = IDB_HIST_SMALL_COLOR;
    rebuild_toolbar(&hToolbar);
    ok(SendMessageA(hToolbar, TB_ADDBITMAP, 1, (LPARAM)&addbmp) == 0, "TB_ADDBITMAP - unexpected return\n");
    CHECK_IMAGELIST(5, 16, 15);
    addbmp.nID = IDB_HIST_LARGE_COLOR;
    rebuild_toolbar(&hToolbar);
    ok(SendMessageA(hToolbar, TB_ADDBITMAP, 1, (LPARAM)&addbmp) == 0, "TB_ADDBITMAP - unexpected return\n");
    CHECK_IMAGELIST(5, 24, 24);


    DestroyWindow(hToolbar);
}

#define CHECK_STRING_TABLE(count, tab) { \
        INT _i; \
        CHAR _buf[260]; \
        for (_i = 0; _i < (count); _i++) {\
            ret = SendMessageA(hToolbar, TB_GETSTRING, MAKEWPARAM(260, _i), (LPARAM)_buf); \
            ok(ret >= 0, "TB_GETSTRING - unexpected return %d while checking string %d\n", ret, _i); \
            if (ret >= 0) \
                ok(strcmp(_buf, (tab)[_i]) == 0, "Invalid string #%d - '%s' vs '%s'\n", _i, (tab)[_i], _buf); \
        } \
        ok(SendMessageA(hToolbar, TB_GETSTRING, MAKEWPARAM(260, (count)), (LPARAM)_buf) == -1, \
            "Too many string in table\n"); \
    }

void test_add_string()
{
    LPCSTR test1 = "a\0b\0";
    LPCSTR test2 = "|a|b||\0";
    LPCSTR ret1[] = {"a", "b"};
    LPCSTR ret2[] = {"a", "b", "|a|b||"};
    LPCSTR ret3[] = {"a", "b", "|a|b||", "p", "q"};
    LPCSTR ret4[] = {"a", "b", "|a|b||", "p", "q", "p"};
    LPCSTR ret5[] = {"a", "b", "|a|b||", "p", "q", "p", "p", "q"};
    LPCSTR ret6[] = {"a", "b", "|a|b||", "p", "q", "p", "p", "q", "p", "", "q"};
    LPCSTR ret7[] = {"a", "b", "|a|b||", "p", "q", "p", "p", "q", "p", "", "q", "br", "c", "d"};
    HWND hToolbar = NULL;
    TBBUTTON button;
    int ret;

    rebuild_toolbar(&hToolbar);
    ret = SendMessageA(hToolbar, TB_ADDSTRINGA, 0, (LPARAM)test1);
    ok(ret == 0, "TB_ADDSTRINGA - unexpected return %d\n", ret);
    CHECK_STRING_TABLE(2, ret1);
    ret = SendMessageA(hToolbar, TB_ADDSTRINGA, 0, (LPARAM)test2);
    ok(ret == 2, "TB_ADDSTRINGA - unexpected return %d\n", ret);
    CHECK_STRING_TABLE(3, ret2);

    ret = SendMessageA(hToolbar, TB_ADDSTRINGA, (WPARAM)GetModuleHandle(NULL), IDS_TBADD1);
    ok(ret == 3, "TB_ADDSTRINGA - unexpected return %d\n", ret);
    CHECK_STRING_TABLE(3, ret2);
    ret = SendMessageA(hToolbar, TB_ADDSTRINGA, (WPARAM)GetModuleHandle(NULL), IDS_TBADD2);
    ok(ret == 3, "TB_ADDSTRINGA - unexpected return %d\n", ret);
    CHECK_STRING_TABLE(5, ret3);
    ret = SendMessageA(hToolbar, TB_ADDSTRINGA, (WPARAM)GetModuleHandle(NULL), IDS_TBADD3);
    ok(ret == 5, "TB_ADDSTRINGA - unexpected return %d\n", ret);
    CHECK_STRING_TABLE(6, ret4);
    ret = SendMessageA(hToolbar, TB_ADDSTRINGA, (WPARAM)GetModuleHandle(NULL), IDS_TBADD4);
    ok(ret == 6, "TB_ADDSTRINGA - unexpected return %d\n", ret);
    CHECK_STRING_TABLE(8, ret5);
    ret = SendMessageA(hToolbar, TB_ADDSTRINGA, (WPARAM)GetModuleHandle(NULL), IDS_TBADD5);
    ok(ret == 8, "TB_ADDSTRINGA - unexpected return %d\n", ret);
    CHECK_STRING_TABLE(11, ret6);
    ret = SendMessageA(hToolbar, TB_ADDSTRINGA, (WPARAM)GetModuleHandle(NULL), IDS_TBADD7);
    ok(ret == 11, "TB_ADDSTRINGA - unexpected return %d\n", ret);
    CHECK_STRING_TABLE(14, ret7);

    ZeroMemory(&button, sizeof(button));
    button.iString = (UINT_PTR)"Test";
    SendMessageA(hToolbar, TB_INSERTBUTTONA, 0, (LPARAM)&button);
    CHECK_STRING_TABLE(14, ret7);
    SendMessageA(hToolbar, TB_ADDBUTTONSA, 1, (LPARAM)&button);
    CHECK_STRING_TABLE(14, ret7);
}

static void expect_hot_notify(int idold, int idnew)
{
    g_fExpectedHotItemOld = idold;
    g_fExpectedHotItemNew = idnew;
    g_fReceivedHotItemChange = FALSE;
}

#define check_hot_notify() \
    ok(g_fReceivedHotItemChange, "TBN_HOTITEMCHANGE not received\n"); \
    g_fExpectedHotItemOld = g_fExpectedHotItemNew = 0;

void test_hotitem()
{
    HWND hToolbar = NULL;
    TBBUTTONINFO tbinfo;
    LRESULT ret;

    g_fBlockHotItemChange = FALSE;

    rebuild_toolbar_with_buttons(&hToolbar);
    /* set TBSTYLE_FLAT. comctl5 allows hot items only for such toolbars.
     * comctl6 doesn't have this requirement even when theme == NULL */
    SetWindowLong(hToolbar, GWL_STYLE, TBSTYLE_FLAT | GetWindowLong(hToolbar, GWL_STYLE));
    ret = SendMessage(hToolbar, TB_GETHOTITEM, 0, 0);
    ok(ret == -1, "Hot item: %ld, expected -1\n", ret);
    ret = SendMessage(hToolbar, TB_SETHOTITEM, 1, 0);
    ok(ret == -1, "TB_SETHOTITEM returned %ld, expected -1\n", ret);
    ret = SendMessage(hToolbar, TB_GETHOTITEM, 0, 0);
    ok(ret == 1, "Hot item: %ld, expected 1\n", ret);
    ret = SendMessage(hToolbar, TB_SETHOTITEM, 2, 0);
    ok(ret == 1, "TB_SETHOTITEM returned %ld, expected 1\n", ret);

    ret = SendMessage(hToolbar, TB_SETHOTITEM, 0xbeef, 0);
    ok(ret == 2, "TB_SETHOTITEM returned %ld, expected 2\n", ret);
    ret = SendMessage(hToolbar, TB_GETHOTITEM, 0, 0);
    ok(ret == 2, "Hot item: %lx, expected 2\n", ret);
    ret = SendMessage(hToolbar, TB_SETHOTITEM, -0xbeef, 0);
    ok(ret == 2, "TB_SETHOTITEM returned %ld, expected 2\n", ret);
    ret = SendMessage(hToolbar, TB_GETHOTITEM, 0, 0);
    ok(ret == -1, "Hot item: %lx, expected -1\n", ret);

    expect_hot_notify(0, 7);
    ret = SendMessage(hToolbar, TB_SETHOTITEM, 3, 0);
    ok(ret == -1, "TB_SETHOTITEM returned %ld, expected -1\n", ret);
    check_hot_notify();
    ret = SendMessage(hToolbar, TB_GETHOTITEM, 0, 0);
    ok(ret == 3, "Hot item: %lx, expected 3\n", ret);
    g_fBlockHotItemChange = TRUE;
    ret = SendMessage(hToolbar, TB_SETHOTITEM, 2, 0);
    ok(ret == 3, "TB_SETHOTITEM returned %ld, expected 2\n", ret);
    ret = SendMessage(hToolbar, TB_GETHOTITEM, 0, 0);
    ok(ret == 3, "Hot item: %lx, expected 3\n", ret);
    g_fBlockHotItemChange = FALSE;

    g_fReceivedHotItemChange = FALSE;
    ret = SendMessage(hToolbar, TB_SETHOTITEM, 0xbeaf, 0);
    ok(ret == 3, "TB_SETHOTITEM returned %ld, expected 3\n", ret);
    ok(g_fReceivedHotItemChange == FALSE, "TBN_HOTITEMCHANGE received for invalid parameter\n");

    g_fReceivedHotItemChange = FALSE;
    ret = SendMessage(hToolbar, TB_SETHOTITEM, 3, 0);
    ok(ret == 3, "TB_SETHOTITEM returned %ld, expected 3\n", ret);
    ok(g_fReceivedHotItemChange == FALSE, "TBN_HOTITEMCHANGE received after a duplication\n");

    expect_hot_notify(7, 0);
    ret = SendMessage(hToolbar, TB_SETHOTITEM, -0xbeaf, 0);
    ok(ret == 3, "TB_SETHOTITEM returned %ld, expected 3\n", ret);
    check_hot_notify();
    SendMessage(hToolbar, TB_SETHOTITEM, 3, 0);

    /* setting disabled buttons will generate a notify with the button id but no button will be hot */
    expect_hot_notify(7, 9);
    ret = SendMessage(hToolbar, TB_SETHOTITEM, 4, 0);
    ok(ret == 3, "TB_SETHOTITEM returned %ld, expected 3\n", ret);
    check_hot_notify();
    ret = SendMessage(hToolbar, TB_GETHOTITEM, 0, 0);
    ok(ret == -1, "Hot item: %lx, expected -1\n", ret);
    /* enabling the button won't change that */
    SendMessage(hToolbar, TB_ENABLEBUTTON, 9, TRUE);
    ret = SendMessage(hToolbar, TB_GETHOTITEM, 0, 0);
    ok(ret == -1, "TB_SETHOTITEM returned %ld, expected -1\n", ret);

    /* disabling a hot button works */
    ret = SendMessage(hToolbar, TB_SETHOTITEM, 3, 0);
    ok(ret == -1, "TB_SETHOTITEM returned %ld, expected -1\n", ret);
    g_fReceivedHotItemChange = FALSE;
    SendMessage(hToolbar, TB_ENABLEBUTTON, 7, FALSE);
    ret = SendMessage(hToolbar, TB_GETHOTITEM, 0, 0);
    ok(ret == 3, "TB_SETHOTITEM returned %ld, expected 3\n", ret);
    ok(g_fReceivedHotItemChange == FALSE, "Unexpected TBN_HOTITEMCHANGE\n");

    SendMessage(hToolbar, TB_SETHOTITEM, 1, 0);
    tbinfo.cbSize = sizeof(TBBUTTONINFO);
    tbinfo.dwMask = TBIF_STATE;
    tbinfo.fsState = 0;  /* disabled */
    g_fReceivedHotItemChange = FALSE;
    ok(SendMessage(hToolbar, TB_SETBUTTONINFO, 1, (LPARAM)&tbinfo) == TRUE, "TB_SETBUTTONINFO failed\n");
    ret = SendMessage(hToolbar, TB_GETHOTITEM, 0, 0);
    ok(ret == 1, "TB_SETHOTITEM returned %ld, expected 1\n", ret);
    ok(g_fReceivedHotItemChange == FALSE, "Unexpected TBN_HOTITEMCHANGE\n");

}

START_TEST(toolbar)
{
    WNDCLASSA wc;
    MSG msg;
    RECT rc;
  
    InitCommonControls();
  
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = GetModuleHandleA(NULL);
    wc.hIcon = NULL;
    wc.hCursor = LoadCursorA(NULL, MAKEINTRESOURCEA(IDC_IBEAM));
    wc.hbrBackground = GetSysColorBrush(COLOR_WINDOW);
    wc.lpszMenuName = NULL;
    wc.lpszClassName = "MyTestWnd";
    wc.lpfnWndProc = MyWndProc;
    RegisterClassA(&wc);
    
    hMainWnd = CreateWindowExA(0, "MyTestWnd", "Blah", WS_OVERLAPPEDWINDOW, 
      CW_USEDEFAULT, CW_USEDEFAULT, 680, 260, NULL, NULL, GetModuleHandleA(NULL), 0);
    GetClientRect(hMainWnd, &rc);
    ShowWindow(hMainWnd, SW_SHOW);

    basic_test();
    test_add_bitmap();
    test_add_string();
    test_hotitem();

    PostQuitMessage(0);
    while(GetMessageA(&msg,0,0,0)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    DestroyWindow(hMainWnd);
}
