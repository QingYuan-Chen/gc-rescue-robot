#ifndef __PID_H
#define __PID_H

struct I_pid_obj {
	float output;					//输出
	float bias;							//误差
	float last_bias;				//上次误差
	float last2_bias;				
	float measure;					//测量值
	float target;						//目标值
};

struct P_pid_obj {
	float output;
	float bias;
	float measure;
	float last_bias;
	float integral;						//积分
	float last_differential;	//上一次微分
	float target;
};

//differential_filterK: 微分项滤波系数，取值范围(0,1]
//系数越小滤波效应越大，当系数为1时不进行滤波
struct PID_param {
	float kp;
	float ki;
	float kd;
	float differential_filterK;			//滤波系数
	float outputMin;
	float outputMax;
};

void incremental_PID (struct I_pid_obj *motor, struct PID_param *pid);
float positional_PID (struct P_pid_obj *obj, struct PID_param *pid);

#endif
