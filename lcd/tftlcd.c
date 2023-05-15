#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/fb.h>
#include <linux/pinctrl/consumer.h>
#include <linux/clk.h>
#include <linux/of.h>
#include <video/of_display_timing.h>
#include <video/videomode.h>
#include <linux/ioport.h>
#include <linux/interrupt.h>
#include <linux/regulator/consumer.h>
#include <linux/dma-mapping.h>
#include <linux/pm_runtime.h>
#include <linux/errno.h>
#include <linux/console.h>
#include <asm/io.h>
#include <stdbool.h>

//ioctl 命令
#define TFTLCD_WAIT_FOR_VSYNC   _IOW('F',0x20,u32)

#define LCDIF_BUS_WIDTH_8BIT    1
#define LCDIF_BUS_WIDTH_16BIT   0
#define LCDIF_BUS_WIDTH_18BIT   2
#define LCDIF_BUS_WIDTH_24BIT   3

#define FB_SYNC_OE_LOW_ACT		0x80000000
#define FB_SYNC_CLK_LAT_FALL	0x40000000

#define MIN_XRES   64
#define MIN_YRES   64

#define RED     0
#define GREEN   1
#define BLUE    2
#define TRANSP  3

#define REG_SET 0x04
#define REG_CLR 0x08

//寄存器地址偏移
#define LCDC_CTRL			    0x00
#define LCDC_CTRL1			    0x10
#define LCDC_CTRL2			    0x20
#define LCDC_TRANSFER_COUNT		0x30
#define LCDC_CUR_BUF			0x40
#define LCDC_NEXT_BUF		    0x50
#define LCDC_TIMING			    0x60
#define LCDC_VDCTRL0			0x70
#define LCDC_VDCTRL1			0x80
#define LCDC_VDCTRL2			0x90
#define LCDC_VDCTRL3			0xa0
#define LCDC_VDCTRL4			0xb0
#define LCDC_DVICTRL0			0xc0
#define LCDC_DVICTRL1			0xd0
#define LCDC_DVICTRL2			0xe0
#define LCDC_DVICTRL3			0xf0
#define LCDC_DVICTRL4			0x100
#define LCDC_V4_DATA			0x180
#define LCDC_V3_DATA			0x1b0
#define LCDC_V4_DEBUG0			0x1d0
#define LCDC_V3_DEBUG0			0x1f0

//寄存器地址中的位偏移
#define CTRL_SFTRST			    (1 << 31)
#define CTRL_CLKGATE			(1 << 30)
#define CTRL_BYPASS_COUNT		(1 << 19)
#define CTRL_VSYNC_MODE			(1 << 18)
#define CTRL_DOTCLK_MODE		(1 << 17)
#define CTRL_DATA_SELECT		(1 << 16)
#define CTRL_SET_BUS_WIDTH(x)	(((x) & 0x3) << 10)
#define CTRL_GET_BUS_WIDTH(x)	(((x) >> 10) & 0x3)
#define CTRL_SET_WORD_LENGTH(x)	(((x) & 0x3) << 8)
#define CTRL_GET_WORD_LENGTH(x)	(((x) >> 8) & 0x3)
#define CTRL_MASTER			    (1 << 5)
#define CTRL_DF16			    (1 << 3)
#define CTRL_DF18			    (1 << 2)
#define CTRL_DF24			    (1 << 1)
#define CTRL_RUN			    (1 << 0)

#define CTRL1_RECOVERY_ON_UNDERFLOW		(1 << 24)
#define CTRL1_FIFO_CLEAR				(1 << 21)
#define CTRL1_SET_BYTE_PACKAGING(x)		(((x) & 0xf) << 16)
#define CTRL1_GET_BYTE_PACKAGING(x)		(((x) >> 16) & 0xf)
#define CTRL1_OVERFLOW_IRQ_EN			(1 << 15)
#define CTRL1_UNDERFLOW_IRQ_EN			(1 << 14)
#define CTRL1_CUR_FRAME_DONE_IRQ_EN		(1 << 13)
#define CTRL1_VSYNC_EDGE_IRQ_EN			(1 << 12)
#define CTRL1_OVERFLOW_IRQ				(1 << 11)
#define CTRL1_UNDERFLOW_IRQ				(1 << 10)
#define CTRL1_CUR_FRAME_DONE_IRQ		(1 << 9)
#define CTRL1_VSYNC_EDGE_IRQ			(1 << 8)
#define CTRL1_IRQ_ENABLE_MASK			(CTRL1_OVERFLOW_IRQ_EN | \
						 CTRL1_UNDERFLOW_IRQ_EN | \
						 CTRL1_CUR_FRAME_DONE_IRQ_EN | \
						 CTRL1_VSYNC_EDGE_IRQ_EN)
#define CTRL1_IRQ_ENABLE_SHIFT			12
#define CTRL1_IRQ_STATUS_MASK			(CTRL1_OVERFLOW_IRQ | \
						 CTRL1_UNDERFLOW_IRQ | \
						 CTRL1_CUR_FRAME_DONE_IRQ | \
						 CTRL1_VSYNC_EDGE_IRQ)
#define CTRL1_IRQ_STATUS_SHIFT			8

#define CTRL2_OUTSTANDING_REQS__REQ_16		(3 << 21)

#define TRANSFER_COUNT_SET_VCOUNT(x)	(((x) & 0xffff) << 16)
#define TRANSFER_COUNT_GET_VCOUNT(x)	(((x) >> 16) & 0xffff)
#define TRANSFER_COUNT_SET_HCOUNT(x)	((x) & 0xffff)
#define TRANSFER_COUNT_GET_HCOUNT(x)	((x) & 0xffff)


#define VDCTRL0_ENABLE_PRESENT		(1 << 28)
#define VDCTRL0_VSYNC_ACT_HIGH		(1 << 27)
#define VDCTRL0_HSYNC_ACT_HIGH		(1 << 26)
#define VDCTRL0_DOTCLK_ACT_FALLING	(1 << 25)
#define VDCTRL0_ENABLE_ACT_HIGH		(1 << 24)
#define VDCTRL0_VSYNC_PERIOD_UNIT	(1 << 21)
#define VDCTRL0_VSYNC_PULSE_WIDTH_UNIT	(1 << 20)
#define VDCTRL0_HALF_LINE		    (1 << 19)
#define VDCTRL0_HALF_LINE_MODE		(1 << 18)
#define VDCTRL0_SET_VSYNC_PULSE_WIDTH(x) ((x) & 0x3ffff)
#define VDCTRL0_GET_VSYNC_PULSE_WIDTH(x) ((x) & 0x3ffff)

#define VDCTRL2_SET_HSYNC_PERIOD(x)	((x) & 0x3ffff)
#define VDCTRL2_GET_HSYNC_PERIOD(x)	((x) & 0x3ffff)

#define VDCTRL3_MUX_SYNC_SIGNALS	(1 << 29)
#define VDCTRL3_VSYNC_ONLY		    (1 << 28)
#define SET_HOR_WAIT_CNT(x)		    (((x) & 0xfff) << 16)
#define GET_HOR_WAIT_CNT(x)		    (((x) >> 16) & 0xfff)
#define SET_VERT_WAIT_CNT(x)		((x) & 0xffff)
#define GET_VERT_WAIT_CNT(x)		((x) & 0xffff)

#define VDCTRL4_SET_DOTCLK_DLY(x)	(((x) & 0x7) << 29) /* v4 only */
#define VDCTRL4_GET_DOTCLK_DLY(x)	(((x) >> 29) & 0x7) /* v4 only */
#define VDCTRL4_SYNC_SIGNALS_ON		(1 << 18)
#define SET_DOTCLK_H_VALID_DATA_CNT(x)	((x) & 0x3ffff)

struct tftlcd_info
{
    struct fb_info *fb_info;
    void __iomem *base;             //寄存器基地址
    struct platform_device *pdev;
    struct regulator *regulator;

    struct clk *clk_pix;
    struct clk *clk_axi;
    struct clk *clk_disp_axi;

    int id;
    unsigned int bus_width;
    unsigned int dotclk_delay;
    unsigned int sync;
    int cur_blank;
    int restore_blank;
    char disp_dev[32];

    bool clk_pix_enabled;
    bool clk_axi_enabled;
    bool clk_disp_axi_enabled;
    bool enabled;
    bool wait4vsync;

    struct completion vsync_complete;
    struct completion flip_complete;
    struct fb_var_screeninfo var;
};


static const struct fb_bitfield def_rgb565[] = {
	[RED] = {
		.offset = 11,
		.length = 5,
	},
	[GREEN] = {
		.offset = 5,
		.length = 6,
	},
	[BLUE] = {
		.offset = 0,
		.length = 5,
	},
	[TRANSP] = {	/* no support for transparency */
		.length = 0,
	}
};

static const struct fb_bitfield def_rgb666[] = {
	[RED] = {
		.offset = 16,
		.length = 6,
	},
	[GREEN] = {
		.offset = 8,
		.length = 6,
	},
	[BLUE] = {
		.offset = 0,
		.length = 6,
	},
	[TRANSP] = {	/* no support for transparency */
		.length = 0,
	}
};

static const struct fb_bitfield def_rgb888[] = {
	[RED] = {
		.offset = 16,
		.length = 8,
	},
	[GREEN] = {
		.offset = 8,
		.length = 8,
	},
	[BLUE] = {
		.offset = 0,
		.length = 8,
	},
	[TRANSP] = {	/* no support for transparency */
		.length = 0,
	}
};

#define bitfield_is_equal(f1, f2)  (!memcmp(&(f1), &(f2), sizeof(f1)))

static inline bool pixfmt_is_equal(struct fb_var_screeninfo *var,
				   const struct fb_bitfield *f)
{
	if (bitfield_is_equal(var->red, f[RED]) &&
	    bitfield_is_equal(var->green, f[GREEN]) &&
	    bitfield_is_equal(var->blue, f[BLUE]))
		return true;

	return false;
}

static inline u32 get_hsync_pulse_width(u32 val)
{
    return (val >> 18) & 0x3fff;
}

static inline u32 set_hsync_pulse_width(u32 val)
{
    return ((val & 0x3fff) << 18);
}

static inline void clk_enable_axi(struct tftlcd_info *host)
{
    if(!host->clk_axi_enabled && host->clk_axi){
        clk_prepare_enable(host->clk_axi);
        host->clk_axi_enabled = true;
    }
}

static inline void clk_disable_axi(struct tftlcd_info *host)
{
    if(host->clk_axi_enabled && host->clk_axi){
        clk_disable_unprepare(host->clk_axi);
        host->clk_axi_enabled = false;
    }
}

static inline void clk_enable_disp_axi(struct tftlcd_info *host)
{
    if(!host->clk_disp_axi_enabled && host->clk_disp_axi){
        clk_prepare_enable(host->clk_disp_axi);
        host->clk_disp_axi_enabled = true;
    }
}

static inline void clk_disable_disp_axi(struct tftlcd_info *host)
{
    if(host->clk_disp_axi_enabled && host->clk_disp_axi){
        clk_disable_unprepare(host->clk_disp_axi);
        host->clk_disp_axi_enabled = false;
    }
}

static inline void clk_enable_pix(struct tftlcd_info *host)
{
    if(!host->clk_pix_enabled && host->clk_pix){
        clk_prepare_enable(host->clk_pix);
        host->clk_pix_enabled = true;
    }
}

static inline void clk_disable_pix(struct tftlcd_info *host)
{
    if(host->clk_pix_enabled && host->clk_pix){
        clk_disable_unprepare(host->clk_pix);
        host->clk_pix_enabled = false;
    }
}

static void tftlcd_enable_controller(struct fb_info *fb_info)
{
    struct tftlcd_info *host = fb_info->par;
    u32 reg;
    int ret;

    if(host->regulator){
        ret = regulator_enable(host->regulator);
        if(ret){
            dev_err(&host->pdev->dev,"lcd enable regulator failed,errno is:%d\n",ret);
            return;
        }
    }

    /* 在设置时钟频率之前必须先将时钟关闭 */
    clk_disable_pix(host);
    ret = clk_set_rate(host->clk_pix,PICOS2KHZ(fb_info->var.pixclock) * 1000U);
    if(ret){
        dev_err(&host->pdev->dev,"lcd pixel rate set failed,errno is:%d\n",ret);
        if(host->regulator){
            ret = regulator_disable(host->regulator);
            if(ret){
                dev_err(&host->pdev->dev,"lcd disable regulator failed,errno is:%d\n",ret);
            }
        }
        return;
    }
    clk_enable_pix(host);

    writel(CTRL2_OUTSTANDING_REQS__REQ_16,host->base + LCDC_CTRL2 + REG_SET);

    /* 重新启动模式 */
    writel(CTRL_DOTCLK_MODE,host->base + LCDC_CTRL + REG_SET);

    /* 先使能同步信号，然后使能DMA */
    reg = readl(host->base + LCDC_VDCTRL4);
    reg |= VDCTRL4_SYNC_SIGNALS_ON;
    writel(reg,host->base + LCDC_VDCTRL4);

    writel(CTRL_MASTER,host->base + LCDC_CTRL + REG_SET);
    writel(CTRL_RUN,host->base + LCDC_CTRL + REG_SET);

    writel(CTRL1_RECOVERY_ON_UNDERFLOW,host->base +LCDC_CTRL1 + REG_SET);

    host->enabled = 1;
}

static void tftlcd_disable_controller(struct fb_info *fb_info)
{
    struct tftlcd_info *host = fb_info->par;
    u32 reg,loop;
    int ret;

    /* 即使设置了相应标志位，只有当前数据传输完成后才会退出，并将run标志位置0 */
    writel(CTRL_DOTCLK_MODE,host->base + LCDC_CTRL + REG_CLR);

    loop = 1000;
    while(loop){
        reg = readl(host->base + LCDC_CTRL + REG_CLR);
        if(!(reg & CTRL_RUN))
            break;
        loop--;
    }

    writel(CTRL_MASTER,host->base + LCDC_CTRL + REG_CLR);

    reg = readl(host->base + LCDC_VDCTRL4);
    writel(reg & (~VDCTRL4_SYNC_SIGNALS_ON),host->base + LCDC_VDCTRL4);

    host->enabled = 0;

    if(host->regulator){
        ret = regulator_disable(host->regulator);
        if(ret){
            dev_err(&host->pdev->dev,"disable regulator failed!\n");
        }
    }
}

static int tftlcd_check_var(struct fb_var_screeninfo *var,struct fb_info *fb_info)
{
    struct tftlcd_info *host = fb_info->par;
    const struct fb_bitfield *rgb = NULL;

    if(var->xres < MIN_XRES)
        var->xres = MIN_XRES;
    if(var->yres < MIN_YRES)
        var->yres = MIN_YRES;
    
    if(var->xres_virtual > var->xres){
        dev_dbg(&host->pdev->dev,"not support stride!\n");
        return -EINVAL;
    }

    if(var->xres_virtual < var->xres)
        var->xres_virtual = var->xres;
    if(var->yres_virtual < var->yres)
        var->yres_virtual = var->yres;
    
    if((32 != var->bits_per_pixel) && (16 != var->bits_per_pixel)){
        var->bits_per_pixel = 32;
    }

    switch(var->bits_per_pixel){
        case 16:
            rgb = def_rgb565;
            break;
        
        case 32:
            switch(host->bus_width){
                case LCDIF_BUS_WIDTH_8BIT:
                    pr_debug("unsupported lcd bus width map\n");
                    return -EINVAL;
                case LCDIF_BUS_WIDTH_16BIT:
                    rgb = def_rgb666;
                    break;
                case LCDIF_BUS_WIDTH_18BIT:
                    if(pixfmt_is_equal(var,def_rgb666)){
                        rgb = def_rgb666;
                    }else{
                        rgb = def_rgb888;
                    }
                case LCDIF_BUS_WIDTH_24BIT:
                    rgb = def_rgb888;
                    break;
            }
            break;
        
        default:
            dev_dbg(&host->pdev->dev,"unsupported color depth!\n");
            return -EINVAL;
    }

    var->red    = rgb[RED];
    var->green  = rgb[GREEN];
    var->blue   = rgb[BLUE];
    var->transp = rgb[TRANSP];

    return 0;
}

/* 检查参数是否相同，如果不同则重新进行初始化 */
static bool tftlcd_par_equal(struct fb_info *fb_info,struct tftlcd_info *host)
{
    /* 忽略xoffset、yoffset，比较 var */
    struct fb_var_screeninfo old_var = host->var;
    struct fb_var_screeninfo new_var = fb_info->var;

    if(((fb_info->var.activate & FB_ACTIVATE_MASK) == FB_ACTIVATE_NOW) && \
        (fb_info->var.activate & FB_ACTIVATE_FORCE))
        return false;
    
    old_var.xoffset = new_var.xoffset = 0;
    old_var.yoffset = new_var.yoffset = 0;
    return memcmp(&old_var,&new_var,sizeof(struct fb_var_screeninfo)) == 0;
}

static int tftlcd_set_par(struct fb_info *fb_info)
{
    struct tftlcd_info *host = fb_info->par;
    static u32 equal_bypass = 0;
    u32 ctrl,vdctrl0,vdctrl4;
    int line_size,fb_size;
    int reenable = 0;

    if(equal_bypass > 1){
        if(tftlcd_par_equal(fb_info,host))
            return 0;
    }else{
        equal_bypass++;
    }

    /* 如果当前为blank模式，暂无必要检查，等到unblank也不迟 */
    if(host->cur_blank != FB_BLANK_UNBLANK)
        return 0;
        
    line_size = fb_info->var.xres * (fb_info->var.bits_per_pixel >> 8);
    fb_info->fix.line_length = line_size;
    fb_size = line_size * fb_info->var.yres_virtual;

    if(fb_size > fb_info->fix.smem_len){
        dev_err(&host->pdev->dev,"exceeds the fb buffer size limit!\n");
        return -ENOMEM;
    }
    
    if(host->enabled){
        reenable = 1;
        tftlcd_disable_controller(fb_info);
    }

    /* 清理fifo */
    writel(CTRL1_FIFO_CLEAR,host->base + LCDC_CTRL1 + REG_SET);

    ctrl = CTRL_BYPASS_COUNT | CTRL_MASTER | CTRL_SET_BUS_WIDTH(host->bus_width);
    switch(fb_info->var.bits_per_pixel){
        case 16:
            dev_dbg(&host->pdev->dev,"setting up rgb565 mode!\n");
            ctrl |= CTRL_SET_WORD_LENGTH(0);
            writel(CTRL1_SET_BYTE_PACKAGING(0xf),host->base + LCDC_CTRL1);
            break;
        case 32:
            dev_dbg(&host->pdev->dev,"setting up rgb888/rgb666 mode!\n");
            ctrl |= CTRL_SET_WORD_LENGTH(3);
            switch(host->bus_width){
                case LCDIF_BUS_WIDTH_8BIT:
                    dev_dbg(&host->pdev->dev,"unsupported bus width mapping!\n");
                    return -EINVAL;
                case LCDIF_BUS_WIDTH_16BIT:
                    ctrl |= CTRL_DF24;
                    break;
                case LCDIF_BUS_WIDTH_18BIT:
                    if(pixfmt_is_equal(&fb_info->var,def_rgb666));
                        ctrl |= CTRL_DF24;
                case LCDIF_BUS_WIDTH_24BIT:
                    break;
            }
            writel(CTRL1_SET_BYTE_PACKAGING(0x7),host->base + LCDC_CTRL1);
            break;
        default:
            dev_dbg(&host->pdev->dev,"unhandled color depth of %u\n",fb_info->var.bits_per_pixel);
            return -EINVAL;
    }

    writel(ctrl,host->base + LCDC_CTRL);
    writel(TRANSFER_COUNT_SET_HCOUNT(fb_info->var.xres) | TRANSFER_COUNT_SET_VCOUNT(fb_info->var.yres),host->base + LCDC_TRANSFER_COUNT);

    /* 总是处于dotclk模式 */
    vdctrl0 = VDCTRL0_ENABLE_PRESENT | VDCTRL0_VSYNC_PERIOD_UNIT | \
              VDCTRL0_VSYNC_PULSE_WIDTH_UNIT | VDCTRL0_SET_VSYNC_PULSE_WIDTH(fb_info->var.vsync_len);
    /* use the saved sync to avoid wrong sync information */
	if (host->sync & FB_SYNC_HOR_HIGH_ACT)
		vdctrl0 |= VDCTRL0_HSYNC_ACT_HIGH;
	if (host->sync & FB_SYNC_VERT_HIGH_ACT)
		vdctrl0 |= VDCTRL0_VSYNC_ACT_HIGH;
	if (!(host->sync & FB_SYNC_OE_LOW_ACT))
		vdctrl0 |= VDCTRL0_ENABLE_ACT_HIGH;
	if (host->sync & FB_SYNC_CLK_LAT_FALL)
		vdctrl0 |= VDCTRL0_DOTCLK_ACT_FALLING;
    writel(vdctrl0,host->base + LCDC_VDCTRL0);
    
    /* 以行为单位的帧长度 */
    writel(fb_info->var.vsync_len + fb_info->var.upper_margin + fb_info->var.yres + \
           fb_info->var.lower_margin,host->base + LCDC_VDCTRL1);
    
    /* 以时钟或像素为单位的行长度 */
    writel(set_hsync_pulse_width(fb_info->var.hsync_len) | VDCTRL2_SET_HSYNC_PERIOD(fb_info->var.hsync_len +
           fb_info->var.left_margin + fb_info->var.xres + fb_info->var.right_margin),host->base + LCDC_VDCTRL2);
    
    writel(SET_HOR_WAIT_CNT(fb_info->var.hsync_len + fb_info->var.left_margin) | \
           SET_VERT_WAIT_CNT(fb_info->var.vsync_len + fb_info->var.upper_margin),host->base + LCDC_VDCTRL3);

    vdctrl4 = SET_DOTCLK_H_VALID_DATA_CNT(fb_info->var.xres);
    vdctrl4 |= VDCTRL4_SET_DOTCLK_DLY(host->dotclk_delay);
    writel(vdctrl4,host->base + LCDC_VDCTRL4);

    writel(fb_info->fix.smem_start + fb_info->fix.line_length + fb_info->var.yoffset,host->base + LCDC_NEXT_BUF);

    if(reenable)
        tftlcd_enable_controller(fb_info);
    
    if((fb_info->var.activate & FB_ACTIVATE_FORCE) && \
        (fb_info->var.activate & FB_ACTIVATE_MASK) == FB_ACTIVATE_NOW)
        fb_info->var.activate = FB_ACTIVATE_NOW;
    
    host->var = fb_info->var;
    return 0;
}

static inline unsigned chan_to_field(unsigned chan, struct fb_bitfield *bf)
{
	chan &= 0xffff;
	chan >>= 16 - bf->length;
	return chan << bf->offset;
}

static int tftlcd_setcolreg(unsigned regno,unsigned red,unsigned green,unsigned blue,unsigned transp,struct fb_info *fb_info)
{
    unsigned int val;
    int ret = -EINVAL;

    /* 如果是灰度图，则将rgb颜色转为灰度 */
    if(fb_info->var.grayscale)
        red = green = blue = (19595 * red + 38470 * green + 7471 * blue) >> 16;

    switch (fb_info->fix.visual) {
        case FB_VISUAL_TRUECOLOR:
            /*
            * 12 or 16-bit True Colour.  We encode the RGB value
            * according to the RGB bitfield information.
            */
            if (regno < 16) {
                u32 *pal = fb_info->pseudo_palette;

                val  = chan_to_field(red, &fb_info->var.red);
                val |= chan_to_field(green, &fb_info->var.green);
                val |= chan_to_field(blue, &fb_info->var.blue);

                pal[regno] = val;
                ret = 0;
            }
            break;

        case FB_VISUAL_STATIC_PSEUDOCOLOR:
        case FB_VISUAL_PSEUDOCOLOR:
		    break;
	}

	return ret;
}

static int tftlcd_wait_for_vsync(struct fb_info *info)
{
    struct tftlcd_info *host = info->par;
    int ret = 0;

    if(host->cur_blank != FB_BLANK_UNBLANK){
        dev_err(&host->pdev->dev,"can't wait for vsync when fb is blank\n");
        return -EINVAL;
    }

    init_completion(&host->vsync_complete);
    host->wait4vsync = 1;

    writel(CTRL1_VSYNC_EDGE_IRQ_EN,host->base + LCDC_CTRL1 + REG_SET);
    ret = wait_for_completion_interruptible_timeout(&host->vsync_complete,1 * HZ);
    if(!ret){
        dev_err(&host->pdev->dev,"wait for vsync failed!\n");
        host->wait4vsync = 0;
        return -ETIME;
    }else if(ret > 0){
        ret = 0;
    }
    return ret;
}

static int tftlcd_ioctl(struct fb_info *info,unsigned int cmd,unsigned long arg)
{
    int ret = -EINVAL;

    switch(cmd){
        case TFTLCD_WAIT_FOR_VSYNC:
            ret = tftlcd_wait_for_vsync(info);
            return ret;
        default:
            break;
    }
    return ret;
}

static int tftlcd_blank(int blank,struct fb_info *info)
{
    struct tftlcd_info *host = info->par;

    host->cur_blank = blank;

    switch(blank){
        case FB_BLANK_POWERDOWN:
        case FB_BLANK_VSYNC_SUSPEND:
        case FB_BLANK_HSYNC_SUSPEND:
        case FB_BLANK_NORMAL:
            if(host->enabled){
                tftlcd_disable_controller(info);
                pm_runtime_put_sync_suspend(&host->pdev->dev);
            }
            clk_disable_disp_axi(host);
            clk_disable_axi(host);
            clk_disable_pix(host);
            break;
        case FB_BLANK_UNBLANK:
            info->var.activate = (info->var.activate & ~FB_ACTIVATE_MASK) | FB_ACTIVATE_NOW | FB_ACTIVATE_FORCE;
            clk_enable_pix(host);
            clk_enable_axi(host);
            clk_enable_disp_axi(host);

            if(!host->enabled){
                pm_runtime_get_sync(&host->pdev->dev);

                writel(0,host->base + LCDC_CTRL);
                tftlcd_set_par(info);
                tftlcd_enable_controller(info);

            }
            break;
    }
    return 0;
}

static int tftlcd_pan_display(struct fb_var_screeninfo *var,struct fb_info *info)
{
    int ret = 0;
	struct tftlcd_info *host = info->par;
	unsigned offset;

	if (host->cur_blank != FB_BLANK_UNBLANK) {
		dev_dbg(info->device, "can't do pan display when fb "
			"is blank\n");
		return -EINVAL;
	}

	if (var->xoffset > 0) {
		dev_dbg(info->device, "x panning not supported\n");
		return -EINVAL;
	}

	if ((var->yoffset + var->yres > var->yres_virtual)) {
		dev_err(info->device, "y panning exceeds\n");
		return -EINVAL;
	}

	init_completion(&host->flip_complete);

	offset = info->fix.line_length * var->yoffset;

	/* update on next VSYNC */
	writel(info->fix.smem_start + offset,
			host->base + LCDC_NEXT_BUF);

	writel(CTRL1_CUR_FRAME_DONE_IRQ_EN,host->base + LCDC_CTRL1 + REG_SET);

	ret = wait_for_completion_timeout(&host->flip_complete, HZ / 2);
	if (!ret) {
		dev_err(info->device,"mxs wait for pan flip timeout\n");
		return -ETIMEDOUT;
	}

	return 0;
}

static int tftlcd_mmap(struct fb_info *info, struct vm_area_struct *vma)
{
	u32 len;
	unsigned long offset = vma->vm_pgoff << PAGE_SHIFT;

	if (offset < info->fix.smem_len) {
		/* mapping framebuffer memory */
		len = info->fix.smem_len - offset;
		vma->vm_pgoff = (info->fix.smem_start + offset) >> PAGE_SHIFT;
	} else
		return -EINVAL;

	len = PAGE_ALIGN(len);
	if (vma->vm_end - vma->vm_start > len)
		return -EINVAL;

	/* make buffers bufferable */
	vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);

	if (remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff,
			    vma->vm_end - vma->vm_start, vma->vm_page_prot)) {
		dev_dbg(info->device, "mmap remap_pfn_range failed\n");
		return -ENOBUFS;
	}

	return 0;
}

static struct fb_ops tftlcd_fbops = {
    .owner = THIS_MODULE,
    .fb_check_var   = tftlcd_check_var,
    .fb_set_par     = tftlcd_set_par,
    .fb_setcolreg   = tftlcd_setcolreg,
    .fb_ioctl       = tftlcd_ioctl,
    .fb_blank       = tftlcd_blank,
    .fb_pan_display = tftlcd_pan_display,
    .fb_mmap        = tftlcd_mmap,
    .fb_fillrect    = cfb_fillrect,
    .fb_copyarea    = cfb_copyarea,
    .fb_imageblit   = cfb_imageblit,
};

static int tftlcd_init_fbinfo_dt(struct tftlcd_info *host)
{
    int ret;
    struct fb_info *fb_info = host->fb_info;
    struct fb_var_screeninfo *var = &fb_info->var;
    struct device *dev = &host->pdev->dev;
    struct device_node *np = dev->of_node;
    struct device_node *display_np;
    struct device_node *timings_np;
    struct display_timings *timings = NULL;
    const char *disp_dev;
    u32 width;
    int i;

    host->id = of_alias_get_id(np,"lcdif");

    display_np = of_parse_phandle(np,"display",0);
    if(!display_np){
        dev_err(dev,"failed to find display phandle!\n");
        return -ENOENT;
    }

    ret = of_property_read_u32(display_np,"bus-width",&width);
    if(ret < 0){
        dev_err(dev,"failed to get property bus width!\n");
        goto put_display_node;
    }

    switch(width){
        case 8:
            host->bus_width = LCDIF_BUS_WIDTH_8BIT;
            break;
        case 16:
            host->bus_width = LCDIF_BUS_WIDTH_16BIT;
            break;
        case 18:
            host->bus_width = LCDIF_BUS_WIDTH_18BIT;
            break;
        case 24:
            host->bus_width = LCDIF_BUS_WIDTH_24BIT;
            break;
        default:
            dev_err(dev,"invalid bus-width value!\n");
            ret = -EINVAL;
            goto put_display_node;
    }

    ret = of_property_read_u32(display_np,"bits-per-pixel",&var->bits_per_pixel);
    if(ret < 0){
        dev_err(dev,"failed to get property bits-per-pixel!\n");
        goto put_display_node;
    }

    ret = of_property_read_string(np,"disp_dev",&disp_dev);
    if(!ret){
        memcpy(host->disp_dev,disp_dev,strlen(disp_dev));
        goto put_display_node;
    }

    timings = of_get_display_timings(display_np);
    if(!timings){
        dev_err(dev,"failed to get display timings!\n");
        ret = -ENOENT;
        goto put_display_node;
    }

    timings_np = of_find_node_by_name(display_np,"display-timings");
    if(!timings_np){
        dev_err(dev,"failed to get timings node\n");
        ret = -ENOENT;
        goto put_display_node;
    }

    for(i = 0 ; i < of_get_child_count(timings_np) ; i++){
        struct videomode vm;
        struct fb_videomode fb_vm;

        ret = videomode_from_timings(timings,&vm,i);
        if(ret < 0)
            goto put_timing_node;
        
        ret = fb_videomode_from_videomode(&vm,&fb_vm);
        if(ret < 0)
            goto put_timing_node;
        
        if (!(vm.flags & DISPLAY_FLAGS_DE_HIGH))
			fb_vm.sync |= FB_SYNC_OE_LOW_ACT;
		if (vm.flags & DISPLAY_FLAGS_PIXDATA_NEGEDGE)
			fb_vm.sync |= FB_SYNC_CLK_LAT_FALL;
        
        fb_add_videomode(&fb_vm,&fb_info->modelist);
    }
put_timing_node:
    of_node_put(timings_np);
put_display_node:
    if(timings)
        kfree(timings);
    of_node_put(display_np);
    return ret;
}

static int tftlcd_map_videomem(struct fb_info *fb_info)
{
    if(fb_info->fix.line_length * fb_info->var.yres > fb_info->fix.smem_len)
        fb_info->fix.smem_len = fb_info->fix.line_length * fb_info->var.yres;
    
    fb_info->screen_base = dma_alloc_writecombine(fb_info->device,fb_info->fix.smem_len,(dma_addr_t *)&fb_info->fix.smem_start,GFP_KERNEL | GFP_DMA);
    if(!fb_info->screen_base){
        dev_err(fb_info->dev,"failed to allocate fb\n");
        fb_info->fix.smem_start = 0;
        fb_info->fix.smem_len = 0;
        return -EBUSY;
    }

    fb_info->screen_size = fb_info->fix.smem_len;

    memset(fb_info->screen_base,0,fb_info->screen_size);
    
    return 0; 
}

static int tftlcd_unmap_videomem(struct fb_info *fb_info)
{
    dma_free_writecombine(fb_info->dev,fb_info->fix.smem_len,fb_info->screen_base,fb_info->fix.smem_start);
    fb_info->screen_base = 0;
    // fb_info->screen_size = 0;
    fb_info->fix.smem_start = 0;
    fb_info->fix.smem_len = 0;
    return 0;
}

static void tftlcd_free_videomem(struct tftlcd_info *host)
{
    struct fb_info *fb_info = host->fb_info;
    tftlcd_unmap_videomem(fb_info);
}

static int tftlcd_restore_mode(struct tftlcd_info *host)
{
    struct fb_info *fb_info = host->fb_info;
    int bits_per_pixel,offset;
    u32 transfer_count,vdctrl0,vdctrl1,vdctrl2,vdctrl3,vdctrl4,ctrl;
    unsigned long pa,fbsize;
    struct fb_videomode fb_vm;

    clk_enable_axi(host);
    clk_enable_disp_axi(host);
    clk_enable_pix(host);

    /* 只能在控制器正在运行的情况下进行恢复 */
    ctrl = readl(host->base + LCDC_CTRL);
    if(!(ctrl & CTRL_RUN))
        return -EINVAL;
    
    memset(&fb_vm,0,sizeof(struct fb_videomode));

    vdctrl0 = readl(host->base + LCDC_VDCTRL0);
    vdctrl1 = readl(host->base + LCDC_VDCTRL1);
    vdctrl2 = readl(host->base + LCDC_VDCTRL2);
    vdctrl3 = readl(host->base + LCDC_VDCTRL3);
    vdctrl4 = readl(host->base + LCDC_VDCTRL4);
    transfer_count = readl(host->base + LCDC_TRANSFER_COUNT);

    fb_vm.xres = TRANSFER_COUNT_GET_HCOUNT(transfer_count);
    fb_vm.yres = TRANSFER_COUNT_GET_VCOUNT(transfer_count);

    switch(CTRL_GET_WORD_LENGTH(ctrl)){
        case 0:
            bits_per_pixel = 16;
            break;
        case 3:
            bits_per_pixel = 32;
            break;
        case 1:
        default:
            return -EINVAL;
    }

    fb_info->var.bits_per_pixel = bits_per_pixel;

    fb_vm.pixclock = KHZ2PICOS(clk_get_rate(host->clk_pix) / 1000U);
    fb_vm.hsync_len = get_hsync_pulse_width(vdctrl2);
    fb_vm.left_margin = GET_HOR_WAIT_CNT(vdctrl3) - fb_vm.hsync_len;
    fb_vm.right_margin = VDCTRL2_GET_HSYNC_PERIOD(vdctrl2) - fb_vm.hsync_len - fb_vm.left_margin - fb_vm.xres;
    fb_vm.vsync_len = VDCTRL0_GET_VSYNC_PULSE_WIDTH(vdctrl0);
    fb_vm.upper_margin = GET_VERT_WAIT_CNT(vdctrl3) - fb_vm.vsync_len;
    fb_vm.lower_margin = vdctrl1 - fb_vm.vsync_len - fb_vm.upper_margin - fb_vm.yres;

    fb_vm.vmode = FB_VMODE_NONINTERLACED;

    fb_vm.sync = 0;

    if(vdctrl0 & VDCTRL0_HSYNC_ACT_HIGH)
        fb_vm.sync |= FB_SYNC_HOR_HIGH_ACT;
    if(vdctrl0 & VDCTRL0_VSYNC_ACT_HIGH)
        fb_vm.sync |= FB_SYNC_VERT_HIGH_ACT;

    pr_debug("Reconstructed video mode:\n");
	pr_debug("%dx%d, hsync: %u left: %u, right: %u, vsync: %u, upper: %u, lower: %u\n",
			fb_vm.xres, fb_vm.yres,
			fb_vm.hsync_len, fb_vm.left_margin, fb_vm.right_margin,
			fb_vm.vsync_len, fb_vm.upper_margin, fb_vm.lower_margin);
	pr_debug("pixclk: %ldkHz\n", PICOS2KHZ(fb_vm.pixclock));

    fb_add_videomode(&fb_vm,&fb_info->modelist);

    host->bus_width = CTRL_GET_BUS_WIDTH(ctrl);
    host->dotclk_delay = VDCTRL4_GET_DOTCLK_DLY(vdctrl4);

    fb_info->fix.line_length = fb_vm.xres * (bits_per_pixel >> 3);

    pa = readl(host->base + LCDC_CUR_BUF);
    fbsize = fb_info->fix.line_length * fb_vm.yres;
    if(pa < fb_info->fix.smem_start)
        return -EINVAL;
    if(pa + fbsize > fb_info->fix.smem_start + fb_info->fix.smem_len)
        return -EINVAL;
    
    offset = pa - fb_info->fix.smem_start;
    if(offset){ 
        memmove(fb_info->screen_base,fb_info->screen_base + offset,fbsize); 
        writel(fb_info->fix.smem_start,host->base + LCDC_NEXT_BUF);
    }

    host->enabled = 1;
    return 0;
}

static int tftlcd_init_fbinfo(struct tftlcd_info *host)
{
    int ret;
    struct fb_info *fb_info = host->fb_info;
    struct fb_var_screeninfo *var = &fb_info->var;
    struct fb_modelist *modelist;

    fb_info->fbops = &tftlcd_fbops;
    fb_info->flags = FBINFO_FLAG_DEFAULT | FBINFO_READS_FAST;
    fb_info->fix.type   = FB_TYPE_PACKED_PIXELS;
    fb_info->fix.visual = FB_VISUAL_TRUECOLOR;
    fb_info->fix.accel  = FB_ACCEL_NONE;

    ret = tftlcd_init_fbinfo_dt(host);
    if(ret)
        return ret;
    
    if(host->id < 0){
        sprintf(fb_info->fix.id,"tft-lcdif");
    }else{
        sprintf(fb_info->fix.id,"tft-lcdif%d",host->id);
    }

    if(!list_empty(&fb_info->modelist)){
        modelist = list_first_entry(&fb_info->modelist,struct fb_modelist,list);
        fb_videomode_to_var(var,&modelist->mode);
    }
    host->sync = var->sync;

    var->nonstd = 0;
    var->activate = FB_ACTIVATE_NOW;
    var->accel_flags = 0;
    var->vmode = FB_VMODE_NONINTERLACED;

    /* 初始化颜色字段 */
    tftlcd_check_var(var,fb_info); 

    fb_info->fix.line_length = (var->xres * var->bits_per_pixel) >> 8;
    fb_info->fix.smem_len = SZ_32M;

    /* 为帧缓存分配内存 */
    if(tftlcd_map_videomem(fb_info) < 0){
        return -ENOMEM;
    }
     
    if(tftlcd_restore_mode(host)){
        memset((char *)fb_info->screen_base,0,fb_info->fix.smem_len);
    }
     
    return 0;
}

irqreturn_t tftlcd_irq_handler(int irq,void *dev_id)
{
    struct tftlcd_info *host = dev_id;
    u32 ctrl1,enable,status,asked_status;

    ctrl1 = readl(host->base + LCDC_CTRL1);
    enable = (ctrl1 & CTRL1_IRQ_ENABLE_MASK) >> CTRL1_IRQ_ENABLE_SHIFT;
    status = (ctrl1 & CTRL1_IRQ_STATUS_MASK) >> CTRL1_IRQ_STATUS_SHIFT;
    asked_status = (enable & status) << CTRL1_IRQ_STATUS_SHIFT;

    if((asked_status & CTRL1_VSYNC_EDGE_IRQ) && host->wait4vsync){
        writel(CTRL1_VSYNC_EDGE_IRQ,host->base + LCDC_CTRL1 + REG_CLR);
        writel(CTRL1_VSYNC_EDGE_IRQ_EN,host->base + LCDC_CTRL1 + REG_CLR);
        host->wait4vsync = 0;
        complete(&host->vsync_complete);
    }

    if(asked_status & CTRL1_CUR_FRAME_DONE_IRQ){
        writel(CTRL1_CUR_FRAME_DONE_IRQ,host->base + LCDC_CTRL1 + REG_CLR);
        writel(CTRL1_CUR_FRAME_DONE_IRQ_EN,host->base + LCDC_CTRL1 + REG_CLR);
        complete(&host->flip_complete);
    }

    if(asked_status & CTRL1_UNDERFLOW_IRQ){
        writel(CTRL1_UNDERFLOW_IRQ,host->base + LCDC_CTRL1 + REG_CLR);
    }

    if(asked_status & CTRL1_OVERFLOW_IRQ){
        writel(CTRL1_OVERFLOW_IRQ,host->base + LCDC_CTRL1 + REG_CLR);
    }
    return 0;
}

static int tftlcd_probe(struct platform_device *pdev)
{
    int ret;
    struct resource *res;
    struct tftlcd_info *host;
    struct fb_info *fb_info;
    // struct pinctrl *pinctrl;
    int irq = platform_get_irq(pdev,0);
    printk("enter:%s\n",__func__);
    res = platform_get_resource(pdev,IORESOURCE_MEM,0);
    if(!res){
        dev_err(&pdev->dev,"can not get memory io resource\n");
        return -ENODEV;
    }

    host = devm_kzalloc(&pdev->dev,sizeof(struct tftlcd_info),GFP_KERNEL);
    if(!host){
        dev_err(&pdev->dev,"failed to allocate tftlcd_info!\n");
        return -ENOMEM;
    }

    fb_info = framebuffer_alloc(0,&pdev->dev);
    if(!fb_info){
        dev_err(&pdev->dev,"failed to allocate fb_info!\n");
        devm_kfree(&pdev->dev,host);
        return -ENOMEM;
    }
    host->fb_info = fb_info;
    fb_info->par = host;

    ret = devm_request_irq(&pdev->dev,irq,tftlcd_irq_handler,0,dev_name(&pdev->dev),host);
    if(ret){
        dev_err(&pdev->dev,"requeset irq(%d) failed,errno is %d\n",irq,ret);
        ret = -ENODEV;
        goto fb_release;
    }

    host->base = devm_ioremap_resource(&pdev->dev,res);
    if(IS_ERR(host->base)){
        dev_err(&pdev->dev,"ioremap failed!\n");
        ret = PTR_ERR(host->base);
        goto fb_release;
    }

    host->pdev = pdev;
    platform_set_drvdata(pdev,host);

    //....devdata

    /* 获取时钟 */
    host->clk_axi = devm_clk_get(&pdev->dev,"axi");
    if(IS_ERR(host->clk_axi)){
        dev_warn(&pdev->dev,"get axi clk failed!\n");
        host->clk_axi = NULL;
        ret = PTR_ERR(host->clk_axi);
        goto fb_release;
    }

    host->clk_pix = devm_clk_get(&pdev->dev,"pix");
    if(IS_ERR(host->clk_pix)){
        dev_warn(&pdev->dev,"get axi pix failed!\n");
        host->clk_pix = NULL;
        ret = PTR_ERR(host->clk_pix);
        goto fb_release;
    }

    host->clk_disp_axi = devm_clk_get(&pdev->dev,"disp_axi");
    if(IS_ERR(host->clk_disp_axi)){
        dev_warn(&pdev->dev,"get axi clk failed!\n");
        host->clk_disp_axi = NULL;
        ret = PTR_ERR(host->clk_disp_axi);
        goto fb_release;
    }

    host->regulator = devm_regulator_get(&pdev->dev,"lcd");
    if(IS_ERR(host->regulator))
        host->regulator = NULL;

    fb_info->pseudo_palette = devm_kzalloc(&pdev->dev,sizeof(32) * 16,GFP_KERNEL);
    if(!fb_info->pseudo_palette){
        ret = -ENOMEM;
        goto fb_release;
    }

    INIT_LIST_HEAD(&fb_info->modelist);

    pm_runtime_enable(&host->pdev->dev);

    ret = tftlcd_init_fbinfo(host);
    if(ret)
        goto fb_runtime_disable;

    //dispdrv...

    if(!host->enabled){
        writel(0,host->base + LCDC_CTRL);
        tftlcd_set_par(fb_info);
        tftlcd_enable_controller(fb_info);
        pm_runtime_get_sync(&host->pdev->dev);
    }

    ret = register_framebuffer(fb_info);
    if(ret){
        dev_err(&host->pdev->dev,"failed to register framebuffer\n");
        goto fb_destroy;
    }

    console_lock();
    ret = fb_blank(fb_info,FB_BLANK_UNBLANK);
    console_unlock();

    if(ret < 0){
        dev_err(&host->pdev->dev,"failed to unblank framebuffer\n");
        goto fb_unregister;
    }

    dev_info(&host->pdev->dev,"initialized\n");

    return 0;

fb_unregister:
    unregister_framebuffer(fb_info);
fb_destroy:
    if(host->enabled)
        clk_disable_unprepare(host->clk_pix);
    fb_destroy_modelist(&fb_info->modelist);
fb_runtime_disable:
    pm_runtime_disable(&host->pdev->dev);
    devm_kfree(&pdev->dev,fb_info->pseudo_palette);
fb_release:
    framebuffer_release(fb_info);
    devm_kfree(&pdev->dev,host);

    return ret;
}

static int tftlcd_remove(struct platform_device *pdev)
{
    struct tftlcd_info *host = platform_get_drvdata(pdev);
    struct fb_info *fb_info = host->fb_info;

    if(host->enabled){
        tftlcd_disable_controller(fb_info);
    }

    pm_runtime_disable(&pdev->dev);
    unregister_framebuffer(fb_info);
    tftlcd_free_videomem(host);

    platform_set_drvdata(pdev,NULL);

    devm_kfree(&pdev->dev,fb_info->pseudo_palette);
    framebuffer_release(fb_info);
    devm_kfree(&pdev->dev,host);

    return 0;
}

static struct of_device_id tftlcd_of_table[] = {
    {.compatible = "my,tftlcd"},
    { /* sentinel */ },
};

static struct platform_device_id tftlcd_id_table[] = {
    {"my,tftlcd",0},
    { /* sentinel */ },
};

static struct platform_driver tftlcd_driver = {
    .driver = {
        .owner = THIS_MODULE,
        .name = "tftlcd driver",
        .of_match_table = tftlcd_of_table,
    },
    .probe = tftlcd_probe,
    .remove = tftlcd_remove,
    .id_table = tftlcd_id_table,
};

static int __init tftlcd_init(void)
{
    printk("enter:%s\n",__func__);
    return platform_driver_register(&tftlcd_driver);
}

static void __exit tftlcd_exit(void)
{
    platform_driver_unregister(&tftlcd_driver);
}

module_init(tftlcd_init);
module_exit(tftlcd_exit);
// module_platform_driver(tftlcd_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("luo");