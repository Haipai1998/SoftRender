int main(void) {
    device_t device; //������Ⱦ����ṹ�壬����������ɫ�����С

    screen_init(); //��ʼ������

    device_init();// ��ʼ���豸

    while (true) {
        screen_dispatch();//�ַ�����������ص�����ȥ����

    }

    return 0;
}