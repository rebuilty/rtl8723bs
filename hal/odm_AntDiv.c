/******************************************************************************
 *
 * Copyright(c) 2007 - 2011 Realtek Corporation. All rights reserved.
 *                                        
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA
 *
 *
 ******************************************************************************/

//============================================================
// include files
//============================================================

#include "odm_precomp.h"

//======================================================
// when antenna test utility is on or some testing need to disable antenna diversity
// call this function to disable all ODM related mechanisms which will switch antenna.
//======================================================
void
ODM_StopAntennaSwitchDm(
	IN	PDM_ODM_T	pDM_Odm
	)
{
	// disable ODM antenna diversity
	pDM_Odm->SupportAbility &= ~ODM_BB_ANT_DIV;
	ODM_RT_TRACE(pDM_Odm,ODM_COMP_ANT_DIV, ODM_DBG_LOUD, ("STOP Antenna Diversity \n"));
}

void
ODM_SetAntConfig(
	IN	PDM_ODM_T	pDM_Odm,
	IN	u1Byte		antSetting	// 0=A, 1=B, 2=C, ....
	)
{
	if(antSetting == 0)		// ant A
		ODM_SetBBReg(pDM_Odm, 0x948, bMaskDWord, 0x00000000);
	else if(antSetting == 1)
		ODM_SetBBReg(pDM_Odm, 0x948, bMaskDWord, 0x00000280);
}

//======================================================


void
ODM_SwAntDivRestAfterLink(
	IN		PDM_ODM_T		pDM_Odm
	)
{
	pSWAT_T		pDM_SWAT_Table = &pDM_Odm->DM_SWAT_Table;
	pFAT_T		pDM_FatTable = &pDM_Odm->DM_FatTable;
	u4Byte             i;

	pDM_Odm->RSSI_test = false;
	pDM_SWAT_Table->try_flag = 0xff;
	pDM_SWAT_Table->RSSI_Trying = 0;
	pDM_SWAT_Table->Double_chk_flag= 0;
	
	pDM_FatTable->RxIdleAnt=MAIN_ANT;
	
	for (i=0; i<ODM_ASSOCIATE_ENTRY_NUM; i++)
	{
		pDM_FatTable->MainAnt_Sum[i] = 0;
		pDM_FatTable->AuxAnt_Sum[i] = 0;
		pDM_FatTable->MainAnt_Cnt[i] = 0;
		pDM_FatTable->AuxAnt_Cnt[i] = 0;
	}
}


#if (defined(CONFIG_HW_ANTENNA_DIVERSITY))
void
odm_AntDiv_on_off( IN PDM_ODM_T	pDM_Odm ,IN u1Byte swch)
{
	if(pDM_Odm->AntDivType==S0S1_SW_ANTDIV || pDM_Odm->AntDivType==CGCS_RX_SW_ANTDIV) 
		return;

	ODM_RT_TRACE(pDM_Odm,ODM_COMP_ANT_DIV, ODM_DBG_LOUD, ("(( Turn %s )) N-Series HW-AntDiv block\n",(swch==ANTDIV_ON)?"ON" : "OFF"));
	ODM_SetBBReg(pDM_Odm, 0xc50 , BIT7, swch); //OFDM AntDiv function block enable
	if( pDM_Odm->AntDivType != S0S1_SW_ANTDIV)
	        ODM_SetBBReg(pDM_Odm, 0xa00 , BIT15, swch); //CCK AntDiv function block enable
}

void
ODM_UpdateRxIdleAnt(IN PDM_ODM_T pDM_Odm, IN u1Byte Ant)
{
	pFAT_T	pDM_FatTable = &pDM_Odm->DM_FatTable;
	u4Byte	DefaultAnt, OptionalAnt,value32;

	//PADAPTER 		pAdapter = pDM_Odm->Adapter;
	//HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(pAdapter);

	if(pDM_FatTable->RxIdleAnt != Ant)
	{
		ODM_RT_TRACE(pDM_Odm,ODM_COMP_ANT_DIV, ODM_DBG_LOUD, ("[ Update Rx-Idle-Ant ] RxIdleAnt =%s\n",(Ant==MAIN_ANT)?"MAIN_ANT":"AUX_ANT"));

		if(Ant == MAIN_ANT)
		{
			DefaultAnt   =  ANT1_2G; 
			OptionalAnt =  ANT2_2G; 
		}
		else
		{
			DefaultAnt  =   ANT2_2G;
			OptionalAnt =  ANT1_2G;
		}
	
		value32 = ODM_GetBBReg(pDM_Odm, 0x948, 0xFFF);
		
		if (value32 !=0x280)
		        ODM_UpdateRxIdleAnt_8723B(pDM_Odm, Ant, DefaultAnt, OptionalAnt);
	        else
		        ODM_RT_TRACE(pDM_Odm,ODM_COMP_ANT_DIV, ODM_DBG_LOUD, ("[ Update Rx-Idle-Ant ] 8723B: Fail to set RX antenna due to 0x948 = 0x280\n"));

		ODM_SetMACReg(pDM_Odm, 0x6D8 , BIT10|BIT9|BIT8, DefaultAnt);	//PathA Resp Tx
	}
	else// pDM_FatTable->RxIdleAnt == Ant
	{
		ODM_RT_TRACE(pDM_Odm,ODM_COMP_ANT_DIV, ODM_DBG_LOUD, ("[ Stay in Ori-Ant ]  RxIdleAnt =%s\n",(Ant==MAIN_ANT)?"MAIN_ANT":"AUX_ANT"));
		pDM_FatTable->RxIdleAnt = Ant;
	}
}


void
odm_UpdateTxAnt(IN PDM_ODM_T pDM_Odm, IN u1Byte Ant, IN u4Byte MacId)
{
	pFAT_T	pDM_FatTable = &pDM_Odm->DM_FatTable;
	u1Byte	TxAnt;

	if(Ant == MAIN_ANT)
		TxAnt = ANT1_2G;
	else
		TxAnt = ANT2_2G;
	
	pDM_FatTable->antsel_a[MacId] = TxAnt&BIT0;
	pDM_FatTable->antsel_b[MacId] = (TxAnt&BIT1)>>1;
	pDM_FatTable->antsel_c[MacId] = (TxAnt&BIT2)>>2;
	//ODM_RT_TRACE(pDM_Odm,ODM_COMP_ANT_DIV, ODM_DBG_LOUD, ("[Tx from TxInfo]: MacID:(( %d )),  TxAnt = (( %s ))\n", 
	//					MacId,(Ant==MAIN_ANT)?"MAIN_ANT":"AUX_ANT"));
	//ODM_RT_TRACE(pDM_Odm,ODM_COMP_ANT_DIV, ODM_DBG_LOUD,("antsel_tr_mux=(( 3'b%d%d%d ))\n",
							//pDM_FatTable->antsel_c[MacId] , pDM_FatTable->antsel_b[MacId] , pDM_FatTable->antsel_a[MacId] ));
	
}

void
odm_TRX_HWAntDiv_Init_8723B(
	IN		PDM_ODM_T		pDM_Odm
)
{
	ODM_RT_TRACE(pDM_Odm,ODM_COMP_ANT_DIV, ODM_DBG_LOUD, ("***8723B AntDiv_Init =>  AntDivType=[CG_TRX_HW_ANTDIV(DPDT)]\n"));
      
	//Mapping Table
	ODM_SetBBReg(pDM_Odm, 0x914 , bMaskByte0, 0);
	ODM_SetBBReg(pDM_Odm, 0x914 , bMaskByte1, 1);
	
	//OFDM HW AntDiv Parameters
	ODM_SetBBReg(pDM_Odm, 0xCA4 , 0x7FF, 0xa0); //thershold
	ODM_SetBBReg(pDM_Odm, 0xCA4 , 0x7FF000, 0x00); //bias
		
	//CCK HW AntDiv Parameters
	ODM_SetBBReg(pDM_Odm, 0xA74 , BIT7, 1); //patch for clk from 88M to 80M
	ODM_SetBBReg(pDM_Odm, 0xA0C , BIT4, 1); //do 64 samples
	
	//BT Coexistence
	ODM_SetBBReg(pDM_Odm, 0x864, BIT12, 0); //keep antsel_map when GNT_BT = 1
	ODM_SetBBReg(pDM_Odm, 0x874 , BIT23, 0); //Disable hw antsw & fast_train.antsw when GNT_BT=1

        //Output Pin Settings
	ODM_SetBBReg(pDM_Odm, 0x870 , BIT8, 0); //
		
	ODM_SetBBReg(pDM_Odm, 0x948 , BIT6, 0); //WL_BB_SEL_BTG_TRXG_anta,  (1: HW CTRL  0: SW CTRL)
	ODM_SetBBReg(pDM_Odm, 0x948 , BIT7, 0);
		
	ODM_SetMACReg(pDM_Odm, 0x40 , BIT3, 1);
	ODM_SetMACReg(pDM_Odm, 0x38 , BIT11, 1);
	ODM_SetMACReg(pDM_Odm, 0x4C ,  BIT24|BIT23, 2); //select DPDT_P and DPDT_N as output pin
		
	ODM_SetBBReg(pDM_Odm, 0x944 , BIT0|BIT1, 3); //in/out
	ODM_SetBBReg(pDM_Odm, 0x944 , BIT31, 0); //

	ODM_SetBBReg(pDM_Odm, 0x92C , BIT1, 0); //DPDT_P non-inverse
	ODM_SetBBReg(pDM_Odm, 0x92C , BIT0, 1); //DPDT_N inverse

	ODM_SetBBReg(pDM_Odm, 0x930 , 0xF0, 8); // DPDT_P = ANTSEL[0]
	ODM_SetBBReg(pDM_Odm, 0x930 , 0xF, 8); // DPDT_N = ANTSEL[0]

	//Timming issue
	ODM_SetBBReg(pDM_Odm, 0xE20 , BIT23|BIT22|BIT21|BIT20, 8); //keep antidx after tx for ACK ( unit x 32 mu sec)

	//2 [--For HW Bug Setting]
	if(pDM_Odm->AntType == ODM_AUTO_ANT)
		ODM_SetBBReg(pDM_Odm, 0xA00 , BIT15, 0); //CCK AntDiv function block enable

	//ODM_SetBBReg(pDM_Odm, 0x80C , BIT21, 0); //TX Ant  by Reg


}

	

void
odm_S0S1_SWAntDiv_Init_8723B(
	IN		PDM_ODM_T		pDM_Odm
)
{
	pSWAT_T		pDM_SWAT_Table = &pDM_Odm->DM_SWAT_Table;
	pFAT_T		pDM_FatTable = &pDM_Odm->DM_FatTable;

	ODM_RT_TRACE(pDM_Odm, ODM_COMP_ANT_DIV, ODM_DBG_LOUD, ("***8723B AntDiv_Init => AntDivType=[ S0S1_SW_AntDiv] \n"));

	//Mapping Table
	ODM_SetBBReg(pDM_Odm, 0x914 , bMaskByte0, 0);
	ODM_SetBBReg(pDM_Odm, 0x914 , bMaskByte1, 1);
	
	//Output Pin Settings
	//ODM_SetBBReg(pDM_Odm, 0x948 , BIT6, 0x1); 
	ODM_SetBBReg(pDM_Odm, 0x870 , BIT9|BIT8, 0); 

	pDM_FatTable->bBecomeLinked  =false;
	pDM_SWAT_Table->try_flag = 0xff;	
	pDM_SWAT_Table->Double_chk_flag = 0;
	pDM_SWAT_Table->TrafficLoad = TRAFFIC_LOW;

	//Timming issue
	ODM_SetBBReg(pDM_Odm, 0xE20 , BIT23|BIT22|BIT21|BIT20, 8); //keep antidx after tx for ACK ( unit x 32 mu sec)
	
	//2 [--For HW Bug Setting]
	ODM_SetBBReg(pDM_Odm, 0x80C , BIT21, 0); //TX Ant  by Reg

}

void
odm_S0S1_SWAntDiv_Reset_8723B(
	IN		PDM_ODM_T		pDM_Odm
)
{
	pSWAT_T		pDM_SWAT_Table = &pDM_Odm->DM_SWAT_Table;
	pFAT_T			pDM_FatTable = &pDM_Odm->DM_FatTable;
    
	pDM_FatTable->bBecomeLinked  =false;
	pDM_SWAT_Table->try_flag = 0xff;	
	pDM_SWAT_Table->Double_chk_flag = 0;
	pDM_SWAT_Table->TrafficLoad = TRAFFIC_LOW;

	ODM_RT_TRACE(pDM_Odm,ODM_COMP_ANT_DIV, ODM_DBG_LOUD, ("odm_S0S1_SWAntDiv_Reset_8723B(): pDM_FatTable->bBecomeLinked = %d\n", pDM_FatTable->bBecomeLinked));
}

void
ODM_UpdateRxIdleAnt_8723B(
	IN		PDM_ODM_T		pDM_Odm,
	IN		u1Byte			Ant,
	IN		u4Byte			DefaultAnt, 
	IN		u4Byte			OptionalAnt
)
{
	pFAT_T			pDM_FatTable = &pDM_Odm->DM_FatTable;
	PADAPTER 		pAdapter = pDM_Odm->Adapter;
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(pAdapter);
	u1Byte			count=0;
	u1Byte			u1Temp;
	u1Byte			H2C_Parameter;
	u4Byte			value32;
	
	if(!pDM_Odm->bLinked)
	{
		ODM_RT_TRACE(pDM_Odm,ODM_COMP_ANT_DIV, ODM_DBG_LOUD, ("[ Update Rx-Idle-Ant ] 8723B: Fail to set RX antenna due to no link\n"));
		return;
	}

	// Send H2C command to FW
	// Enable wifi calibration
	H2C_Parameter = true;
	ODM_FillH2CCmd(pDM_Odm, ODM_H2C_WIFI_CALIBRATION, 1, &H2C_Parameter);

	// Check if H2C command sucess or not (0x1e6)
	u1Temp = ODM_Read1Byte(pDM_Odm, 0x1e6);
	while((u1Temp != 0x1) && (count < 100))
	{
		udelay(10);	
		u1Temp = ODM_Read1Byte(pDM_Odm, 0x1e6);
		count++;
	}
	ODM_RT_TRACE(pDM_Odm,ODM_COMP_ANT_DIV, ODM_DBG_LOUD, ("[ Update Rx-Idle-Ant ] 8723B: H2C command status = %d, count = %d\n", u1Temp, count));

	if(u1Temp == 0x1)
	{
		// Check if BT is doing IQK (0x1e7)
		count = 0;
		u1Temp = ODM_Read1Byte(pDM_Odm, 0x1e7);
		while((!(u1Temp & BIT0))  && (count < 100))
		{
			udelay(50);	
			u1Temp = ODM_Read1Byte(pDM_Odm, 0x1e7);
			count++;
		}
		ODM_RT_TRACE(pDM_Odm,ODM_COMP_ANT_DIV, ODM_DBG_LOUD, ("[ Update Rx-Idle-Ant ] 8723B: BT IQK status = %d, count = %d\n", u1Temp, count));

		if(u1Temp & BIT0)
		{
			ODM_SetBBReg(pDM_Odm, 0x948 , BIT6, 0x1);
			ODM_SetBBReg(pDM_Odm, 0x948 , BIT9, DefaultAnt);	
			ODM_SetBBReg(pDM_Odm, 0x864 , BIT5|BIT4|BIT3, DefaultAnt);	//Default RX
			ODM_SetBBReg(pDM_Odm, 0x864 , BIT8|BIT7|BIT6, OptionalAnt);	//Optional RX
			ODM_SetBBReg(pDM_Odm, 0x860, BIT14|BIT13|BIT12, DefaultAnt); //Default TX	
			pDM_FatTable->RxIdleAnt = Ant;

			// Set TX AGC by S0/S1
			// Need to consider Linux driver
			rtw_hal_set_tx_power_level(pAdapter, pHalData->CurrentChannel);

			// Set IQC by S0/S1
			ODM_SetIQCbyRFpath(pDM_Odm,DefaultAnt);
			ODM_RT_TRACE(pDM_Odm,ODM_COMP_ANT_DIV, ODM_DBG_LOUD, ("[ Update Rx-Idle-Ant ] 8723B: Sucess to set RX antenna\n"));
		}
		else
			ODM_RT_TRACE(pDM_Odm,ODM_COMP_ANT_DIV, ODM_DBG_LOUD, ("[ Update Rx-Idle-Ant ] 8723B: Fail to set RX antenna due to BT IQK\n"));
	}
	else
		ODM_RT_TRACE(pDM_Odm,ODM_COMP_ANT_DIV, ODM_DBG_LOUD, ("[ Update Rx-Idle-Ant ] 8723B: Fail to set RX antenna due to H2C command fail\n"));

	// Send H2C command to FW
	// Disable wifi calibration
	H2C_Parameter = false;
	ODM_FillH2CCmd(pDM_Odm, ODM_H2C_WIFI_CALIBRATION, 1, &H2C_Parameter);

}

void
odm_HW_AntDiv(
	IN		PDM_ODM_T		pDM_Odm
)
{
	u4Byte	i,MinMaxRSSI=0xFF, AntDivMaxRSSI=0, MaxRSSI=0, LocalMaxRSSI;
	u4Byte	Main_RSSI, Aux_RSSI, pkt_ratio_m=0, pkt_ratio_a=0,pkt_threshold=10;
	u1Byte	RxIdleAnt=0, TargetAnt=7;
	pFAT_T	pDM_FatTable = &pDM_Odm->DM_FatTable;
	pDIG_T	pDM_DigTable = &pDM_Odm->DM_DigTable;
	PSTA_INFO_T   	pEntry;

	if(!pDM_Odm->bLinked) //bLinked==False
	{
		ODM_RT_TRACE(pDM_Odm,ODM_COMP_ANT_DIV, ODM_DBG_LOUD, ("[No Link!!!]\n"));

		if(pDM_FatTable->bBecomeLinked == true)
		{
			odm_AntDiv_on_off(pDM_Odm, ANTDIV_OFF);
			ODM_UpdateRxIdleAnt(pDM_Odm, MAIN_ANT);

			pDM_FatTable->bBecomeLinked = pDM_Odm->bLinked;
		}
		return;
	}	
	else
	{
		if(pDM_FatTable->bBecomeLinked ==false)
		{
			ODM_RT_TRACE(pDM_Odm,ODM_COMP_ANT_DIV, ODM_DBG_LOUD, ("[Linked !!!]\n"));
			odm_AntDiv_on_off(pDM_Odm, ANTDIV_ON);
			//if(pDM_Odm->SupportICType == ODM_RTL8821 )
				//ODM_SetBBReg(pDM_Odm, 0x800 , BIT25, 0); //CCK AntDiv function disable

			//else if(pDM_Odm->SupportICType == ODM_RTL8723B ||pDM_Odm->SupportICType == ODM_RTL8812)
				//ODM_SetBBReg(pDM_Odm, 0xA00 , BIT15, 0); //CCK AntDiv function disable
			
			pDM_FatTable->bBecomeLinked = pDM_Odm->bLinked;

			if(pDM_Odm->AntDivType == CG_TRX_HW_ANTDIV)
			{
				ODM_SetBBReg(pDM_Odm, 0x930 , 0xF0, 8); // DPDT_P = ANTSEL[0]   // for 8723B AntDiv function patch.  BB  Dino  130412	
				ODM_SetBBReg(pDM_Odm, 0x930 , 0xF, 8); // DPDT_N = ANTSEL[0]
			}
		}	
	}	

	ODM_RT_TRACE(pDM_Odm,ODM_COMP_ANT_DIV, ODM_DBG_LOUD, ("\n[HW AntDiv] Start =>\n"));
	   
	for (i=0; i<ODM_ASSOCIATE_ENTRY_NUM; i++)
	{
		pEntry = pDM_Odm->pODM_StaInfo[i];
		if(IS_STA_VALID(pEntry))
		{
			//2 Caculate RSSI per Antenna
			Main_RSSI = (pDM_FatTable->MainAnt_Cnt[i]!=0)?(pDM_FatTable->MainAnt_Sum[i]/pDM_FatTable->MainAnt_Cnt[i]):0;
			Aux_RSSI = (pDM_FatTable->AuxAnt_Cnt[i]!=0)?(pDM_FatTable->AuxAnt_Sum[i]/pDM_FatTable->AuxAnt_Cnt[i]):0;
			TargetAnt = (Main_RSSI==Aux_RSSI)?pDM_FatTable->RxIdleAnt:((Main_RSSI>=Aux_RSSI)?MAIN_ANT:AUX_ANT);
			/*
			if( pDM_FatTable->MainAnt_Cnt[i]!=0 && pDM_FatTable->AuxAnt_Cnt[i]!=0 )
			{
			pkt_ratio_m=( pDM_FatTable->MainAnt_Cnt[i] / pDM_FatTable->AuxAnt_Cnt[i] );
			pkt_ratio_a=( pDM_FatTable->AuxAnt_Cnt[i] / pDM_FatTable->MainAnt_Cnt[i] );
				
				if (pkt_ratio_m >= pkt_threshold)
					TargetAnt=MAIN_ANT;
				
				else if(pkt_ratio_a >= pkt_threshold)
					TargetAnt=AUX_ANT;
			}
			*/			
			//ODM_RT_TRACE(pDM_Odm, ODM_COMP_ANT_DIV,ODM_DBG_LOUD,("*** SupportICType=[%lu] \n",pDM_Odm->SupportICType));
			ODM_RT_TRACE(pDM_Odm,ODM_COMP_ANT_DIV, ODM_DBG_LOUD, ("***  Main_Cnt = (( %d ))  , Main_RSSI= ((  %d )) \n", pDM_FatTable->MainAnt_Cnt[i], Main_RSSI));
			ODM_RT_TRACE(pDM_Odm,ODM_COMP_ANT_DIV, ODM_DBG_LOUD, ("***  Aux_Cnt   = (( %d ))  , Aux_RSSI = ((  %d )) \n", pDM_FatTable->AuxAnt_Cnt[i]  , Aux_RSSI ));
			ODM_RT_TRACE(pDM_Odm,ODM_COMP_ANT_DIV, ODM_DBG_LOUD, ("*** MAC ID:[ %d ] , TargetAnt = (( %s )) \n", i ,( TargetAnt ==MAIN_ANT)?"MAIN_ANT":"AUX_ANT"));

			ODM_RT_TRACE(pDM_Odm, ODM_COMP_ANT_DIV,ODM_DBG_LOUD,("*** Phy_AntSel_A=[ %d, %d, %d] \n",((pDM_Odm->DM_FatTable.antsel_rx_keep_0)&BIT2)>>2,
				                                                                              ((pDM_Odm->DM_FatTable.antsel_rx_keep_0)&BIT1) >>1, ((pDM_Odm->DM_FatTable.antsel_rx_keep_0)&BIT0)));

			LocalMaxRSSI = (Main_RSSI>Aux_RSSI)?Main_RSSI:Aux_RSSI;
			//2 Select MaxRSSI for DIG
			if((LocalMaxRSSI > AntDivMaxRSSI) && (LocalMaxRSSI < 40))
				AntDivMaxRSSI = LocalMaxRSSI;
			if(LocalMaxRSSI > MaxRSSI)
				MaxRSSI = LocalMaxRSSI;

			//2 Select RX Idle Antenna
			if ( (LocalMaxRSSI != 0) &&  (LocalMaxRSSI < MinMaxRSSI) )
			{
				RxIdleAnt = TargetAnt;
				MinMaxRSSI = LocalMaxRSSI;
			}
			/*
			if((pDM_FatTable->RxIdleAnt == MAIN_ANT) && (Main_RSSI == 0))
				Main_RSSI = Aux_RSSI;
			else if((pDM_FatTable->RxIdleAnt == AUX_ANT) && (Aux_RSSI == 0))
				Aux_RSSI = Main_RSSI;
		
			LocalMinRSSI = (Main_RSSI>Aux_RSSI)?Aux_RSSI:Main_RSSI;
			if(LocalMinRSSI < MinRSSI)
			{
				MinRSSI = LocalMinRSSI;
				RxIdleAnt = TargetAnt;
			}	
			*/
			//2 Select TX Antenna

			#if TX_BY_REG
			
			#else
				if(pDM_Odm->AntDivType != CGCS_RX_HW_ANTDIV)
					odm_UpdateTxAnt(pDM_Odm, TargetAnt, i);
			#endif

		}
		pDM_FatTable->MainAnt_Sum[i] = 0;
		pDM_FatTable->AuxAnt_Sum[i] = 0;
		pDM_FatTable->MainAnt_Cnt[i] = 0;
		pDM_FatTable->AuxAnt_Cnt[i] = 0;
	}
       
	//2 Set RX Idle Antenna
	ODM_UpdateRxIdleAnt(pDM_Odm, RxIdleAnt);

	if(AntDivMaxRSSI == 0)
		pDM_DigTable->AntDiv_RSSI_max = pDM_Odm->RSSI_Min;
	else
        	pDM_DigTable->AntDiv_RSSI_max = AntDivMaxRSSI;
	pDM_DigTable->RSSI_max = MaxRSSI;
}

void
odm_S0S1_SwAntDiv(
	IN		PDM_ODM_T		pDM_Odm,
	IN		u1Byte			Step
	)
{
	u4Byte			i,MinMaxRSSI=0xFF, LocalMaxRSSI,LocalMinRSSI;
	u4Byte			Main_RSSI, Aux_RSSI;
	u1Byte			reset_period=10, SWAntDiv_threshold=35;
	u1Byte			HighTraffic_TrainTime_U=0x32,HighTraffic_TrainTime_L=0,Train_time_temp;
	u1Byte			LowTraffic_TrainTime_U=200,LowTraffic_TrainTime_L=0;
	pSWAT_T			pDM_SWAT_Table = &pDM_Odm->DM_SWAT_Table;
	pFAT_T			pDM_FatTable = &pDM_Odm->DM_FatTable;	
	u1Byte			RxIdleAnt = pDM_SWAT_Table->PreAntenna, TargetAnt, nextAnt=0;
	PSTA_INFO_T		pEntry=NULL;
	//static u1Byte		reset_idx;
	u4Byte			value32;
	PADAPTER		Adapter	 =  pDM_Odm->Adapter;
	u8Byte			curTxOkCnt=0, curRxOkCnt=0,TxCntOffset, RxCntOffset;
	
	if(!pDM_Odm->bLinked) //bLinked==False
	{
		ODM_RT_TRACE(pDM_Odm,ODM_COMP_ANT_DIV, ODM_DBG_LOUD, ("[No Link!!!]\n"));
		if(pDM_FatTable->bBecomeLinked == true)
		{
			ODM_RT_TRACE(pDM_Odm,ODM_COMP_ANT_DIV, ODM_DBG_LOUD, ("Set REG 948[9:6]=0x0 \n"));
			ODM_SetBBReg(pDM_Odm, 0x948 , BIT9|BIT8|BIT7|BIT6, 0x0); 
			
			pDM_FatTable->bBecomeLinked = pDM_Odm->bLinked;
		}
		return;
	}
	else
	{
		if(pDM_FatTable->bBecomeLinked ==false)
		{
			ODM_RT_TRACE(pDM_Odm,ODM_COMP_ANT_DIV, ODM_DBG_LOUD, ("[Linked !!!]\n"));
			
			value32 = ODM_GetBBReg(pDM_Odm, 0x864, BIT5|BIT4|BIT3);
			
			if (value32==0x0)
				ODM_UpdateRxIdleAnt_8723B(pDM_Odm, MAIN_ANT, ANT1_2G, ANT2_2G);
			else if (value32==0x1)
				ODM_UpdateRxIdleAnt_8723B(pDM_Odm, AUX_ANT, ANT2_2G, ANT1_2G);
			
			ODM_RT_TRACE(pDM_Odm,ODM_COMP_ANT_DIV, ODM_DBG_LOUD, ("8723B: First link! Force antenna to  %s\n",(value32 == 0x0?"MAIN":"AUX") ));

			pDM_SWAT_Table->lastTxOkCnt = 0; 
			pDM_SWAT_Table->lastRxOkCnt =0; 
			TxCntOffset = *(pDM_Odm->pNumTxBytesUnicast);
			RxCntOffset = *(pDM_Odm->pNumRxBytesUnicast);
			
			pDM_FatTable->bBecomeLinked = pDM_Odm->bLinked;
		}
		else
		{
			TxCntOffset = 0;
			RxCntOffset = 0;
		}
	}
	
	ODM_RT_TRACE(pDM_Odm, ODM_COMP_ANT_DIV, ODM_DBG_LOUD, ("[%d] { try_flag=(( %d )), Step=(( %d )), Double_chk_flag = (( %d )) }\n",
		__LINE__,pDM_SWAT_Table->try_flag,Step,pDM_SWAT_Table->Double_chk_flag));

	// Handling step mismatch condition.
	// Peak step is not finished at last time. Recover the variable and check again.
	if(	Step != pDM_SWAT_Table->try_flag	)
	{
		ODM_RT_TRACE(pDM_Odm, ODM_COMP_ANT_DIV, ODM_DBG_LOUD, ("[Step != try_flag]    Need to Reset After Link\n"));
		ODM_SwAntDivRestAfterLink(pDM_Odm);
	}

	if(pDM_SWAT_Table->try_flag == 0xff) 
	{	
		pDM_SWAT_Table->try_flag = 0;
		pDM_SWAT_Table->Train_time_flag=0;
		ODM_RT_TRACE(pDM_Odm, ODM_COMP_ANT_DIV, ODM_DBG_LOUD,("[set try_flag = 0]  Prepare for peak!\n\n"));
		return;
	}	
	else//if( try_flag != 0xff ) 
	{
		//1 Normal State (Begin Trying)
		if(pDM_SWAT_Table->try_flag == 0) 
		{
		
			//---trafic decision---
			curTxOkCnt = *(pDM_Odm->pNumTxBytesUnicast) - pDM_SWAT_Table->lastTxOkCnt - TxCntOffset;
			curRxOkCnt = *(pDM_Odm->pNumRxBytesUnicast) - pDM_SWAT_Table->lastRxOkCnt - RxCntOffset;
			pDM_SWAT_Table->lastTxOkCnt =  *(pDM_Odm->pNumTxBytesUnicast);
			pDM_SWAT_Table->lastRxOkCnt =  *(pDM_Odm->pNumRxBytesUnicast);
			
			if (curTxOkCnt > 1875000 || curRxOkCnt > 1875000)//if(PlatformDivision64(curTxOkCnt+curRxOkCnt, 2) > 1875000)  ( 1.875M * 8bit ) / 2= 7.5M bits /sec )
			{
				pDM_SWAT_Table->TrafficLoad = TRAFFIC_HIGH;
				Train_time_temp=pDM_SWAT_Table->Train_time ;
				
				if(pDM_SWAT_Table->Train_time_flag==3)
				{
					HighTraffic_TrainTime_L=0xa;
					
					if(Train_time_temp<=16)
						Train_time_temp=HighTraffic_TrainTime_L;
					else
						Train_time_temp-=16;
					
				}				
				else if(pDM_SWAT_Table->Train_time_flag==2)
				{
					Train_time_temp-=8;
					HighTraffic_TrainTime_L=0xf;
				}	
				else if(pDM_SWAT_Table->Train_time_flag==1)
				{
					Train_time_temp-=4;
					HighTraffic_TrainTime_L=0x1e;
				}
				else if(pDM_SWAT_Table->Train_time_flag==0)
				{
					Train_time_temp+=8;
					HighTraffic_TrainTime_L=0x28;
				}

				
				//ODM_RT_TRACE(pDM_Odm,ODM_COMP_ANT_DIV, ODM_DBG_LOUD, ("*** Train_time_temp = ((%d))\n",Train_time_temp));

				//--
				if(Train_time_temp > HighTraffic_TrainTime_U)
					Train_time_temp=HighTraffic_TrainTime_U;
				
				else if(Train_time_temp < HighTraffic_TrainTime_L)
					Train_time_temp=HighTraffic_TrainTime_L;

				pDM_SWAT_Table->Train_time = Train_time_temp; //50ms~10ms
				
				ODM_RT_TRACE(pDM_Odm, ODM_COMP_ANT_DIV, ODM_DBG_LOUD,("  Train_time_flag=((%d)) , Train_time=((%d)) \n",pDM_SWAT_Table->Train_time_flag, pDM_SWAT_Table->Train_time));
				ODM_RT_TRACE(pDM_Odm, ODM_COMP_ANT_DIV, ODM_DBG_LOUD, ("  [HIGH Traffic]  \n" ));
			}
			else if (curTxOkCnt > 125000 || curRxOkCnt > 125000) // ( 0.125M * 8bit ) / 2 =  0.5M bits /sec )
			{
				pDM_SWAT_Table->TrafficLoad = TRAFFIC_LOW;
				Train_time_temp=pDM_SWAT_Table->Train_time ;

				if(pDM_SWAT_Table->Train_time_flag==3)
				{
					LowTraffic_TrainTime_L=10;
					if(Train_time_temp<50)
						Train_time_temp=LowTraffic_TrainTime_L;
					else
						Train_time_temp-=50;
				}				
				else if(pDM_SWAT_Table->Train_time_flag==2)
				{
					Train_time_temp-=30;
					LowTraffic_TrainTime_L=36;
				}	
				else if(pDM_SWAT_Table->Train_time_flag==1)
				{
					Train_time_temp-=10;
					LowTraffic_TrainTime_L=40;
				}
				else
					Train_time_temp+=10;	

				//--
				if(Train_time_temp >= LowTraffic_TrainTime_U)
					Train_time_temp=LowTraffic_TrainTime_U;
				
				else if(Train_time_temp <= LowTraffic_TrainTime_L)
					Train_time_temp=LowTraffic_TrainTime_L;

				pDM_SWAT_Table->Train_time = Train_time_temp; //50ms~20ms

				ODM_RT_TRACE(pDM_Odm, ODM_COMP_ANT_DIV, ODM_DBG_LOUD,("  Train_time_flag=((%d)) , Train_time=((%d)) \n",pDM_SWAT_Table->Train_time_flag, pDM_SWAT_Table->Train_time));
				ODM_RT_TRACE(pDM_Odm, ODM_COMP_ANT_DIV, ODM_DBG_LOUD, ("  [Low Traffic]  \n" ));
			}
			else
			{
				pDM_SWAT_Table->TrafficLoad = TRAFFIC_UltraLOW;
				pDM_SWAT_Table->Train_time = 0xc8; //200ms
				ODM_RT_TRACE(pDM_Odm, ODM_COMP_ANT_DIV, ODM_DBG_LOUD, ("  [Ultra-Low Traffic]  \n" ));
			}
			ODM_RT_TRACE(pDM_Odm, ODM_COMP_ANT_DIV, ODM_DBG_LOUD, ("TxOkCnt=(( %llu )), RxOkCnt=(( %llu )) \n", 
				curTxOkCnt ,curRxOkCnt ));
				
			//-----------------
		
			ODM_RT_TRACE(pDM_Odm, ODM_COMP_ANT_DIV, ODM_DBG_LOUD,(" Current MinMaxRSSI is ((%d)) \n",pDM_FatTable->MinMaxRSSI));

                        //---reset index---
			if(pDM_SWAT_Table->reset_idx>=reset_period)
			{
				pDM_FatTable->MinMaxRSSI=0; //
				pDM_SWAT_Table->reset_idx=0;
			}
			ODM_RT_TRACE(pDM_Odm, ODM_COMP_ANT_DIV, ODM_DBG_LOUD, ("reset_idx = (( %d )) \n",pDM_SWAT_Table->reset_idx ));
			//ODM_RT_TRACE(pDM_Odm, ODM_COMP_ANT_DIV, ODM_DBG_LOUD,("reset_idx=%d\n",pDM_SWAT_Table->reset_idx));
			pDM_SWAT_Table->reset_idx++;

			//---double check flag---
			if(pDM_FatTable->MinMaxRSSI > SWAntDiv_threshold && pDM_SWAT_Table->Double_chk_flag== 0)
			{			
				ODM_RT_TRACE(pDM_Odm, ODM_COMP_ANT_DIV, ODM_DBG_LOUD,(" MinMaxRSSI is ((%d)), and > %d \n",
					pDM_FatTable->MinMaxRSSI,SWAntDiv_threshold));

				pDM_SWAT_Table->Double_chk_flag =1;
				pDM_SWAT_Table->try_flag = 1; 
			        pDM_SWAT_Table->RSSI_Trying = 0;

				ODM_RT_TRACE(pDM_Odm, ODM_COMP_ANT_DIV, ODM_DBG_LOUD, (" Test the current Ant for (( %d )) ms again \n", pDM_SWAT_Table->Train_time));
				ODM_UpdateRxIdleAnt(pDM_Odm, pDM_FatTable->RxIdleAnt);	
				ODM_SetTimer(pDM_Odm,&pDM_SWAT_Table->SwAntennaSwitchTimer_8723B, pDM_SWAT_Table->Train_time ); //ms	
				return;
			}
			
			nextAnt = (pDM_FatTable->RxIdleAnt == MAIN_ANT)? AUX_ANT : MAIN_ANT;

			pDM_SWAT_Table->try_flag = 1;
			
			if(pDM_SWAT_Table->reset_idx<=1)
				pDM_SWAT_Table->RSSI_Trying = 2;
			else
				pDM_SWAT_Table->RSSI_Trying = 1;
			
			odm_S0S1_SwAntDivByCtrlFrame(pDM_Odm, SWAW_STEP_PEAK);
			ODM_RT_TRACE(pDM_Odm, ODM_COMP_ANT_DIV, ODM_DBG_LOUD,("[set try_flag=1]  Normal State:  Begin Trying!! \n"));
		}
	
		else if(pDM_SWAT_Table->try_flag == 1 && pDM_SWAT_Table->Double_chk_flag== 0)
		{	
			nextAnt = (pDM_FatTable->RxIdleAnt  == MAIN_ANT)? AUX_ANT : MAIN_ANT;		
			pDM_SWAT_Table->RSSI_Trying--;
		}
		
		//1 Decision State
		if((pDM_SWAT_Table->try_flag == 1)&&(pDM_SWAT_Table->RSSI_Trying == 0) )
		{
			bool bByCtrlFrame = false;
			u8Byte	pkt_cnt_total = 0;
			
			for (i=0; i<ODM_ASSOCIATE_ENTRY_NUM; i++)
			{
				pEntry = pDM_Odm->pODM_StaInfo[i];
				if(IS_STA_VALID(pEntry))
				{
					//2 Caculate RSSI per Antenna
					Main_RSSI = (pDM_FatTable->MainAnt_Cnt[i]!=0)?(pDM_FatTable->MainAnt_Sum[i]/pDM_FatTable->MainAnt_Cnt[i]):0;
					Aux_RSSI = (pDM_FatTable->AuxAnt_Cnt[i]!=0)?(pDM_FatTable->AuxAnt_Sum[i]/pDM_FatTable->AuxAnt_Cnt[i]):0;
					
					if(pDM_FatTable->MainAnt_Cnt[i]<=1 && pDM_FatTable->CCK_counter_main>=1)
						Main_RSSI=0;	
					
					if(pDM_FatTable->AuxAnt_Cnt[i]<=1 && pDM_FatTable->CCK_counter_aux>=1)
						Aux_RSSI=0;

					TargetAnt = (Main_RSSI==Aux_RSSI)?pDM_SWAT_Table->PreAntenna:((Main_RSSI>=Aux_RSSI)?MAIN_ANT:AUX_ANT);
					LocalMaxRSSI = (Main_RSSI>=Aux_RSSI) ? Main_RSSI : Aux_RSSI;
					LocalMinRSSI = (Main_RSSI>=Aux_RSSI) ? Aux_RSSI : Main_RSSI;
					
					ODM_RT_TRACE(pDM_Odm,ODM_COMP_ANT_DIV, ODM_DBG_LOUD, ("***  CCK_counter_main = (( %d ))  , CCK_counter_aux= ((  %d )) \n", pDM_FatTable->CCK_counter_main, pDM_FatTable->CCK_counter_aux));
					ODM_RT_TRACE(pDM_Odm,ODM_COMP_ANT_DIV, ODM_DBG_LOUD, ("***  OFDM_counter_main = (( %d ))  , OFDM_counter_aux= ((  %d )) \n", pDM_FatTable->OFDM_counter_main, pDM_FatTable->OFDM_counter_aux));
					ODM_RT_TRACE(pDM_Odm,ODM_COMP_ANT_DIV, ODM_DBG_LOUD, ("***  Main_Cnt = (( %d ))  , Main_RSSI= ((  %d )) \n", pDM_FatTable->MainAnt_Cnt[i], Main_RSSI));
					ODM_RT_TRACE(pDM_Odm,ODM_COMP_ANT_DIV, ODM_DBG_LOUD, ("***  Aux_Cnt   = (( %d ))  , Aux_RSSI = ((  %d )) \n", pDM_FatTable->AuxAnt_Cnt[i]  , Aux_RSSI ));
					ODM_RT_TRACE(pDM_Odm,ODM_COMP_ANT_DIV, ODM_DBG_LOUD, ("*** MAC ID:[ %d ] , TargetAnt = (( %s )) \n", i ,( TargetAnt ==MAIN_ANT)?"MAIN_ANT":"AUX_ANT"));
					
					//2 Select RX Idle Antenna
					
					if (LocalMaxRSSI != 0 && LocalMaxRSSI < MinMaxRSSI)
					{
							RxIdleAnt = TargetAnt;
							MinMaxRSSI = LocalMaxRSSI;
							ODM_RT_TRACE(pDM_Odm,ODM_COMP_ANT_DIV, ODM_DBG_LOUD, ("*** LocalMaxRSSI-LocalMinRSSI = ((%d))\n",(LocalMaxRSSI-LocalMinRSSI)));
					
							if((LocalMaxRSSI-LocalMinRSSI)>8)
							{
								if(LocalMinRSSI != 0)
									pDM_SWAT_Table->Train_time_flag=3;
								else
								{
									if(MinMaxRSSI > SWAntDiv_threshold)
										pDM_SWAT_Table->Train_time_flag=0;
									else
										pDM_SWAT_Table->Train_time_flag=3;
								}
							}
							else if((LocalMaxRSSI-LocalMinRSSI)>5)
								pDM_SWAT_Table->Train_time_flag=2;
							else if((LocalMaxRSSI-LocalMinRSSI)>2)
								pDM_SWAT_Table->Train_time_flag=1;
							else
								pDM_SWAT_Table->Train_time_flag=0;
							
					}
					
					//2 Select TX Antenna
					if(TargetAnt == MAIN_ANT)
						pDM_FatTable->antsel_a[i] = ANT1_2G;
					else
						pDM_FatTable->antsel_a[i] = ANT2_2G;
			
				}
					pDM_FatTable->MainAnt_Sum[i] = 0;
					pDM_FatTable->AuxAnt_Sum[i] = 0;
					pDM_FatTable->MainAnt_Cnt[i] = 0;
					pDM_FatTable->AuxAnt_Cnt[i] = 0;
			}

			if(pDM_SWAT_Table->bSWAntDivByCtrlFrame)
			{
				odm_S0S1_SwAntDivByCtrlFrame(pDM_Odm, SWAW_STEP_DETERMINE);
				bByCtrlFrame = true;
			}

			pkt_cnt_total = pDM_FatTable->CCK_counter_main + pDM_FatTable->CCK_counter_aux + 
			pDM_FatTable->OFDM_counter_main + pDM_FatTable->OFDM_counter_aux;
			pDM_FatTable->CCK_counter_main=0;
			pDM_FatTable->CCK_counter_aux=0;
			pDM_FatTable->OFDM_counter_main=0;
			pDM_FatTable->OFDM_counter_aux=0;
			ODM_RT_TRACE(pDM_Odm, ODM_COMP_ANT_DIV, ODM_DBG_LOUD,("Control frame packet counter = %d, Data frame packet counter = %llu\n", 
				pDM_SWAT_Table->PktCnt_SWAntDivByCtrlFrame, pkt_cnt_total));

			if(MinMaxRSSI == 0xff || ((pkt_cnt_total < (pDM_SWAT_Table->PktCnt_SWAntDivByCtrlFrame >> 1)) && pDM_Odm->PhyDbgInfo.NumQryBeaconPkt < 2))
			{	
				MinMaxRSSI = 0;
				ODM_RT_TRACE(pDM_Odm, ODM_COMP_ANT_DIV, ODM_DBG_LOUD,("Check RSSI of control frame because MinMaxRSSI == 0xff\n"));
				ODM_RT_TRACE(pDM_Odm, ODM_COMP_ANT_DIV, ODM_DBG_LOUD,("bByCtrlFrame = %d\n", bByCtrlFrame));
				
				if(bByCtrlFrame)
				{
					Main_RSSI = (pDM_FatTable->MainAnt_CtrlFrame_Cnt!=0)?(pDM_FatTable->MainAnt_CtrlFrame_Sum/pDM_FatTable->MainAnt_CtrlFrame_Cnt):0;
					Aux_RSSI = (pDM_FatTable->AuxAnt_CtrlFrame_Cnt!=0)?(pDM_FatTable->AuxAnt_CtrlFrame_Sum/pDM_FatTable->AuxAnt_CtrlFrame_Cnt):0;
					
					if(pDM_FatTable->MainAnt_CtrlFrame_Cnt<=1 && pDM_FatTable->CCK_CtrlFrame_Cnt_main>=1)
						Main_RSSI=0;	
					
					if(pDM_FatTable->AuxAnt_CtrlFrame_Cnt<=1 && pDM_FatTable->CCK_CtrlFrame_Cnt_aux>=1)
						Aux_RSSI=0;

					if (Main_RSSI != 0 || Aux_RSSI != 0)
					{
						RxIdleAnt = (Main_RSSI==Aux_RSSI)?pDM_SWAT_Table->PreAntenna:((Main_RSSI>=Aux_RSSI)?MAIN_ANT:AUX_ANT);
						LocalMaxRSSI = (Main_RSSI>=Aux_RSSI) ? Main_RSSI : Aux_RSSI;
						LocalMinRSSI = (Main_RSSI>=Aux_RSSI) ? Aux_RSSI : Main_RSSI;
		
						if((LocalMaxRSSI-LocalMinRSSI)>8)
							pDM_SWAT_Table->Train_time_flag=3;
						else if((LocalMaxRSSI-LocalMinRSSI)>5)
							pDM_SWAT_Table->Train_time_flag=2;
						else if((LocalMaxRSSI-LocalMinRSSI)>2)
							pDM_SWAT_Table->Train_time_flag=1;
						else
							pDM_SWAT_Table->Train_time_flag=0;

						ODM_RT_TRACE(pDM_Odm, ODM_COMP_ANT_DIV, ODM_DBG_LOUD,("Control frame: Main_RSSI = %d, Aux_RSSI = %d\n", Main_RSSI, Aux_RSSI));
						ODM_RT_TRACE(pDM_Odm, ODM_COMP_ANT_DIV, ODM_DBG_LOUD,("RxIdleAnt decided by control frame = %s\n", (RxIdleAnt == MAIN_ANT?"MAIN":"AUX")));
					}
				}
			}
			
			pDM_FatTable->MinMaxRSSI=MinMaxRSSI;
			pDM_SWAT_Table->try_flag = 0;
						
			if( pDM_SWAT_Table->Double_chk_flag==1)
			{
				pDM_SWAT_Table->Double_chk_flag=0;
				if(pDM_FatTable->MinMaxRSSI > SWAntDiv_threshold)
				{
					ODM_RT_TRACE(pDM_Odm, ODM_COMP_ANT_DIV, ODM_DBG_LOUD,(" [Double check] MinMaxRSSI ((%d)) > %d again!! \n",
						pDM_FatTable->MinMaxRSSI,SWAntDiv_threshold));
					
					ODM_UpdateRxIdleAnt(pDM_Odm, RxIdleAnt);	
					
					ODM_RT_TRACE(pDM_Odm, ODM_COMP_ANT_DIV, ODM_DBG_LOUD,("[reset try_flag = 0] Training accomplished !!!] \n\n\n"));
					return;
				}
				else
				{
					ODM_RT_TRACE(pDM_Odm, ODM_COMP_ANT_DIV, ODM_DBG_LOUD,(" [Double check] MinMaxRSSI ((%d)) <= %d !! \n",
						pDM_FatTable->MinMaxRSSI,SWAntDiv_threshold));

					nextAnt = (pDM_FatTable->RxIdleAnt  == MAIN_ANT)? AUX_ANT : MAIN_ANT;
					pDM_SWAT_Table->try_flag = 0; 
					pDM_SWAT_Table->reset_idx=reset_period;
					ODM_RT_TRACE(pDM_Odm, ODM_COMP_ANT_DIV, ODM_DBG_LOUD,("[set try_flag=0]  Normal State:  Need to tryg again!! \n\n\n"));
					return;
				}
			}
			else
			{
				if(pDM_FatTable->MinMaxRSSI < SWAntDiv_threshold)
					pDM_SWAT_Table->reset_idx=reset_period;
				
				pDM_SWAT_Table->PreAntenna =RxIdleAnt;
				ODM_UpdateRxIdleAnt(pDM_Odm, RxIdleAnt );
				ODM_RT_TRACE(pDM_Odm, ODM_COMP_ANT_DIV, ODM_DBG_LOUD,("[reset try_flag = 0] Training accomplished !!!] \n\n\n"));
			        return;
			}
			
		}

	}

	//1 4.Change TRX antenna

	ODM_RT_TRACE(pDM_Odm, ODM_COMP_ANT_DIV, ODM_DBG_LOUD, ("RSSI_Trying = (( %d )),    Ant: (( %s )) >>> (( %s )) \n",
		pDM_SWAT_Table->RSSI_Trying, (pDM_FatTable->RxIdleAnt  == MAIN_ANT?"MAIN":"AUX"),(nextAnt == MAIN_ANT?"MAIN":"AUX")));
		
	ODM_UpdateRxIdleAnt(pDM_Odm, nextAnt);

	//1 5.Reset Statistics

	pDM_FatTable->RxIdleAnt  = nextAnt;

	//1 6.Set next timer   (Trying State)
	
	ODM_RT_TRACE(pDM_Odm, ODM_COMP_ANT_DIV, ODM_DBG_LOUD, (" Test ((%s)) Ant for (( %d )) ms \n", (nextAnt == MAIN_ANT?"MAIN":"AUX"), pDM_SWAT_Table->Train_time));
	ODM_SetTimer(pDM_Odm,&pDM_SWAT_Table->SwAntennaSwitchTimer_8723B, pDM_SWAT_Table->Train_time ); //ms
}


void
ODM_SW_AntDiv_WorkitemCallback(
    IN void *            pContext
    )
{
	PADAPTER		pAdapter = (PADAPTER)pContext;
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(pAdapter);

	//DbgPrint("SW_antdiv_Workitem_Callback");
	odm_S0S1_SwAntDiv(&pHalData->odmpriv, SWAW_STEP_DETERMINE);
}

void
ODM_SW_AntDiv_Callback(void *FunctionContext)
{
	PDM_ODM_T	pDM_Odm= (PDM_ODM_T)FunctionContext;
	PADAPTER	padapter = pDM_Odm->Adapter;


	if(padapter->net_closed == true)
	    return;

#if 0 // Can't do I/O in timer callback
	odm_S0S1_SwAntDiv(pDM_Odm, SWAW_STEP_DETERMINE);
#else
	rtw_run_in_thread_cmd(padapter, ODM_SW_AntDiv_WorkitemCallback, padapter);
#endif
}

void
odm_S0S1_SwAntDivByCtrlFrame(
	IN		PDM_ODM_T		pDM_Odm,
	IN		u1Byte			Step
	)
{
	pSWAT_T	pDM_SWAT_Table = &pDM_Odm->DM_SWAT_Table;
	pFAT_T		pDM_FatTable = &pDM_Odm->DM_FatTable;
	
	switch(Step)
	{
		case SWAW_STEP_PEAK:
			pDM_SWAT_Table->PktCnt_SWAntDivByCtrlFrame = 0;
			pDM_SWAT_Table->bSWAntDivByCtrlFrame = true;
			pDM_FatTable->MainAnt_CtrlFrame_Cnt = 0;
			pDM_FatTable->AuxAnt_CtrlFrame_Cnt = 0;
			pDM_FatTable->MainAnt_CtrlFrame_Sum = 0;
			pDM_FatTable->AuxAnt_CtrlFrame_Sum = 0;
			pDM_FatTable->CCK_CtrlFrame_Cnt_main = 0;
			pDM_FatTable->CCK_CtrlFrame_Cnt_aux = 0;
			pDM_FatTable->OFDM_CtrlFrame_Cnt_main = 0;
			pDM_FatTable->OFDM_CtrlFrame_Cnt_aux = 0;
			ODM_RT_TRACE(pDM_Odm, ODM_COMP_ANT_DIV, ODM_DBG_LOUD,("odm_S0S1_SwAntDivForAPMode(): Start peak and reset counter\n"));
			break;
		case SWAW_STEP_DETERMINE:
			pDM_SWAT_Table->bSWAntDivByCtrlFrame = false;
			ODM_RT_TRACE(pDM_Odm, ODM_COMP_ANT_DIV, ODM_DBG_LOUD,("odm_S0S1_SwAntDivForAPMode(): Stop peak\n"));
			break;
		default:
			pDM_SWAT_Table->bSWAntDivByCtrlFrame = false;
			break;
	}			
}

void
odm_AntselStatisticsOfCtrlFrame(
	IN		PDM_ODM_T		pDM_Odm,
	IN		u1Byte			antsel_tr_mux,
	IN		u4Byte			RxPWDBAll
)
{
	pFAT_T	pDM_FatTable = &pDM_Odm->DM_FatTable;

	if(antsel_tr_mux == ANT1_2G)
	{
		pDM_FatTable->MainAnt_CtrlFrame_Sum+=RxPWDBAll;
		pDM_FatTable->MainAnt_CtrlFrame_Cnt++;
	}
	else
	{
		pDM_FatTable->AuxAnt_CtrlFrame_Sum+=RxPWDBAll;
		pDM_FatTable->AuxAnt_CtrlFrame_Cnt++;
	}
}

void
odm_S0S1_SwAntDivByCtrlFrame_ProcessRSSI(
	IN		PDM_ODM_T				pDM_Odm,
	IN		PODM_PHY_INFO_T		pPhyInfo,
	IN		PODM_PACKET_INFO_T		pPktinfo
	)
{
	pSWAT_T	pDM_SWAT_Table = &pDM_Odm->DM_SWAT_Table;
	pFAT_T		pDM_FatTable = &pDM_Odm->DM_FatTable;
	bool		isCCKrate;

	if(!(pDM_Odm->SupportAbility & ODM_BB_ANT_DIV))
		return;

	if(pDM_Odm->AntDivType != S0S1_SW_ANTDIV)
		return;

	// In try state
	if(!pDM_SWAT_Table->bSWAntDivByCtrlFrame)
		return;

	// No HW error and match receiver address
	if(!pPktinfo->bToSelf)
		return;
	
	pDM_SWAT_Table->PktCnt_SWAntDivByCtrlFrame++;
	isCCKrate = ((pPktinfo->DataRate >= DESC_RATE1M ) && (pPktinfo->DataRate <= DESC_RATE11M ))?true :false;

	if(isCCKrate)
	{
	 	pDM_FatTable->antsel_rx_keep_0 = (pDM_FatTable->RxIdleAnt == MAIN_ANT) ? ANT1_2G : ANT2_2G;

		if(pDM_FatTable->antsel_rx_keep_0==ANT1_2G)
			pDM_FatTable->CCK_CtrlFrame_Cnt_main++;
		else
			pDM_FatTable->CCK_CtrlFrame_Cnt_aux++;

		odm_AntselStatisticsOfCtrlFrame(pDM_Odm, pDM_FatTable->antsel_rx_keep_0, pPhyInfo->RxMIMOSignalStrength[ODM_RF_PATH_A]);
	}
	else
	{
		if(pDM_FatTable->antsel_rx_keep_0==ANT1_2G)
			pDM_FatTable->OFDM_CtrlFrame_Cnt_main++;
		else
			pDM_FatTable->OFDM_CtrlFrame_Cnt_aux++;

		odm_AntselStatisticsOfCtrlFrame(pDM_Odm, pDM_FatTable->antsel_rx_keep_0, pPhyInfo->RxPWDBAll);
	}
}

void
ODM_AntDivReset(
	IN PDM_ODM_T	pDM_Odm 
	)
{
	//2 [--8723B---]
	odm_S0S1_SWAntDiv_Reset_8723B(pDM_Odm);
}

void
ODM_AntDivInit(
	IN PDM_ODM_T	pDM_Odm 
	)
{
	pFAT_T			pDM_FatTable = &pDM_Odm->DM_FatTable;
	pSWAT_T			pDM_SWAT_Table = &pDM_Odm->DM_SWAT_Table;


	if(!(pDM_Odm->SupportAbility & ODM_BB_ANT_DIV))
	{
		ODM_RT_TRACE(pDM_Odm, ODM_COMP_ANT_DIV,ODM_DBG_LOUD,("[Return!!!]   Not Support Antenna Diversity Function\n"));
		return;
	}

	//2 [--General---]
	pDM_Odm->antdiv_period=0;
	pDM_Odm->antdiv_select=0;
	pDM_SWAT_Table->Ant5G = MAIN_ANT;
	pDM_SWAT_Table->Ant2G = MAIN_ANT;
	pDM_FatTable->CCK_counter_main=0;
	pDM_FatTable->CCK_counter_aux=0;
	pDM_FatTable->OFDM_counter_main=0;
	pDM_FatTable->OFDM_counter_aux=0;
	
	//3 [Set MAIN_ANT as default antenna if Auto-Ant enable]
	if (pDM_Odm->antdiv_select==1)
		pDM_Odm->AntType = ODM_FIX_MAIN_ANT;
	else if (pDM_Odm->antdiv_select==2)
		pDM_Odm->AntType = ODM_FIX_AUX_ANT;
	else if(pDM_Odm->antdiv_select==0)
		pDM_Odm->AntType = ODM_AUTO_ANT;
	
	if(pDM_Odm->AntType == ODM_AUTO_ANT)
	{
		odm_AntDiv_on_off(pDM_Odm, ANTDIV_OFF);
		ODM_UpdateRxIdleAnt(pDM_Odm, MAIN_ANT);
	}
	else
	{
		odm_AntDiv_on_off(pDM_Odm, ANTDIV_OFF);
		
		if(pDM_Odm->AntType == ODM_FIX_MAIN_ANT)
		{
			ODM_UpdateRxIdleAnt(pDM_Odm, MAIN_ANT);
			return;
		}
		else if(pDM_Odm->AntType == ODM_FIX_AUX_ANT)
		{
			ODM_UpdateRxIdleAnt(pDM_Odm, AUX_ANT);
			return;
		}
	}
	//---
	if(pDM_Odm->AntDivType != CGCS_RX_HW_ANTDIV)
	{
		#if TX_BY_REG
		ODM_SetBBReg(pDM_Odm, 0x80c , BIT21, 0); //Reg80c[21]=1'b0		//from Reg
		#else
		ODM_SetBBReg(pDM_Odm, 0x80c , BIT21, 1);
		#endif
	}
		
	//2 [--8723B---]
	//pDM_Odm->AntDivType = S0S1_SW_ANTDIV;
	//pDM_Odm->AntDivType = CG_TRX_HW_ANTDIV;

	if(pDM_Odm->AntDivType != S0S1_SW_ANTDIV && pDM_Odm->AntDivType != CG_TRX_HW_ANTDIV)
	{
		ODM_RT_TRACE(pDM_Odm, ODM_COMP_ANT_DIV,ODM_DBG_LOUD,("[Return!!!] 8723B  Not Supprrt This AntDiv Type\n"));
		pDM_Odm->SupportAbility &= ~(ODM_BB_ANT_DIV);
		return;
	}
		
	if( pDM_Odm->AntDivType==S0S1_SW_ANTDIV)
		odm_S0S1_SWAntDiv_Init_8723B(pDM_Odm);
	else if(pDM_Odm->AntDivType==CG_TRX_HW_ANTDIV)
		odm_TRX_HWAntDiv_Init_8723B(pDM_Odm);		
	//ODM_RT_TRACE(pDM_Odm, ODM_COMP_ANT_DIV,ODM_DBG_LOUD,("*** SupportICType=[%lu] \n",pDM_Odm->SupportICType));
	//ODM_RT_TRACE(pDM_Odm, ODM_COMP_ANT_DIV,ODM_DBG_LOUD,("*** AntDiv SupportAbility=[%lu] \n",(pDM_Odm->SupportAbility & ODM_BB_ANT_DIV)>>6));
	//ODM_RT_TRACE(pDM_Odm, ODM_COMP_ANT_DIV,ODM_DBG_LOUD,("*** AntDiv Type=[%d] \n",pDM_Odm->AntDivType));

}

void
ODM_AntDiv(
	IN		PDM_ODM_T		pDM_Odm
)
{	
	PADAPTER		pAdapter	= pDM_Odm->Adapter;
	pFAT_T			pDM_FatTable = &pDM_Odm->DM_FatTable;

	if(*pDM_Odm->pBandType == ODM_BAND_5G )
	{
		if(pDM_FatTable->idx_AntDiv_counter_5G <  pDM_Odm->antdiv_period )
		{
			pDM_FatTable->idx_AntDiv_counter_5G++;
			return;
		}
		else
			pDM_FatTable->idx_AntDiv_counter_5G=0;
	}
	else 	if(*pDM_Odm->pBandType == ODM_BAND_2_4G )
	{
		if(pDM_FatTable->idx_AntDiv_counter_2G <  pDM_Odm->antdiv_period )
		{
			pDM_FatTable->idx_AntDiv_counter_2G++;
			return;
		}
		else
			pDM_FatTable->idx_AntDiv_counter_2G=0;
	}

	//----------
	if(!(pDM_Odm->SupportAbility & ODM_BB_ANT_DIV))
	{
		ODM_RT_TRACE(pDM_Odm, ODM_COMP_ANT_DIV,ODM_DBG_LOUD,("[Return!!!]   Not Support Antenna Diversity Function\n"));
		return;
	}

	if (pDM_Odm->antdiv_select==1)
		pDM_Odm->AntType = ODM_FIX_MAIN_ANT;
	else if (pDM_Odm->antdiv_select==2)
		pDM_Odm->AntType = ODM_FIX_AUX_ANT;
	else  if (pDM_Odm->antdiv_select==0)
		pDM_Odm->AntType = ODM_AUTO_ANT;

	//ODM_RT_TRACE(pDM_Odm, ODM_COMP_ANT_DIV,ODM_DBG_LOUD,("AntType= (( %d )) , pre_AntType= (( %d ))  \n",pDM_Odm->AntType,pDM_Odm->pre_AntType));
	
	if(pDM_Odm->AntType != ODM_AUTO_ANT)
	{
			ODM_RT_TRACE(pDM_Odm,ODM_COMP_ANT_DIV, ODM_DBG_LOUD, ("Fix Antenna at (( %s ))\n",(pDM_Odm->AntType == ODM_FIX_MAIN_ANT)?"MAIN":"AUX"));
			
		if(pDM_Odm->AntType != pDM_Odm->pre_AntType)
		{
			odm_AntDiv_on_off(pDM_Odm, ANTDIV_OFF);

			ODM_SetBBReg(pDM_Odm, 0x80c , BIT21, 0);

			if(pDM_Odm->AntType == ODM_FIX_MAIN_ANT)
				ODM_UpdateRxIdleAnt(pDM_Odm, MAIN_ANT);
			else if(pDM_Odm->AntType == ODM_FIX_AUX_ANT)
				ODM_UpdateRxIdleAnt(pDM_Odm, AUX_ANT);
		}
		pDM_Odm->pre_AntType=pDM_Odm->AntType; 
		return;
	}
	else
	{
		if(pDM_Odm->AntType != pDM_Odm->pre_AntType)
		{
			odm_AntDiv_on_off(pDM_Odm, ANTDIV_ON);
			ODM_SetBBReg(pDM_Odm, 0x80c , BIT21, 1);
		}
		pDM_Odm->pre_AntType=pDM_Odm->AntType;
	}
	 
	
	//3 -----------------------------------------------------------------------------------------------------------

	if (pDM_Odm->AntDivType==S0S1_SW_ANTDIV)
		odm_S0S1_SwAntDiv(pDM_Odm, SWAW_STEP_PEAK);
	else if (pDM_Odm->AntDivType==CG_TRX_HW_ANTDIV)
		odm_HW_AntDiv(pDM_Odm);
}


void
odm_AntselStatistics(
	IN		PDM_ODM_T		pDM_Odm,
	IN		u1Byte			antsel_tr_mux,
	IN		u4Byte			MacId,
	IN		u4Byte			RxPWDBAll
)
{
	pFAT_T	pDM_FatTable = &pDM_Odm->DM_FatTable;

	if(antsel_tr_mux == ANT1_2G)
	{
		pDM_FatTable->MainAnt_Sum[MacId]+=RxPWDBAll;
		pDM_FatTable->MainAnt_Cnt[MacId]++;
	}
	else
	{
		pDM_FatTable->AuxAnt_Sum[MacId]+=RxPWDBAll;
		pDM_FatTable->AuxAnt_Cnt[MacId]++;
	}
}


void
ODM_Process_RSSIForAntDiv(	
	IN OUT	PDM_ODM_T					pDM_Odm,
	IN		PODM_PHY_INFO_T				pPhyInfo,
	IN		PODM_PACKET_INFO_T			pPktinfo
	)
{
u1Byte			isCCKrate=0,CCKMaxRate=DESC_RATE11M;
pFAT_T			pDM_FatTable = &pDM_Odm->DM_FatTable;

	u1Byte			RxPower_Ant0, RxPower_Ant1;	

	CCKMaxRate=DESC_RATE11M;
	isCCKrate = (pPktinfo->DataRate <= CCKMaxRate)?true:false;

	RxPower_Ant0=pPhyInfo->RxPWDBAll;

	if(pDM_Odm->AntDivType != CG_TRX_SMART_ANTDIV)
	{
		if(pPktinfo->bPacketToSelf || pPktinfo->bPacketMatchBSSID)
		{
			if(isCCKrate && (pDM_Odm->AntDivType == S0S1_SW_ANTDIV))
			{
			 	pDM_FatTable->antsel_rx_keep_0 = (pDM_FatTable->RxIdleAnt == MAIN_ANT) ? ANT1_2G : ANT2_2G;


					if(pDM_FatTable->antsel_rx_keep_0==ANT1_2G)
						pDM_FatTable->CCK_counter_main++;
					else// if(pDM_FatTable->antsel_rx_keep_0==ANT2_2G)
						pDM_FatTable->CCK_counter_aux++;

				odm_AntselStatistics(pDM_Odm, pDM_FatTable->antsel_rx_keep_0, pPktinfo->StationID, RxPower_Ant0);
			}
			else
			{
				if(pDM_FatTable->antsel_rx_keep_0==ANT1_2G)
					pDM_FatTable->OFDM_counter_main++;
				else// if(pDM_FatTable->antsel_rx_keep_0==ANT2_2G)
					pDM_FatTable->OFDM_counter_aux++;
				odm_AntselStatistics(pDM_Odm, pDM_FatTable->antsel_rx_keep_0, pPktinfo->StationID, RxPower_Ant0);
			}
		}
	}
	//ODM_RT_TRACE(pDM_Odm,ODM_COMP_ANT_DIV, ODM_DBG_LOUD,("isCCKrate=%d, PWDB_ALL=%d\n",isCCKrate, pPhyInfo->RxPWDBAll));
	//ODM_RT_TRACE(pDM_Odm,ODM_COMP_ANT_DIV, ODM_DBG_LOUD,("antsel_tr_mux=3'b%d%d%d\n",pDM_FatTable->antsel_rx_keep_2, pDM_FatTable->antsel_rx_keep_1, pDM_FatTable->antsel_rx_keep_0));
}

void
ODM_SetTxAntByTxInfo(
	IN		PDM_ODM_T		pDM_Odm,
	IN		pu1Byte			pDesc,
	IN		u1Byte			macId	
)
{
	pFAT_T	pDM_FatTable = &pDM_Odm->DM_FatTable;

	if(!(pDM_Odm->SupportAbility & ODM_BB_ANT_DIV))
		return;

	if(pDM_Odm->AntDivType==CGCS_RX_HW_ANTDIV)
		return;


	SET_TX_DESC_ANTSEL_A_8723B(pDesc, pDM_FatTable->antsel_a[macId]);
	//ODM_RT_TRACE(pDM_Odm,ODM_COMP_ANT_DIV, ODM_DBG_LOUD, ("[8723B] SetTxAntByTxInfo_WIN: MacID=%d, antsel_tr_mux=3'b%d%d%d\n", 
		//macId, pDM_FatTable->antsel_c[macId], pDM_FatTable->antsel_b[macId], pDM_FatTable->antsel_a[macId]));
}

void
ODM_AntDiv_Config(
	IN		PDM_ODM_T		pDM_Odm
	)
{
	pFAT_T			pDM_FatTable = &pDM_Odm->DM_FatTable;

	ODM_RT_TRACE(pDM_Odm,ODM_COMP_ANT_DIV, ODM_DBG_LOUD, ("CE Config Antenna Diversity\n"));
	pDM_Odm->AntDivType = S0S1_SW_ANTDIV;

	ODM_RT_TRACE(pDM_Odm,ODM_COMP_ANT_DIV, ODM_DBG_LOUD, ("SupportAbility = (( %x ))\n", pDM_Odm->SupportAbility ));
}


void
ODM_AntDivTimers(
	IN PDM_ODM_T	pDM_Odm,
	IN u1Byte			state
	)
{
	if(state==INIT_ANTDIV_TIMMER)
	{
		ODM_InitializeTimer(pDM_Odm,&pDM_Odm->DM_SWAT_Table.SwAntennaSwitchTimer_8723B,
		(RT_TIMER_CALL_BACK)ODM_SW_AntDiv_Callback, NULL, "SwAntennaSwitchTimer_8723B");
	}
	else if(state==CANCEL_ANTDIV_TIMMER)
	{
		ODM_CancelTimer(pDM_Odm,&pDM_Odm->DM_SWAT_Table.SwAntennaSwitchTimer_8723B);
	}
	else if(state==RELEASE_ANTDIV_TIMMER)
	{
		ODM_ReleaseTimer(pDM_Odm,&pDM_Odm->DM_SWAT_Table.SwAntennaSwitchTimer_8723B);
	}

}

#endif //#if (defined(CONFIG_HW_ANTENNA_DIVERSITY))


