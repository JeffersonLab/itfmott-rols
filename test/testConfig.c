
#include "../uitf_config.c"

int32_t
main(int32_t argc, char *argv[])
{

  if(argc == 2)
    {
      if(uitf_config_init(argv[1]) < 0)
	return -1;
    }
  else
    {
      if(uitf_config_init("uitf_mott.cfg") < 0)
	return -1;
    }

  vmeOpenDefaultWindows();
  tiInit(0x180000,0,0);
  uitf_config_modules_init();

  tiStatus(0);
  faGStatus(0);
  faSDC_Status(0);
  faSDC_Status_Integrating(0);
  vmeCloseDefaultWindows();

  return 0;
}
/*
  Local Variables:
  compile-command: "make -k testConfig "
  End:
*/
