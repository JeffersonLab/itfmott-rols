
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

  if(uitf_config_parse() < 0)
    return -1;

  return 0;
}
/*
  Local Variables:
  compile-command: "make -k testConfig "
  End:
*/
