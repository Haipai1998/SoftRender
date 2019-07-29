#include <cstdio>
#include <tchar.h>
#include <Windows.h>
#include <assert.h>
typedef unsigned int UINT32;

typedef struct {
    int width;
    int height;
    UINT32 **framebuffer;
    float **zbuffer;
    UINT32 **texture;
    int tex_width;
    int tex_height;
    float max_u;
    float max_v;
    int render_state;
    UINT32 background;
    UINT32 foreground;
}device_t;

int screen_exit;
int screen_pitch;
int screen_w, screen_h;
int screen_keys[512];
static HWND screen_handle = NULL;
static HDC screen_dc = NULL;
static HBITMAP screen_hb = NULL;
static HBITMAP screen_ob = NULL;
unsigned char *screen_fb = NULL;


static LRESULT screen_events(HWND hWnd, UINT msg,
    WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CLOSE: screen_exit = 1; break;
    //case WM_KEYDOWN: screen_keys[wParam & 511] = 1; break;
    //case WM_KEYUP: screen_keys[wParam & 511] = 0; break;
    default: return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    return 0;
}

int screen_close(void) {
    if (screen_dc) {
        if (screen_ob) {
            SelectObject(screen_dc, screen_ob);
            screen_ob = NULL;
        }
        DeleteDC(screen_dc);
        screen_dc = NULL;
    }
    if (screen_hb) {
        DeleteObject(screen_hb);
        screen_hb = NULL;
    }
    if (screen_handle) {
        CloseWindow(screen_handle);
        screen_handle = NULL;
    }
    return 0;
}

void screen_dispatch(void) {
    MSG msg;
    while (1) {
        if (!PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE)) break;
        if (!GetMessage(&msg, NULL, 0, 0)) break;
        DispatchMessage(&msg);
    }
}

int screen_init(int w, int h, const TCHAR *title) {
    WNDCLASS wc = { CS_BYTEALIGNCLIENT, (WNDPROC)screen_events, 0, 0, 0,
        NULL, NULL, NULL, NULL, _T("SCREEN3.1415926") };
    BITMAPINFO bi = { { sizeof(BITMAPINFOHEADER), w, -h, 1, 32, BI_RGB,
        w * h * 4, 0, 0, 0, 0 } };
    RECT rect = { 0, 0, w, h };
    int wx, wy, sx, sy;
    LPVOID ptr;
    HDC hDC;

    screen_close();

    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.hInstance = GetModuleHandle(NULL);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    if (!RegisterClass(&wc)) return -1;

    screen_handle = CreateWindow(_T("SCREEN3.1415926"), title,
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        0, 0, 0, 0, NULL, NULL, wc.hInstance, NULL);
    if (screen_handle == NULL) return -2;

    screen_exit = 0;
    hDC = GetDC(screen_handle);
    screen_dc = CreateCompatibleDC(hDC);
    ReleaseDC(screen_handle, hDC);

    screen_hb = CreateDIBSection(screen_dc, &bi, DIB_RGB_COLORS, &ptr, 0, 0);
    if (screen_hb == NULL) return -3;

    screen_ob = (HBITMAP)SelectObject(screen_dc, screen_hb);
    screen_fb = (unsigned char*)ptr;
    screen_pitch = w * 4;

    AdjustWindowRect(&rect, GetWindowLong(screen_handle, GWL_STYLE), 0);
    wx = rect.right - rect.left;
    wy = rect.bottom - rect.top;
    sx = (GetSystemMetrics(SM_CXSCREEN) - wx) / 2;
    sy = (GetSystemMetrics(SM_CYSCREEN) - wy) / 2;
    if (sy < 0) sy = 0;
    SetWindowPos(screen_handle, NULL, sx, sy, wx, wy, (SWP_NOCOPYBITS | SWP_NOZORDER | SWP_SHOWWINDOW));
    SetForegroundWindow(screen_handle);

    ShowWindow(screen_handle, SW_NORMAL);
    screen_dispatch();

    memset(screen_keys, 0, sizeof(int) * 512);
    memset(screen_fb, 0, w * h * 4);

    return 0;
}
void device_init(device_t *device, int width, int height, void *fb) {
    int need = sizeof(void*) * (height * 2 + 1024) + width * height * 8;
    char *ptr = (char*)malloc(need + 64);
    char *framebuf, *zbuf;
    int j;
    assert(ptr);
    device->framebuffer = (UINT32**)ptr;
    device->zbuffer = (float**)(ptr + sizeof(void*) * height);
    ptr += sizeof(void*) * height * 2;
    device->texture = (UINT32**)ptr;
    ptr += sizeof(void*) * 1024;
    framebuf = (char*)ptr;
    zbuf = (char*)ptr + width * height * 4;
    ptr += width * height * 8;
    if (fb != NULL) framebuf = (char*)fb;
    for (j = 0; j < height; j++) {
        device->framebuffer[j] = (UINT32*)(framebuf + width * 4 * j);
        device->zbuffer[j] = (float*)(zbuf + width * 4 * j);
    }
    device->texture[0] = (UINT32*)ptr;
    device->texture[1] = (UINT32*)(ptr + 16);
    memset(device->texture[0], 0, 64);
    device->tex_width = 2;
    device->tex_height = 2;
    device->max_u = 1.0f;
    device->max_v = 1.0f;
    device->width = width;
    device->height = height;
    device->background = 0xc0c0c0;
    device->foreground = 0;
    //transform_init(&device->transform, width, height);
    //device->render_state = RENDER_STATE_WIREFRAME;
}
int main(void) {
    device_t device; //定义渲染方块结构体，包括背景颜色，框大小

    const TCHAR *Title = _T("Soft Render implement");
    screen_w = 800;
    screen_h = 600;

    int res = screen_init(screen_w, screen_h, Title); //初始化窗口
    if (!res) {
        //ok
    }
    else {
        printf("Fuction : screen_init() failure\n");
        return 0;
    }

    device_init(&device, screen_w, screen_h,screen_fb);// 初始化设备，将HDC和对应的内存绑定

    //printf(">>!%d", strlen((const char*)screen_fb));
    //for (int i = 0; i < strlen((const char*)screen_fb);++i) {
    //    printf("%c ! ",screen_fb[i]);
    //}
    //printf(">>");

    while (true) {
        if (screen_exit)    break;

        screen_dispatch();//分发所有命令给回调函数去处理：处理按键输入
        device_InitFB();
    }

    return 0;
}