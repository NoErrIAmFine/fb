#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/gpio/consumer.h>
#include <linux/fb.h>
#include <linux/vmalloc.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/mm.h>
#include <asm/uaccess.h>

/* 这块内存将会被映射到用户空间,一位表示一个点，0表示灭，非0表示亮 */
/* 4k,因为映射是以页为单位的,最小都得4k,我实在找不到只映射几十kb的方法 */
#define VIDEOMEMSIZE (4 * 1024)     

static void *video_mem;
static unsigned long video_mem_size = VIDEOMEMSIZE;

struct digital_tube_dev
{
    struct gpio_desc *shcp_gpio;
    struct gpio_desc *stcp_gpio;
    struct gpio_desc *data1_gpio;
    struct gpio_desc *data2_gpio;
    /* 使用fb_info纯粹是为了熟悉该框架 */
    struct fb_info *fb_info;
    struct platform_device *pdev;
    struct timer_list timer;
    u8 *row_map;
    u8 *col_map;
    bool is_mapped;
};

struct digital_tube_dev *d_tube_dev;
static u8 row_map[8] = {15,10,0,12,7,1,6,3};
static u8 col_map[8] = {11,5,4,14,2,13,9,8};

static void digital_tube_write_short(struct digital_tube_dev *dev,u16 data1,u16 data2)
{
    struct gpio_desc *shcp = dev->shcp_gpio;
    struct gpio_desc *stcp = dev->stcp_gpio;
    struct gpio_desc *data1_gpio = dev->data1_gpio;
    struct gpio_desc *data2_gpio = dev->data2_gpio;

    int i = 0;
    // printk("enter:%s\n",__func__);
    gpiod_set_value(shcp,0);
    gpiod_set_value(stcp,0);
    for(i = 0 ; i < 16 ; i++){
        if(data1 & 0x8000){
            gpiod_set_value(data1_gpio,1);
        }else{
            gpiod_set_value(data1_gpio,0);
        }
        if(data2 & 0x8000){
            gpiod_set_value(data2_gpio,1);
        }else{
            gpiod_set_value(data2_gpio,0);
        }
        /* shcp引脚上升沿时对数据取样，并移位 */
        gpiod_set_value(shcp,1);
        
        gpiod_set_value(shcp,0);
        data1 <<= 1;
        data2 <<= 1;
    }
    /* 所有数据移位完毕，锁存数据 */
    gpiod_set_value(stcp,1);
}

static void flush_d_tube_fb(struct digital_tube_dev *d_tube)
{
    int row,col;
    u16 data1;
    u16 data2;
    u8 *row_map = d_tube->row_map;
    u8 *col_map = d_tube->col_map;
    u8 *mem;
    mem = d_tube->fb_info->screen_base;
    
    // printk("enter:%s\n",__func__);
    // printk("mem0:%x,mem1:%x,mem2:%x,mem3:%x\n",mem[0],mem[1],mem[2],mem[3]);
    // printk("mem4:%x,mem5:%x,mem6:%x,mem7:%x\n",mem[4],mem[5],mem[6],mem[7]);
    // printk("mem8:%x,mem9:%x,mem10:%x,mem11:%x\n",mem[8],mem[9],mem[10],mem[11]);
    // printk("mem12:%x,mem13:%x,mem14:%x,mem15:%x\n",mem[12],mem[13],mem[14],mem[15]);
    // for(row = 0 ; row < 8 ; row++){
    //     data1 = (1u << row_map[row]);
    //     data2 = (1u << row_map[row]);
    //     for(col = 0 ; col < 8 ; col++){
    //         if(!(*(mem + 2 * row) & (1u << col)))
    //             data1 |= (1u << col_map[col]);
    //         if(!(*(mem + 2 * row + 1) & (1u << col)))
    //             data2 |= (1u << col_map[col]);
    //     }
    //     digital_tube_write_short(d_tube,data1,data2);
    //     udelay(10);     //为了提高亮度,延时一会儿
    // }
    /* 为保持亮度一致，刷新完一帧后清零 */
    // digital_tube_write_short(d_tube,0,0);
}


static void d_tube_timer_func(unsigned long data)
{
    struct digital_tube_dev *d_tube = (struct digital_tube_dev *)data;
    /* 该函数每20微妙执行一次 */
    unsigned long expire = jiffies + 10;
    // u8 *mem = d_tube->fb_info->screen_base;
    u8 *mem = video_mem;
    // printk("enter:%s\n",__func__);
    printk("mem0:%x,mem1:%x,mem2:%x,mem3:%x\n",mem[0],mem[1],mem[2],mem[3]);
    printk("mem4:%x,mem5:%x,mem6:%x,mem7:%x\n",mem[4],mem[5],mem[6],mem[7]);
    printk("mem8:%x,mem9:%x,mem10:%x,mem11:%x\n",mem[8],mem[9],mem[10],mem[11]);
    printk("mem12:%x,mem13:%x,mem14:%x,mem15:%x\n",mem[12],mem[13],mem[14],mem[15]);
    flush_d_tube_fb(d_tube);
    
    /* 重新开始计时 */
    if(time_after_eq(expire,jiffies)){
        d_tube->timer.expires = expire;
        add_timer(&d_tube->timer);
    }else{
        d_tube->timer.expires = jiffies;
        add_timer(&d_tube->timer);
    }
}

static int d_tube_fb_open(struct fb_info *info, int user)
{
    return 0;
}

static int d_tube_fb_release(struct fb_info *info, int user)
{
    return 0;
}

static ssize_t d_tube_fb_write(struct fb_info *info, const char __user *buf, size_t count, loff_t *ppos)
{
    struct digital_tube_dev *d_tube = info->par;
    unsigned long p = *ppos;
    int err = 0;
    void *dst;
    unsigned long total_size;

    if(info->state != FBINFO_STATE_RUNNING)
        return -EPERM;
    
    total_size = info->screen_size;

    if(!total_size)
        total_size = info->fix.smem_len;
    
    if(p > total_size)
        return -EFBIG;
    
    if(count > total_size){
        err = -EFBIG;
        count = total_size;
    }
    
    if((count + p) > total_size){
        if(!err)
            err = -EFBIG;
        count = total_size - p;
    }

    dst = (void __force *)(info->screen_base + p);

    if(info->fbops->fb_sync)
        info->fbops->fb_sync(info);
    
    if(copy_from_user(dst,buf,count))
        err = -EFAULT;
    
    if(!err)
        (*ppos)+=count;
    
    /* 如果已经映射了内存,则什么也不用做,因为稍后内存会自动同步到oled上的 */
    if(d_tube->is_mapped){
        return err ? err : count;
    }else{  //否则手动刷新缓存
        // flush_d_tube_fb(d_tube);
    }
    return err ? err : count;
}

static int d_tube_fb_mmap(struct fb_info *info, struct vm_area_struct *vma)
{
    struct digital_tube_dev *d_tube = info->par;
    unsigned long start = vma->vm_start;
    unsigned long size  = vma->vm_end - vma->vm_start;
    unsigned long offset = (vma->vm_pgoff) << PAGE_SHIFT;
    unsigned long page,pos;

    if(d_tube->is_mapped){
        return 0;
    }

    if(vma->vm_pgoff > (~0UL >> PAGE_SHIFT))
        return -EINVAL;
    if(size > info->fix.smem_len)
        return -EINVAL;
    if((offset + size) > info->fix.smem_len)
        return -EINVAL;
    
    /* 必须用这个,fix中存的是最起始的物理地址 */
    pos = (unsigned long)(info->screen_base + offset);

    while(size > 0){
        page = vmalloc_to_pfn((void *)pos);
        // page = virt_to_pfn(pos);
        if(remap_pfn_range(vma,start,page,PAGE_SIZE,PAGE_SHARED)){
            return -EAGAIN;
        }
        start += PAGE_SIZE;
        pos += PAGE_SIZE;
        if(size > PAGE_SIZE)
            size -= PAGE_SIZE;
        else
            size = 0;
    }

    /* 将定时器加入系统开始调度执行 */
    // d_tube->timer.expires = jiffies + HZ;
    // add_timer(&d_tube->timer);
    // d_tube->is_mapped = 1;
    return 0;
}

static struct fb_ops d_tube_fb_ops = {
    .owner      = THIS_MODULE,
    .fb_open    = d_tube_fb_open,
    .fb_release = d_tube_fb_release,
    .fb_write   = d_tube_fb_write,
    .fb_mmap    = d_tube_fb_mmap,
    .fb_copyarea  = cfb_copyarea,
    .fb_fillrect  = cfb_fillrect,
    .fb_imageblit = cfb_imageblit,
};

static void *rvmalloc(unsigned long size)
{
    unsigned long addr;
    void *mem;

    if(!size)
        return NULL;

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

static int d_tube_probe(struct platform_device *pdev)
{   
    int ret = -ENOMEM;
    struct timer_list *timer;
    struct digital_tube_dev *d_tube;
    struct fb_info *info;
    
    /* 分配struct digital_tube_dev */
    d_tube = devm_kzalloc(&pdev->dev,sizeof(struct digital_tube_dev),GFP_KERNEL);
    if(!d_tube){
        dev_err(&pdev->dev,"allocate d_tube_dev failed!\n");
        return ret;
    }
    platform_set_drvdata(pdev,d_tube);
    d_tube->pdev = pdev;
    
    /* 获取用到的gpio */
    d_tube->shcp_gpio = gpiod_get(&pdev->dev,"shcp",GPIOD_OUT_LOW);
    if((ret = IS_ERR(d_tube->shcp_gpio)))
        goto release_d_tube;
    d_tube->stcp_gpio = gpiod_get(&pdev->dev,"stcp",GPIOD_OUT_LOW);
    if((ret = IS_ERR(d_tube->stcp_gpio)))
        goto put_shcp;
    d_tube->data1_gpio = gpiod_get(&pdev->dev,"data1",GPIOD_OUT_LOW);
    if((ret = IS_ERR(d_tube->data1_gpio)))
        goto put_stcp;
    d_tube->data2_gpio = gpiod_get(&pdev->dev,"data2",GPIOD_OUT_LOW);
    if((ret = IS_ERR(d_tube->data2_gpio)))
        goto put_data1;
    
    /* 分配fb设备 */
    d_tube->fb_info = framebuffer_alloc(0,&pdev->dev);
    if(!d_tube->fb_info){
        dev_err(&pdev->dev,"allocate d_tube_dev failed!\n");
        goto put_data2;
    }
    d_tube->fb_info->par = d_tube;

    /* 填充fb_info */
    info = d_tube->fb_info;
    video_mem = rvmalloc(video_mem_size);
    // video_mem = kzalloc(video_mem_size,GFP_KERNEL);
    if(!video_mem){
        dev_err(&pdev->dev,"allocate video mem failed!\n");
        goto release_fb_info;
    }
    /* 内存对应的物理地址 */
    info->fix.smem_start = page_to_phys(vmalloc_to_page(video_mem)) + offset_in_page(video_mem);
    // info->fix.smem_start = virt_to_phys(video_mem);
    info->fix.smem_len = video_mem_size;    //所用内存的实际长度
    info->fix.line_length = 2;              //一行16个灯,没位对应一个,只需两个字节
    info->fix.visual = FB_VISUAL_MONO10;

    info->var.xres = 16;
    info->var.yres = 8;
    info->var.bits_per_pixel = 1;

    info->screen_base = video_mem;
    info->screen_size = info->fix.line_length * info->var.yres;
    info->fbops = &d_tube_fb_ops;

    /* 注册fb设备 */
    ret = register_framebuffer(info);
    if(ret){
        dev_err(&pdev->dev,"register framebuffer failed!\n");
        goto release_video_mem;
    }

    /* 初始化digital_tube_dev的一些其他成员 */
    d_tube->row_map = row_map;
    d_tube->col_map = col_map;
    
    /* 初始化定时器,注意,此时只是初始化,并未加入执行 */
    timer = &d_tube->timer;
    init_timer(timer);
    timer->data     = (unsigned long)d_tube;
    timer->function = d_tube_timer_func;
    
    return 0;

release_video_mem:
    rvfree(info->screen_base,info->fix.smem_len);
release_fb_info:
    framebuffer_release(d_tube->fb_info);
put_data2:
    gpiod_put(d_tube->data2_gpio);
put_data1:
    gpiod_put(d_tube->data1_gpio);
put_stcp:
    gpiod_put(d_tube->stcp_gpio);
put_shcp:
    gpiod_put(d_tube->shcp_gpio);
release_d_tube:
    devm_kfree(&pdev->dev,d_tube);
    platform_set_drvdata(pdev,NULL);
    return ret;
}

static int d_tube_remove(struct platform_device *pdev)
{
    struct digital_tube_dev *d_tube;
    struct fb_info *info;

    d_tube = platform_get_drvdata(pdev);
    platform_set_drvdata(pdev,NULL);
    info = d_tube->fb_info;

    rvfree(info->screen_base,info->fix.smem_len);
    unregister_framebuffer(info);
    framebuffer_release(info);

    gpiod_put(d_tube->shcp_gpio);
    gpiod_put(d_tube->stcp_gpio);
    gpiod_put(d_tube->data1_gpio);
    gpiod_put(d_tube->data2_gpio);

    del_timer(&d_tube->timer);

    devm_kfree(&pdev->dev,d_tube);

    return 0;
}


const static struct of_device_id d_tube_of_match_table[] = {
    {
        .compatible = "digital_tube,16x8",
    },
    {}
};

const static struct platform_device_id d_tube_id_table[] = {
    {"digital_tube,16x8",0},
    {}
};

static struct platform_driver d_tube_driver = {
    .driver = {
        .name = "digital tube driver",
        .of_match_table = d_tube_of_match_table,
        .owner = THIS_MODULE,
    },
    .probe = d_tube_probe,
    .remove = d_tube_remove,
    .id_table = d_tube_id_table,
};

module_platform_driver(d_tube_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("luo");