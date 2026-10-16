#ifndef ROBOT_VOFA_STREAM_H
#define ROBOT_VOFA_STREAM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define VOFA_SAMPLE_CAPACITY 32U
#define VOFA_LINE_CAPACITY 96U

typedef struct
{
    bool enabled;
    uint32_t queued;
    uint32_t captured;
    uint32_t dropped;
    uint32_t sent;
    uint32_t tx_errors;
} vofa_stream_status_t;

/* 以下函数只在任务上下文调用；采集开关不提交任何运动或参数请求。 */
void vofa_stream_start(uint32_t tick_ms);
void vofa_stream_stop(void);
void vofa_stream_get_status(vofa_stream_status_t *status);

/* 装配层在车轮周期完成后调用，只复制快照，不格式化或等待发送。 */
void vofa_stream_capture(void);

/* 调试任务取一帧并格式化；缓冲区至少 VOFA_LINE_CAPACITY 字节。
 * 返回 0 表示无数据；发送完成后调用 record_tx 记录实际发送结果。
 */
size_t vofa_stream_format_next(char *line, size_t capacity);
void vofa_stream_record_tx(bool success);

#endif
