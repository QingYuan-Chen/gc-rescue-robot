#ifndef TEST_FREERTOS_H
#define TEST_FREERTOS_H
void test_enter(void);
void test_exit(void);
#define taskENTER_CRITICAL() test_enter()
#define taskEXIT_CRITICAL() test_exit()
#endif
