#include <libconfig.h>
#include <stdio.h>

int main(int argc, char** argv) {
  if(argc != 2) {
    fprintf(stderr, "usage: %s <config file>\n", argv[0]);
    return 2;
  }

  config_t cfg;
  config_init(&cfg);
  if(config_read_file(&cfg, argv[1])) {
    config_destroy(&cfg);
    printf("sample config is valid.");
    return 0;
  }

  fprintf(stderr, "%s:%d: %s\n", argv[1], config_error_line(&cfg), config_error_text(&cfg));
  config_destroy(&cfg);
  return 1;
}
