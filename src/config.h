#pragma once

#include "structs.h"

void initconfig(state_t* s);
void readconfig(state_t* s, config_data_t* data);
void reloadconfig(state_t* s, config_data_t* data);
void destroyconfig(void);
