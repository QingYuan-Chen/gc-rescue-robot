#ifndef __PID_H
#define __PID_H

// 增量式 PID 状态；目标值与反馈值须采用相同单位。
struct I_pid_obj {
	float output;					// 累计输出，单位由调用方确定
	float bias;							// 本次误差
	float last_bias;				// 上次误差
	float last2_bias;				// 上上次误差
	float measure;					// 测量值，与目标值使用相同尺度
	float target;						// 目标值
};

// 位置式 PID 状态；积分与差分均按调用次数更新。
struct P_pid_obj {
	float output;
	float bias;
	float measure;
	float last_bias;
	float integral;						// 逐次累加的误差，未乘采样时间
	float last_differential;	// 上次滤波后的误差差分
	float target;
};

// 离散 PID 参数沿用原调用周期和输入输出尺度。
// differential_filterK 的约定范围为 (0, 1]，越小滤波越强；为 1 时使用本次差分。
struct PID_param {
	float kp;
	float ki;
	float kd;
	float differential_filterK;			// 位置式 PID 的微分滤波系数
	float outputMin; // 位置式 PID 的输出下限
	float outputMax; // 位置式 PID 上限，同时作为增量式 PID 的对称饱和阈值
};

void incremental_PID (struct I_pid_obj *motor, struct PID_param *pid);
float positional_PID (struct P_pid_obj *obj, struct PID_param *pid);

#endif
