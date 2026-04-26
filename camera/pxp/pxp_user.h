#ifndef __PXP_USER_H__
#define __PXP_USER_H__

#include <stdint.h>
#include <stdbool.h>
#include <sys/ioctl.h>

typedef uint64_t __u64;
typedef uint32_t __u32;
typedef uint16_t __u16;
typedef uint8_t  __u8;
typedef unsigned long dma_addr_t;

struct rect {
	int top;
	int left;
	int width;
	int height;
};

#define fourcc(a, b, c, d) \
	(((__u32)(a) << 0) | ((__u32)(b) << 8) | ((__u32)(c) << 16) | ((__u32)(d) << 24))

#define PXP_PIX_FMT_RGB565  fourcc('R', 'G', 'B', 'P')
#define PXP_PIX_FMT_YUYV    fourcc('Y', 'U', 'Y', 'V')

enum pxp_working_mode {
	PXP_MODE_LEGACY   = 0x1,
	PXP_MODE_STANDARD = 0x2,
	PXP_MODE_ADVANCED = 0x4,
};

enum pxp_engine_ctrl {
	PXP_ENABLE_ROTATE0             = 0x001,
	PXP_ENABLE_ROTATE1             = 0x002,
	PXP_ENABLE_LUT                 = 0x004,
	PXP_ENABLE_CSC2                = 0x008,
	PXP_ENABLE_ALPHA_B             = 0x010,
	PXP_ENABLE_INPUT_FETCH_SOTRE   = 0x020,
	PXP_ENABLE_WFE_B               = 0x040,
	PXP_ENABLE_WFE_A               = 0x080,
	PXP_ENABLE_DITHER              = 0x100,
	PXP_ENABLE_PS_AS_OUT           = 0x200,
	PXP_ENABLE_COLLISION_DETECT    = 0x400,
	PXP_ENABLE_HANDSHAKE           = 0x1000,
	PXP_ENABLE_DITHER_BYPASS       = 0x2000,
};

struct pxp_layer_param {
	unsigned short left;
	unsigned short top;
	unsigned short width;
	unsigned short height;
	unsigned short stride;
	unsigned int pixel_fmt;

	unsigned int flag;
	bool combine_enable;
	unsigned int color_key_enable;
	unsigned int color_key;
	bool global_alpha_enable;
	bool global_override;
	unsigned char global_alpha;
	bool alpha_invert;
	bool local_alpha_enable;
	int comp_mask;

	dma_addr_t paddr;
};

struct pxp_proc_data {
	int scaling;
	int hflip;
	int vflip;
	int rotate;
	int rot_pos;
	int yuv;

	struct rect srect;
	struct rect drect;

	unsigned int bgcolor;
	int overlay_state;
	int lut_transform;
	unsigned char *lut_map;
	bool lut_map_updated;
	bool combine_enable;

	__u64 lut_sels;
	enum pxp_working_mode working_mode;
	enum pxp_engine_ctrl engine_enable;

	bool partial_update;
	bool alpha_en;
	bool lut_update;
	bool reagl_en;
	bool reagl_d_en;
	bool detection_only;
	int lut;
	bool lut_cleanup;
	unsigned int lut_status_1;
	unsigned int lut_status_2;

	int dither_mode;
	unsigned int quant_bit;
};

struct pxp_config_data {
	struct pxp_layer_param s0_param;
	struct pxp_layer_param ol_param[8];
	struct pxp_layer_param out_param;
	struct pxp_layer_param wfe_a_fetch_param[2];
	struct pxp_layer_param wfe_a_store_param[2];
	struct pxp_layer_param wfe_b_fetch_param[2];
	struct pxp_layer_param wfe_b_store_param[2];
	struct pxp_layer_param dither_fetch_param[2];
	struct pxp_layer_param dither_store_param[2];
	struct pxp_proc_data proc_data;
	int layer_nr;
	int handle;
};

struct pxp_chan_handle {
	unsigned int handle;
	int hist_status;
};

struct pxp_mem_desc {
	unsigned int handle;
	unsigned int size;
	dma_addr_t phys_addr;
	unsigned int virt_uaddr;		/* virtual user space address */
	unsigned int mtype;
};

struct pxp_mem_flush {
	unsigned int handle;
	unsigned int type;
};

#define PXP_IOC_MAGIC  'P'

#define PXP_IOC_GET_CHAN      _IOR(PXP_IOC_MAGIC, 0, struct pxp_mem_desc)
#define PXP_IOC_PUT_CHAN      _IOW(PXP_IOC_MAGIC, 1, struct pxp_mem_desc)
#define PXP_IOC_CONFIG_CHAN   _IOW(PXP_IOC_MAGIC, 2, struct pxp_mem_desc)
#define PXP_IOC_START_CHAN    _IOW(PXP_IOC_MAGIC, 3, struct pxp_mem_desc)
#define PXP_IOC_GET_PHYMEM    _IOWR(PXP_IOC_MAGIC, 4, struct pxp_mem_desc)
#define PXP_IOC_PUT_PHYMEM    _IOW(PXP_IOC_MAGIC, 5, struct pxp_mem_desc)
#define PXP_IOC_WAIT4CMPLT    _IOWR(PXP_IOC_MAGIC, 6, struct pxp_mem_desc)
#define PXP_IOC_FLUSH_PHYMEM   _IOR(PXP_IOC_MAGIC, 7, struct pxp_mem_flush)

/* Memory types supported*/
#define MEMORY_TYPE_UNCACHED 0x0
#define MEMORY_TYPE_WC	     0x1
#define MEMORY_TYPE_CACHED   0x2

/* Cache flush operations */
#define CACHE_CLEAN      0x1
#define CACHE_INVALIDATE 0x2
#define CACHE_FLUSH      0x4

#endif
