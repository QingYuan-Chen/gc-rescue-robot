#include "wheel_drive.h"

#include "bsp/encoder.h"
#include "bsp/motor.h"
#include "config/wheel_config.h"

#include "FreeRTOS.h"
#include "task.h"

typedef struct
{
    int32_t duty_permille[WHEEL_COUNT];
    uint32_t duration_ms;
} wheel_request_t;

static wheel_request_t pending_request;
static bool request_pending;
static wheel_request_t active_request;
static uint32_t started_tick_ms;
static wheel_drive_status_t snapshot;
static wheel_drive_status_t current;

bool wheel_drive_init(void)
{
    return motor_init() && encoder_init();
}

bool wheel_drive_request(int32_t duty_left, int32_t duty_right, uint32_t duration_ms)
{
    if (duty_left < -WHEEL_MANUAL_MAX_DUTY_PERMILLE || duty_left > WHEEL_MANUAL_MAX_DUTY_PERMILLE ||
        duty_right < -WHEEL_MANUAL_MAX_DUTY_PERMILLE ||
        duty_right > WHEEL_MANUAL_MAX_DUTY_PERMILLE || duration_ms < WHEEL_PERIOD_MS ||
        duration_ms > WHEEL_MANUAL_MAX_DURATION_MS || duration_ms % WHEEL_PERIOD_MS != 0U)
    {
        return false;
    }
    taskENTER_CRITICAL();
    pending_request = (wheel_request_t){{duty_left, duty_right}, duration_ms};
    request_pending = true;
    taskEXIT_CRITICAL();
    return true;
}

void wheel_drive_stop(void)
{
    taskENTER_CRITICAL();
    pending_request = (wheel_request_t){0};
    request_pending = true;
    taskEXIT_CRITICAL();
}

void wheel_drive_get_status(wheel_drive_status_t *status)
{
    taskENTER_CRITICAL();
    *status = snapshot;
    taskEXIT_CRITICAL();
}

void wheel_drive_step(uint32_t tick_ms)
{
    taskENTER_CRITICAL();
    if (request_pending)
    {
        active_request = pending_request;
        request_pending = false;
        started_tick_ms = tick_ms;
    }
    taskEXIT_CRITICAL();

    const uint32_t elapsed_ms = tick_ms - started_tick_ms;
    if (elapsed_ms >= active_request.duration_ms)
    {
        active_request = (wheel_request_t){0};
    }
    current.remaining_ms =
        active_request.duration_ms == 0U ? 0U : active_request.duration_ms - elapsed_ms;
    current.sample_tick_ms = tick_ms;
    for (wheel_id_t wheel = WHEEL_LEFT; wheel < WHEEL_COUNT; wheel++)
    {
        current.delta_counts[wheel] = encoder_read_delta_counts(wheel, &current.raw_counts[wheel]);
        current.total_counts[wheel] += current.delta_counts[wheel];
        int32_t requested = current.remaining_ms == 0U ? 0 : active_request.duty_permille[wheel];
        if (requested * current.duty_permille[wheel] < 0)
        {
            /* 换向先保持一个完整周期的零输出，再接入相反方向。 */
            requested = 0;
        }
        motor_set_duty_permille(wheel, requested);
        current.duty_permille[wheel] = requested;
    }
    taskENTER_CRITICAL();
    snapshot = current;
    taskEXIT_CRITICAL();
}
