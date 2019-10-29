#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <error.h>
#include <unistd.h>
#include <sys/mman.h>
#include <yuyv.h>
#include <pthread.h>		//创建线程
#include <linux/input.h>	//输入子系统所有事件码
#include <locale.h>
#include <wchar.h>
#include <sys/ioctl.h>
#include <termios.h> 
#include <sys/select.h>
#include <netdb.h>
#include <errno.h>
#include "sqlite3.h"
#include "lcd_app.h"

#define DEV_PATH   "/dev/ttySAC1"
#define _DEBUG_

#define width	60 //40
#define height	50 //33

//共用变量声明
volatile unsigned int cardid ;
static struct timeval timeout;
char location[80][20];	//每层80个车位
int touch_x,touch_y;
int rfid_fd;
wchar_t *wchar = L"good good study ,day day up";
wchar_t *chinese_str = L"车库已满！";
wchar_t *bufasi = L"入库";
wchar_t *bufaso = L"出库";
wchar_t *bufasm = L"查看";

char carpp[42][20] ={"赣B.68937", "京Q.92Q23", "辽B.SB888", "晋A.V6600", "川A.00000", "冀G.UV123", "京N.2B945", "京H.99966", "辽A.TG421", "浙A.B8888", "黑A.2345F", "闽F.A7485", "浙B.99996", "黑L.B0444", "豫U.88888", "鲁J.74110", "粤K.29999", "江A.52555", "湘F.1C169", "湘A.55555", "辽L.12345", "辽E.34050", "湘K.NT000", "京A.FB007", "粤D.JB001", "粤Z.F0023", "粤A.12345", "川S.22222", "冀B.88BB8", "湘F.1C197", "苏O.F7238", "粤B.0K999", "京Q.R1A37", "京A.34157", "沪A.0012B", "苏D.FF458", "闽A.9988A", "豫J.00001", "豫J.PP777", "桂K.88888", "川F.2B937", "黑B.52DJ8"};


//外部函数引用声明
extern int read_fit_JPEG_buf(unsigned char *jpg_data, unsigned int jpg_size, unsigned int *lcd_p, int x_s, int y_s, int x_e, int y_e);
extern int read_fit_JPEG_file (char * filename,unsigned int *lcd_p, int x_s, int y_s, int x_e, int y_e);
extern int read_JPEG_file(char * filename,unsigned int *lcd_p);
extern void init_tty(int fd);
extern int PiccRequest(int fd, struct timeval *timeout);
extern int PiccAnticoll(int fd, struct timeval *timeout,volatile unsigned int *cardid);
size_t mbstowcs(wchar_t *dest, const char *src, size_t n);	//把char型字符串转换成宽型

//在某区域使用颜色覆盖
void clean(unsigned int*lcd_p,int px,int py,int x,int y,int color)
{

	lcd_p = lcd_p+py*800+px;
	
	for(py=0;py<y;py++)
	for(px=0;px<x;px++)
	{
		*(lcd_p+py*800+px) = color;
	}	
}

int get_xy(int *x, int *y)		//获取坐标
{
	//打开触摸屏驱动设备
	int touch_fd = open("/dev/input/event0", O_RDONLY);
	if(touch_fd == -1)
	{
		perror("open touch_fd fail");
		return -1;
	}
	
	//触摸屏事件数据信息结构体
	struct input_event ts_buf;
	int z=0;
	
	while(1)
	{
		//读取触摸屏数据
		read(touch_fd, &ts_buf, sizeof(struct input_event));
		
		//判断是否为触摸屏事件
		if(ts_buf.type == EV_ABS)
		{
			//判断是X轴 ABS_X,打印X轴数据 ts_buf.value
			if(ts_buf.code == ABS_X)
			{
				*x = ts_buf.value;
			}
			
			//判断是Y轴 ABS_y,打印Y轴数据 ts_buf.value
			if(ts_buf.code == ABS_Y)
			{
				*y = ts_buf.value;
			}
			
			//判断是松手 ABS_PRESSURE, 打印松手数据 ts_buf.value
			if(ts_buf.code == ABS_PRESSURE)
			{
				z = ts_buf.value;
				//printf("z = %d\n", ts_buf.value);
				//松手判断,按下为200, 松手是0
				if(z == 0)
				{
					//printf("x = %d\n", *x);
					//printf("y = %d\n", *y);
					// break;
					continue;
				}
			}
		}
		
		if(ts_buf.type == EV_KEY && ts_buf.code==BTN_TOUCH)  // 判断是不是 松开还是按下 0 为松开 200 为按下
		{
			z=ts_buf.value;	
			//printf("z=%d\n", z);
			if(z==0)
			{
				//printf("x:%d\n",*x);   // x坐标的值
				//printf("y:%d\n",*y);   // y坐标的值
				// break;
				continue;
			}
		}
	}
	close(touch_fd);
}

//从数组中得到空闲位置
int get_FreePosition(int cur_position)
{
	int free=cur_position;
	int count = 0;
	while(count < 80)
	{
		if(strcmp(location[free],"0000000000000000000")==0)
			break;
		free = (free+1)%80;
		printf("count:%d location[%d]=%s\n",count,free,location[free]);
		count++;
	}
	
	if(count == 80)
	{
		free = -1;
	}
	
	return free;
}

int get_curTime(char *buf)		//将当前时间转换成字符串 -》2019-07-22 231212
{
	time_t now;
	struct tm *tm_v;
	
	time(&now);	//得到当前时间：秒值
	
	tm_v = localtime(&now);	//将秒钟值转换存入tm结构体中
	
	sprintf(buf,"%.4d-%.2d-%.2d %.2d%.2d%.2d",
				tm_v->tm_year+1900,tm_v->tm_mon+1,tm_v->tm_mday,
				tm_v->tm_hour,tm_v->tm_min,tm_v->tm_sec);
	printf("current time:%s\n",buf);
	return 0;
}

int get_imgName(char *buf)		//将当前时间转换成字符串 -》./img/20190722231212.jpg
{
	time_t now;
	struct tm *tm_v;
	
	time(&now);	//得到当前时间：秒值
	
	tm_v = localtime(&now);	//将秒钟值转换存入tm结构体中
	
	sprintf(buf,"./img/%.4d%.2d%.2d%.2d%.2d%.2d.jpg",
				tm_v->tm_year+1900,tm_v->tm_mon+1,tm_v->tm_mday,
				tm_v->tm_hour,tm_v->tm_min,tm_v->tm_sec);
	printf("imgName:%s\n",buf);
	return 0;
}

int get_CardId()		//获取卡号
{
	while(1)
	{
		/*请求天线范围的卡*/
		if ( PiccRequest(rfid_fd,&timeout) )
		{
			printf("The request failed!\n");
			// close(rfid_fd);
			sleep(2);
			continue;
		}
		/*进行防碰撞，获取天线范围内最大的ID*/
		if( PiccAnticoll(rfid_fd,&timeout,&cardid) )
		{
			printf("Couldn't get card-id!\n");
			close(rfid_fd);
			rfid_fd = open(DEV_PATH, O_RDWR | O_NOCTTY | O_NONBLOCK);
			/*初始化串口*/
			init_tty(rfid_fd);
			printf("reboot !\n");
			sleep(2);
			continue;
		}
		printf("card ID = %x\n", cardid);
		sleep(2);
		cardid = 0;
	}
}

//线程调用函数
void *Pt_GetXY(void *arg)
{
	get_xy(&touch_x,&touch_y);
}

void *Pt_GetCardID(void *arg)
{
	get_CardId();
}


int main(void)
{
	//字库使用
	wchar_t  bufas[1024]={0};
	memset(bufas,0,sizeof(bufas));//将内存空间都赋值为‘\0’
	
	//数据库建立
	sqlite3 *db = NULL;
	char *zErrMsg = 0;
	int rc,i;
	char *sql_cre;
	char sql[100];
	char carno[20];
	char time[20];
	int position=0,cur_position=0;
	char imgpath[30];		//="./img/cap123.jpg"
	for(i=0;i<80;i++)
	{
		strcpy(location[i],"0000000000000000000");
	}
	//数据查询
	int nrow=0,ncolumn=0;
	int status = 0;
	char **azResult;	//二维数组存放结果
	
	rc = sqlite3_open("car.db",&db);	//打开指定的数据库文件，如果不存在将创建一个
	if(rc)
	{
		fprintf(stderr,"Can't open database:%s\n",sqlite3_errmsg(db));
		sqlite3_close(db);
		return 0;
	}
	else
	{
		printf(" open database \'car.db\' successful!\n");
	}

	//创建一个表，如果该表存在，则不创建，并给出提示信息，存储在zErrMsg中
	sql_cre = "CREATE TABLE CarInfo(\
	       ID INTEGER PRIMARY KEY AUTOINCREMENT,\
		   CardID INTEGER not null,\
	       CarID VARCHAR(20) not null,\
		   Position VACHAR(5),\
	       InTime datetime default (datetime('now','localtime')),\
	       OutTime datetime default (datetime('now','localtime')),\
		   ImgPath text\
	       );";
	sqlite3_exec(db,sql_cre,0,0,&zErrMsg);
	//创建表InCabInfo,现在仍在库的信息
	sql_cre = "CREATE TABLE InCabInfo(\
	       ID INTEGER PRIMARY KEY AUTOINCREMENT,\
	       CarID VARCHAR(20) not null,\
		   Position VACHAR(5),\
	       InTime datetime default (datetime('now','localtime')),\
		   ImgPath text\
	       );";
	sqlite3_exec(db,sql_cre,0,0,&zErrMsg);
	
	//屏幕点击
	pthread_t pt_touch,pt_cardid;	//线程ID
	int reflush=1,ret,c_x=0,c_y=0;
	int mode = 1;	//工作模式：	1、录入模式		2、查看模式
	int flag = 0;
	struct jpg_data camera_buf;		//摄像头数据
	
	//获取LCD屏幕信息
	get_lcd_info();
	fflush(stdout);

	
	//以读写方式打开屏幕设备
	int lcd_fd = open("/dev/fb0", O_RDWR);
	if(lcd_fd == -1)
	{
		perror("open lcd fail : ");
		return -1;
	}
	
	//映射屏幕设备到共享内存
	unsigned int *lcd_p = mmap(NULL, 800*480*4, PROT_READ|PROT_WRITE, MAP_SHARED, lcd_fd, 0);
	if(lcd_p == NULL)
	{
		perror("mmap fail : ");
		close(lcd_fd);
		return -1;
	}	
	
	clean(lcd_p,0,0,800,480,0);		
	read_JPEG_file("./img/fix_box.jpg", lcd_p);	//刷底图
	
	/***********采集摄像头数据***************/
	
	//摄像头初始化，打开摄像头驱动文件
	linux_v4l2_yuyv_init("/dev/video7");
	
	//要求摄像头采集数据
	linux_v4l2_start_yuyv_capturing();
	
	/*************** RFIF模块 *****************/
	rfid_fd = open(DEV_PATH, O_RDWR | O_NOCTTY | O_NONBLOCK);
	/*O_NOCTTY：表示告诉linux系统这个程序不会成为这个端口的控制终端，否则，所有的输入
	  比如键盘上过来的Ctrl+C中止信号，会影响你的进程

	O_NDELAY：表示告诉系统，不关心载波检测，即另一端是否连接，否则，除非
	  DCD信号线上有space电压，否则会一直睡眠。*/
	
	if (rfid_fd < 0)
	{
		fprintf(stderr, "Open Gec210_ttySAC1 fail!\n");
		return -1;
	}
	/*初始化串口*/
	init_tty(rfid_fd);
	timeout.tv_sec = 1;
	timeout.tv_usec = 0;
	
	//创建子线程，检测屏幕点击
	ret = pthread_create(&pt_touch,NULL,Pt_GetXY,NULL);
	if(ret != 0)
	{
		printf("建立线程失败\n");
		return -10;
	}
	//创建子线程，用于获取卡号
	ret = pthread_create(&pt_cardid,NULL,Pt_GetCardID,NULL);
	if(ret != 0)
	{
		printf("建立线程失败\n");
		return -10;
	}
	
	while(1)
	{
		//读取一帧的内容获取摄像头采集数据
		linux_v4l2_get_yuyv_data(&camera_buf);
		read_fit_JPEG_buf(camera_buf.jpg_data, camera_buf.jpg_size, lcd_p, 640, 350, 800, 480);
		
		if(reflush == 1)	//更新标识
		{
			for(i=0;i<80;i++)
			{
				if(strcmp(location[i],"0000000000000000000")==0)	//没有
				{
					//图标显示
					read_fit_JPEG_file("./img/kong.jpg", lcd_p,
						width*(int)(i%10)+20, height*(int)(i/10)+20,
						width*(int)(i%10)+60, height*(int)(i/10)+50 );	//结束坐标
				}
				else
				{
					//图标显示
					read_fit_JPEG_file("./img/yong.jpg", lcd_p,
						width*(int)(i%10)+20, height*(int)(i/10)+20,
						width*(int)(i%10)+60, height*(int)(i/10)+50 );	//结束坐标
				}
			}
			reflush = 0;
		}
		
		if(mode ==2)		//查看模式
		{
			nrow=0;
			ncolumn=0;
			if(touch_x>20 && touch_x<60 && touch_y>20 && touch_y<50 && flag==0)	//车位0
			{
				touch_x = 0;
				touch_y = 0;
				
				if(strcmp(location[0],"0000000000000000000") == 0)
				{
					printf("此地无垠:%s\n",location[0]);
					continue;
				}
				bzero(sql,sizeof(sql));	//清空sql缓存
				sprintf(sql,"SELECT CarID,InTime,ImgPath FROM InCabInfo where CarID='%s' ",location[0]);
				flag = 1;
			}
			
			if(touch_x>80 && touch_x<120 && touch_y>20 && touch_y<50 && flag==0)	//车位1
			{
				touch_x = 0;
				touch_y = 0;
				
				if(strcmp(location[1],"0000000000000000000") == 0)
				{
					printf("此地无垠:%s\n",location[1]);
					continue;
				}
				bzero(sql,sizeof(sql));	//清空sql缓存
				sprintf(sql,"SELECT CarID,InTime,ImgPath FROM InCabInfo where CarID='%s' ",location[1]);
				flag = 1;
			}
			
			if(touch_x>140 && touch_x<180 && touch_y>20 && touch_y<50 && flag==0)	//车位2
			{
				touch_x = 0;
				touch_y = 0;
				
				if(strcmp(location[2],"0000000000000000000") == 0)
				{
					printf("此地无垠:%s\n",location[2]);
					continue;
				}
				bzero(sql,sizeof(sql));	//清空sql缓存
				sprintf(sql,"SELECT CarID,InTime,ImgPath FROM InCabInfo where CarID='%s' ",location[2]);
				flag = 1;
				
				// touch_x = 0;
				// touch_y = 0;
			}
			
			if(touch_x>200 && touch_x<240 && touch_y>20 && touch_y<50 && flag==0)	//车位3
			{
				touch_x = 0;
				touch_y = 0;
				
				if(strcmp(location[3],"0000000000000000000") == 0)
				{
					printf("此地无垠:%s\n",location[3]);
					continue;
				}
				bzero(sql,sizeof(sql));	//清空sql缓存
				sprintf(sql,"SELECT CarID,InTime,ImgPath FROM InCabInfo where CarID='%s' ",location[3]);
				flag = 1;
			}
			
			if(touch_x>260 && touch_x<300 && touch_y>20 && touch_y<50 && flag==0)	//车位4
			{
				touch_x = 0;
				touch_y = 0;
				
				if(strcmp(location[4],"0000000000000000000") == 0)
				{
					printf("此地无垠:%s\n",location[4]);
					continue;
				}
				bzero(sql,sizeof(sql));	//清空sql缓存
				sprintf(sql,"SELECT CarID,InTime,ImgPath FROM InCabInfo where CarID='%s' ",location[4]);
				flag = 1;
			}
			
			if(touch_x>320 && touch_x<360 && touch_y>20 && touch_y<50 && flag==0)	//车位5
			{
				touch_x = 0;
				touch_y = 0;
				
				if(strcmp(location[5],"0000000000000000000") == 0)
				{
					printf("此地无垠:%s\n",location[5]);
					continue;
				}
				bzero(sql,sizeof(sql));	//清空sql缓存
				sprintf(sql,"SELECT CarID,InTime,ImgPath FROM InCabInfo where CarID='%s' ",location[5]);
				flag = 1;
			}
			
			if(touch_x>380 && touch_x<420 && touch_y>20 && touch_y<50 && flag==0)	//车位6
			{
				touch_x = 0;
				touch_y = 0;
				
				if(strcmp(location[6],"0000000000000000000") == 0)
				{
					printf("此地无垠:%s\n",location[6]);
					continue;
				}
				bzero(sql,sizeof(sql));	//清空sql缓存
				sprintf(sql,"SELECT CarID,InTime,ImgPath FROM InCabInfo where CarID='%s' ",location[6]);
				flag = 1;
			}
			//....
			
			if(touch_x>640 && touch_x<800 && touch_y>0 && touch_y<480)
			{
				reflush = 1;	//重建 图
				mode = 1;		//返回录入模式
				bufasm = L"录入";
			}
			
			if(flag == 1)
			{
				sqlite3_get_table(db,sql,&azResult,&nrow,&ncolumn,&zErrMsg);
				printf("row:%d column:%d\n",nrow,ncolumn);
				printf("\nThe result of querying is : \n");
				for(i=0;i<(nrow+1)*ncolumn;i++)
				{
					printf("azResult[%d] = %s\n",i,azResult[i]);
				}
				
				bzero(sql,sizeof(sql));	//清除sql缓存

				get_curTime(time);
				if(nrow > 0)
				{
					//显示车辆信息 CarID,InTime,ImgPath
					printf("carid:%s InTime:%s Total:%d\n",
							azResult[(nrow+1)*ncolumn-3],
							azResult[(nrow+1)*ncolumn-2],
							(int)(atoi(time)-atoi(azResult[(nrow+1)*ncolumn-2])));
							
					//信息显示
					clean(lcd_p,640,0,800,300,0);	
					Lcd_Show_FreeType(lcd_p,bufasm,32,0xffffff,1,0,660,150);

					//显示车辆图片
					read_fit_JPEG_file(azResult[(nrow+1)*ncolumn-1], lcd_p, 10, 10,600,470);	//结束坐标
				}
				
				
				//释放 azResult 的内存空间
				sqlite3_free_table( azResult );
#ifdef _DEBUG_
	printf("zErrMsg = %s\n",zErrMsg);
#endif		
				
				//更改更新标志
				flag = 0;
				
			}
			
		}
		else if(mode == 1)	//录入模式
		{
			if(touch_x>0 && touch_x<640 && touch_y>0 && touch_y<480)
			{
				mode = 2;	//查看模式，暂时不清空坐标，在查看模式下清空
				bufasm = L"查看";
			}
			
			if(cardid == 0)		//2s内是否有刷卡，=0刷卡失败
			{
				continue;
			}
			printf("card ID = %x\n", cardid);

			//随机选择车牌	0-41 的随机整数
			bzero(carno,sizeof(carno));
			i = (int)rand()*42%42;
			printf("输入车牌号码：");
			printf("random:%d\n",abs(i));
			
			strcpy(carno,carpp[abs(i)%42]);
			
			nrow=0;
			ncolumn=0;
			sprintf(sql,"SELECT CarID,InTime FROM InCabInfo where CarID='%s' ",carno);
			sqlite3_get_table(db,sql,&azResult,&nrow,&ncolumn,&zErrMsg);
			printf("row:%d column:%d\n",nrow,ncolumn);
			printf("\nThe result of querying is : \n");
			for(i=0;i<(nrow+1)*ncolumn;i++)
			{
				printf("azResult[%d] = %s\n",i,azResult[i]);
			}
			
			if(nrow > 0)
			{
				status = 2;	//有入记录
			}
			else
			{
				status = 1;	//没有记录
			}
			
			printf("status:%d\n",status);
			
			//释放 azResult 的内存空间
			sqlite3_free_table( azResult );

#ifdef _DEBUG_
	printf("zErrMsg = %s\n",zErrMsg);
#endif
	
			bzero(sql,sizeof(sql));		//清空sql缓存
			if(status == 1)			//插入记录
			{
				get_imgName(imgpath);	//以当前时间作为保存图片名
				
				//保存图片
				int save_imgfd = open(imgpath,O_RDWR|O_CREAT);
				if(save_imgfd == -1)
					perror("save file error!\n");
				
				if(write(save_imgfd,camera_buf.jpg_data,camera_buf.jpg_size)<0)
					perror("write file error!\n");
				close(save_imgfd);
				
				get_curTime(time);	//获取当前时间
				position = get_FreePosition(cur_position);
				if(position == -1)
				{
					printf("车库已满！\n");
					// clean(lcd_p,640,0,800,300,0);
					Lcd_Show_FreeType(lcd_p,chinese_str,32,0xffffff,1,0,660,250);
					continue;
				}
				cur_position = position;
				//更新数组信息
				strcpy(location[position],carno);
				
				//插入数据
				sprintf(sql,"INSERT INTO \"CarInfo\" VALUES (NULL,%d,'%s',%d,'%s','%s','%s')",cardid,carno,position,time,time,imgpath);
				sqlite3_exec(db,sql,0,0,&zErrMsg);
				
				sprintf(sql,"INSERT INTO \"InCabInfo\" VALUES (NULL,'%s',%d,'%s','%s')",carno,position,time,imgpath);
				sqlite3_exec(db,sql,0,0,&zErrMsg);
				
				//信息显示
				// clean(lcd_p,640,0,800,300,0);
				Lcd_Show_FreeType(lcd_p,bufasi,32,0xffffff,1,0,660,150);
				
				//图标显示
				read_fit_JPEG_file("./img/yong.jpg", lcd_p,
					width*(int)(position%10)+20, height*(int)(position/10)+20,
					width*(int)(position%10)+60, height*(int)(position/10)+50 );	//结束坐标
			}
			else if(status == 2)	//更新出记录
			{
				printf("出库！\n");
				get_curTime(time);	//获取当前时间
				//更新CarInfo数据
				sprintf(sql,"UPDATE \"CarInfo\" SET OutTime='%s' where CarID = '%s'",time,carno);
				sqlite3_exec(db,sql,0,0,&zErrMsg);
				//删除InCabInfo数据
				sprintf(sql,"DELETE FROM \"InCabInfo\" where CarID = '%s'",carno);
				sqlite3_exec(db,sql,0,0,&zErrMsg);
				//查询InTime数据
				sprintf(sql,"SELECT InTime,Position FROM CarInfo where CarID='%s' ",carno);
				sqlite3_get_table(db,sql,&azResult,&nrow,&ncolumn,&zErrMsg);
				printf("row:%d column:%d\n",nrow,ncolumn);
				printf("\nThe result of querying is : \n");
				for(i=0;i<(nrow+1)*ncolumn;i++)
				{
					printf("azResult[%d] = %s\n",i,azResult[i]);
				}
				
				printf("%s停靠时间为：%d",carno,(atoi(time)-atoi(azResult[(nrow+1)*ncolumn-2])));
				//更新数组信息
				position = atoi(azResult[(nrow+1)*ncolumn-1]);
				// bzero(location[position],sizeof(location[position]));
				strcpy(location[position],"0000000000000000000");
				
				//释放 azResult 的内存空间
				sqlite3_free_table( azResult );
				
				//信息显示
				// clean(lcd_p,640,0,800,300,0);
				Lcd_Show_FreeType(lcd_p,bufaso,32,0xffffff,1,0,660,150);
				
				//图标显示
				read_fit_JPEG_file("./img/kong.jpg", lcd_p,
					width*(int)(position%10)+20, height*(int)(position/10)+20,
					width*(int)(position%10)+60, height*(int)(position/10)+50 );	//结束坐标
				
			}
			
#ifdef _DEBUG_
	printf("zErrMsg = %s\n",zErrMsg);
#endif	
		
		}

		
	}
	
	//解除映射释放资源,关闭打开的屏幕设备
	munmap(lcd_p, 800*480*4);
	sqlite3_close(db);
	close(rfid_fd);
	close(lcd_fd);
	
	return 0;
}


/********************测试数据*********************************
*
*

	17,17	56,50
	77,16	115,50
	17,70
	
	O.(17,17)			
	 0		     40	 60
     ++++++++++++++  +++++++++++++++
	 +
	 +
	 + =33
	 
	 + =53
	 +
	 +
*
*



CREATE TABLE eCarInfo(
id integer primary key autoincrement,
CarId varchar(10) not null,
Username varchar(10),
InTime datetime not null default (datetime('now','localtime')),
OutTime datetime not null default (datetime('now','localtime')),
ImgPath text
);

insert into eCarInfo values (1,"苏B-H2222","GH",20190712102532,20190712102633,".img/cap001.jpg");

insert into eCarInfo values (0,"苏B-H2222","GH",'2019-07-16 10:23:12','2019-07-16 10:23:12',".img/cap001.jpg");
*
*
*
	read_fit_JPEG_file(imgpath, lcd_p,
			width*position+17, height*(int)(position/10)+17,
			width*position+57, height*(int)(position/10)+50 );	//结束坐标
*
*
*
*****************************************************/

