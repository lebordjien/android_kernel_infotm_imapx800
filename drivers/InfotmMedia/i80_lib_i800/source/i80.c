/***************************************************************************** 
** 
** Copyright (c) 2012~2112 ShangHai Infotm Ltd all rights reserved. 
** 
** Use of Infotm's code is governed by terms and conditions 
** stated in the accompanying licensing statement. 
** 
** Description: Implementation file of i80 library internal logical call
**
** Author:
**     Sam Ye<weize_ye@infotm.com>
**      
** Revision History: 
** ----------------- 
** 1.0.1	 Sam@2012/3/20 :  first commit
**
*****************************************************************************/

#include <InfotmMedia.h>
#include <i80.h>
#include <i80_reg.h>

static enum_i80if_supports_lcd g_eI80_current_lcd = I80IF_LCD_0;	// The current lcd 
// Not OK for two ids. Later should rewrite all the codes when time permitted.


void i80api_set_idsxNo(IM_UINT32 idsx)
{
	i80reg_set_idsxNo(idsx);
}

/*===============================================
func: �ȴ�i80����
log:
-----------------------------------------------*/
void
i80api_wait_idle(void)
{
	// wait idle
	while( i80reg_get_status() &
			(I80IF_STAT_TRANSMITTING_NORMAL_CMD |
			 I80IF_STAT_TRANSMITTING_AUTO_CMD |
			 I80IF_STAT_TRANSMITTING_FRAME |
			 I80IF_STAT_NORMAL_CMD_STARTED) );
}

/*===============================================
func: ��ʼ��i80if����������ʼ���Ĵ�����ʹ��
log:
-----------------------------------------------*/
IM_UINT32	// 1--�ɹ�, 0--ʧ��
i80api_init(
		enum_i80if_supports_lcd eLcd,	// Ҫ���õ�Lcd
		struct_i80if *pI80if 			// ���ò���
		)
{
	pI80if->eMain_lcd = eLcd;
	pI80if->InitedFlag = 1;

	return 1;
}

/*===============================================
func: ��ȡ��ǰi80if״̬
log:
-----------------------------------------------*/
IM_UINT32
i80api_get_status(void)
{
    return i80reg_get_status();
}

/*===============================================
func: ʹ��/��ֹ i80ifģ��
log:
-----------------------------------------------*/
void
i80api_if_enable(IM_UINT32 fEn)
{
	i80reg_if_enable(fEn);

}

/*===============================================
func: ʹ��/��ֹ lcd
log:
-----------------------------------------------*/
void
i80api_lcd_enable(IM_UINT32 fEn)
{
	i80reg_lcd_enable(fEn);
}

/*===============================================
func: ����cmd list, ��ǰѡ�е��Ǹ�LCDʱ����ȥ�޸ļĴ���
log:
-----------------------------------------------*/
void
i80api_set_cmd_list(
		struct_i80if *pI80if,
		enum_i80if_supports_lcd eCurrent_lcd,
		//enum_i80if_supports_lcd eLcd,		// Ҫ���õ�Lcd
		struct_i80if_cmd_property *pCmd,	// cmd����
		IM_INT32 nIndex 	// ��16��cmd fifo�е�����,0-15
		)
{
	IM_UINT32 high_byte, low_byte;

	if( (pI80if->eMain_lcd != I80IF_LCD_0) && (pI80if->eMain_lcd != I80IF_LCD_1) ){
		return;
	}

	if( (nIndex >= I80IF_CMD_FIFO_NUM) || (nIndex < 0) ){
		return;
	}

	pI80if->stCmd[nIndex].uCmd = pCmd->uCmd;
	pI80if->stCmd[nIndex].eCmd_type = pCmd->eCmd_type;
	pI80if->stCmd[nIndex].eRS_level = pCmd->eRS_level;

	// ��ǰѡ�е��Ǹ�LCDʱ����ȥ�޸ļĴ���������ֻ�Ǳ������ü�¼
	if( pI80if->eMain_lcd == eCurrent_lcd )
	{
		i80api_wait_idle();
		switch( pI80if->eData_format )
		{
			case I80IF_BUS16_00_11_x_x:
			case I80IF_BUS8_0x_10_0_0:
				break;

			case I80IF_BUS18_xx_xx_x_x:
			case I80IF_BUS16_11_11_x_x:
			case I80IF_BUS16_11_10_0_0:
			case I80IF_BUS16_11_01_0_1:
				high_byte = (pI80if->stCmd[nIndex].uCmd & 0xff00) << 2;	//���ֽ���DB10�Ͽ�ʼ
				low_byte = (pI80if->stCmd[nIndex].uCmd & 0x00ff) << 1;	//���ֽ���DB1�Ͽ�ʼ
				pI80if->stCmd[nIndex].uCmd = high_byte | low_byte;
				break;

			default:
				break;
		}

		i80reg_set_cmd_list(&pI80if->stCmd[nIndex], nIndex);
	}
}

/*===============================================
func: ���cmd list, ��ǰѡ�е��Ǹ�LCDʱ����ȥ�޸ļĴ���
log:
-----------------------------------------------*/
void
i80api_clear_cmd_list(
		struct_i80if *pI80if,
		enum_i80if_supports_lcd eCurrent_lcd
		)
{
	IM_INT32 i;

	if( (pI80if->eMain_lcd != I80IF_LCD_0) && (pI80if->eMain_lcd != I80IF_LCD_1) ){
		return;
	}

	for(i=0; i<I80IF_CMD_FIFO_NUM; i++){
		pI80if->stCmd[i].uCmd = 0;
		pI80if->stCmd[i].eCmd_type = I80IF_CMD_TYPE_DISABLE;
		pI80if->stCmd[i].eRS_level = I80IF_RS_LEVEL_LOW;
	}

	// �����ĵ�ǰLcd�����б�����ʱ��ȥ�޸ļĴ���
	if( pI80if->eMain_lcd == eCurrent_lcd){
		i80api_wait_idle();
		i80reg_clear_cmd_list();
	}
}

/*===============================================
func: ʹ��/��ֹnormal cmd,normal cmd��ָֻ����cmd list����������������
log:
-----------------------------------------------*/
IM_UINT32	// 1-�ɹ���0-ʧ��
i80api_transmit_normal_cmd(
		IM_UINT32 fEn,			// 1--start, 0--stop
		IM_UINT32 fWaitComplete,	// �Ƿ�ȴ�normal cmd���ͽ���
		IM_UINT32 i80if_en
		)
{
	if( i80if_en /*g_fI80if_en*/ == 0 ){
		return 0;
	}

	if( fEn )
	{
		i80api_wait_idle();
		i80reg_transmit_normal_cmd(1);

		if( fWaitComplete ){
			i80api_wait_idle();
		}
	}else{
		i80reg_transmit_normal_cmd(0);
	}

	return 1;
}

/*===============================================
func: ���õ�ǰʹ�ܵ�LCD��ͬʱ����i80if�Ĵ���������CS
log:
-----------------------------------------------*/
IM_UINT32	// 1--�ɹ���0--ʧ��
i80api_set_current_lcd(
		struct_i80if *pI80if,
		IM_UINT32 i80if_en
		)
{
	IM_UINT32 fi80EnableFlag;    // ���浱ǰi80ifʹ�ܱ�־

	if( (pI80if->eMain_lcd != I80IF_LCD_0) && (pI80if->eMain_lcd != I80IF_LCD_1) ){
		return 0;
	}

	// �����lcdû�б���ʼ��������᷵��0
	if( pI80if->InitedFlag == 0 ){
		return 0;
	}

	fi80EnableFlag = i80if_en;
	if( fi80EnableFlag == 1 )
		i80api_if_enable(0);
	i80api_wait_idle();
	i80reg_init(pI80if);
	g_eI80_current_lcd = pI80if->eMain_lcd;		// ���õ�ǰʹ�ܵ�lcd
	if( fi80EnableFlag == 1 )
		i80api_if_enable(1);

	return 1;
}

/*===============================================
func: manual������onceֻ����rd, wr�����ݣ�cs,rsҪ������ơ�cs���ǵ�ǰ���õ���Чcs��
log:
-----------------------------------------------*/
void
i80api_manual_write_once(
		IM_UINT32 data
		)
{
	i80reg_manual_write_once(data);
}

void
i80api_manual_read_once(
		IM_UINT32 *pData
		)
{
	i80reg_manual_read_once(pData);
}

void
i80api_manual_low_cs(void)
{
	if( g_eI80_current_lcd == I80IF_LCD_0 ){
		i80if_manual_sig_cs0_low();
	}else{
		i80if_manual_sig_cs1_low();
	}
}

void
i80api_manual_high_cs(void)
{
	if( g_eI80_current_lcd == I80IF_LCD_0 ){
		i80if_manual_sig_cs0_high();
	}else{
		i80if_manual_sig_cs1_high();
	}
}

IM_UINT32	// ��ʼ����ʹ��manual��ʽ�������ʱi80ģ��û��ʹ�ܣ��򷵻�0
i80api_manual_init(void)
{
	i80if_manual_sig_init();
	return 1;
}

void
i80api_manual_deinit(void)
{
	i80if_manual_sig_deinit();
}

void
i80api_manual_low_rs(void)
{
	i80if_manual_sig_rs_low();
}

void
i80api_manual_high_rs(void)
{
	i80if_manual_sig_rs_high();
}

/*===============================================
func:
log:
-----------------------------------------------*/
void
i80api_set_data_format(
		struct_i80if *pI80if,
		enum_i80if_supports_lcd eCurrent_lcd,
		enum_i80if_data_format fmt
		)
{
	if( (pI80if->eMain_lcd != I80IF_LCD_0) && (pI80if->eMain_lcd != I80IF_LCD_1) ){
		return;
	}	
	
	pI80if->eData_format = fmt;
	
	// �����ĵ�ǰLCD���ݸ�ʽʱ��ȥ�޸ļĴ���
	if( eCurrent_lcd == pI80if->eMain_lcd){
		i80reg_set_data_format(fmt);
	}
}


