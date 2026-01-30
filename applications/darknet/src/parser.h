#ifndef PARSER_H
#define PARSER_H
#include "network.h"
#include "cont.h"

#ifdef __cplusplus


extern "C" {
#endif
static const int NO_CP_ONGOING = 0;
 static const int CP_COPY = 1;
 static const int CP_SHADOW_COPY = 2;
extern int check_point_phase;

network parse_network_cfg(char *filename);
network parse_network_cfg_custom(char *filename, int batch, int time_steps);
void save_weights(network net, char *filename);
void save_weights_2(network net, char *filename, double* pull_time, double* cp_time);
void save_weights_upto(network net, char *filename, int cutoff, int save_ema, double* pull_time, double* cp_time);
int save_weights_libpm(network net, char *filename, bool is_inital_checkpoint, struct container* cont ,
    unsigned int container_num, double* pull_time, double* cp_time, double* flush_time, int skip_layers);
int save_weights_upto_libpm(network net, char *filename, int cutoff, int save_ema, bool is_inital_checkpoint, 
    struct container* cont, unsigned int container_num, double* pull_time, double* cp_time, double* flush_time, int skip_layers);
void save_weights_double(network net, char *filename);
void load_weights(network *net, char *filename);
void load_weights_upto(network *net, char *filename, int cutoff);
void load_weights_upto_libpm(network* net,struct container* cont,int container_num,int cutoff);
void load_weights_libpm(network *net,struct container* cont, unsigned int container_num);


#ifdef __cplusplus
}
#endif
#endif
