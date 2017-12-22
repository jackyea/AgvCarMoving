#ifdef USE_XENO
#define eprintf(...)
#else
#define eprintf(...) printf (__VA_ARGS__)
#endif

#include <stdio.h>
#include<unistd.h>
#include<stdlib.h>
#include "canfestival.h"


void	init_sigaction(void);
void	init_time(void);
//void Send2Upper(int signo);
void Send2Upper();
void *RfidThreadFunc(void * arg);
void *BbbExecuteThreadFunc(void * arg);
void RecvUpperFunc();

void updatesenser();
void *stopThreadFunc(void *arg);

void TimerFuncDispatch(int signo);

/*
//故障报警
#define ERROR_STATE false //当出现故障时，将此标志位置位，主循环每次先检测这个标志位，做出相应动作。

//通信交互
#define  COMMUNICATION_STATE false //通信状态指示
#define MESSAGE_RECEIVE false //新消息状态指示
extern char* MessageReceive;//通信子程序运行时自动接收到的消息存在这里，当主循环检测到MESSAGE_RECEIVE状态变化时，就会到这里读取信息。
#define MESSAGE_SEND false //客户端上报消息状态指示
extern char* MessageSend;//通信子程序每次循环时先检测MESSAGE_SEND，如果被置位，就会到这里读取信息，发给上位机。

//电机交互
extern float StraightMotorEncoder[];//四个直行电机编码器反馈的信号
extern float RotateMotorEncoder[];//四个旋转电机编码器反馈的信号
extern float LiftMotorEncoder;//一个举升电机编码器反馈的信号
extern float PreStraightMotorEncoder[];//四个直行电机的预设信号，即三段速规划出的电机速度，要和编码器反馈的速度进行比较，看是否符合预期。
extern float PreRotateMotorEncoder[];//四个旋转电机编码器的预设信号
extern float PreLiftMotorEncoder;//一个举升电机编码器的预设信号
extern float NavigationDiff;//导航传感器给出的路径偏移量
extern float RfidNear;//Rfid地址标签给出的接近或到达信号

//电源管理，1、检测电压电流由主程序判断是否该充电 2、主程序只需循环检查电源管理系统的低电报警信号
extern float BattaryVoltage;//主循环每次检查一次电压值，与低电报警值与比较
#define BATTARY_ALARM_VOLTAGE 40;//低电报警电压值
#define BATTARY_ALARM_COUNTER 5;//为排除干扰，当主循环连续5次都检测到低于BATTARYALARMVOLTAGE时，才真正触发报警信号
#define BATTARY_STATE false;//电池低电标志

//防撞管理
#define BLOCK_STATE false;//防撞标志
extern float BlockMessage;//防撞传感器传来的信息
*/


