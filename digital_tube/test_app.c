#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

int main(int argc,char *argv[])
{
    int fd,i,j,ret;
    int screen_size,screen_width,screen_height;
    struct fb_var_screeninfo var_info;
    char *mem_base;
    char *map_buf;
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
    
    map_buf = mem_base;
    // j = 0;
    // while(1){
    //     for(i = 0 ; i < screen_size ; i++){
    //         map_buf[i] = j;
    //     }
    //     j++;
    //     if(10 == j)
    //         j = 0;
    // }
    
    // sleep(2);
    // fprintf(stderr,"**********************************************************************************\n");
    j = 1;
    while(1){
        for(i = 0 ; i < 160  ; i++){
            map_buf[i] = j++;
        }
        sleep(1);
        for(i = 0 ; i < 160 ; i++){
            printf("map_buf[%d]:%x\n",i,map_buf[i]);
        }
    }
   
        
    close(fd);
    return 0;
}