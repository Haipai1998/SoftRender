#include <cstdio>
#include <tchar.h>
#include <Windows.h>
#include <math.h>
#include <assert.h>
typedef unsigned int UINT32;

typedef struct {
    float m[4][4];
}matrix_t;

typedef struct {
    matrix_t world;
    matrix_t view;
    matrix_t projection;
    matrix_t transform;
    float w, h;
}transform_t;


#define RENDER_STATE_WIREFRAME      1		// 渲染线框
#define RENDER_STATE_TEXTURE        2		// 渲染纹理
#define RENDER_STATE_COLOR          4		// 渲染颜色

typedef struct {
    transform_t transform;
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

typedef struct {
    float x, y, z, w;
}point_t, vector_t;

int screen_exit;
int screen_pitch;
int screen_w, screen_h;
int screen_keys[512];
static HWND screen_handle = NULL;
static HDC screen_dc = NULL;
static HBITMAP screen_hb = NULL;
static HBITMAP screen_ob = NULL;
unsigned char *screen_fb = NULL;

void matrix_set_identity(matrix_t *matrix) {
    for (int i = 0; i <= 3; i++) {
        for (int j = 0; j <= 3; j++) {
            if (i == j) {
                matrix->m[i][j] = 1.0f;
                continue;
            }
            matrix->m[i][j] = 0.0f;
        }
    }

}

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

void matrix_set_zero(matrix_t *matrix) {
    for (int i = 0; i <= 3; i++) {
        for (int j = 0; j <= 3; j++) {
            matrix->m[i][j] = 0.0f;
        }
    }
}

void matrix_set_perspective(matrix_t *m, float fovy, float aspect, float zn, float zf) {
    float fax = 1.0f / (float)tan(fovy * 0.5f);
    matrix_set_zero(m);
    m->m[0][0] = (float)(fax / aspect);
    m->m[1][1] = (float)(fax);
    m->m[2][2] = zf / (zf - zn);
    m->m[3][2] = -zn * zf / (zf - zn);
    m->m[2][3] = 1;
}

void transform_init(transform_t *ts, int width, int height) {
    float aspect = (float)width / ((float)height);
    matrix_set_identity(&ts->world);
    matrix_set_identity(&ts->view);
    matrix_set_perspective(&ts->projection, 3.1415926f * 0.5f, aspect, 1.0f, 500.0f);
    ts->w = (float)width;
    ts->h = (float)height;

    return;
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
    transform_init(&device->transform, width, height);
    device->render_state = RENDER_STATE_WIREFRAME;
}

void device_InitFB(device_t *device) {
    for (int i = 0; i < device->height; ++i) {
        int rgb = (device->height - i - 1) * 233 / (device->height - 1);
        rgb = (rgb << 16) | (rgb << 8) | rgb;
        UINT32 *dst = device->framebuffer[i];
        for (int j = 0; j < device->width; ++j) {
            dst[j] = rgb;
            //printf("%d %d\n",&dst[0],j);
        }
    }

    return;
}


void screen_update() {
    HDC hDC = GetDC(screen_handle);
    BitBlt(hDC, 0, 0, screen_w, screen_h, screen_dc, 0, 0, SRCCOPY);
    ReleaseDC(screen_handle, hDC);
    screen_dispatch();
}

void DrawPoint(device_t *device, int x, int y, UINT32 color) {
    if (((UINT32)x) < (UINT32)device->width && ((UINT32)y) < (UINT32)device->height) {
        device->framebuffer[y][x] = color;
    }
}

void matrix_mul(matrix_t *c, const matrix_t *a, const matrix_t *b) {
    matrix_t z;
    int i, j;
    for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
            z.m[j][i] = (a->m[j][0] * b->m[0][i]) +
                (a->m[j][1] * b->m[1][i]) +
                (a->m[j][2] * b->m[2][i]) +
                (a->m[j][3] * b->m[3][i]);
        }
    }
    c[0] = z;
}

void transform_update(transform_t *ts) {
    matrix_t m;
    matrix_mul(&m, &ts->world, &ts->view);
    matrix_mul(&ts->transform, &m, &ts->projection);
}
void vector_sub(vector_t *z, const vector_t *x, const vector_t *y) {
    z->x = x->x - y->x;
    z->y = x->y - y->y;
    z->z = x->z - y->z;
    z->w = 1.0;
}
float vector_length(const vector_t *v) {
    float sq = v->x * v->x + v->y * v->y + v->z * v->z;
    return (float)sqrt(sq);
}
void vector_normalize(vector_t *v) {
    float length = vector_length(v);
    if (length != 0.0f) {
        float inv = 1.0f / length;
        v->x *= inv;
        v->y *= inv;
        v->z *= inv;
    }
}
void vector_crossproduct(vector_t *z, const vector_t *x, const vector_t *y) {
    float m1, m2, m3;
    m1 = x->y * y->z - x->z * y->y;
    m2 = x->z * y->x - x->x * y->z;
    m3 = x->x * y->y - x->y * y->x;
    z->x = m1;
    z->y = m2;
    z->z = m3;
    z->w = 1.0f;
}
float vector_dotproduct(const vector_t *x, const vector_t *y) {
    return x->x * y->x + x->y * y->y + x->z * y->z;
}
void matrix_set_lookat(matrix_t *m, const vector_t *eye, const vector_t *at, const vector_t *up) {
    vector_t xaxis, yaxis, zaxis;

    vector_sub(&zaxis, at, eye);
    vector_normalize(&zaxis);
    vector_crossproduct(&xaxis, up, &zaxis);
    vector_normalize(&xaxis);
    vector_crossproduct(&yaxis, &zaxis, &xaxis);

    m->m[0][0] = xaxis.x;
    m->m[1][0] = xaxis.y;
    m->m[2][0] = xaxis.z;
    m->m[3][0] = -vector_dotproduct(&xaxis, eye);

    m->m[0][1] = yaxis.x;
    m->m[1][1] = yaxis.y;
    m->m[2][1] = yaxis.z;
    m->m[3][1] = -vector_dotproduct(&yaxis, eye);

    m->m[0][2] = zaxis.x;
    m->m[1][2] = zaxis.y;
    m->m[2][2] = zaxis.z;
    m->m[3][2] = -vector_dotproduct(&zaxis, eye);

    m->m[0][3] = m->m[1][3] = m->m[2][3] = 0.0f;
    m->m[3][3] = 1.0f;
}

//根据新的相机位置设立view矩阵
void camera_set_pos(device_t *device, int x, int y, int z) {
    point_t eye = { x, y, z, 1 }, at = { 0, 0, 0, 1 }, up = { 0, 0, 1, 1 };
    matrix_set_lookat(&device->transform.view, &eye, &at, &up);//重新更新世界到相机空间的矩阵
    transform_update(&device->transform);

    return;
}
// 旋转矩阵
void matrix_set_rotate(matrix_t *m, float x, float y, float z, float theta) {
    float qsin = (float)sin(theta * 0.5f);
    float qcos = (float)cos(theta * 0.5f);
    vector_t vec = { x, y, z, 1.0f };
    float w = qcos;
    vector_normalize(&vec);
    x = vec.x * qsin;
    y = vec.y * qsin;
    z = vec.z * qsin;
    m->m[0][0] = 1 - 2 * y * y - 2 * z * z;
    m->m[1][0] = 2 * x * y - 2 * w * z;
    m->m[2][0] = 2 * x * z + 2 * w * y;
    m->m[0][1] = 2 * x * y + 2 * w * z;
    m->m[1][1] = 1 - 2 * x * x - 2 * z * z;
    m->m[2][1] = 2 * y * z - 2 * w * x;
    m->m[0][2] = 2 * x * z - 2 * w * y;
    m->m[1][2] = 2 * y * z + 2 * w * x;
    m->m[2][2] = 1 - 2 * x * x - 2 * y * y;
    m->m[0][3] = m->m[1][3] = m->m[2][3] = 0.0f;
    m->m[3][0] = m->m[3][1] = m->m[3][2] = 0.0f;
    m->m[3][3] = 1.0f;
}
typedef struct { point_t pos; float rhw; } vertex_t;
vertex_t mesh[8] = {
    { { -1, -1,  1, 1 }, 1 },
    { {  1, -1,  1, 1 }, 1 },
    { {  1,  1,  1, 1 }, 1 },
    { { -1,  1,  1, 1 }, 1 },
    { { -1, -1, -1, 1 }, 1 },
    { {  1, -1, -1, 1 }, 1 },
    { {  1,  1, -1, 1 }, 1 },
    { { -1,  1, -1, 1 }, 1 },
};
void device_pixel(device_t *device, int x, int y, UINT32 color) {
    if (((UINT32)x) < (UINT32)device->width && ((UINT32)y) < (UINT32)device->height) {
        device->framebuffer[y][x] = color;
    }
}
// 绘制线段算法 BresenHam算法
void device_draw_line(device_t *device, int x1, int y1, int x2, int y2, UINT32 c) {
    int x, y, rem = 0;
    if (x1 == x2 && y1 == y2) {
        device_pixel(device, x1, y1, c);
    }
    else if (x1 == x2) {
        int inc = (y1 <= y2) ? 1 : -1;
        for (y = y1; y != y2; y += inc) device_pixel(device, x1, y, c);
        device_pixel(device, x2, y2, c);
    }
    else if (y1 == y2) {
        int inc = (x1 <= x2) ? 1 : -1;
        for (x = x1; x != x2; x += inc) device_pixel(device, x, y1, c);
        device_pixel(device, x2, y2, c);
    }
    else {
        int dx = (x1 < x2) ? x2 - x1 : x1 - x2;
        int dy = (y1 < y2) ? y2 - y1 : y1 - y2;
        if (dx >= dy) {
            if (x2 < x1) {
                x = x1, y = y1, x1 = x2, y1 = y2, x2 = x, y2 = y;
            }
            for (x = x1, y = y1; x <= x2; x++) {
                device_pixel(device, x, y, c);
                rem += dy;
                if (rem >= dx) {
                    rem -= dx;
                    y += (y2 >= y1) ? 1 : -1;
                    device_pixel(device, x, y, c);
                }
            }
            device_pixel(device, x2, y2, c);
        }
        else {
            if (y2 < y1) x = x1, y = y1, x1 = x2, y1 = y2, x2 = x, y2 = y;
            for (x = x1, y = y1; y <= y2; y++) {
                device_pixel(device, x, y, c);
                rem += dx;
                if (rem >= dy) {
                    rem -= dy;
                    x += (x2 >= x1) ? 1 : -1;
                    device_pixel(device, x, y, c);
                }
            }
            device_pixel(device, x2, y2, c);
        }
    }
}

void matrix_apply(vector_t *y, const vector_t *x, const matrix_t *m) {
    float X = x->x, Y = x->y, Z = x->z, W = x->w;
    y->x = X * m->m[0][0] + Y * m->m[1][0] + Z * m->m[2][0] + W * m->m[3][0];
    y->y = X * m->m[0][1] + Y * m->m[1][1] + Z * m->m[2][1] + W * m->m[3][1];
    y->z = X * m->m[0][2] + Y * m->m[1][2] + Z * m->m[2][2] + W * m->m[3][2];
    y->w = X * m->m[0][3] + Y * m->m[1][3] + Z * m->m[2][3] + W * m->m[3][3];
}
void transform_homogenize(const transform_t *ts, vector_t *y, const vector_t *x) {
    float rhw = 1.0f / x->w;
    y->x = (x->x * rhw + 1.0f) * ts->w * 0.5f;
    y->y = (1.0f - x->y * rhw) * ts->h * 0.5f;
    y->z = x->z * rhw;
    y->w = 1.0f;
}
void device_draw_primitive(device_t *device, const vertex_t *v1,
    const vertex_t *v2, const vertex_t *v3) {
    point_t p1, p2, p3, c1, c2, c3;
    int render_state = device->render_state;

    // 按照 Transform 变化，到达裁剪空间
    matrix_apply(&c1, &v1->pos, &(device->transform.transform));
    matrix_apply(&c2, &v2->pos, &(device->transform.transform));
    matrix_apply(&c3, &v3->pos, &(device->transform.transform));

    // 归一化，得到屏幕坐标
    transform_homogenize(&device->transform, &p1, &c1);
    transform_homogenize(&device->transform, &p2, &c2);
    transform_homogenize(&device->transform, &p3, &c3);

    if (render_state & RENDER_STATE_WIREFRAME) {		// 线框绘制
        device_draw_line(device, (int)p1.x, (int)p1.y, (int)p2.x, (int)p2.y, device->foreground);
        device_draw_line(device, (int)p1.x, (int)p1.y, (int)p3.x, (int)p3.y, device->foreground);
        device_draw_line(device, (int)p3.x, (int)p3.y, (int)p2.x, (int)p2.y, device->foreground);
    }
}

void draw_plane(device_t *device, int a, int b, int c, int d) {
    vertex_t p1 = mesh[a], p2 = mesh[b], p3 = mesh[c], p4 = mesh[d];
    device_draw_primitive(device, &p1, &p2, &p3);
    device_draw_primitive(device, &p3, &p4, &p1);
}

void Draw_Box(device_t *device, float theta) {
    matrix_t m;
    matrix_set_rotate(&m, -1, -0.5, 1, theta);
    device->transform.world = m;
    transform_update(&device->transform);
    draw_plane(device, 0, 1, 2, 3);
    draw_plane(device, 7, 6, 5, 4);
    draw_plane(device, 0, 4, 5, 1);
    draw_plane(device, 1, 5, 6, 2);
    draw_plane(device, 2, 6, 7, 3);
    draw_plane(device, 3, 7, 4, 0);
}

int main(void) {
    device_t device; //定义渲染方块结构体，包括背景颜色，框大小

    const TCHAR *Title = _T("Soft Render implement");
    screen_w = 800;
    screen_h = 600;
    float camera_pos = 3.0f;
    float camera_theta = 1.0f;
    int res = screen_init(screen_w, screen_h, Title); //初始化窗口
    if (!res) {
        //ok maybe dosth to celebrate successful?
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

        screen_dispatch();//分发所有命令给回调函数去处理：主要处理按键输入
        device_InitFB(&device);

        camera_set_pos(&device, 0, camera_pos, 0);

        //画线，画点测试
        //for (int i = 0; i<=50; i++) {
        //    DrawPoint(&device, 400-i, 300-i, ((200+i) << 16) | (200 + i) | ((200 + i) << 8));
        //    printf("i=%d\n",i);
        //}

        Draw_Box(&device, camera_theta);

        screen_update();
    }

    return 0;
}