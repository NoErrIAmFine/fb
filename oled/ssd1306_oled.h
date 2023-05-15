#ifndef __SSD1306_OLED_H
#define __SSD1306_OLED_H

/* 为命令定义一个易记的宏 */
#define __ssd_cmd(_fixed,_offset,_var) ((_fixed) << _offset)

#define CMD_SET_CONTRAST_CONTROL (0x81)
#define CMD_ENTIRE_DISPLAY_ON(_arg) (0xa4 & (_arg & 0x01))
#define CMD_INVERT_DISPLAY(_arg) (0xa6 & (_arg & 0x01))
#define CMD_DISPLAY_ON(_arg) (0xae & (_arg & 0x01))

#define CMD_SET_LOW_COL_ADDR(_arg) (0x0 & (_arg & 0xf))
#define CMD_SET_HIGH_COL_ADDR(_arg) (0x1 & (_arg & 0xf))
#define CMD_SET_ADDR_MODE (0x20)
#define CMD_SET_COL_ADDR (0x21)
#define CMD_SET_PAGE_ADDR (0x22)



#endif // !__SSD1306_OLED_H