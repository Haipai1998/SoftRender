int main(void) {
    device_t device; //定义渲染方块结构体，包括背景颜色，框大小

    screen_init(); //初始化窗口

    device_init();// 初始化设备

    while (true) {
        screen_dispatch();//分发所有命令给回调函数去处理

    }

    return 0;
}