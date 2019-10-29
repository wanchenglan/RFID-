// lcd_test.c 
#include "lcd_app.h"
#include <sys/ioctl.h>

static int x_res, y_res;
static int line_length;
static int screen_size;
static int pixel_length;

static	struct fb_var_screeninfo var;	/* Current var */
static	struct fb_fix_screeninfo fix;	/* Current fix */

void get_lcd_info(void)
{
	int ret;
	/* 打开 framebuffer */
	int fb_fd = open("/dev/fb0" , O_RDWR);
	if(fb_fd < 0)
	{
		printf("neo: cannot open the fb device\n");
		return ;
	}

	/*获得固定参数 和 变化参数 */
	ret = ioctl(fb_fd , FBIOGET_VSCREENINFO , &var);
	if(ret)
	{
		printf("neo: get FBIOGET_VSCREENINFO args error");
		return  ;
	}
	
	ret = ioctl(fb_fd , FBIOGET_FSCREENINFO , &fix);
	if(ret)
	{
		printf("neo: get FBIOGET_FSCREENINFO args error");
		return  ;
	}

	line_length	 =  fix.line_length;
	screen_size  =  fix.smem_len;
	pixel_length =  var.bits_per_pixel / 8;
	x_res = var.xres;
	y_res = var.yres;
	
	
	printf("fix.line_length 	= %d\n",fix.line_length);
	printf("fix.smem_len 		= %d\n",fix.smem_len);
	printf("var.bits_per_pixel 	= %d\n",var.bits_per_pixel);
	printf("var.xres = %d\n",var.xres);
	printf("var.yres = %d\n",var.yres);
	printf("screen_size = %d\n",screen_size); //3072000  800*960
	
	close(fb_fd);
	
}
//输出像素到屏幕中
int  show_put_pixel(unsigned int *fb_mem,int x ,int y , unsigned int color)
{
	
	
	if(y < 480 && x<800 && x >= 0 && y >= 0 )
	{
	unsigned int *pen_32 = (unsigned int *)(fb_mem + y*800 + x);
	if(var.bits_per_pixel != 32)
	{
		printf(" sorry ! only support 32 bit\n");
		return 1;
	}
	*pen_32 = color ;
	}
	else
	{
		printf("x=%d,y=%d is overflow\n",x,y);
		return 1;
	}
	
	return 0;
}

//显示单个汉字
void lcd_put_ascii(unsigned int *fb_mem,int x, int y , unsigned char c)
{
	int i,j;
	unsigned char byte;
	// 获得点阵的起始坐标
	unsigned char *dots = (unsigned char*)&fontdata_8x16[c*16] ;
	printf("dots \n");
	for(i=0 ; i<16; i++)
	{
		byte = dots[i];
		for(j=7;j>=0;j--)
		{
			if(byte & (1<<j))	//有数据就显示颜色，前景色
				show_put_pixel(fb_mem,x+(7-j),y+i,0xffffff);
			else	//没数据就显示黑色，可以设为背景色
			{
				show_put_pixel(fb_mem,x+(7-j),y+i,0);  //显示字符时的底色
			}
		}
	}
}

/*	
	功能：根据得到的RGB24数据，进行图像任意位置显示
	（主要是迎合上面的各种转换函数的显示测试）
	x:显示的x轴起点
	y:显示的y轴起点
	w:显示图像宽度。
	h:显示图像高度。
	bit_dept:要显示的图像位深 24/32
	pbmp_data：要进行显示的RGB24_buf
*/
int Show_FreeType_Bitmap(unsigned int *fb_mem,FT_Bitmap*  bitmap,int start_x,int start_y,int color, int flag,int flagcolor)
{
	int  buff_x, buff_y;	//遍历bitmap时使用
	int  x,y;	//循环遍历使用
	int  end_x = start_x + bitmap->width;	//图像宽度
	int  end_y = start_y + bitmap->rows;	//图像高度
		
	for ( x = start_x, buff_x = 0; x < end_x; buff_x++, x++ )	//y表示起点y，x表示起点x
	{
		for ( y = start_y, buff_y = 0; y < end_y; buff_y++, y++ )	//y表示起点y，x表示起点x
		{
			//LCD边界处理
			if ( x < 0 || y < 0)
				continue;

			if(bitmap->buffer[buff_y * bitmap->width + buff_x] )	//判断该位上是不是为1，1则表明需要描点
			{
				if(show_put_pixel(fb_mem,start_x+buff_x , start_y+buff_y ,  color))	//在当前x位置加上p的偏移量（p表示buff中列的移动）
				{
					return  1;
				}
				//在当前y位置加上q的偏移量（q表示buff中行的移动）
			}
			else if(flag == 1)	
			{				
				if(show_put_pixel(fb_mem,start_x+buff_x,start_y+buff_y , flagcolor))		//显示底色
				{
					return 1;
				}
			}
		}
	}
	return 0;
} 
//在LCD 映射地址上显示汉字
int Lcd_Show_FreeType(unsigned int *fb_mem,wchar_t *wtext,int size,int color, int flag, int flagcolor,int start_x,int start_y)
{
	//判断传递的参数是否正确
	if(start_x<0 || start_y < 0)
	{
		return 0;
	}

	
	//1.定义库所需要的变量
	FT_Library    library;
	FT_Face       face;
	FT_GlyphSlot  slot;	//用于指向face中的glyph
	FT_Vector     pen;                    
	FT_Error      error;

	//2.初始化库
	error = FT_Init_FreeType( &library );            

	//3.打开一个字体文件，并加载face对象:
	error = FT_New_Face( library, "./simsun.ttc", 0, &face ); 
	slot = face->glyph;
	
	//4.设置字体大小
	error = FT_Set_Pixel_Sizes(face, size, 0); 
	
	//x起点位置：start_x。需要*64
	pen.x = start_x * 64;	
	//y起点位置：LCD高度 - start_y。需要*64
	pen.y = ( y_res - start_y ) * 64;
			//480     //100     300  
	unsigned int  n=0;
	//每次取出显示字符串中的一个字
	for ( n = 0; n < wcslen(wtext); n++ )	
	{
		//5.设置显示位置和旋转角度，0为不旋转，pen为提前设置好的坐标
		FT_Set_Transform( face, 0, &pen );
		
		
		//将字形槽的字形图像，转为位图，并存到 face->glyph->bitmap->buffer[]，并且更新bitmap下的其他成员，
		//	face->glyph->bitmap.rows:图像总高度
		//	face->glyph->bitmap.width:图像总宽度
		//	face->glyph->bitmap.pitch:每一行的字节数
		//	face->glyph->bitmap.pixel_mode：像素模式（1单色 8反走样灰度）
		//	face->glyph->bitmap.buffer：点阵位图的数据缓冲区
	

		error = FT_Load_Char( face, wtext[n], FT_LOAD_RENDER );
			//FT_LOAD_RENDER表示转换RGB888的位图像素数据
			
		//出错判断	
		if ( error )
		  continue;                
		
		if(Show_FreeType_Bitmap(fb_mem,&slot->bitmap,slot->bitmap_left,y_res - slot->bitmap_top,color,flag,flagcolor))
		{
			//释放字库资源
			FT_Done_Face    (face);
			FT_Done_FreeType(library);
			return  0;
		}
		
		//增加笔位
		pen.x += slot->advance.x;
		
	}	
	
	//释放字库资源
	FT_Done_Face    (face);
	FT_Done_FreeType(library);
	return 1;
}






