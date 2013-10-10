#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/ctype.h>
#include <linux/io.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <mach/power-gate.h>
#include <mach/pad.h>
#include <mach/items.h>

#include "mach/imap-iomap.h"


/*
 *  configure usb reference clock
 *
*/
static void usb_ref_clk_cfg(void __iomem *base_reg, int32_t ref_clk)
{
	int32_t val;

	val = readl(base_reg + 0x20);
	val &=~0x7;
	switch(ref_clk)
	{
	case 96000000:
		// 9.6MHz
		val |= 0x0;
		break;
	case 10000000:
		// 10MHz
		val |= 0x1;
		break;
	case 12000000:
		// 12MHz
		val |= 0x2;
		break;
	case 19200000:
		// 19.2MHz
		val |= 0x3;
		break;
	case 20000000:
		// 20MHz
		val |= 0x4;
		break;
	case 24000000:
		// 24MHz
		val |= 0x5;
		break;
	case 50000000:
		// 50MHz
		val |= 0x7;
		break;
	default:
		// 24MHz
		val |= 0x5;
		break;
	}
	writel(val, base_reg + 0x20);
}


int host_phy_config(int32_t ref_clk, int utmi_16bit)
{
	void __iomem *base_reg;
	int32_t val;

	base_reg = IO_ADDRESS(SYSMGR_USBH_BASE);

	val = readl(base_reg + 0x30);
	if(val & 0x1)
	{
		printk("usb host phy always config\n");
	}
	else
	{
		module_power_on(SYSMGR_USBH_BASE);
		usb_ref_clk_cfg(base_reg, ref_clk);
		// configure utmi+ 16-bit
		val = readl(base_reg + 0x24);
		if(utmi_16bit)
			val |= 0x1;
		else
			val &=~0x1;
		writel(val, base_reg + 0x24);

		// configure port0 sleep mode to resume
		val = readl(base_reg + 0x30);
		val |= 0x1;
		writel(val, base_reg + 0x30);

		// configure port1 sleep mode to resume
		val = readl(base_reg + 0x74);
		val |= 0x1;
		writel(val, base_reg + 0x74);

		// usb controller reset
		val = readl(base_reg);
		val |= 0x1f;
		writel(val, base_reg);
		//udelay(2);
		mdelay(3);
		val &=~0x1e;
		writel(val, base_reg);
		udelay(2);
		val &=~0x1;
		writel(val, base_reg);
#if 0
		writel(0x1, base_reg + 0xac);
		writel(0x1, base_reg + 0xb4);
#endif		
	}
}

int otg_phy_reconfig(int utmi_16bit, int otg_flag)
{
	void __iomem *base_reg;
	int32_t val;

	if(otg_flag)
	{
        if(item_equal("pmu.model", "axp202", 0) == 0)
            imapx_pad_cfg(IMAPX_OTG, 1);
	}
	else
	{
        if( (item_exist("board.cpu") == 0 )||item_equal("board.cpu", "imapx820", 0) ){
            imapx_pad_set_mode(1,1,78);
            imapx_pad_set_dir(0,1,78);
            imapx_pad_set_outdat(1,1,78);
        } else if(item_equal("board.cpu", "i15", 0)){
           // imapx_pad_set_mode(1,1,78);
           // imapx_pad_set_dir(0,1,78);
           //imapx_pad_set_outdat(1,1,78);
        }

	}

	base_reg = IO_ADDRESS(SYSMGR_OTG_BASE);
	
	// configure sleep mode to resume
	val = readl(base_reg + 0x2c);
	val &= ~0x1;
	writel(val, base_reg + 0x2c);

	// configure utim+ 16-bit
	val = readl(base_reg + 0x24);
	if(otg_flag)
		val |= 0x1;
	else
		val &=~0x1;
	if(utmi_16bit)
		val |= 0x2;
	else
		val &=~0x2;
	writel(val, base_reg + 0x24);


	// otg controller reset
	val = readl(base_reg);
	val |= 0x1;
	writel(val, base_reg);
	mdelay(3);
	//val &=~0xe;
	//writel(val, base_reg);
	//msleep(20);
	val &=~0x1;
	writel(val, base_reg);

	// configure sleep mode to resume
	val = readl(base_reg + 0x2c);
	val |= 0x1;
	writel(val, base_reg + 0x2c);

}

int otg_phy_config(int32_t ref_clk, int utmi_16bit, int otg_flag)
{
	void __iomem *base_reg;
	void __iomem *uart_reg;
	int32_t val;

	if(otg_flag)
	{
        if(item_equal("pmu.model", "axp202", 0) == 0)
            imapx_pad_cfg(IMAPX_OTG, 1);
	}
	else
	{
        if( (item_exist("board.cpu") == 0 )||item_equal("board.cpu", "imapx820", 0) ){
            imapx_pad_set_mode(1,1,78);
            imapx_pad_set_dir(0,1,78);
            imapx_pad_set_outdat(1,1,78);
        } else if(item_equal("board.cpu", "i15", 0)){
           // imapx_pad_set_mode(1,1,78);
           // imapx_pad_set_dir(0,1,78);
           //imapx_pad_set_outdat(1,1,78);
        }

	}

	base_reg = IO_ADDRESS(SYSMGR_OTG_BASE);
	uart_reg = IO_ADDRESS(SYSMGR_UART_BASE);
	
	module_power_on(SYSMGR_OTG_BASE);
	usb_ref_clk_cfg(base_reg, ref_clk);
	// configure utim+ 16-bit
	val = readl(base_reg + 0x24);
	if(otg_flag)
		val |= 0x1;
	else
		val &=~0x1;
	if(utmi_16bit)
		val |= 0x2;
	else
		val &=~0x2;
	writel(val, base_reg + 0x24);

	// configure sleep mode to resume
	val = readl(base_reg + 0x2c);
	val |= 0x1;
	writel(val, base_reg + 0x2c);

	// otg controller reset
	val = readl(base_reg);
	val |= 0xf;
	writel(val, base_reg);
	mdelay(3);
	val &=~0xe;
	writel(val, base_reg);
	mdelay(3);
	val &=~0x1;
	writel(val, base_reg);

        if( item_exist("otg.uartmode") && item_equal("otg.uartmode", "enable", 0)){
            writel(0x1, base_reg + 0x64);
            writel(0x1, base_reg + 0x6c);
            writel(0x18, uart_reg + 0x24);
        }
}



