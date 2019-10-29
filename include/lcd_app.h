#ifndef __LCD_APP_H
#define __LCD_APP_H

// ��ʾ ���� ���� �� �ַ� ���� ���� ͼ���
#include <sys/mman.h>
#include <stdio.h>
#include <linux/fb.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <wchar.h>
#include <string.h>
#include "ascii.h"

#include <ft2build.h>
#include FT_FREETYPE_H

//��ȡLCD�豸��Ϣ��ʼ���ֿ�
extern void get_lcd_info(void);

//��ʾ�����ַ�
extern void lcd_put_ascii(unsigned int *fb_mem,int x, int y , unsigned char c);

//��ʾ��
//extern void Lcd_Show_FreeType(unsigned int *fb_mem,wchar_t *wtext,int size,int color, int flag,int start_x,int start_y);
int Lcd_Show_FreeType(unsigned int *fb_mem,wchar_t *wtext,int size,int color, int flag, int flagcolor,int start_x,int start_y);
#endif