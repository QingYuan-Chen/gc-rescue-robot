#ifndef ROBOT_APP_H
#define ROBOT_APP_H

/* GPIO 初始化之后、调度器启动之前调用一次。 */
void robot_app_init(void);

/* 由生成的任务入口调用，接入当前阶段的运行模块。 */
void robot_app_run(void);

#endif
