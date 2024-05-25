/*************************************************************************
 *
 *  uitf_config.c - Library of routines to read configuration parameters
 *                  for the modules in the MOTT DAQ
 *
 *
 *
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <libconfig.h>

config_t uitfCfg;

typedef struct
{
  uint32_t period;
  uint32_t timestep;
} trigger_rule_t;

typedef struct
{
  uint32_t enabled;
  uint32_t prescale;
} random_pulser_t;

typedef struct
{
  uint32_t enabled;
  uint32_t nevents;
  uint32_t period;
  uint32_t timestep;
} fixed_pulser_t;

typedef struct
{
  uint32_t blocklevel;
  uint32_t bufferlevel;
  uint32_t prescale;
  trigger_rule_t rule[4];
  random_pulser_t random;
  fixed_pulser_t fixed;
} ti_config_t;


typedef struct
{
  uint32_t helicity_pattern;
  uint32_t window_delay;
  uint32_t settle_time;
  uint32_t stable_time;
  uint32_t seed;
} internal_helicity_t;

typedef struct
{
  uint32_t enabled;
  uint32_t address;
  uint32_t slot;
  uint32_t input_delay;
  uint32_t trigger_latency_delay;
  uint32_t use_internal_helicity;
  internal_helicity_t internal;
} hd_config_t;

typedef struct
{
  uint32_t type;
  uint32_t address;
  uint32_t slot;

  uint32_t sd_fp_address;
  uint32_t init_arg;

  uint32_t mode;
  uint32_t pl;
  uint32_t ptw;
  uint32_t nsb;
  uint32_t nsa;
  uint32_t np;

  uint32_t delay8;
  uint32_t delay9;
  uint32_t delay11;

  uint32_t threshold[16];
  uint32_t dac[16];

} fadc_config_t;

enum
  {
    UITF_COUNTING = 0,
    UITF_INTEGRATING = 1
  };

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

/*
  Local Variables:
  compile-command: "cd test; make "
  End:
*/
