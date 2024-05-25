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
#include "tiprimary_list.c"
#include "dmaBankTools.h"

/* Define initial blocklevel and buffering level */
#define BLOCKLEVEL 1
#define BUFFERLEVEL 1

/* Library to pipe stdout to daLogMsg */
#include "dalmaRolLib.h"

/* Event type 137 code */
#include "rocUtils.c"

/* fadc library*/
#include "fadcLib.h"
extern int32_t nfadc;
extern uint32_t fadcA32Base;
int32_t MAXFADCWORDS = 0;

/*
  Global to configure the trigger source
      0 : tsinputs
      1 : TI random pulser
      2 : TI fixed pulser

  Set with rocSetTriggerSource(int source);
*/
int rocTriggerSource = 1;
void rocSetTriggerSource(int source); // routine prototype

/****************************************
 *  DOWNLOAD
 ****************************************/
void
rocDownload()
{
  int stat;

  /* Setup Address and data modes for DMA transfers
   *
   *  vmeDmaConfig(addrType, dataType, sstMode);
   *
   *  addrType = 0 (A16)    1 (A24)    2 (A32)
   *  dataType = 0 (D16)    1 (D32)    2 (BLK32) 3 (MBLK) 4 (2eVME) 5 (2eSST)
   *  sstMode  = 0 (SST160) 1 (SST267) 2 (SST320)
   */
  vmeDmaConfig(2,5,1);

  /* Define BLock Level */
  blockLevel = BLOCKLEVEL;


  /*****************
   *   TI SETUP
   *****************/
  /*
   * Set Trigger source
   *    For the TI-Master, valid sources:
   *      TI_TRIGGER_FPTRG     2  Front Panel "TRG" Input
   *      TI_TRIGGER_TSINPUTS  3  Front Panel "TS" Inputs
   *      TI_TRIGGER_TSREV2    4  Ribbon cable from Legacy TS module
   *      TI_TRIGGER_PULSER    5  TI Internal Pulser (Fixed rate and/or random)
   */
  if(rocTriggerSource == 0)
    {
      tiSetTriggerSource(TI_TRIGGER_TSINPUTS); /* TS Inputs enabled */
    }
  else
    {
      tiSetTriggerSource(TI_TRIGGER_PULSER); /* Internal Pulser */
    }

  /* Enable set specific TS input bits (1-6) */
  tiEnableTSInput( TI_TSINPUT_1 | TI_TSINPUT_2 );

  /* Load the trigger table that associates
   *  pins 21/22 | 23/24 | 25/26 : trigger1
   *  pins 29/30 | 31/32 | 33/34 : trigger2
   */
  tiLoadTriggerTable(0);

  tiSetTriggerHoldoff(1,10,0);
  tiSetTriggerHoldoff(2,10,0);

  /* Set initial number of events per block */
  tiSetBlockLevel(blockLevel);

  /* Set Trigger Buffer Level */
  tiSetBlockBufferLevel(BUFFERLEVEL);

  tiSetTriggerPulse(1,0,25,0);

  /* Set prompt output width (127 + 2) * 4 = 516 ns */
  tiSetPromptTriggerWidth(127);

  /* Init all FADC Modules Here */
  int32_t iflag = 0xea00; /* SDC Board address */
  iflag |= FA_INIT_EXT_SYNCRESET;  /* Front panel sync-reset */
  iflag |= FA_INIT_FP_TRIG;  /* Front Panel Input trigger source */
  iflag |= FA_INIT_FP_CLKSRC;  /* Internal 250MHz Clock source */

  uint32_t FADC_ADDR = 0xed0000, FADC_INCR = 0x10000, NFADC=2;

  fadcA32Base = 0x09000000;

  vmeSetQuietFlag(1);
  faInit(FADC_ADDR, FADC_INCR, NFADC, iflag);
  vmeSetQuietFlag(0);

  faSDC_Status(0);
  faGStatus(0);

  tiStatus(0);

  printf("rocDownload: User Download Executed\n");

}

/****************************************
 *  PRESTART
 ****************************************/
void
rocPrestart()
{

  /* Program modules */
  int ifa;

  int32_t FADC_MODE = 0, FADC_WINDOW_LAT = 0, FADC_WINDOW_WIDTH = 0;

  /* Program/Init VME Modules Here */
  for(ifa = 0; ifa < nfadc; ifa++)
    {
      /* Set clock source to FP (from faSDC) */
      faSetClockSource(faSlot(ifa), FA_REF_CLK_FP);

      faSoftReset(faSlot(ifa),0);
      faResetTriggerCount(faSlot(ifa));

      faEnableBusError(faSlot(ifa));

      /* Set input DAC level */
      faSetDAC(faSlot(ifa), 3250, 0);

      /*  Set All channel thresholds to 150 */
      faSetThreshold(faSlot(ifa), 150, 0xffff);

      /*********************************************************************************
       * faSetProcMode(int id, int pmode, unsigned int PL, unsigned int PTW,
       *    int NSB, unsigned int NSA, unsigned int NP,
       *    unsigned int NPED, unsigned int MAXPED, unsigned int NSAT);
       *
       *  id    : fADC250 Slot number
       *  pmode : Processing Mode
       *          9 - Pulse Parameter (ped, sum, time)
       *         10 - Debug Mode (9 + Raw Samples)
       *    PL : Window Latency
       *   PTW : Window Width
       *   NSB : Number of samples before pulse over threshold
       *   NSA : Number of samples after pulse over threshold
       *    NP : Number of pulses processed per window
       *  NPED : Number of samples to sum for pedestal
       *MAXPED : Maximum value of sample to be included in pedestal sum
       *  NSAT : Number of consecutive samples over threshold for valid pulse
       */
      faSetProcMode(faSlot(ifa),
		    FADC_MODE,   /* Processing Mode */
		    FADC_WINDOW_LAT, /* PL */
		    FADC_WINDOW_WIDTH,  /* PTW */
		    3,   /* NSB */
		    6,   /* NSA */
		    1,   /* NP */
		    4,   /* NPED */
		    250, /* MAXPED */
		    2);  /* NSAT */

      /* Enable hitbits for scalers and CTP */
      faSetHitbitsMode(faSlot(ifa), 1);

    }

  /* Enable syncreset source */
  faGEnableSyncSrc();

  /* Sync Reset to synchronize TI and fadc250 timestamps and their internal buffers */
  faSDC_Sync();


  DALMAGO;
  faGStatus(0);
  faSDC_Status(0);
  tiStatus(0);
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

  int bufferLevel = 0;
  /* Get the current buffering settings (blockLevel, bufferLevel) */
  blockLevel = tiGetCurrentBlockLevel();
  bufferLevel = tiGetBroadcastBlockBufferLevel();
  printf("%s: Block Level = %d,  Buffer Level (broadcasted) = %d (%d)\n",
	 __func__,
	 blockLevel,
	 tiGetBlockBufferLevel(),
	 bufferLevel);




  faGSetBlockLevel(blockLevel);

  /*  Enable FADC */
  faGEnable(0, 0);




  if(rocTriggerSource != 0)
    {
      printf("************************************************************\n");
      daLogMsg("INFO","TI Configured for Internal Pulser Triggers");
      printf("************************************************************\n");

      if(rocTriggerSource == 1)
	{
	  /* Enable Random at rate 500kHz/(2^7) = ~3.9kHz */
	  	  tiSetRandomTrigger(1,0xd);
	  //tiSetRandomTrigger(1,0x4);
	}

      if(rocTriggerSource == 2)
	{
	  /*    Enable fixed rate with period (ns)
		120 +30*700*(1024^0) = 21.1 us (~47.4 kHz)
		- arg2 = 0xffff - Continuous
		- arg2 < 0xffff = arg2 times
	  */
	  tiSoftTrig(1,0xffff,100,0);
	}
    }

  DALMAGO;
  tiStatus(0);
  DALMASTOP;
}

/****************************************
 *  END
 ****************************************/
void
rocEnd()
{

  if(rocTriggerSource == 1)
    {
      /* Disable random trigger */
      tiDisableRandomTrigger();
    }

  if(rocTriggerSource == 2)
    {
      /* Disable Fixed Rate trigger */
      tiSoftTrig(1,0,100,0);
    }

  /* FADC Disable */
  faGDisable(0);


  DALMAGO;
  tiStatus(0);
  faGStatus(0);
  DALMASTOP;

  printf("rocEnd: Ended after %d blocks\n",tiGetIntCount());

}

/****************************************
 *  TRIGGER
 ****************************************/
void
rocTrigger(int arg)
{
  int ev_num = 0, dCnt = 0;

  ev_num = tiGetIntCount();

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

  /* fADC250 Readout */
  BANKOPEN(3,BT_UI4,0);

  /* Mask of initialized modules */
  uint32_t scanmask = faScanMask();
  /* Check scanmask for block ready up to 100 times */
  uint32_t datascan = faGBlockReady(scanmask, 100);
  int32_t stat = (datascan == scanmask);

  if(stat)
    {
      int32_t ifa = 0, blockError = 0;
      for(ifa = 0; ifa < nfadc; ifa++)
	{
	  dCnt = faReadBlock(faSlot(ifa), dma_dabufp, MAXFADCWORDS, 1);

	  /* Check for ERROR in block read */
	  blockError = faGetBlockError(1);

	  if(blockError)
	    {
	      printf("ERROR: Slot %d: in transfer (event = %d), dCnt = 0x%x\n",
		     faSlot(ifa), ev_num, dCnt);

	      if(dCnt > 0)
		dma_dabufp += dCnt;
	    }
	  else
	    {
	      dma_dabufp += dCnt;
	    }
	}
    }
  else
    {
      printf("ERROR: Event %d: Datascan != Scanmask  (0x%08x != 0x%08x)\n",
	     ev_num, datascan, scanmask);
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

      int32_t ifa;
      for(ifa = 0; ifa < nfadc; ifa++)
	{
	  davail = faBready(faSlot(ifa));
	  if(davail > 0)
	    {
	      printf("%s: ERROR: fADC250 Data available (%d) after readout in SYNC event \n",
		     __func__, davail);

	      while(faBready(faSlot(ifa)))
		{
		  vmeDmaFlush(faGetA32(faSlot(ifa)));
		}
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


void
rocSetTriggerSource(int source)
{
  if(TIPRIMARYflag == 1)
    {
      printf("%s: ERROR: Trigger Source already enabled.  Ignoring change to %d.\n",
	     __func__, source);
    }
  else
    {
      rocTriggerSource = source;

      if(rocTriggerSource == 0)
	{
	  tiSetTriggerSource(TI_TRIGGER_TSINPUTS); /* TS Inputs enabled */
	}
      else
	{
	  tiSetTriggerSource(TI_TRIGGER_PULSER); /* Internal Pulser */
	}

      daLogMsg("INFO","Setting trigger source (%d)", rocTriggerSource);
    }
}


/*
  Local Variables:
  compile-command: "make -k uitf_list.so "
  End:
*/
