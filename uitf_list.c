/*************************************************************************
 *
 *  uitf_list.c - Library of routines for readout and buffering of
 *                data from the UITF Mott Detector
 *
 *   Modes (selected from usr->Config) :
 *       - Integrating
 *       - Counting
 *
 *
 */

/* Event Buffer definitions */
#define MAX_EVENT_POOL     10
#define MAX_EVENT_LENGTH   1024*64      /* Size in Bytes */

#ifndef TI_MASTER
#define TI_MASTER
#endif

/* EXTernal trigger source (e.g. front panel ECL input), POLL for available data */
#define TI_READOUT TI_READOUT_EXT_POLL

/* TI VME address (Slot 3) */
#define TI_ADDR  (3 << 19)

#define FIBER_LATENCY_OFFSET 0x10

 /* Source required for CODA readout lists using the TI */
#include "dmaBankTools.h"
#include "tiprimary_list.c"

/* Library to pipe stdout to daLogMsg */
#include "dalmaRolLib.h"

/* Event type 137 code */
#include "rocUtils.c"

/* uitf config library */
#include "uitf_config.c"

/* fadc library*/
#include "fadcLib.h"
int32_t MAXFADCWORDS = 0;
const uint32_t FADC250_DECODER_BANK = 0x5;

/* helicity decoder library */
#include "hdLib.h"
const uint32_t HELICITY_DECODER_BANK = 0x11;

// runtype set by user string at Download.  default to counting
int32_t UITF_RUN_TYPE = UITF_COUNTING;

/****************************************
 *  DOWNLOAD
 ****************************************/
void
rocDownload()
{
  int stat;

  if(strlen(rol->usrString) > 0)
    {
      if(strcasecmp(rol->usrString, "integrating") == 0)
	UITF_RUN_TYPE = UITF_INTEGRATING;
      else if(strcasecmp(rol->usrString, "counting") == 0)
	UITF_RUN_TYPE = UITF_COUNTING;
      else
	printf("%s: ERROR: Invalid runtype (%s).  Default to counting\n",
	       __func__, rol->usrString);
    }

  if(uitf_config_init(rol->usrConfig) != 0)
    {
      daLogMsg("ERROR","Error Loading Configfile: %s", rol->usrConfig);
      return;
    }

  if(uitf_config_modules_init() != 0)
    {
      daLogMsg("ERROR", "Module init error");
      return;
    }

  blockLevel = ti_params.blocklevel;
  /*
   * Set Trigger source
   *    For the TI-Master, valid sources:
   *      TI_TRIGGER_FPTRG     2  Front Panel "TRG" Input
   *      TI_TRIGGER_TSINPUTS  3  Front Panel "TS" Inputs
   *      TI_TRIGGER_PULSER    5  TI Internal Pulser (Fixed rate and/or random)
   */
  if(UITF_RUN_TYPE == UITF_COUNTING)
    {
       /* Front Panel TS Inputs */
      tiSetTriggerSource(TI_TRIGGER_TSINPUTS);
    }
  else if(UITF_RUN_TYPE == UITF_INTEGRATING)
    {
      /* Front Panel TRG */
      tiSetTriggerSource(TI_TRIGGER_FPTRG);
    }

  tiStatus(0);
  faSDC_Status(0);
  faSDC_Status_Integrating(0);
  faGStatus(0);
  if(hd_params.enabled)
    hdStatus(0);

  printf("rocDownload: User Download Executed\n");

}

/****************************************
 *  PRESTART
 ****************************************/
void
rocPrestart()
{

  /* Program modules */

  DALMAGO;
  tiStatus(0);
  faSDC_Status(0);
  faSDC_Status_Integrating(0);
  faGStatus(0);
  if(hd_params.enabled)
    hdStatus(0);
  DALMASTOP;

  if(rol->usrConfig)
    {
      int32_t inum = 0, maxsize = MAX_EVENT_LENGTH-128, nwords = 0;

      UEOPEN(137, BT_BANK, 0);
      nwords = rocFile2Bank(rol->usrConfig,
			    (uint8_t *)rol->dabufp,
			    ROCID, inum++, maxsize);

      if(nwords > 0)
	rol->dabufp += nwords;

      UECLOSE;
    }


  printf("rocPrestart: User Prestart Executed\n");

}

/****************************************
 *  GO
 ****************************************/
void
rocGo()
{

  /* Print out the Run Number and Run Type (config id) */
  printf("rocGo: Activating Run Number %d, Config id = %d\n",
	 rol->runNumber,rol->runType);

  DALMAGO;
  tiStatus(0);
  faSDC_Status(0);
  faSDC_Status_Integrating(0);
  faGStatus(0);
  if(hd_params.enabled)
    hdStatus(0);
  DALMASTOP;

  if(UITF_RUN_TYPE == UITF_COUNTING)
    {
      /* Enable syncreset source */
      faGEnableSyncSrc();

      /* Sync Reset to init fadc250 timestamp and internal buffers */
      faSDC_Sync();
      taskDelay(1);


      if(hd_params.enabled)
	  hdEnable();

      faEnable(fadc_params[UITF_COUNTING].slot, 0, 0);
    }
  else if(UITF_RUN_TYPE == UITF_INTEGRATING)
    {
      faSDC_Sync_Integrating();
      taskDelay(1);

      tiIntEnable(1);

      taskDelay(1);
      faEnable(fadc_params[UITF_INTEGRATING].slot, 0, 0);
    }

}

/****************************************
 *  END
 ****************************************/
void
rocEnd()
{
  faGDisable(0);

  if(hd_params.enabled)
    hdDisable();

  DALMAGO;
  tiStatus(0);
  faSDC_Status(0);
  faSDC_Status_Integrating(0);
  faGStatus(0);
  if(hd_params.enabled)
    hdStatus(0);
  DALMASTOP;

  printf("rocEnd: Ended after %d blocks\n",tiGetIntCount());

}

/****************************************
 *  TRIGGER
 ****************************************/
void
rocTrigger(int arg)
{
  extern int32_t nfadc;
  int ev_num = 0, dCnt = 0;
  int timeout=0, maxtime = 100;

  ev_num = tiGetIntCount();

  /* Setup Address and data modes for DMA transfers
   *
   *  vmeDmaConfig(addrType, dataType, sstMode);
   *
   *  addrType = 0 (A16)    1 (A24)    2 (A32)
   *  dataType = 0 (D16)    1 (D32)    2 (BLK32) 3 (MBLK) 4 (2eVME) 5 (2eSST)
   *  sstMode  = 0 (SST160) 1 (SST267) 2 (SST320)
   */
  vmeDmaConfig(2,5,1);

  /* Readout the trigger block from the TI
     Trigger Block MUST be readout first */
  dCnt = tiReadTriggerBlock(dma_dabufp);

  if(dCnt<=0)
    {
      printf("ERROR: Event %d: No TI Trigger data or error.  dCnt = %d\n",
	     ev_num, dCnt);
    }
  else
    { /* TI Data is already in a bank structure.  Bump the pointer */
      dma_dabufp += dCnt;
    }


  /* Helicity Decoder readout */
  if(hd_params.enabled)
    {
      dCnt = 0;
      BANKOPEN(HELICITY_DECODER_BANK, BT_UI4, blockLevel);
      while((hdBReady()!=1) && (timeout<maxtime))
	{
	  timeout++;
	}

      if(timeout>=maxtime)
	{
	  printf("%s: ERROR: TIMEOUT waiting for Helicity Decoder Block Ready\n",
		 __func__);
	}
      else
	{
	  dCnt = hdReadBlock(dma_dabufp, 1024>>2,1);
	  if(dCnt<=0)
	    {
	      printf("%s: ERROR or NO data from hdReadBlock(...) = %d\n",
		     __func__, dCnt);
	    }
	  else
	    {
	      dma_dabufp += dCnt;
	    }
	}

      BANKCLOSE;
    }


  /* fADC250 Readout */
  BANKOPEN(FADC250_DECODER_BANK,BT_UI4,blockLevel);

  timeout = 0;
  while((faBready(fadc_params[UITF_RUN_TYPE].slot) != 1) && (timeout<maxtime))
    {
      timeout++;
    }

  if(timeout>=maxtime)
    {
      printf("%s: ERROR: TIMEOUT waiting for FADC (slot = %d) Block Ready\n",
	     __func__, fadc_params[UITF_RUN_TYPE].slot);
    }
  else
    {
      int32_t blockError = 0;

      dCnt = faReadBlock(fadc_params[UITF_RUN_TYPE].slot, dma_dabufp, MAXFADCWORDS, 1);

      blockError = faGetBlockError(1);
      if(blockError)
	{
	  printf("ERROR: Slot %d: in transfer (event = %d), dCnt = 0x%x\n",
		 fadc_params[UITF_RUN_TYPE].slot, ev_num, dCnt);

	  if(dCnt > 0)
	    dma_dabufp += dCnt;
	}
      else
	{
	  dma_dabufp += dCnt;
	}
    }
  BANKCLOSE;

  /* Check for SYNC Event */
  if(tiGetSyncEventFlag() == 1)
    {
      /* Check for data available */
      int davail = tiBReady();
      if(davail > 0)
	{
	  printf("%s: ERROR: TI Data available (%d) after readout in SYNC event \n",
		 __func__, davail);

	  while(tiBReady())
	    {
	      vmeDmaFlush(tiGetAdr32());
	    }
	}

      if(hd_params.enabled)
	{
	  davail = hdBReady();
	  if(davail > 0)
	    {
	      printf("%s: ERROR: Helicity Decoder Data available (%d) after readout in SYNC event \n",
		     __func__, davail);

	      while(hdBReady(0))
		{
		  vmeDmaFlush(hdGetA32());
		}
	    }
	}

      davail = faBready(fadc_params[UITF_RUN_TYPE].slot);
      if(davail > 0)
	{
	  printf("%s: ERROR: fADC250 Data available (%d) after readout in SYNC event \n",
		 __func__, davail);

	  while(faBready(fadc_params[UITF_RUN_TYPE].slot))
	    {
	      vmeDmaFlush(faGetA32(fadc_params[UITF_RUN_TYPE].slot));
	    }
	}
    }
}

void
rocLoad()
{
  dalmaInit(1);
}

void
rocCleanup()
{
  printf("%s: Reset all Modules\n",__FUNCTION__);
  tiResetSlaveConfig();
  faGReset(1);
  dalmaClose();
}

/*
  Local Variables:
  compile-command: "make -k uitf_list.so "
  End:
*/
