/*
 * Battery charger driver for X-Powers AXP
 *
 * Copyright (C) 2011 X-Powers, Ltd.
 *  Zhang Donglu <zhangdonglu@x-powers.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/workqueue.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>

#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/slab.h>

#include <linux/seq_file.h>
#include <linux/input.h>
#include <asm/div64.h>
#include <linux/interrupt.h>
#include <mach/irqs.h>
#include <mach/pad.h>
#include <mach/items.h>

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#define DBG_AXP_PSY 0
#if  DBG_AXP_PSY
#define DBG_PSY_MSG(format,args...)   printk("[AXP]"format,##args)
#else
#define DBG_PSY_MSG(format,args...)   do {} while (0)
#endif

#include "axp-sply.h"
static void axp_charger_update_state(struct axp_charger *charger);
static void axp_charger_update(struct axp_charger *charger);
static int axp_ibat_to_mA(uint16_t reg);
static void axp_close(struct axp_charger *charger);
#include "axp-init.h"
#include "axp-debug.h"
//#include "axp-noirq.h"

#include "axp-calocv.h"
#include "axp-calcou.h"

#ifdef CONFIG_HAS_EARLYSUSPEND
static struct early_suspend axp_early_suspend;
#endif

extern int read_rtc_gpx(int);
extern int32_t get_otg_charge_mode(void);
extern void vbus_en_pull(int);
static int charger_ing =0;
extern int batt_export_axp202_cap;

static inline int axp_vbat_to_mV(uint16_t reg)
{
	return (reg) * 1100 / 1000;
}

static inline int axp_vdc_to_mV(uint16_t reg)
{
	return (reg) * 1700 / 1000;
}

static inline int axp_ibat_to_mA(uint16_t reg)
{
	return (reg) * 500 / 1000;
}

static inline int axp_iac_to_mA(uint16_t reg)
{
	return (reg) * 625 / 1000;
}

static inline int axp_iusb_to_mA(uint16_t reg)
{
	return (reg) * 375 / 1000;
}

static inline void axp_read_adc(struct axp_charger *charger,
  struct axp_adc_res *adc)
{
	uint8_t tmp[8];
	axp_reads(charger->master,AXP_VACH_RES,8,tmp);
	adc->vac_res = ((uint16_t) tmp[0] << 4 )| (tmp[1] & 0x0f);
	adc->iac_res = ((uint16_t) tmp[2] << 4 )| (tmp[3] & 0x0f);
	adc->vusb_res = ((uint16_t) tmp[4] << 4 )| (tmp[5] & 0x0f);
	adc->iusb_res = ((uint16_t) tmp[6] << 4 )| (tmp[7] & 0x0f);
	axp_reads(charger->master,AXP_VBATH_RES,6,tmp);
	adc->vbat_res = ((uint16_t) tmp[0] << 4 )| (tmp[1] & 0x0f);
#if defined (CONFIG_KP_AXP20)
	adc->ichar_res = ((uint16_t) tmp[2] << 4 )| (tmp[3] & 0x0f);
#endif
#if defined (CONFIG_KP_AXP19)
	adc->ichar_res = ((uint16_t) tmp[2] << 5 )| (tmp[3] & 0x1f);
#endif
	adc->idischar_res = ((uint16_t) tmp[4] << 5 )| (tmp[5] & 0x1f);
}

static void axp_charger_update_state(struct axp_charger *charger)
{
	uint8_t val[2];
	uint16_t tmp;

	axp_reads(charger->master,AXP_CHARGE_STATUS,2,val);
	tmp = (val[1] << 8 )+ val[0];
	charger->is_on 					= (tmp & AXP_STATUS_INCHAR)?1:0;
	charger->bat_det 				= (tmp & AXP_STATUS_BATEN)?1:0;
	charger->ac_det 				= (tmp & AXP_STATUS_ACEN)?1:0;
	charger->usb_det 				= (tmp & AXP_STATUS_USBEN)?1:0;
	charger->usb_valid 				= (tmp & AXP_STATUS_USBVA)?1:0;
	charger->ac_valid 				= (tmp & AXP_STATUS_ACVA)?1:0;
	charger->ext_valid 				= charger->ac_valid | charger->usb_valid;
	charger->bat_current_direction 	= (tmp & AXP_STATUS_BATCURDIR)?1:0;
	charger->in_short 				= (tmp& AXP_STATUS_ACUSBSH)?1:0;
	charger->batery_active 			= (tmp & AXP_STATUS_BATINACT)?1:0;
	charger->low_charge_current 	= (tmp & AXP_STATUS_CHACURLOEXP)?1:0;
	charger->int_over_temp 			= (tmp & AXP_STATUS_ICTEMOV)?1:0;
	axp_read(charger->master,AXP_CHARGE_CONTROL1,val);
	charger->charge_on 				= ((val[0] & 0x80)?1:0);
}

static void axp_charger_update(struct axp_charger *charger)
{
	uint16_t tmp;
	axp_read_adc(charger, &charger->adc);
	tmp = charger->adc.vbat_res;
	charger->vbat = axp_vbat_to_mV(tmp);
	charger->ibat = ABS(axp_ibat_to_mA(charger->adc.ichar_res)-axp_ibat_to_mA(charger->adc.idischar_res));
	tmp = charger->adc.vac_res;
	charger->vac = axp_vdc_to_mV(tmp);
	tmp = charger->adc.iac_res;
	charger->iac = axp_iac_to_mA(tmp);
	tmp = charger->adc.vusb_res;
	charger->vusb = axp_vdc_to_mV(tmp);
	tmp = charger->adc.iusb_res;
	charger->iusb = axp_iusb_to_mA(tmp);
	if(!charger->ext_valid){
		charger->disvbat =  charger->vbat;
		charger->disibat =  charger->ibat;
	}
}

static enum power_supply_property axp_battery_props[] = {
	POWER_SUPPLY_PROP_MODEL_NAME,
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN,
	POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_TEMP,
};

static enum power_supply_property axp_ac_props[] = {
	POWER_SUPPLY_PROP_MODEL_NAME,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
};

static enum power_supply_property axp_usb_props[] = {
	POWER_SUPPLY_PROP_MODEL_NAME,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
};

static void axp_battery_check_status(struct axp_charger *charger,
            union power_supply_propval *val)
{
	if (charger->bat_det) {
		if (charger->ext_valid){
			if( charger->rest_cap == 100)
				val->intval = POWER_SUPPLY_STATUS_FULL;
			else if(charger->charge_on)
    			val->intval = POWER_SUPPLY_STATUS_CHARGING;
			else
				val->intval = POWER_SUPPLY_STATUS_NOT_CHARGING;
		} else {
			val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
		}
	} else {
		val->intval = POWER_SUPPLY_STATUS_FULL;
	}
}

static void axp_battery_check_health(struct axp_charger *charger,
            union power_supply_propval *val)
{
	val->intval = POWER_SUPPLY_HEALTH_GOOD;
}

static int axp_battery_get_property(struct power_supply *psy,
           enum power_supply_property psp,
           union power_supply_propval *val)
{
	struct axp_charger *charger;
	int ret = 0;
	charger = container_of(psy, struct axp_charger, batt);

	switch (psp) {
		case POWER_SUPPLY_PROP_STATUS:
			axp_battery_check_status(charger, val);break;
		case POWER_SUPPLY_PROP_HEALTH:
			axp_battery_check_health(charger, val);break;
		case POWER_SUPPLY_PROP_TECHNOLOGY:
			val->intval = charger->battery_info->technology;break;
		case POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN:
			val->intval = charger->battery_info->voltage_max_design;break;
		case POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN:
			val->intval = charger->battery_info->voltage_min_design;break;
		case POWER_SUPPLY_PROP_VOLTAGE_NOW:
			val->intval = charger->ocv * 1000;break;
		case POWER_SUPPLY_PROP_CURRENT_NOW:
			val->intval = charger->ibat * 1000;break;
		case POWER_SUPPLY_PROP_MODEL_NAME:
			val->strval = charger->batt.name;break;
		case POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN:
			val->intval = charger->battery_info->energy_full_design;break;
		case POWER_SUPPLY_PROP_CAPACITY:
			batt_export_axp202_cap = charger->rest_cap;
			val->intval = charger->rest_cap;break;
		case POWER_SUPPLY_PROP_ONLINE:
			val->intval = (!charger->is_on)&&(charger->bat_det) && (! charger->ext_valid);break;
		case POWER_SUPPLY_PROP_PRESENT:
			val->intval = charger->bat_det;break;
		case POWER_SUPPLY_PROP_TEMP:
			val->intval =  300;break;
		default:
			ret = -EINVAL;break;
	}

	return ret;
}

static int axp_ac_get_property(struct power_supply *psy,
           enum power_supply_property psp,
           union power_supply_propval *val)
{
	struct axp_charger *charger;
	int ret = 0;
	charger = container_of(psy, struct axp_charger, ac);

	switch(psp){
		case POWER_SUPPLY_PROP_MODEL_NAME:
			val->strval = charger->ac.name;break;
		case POWER_SUPPLY_PROP_PRESENT:
			val->intval = charger->ac_det;break;
		case POWER_SUPPLY_PROP_ONLINE:
			val->intval = charger->ac_valid;break;
		case POWER_SUPPLY_PROP_VOLTAGE_NOW:
			val->intval = charger->vac * 1000;break;
		case POWER_SUPPLY_PROP_CURRENT_NOW:
			val->intval = charger->iac * 1000;break;
		default:
			ret = -EINVAL;break;
	}
	return ret;
}

static int axp_usb_get_property(struct power_supply *psy,
           enum power_supply_property psp,
           union power_supply_propval *val)
{
	struct axp_charger *charger;
	int ret = 0;
	charger = container_of(psy, struct axp_charger, usb);

	switch(psp){
		case POWER_SUPPLY_PROP_MODEL_NAME:
			val->strval = charger->usb.name;break;
		case POWER_SUPPLY_PROP_PRESENT:
			val->intval = charger->usb_det;break;
		case POWER_SUPPLY_PROP_ONLINE:
			val->intval = charger->usb_valid;break;
		case POWER_SUPPLY_PROP_VOLTAGE_NOW:
			val->intval = charger->vusb * 1000;break;
		case POWER_SUPPLY_PROP_CURRENT_NOW:
			val->intval = charger->iusb * 1000;break;
		default:
			ret = -EINVAL;break;
	}
	return ret;
}

static void axp_change(struct axp_charger *charger)
{
	DBG_PSY_MSG("battery state change\n");
	axp_charger_update_state(charger);
	axp_charger_update(charger);
	state_count = 1;
	power_supply_changed(&charger->batt);
    power_supply_changed(&charger->ac);
}

static void axp_presslong(struct axp_charger *charger)
{
//	DBG_PSY_MSG("press long\n");
//#if defined (CONFIG_AXP_PEKINFO)
//	input_report_key(powerkeydev, KEY_POWER, 1);
//	input_sync(powerkeydev);
//	ssleep(2);
//	DBG_PSY_MSG("press long up\n");
//	input_report_key(powerkeydev, KEY_POWER, 0);
//	input_sync(powerkeydev);
//#endif
}

static void axp_pressshort(struct axp_charger *charger)
{
//	DBG_PSY_MSG("press short\n");
//#if defined (CONFIG_AXP_PEKINFO)
//	input_report_key(powerkeydev, KEY_POWER, 1);
//	input_sync(powerkeydev);
//	msleep(100);
//	input_report_key(powerkeydev, KEY_POWER, 0);
//	input_sync(powerkeydev);
//#endif
}

static void axp_capchange(struct axp_charger *charger)
{
	DBG_PSY_MSG("battery change\n");
	ssleep(2);
	axp_charger_update_state(charger);
	axp_charger_update(charger);
	if(charger->bat_det == 1){
		axp_battery_infoinit(charger);
		schedule_delayed_work(&charger->work, charger->interval);
	} else {
		charger->rest_cap = 100;
		charger->ocv = 0;
		cancel_delayed_work_sync(&charger->work);
	}
	power_supply_changed(&charger->batt);
}

static void axp_close(struct axp_charger *charger)
{
		DBG_PSY_MSG("axp_close\n");
	charger->rest_cap = SHUTDOWNRATE;
#if defined (CONFIG_KP_AXP20)
	axp_write(charger->master,AXP_DATA_BUFFER1,(0x80|SHUTDOWNRATE));
#endif
	DBG_PSY_MSG("\n==================event in close==============\n");
	power_supply_changed(&charger->batt);
}

static void axp_close_input(struct axp_charger *charger)
{
    DBG_PSY_MSG("------axp close input------\n");
    axp_clr_bits(charger->master, 0x30, (1<<7));
    vbus_en_pull(1);/*VBUS_EN high*/
}

#if defined (CONFIG_KP_USEIRQ)
static int axp_battery_event(struct notifier_block *nb, unsigned long event,
        void *data)
{
	struct axp_charger *charger =
	container_of(nb, struct axp_charger, nb);
#if defined (CONFIG_KP_AXP20)
	uint8_t w[9];
#endif
#if defined (CONFIG_KP_AXP19)
	uint8_t w[7];
#endif
	w[0] = (uint8_t) ((event) & 0xFF);
	w[1] = AXP_INTSTS2;
	w[2] = (uint8_t) ((event >> 8) & 0xFF);
	w[3] = AXP_INTSTS3;
	w[4] = (uint8_t) ((event >> 16) & 0xFF);
	w[5] = AXP_INTSTS4;
	w[6] = (uint8_t) ((event >> 24) & 0xFF);
#if defined (CONFIG_KP_AXP20)
	w[7] = AXP_INTSTS5;
	w[8] = (uint8_t) (((uint64_t) event >> 32) & 0xFF);
#endif

	if(event & (AXP_IRQ_BATIN|AXP_IRQ_BATRE)) {
		axp_capchange(charger);
	}

	if(event & AXP_IRQ_CHAOV) {
			DBG_PSY_MSG("charging over\n");
		axp_change(charger);
	}

	if(event & AXP_IRQ_CHAST) {
			DBG_PSY_MSG("charging on\n");
		axp_change(charger);
	}

	if(event & AXP_IRQ_CHACURLO) {
		DBG_PSY_MSG("charging current low\n");

	//	axp_read(charger->master, 0x30, &val);
	//	DBG_PSY_MSG("VBUS-IPSOUT reg=0x%x\n", val);
	//	axp_read(charger->master, 0x33, &val);
	//	DBG_PSY_MSG("CHARGING CTRL = 0x%x\n", val);
		axp_change(charger);
	}

	if(event & (AXP_IRQ_ACOV|AXP_IRQ_USBOV|AXP_IRQ_ICTEMOV)) {
		    if(event & (AXP_IRQ_ACOV|AXP_IRQ_USBOV))
                DBG_PSY_MSG("charger voltage too high!\n");
            else if(event & AXP_IRQ_ICTEMOV)
                DBG_PSY_MSG("IC internal over heat!\n");
        axp_close_input(charger);
        schedule_delayed_work(&charger->check_vbus_low,
                                msecs_to_jiffies(500));
		//axp_change(charger);
	}

	//if(event & (AXP_IRQ_ACRE|AXP_IRQ_USBRE)) {
	//		DBG_PSY_MSG("charger remove\n");
	//	axp_clr_bits(charger->master, 0x30, 0x83);
	//	axp_set_bits(charger->master, 0x30, 0x2);/*VBUS current limit to 100mA*/
	//	axp_clr_bits(charger->master, 0x33, 0xf);/*charging current min*/
	//	vbus_en_pull(1);/*vbus en high*/
	//	schedule_delayed_work(&charger->check_ac,
	//		                        msecs_to_jiffies(500));
	//	axp_change(charger);
	//}

//	if(event & AXP_IRQ_PEKLO) {
//		axp_presslong(charger);
//	}

//	if(event & AXP_IRQ_PEKSH) {
//		axp_pressshort(charger);
//	}

	DBG_PSY_MSG("event = 0x%x\n",(int) event);
#if defined (CONFIG_KP_AXP20)
	axp_writes(charger->master,AXP_INTSTS1,9,w);
#endif
#if defined (CONFIG_KP_AXP19)
	axp_writes(charger->master,AXP_INTSTS1,7,w);
#endif
	return 0;
}
#endif

static char *supply_list[] = {
	"axp-battery",
};  

static void axp_battery_setup_psy(struct axp_charger *charger)
{
	struct power_supply *batt = &charger->batt;
	struct power_supply *ac = &charger->ac;
	struct power_supply *usb = &charger->usb;
	struct power_supply_info *info = charger->battery_info;

	batt->name = "Battery";
	batt->use_for_apm = info->use_for_apm;
	batt->type = POWER_SUPPLY_TYPE_BATTERY;
	batt->get_property = axp_battery_get_property;

	batt->properties = axp_battery_props;
	batt->num_properties = ARRAY_SIZE(axp_battery_props);

	ac->name = "ac";
	ac->type = POWER_SUPPLY_TYPE_MAINS;
	ac->get_property = axp_ac_get_property;

	ac->supplied_to = supply_list,
	ac->num_supplicants = ARRAY_SIZE(supply_list),

	ac->properties = axp_ac_props;
	ac->num_properties = ARRAY_SIZE(axp_ac_props);

	usb->name = "usb";
	usb->type = POWER_SUPPLY_TYPE_USB;
	usb->get_property = axp_usb_get_property;

	usb->supplied_to = supply_list,
	usb->num_supplicants = ARRAY_SIZE(supply_list),

	usb->properties = axp_usb_props;
	usb->num_properties = ARRAY_SIZE(axp_usb_props);
};

#if defined CONFIG_HAS_EARLYSUSPEND
static void axp_earlysuspend(struct early_suspend *h)
{
	DBG_PSY_MSG("======early suspend=======\n");
    
    if(charger_ing == 1)
    {
//#if defined (CONFIG_AXP_CHGCURCHG) || (CONFIG_AXP_DEBUG)
	axp_set_chargecurrent(axpcharger->chgearcur);
//#endif
    }
}
static void axp_lateresume(struct early_suspend *h)
{
	DBG_PSY_MSG("======late resume=======\n");
//#if defined (CONFIG_AXP_CHGCURCHG) || (CONFIG_AXP_DEBUG)
	axp_set_chargecurrent(axpcharger->chgcur);
//#endif
}
#endif

//int charger_flag;/*0 - not initialize, 1 - ac_charger, 2 - usb_charger, 3 - usb_host*/
//static int pre_status;

#if 0
static void axp_check_usb_status(struct delayed_work *work)
{
	uint8_t val;
	struct axp_charger *charger = 
		container_of(work, struct axp_charger, check_work);

    if (charger->ac_det && charger-> usb_det)

    {

     	    DBG_PSY_MSG("---AC and USB IN--- \n");
			axp_read(charger->master, 0x30, &val);
			val &= ~0x3;
			val |= 0x1;
			axp_write(charger->master, 0x30, val);/*vbus current limit to 500mA*/
			axp_write(charger->master, 0x33, 0xc3);/*charging current 600mA*/
			vbus_en_pull(0);/*VBUS_EN low*/
            schedule_delayed_work(&charger->check_vbus_low,msecs_to_jiffies(500));
			axp_change(charger);

	       return 0;
    }


    else if( charger->ac_det == 1)

    {
            DBG_PSY_MSG("--AC IN--\n");
		    axp_read(charger->master, 0x30, &val);
		    val |= 0x3;
		    axp_write(charger->master, 0x30, val);/*vbus current no limit*/
		    axp_write(charger->master, 0x33, 0xc3);/*charging current 600 = 300+3*100 */
		    vbus_en_pull(0);/*VBUS_EN low*/
            schedule_delayed_work(&charger->check_vbus_low,msecs_to_jiffies(500));
		    axp_change(charger);

	       return 0;
     }

}
//wfliao add usb check
static void axp_check_usb_working(struct delayed_work *work)
{
    int status;
	uint8_t val;
	struct axp_charger *charger = container_of(work, struct axp_charger, check_usb_work);
    status = get_otg_charge_mode();
    DBG_PSY_MSG(" --------------status = %d \n",status);

  switch(status) {      
        case 0:/*idle*/       
              DBG_PSY_MSG("## IDLE ###\n");
              //schedule_delayed_work(&charger->check_usb_remove,msecs_to_jiffies(800));        
              schedule_delayed_work(&charger->check_id_high, msecs_to_jiffies(100));
              break;
        case 1:
              break;

        case 2:
#if 0
                if (charger->ac_det == 0) 
                {
                    
                DBG_PSY_MSG("## USB IN ###\n");
			    axp_read(charger->master, 0x30, &val);
			    val &= ~0x3;
			    val |= 0x1;
			    axp_write(charger->master, 0x30, val);/*vbus current limit to 500mA*/

			    axp_write(charger->master, 0x33, 0xc2);/*charging current 500mA*/

			    vbus_en_pull(0);/*VBUS_EN low*/
                schedule_delayed_work(&charger->check_usb_remove,msecs_to_jiffies(800));
			    axp_change(charger);
                }
                else
                {
                DBG_PSY_MSG("## back USB IN ###\n");
                vbus_en_pull(0);/*VBUS_EN low*/
                schedule_delayed_work(&charger->check_usb_remove,msecs_to_jiffies(800));
			    axp_change(charger);

                }
#endif
                vbus_en_pull(1);
                schedule_delayed_work(&charger->check_id_high, msecs_to_jiffies(100));
             break;
        case 3:
		        DBG_PSY_MSG("## only USB HOST ###\n");
                vbus_en_pull(1);
                //schedule_delayed_work(&charger->check_usb_remove,msecs_to_jiffies(800));
                schedule_delayed_work(&charger->check_id_high, msecs_to_jiffies(100));
                break;
    }
      	return 0;
    
}
#endif


static void axp_check_vbus_low(struct delayed_work *work)
{
    struct axp_charger *charger =
            container_of(work, struct axp_charger, check_vbus_low);
    int ret = read_rtc_gpx(2);

    if(ret) {
        schedule_delayed_work(&charger->check_vbus_low,
                                msecs_to_jiffies(500));
    }else {

//        if(charger->usb_det == 0)
//        {
        DBG_PSY_MSG("------VBUS -->> Low------\n");
        charger->flag = 0;
        axp_close_input(charger);
        cancel_delayed_work(&charger->check_vbus_low);
        axp_write(charger->master, 0x33, 0xc2);/*set charging current 500mA*/
        charger_ing =0;
        axp_change(charger);
        schedule_delayed_work(&charger->check_vbus_high,
                                    msecs_to_jiffies(100));
//        }
//        else{
//        DBG_PSY_MSG("-don't close--\n");
//        charger_flag = 0;
//        cancel_delayed_work(&charger->check_vbus_low);
//        schedule_delayed_work(&charger->check_vbus_high,
//                                msecs_to_jiffies(500));
//        axp_change(charger);
//        }
    }

}

static void axp_check_vbus_high(struct delayed_work *work)
{
    struct axp_charger *charger =
            container_of(work, struct axp_charger, check_vbus_high);
    int ret = read_rtc_gpx(2);

    if(ret) {
        DBG_PSY_MSG("------VBUS -->> High------\n");
        cancel_delayed_work(&charger->check_vbus_high);
        axp_write(charger->master, 0x33, 0xc4);/*set charging current 600mA*/
        charger_ing =1;
        axp_change(charger);
        schedule_delayed_work(&charger->check_vbus_low,
                                msecs_to_jiffies(100));
    }else {
        schedule_delayed_work(&charger->check_vbus_high,
                                msecs_to_jiffies(500));
    }
}

#if 0
// wfliao add usb check --------------
static void axp_check_usb_remove(struct delayed_work *work)
{
    int status;
    struct axp_charger *charger = container_of(work, struct axp_charger, check_usb_remove);
    status = get_otg_charge_mode();
    if(status)
    {
        schedule_delayed_work(&charger->check_usb_remove,msecs_to_jiffies(800));
    }
    else
    {
        DBG_PSY_MSG(" axp202 check USB output ---\n");
        axp_close_input(charger);
        cancel_delayed_work(&charger->check_usb_remove);
        schedule_delayed_work(&charger->check_usb_charger,msecs_to_jiffies(800));
        axp_change(charger);
   }
}

static void axp_check_usb_charger(struct delayed_work *work)
{
    int status;
    struct axp_charger *charger = container_of(work, struct axp_charger, check_usb_charger);
    status = get_otg_charge_mode();
    if(status )
    {
        DBG_PSY_MSG(" axp202 check USB input ---\n");
        cancel_delayed_work(&charger->check_usb_charger);
        schedule_delayed_work(&charger->check_usb_work,msecs_to_jiffies(1500));
    }
    else
    {
        schedule_delayed_work(&charger->check_usb_charger,msecs_to_jiffies(800));
   }
}
//---------------------------------------------------------------------------
#endif

static void axp_chargingstate_monitor(struct work_struct *work)
{
    struct axp_charger *charger;
    uint8_t press;
    static int pre_ac;
    charger = container_of(work, struct axp_charger, workstate.work);

    axp_charger_update_state(charger);
    if(pre_ac != charger->ac_valid) {
        DBG_PSY_MSG("battery state change: acvalid:%d->%d\n", pre_ac, charger->ac_valid);
        pre_ac = charger->ac_valid;
        state_count = 1;
        power_supply_changed(&charger->batt);
    }

    schedule_delayed_work(&charger->workstate, charger->intervalstate);
}

static int axp_battery_probe(struct platform_device *pdev)
{
	struct axp_charger *charger;
	struct axp_supply_init_data *pdata = pdev->dev.platform_data;
	int ret;
		DBG_PSY_MSG("axp_battery_probe_m75d\n");

	if (pdata == NULL)
		return -EINVAL;

	/* ����pek������Ϣ */
	ret = axp_set_pekinfo(pdev);
	if(ret){
		DBG_PSY_MSG("[AXP]Unable to set pek input device\n");
		return ret;
	}

	charger = kzalloc(sizeof(*charger), GFP_KERNEL);
	if (charger == NULL)
		return -ENOMEM;

	charger->master = pdev->dev.parent;

	charger->chgcur			= pdata->chgcur;
	charger->chgearcur		= pdata->chgearcur;
	charger->chgsuscur		= pdata->chgsuscur;
	charger->chgclscur		= pdata->chgclscur;
	charger->chgvol			= pdata->chgvol;
	charger->chgend			= pdata->chgend;
	charger->adc_freq		= pdata->adc_freq;
	charger->chgpretime		= pdata->chgpretime;
	charger->chgcsttime		= pdata->chgcsttime;
	charger->battery_info	= pdata->battery_info;
	if(BATT_VOL_OPT){
	    charger->update_flag    = 0;
	    charger->delta          = 0;
	}

    charger->battery_info->energy_full_design =
        item_exist("batt.cap")?item_integer("batt.cap", 0):2300;
	
	/* ����ȫ�ֱ��������ڵ��� */
	axpcharger = charger;

	/* ��ʼ������ */
	ret = axp_battery_first_init(charger);
	if (ret)
		goto err_charger_init;
#if defined (CONFIG_KP_USEIRQ)
	charger->nb.notifier_call = axp_battery_event;
	ret = axp_register_notifier(charger->master, &charger->nb, AXP_NOTIFIER_ON);
	if (ret)
		goto err_notifier;
#endif

	axp_battery_setup_psy(charger);
	ret = power_supply_register(&pdev->dev, &charger->batt);
	if (ret)
		goto err_ps_register;

	ret = power_supply_register(&pdev->dev, &charger->ac);
	if (ret){
		power_supply_unregister(&charger->batt);
		goto err_ps_register;
	}
  	
	ret = power_supply_register(&pdev->dev, &charger->usb);
	if (ret){
		power_supply_unregister(&charger->ac);
		power_supply_unregister(&charger->batt);
		goto err_ps_register;
	}

#if defined (CONFIG_AXP_CHGCURCHG) || (CONFIG_AXP_DEBUG)
	ret = axp_charger_create_attrs(&charger->batt);
	if(ret){
		return ret;
	}
#endif

	platform_set_drvdata(pdev, charger);

	/* ��ʼ����ʾ�����Ϣ */
	axp_charger_update_state(charger);
	axp_charger_update(charger);
	if(charger->bat_det == 1)
		axp_battery_infoinit(charger);

	/* �������µ����Ϣ���� */
	charger->interval = msecs_to_jiffies(TIMER1 * 1000);
	INIT_DELAYED_WORK(&charger->work, axp_charging_monitor);
	if(charger->bat_det == 1){
		schedule_delayed_work(&charger->work, charger->interval);
	} else {
		charger->rest_cap = 100;
	}

	charger->flag = 0;
	//INIT_DELAYED_WORK(&charger->check_work, axp_check_usb_status);
	INIT_DELAYED_WORK(&charger->check_vbus_high, axp_check_vbus_high);
    INIT_DELAYED_WORK(&charger->check_vbus_low, axp_check_vbus_low);
	schedule_delayed_work(&charger->check_vbus_high,
		                        msecs_to_jiffies(2500));

    //wfliao add usb checkout charger-------------------
    //INIT_DELAYED_WORK(&charger->check_usb_work,axp_check_usb_working);
    //INIT_DELAYED_WORK(&charger->check_usb_remove,axp_check_usb_remove);
    //INIT_DELAYED_WORK(&charger->check_usb_charger, axp_check_usb_charger);
	//schedule_delayed_work(&charger->check_usb_charger,msecs_to_jiffies(2600));

#ifndef CONFIG_KP_USEIRQ
	/* δ���ж�ʱ����ѯ��� */
	charger->intervalstate = msecs_to_jiffies(TIMER6 * 100);
	INIT_DELAYED_WORK(&charger->workstate, axp_chargingstate_monitor);
	schedule_delayed_work(&charger->workstate, charger->intervalstate);
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
	axp_early_suspend.suspend = axp_earlysuspend;
	axp_early_suspend.resume = axp_lateresume;
	axp_early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 2;
	register_early_suspend(&axp_early_suspend);
#endif
	/* ���Խӿ�ע�� */
	class_register(&axppower_class);

	return ret;

err_ps_register:
		DBG_PSY_MSG("err_ps_register\n");
#if defined (CONFIG_KP_USEIRQ)
	axp_unregister_notifier(charger->master, &charger->nb, AXP_NOTIFIER_ON);
#endif

#if defined (CONFIG_KP_USEIRQ)
err_notifier:
#endif
		DBG_PSY_MSG("err_notifier\n");
	cancel_delayed_work_sync(&charger->work);

err_charger_init:
		DBG_PSY_MSG("err_charger_init\n");
	kfree(charger);
	//input_unregister_device(powerkeydev);
	//kfree(powerkeydev);

	return ret;
}

static int axp_battery_remove(struct platform_device *dev)
{
	struct axp_charger *charger = platform_get_drvdata(dev);
#if defined (CONFIG_KP_USEIRQ)
	axp_unregister_notifier(charger->master, &charger->nb, AXP_NOTIFIER_ON);
#endif
	cancel_delayed_work_sync(&charger->work);
#ifndef CONFIG_KP_USEIRQ
	cancel_delayed_work_sync(&charger->workstate);
#endif
	power_supply_unregister(&charger->usb);
	power_supply_unregister(&charger->ac);
	power_supply_unregister(&charger->batt);

	kfree(charger);
	//input_unregister_device(powerkeydev);
	//kfree(powerkeydev);

	return 0;
}

static void axp_shutdown(struct platform_device *dev)
{
	struct axp_charger *charger = platform_get_drvdata(dev);

	cancel_delayed_work_sync(&charger->work);
#ifndef CONFIG_KP_USEIRQ
	cancel_delayed_work_sync(&charger->workstate);
#endif
	axp_set_rdcclose();
	/*���ùػ�������*/
//#if defined (CONFIG_AXP_CHGCURCHG) || (CONFIG_AXP_DEBUG)
	axp_set_chargecurrent(charger->chgclscur);
//#endif
}

static struct platform_driver axp_battery_driver = {
	.driver 	= {
		.name 	= "axp-supplyer",
		.owner	= THIS_MODULE,
	},
	.probe 		= axp_battery_probe,
	.remove 	= axp_battery_remove,
	.suspend	= axp_suspend,
	.resume 	= axp_resume,
	.shutdown	= axp_shutdown,
};

static int axp_battery_init(void)
{
    if(item_equal("charger.pattern", "mode4", 0)) {
	    return platform_driver_register(&axp_battery_driver);
    }else {
        return -1;
    }
}

static void axp_battery_exit(void)
{
	platform_driver_unregister(&axp_battery_driver);
}

subsys_initcall(axp_battery_init);
module_exit(axp_battery_exit);

MODULE_DESCRIPTION("Battery Driver for X-Powers AXP PMIC");
MODULE_AUTHOR("Donglu Zhang, <zhangdonglu@x-powers.com>");
MODULE_LICENSE("GPL");
