#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

static char temp_buf[] = {
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x03,0x40,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0xFF,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x40,0x01,0xFF,0xFF,0x80,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x40,0x00,0x00,0x00,0xFF,0xFE,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x7E,0x00,
    0x00,0x40,0x00,0x00,0x00,0x00,0x03,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xC2,0x00,
    0x00,0x40,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x02,0x00,
    0x00,0x40,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x02,0x00,
    0x00,0x40,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x02,0x00,
    0x00,0x40,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x02,0x00,
    0x00,0x40,0x00,0x00,0x00,0x00,0x00,0x03,0x80,0x00,0x00,0x00,0x00,0x00,0x02,0x00,
    0x00,0x40,0x00,0x00,0x00,0x00,0x00,0x0E,0x70,0x00,0x00,0x00,0x00,0x00,0x02,0x00,
    0x00,0x40,0x00,0x00,0x00,0x00,0x00,0x38,0x1E,0x00,0x00,0x00,0x00,0x00,0x02,0x00,
    0x00,0x40,0x00,0x00,0x00,0x00,0x00,0xE0,0x01,0xC0,0x00,0x00,0x00,0x00,0x02,0x00,
    0x00,0x40,0x00,0x00,0x00,0x00,0x03,0x80,0x00,0x38,0x00,0x00,0x00,0x00,0x02,0x00,
    0x00,0x40,0x00,0x00,0x00,0x00,0x0E,0x00,0x00,0x07,0x00,0x00,0x00,0x00,0x02,0x00,
    0x00,0x40,0x00,0x00,0x00,0x00,0x78,0x00,0x00,0x00,0xC0,0x00,0x00,0x00,0x02,0x00,
    0x00,0x40,0x00,0x00,0x00,0x03,0xC0,0x00,0x00,0x00,0x38,0x00,0x00,0x00,0x02,0x00,
    0x00,0x40,0x00,0x00,0x00,0x1E,0x00,0x00,0x00,0x00,0x0C,0x00,0x00,0x00,0x02,0x00,
    0x00,0x80,0x00,0x00,0x00,0x70,0x00,0x00,0x00,0x00,0x03,0x00,0x00,0x00,0x02,0x00,
    0x00,0x80,0x00,0x00,0x07,0x80,0x00,0x00,0x00,0x00,0x00,0xC0,0x00,0x00,0x02,0x00,
    0x00,0x80,0x00,0x00,0x38,0x00,0x00,0x00,0x00,0x00,0x00,0x70,0x00,0x00,0x04,0x00,
    0x00,0x80,0x00,0x03,0xC0,0x00,0x00,0x00,0x00,0x00,0x00,0x18,0x00,0x00,0x04,0x00,
    0x00,0x80,0x00,0x1E,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0E,0x00,0x00,0x04,0x00,
    0x01,0x00,0x00,0x70,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x03,0x00,0x00,0x04,0x00,
    0x01,0x00,0x00,0xC0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0xC0,0x00,0x04,0x00,
    0x01,0x00,0x00,0xC0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x70,0x00,0x04,0x00,
    0x01,0x00,0x00,0x60,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x1E,0x00,0x08,0x00,
    0x01,0x00,0x00,0x38,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0F,0x00,0x08,0x00,
    0x01,0x00,0x00,0x0C,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xF8,0x00,0x08,0x00,
    0x01,0x00,0x00,0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0F,0x00,0x00,0x10,0x00,
    0x01,0x00,0x00,0x01,0x80,0x00,0x00,0x00,0x00,0x00,0x00,0x38,0x00,0x00,0x10,0x00,
    0x01,0x00,0x00,0x00,0x60,0x00,0x00,0x00,0x00,0x00,0x01,0xC0,0x00,0x00,0x10,0x00,
    0x01,0x00,0x00,0x00,0x1C,0x00,0x00,0x00,0x00,0x00,0x03,0x00,0x00,0x00,0x30,0x00,
    0x01,0x00,0x00,0x00,0x03,0x80,0x00,0x00,0x00,0x00,0x0C,0x00,0x00,0x00,0x20,0x00,
    0x01,0x00,0x00,0x00,0x00,0xE0,0x00,0x00,0x00,0x00,0x38,0x00,0x00,0x00,0x20,0x00,
    0x01,0x00,0x00,0x00,0x00,0x1C,0x00,0x00,0x00,0x00,0x60,0x00,0x00,0x00,0x20,0x00,
    0x01,0x00,0x00,0x00,0x00,0x07,0x80,0x00,0x00,0x01,0x80,0x00,0x00,0x00,0x20,0x00,
    0x01,0x00,0x00,0x00,0x00,0x00,0xE0,0x00,0x00,0x07,0x00,0x00,0x00,0x00,0x20,0x00,
    0x01,0x00,0x00,0x00,0x00,0x00,0x30,0x00,0x00,0x0C,0x00,0x00,0x00,0x00,0x20,0x00,
    0x01,0x00,0x00,0x00,0x00,0x00,0x1C,0x00,0x00,0x30,0x00,0x00,0x00,0x00,0x20,0x00,
    0x01,0x00,0x00,0x00,0x00,0x00,0x03,0x00,0x00,0xE0,0x00,0x00,0x00,0x00,0x60,0x00,
    0x02,0x00,0x00,0x00,0x00,0x00,0x00,0xE0,0x01,0x80,0x00,0x00,0x00,0x00,0x40,0x00,
    0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x38,0x06,0x00,0x00,0x00,0x00,0x00,0x40,0x00,
    0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x0E,0x18,0x00,0x00,0x00,0x00,0x00,0x40,0x00,
    0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x03,0xB0,0x00,0x00,0x00,0x00,0x00,0x40,0x00,
    0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xE0,0x00,0x00,0x00,0x00,0x00,0x40,0x00,
    0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x20,0x00,0x00,0x00,0x00,0x00,0x40,0x00,
    0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x40,0x00,
    0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x40,0x00,
    0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x40,0x00,
    0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x40,0x00,
    0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x40,0x00,
    0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x40,0x00,
    0x02,0x00,0x07,0xFF,0xFF,0xFF,0xFF,0xFF,0xF8,0x00,0x00,0x00,0x00,0x00,0x60,0x00,
    0x02,0x01,0xFC,0x00,0x00,0x00,0x00,0x00,0x0F,0xF0,0x00,0x00,0x00,0x00,0x20,0x00,
    0x01,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x1F,0xC0,0x00,0x00,0x00,0x20,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x3F,0xC0,0x00,0x3F,0xE0,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x3F,0xFF,0xC0,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
};

int main(int argc,char *argv[])
{
    int fd,i,ret;
    int screen_size,screen_width,screen_height;
    struct fb_var_screeninfo var_info;
    unsigned char *mem_base;
    unsigned char *map_buf;
    // char temp_buf[16] = {0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff};

    fd = open(argv[1],O_RDWR);
    if(fd < 0){
        perror("open failed!\n");
        return errno;
    }

    ret = ioctl(fd,FBIOGET_VSCREENINFO,&var_info);
    if(ret < 0){
        perror("ioctl failed!\n");
        return errno;
    }
    screen_width = var_info.xres;
    screen_height = var_info.yres;
    screen_size = screen_width * screen_height * var_info.bits_per_pixel / 8;
    fprintf(stderr,"screen_width:%d,screen_height:%d,screen_size:%d\n",screen_width,screen_height,screen_size);
    mem_base = mmap(NULL,screen_size,PROT_READ | PROT_WRITE,MAP_SHARED,fd,0);
    if(mem_base == MAP_FAILED){
        perror("map failed\n");
        fprintf(stderr,"screen_size:%d\n",screen_size);
        close(fd);
        return errno;
    }
    // close(fd);
    map_buf = mem_base;
    // memcpy(mem_base,temp_buf,sizeof(temp_buf));
    // sleep(3);
    // printf("read in user:%x %x %x %x\n",mem_base[0],mem_base[1],mem_base[2],mem_base[3]);
    // printf("read in user:%x %x %x %x\n",mem_base[4],mem_base[5],mem_base[6],mem_base[7]);
    // printf("write in user:%x %x %x %x\n",temp_buf[0],temp_buf[1],temp_buf[2],temp_buf[3]);
    // printf("write in user:%x %x %x %x\n",temp_buf[4],temp_buf[5],temp_buf[6],temp_buf[7]);
    for(i = 0 ; i < sizeof(temp_buf) ; i++){
        *map_buf++ = temp_buf[i];
    }
    return 0;
}