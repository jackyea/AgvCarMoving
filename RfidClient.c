/*
 * RfidClient.c
 *
 *  Created on: 2017-7-5
 *      Author: yenb
 */
#include<errno.h>
#include<stdio.h>
#include<string.h>
#include<sys/types.h>
#include<netinet/in.h>
#include<sys/socket.h>
#include<netdb.h>
#include<linux/tcp.h>
#include"serverconnect.h"
#include "ExchangingInfoStruct.h"

#define MAXDATASIZE 128 //listen的请求接收队列长度
extern struct StructAgvReport agvreportstruct;
int Rfid4Client()
{
	int sockfd[4];
	int maxsockfd; //所有sockfd中的最大值
	struct timeval tv; //select函数的超时参数
	fd_set sockfdset; //select的关键参数，里面存储的是sockfd[4]内容
	char buf[MAXDATASIZE]; //接收缓冲区
	memset(buf,0,sizeof(buf));

	char startbuf[MAXDATASIZE] = "\2sMN mTCgateon\3"; //send start RFID
	char ackbuf[MAXDATASIZE] = "\2sAN mTCgateon 1\3"; //ACK after RFID received start
	//char ackFailedbuf[MAXDATASIZE] = "\2sAN mTCgateon 0\3"; //ACK after RFID received start
	int countrfid = 0;

	//获得四个rfid读卡器的连接描述符,同时将sockfd中的最大值赋给maxsockfd;
	//暂不考虑sockfd相同的情况
	if ((sockfd[0] = serverconnect("192.168.1.11", 2112)) < 0) {
		printf("连接rfid1失败\n");
		return -1;
	}
	maxsockfd = sockfd[0];

	if ((sockfd[1] = serverconnect("192.168.1.12", 2112)) < 0) {
		printf("连接rfid2失败\n");
		return -1;
	}

	if (maxsockfd < sockfd[1]) {
		maxsockfd = sockfd[1];
	}

	if ((sockfd[2] = serverconnect("192.168.1.13", 2112)) < 0) {
		printf("连接rfid3失败\n");
		return -1;
	}
	if (maxsockfd < sockfd[2]) {
		maxsockfd = sockfd[2];
	}

	if ((sockfd[3] = serverconnect("192.168.1.14", 2112)) < 0) {
		printf("连接rfid4失败\n");
		return -1;
	}
	if (maxsockfd < sockfd[3]) {
		maxsockfd = sockfd[3];
	}

	/*初始化RFID，使其进入读取状态*/
	send(sockfd[0], startbuf, strlen(startbuf), 0);
	send(sockfd[1], startbuf, strlen(startbuf), 0);
	send(sockfd[2], startbuf, strlen(startbuf), 0);
	send(sockfd[3], startbuf, strlen(startbuf), 0);
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	while (1)
	{
		FD_ZERO(&sockfdset);	//清空描述符集

		/*清空buf数组*/
		memset(buf,0,sizeof(buf));

		//超时设定
		tv.tv_sec = 0;
		tv.tv_usec = 0;

		//添加连接到描述符集中
		FD_SET(sockfd[0], &sockfdset);
		FD_SET(sockfd[1], &sockfdset);
		FD_SET(sockfd[2], &sockfdset);
		FD_SET(sockfd[3], &sockfdset);

		int ret = select(maxsockfd + 1, &sockfdset, NULL, NULL, &tv);//带超时的select函数
		//int ret=select(maxsockfd+1,&sockfdset,NULL,NULL,NULL);//不带超时的select函数，不阻塞，如果没有数据，立即返回。

		if (ret < 0)//没有有效连接
		{
			return -1;
		}
		else if (ret == 0)//超时，继续循环
		{
			continue;
		}
		else//连接正常，可以接收数据
		{
			int i=0;
			for(i=0;i<4;i++)
			{
				countrfid=0;
				memset(buf,0,sizeof(buf));
				/*如果rfid有数据可读*/
				if (FD_ISSET(sockfd[i],&sockfdset))
				{
					ret = recv(sockfd[i], buf, sizeof(buf), 0);
					/*与ACK信息对比*/
					int cmp = strncmp(buf, ackbuf, 15);
					int cmpfull = strcmp(buf, ackbuf);

					/*接收出错*/
					if (ret <= 0) {
						printf("rfid%d数据读取出错!\n",i+1);
					}

					/*接收正常，开始处理数据*/
					else {
						switch (cmp) //判断是否是ACK返回信息
						{
							case 0: /*是ACK返回信息*/
							{
								if (cmpfull == 0) /*ACK返回信息正确*/
								{
									//printf("rfid%d Receive OK,Detecting Signal...%s\n",i+1,buf);
								}
								else //ACK返回信息错误，再次尝试发送‘开始’命令
								{
									printf("rfid%d Receive FAILED,Send Again\n",i+1);
									send(sockfd[i], startbuf, strlen(startbuf), 0); //发送命令让rfid开始读取。
									if (countrfid++ == 10) //尝试次数
									{
										printf("rfid%d ERROR!!!\n",i+1);
									}
								}
								break;
							}
							default: /*RFID读取到数据*/
							{
								int j=0;
								for(j=0;j<4;j++)
								{
									agvreportstruct.RFIDSenser[i][j]=buf[j+1];
									//printf("rfid%d is %s\n",j,buf);
								}
								int k=0;
								while(k<1000)
								{
									k++;
								}
								send(sockfd[i], startbuf, strlen(startbuf), 0); //发送命令让rfid开始读取。
								break;
							}
						}
					}
				}
			}
		}
	}
	return 0;
}
