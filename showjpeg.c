#include <math.h>
#include <stdio.h>
#include "jpeglib.h"
#include <setjmp.h>
#include <yuyv.h>
#include <stdlib.h>


extern JSAMPLE * image_buffer;	/* Points to large array of R,G,B-order data */
extern int image_height;	/* Number of rows in image */
extern int image_width;		/* Number of columns in image */


struct my_error_mgr {
  struct jpeg_error_mgr pub;	/* "public" fields */

  jmp_buf setjmp_buffer;	/* for return to caller */
};

typedef struct my_error_mgr * my_error_ptr;

/*
 * Here's the routine that will replace the standard error_exit method:
 */

METHODDEF(void)
my_error_exit (j_common_ptr cinfo)
{
  /* cinfo->err really points to a my_error_mgr struct, so coerce pointer */
  my_error_ptr myerr = (my_error_ptr) cinfo->err;

  /* Always display the message. */
  /* We could postpone this until after returning, if we chose. */
  (*cinfo->err->output_message) (cinfo);

  /* Return control to the setjmp point */
  longjmp(myerr->setjmp_buffer, 1);
}


/*
 *	读取一张jpg图片
 */
 
GLOBAL(int)
read_JPEG_file(char * filename,unsigned int *lcd_p)
{
   //定义一个jpeg解码对象
  struct jpeg_decompress_struct cinfo;
  
   //定义一个jpeg出错对象
  struct my_error_mgr jerr;
  /* More stuff */
  FILE * infile;		/* 源文件 */
  JSAMPARRAY buffer;		/* 输出缓存buffer*/
  
  int row_stride;		/* 输出的行宽*/


    //打开需要解码的文件
  if ((infile = fopen(filename, "rb")) == NULL) {
    fprintf(stderr, "can't open %s\n", filename);
    return 0;
  }

  //初始化 JPEG出错对象
  cinfo.err = jpeg_std_error(&jerr.pub);
  jerr.pub.error_exit = my_error_exit;
  /* Establish the setjmp return context for my_error_exit to use. */
  if (setjmp(jerr.setjmp_buffer)) {
    /* 如果运行到这里, jpeg解码程序已经出错, If we get here, the JPEG code has signaled an error.
     * 需要进行一些出错处理, 比如关闭打开的jpeg图片等等, We need to clean up the JPEG object, close the input file, and return.
     */ 
    jpeg_destroy_decompress(&cinfo);
    fclose(infile);
    return 0;
  }
  
   //现在就可以初始化 解码对象
  jpeg_create_decompress(&cinfo);

  
   //关联 源文件 与 解码对象
  jpeg_stdio_src(&cinfo, infile);


   //读取jpeg文件头信息
  (void) jpeg_read_header(&cinfo, TRUE);
 

   //开始解码
  (void) jpeg_start_decompress(&cinfo);
  
	//图片的宽度 *  图片的位深    ->求出一行所占用的字节数
  row_stride = cinfo.output_width * cinfo.output_components;
 
 
	//申请堆空间   用buffer 指向堆空间
  buffer = (*cinfo.mem->alloc_sarray)
		((j_common_ptr) &cinfo, JPOOL_IMAGE, row_stride, 1);

		
   //int colorbuf[cinfo.output_height][cinfo.output_width];
   
   int x=0, y=0;
   unsigned char *p = NULL;
   char r, g, b;
   
   //开始读取图片的rgb颜色值
   //判断当前读取的行数是不是小于最后一行
  while (cinfo.output_scanline < cinfo.output_height) {
	
	y = cinfo.output_scanline;
	//printf("y=%d\n", y);
	
		//读取一行颜色数据到buffer中
    (void) jpeg_read_scanlines(&cinfo, buffer, 1);
	
    /* Assume put_scanline_someplace wants a pointer and sample count. */
	//把读取出来的颜色数据放到需要显示的地方
	//put_scanline_someplace(buffer[0], row_stride);  重点!!!重点!!!!重点
	
	p = buffer[0];	//存放第一个像素点的首地址,用于拼接四字节的像素点

/*	
	r = *(p+0);	//红像素
	g = *(p+1);	//绿像素
	b = *(p+2);	//蓝像素
	
	r = *(p+3);	//红像素
	g = *(p+4);	//绿像素
	b = *(p+5);	//蓝像素
*/

	//把RGB数据写入屏幕, 刷的只是图片宽度的像素点
	for(x=0; x<cinfo.output_width; x++)
	{
		r = *p++;	//红像素	00001010
		g = *p++;	//绿像素	11110000
		b = *p++;	//蓝像素	10100000
		
		if(y<480 && x<800)
		{
			*(lcd_p + x + y*800) = 0<<24 | r<<16 | g<<8 | b<<0;
		}
	}
	
	/*
	00000000 00001010  00000000  00000000 	r
	00000000 00000000  11110000  00000000 	g
	00000000 00000000  00000000  10100000	b
	------------------------------------------
	00000000 00001010  11110000  10100000
	A        R         G            B
	*/
  }


	
	//解码完成
  (void) jpeg_finish_decompress(&cinfo);
 
  //释放jpeg申请的所有资源
  jpeg_destroy_decompress(&cinfo);

  //关闭文件
  fclose(infile);


  return 1;
}

GLOBAL(int)
read_fit_JPEG_file (char * filename,unsigned int *lcd_p, int x_s, int y_s, int x_e, int y_e)
{
  /* This struct contains the JPEG decompression parameters and pointers to
   * working space (which is allocated as needed by the JPEG library).
   */
  struct jpeg_decompress_struct cinfo;
  /* We use our private extension JPEG error handler.
   * Note that this struct must live as long as the main JPEG parameter
   * struct, to avoid dangling-pointer problems.
   */
  struct my_error_mgr jerr;
  /* More stuff */
  FILE * infile;		/* source file */
  JSAMPARRAY buffer;		/* Output row buffer */
  int row_stride;		/* physical row width in output buffer */

  /* In this example we want to open the input file before doing anything else,
   * so that the setjmp() error recovery below can assume the file is open.
   * VERY IMPORTANT: use "b" option to fopen() if you are on a machine that
   * requires it in order to read binary files.
   */

  if ((infile = fopen(filename, "rb")) == NULL) {
    fprintf(stderr, "can't open %s\n", filename);
    return 0;
  }

  /* Step 1: allocate and initialize JPEG decompression object */

  /* We set up the normal JPEG error routines, then override error_exit. */
  cinfo.err = jpeg_std_error(&jerr.pub);
  jerr.pub.error_exit = my_error_exit;
  /* Establish the setjmp return context for my_error_exit to use. */
  if (setjmp(jerr.setjmp_buffer)) {
    /* If we get here, the JPEG code has signaled an error.
     * We need to clean up the JPEG object, close the input file, and return.
     */
    jpeg_destroy_decompress(&cinfo);
    fclose(infile);
    return 0;
  }
  /* Now we can initialize the JPEG decompression object. */
  jpeg_create_decompress(&cinfo);

  /* Step 2: specify data source (eg, a file) */

  jpeg_stdio_src(&cinfo, infile);

  /* Step 3: read file parameters with jpeg_read_header() */

  (void) jpeg_read_header(&cinfo, TRUE);
  /* We can ignore the return value from jpeg_read_header since
   *   (a) suspension is not possible with the stdio data source, and
   *   (b) we passed TRUE to reject a tables-only JPEG file as an error.
   * See libjpeg.txt for more info.
   */

  /* Step 4: set parameters for decompression */

  /* In this example, we don't need to change any of the defaults set by
   * jpeg_read_header(), so we do nothing here.
   */

  /* Step 5: Start decompressor */

  (void) jpeg_start_decompress(&cinfo);
  /* We can ignore the return value since suspension is not possible
   * with the stdio data source.
   */

  /* We may need to do some setup of our own at this point before reading
   * the data.  After jpeg_start_decompress() we have the correct scaled
   * output image dimensions available, as well as the output colormap
   * if we asked for color quantization.
   * In this example, we need to make an output work buffer of the right size.
   */ 
  /* JSAMPLEs per row in output buffer */
  row_stride = cinfo.output_width * cinfo.output_components;
  /* Make a one-row-high sample array that will go away when done with image */
  buffer = (*cinfo.mem->alloc_sarray)
		((j_common_ptr) &cinfo, JPOOL_IMAGE, row_stride, 1);
   int x=0, y=0, y_scan=0;
   unsigned char *p = NULL;
   char r, g, b;
   
   //定义一行像素点缓存
   // int lcd_buf[cinfo.output_width];
   //在对内存中申请图片大小缓存，存放argb数据
   int *pic_buf=NULL;
   pic_buf = (int *)malloc(cinfo.output_width*sizeof(int));
   
   float x_sample = (float)(cinfo.output_width) / (x_e-x_s);
   float y_sample = (float)(cinfo.output_height)/ (y_e-y_s);
   //printf("sample = %f\n", x_sample);
   
   float fit_sample = (x_sample>y_sample)?x_sample:y_sample;		//调整缩放比例，避免图片扭曲
    //printf("fit_sample = %f\n", fit_sample);
   
   //开始读取图片的rgb颜色值
   //判断当前读取的行数是不是小于最后一行
  while (cinfo.output_scanline < cinfo.output_height) {
	
	//y_scan记录当前读取的行
	y_scan = cinfo.output_scanline;
	// printf("y_scan=%d\n", y_scan);
	
		//读取一行颜色数据到buffer中
    (void) jpeg_read_scanlines(&cinfo, buffer, 1);
	
	if(y_scan != (int)(y*fit_sample))
	{
		continue;
	}
	
	//y记录已经写入屏幕的行数
	y++;
	if(y>=(y_e-y_s))
	{
		continue;
	}
	
	/* Assume put_scanline_someplace wants a pointer and sample count. */
	//把读取出来的颜色数据放到需要显示的地方
	
	p = buffer[0];	//存放第一个像素点的首地址,用于拼接四字节的像素点


	//把RGB数据写入屏幕, 刷的只是图片宽度的像素点
	for(x=0; x<cinfo.output_width; x++)
	{
		r = *p++;	//红像素	00001010
		g = *p++;	//绿像素	11110000
		b = *p++;	//蓝像素	10100000
		
		//把转化的像素点存放到缓存
		// lcd_buf[x] = 0<<24 | r<<16 | g<<8 | b<<0;
		pic_buf[x] = 0<<24 | r<<16 | g<<8 | b<<0;
	}
	
	//把像素点按比例丢弃显示到屏幕
	for(x=0; x<cinfo.output_width/fit_sample; x++)
	{
		// printf("x:%.0f\n",x*fit_sample);
		if((x+x_s) < x_e && (y+y_s) < y_e)
			*(lcd_p + (x+x_s) + (y+y_s)*800) = pic_buf[(int)(x*fit_sample)];
			// *(lcd_p + (x+x_s) + (y+y_s)*800) = lcd_buf[(int)(x*fit_sample)];
	}
  
  }
  
	free(pic_buf);
	//解码完成
	(void) jpeg_finish_decompress(&cinfo);

	//释放jpeg申请的所有资源
	jpeg_destroy_decompress(&cinfo);

	//关闭文件
	fclose(infile);


	return 1;
}

GLOBAL(int)
read_JPEG_buf(unsigned char *jpg_data, unsigned int jpg_size, unsigned int *lcd_p)
{
   //定义一个jpeg解码对象
  struct jpeg_decompress_struct cinfo;
  
   //定义一个jpeg出错对象
  struct my_error_mgr jerr;
  /* More stuff */
  FILE * infile;		/* 源文件 */
  JSAMPARRAY buffer;		/* 输出缓存buffer*/
  
  int row_stride;		/* 输出的行宽*/


  //初始化 JPEG出错对象
  cinfo.err = jpeg_std_error(&jerr.pub);
  jerr.pub.error_exit = my_error_exit;
  /* Establish the setjmp return context for my_error_exit to use. */
  if (setjmp(jerr.setjmp_buffer)) {
    /* 如果运行到这里, jpeg解码程序已经出错, If we get here, the JPEG code has signaled an error.
     * 需要进行一些出错处理, 比如关闭打开的jpeg图片等等, We need to clean up the JPEG object, close the input file, and return.
     */ 
    jpeg_destroy_decompress(&cinfo);

    return 0;
  }
  
   //现在就可以初始化 解码对象
  jpeg_create_decompress(&cinfo);

  
   //关联 源文件 与 解码对象
  //jpeg_stdio_src(&cinfo, infile);
  
  //关联 图像缓存 与 解码对象
  jpeg_mem_src(&cinfo, jpg_data, jpg_size);
	
	

   //读取jpeg文件头信息
  (void) jpeg_read_header(&cinfo, TRUE);
 

   //开始解码
  (void) jpeg_start_decompress(&cinfo);
  
	//图片的宽度 *  图片的位深    ->求出一行所占用的字节数
  row_stride = cinfo.output_width * cinfo.output_components;
 
 
	//申请堆空间   用buffer 指向堆空间
  buffer = (*cinfo.mem->alloc_sarray)
		((j_common_ptr) &cinfo, JPOOL_IMAGE, row_stride, 1);

		
   //int colorbuf[cinfo.output_height][cinfo.output_width];
   
   int x=0, y=0;
   unsigned char *p = NULL;
   char r, g, b;
   
   //开始读取图片的rgb颜色值
   //判断当前读取的行数是不是小于最后一行
  while (cinfo.output_scanline < cinfo.output_height) {
	
	y = cinfo.output_scanline;
	//printf("y=%d\n", y);
	
		//读取一行颜色数据到buffer中
    (void) jpeg_read_scanlines(&cinfo, buffer, 1);
	
    /* Assume put_scanline_someplace wants a pointer and sample count. */
	//把读取出来的颜色数据放到需要显示的地方
	p = buffer[0];	//存放第一个像素点的首地址,用于拼接四字节的像素点


	//把RGB数据写入屏幕, 刷的只是图片宽度的像素点   ====>>居中显示
	for(x=0; x<cinfo.output_width; x++)
	{
		r = *p++;	//红像素	00001010
		g = *p++;	//绿像素	11110000
		b = *p++;	//蓝像素	10100000
		
		if(y<480 && x<800)		// x =====  (x+(800-cinfo.output_width)/2)
		{
			*(lcd_p + x + y*800) = 0<<24 | r<<16 | g<<8 | b<<0;
		}
	}
  }


	//解码完成
  (void) jpeg_finish_decompress(&cinfo);
 
  //释放jpeg申请的所有资源
  jpeg_destroy_decompress(&cinfo);


  return 1;
}

GLOBAL(int)
read_fit_JPEG_buf(unsigned char *jpg_data, unsigned int jpg_size, unsigned int *lcd_p, int x_s, int y_s, int x_e, int y_e)
{
   //定义一个jpeg解码对象
  struct jpeg_decompress_struct cinfo;
  
   //定义一个jpeg出错对象
  struct my_error_mgr jerr;
  /* More stuff */
  FILE * infile;		/* 源文件 */
  JSAMPARRAY buffer;		/* 输出缓存buffer*/
  
  int row_stride;		/* 输出的行宽*/


  //初始化 JPEG出错对象
  cinfo.err = jpeg_std_error(&jerr.pub);
  jerr.pub.error_exit = my_error_exit;
  /* Establish the setjmp return context for my_error_exit to use. */
  if (setjmp(jerr.setjmp_buffer)) {
    /* 如果运行到这里, jpeg解码程序已经出错, If we get here, the JPEG code has signaled an error.
     * 需要进行一些出错处理, 比如关闭打开的jpeg图片等等, We need to clean up the JPEG object, close the input file, and return.
     */ 
    jpeg_destroy_decompress(&cinfo);

    return 0;
  }
  
   //现在就可以初始化 解码对象
  jpeg_create_decompress(&cinfo);

  
   //关联 源文件 与 解码对象
  //jpeg_stdio_src(&cinfo, infile);
  
  //关联 图像缓存 与 解码对象
  jpeg_mem_src(&cinfo, jpg_data, jpg_size);
	
	

   //读取jpeg文件头信息
  (void) jpeg_read_header(&cinfo, TRUE);
 

   //开始解码
  (void) jpeg_start_decompress(&cinfo);
  
	//图片的宽度 *  图片的位深    ->求出一行所占用的字节数
  row_stride = cinfo.output_width * cinfo.output_components;
 
 
	//申请堆空间   用buffer 指向堆空间
  buffer = (*cinfo.mem->alloc_sarray)
		((j_common_ptr) &cinfo, JPOOL_IMAGE, row_stride, 1);

		
   //int colorbuf[cinfo.output_height][cinfo.output_width];
   
   int x=0, y=0, y_scan=0;
   unsigned char *p = NULL;
   char r, g, b;
   
   //定义一行像素点缓存
   int lcd_buf[cinfo.output_width];
   //在对内存中申请图片大小缓存，存放argb数据
   int *pic_buf=NULL;
   pic_buf = (int *)malloc(cinfo.output_width*sizeof(int));
   
   float x_sample = (float)(cinfo.output_width) / (x_e-x_s);
   float y_sample = (float)(cinfo.output_height)/ (y_e-y_s);
   //printf("sample = %f\n", x_sample);
   
   float fit_sample = (x_sample>y_sample)?x_sample:y_sample;		//最大缩放比例，避免图片扭曲
   
   //开始读取图片的rgb颜色值
   //判断当前读取的行数是不是小于最后一行
  while (cinfo.output_scanline < cinfo.output_height) {
	
	//y_scan记录当前读取的行
	y_scan = cinfo.output_scanline;
	//printf("y_scan=%d\n", y_scan);
	
		//读取一行颜色数据到buffer中
    (void) jpeg_read_scanlines(&cinfo, buffer, 1);
	
	if(y_scan != (int)(y*fit_sample))
	{
		continue;
	}
	
	//y记录已经写入屏幕的行数
	y++;
	if(y>=(y_e-y_s))
	{
		continue;
	}
	
    /* Assume put_scanline_someplace wants a pointer and sample count. */
	//把读取出来的颜色数据放到需要显示的地方
	p = buffer[0];	//存放第一个像素点的首地址,用于拼接四字节的像素点


	//把RGB数据写入屏幕, 刷的只是图片宽度的像素点
	for(x=0; x<cinfo.output_width; x++)
	{
		r = *p++;	//红像素	00001010
		g = *p++;	//绿像素	11110000
		b = *p++;	//蓝像素	10100000
		
		//把转化的像素点存放到缓存
		// lcd_buf[x] = 0<<24 | r<<16 | g<<8 | b<<0;
		pic_buf[x] = 0<<24 | r<<16 | g<<8 | b<<0;
	}
	
	//把像素点按比例丢弃显示到屏幕
	for(x=0; x<cinfo.output_width/fit_sample; x++)
	{
		if((x+x_s) < x_e && (y+y_s) < y_e)
			*(lcd_p + (x+x_s) + (y+y_s)*800) = pic_buf[(int)(x*fit_sample)];
			// *(lcd_p + (x+x_s) + (y+y_s)*800) = lcd_buf[(int)(x*fit_sample)];
	}
  }


	free(pic_buf);
	//解码完成
  (void) jpeg_finish_decompress(&cinfo);
 
  //释放jpeg申请的所有资源
  jpeg_destroy_decompress(&cinfo);


  return 1;
}
