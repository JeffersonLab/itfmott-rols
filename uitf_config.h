#pragma once
/*************************************************************************
 *
 *  uitf_config.h - Library of routines to read configuration parameters
 *                  for the modules in the MOTT DAQ
 *
 *
 *
 */

#include <stdint.h>

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

int32_t uitf_config_init(char *filename);
int32_t uitf_config_parse();
int32_t uitf_config_modules_init();
int32_t uitf_config_modules_prestart();
