#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/gpio/consumer.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/errno.h>
#include <asm/uaccess.h>

#include "ssd1306_oled.h"

#define VIDEOMEMSIZE (4 * 1024)     /* 4k,因为一页最小,反正最后会被延长到4k */
#define SSD_FLUSH_INTERVAL (10)     /* 将内容刷新到屏幕上的时间间隔，单位：ms */

static void *video_mem;
static u_long video_mem_size = VIDEOMEMSIZE;

struct ssd1306_dev
{
    struct fb_info *info;
    struct i2c_client *client;
    struct timer_list timer;
    struct work_struct work;
    struct workqueue_struct *wqueue;
    bool is_mapped;
};

static struct ssd1306_dev ssd1306_dev;

static void *rvmalloc(unsigned long size)
{
    void *mem;
    unsigned long addr;
    
    size = PAGE_ALIGN(size);
    mem = vmalloc(size);
    if(!mem){
        return NULL;
    }
    
    memset(mem,0,size);     //防止数据泄露到用户空间
    
    addr = (unsigned long)mem;
    
    while((long)size > 0){
        SetPageReserved(vmalloc_to_page((void *)addr));
        addr += PAGE_SIZE;
        size -= PAGE_SIZE;
    }

    return mem;
}

static void rvfree(void *mem,unsigned long size)
{
    unsigned long addr;

    if(!mem)
        return;

    addr = (unsigned long)mem;

    while((long)size > 0){
        ClearPageReserved(vmalloc_to_page((void *)addr));
        addr += PAGE_SIZE;
        size -= PAGE_SIZE;
    }

    vfree(mem);
}

/* 写入数据 */
static int ssd1306_write_data(struct i2c_client *client,const u8 *buf,int len)
{
    struct i2c_msg msg;
    int ret,retries;
    u8 wbuf[len + 1];

    /* 需要验证 */
    wbuf[0] = 0x40;
    memcpy(&wbuf[1],buf,len);
    
    msg.addr = 0x3c;
    msg.flags = 0;
    msg.buf = wbuf;
    msg.len = len + 1;
    
    ret = i2c_transfer(client->adapter,&msg,1);
    
    if(ret != 1){
        retries = 0;
        while(++retries < 5){
            dev_err(&client->dev,"transfer failed! %d times retry\n",retries);
            ret = i2c_transfer(client->adapter,&msg,1);
            if(ret == 1)
                return 0;
        }
        return ret;
    }
    
    return 0;
}

/* 写入命令 */
static int ssd1306_write_cmd(struct i2c_client *client,const u8 *buf,int len)
{
    struct i2c_msg msg;
    int ret,retries;
    u8 wbuf[len + 1];
    int i;
    wbuf[0] = 0x00;
    memcpy(&wbuf[1],buf,len);
    
    // msg.addr = client->addr;
    msg.addr = 0x3c;
    msg.flags = 0;
    msg.buf = wbuf;
    msg.len = len + 1;
    
    for(i = 0 ; i < 2 ; i++){
        if(i == 1){
            printk("second times use slave addr : 0x7a\n");
            msg.addr = 0x3d;
        }
        
        ret = i2c_transfer(client->adapter,&msg,1);
        
        if(ret != 1){
            retries = 0;
            while(++retries < 5){
                dev_err(&client->dev,"transfer failed! error no is %d,%d times retry",ret,retries);
                ret = i2c_transfer(client->adapter,&msg,1);
                if(ret == 1)
                    return 0;
            }
            // return ret;
        }else{
            return 0;
        }
    }
    return 0;
}

static int ssd1306_dev_init(struct i2c_client *client)
{
    // int i;
    char cmd_buf[] = {
        CMD_SET_ADDR_MODE,0x00,CMD_SET_COL_ADDR,0,0x7f,CMD_SET_PAGE_ADDR,0,0x07
    };
    
    // ssd1306_write_cmd(client,"\x00",1);
    // ssd1306_write_cmd(client,"\x10",1);
    ssd1306_write_cmd(client,cmd_buf,sizeof(cmd_buf));
    ssd1306_write_cmd(client,"\x40",1);
    // ssd1306_write_cmd(client,"\xb0",1);
    ssd1306_write_cmd(client,"\x81",1);
    ssd1306_write_cmd(client,"\xff",1);
    ssd1306_write_cmd(client,"\xa1",1);
    ssd1306_write_cmd(client,"\xa6",1);
    ssd1306_write_cmd(client,"\xa8",1);
    ssd1306_write_cmd(client,"\x3f",1);
    ssd1306_write_cmd(client,"\xc8",1);
    ssd1306_write_cmd(client,"\xd3",1);
    ssd1306_write_cmd(client,"\x00",1);

    ssd1306_write_cmd(client,"\xd5",1);
    ssd1306_write_cmd(client,"\x80",1);

    ssd1306_write_cmd(client,"\xd8",1);
    ssd1306_write_cmd(client,"\x05",1);

    ssd1306_write_cmd(client,"\xd9",1);
    ssd1306_write_cmd(client,"\xf1",1);

    ssd1306_write_cmd(client,"\xda",1);
    ssd1306_write_cmd(client,"\x12",1);

    ssd1306_write_cmd(client,"\xdb",1);
    ssd1306_write_cmd(client,"\x30",1);

    ssd1306_write_cmd(client,"\x8d",1);
    ssd1306_write_cmd(client,"\x14",1);

    ssd1306_write_cmd(client,"\xaf",1);
    // for(i = 0 ; i < 128 ; i++){
    //     ssd1306_write_data(client,"\xff",1);
    // }
    return 0;
}

static int ssd1306_dev_exit(struct i2c_client *client)
{
    ssd1306_write_cmd(client,"\xae",1);
    return 0;
}

static void ssd1306_timer_func(unsigned long data)
{
    struct ssd1306_dev *ssd1306 = (struct ssd1306_dev*)data;
    
    /* 在此函数中只将work_struct加入队列，其他耗时的操作在工作函数中完成 */
    queue_work(ssd1306->wqueue,&ssd1306->work);
}

/* 将屏幕清0 */
static void ssd1306_clear(struct ssd1306_dev *ssd1306)
{
    struct i2c_client *client = ssd1306->client;
    struct fb_info *info = ssd1306->info;
    int screen_size = info->var.xres * info->var.yres / 8;
    char temp_buf[screen_size];

    memset(temp_buf,0,sizeof(temp_buf));
    ssd1306_write_data(client,temp_buf,sizeof(temp_buf));
}

/* 将内存上的内容同步到oled上 */
static void ssd1306_sync_buffer(struct ssd1306_dev *ssd1306)
{
    struct fb_info *info = ssd1306->info;
    struct i2c_client *client = ssd1306->client;
    unsigned char *smem_base;
    int screen_width,screen_height;
    int line_bytes,i,j,k,bits;
    int srceen_size = info->screen_size;
    char byte_data;
    char transfrom_data[srceen_size];
   
    smem_base = info->screen_base;
    if(!smem_base){
        smem_base = (unsigned char *)info->fix.smem_start;
    }
    if(!smem_base){
        return;
    }

    screen_width = info->var.xres;
    screen_height = info->var.yres;
    line_bytes = screen_width / 8;

    k = 0;
    for(i = 0 ; i < screen_height / 8 ; i++){
        for(j = 0 ; j < screen_width / 8 ; j++){
            bits = 0;
            for(bits = 0 ; bits < 8 ;bits++){
                byte_data = 0;
                byte_data |= ((*(smem_base + (i * 8 + 0) * line_bytes + j) << bits) & 0x80) >> 7;
                byte_data |= ((*(smem_base + (i * 8 + 1) * line_bytes + j) << bits) & 0x80) >> 6;
                byte_data |= ((*(smem_base + (i * 8 + 2) * line_bytes + j) << bits) & 0x80) >> 5;
                byte_data |= ((*(smem_base + (i * 8 + 3) * line_bytes + j) << bits) & 0x80) >> 4;
                byte_data |= ((*(smem_base + (i * 8 + 4) * line_bytes + j) << bits) & 0x80) >> 3;
                byte_data |= ((*(smem_base + (i * 8 + 5) * line_bytes + j) << bits) & 0x80) >> 2;
                byte_data |= ((*(smem_base + (i * 8 + 6) * line_bytes + j) << bits) & 0x80) >> 1;
                byte_data |= ((*(smem_base + (i * 8 + 7) * line_bytes + j) << bits) & 0x80) >> 0;
                transfrom_data[k++] = byte_data;
                // ssd1306_write_data(client,&byte_data,1);
            }
        }
    }

    ssd1306_write_data(client,transfrom_data,k);
}

static void ssd1306_work_func(struct work_struct *work)
{
    struct ssd1306_dev *ssd1306 = container_of(work,struct ssd1306_dev,work);
    unsigned long expire_time = jiffies + SSD_FLUSH_INTERVAL;
    
    ssd1306_sync_buffer(ssd1306);
   
    /* 重新启动定时器 */
    if(time_after_eq(expire_time,jiffies)){
        ssd1306->timer.expires = expire_time;
        add_timer(&ssd1306->timer);
    }else{
        ssd1306->timer.expires = jiffies;
        add_timer(&ssd1306->timer);
    }  
}

static int ssd1306_fb_open(struct fb_info *info, int user)
{
    struct ssd1306_dev *ssd1306 = info->par;
    struct i2c_client *client = ssd1306->client;
    
    ssd1306_dev_init(client);

    /* 初始化定时器，但并不在此函数内启动，在mmap函数中启动 */
    INIT_WORK(&ssd1306->work,ssd1306_work_func);
    init_timer(&ssd1306->timer);
    ssd1306->timer.function = ssd1306_timer_func;
    ssd1306->timer.data = (unsigned long)ssd1306;

    /* 将屏幕清0 */
    ssd1306_clear(ssd1306);

    return 0;
}

static int ssd1306_fb_release(struct fb_info *info, int user)
{
    // del_timer(&ssd1306_dev.timer);
    // ssd1306_dev_exit(ssd1306_dev.client);
    return 0;
}

ssize_t ssd1306_fb_write(struct fb_info *info, const char __user *buf,size_t count, loff_t *ppos)
{
    struct ssd1306_dev *ssd1306 = info->par;
    unsigned long p = *ppos;
	void *dst;
	int err = 0;
	unsigned long total_size;

	if (info->state != FBINFO_STATE_RUNNING)
		return -EPERM;

	total_size = info->screen_size;

	if (total_size == 0)
		total_size = info->fix.smem_len;

	if (p > total_size)
		return -EFBIG;

	if (count > total_size) {
		err = -EFBIG;
		count = total_size;
	}

	if (count + p > total_size) {
		if (!err)
			err = -ENOSPC;

		count = total_size - p;
	}

	dst = (void __force *) (info->screen_base + p);

	if (info->fbops->fb_sync)
		info->fbops->fb_sync(info);

	if (copy_from_user(dst, buf, count))
		err = -EFAULT;

	if  (!err)
		*ppos += count;

    /* 如果已经映射了内存,则什么也不用做,因为稍后内存会自动同步到oled上的 */
    if(ssd1306->is_mapped){
        return (err) ? err : count;;
    }else{
        ssd1306_sync_buffer(ssd1306);
    }

	return (err) ? err : count;
}

int ssd1306_fb_mmap(struct fb_info *info, struct vm_area_struct *vma)
{
    struct ssd1306_dev *ssd1306 = info->par;
    unsigned long start = vma->vm_start;
	unsigned long size = vma->vm_end - vma->vm_start;
	unsigned long offset = vma->vm_pgoff << PAGE_SHIFT;
	unsigned long page, pos;
    
    if(ssd1306->is_mapped){
        return 0;
    }

	if (vma->vm_pgoff > (~0UL >> PAGE_SHIFT))
		return -EINVAL;
	if (size > info->fix.smem_len)
		return -EINVAL;
	if (offset > info->fix.smem_len - size)
		return -EINVAL;

	pos = (unsigned long)info->fix.smem_start + offset;
    
	while (size > 0) {
		page = vmalloc_to_pfn((void *)pos);
		if (remap_pfn_range(vma, start, page, PAGE_SIZE, PAGE_SHARED)) {
			return -EAGAIN;
		}
		start += PAGE_SIZE;
		pos += PAGE_SIZE;
		if (size > PAGE_SIZE)
			size -= PAGE_SIZE;
		else
			size = 0;
	}

    ssd1306->timer.expires = jiffies + HZ;
    add_timer(&ssd1306->timer); 
    ssd1306->is_mapped = 1;

	return 0;
}

struct fb_ops ssd1306_fbops = {
    .owner          = THIS_MODULE,
    .fb_open        = ssd1306_fb_open,
    .fb_release     = ssd1306_fb_release,
    .fb_write       = ssd1306_fb_write,
    .fb_mmap        = ssd1306_fb_mmap,
    .fb_fillrect	= cfb_fillrect,
	.fb_copyarea	= cfb_copyarea,
	.fb_imageblit	= cfb_imageblit,
};

static int ssd1306_probe(struct i2c_client *client,const struct i2c_device_id *id)
{
    struct fb_info *info;
    int ret = -ENOMEM;
    
    if(!(video_mem = rvmalloc(video_mem_size)))
        return ret;

    info = framebuffer_alloc(0,&client->dev);
    if(!info)
        goto err;
    ssd1306_dev.info = info;
    info->par = &ssd1306_dev;
    ssd1306_dev.client = client;
    
    /* 设置fix参数 */
    strcpy(info->fix.id,"my oled");
    info->fix.smem_start = (unsigned long)video_mem;
    info->fix.smem_len   = video_mem_size;
    info->fix.type       = FB_TYPE_PACKED_PIXELS;
    info->fix.visual     = FB_VISUAL_MONO10;
    info->fix.line_length = 128 / 8;
    
    /* 设置var参数 */
    info->var.xres = 128;
    info->var.yres = 64;
    info->var.bits_per_pixel = 1;
    info->var.activate = FB_ACTIVATE_NXTOPEN;
    
     /* 设置info */
    info->screen_base = (void *__iomem)video_mem;
    /* 这里保存的是用到的显存的实际大小 */
    info->screen_size = info->fix.line_length * info->var.yres;

    /* 设置操作函数 */
    info->fbops = &ssd1306_fbops;
    
    /* 注册 */
    ret = register_framebuffer(info);
    if(ret < 0){
        goto err1;
    }
    
    i2c_set_clientdata(client,&ssd1306_dev);
    
    ssd1306_dev.wqueue = create_singlethread_workqueue("ssd1306_wqueue");
    if(!ssd1306_dev.wqueue){
        printk("create workqueue failed!\n");
        goto err1;
    }
    ssd1306_dev_init(client);
    return 0;
err1:
    unregister_framebuffer(info);
err:
    rvfree(video_mem,video_mem_size);
    return 0;
}

static int ssd1306_remove(struct i2c_client *client)
{   
    struct ssd1306_dev *ssd1306;
    struct fb_info *info;

    ssd1306_dev_exit(client);
    ssd1306 = i2c_get_clientdata(client);
    info = ssd1306->info;
    if(info){
        rvfree(video_mem,video_mem_size);
        unregister_framebuffer(info);
        framebuffer_release(info);
    }
    del_timer(&ssd1306->timer);
    destroy_workqueue(ssd1306->wqueue);
    return 0;
}

static const struct of_device_id ssd1306_of_match_table[] = {
    {
        .compatible = "ssd1306",
    },
    {}
};

static const struct i2c_device_id ssd1306_id_table[] = {
    {"ssd1306",0},
    {}
};

static struct i2c_driver ssd1306_driver = {
    .driver = {
        .name = "ssd1306_i2c_driver",
        .of_match_table = ssd1306_of_match_table,
        .owner = THIS_MODULE,
    },
    .probe = ssd1306_probe,
    .remove = ssd1306_remove,
    .id_table = ssd1306_id_table,
};

static int __init ssd1306_init(void)
{
    // printk("enter %s\n",__func__);
    i2c_add_driver(&ssd1306_driver);
    return 0;
}

static void __exit ssd1306_exit(void)
{   
    i2c_del_driver(&ssd1306_driver);
}

module_init(ssd1306_init);
module_exit(ssd1306_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("luo");