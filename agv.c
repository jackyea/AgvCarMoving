#include<signal.h>
#include<sys/time.h>
#include<sched.h>
#include<pthread.h>
#include<string.h>
#include<sys/socket.h>
#include <stddef.h>		/* for NULL */
#include"agv.h"
#include"Log.h"
#include "InitAndSelfTest.h"
#include "ExchangingInfoStruct.h"
#include"RfidClient.h"
#include"serverconnect.h"
#include"CANOpenShellMasterOD.h"
#include"MotorControl.h"
#include "CanOpenShell.h"  //引用 extern CO_Data* CANOpenShellOD_Data;
#include "HandMotorControl.h"


pthread_t RfidThreadId,BbbExecuteThreadId,stopThreadId;//新线程的Id
//extern pthread_mutex_t mut;
char* hostIP="192.168.1.101";//上位机IP地址和端口
int port=3490;
int sockfd;
int timertimer=0;//定时器的计数
char sendbuf[100],recvbuf[100];
extern struct StructAgvReport agvreportstruct;
extern struct StructInner innerstruct;

//main函数运行过程：1、连接上位机，2、连通后起rfid线程，3、初始化Can总线,检查电机、传感器是否正确，4、起执行命令线程，5、起定时器，相当于打开向上位机传输结构体线程，6、主程序中运行RecvUpperFunc()，接收上位机命令
int main()
{
	Log("\n\n\n******************程序启动******************"); //防止Agv.c中找不到开头
	sockfd=serverconnect(hostIP,port);
	if(sockfd==-1) //连接服务器
	{
		printf("与上位机建立连接失败\n");
		exit(1);
	}
	Log("与上位机通信线程已连接服务器!");
	printf("与上位机通信线程已连接服务器!\n");

	//pthread_mutex_init(&mut,NULL);//锁的初始化
	int ret=0;
	ret=pthread_create(&RfidThreadId,NULL,RfidThreadFunc,NULL);//启动Rfid线程，接收四个rfid读卡器的数据，并写入了agvreportstruct中。
	if(ret)
	{
		printf("**************************************************RfidThread error\n");
		Log("agv Rfid线程错误");
		return 0;
	}


	if(InitRov()==-1)//初始化
	{
		printf("程序初始化失败！\n");
		Log("程序初始化失败！\n");
		return 0;
	}

	ret=pthread_create(&BbbExecuteThreadId,NULL,BbbExecuteThreadFunc,NULL);//这个线程读取bbb接收到的命令，然后执行，最后把执行结果通知bbb。
	if(ret)
	{
		printf("**************************************************BbbExecuteThread error\n");
		Log("agv命令执行线程错误");
		return 0;
	}

	init_sigaction();
	init_time();
	RecvUpperFunc();
	canClose(CANOpenShellOD_Data);
	return 0;
}

void init_sigaction(void)//信号量初始化函数，它指向了用户函数，当此信号量得到定时器传给的信息后，会调用用户函数。
{
	struct sigaction act;
	//act.sa_handler=Send2Upper;
	act.sa_handler=TimerFuncDispatch;
	act.sa_flags=0;
	sigemptyset(&act.sa_mask);
	sigaction(SIGALRM,&act,NULL);
}

void init_time(void)//定时器函数，被调用后不断执行。并在it_interval到期后通知sigaction。
{

	struct itimerval val;//结构体itimerval，里面有两个数据，it_value和it_interval,这两个数据又是timeval结构体
	val.it_value.tv_sec=0;
	val.it_value.tv_usec=20000;
	val.it_interval.tv_sec=0;
	val.it_interval.tv_usec=20000;
	setitimer(ITIMER_REAL,&val,NULL);/*linux内部有三种定时器类型，分别是ITIMER_REAL：实时定时器，不管进程在何种模式下运行（甚至在进程被挂起时），它总在计数。定时到达，向进程发送SIGALRM信号。
	ITIMER_VIRTUAL：这个不是实时定时器，当进程在用户模式（即程序执行时）计算进程执行的时间。定时到达后向该进程发送SIGVTALRM信号。	ITIMER_PROF：进程在用户模式（即程序执行时）和核心模式（即进程调度用时）均计数。定时到达产生SIGPROF信号。ITIMER_PROF记录的时间比ITIMER_VIRTUAL多了进程调度所花的时间*/

}
void TimerFuncDispatch(int signo)
{
	timertimer++;
	if(timertimer==10&&innerstruct.AdjustRouteFlag==1)
	{
		//调用纠偏函数
	}
	if(timertimer==10)
	{
		Send2Upper();
		timertimer=0;
	}
}
//void Send2Upper(int signo)
void Send2Upper()
{
	updatesenser();
	//发送agv状态结构体的内容到上位机**************************************//
	if(agvreportstruct.RWFlag==1)
	{
		memset(sendbuf,0,sizeof(sendbuf));
		//pthread_mutex_lock(&mut);
		memcpy(sendbuf,&agvreportstruct,sizeof(struct StructAgvReport));
		//printf("state is %d\n",agvreportstruct.ExecuteCommand);
		//pthread_mutex_unlock(&mut);
		int sendbytes=send(sockfd,sendbuf,sizeof(struct StructAgvReport),0);
		if(sendbytes==-1)
		{
			Log("bbb向上位机发送命令失败");
		}
	}

	//*********************************************************************//
//从can，rfid中读取各种传感器数据，写入agvreportstruct中,这样可以避免读结构体和写结构体时的冲突。



	//*********************************************************************//

}

//rfid读取子线程，开机先启动此线程，调用Rfid4Client()函数，让其while循环，读到数据后将rfid数据写入agvreportstruct中，供其它程序使用。
void *RfidThreadFunc(void * arg)
{
	if(Rfid4Client()==-1)
	{
		printf("未能与Rfid建立正确连接\n");
		Log("Rfid 未能建立连接");
		agvreportstruct.RfidError=1;
		pthread_join(RfidThreadId,NULL);
	}
	return 0;
}

//执行命令子线程，开机先启动此线程，
void *BbbExecuteThreadFunc(void * arg)
{
	while(1)
	{
		if(innerstruct.RWflag==1)
		{
			char * commandcopy=innerstruct.Command;
			innerstruct.RWflag=0;//将命令取出后，把标志位清空，使通信程序可以写新命令进去。

			//pthread_mutex_lock(&mut);
			agvreportstruct.ExecuteCommand=1;//取出命令后，将命令执行标志位置1,表示命令正在执行
			//pthread_mutex_unlock(&mut);
			//printf("agvreportstruct.ExecuteCommand==%d\n",agvreportstruct.ExecuteCommand);

			int ret=ExecuteCommand(commandcopy);//将命令传入执行程序

			if(ret<0)//如果命令执行未正常结束
			{
				agvreportstruct.ExecuteCommand=-1;
				Log("命令执行出现错误！");
			}
			else if(ret==0)//如果命令成功执行完成，
			{
				//printf("ret is %d \n",ret);
				agvreportstruct.ExecuteCommand=0;//将执行命令标志位清空，表示可以下发新命令。
				//printf("agvreportstruct.ExecuteCommand is %d\n",agvreportstruct.ExecuteCommand);
			}
		}
	}
	return 0;
}

/*遥控器*/
int Remoter[13]={0};

int Flag_Remoter=0;
//bbb与上位机通信线程
void RecvUpperFunc()
{
	fd_set sockfdset;

	struct timeval tv;//select函数的超时参数

	int isconnect=0;//与上位机是否连接指示

//	while(1)								//此函数while不执行后，将退出整个系统，所以其实其他函数中判断MotorError后的打印都没有必要了，
	while(agvreportstruct.MotorError==0)  //遥控器模式行走无法判断EMCY，使用agvreportstruct.MotorError==0判断EMCY是否可行？？若EMCY 将切断接收上位机指令，此时还能否上报？？
	{
		/*用于检测是否要进入遥控模式，剩余的在遥控模式中进行，减小对上位机模式的影响*/
		Remoter[6] =(IN6_Remoter_bit9_16 & (1<<0)) ? 1 : 0;
		/*遥控器模式*/
		if(Remoter[6]==1) //遥控器1+2同时按下3S，准备进入遥控模式 Remoter[1] && Remoter[2]
		{
			int delaycount=0;
			while(delaycount<=100000000)
			{
				delaycount++;
			} //100000000大概1.2s左右；300000000=4.5s左右
			Remoter[6] =(IN6_Remoter_bit9_16 & (1<<0)) ? 1 : 0;
			if(Remoter[6]) //再次判断IO站电位，1&&2=1超过3S进入遥控模式 Remoter[1] && Remoter[2]
			{
				//绿灯取反=bit2--进入熄灭  退出点亮
				ReverseBit(2);
				//黄灯取反=bit3,--进入点亮，退出熄灭
				ReverseBit(3);
				//根据黄灯bit3给予Flag_Remoter赋值，决定是否触发遥控模式；黄灯亮=遥控模式；这样使通过灯来判断是否进入遥控模式更加准确。
				Flag_Remoter=(OUT8_Relay_bit1_8 & (1<<3)) ? 1 : 0;
				sendPDOevent(CANOpenShellOD_Data);
				//向上位机上报操作模式
				agvreportstruct.RemoteControl=Flag_Remoter; //向上位机上报操作模式 0=上位机模式，1=遥控器模式
				if (Flag_Remoter==0)
					{
						M1_TPDO_mode_of_operation=0x01; //上位机01位置模式
						sendPDOevent(CANOpenShellOD_Data);
						HandAgvStopVelocity();//重新进入上位机模式，去除使能
						Log("AGV退出遥控器模式,已去除使能");
						eprintf("AGV退出遥控器模式,已去除使能");
						fflush(stdout);
					}
				if (Flag_Remoter==1)
					{
						M1_TPDO_mode_of_operation=0x03; //遥控器03速度模式
						sendPDOevent(CANOpenShellOD_Data);
						Log("AGV进入遥控器模式");
						eprintf("AGV进入遥控器模式");
						fflush(stdout);
					}
			}
		}

		/* 进入遥控器模式
		 * 行走电机：按下遥控器触发电机，松开遥控器停止电机
		 * 举升+转向电机（位置模式）：按下遥控器，执行整个命令，即使松开遥控器也继续执行，并判断完成后才返回！*/
		if(Flag_Remoter==1)//遥控器标志位 =1 进入遥控器 Flag_Remoter
		{
			Remoter[1] =(IN6_Remoter_bit9_16 & (1<<5)) ? 1 : 0; //判断bit5=1，则Remoter1_Forward=1；若bit0=0，则Remoter1_Forward=0
			Remoter[2] =(IN6_Remoter_bit9_16 & (1<<4)) ? 1 : 0;
			Remoter[3] =(IN6_Remoter_bit9_16 & (1<<3)) ? 1 : 0;
			Remoter[4] =(IN6_Remoter_bit9_16 & (1<<2)) ? 1 : 0;
			Remoter[5] =(IN6_Remoter_bit9_16 & (1<<1)) ? 1 : 0;
//			Remoter[6] =(IN6_Remoter_bit9_16 & (1<<0)) ? 1 : 0;
			Remoter[7] =(IN6_Remoter_bit9_16 & (1<<6)) ? 1 : 0;
			Remoter[8] =(IN6_Remoter_bit9_16 & (1<<7)) ? 1 : 0;
			Remoter[10] =(IN5_Limit_Switch_bit9_10 & (1<<7)) ? 1 : 0;
			Remoter[11] =(IN5_Limit_Switch_bit9_10 & (1<<6)) ? 1 : 0;
			Remoter[12] =(IN5_Limit_Switch_bit9_10 & (1<<5)) ? 1 : 0;
			//遥控器主程序
			//1.首先检测遥控器所有直行电机(行进后退左移右移，左旋，右旋)，若全=0，则表示松开遥控器，向所有电机发送停止命令
			if(!(Remoter[1] || Remoter[2] || Remoter[3] || Remoter[4]||Remoter[7]||Remoter[8]))
			{
				//向所有行走电机发送停止命令
				M1_TPDO_control_word=WalkStop;
				sendPDOevent(CANOpenShellOD_Data);
			} //end 停止命令

			int remoter_count=0;
			int i=1;
			for (i=1;i<=12;i++) //判断几个按键同时被按下
			{
				remoter_count+=Remoter[i];
			}

			if (remoter_count==1) //判断 有 且只有 1个按键触发
			{
			/*2.然后开始判断各个IO站遥控器输出，执行具体命令，并启动电机（行走电机的停止命令在上一个if执行）*/
			if		(Remoter[1]) //前进
				{
					if (agvreportstruct.WheelDirection == 0)
					{
						if(HandAgvFoward())
						{ //出错！
							//HandAgvErrorStop();
							AgvEmcyStop();
							printf("遥控器-AGV直行出错");
							Log("遥控器-AGV直行出错");
							break; //跳出while循环
						}
					}
				}
			else if (Remoter[2]) //后退
				{
					if (agvreportstruct.WheelDirection == 0)
					{
						if(HandAgvBack())
						{ //出错！
							//HandAgvErrorStop();
							AgvEmcyStop();
							Log("遥控器-AGV后退出错");
							printf("遥控器-AGV后退出错\n");
							break;
						}
					}
				}
			else if (Remoter[3]) //左移
				{
					if (agvreportstruct.WheelDirection == 1)
					{
						if(HandAgvLeft())
						{ //出错！
							//HandAgvErrorStop();
							AgvEmcyStop();
							Log("遥控器-AGV左横移出错");
							printf("遥控器-AGV左横移出错\n");
							break;
						}
					}
				}
			else if (Remoter[4])//右移
				{
					if (agvreportstruct.WheelDirection == 1)
					{
						if(HandAgvRight())
						{ //出错！
							//HandAgvErrorStop();
							AgvEmcyStop();
							Log("遥控器-AGV右横移出错");
							printf("遥控器-AGV右横移出错\n");
							break;
						}
					}
				}
			else if (Remoter[5]) //举升+下降
				{
					if(HandAgvUpOrDown()==-1)
					{
						//HandAgvErrorStop();
						AgvEmcyStop();
						break;
					}
				}

//			else if (Remoter[6]) //模式切换
//				{
//					if(HandAgvDown()==-1)
//					{
//						HandAgvErrorStop();
//						Log("遥控器-下降时限位开关触发");
//						break;
//					}
//					Log("遥控器-下降完成");
//				}
			else if (Remoter[7])//左原地旋转
				{
					if (agvreportstruct.WheelDirection == 2)
					{
						if(HandAgvRotateLeft())
						{ //出错！
							//HandAgvErrorStop();
							AgvEmcyStop();
							Log("遥控器-AGV左原地旋转出错");
							printf("遥控器-AGV左原地旋转出错\n");
							break;
						}
					}
				}
			else if (Remoter[8])//右原地旋转
				{
					if (agvreportstruct.WheelDirection == 2)
					{
						if(HandAgvRotateRight())
						{ 	//出错！
							//HandAgvErrorStop();
							AgvEmcyStop();
							Log("遥控器-AGV右原地旋转出错");
							printf("遥控器-AGV右原地旋转出错\n");
							break;
						}
					}
				}
			else if (Remoter[10]) //轮子横移位置
				{
					if(HandRotatorWheelCrab()==-1)
					{
						//HandAgvErrorStop();
						AgvEmcyStop();
						Log("遥控器-至横移位置时出错！");
						printf("遥控器-至横移位置时出错！\n");
						break;
					}
					Log("遥控器-到达横移位置");
					printf("遥控器-到达横移位置\n");
				}
			else if (Remoter[11]) //轮子自旋位置
				{
					if(HandRotateWheelRotate()==-1)
					{
						//HandAgvErrorStop();
						AgvEmcyStop();
						Log("遥控器-至自旋位置时出错！");
						printf("遥控器-至自旋位置时出错！\n");
						break;
					}
					Log("遥控器-到达自旋位置");
				}
			else if (Remoter[12])//轮子零位置
				{
					if(HandRotateWheelZero()==-1)
					{
						//HandAgvErrorStop();
						AgvEmcyStop();
						Log("遥控器-至0位置时出错！");
						printf("遥控器-至0位置时出错！\n");
						break;
					}
					Log("遥控器-到达零位置");
					printf("遥控器-到达零位置\n");
				}
			}//end 只有一个按键触发
		} //end 遥控器具体指令



		/*进入上位机模式*/
		else
		{
		agvreportstruct.RemoteControl=0; //向上位机上报操作模式 0=上位机模式，1=遥控器模式
		isconnect++;
		if(isconnect>100000)//每隔一定时间检测一次连接情况
		{
			if(IsConnect(sockfd))//如果返回1,说明未断开
			{
				isconnect=0;
			}
			else//如果返回0,说明连接断开
			{
//				Log("与上位机连接断开，正在重连");
//				printf("与上位机连接断开，正在重连");
//				//这里应该用settimer把定时器停掉，不让它再向上位机发东西，不过继续发也没关系，因为已经断掉了，不会影响上位机，而发送失败只是Log一下，也没有其它处理，所以也不会有问题
//				sockfd=serverconnect(hostIP,port);
//				Log("与上位机重连成功");
//				printf("与上位机重连成功");
//				isconnect=0;
				return;
			}

		}
		//接收信息*********************************************************************//
		FD_ZERO(&sockfdset);//清空描述符集 //注释掉会出问题吗？？？？

		FD_SET(sockfd,&sockfdset);//添加连接到描述符集中

		//超时设定,将超时设置为0,是不阻塞模式，如果没有数据，立即返回。
		tv.tv_sec=0;
		tv.tv_usec=0;

		int ret=select(sockfd+1,&sockfdset,NULL,NULL,&tv);//select函数
		if(ret>0)
		{
			memset(recvbuf,0,sizeof(recvbuf));
			int rbytes=recv(sockfd,recvbuf,sizeof(recvbuf),0);
			if(rbytes>1)//接收服务器发来的信息
			{
				if(recvbuf[0]=='A'&&recvbuf[1]=='A'&&recvbuf[rbytes-2]=='E'&&recvbuf[rbytes-1]=='E')//如果命令以AA开头，以EE结尾，说明命令正确
				{
					if(recvbuf[2]=='s'&&recvbuf[3]=='t'&&recvbuf[4]=='o'&&recvbuf[5]=='p')//如果接收到电机停止命令
					{
						innerstruct.StopCommand=1;//给这个标志位置1,前行、后退、左移、右移程序在检测到此标志位为1后，将调用AgvStop(),使小车停止
					}
					else if(recvbuf[2]=='s'&&recvbuf[3]=='l'&&recvbuf[4]=='o'&&recvbuf[5]=='w')//如果接收到电机减速命令
					{
						innerstruct.SlowCommand=1;//给这个标志位置1,前行、后退、左移、右移程序在检测到此标志位为1后，使小车减速
					}
					else if(innerstruct.RWflag==0)//如果上一条命令已经被取走，将新命令写入结构体
					{
						innerstruct.Command=recvbuf;
						innerstruct.RWflag=1;//写入结构体后将标志位置1,让命令执行程序去读
					}
					printf("%s \n",recvbuf);//将命令打印出来

				}
			}
		}
		}//end 上位机程序
	} //end while
	printf("我是RecvUpperFunc函数，收到EMCY，程序退出\n");
	Log("我是RecvUpperFunc函数，收到EMCY，程序退出");
	AgvEmcyStop();
	return;
}

void updatesenser()
{
	agvreportstruct.MotorVel[0]=R1_speed;
	agvreportstruct.MotorVel[1]=R2_speed;
	agvreportstruct.MotorVel[2]=R3_speed;
	agvreportstruct.MotorVel[3]=R4_speed;

	agvreportstruct.R5position=R5_position;
	agvreportstruct.R6position=R6_position;
	agvreportstruct.R7position=R7_position;
	agvreportstruct.R8position=R8_position;

	agvreportstruct.R1position=R1_position;
	agvreportstruct.R2position=R2_position;
	agvreportstruct.R3position=R3_position;
	agvreportstruct.R4position=R4_position;

	/*HitAvoidSenser[0] = 0:无区域触发  0x1：内层触发  0x2：外层触发 0x3：内外层都触发  设备定义的最外层不用 只用中间层和内层*/
	agvreportstruct.HitAvoidSenser[0]=((IN6_DLS1_4_bit1_8>>0) & 3);  //((IN6_DLS1_4_bit1_8>>0) & 3);
	agvreportstruct.HitAvoidSenser[1]=((IN6_DLS1_4_bit1_8>>2) & 3);  //((IN6_DLS1_4_bit1_8>>2) & 3);
	agvreportstruct.HitAvoidSenser[2]=((IN6_DLS1_4_bit1_8>>4) & 3); //((IN6_DLS1_4_bit1_8>>4) & 3);
	agvreportstruct.HitAvoidSenser[3]=((IN6_DLS1_4_bit1_8>>6) & 3); //((IN6_DLS1_4_bit1_8>>6) & 3);

	/*磁导航拼接*/
	agvreportstruct.NavigationSenser[0]=((IN1_NAVI1_bit9_16<<8)|IN1_NAVI1_bit1_8); //((IN1_NAVI1_bit9_16<<8)|IN1_NAVI1_bit1_8);
	agvreportstruct.NavigationSenser[1]=((IN2_NAVI2_bit9_16<<8)|IN2_NAVI2_bit1_8); //((IN2_NAVI2_bit9_16<<8)|IN2_NAVI2_bit1_8);
	agvreportstruct.NavigationSenser[2]=((IN3_NAVI3_bit9_16<<8)|IN3_NAVI3_bit1_8); //((IN3_NAVI3_bit9_16<<8)|IN3_NAVI3_bit1_8);
	agvreportstruct.NavigationSenser[3]=((IN4_NAVI4_bit9_16<<8)|IN4_NAVI4_bit1_8); //((IN4_NAVI4_bit9_16<<8)|IN4_NAVI4_bit1_8);
}
