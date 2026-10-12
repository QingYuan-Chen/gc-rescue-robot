#include "pid.h"

// 增量式 PID：按误差方向抑制继续饱和的积分增量，输出限幅由调用方执行。
// 公式未显式乘除采样时间，迁移时须同时保留调用周期和输入输出尺度。
void incremental_PID (struct I_pid_obj *motor, struct PID_param *pid)
{
	float proportion = 0, integral = 0, differential = 0;
	
	motor->bias = motor->target - motor->measure;
	
	proportion = motor->bias - motor->last_bias;
	
	// 已越过输出边界时，仅允许有助于退出饱和的积分增量。
	if (motor->output > pid->outputMax)
	{
		if (motor->bias < 0)
			integral = motor->bias;
	}
	else if (motor->output < -pid->outputMax)
	{
		if (motor->bias > 0)
			integral = motor->bias;
	}
	else
	{
		integral = motor->bias;
	}
	
	differential = (motor->bias - 2 * motor->last_bias + motor->last2_bias);
	
	motor->output += pid->kp*proportion + pid->ki*integral + pid->kd*differential;
	
	motor->last2_bias = motor->last_bias;
	motor->last_bias = motor->bias;
}

// 位置式 PID：条件积分、微分低通滤波及输出限幅。
float positional_PID (struct P_pid_obj *obj, struct PID_param *pid)
{
	float differential = 0;
	
	obj->bias = obj->target - obj->measure;
	
	if (obj->output >= pid->outputMax)
	{
		if (obj->bias < 0)
			obj->integral += obj->bias;
	}
	else if (obj->output <= pid->outputMin)
	{
		if (obj->bias > 0)
			obj->integral += obj->bias;
	}
	else
	{
		obj->integral += obj->bias;
	}
	
	
	// 对本次误差差分与上次微分状态加权，沿用原滤波系数。
	differential = (obj->bias - obj->last_bias) * pid->differential_filterK + 
					(1 - pid->differential_filterK) * obj->last_differential;
	
	obj->output = pid->kp * obj->bias + pid->ki * obj->integral + pid->kd * differential;
	
	if(obj->output>pid->outputMax){
		obj->output = pid->outputMax;
	}
	else if(obj->output<pid->outputMin){
		obj->output = pid->outputMin;
	}
	obj->last_bias = obj->bias;
	obj->last_differential = differential;
	
	
	return obj->output;
}

