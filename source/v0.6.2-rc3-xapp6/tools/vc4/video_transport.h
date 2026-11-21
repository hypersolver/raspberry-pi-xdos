#pragma once

#include <stdbool.h>
#include <stdint.h>

#define VCHIQ_FOURCC(a,b,c,d) (((uint32_t)(a)<<24)|((uint32_t)(b)<<16)|((uint32_t)(c)<<8)|(uint32_t)(d))
#define vchiq_bus_addr(address) ((uint32_t)(address)|0xc0000000u)

typedef struct {
    void (*on_data)(const void *, unsigned);
    void (*on_bulk_tx_done)(void *, int);
} vchiq_callbacks_t;

#define vchiq_init xdos_video_vchi_init
#define vchiq_open_service xdos_video_service_open
#define vchiq_close_service xdos_video_service_close
#define vchiq_queue_message xdos_video_queue
#define vchiq_poll xdos_video_poll
#define vchiq_alloc_shared xdos_video_alloc
#define vchiq_free_shared xdos_video_free

#ifdef __cplusplus
extern "C" {
#endif
bool vchiq_init(void);
int vchiq_open_service(uint32_t, short, short, const vchiq_callbacks_t *);
bool vchiq_close_service(int);
bool vchiq_queue_message(int, const void *, unsigned);
void vchiq_poll(void);
uint32_t vchiq_alloc_shared(uint32_t, uint32_t *);
void vchiq_free_shared(uint32_t);
void xdos_video_cache_sync(uint32_t, uint32_t);
void xdos_video_log(const char *, ...);
void xdos_video_set_log(void (*sink)(const char *));
unsigned vc4_clock_us(void);
void vc4_poll(void);
int xdos_video_read(void *, unsigned);
unsigned xdos_video_size(void);
#ifdef __cplusplus
}
#endif
