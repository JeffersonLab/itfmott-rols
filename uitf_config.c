/*************************************************************************
 *
 *  uitf_config.c - Library of routines to read configuration parameters
 *                  for the modules in the MOTT DAQ
 *
 *
 *
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <libconfig.h>

#include "uitf_config.h"
#include "jvme.h"
#include "tiLib.h"
#include "hdLib.h"
#include "fadcLib.h"

config_t uitfCfg;


ti_config_t ti_params;
hd_config_t hd_params;
fadc_config_t fadc_params[2];

/**
 * @details Initialize the library with the config filename
 * @param[in] filename Config filename
 * @return 0 if successful, otherwise -1
 */

int32_t
uitf_config_init(char *filename)
{
  if(filename==NULL)
    {
      printf("%s: ERROR: filename may not be NULL\n", __func__);
      return -1;
    }

  config_init(&uitfCfg);

  /* Read the file. If there is an error, report it and exit. */
  if(! config_read_file(&uitfCfg, filename))
    {
      printf("Error in %s:%d - %s\n",
	     config_error_file(&uitfCfg),
	     config_error_line(&uitfCfg), config_error_text(&uitfCfg));
      config_destroy(&uitfCfg);
      return -1;
    }

  memset(&ti_params, 0, sizeof(ti_params));
  memset(&hd_params, 0, sizeof(hd_params));
  memset(&fadc_params, 0, 2*sizeof(fadc_params));

  return 0;
}

#define FIND_N_FILL(x_config_setting, x_params, x_param_name) {		\
    config_setting_lookup_int(x_config_setting, #x_param_name,		\
			      (int32_t *)&x_params.x_param_name);}

#define PRINT_PARAM(x_params, x_params_name) {				\
    printf("%s: %s.%s   0x%08x\n", __func__, #x_params, #x_params_name,	\
	   x_params.x_params_name);}

/**
 * @details Parse the configfile opened with uitf_config_init
 * @return 0 if successful, otherwise -1
 */
int32_t
uitf_config_parse()
{

  config_setting_t *confti, *confhd, *confadc;
  int32_t len;

  //
  // TI
  //
  confti = config_lookup(&uitfCfg, "ti");
  if(config_setting_length(confti) < 0)
    {
      printf("%s: ERROR: TI missing from config\n",
	   __func__);
      return -1;
    }

  FIND_N_FILL(confti, ti_params, blocklevel);
  FIND_N_FILL(confti, ti_params, bufferlevel);
  FIND_N_FILL(confti, ti_params, prescale);

  // ti: check for trigger rules
  config_setting_t *trig_rules = config_setting_get_member(confti, "trigger_rules");
  if(trig_rules==NULL)
    {
      printf("%s: didn't find trigger_rules\n", __func__);
      return -1;
    }

  int32_t itr = 0, ntr = 0;
  ntr = config_setting_length(trig_rules);
  if(ntr != 4)
    {
      printf("%s: not the right number of ntr (%d)\n", __func__, ntr);
      return -1;
    }
  for(itr = 0; itr < ntr; itr++)
    {
      // get each trigger rule and extract period and timestep
      const config_setting_t *tr = config_setting_get_elem(trig_rules, itr);

      FIND_N_FILL(tr, ti_params.rule[itr], period);
      FIND_N_FILL(tr, ti_params.rule[itr], timestep);
    }



  // ti: check for random pulser
  config_setting_t *rand_pulser = config_setting_get_member(confti, "random_pulser");
  if(rand_pulser==NULL)
    {
      printf("%s: didn't find random_pulser\n", __func__);
      return -1;
    }
  FIND_N_FILL(rand_pulser, ti_params.random, enabled);
  FIND_N_FILL(rand_pulser, ti_params.random, prescale);

  // ti: check for fixed pulser
  config_setting_t *fixed_pulser = config_setting_get_member(confti, "fixed_pulser");
  if(fixed_pulser==NULL)
    {
      printf("%s: didn't find fixed_pulser\n", __func__);
      return -1;
    }
  FIND_N_FILL(fixed_pulser, ti_params.fixed, enabled);
  FIND_N_FILL(fixed_pulser, ti_params.fixed, nevents);
  FIND_N_FILL(fixed_pulser, ti_params.fixed, period);
  FIND_N_FILL(fixed_pulser, ti_params.fixed, timestep);


  //
  // Helcity Decoder
  //
  confhd = config_lookup(&uitfCfg, "helicity_decoder");
  if(config_setting_length(confhd) < 0)
    {
      printf("%s: ERROR: helicity decoder missing from config\n",
	   __func__);
      return -1;
    }
  FIND_N_FILL(confhd, hd_params, enabled);
  FIND_N_FILL(confhd, hd_params, address);
  FIND_N_FILL(confhd, hd_params, slot);

  FIND_N_FILL(confhd, hd_params, input_delay);
  FIND_N_FILL(confhd, hd_params, trigger_latency_delay);

  FIND_N_FILL(confhd, hd_params, use_internal_helicity);

  // hd: check for internal_helicity
  config_setting_t *int_helicity = config_setting_get_member(confhd, "internal_helicity");
  if(int_helicity==NULL)
    {
      printf("%s: didn't find internal_helicity\n", __func__);
      return -1;
    }
  FIND_N_FILL(int_helicity, hd_params.internal, helicity_pattern);
  FIND_N_FILL(int_helicity, hd_params.internal, window_delay);
  FIND_N_FILL(int_helicity, hd_params.internal, settle_time);
  FIND_N_FILL(int_helicity, hd_params.internal, stable_time);
  FIND_N_FILL(int_helicity, hd_params.internal, seed);

  //
  // fadc250
  //

  int32_t ifa, nfa, itype = 0;
  confadc = config_lookup(&uitfCfg, "fadc250");
  nfa = config_setting_length(confadc);
  if(nfa != 2)
    {
      printf("%s: ERROR: fadc250 missing from config.  Found: %d\n",
	     __func__, nfa);
      return -1;
    }

  for(ifa = 0; ifa < nfa; ifa++)
    {
      const config_setting_t *fa = config_setting_get_elem(confadc, ifa);

      const char *fadc_type;
      config_setting_lookup_string(fa, "type", &fadc_type);

      if(strcasecmp(fadc_type, "counting") == 0)
	itype = UITF_COUNTING;
      else if(strcasecmp(fadc_type, "integrating") == 0)
	itype = UITF_INTEGRATING;
      else
	{
	  printf("%s: ERROR: unknown fadc250 type (%s)\n",
		 __func__, fadc_type);
	  return -1;
	}

      FIND_N_FILL(fa, fadc_params[itype], address);
      FIND_N_FILL(fa, fadc_params[itype], slot);
      FIND_N_FILL(fa, fadc_params[itype], sd_fp_address);

      FIND_N_FILL(fa, fadc_params[itype], init_arg);

      FIND_N_FILL(fa, fadc_params[itype], mode);
      FIND_N_FILL(fa, fadc_params[itype], pl);
      FIND_N_FILL(fa, fadc_params[itype], ptw);
      FIND_N_FILL(fa, fadc_params[itype], nsb);
      FIND_N_FILL(fa, fadc_params[itype], nsa);
      FIND_N_FILL(fa, fadc_params[itype], np);

      FIND_N_FILL(fa, fadc_params[itype], delay8);
      FIND_N_FILL(fa, fadc_params[itype], delay9);
      FIND_N_FILL(fa, fadc_params[itype], delay11);
      FIND_N_FILL(fa, fadc_params[itype], dac[0]);

      // fadc250: check for dac
      config_setting_t *dac = config_setting_get_member(fa, "dac");
      if(dac==NULL)
	{
	  printf("%s: didn't find dac\n", __func__);
	  return -1;
	}

      int32_t idac = 0, ndac = 0;
      ndac = config_setting_length(dac);
      if(ndac != 16)
	{
	  printf("%s: not the right number of dac (%d)\n", __func__, ndac);
	  return -1;
	}

      for(idac = 0; idac < ndac; idac ++)
	fadc_params[itype].dac[idac] = config_setting_get_int_elem(dac, idac);

      // fadc250: check for threshold
      config_setting_t *threshold = config_setting_get_member(fa, "threshold");
      if(threshold==NULL)
	{
	  printf("%s: fadc250: %s: didn't find threshold\n", __func__,
		 fadc_type);
	  return -1;
	}

      int32_t itet = 0, ntet = 0;
      ntet = config_setting_length(threshold);
      if(ntet != 16)
	{
	  printf("%s: not the right number of threshold (%d)\n", __func__, ntet);
	  return -1;
	}

      for(itet = 0; itet < ntet; itet ++)
	fadc_params[itype].threshold[itet] = config_setting_get_int_elem(threshold, itet);

    }

  return 0;
}

// use the config file parameters to init libraries and configure modules
int32_t
uitf_config_modules_init()
{
  int32_t stat = OK;

  /* Load the trigger table that associates
   *  pins 21/22 | 23/24 | 25/26 : trigger1
   *  pins 29/30 | 31/32 | 33/34 : trigger2
   */
  tiLoadTriggerTable(0);

  /* Set prompt output width (10 + 2) * 4 = 48 ns */
  tiSetPromptTriggerWidth(10);

  int32_t irule = 0, nrule = 4;
  for(irule = 0; irule < nrule; irule++)
    tiSetTriggerHoldoff(irule, ti_params.rule[irule].period,
			ti_params.rule[irule].timestep);

  /* Set initial number of events per block */
  tiSetBlockLevel(ti_params.blocklevel);

  /* Set Trigger Buffer Level */
  tiSetBlockBufferLevel(ti_params.bufferlevel);


  extern u_long fadcA32Base;
  extern uint32_t fadcAddrList[FA_MAX_BOARDS];

  int32_t iflag = fadc_params[UITF_COUNTING].sd_fp_address;
  iflag |= fadc_params[UITF_COUNTING].init_arg;
  iflag |= FA_INIT_USE_ADDRLIST;


  fadcAddrList[UITF_COUNTING] = fadc_params[UITF_COUNTING].address;
  fadcAddrList[UITF_INTEGRATING] = fadc_params[UITF_INTEGRATING].address;

  vmeSetQuietFlag(1);
  fadcA32Base = 0x08800000;
  stat = faInit(0, 0, 2, iflag);
  vmeSetQuietFlag(0);

  faSDC_Init_Integrating(fadc_params[UITF_INTEGRATING].sd_fp_address);

  extern int32_t nfadc;
  int ifa;
  /* Program/Init VME Modules Here */
  for(ifa = 0; ifa < nfadc; ifa++)
    {
      /* Set clock source to FP (from faSDC) */
      faSetClockSource(fadc_params[ifa].slot, FA_REF_CLK_FP);

      faSoftReset(fadc_params[ifa].slot, 0);
      faResetTriggerCount(fadc_params[ifa].slot);

      faEnableBusError(fadc_params[ifa].slot);

      /* Set input DAC level */
      int32_t ich = 0; int32_t nchan = 16;
      for(ich = 0; ich < nchan; ich++)
	{
	  faSetDAC(fadc_params[ifa].slot, fadc_params[ifa].dac[ich], (1 << ich));
	  faSetThreshold(fadc_params[ifa].slot, fadc_params[ifa].dac[ich], (1 << ich));
	}

      faSetProcMode(fadc_params[ifa].slot,
		    fadc_params[ifa].mode,
		    fadc_params[ifa].pl,
		    fadc_params[ifa].ptw,
		    fadc_params[ifa].nsb,
		    fadc_params[ifa].nsa,
		    fadc_params[ifa].np,
		    0);

      /* Enable hitbits for scalers and CTP */
      faSetHitbitsMode(fadc_params[ifa].slot, 1);
    }

  if(hd_params.enabled)
    {
      stat = hdInit(hd_params.address, HD_INIT_FP, HD_INIT_EXTERNAL_FIBER, 0);

      hdSetProcDelay(hd_params.input_delay, hd_params.trigger_latency_delay);

      /* Enable the module decoder, well before triggers are enabled */
      hdEnableDecoder();

      if(hd_params.use_internal_helicity)
	{
	  hdSetHelicitySource(1, 0, 1);
	  hdHelicityGeneratorConfig(hd_params.internal.helicity_pattern,
				    hd_params.internal.window_delay,
				    hd_params.internal.settle_time,
				    hd_params.internal.stable_time,
				    hd_params.internal.seed);
	  hdEnableHelicityGenerator();
	}
    }

  return OK;
}

int32_t
uitf_config_modules_prestart()
{
  // Just enable syncreset sources

  return OK;
}
