#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/gpio/consumer.h>
#include <linux/delay.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-ctrls.h>

#define MY_OV5640_ARRAY_SIZE(x) sizeof(x) / sizeof((x)[0])

#define OV5640_MODE_MAX					2
#define OV5640_I2C_WRITE_LEN			254
#define OV5640_I2C_READ_LEN				256

#define OV5640_SYSTEM_CTROL0				0x3008
#define OV5640_CHIP_ID_HIGH_BYTE			0x300A
#define OV5640_SCCB_SYSTEM_CTRL1			0x3103

#define OV5640_CHIP_ID			0x5640

struct reg_value {
    u16 reg;
    u8 reg_val;
};

struct ov5640_mode {
	u16 width;
	u16 height;
	u8 fps;
	struct reg_value* reg_list;
	u16 reg_list_len;
};

struct ov5640_dev {
	struct i2c_client* client;
	struct v4l2_captureparm streamcap;
	struct v4l2_subdev subdev;
	struct ov5640_mode* current_mode;
	struct gpio_desc* reset_desc;
	struct gpio_desc* pwdn_desc;
	struct v4l2_ctrl_handler ctrl_handler;
};

static struct ov5640_dev ov5640;

static struct reg_value ov5640_global_setting[] = {
	{0x3008, 0x42},
	{0x3103, 0x03}, {0x3017, 0xff}, {0x3018, 0xff},
	{0x3034, 0x18}, {0x3037, 0x13}, {0x3108, 0x01},
	{0x3630, 0x36}, {0x3631, 0x0e}, {0x3632, 0xe2},
	{0x3633, 0x12}, {0x3621, 0xe0}, {0x3704, 0xa0},
	{0x3703, 0x5a}, {0x3715, 0x78}, {0x3717, 0x01},
	{0x370b, 0x60}, {0x3705, 0x1a}, {0x3905, 0x02},
	{0x3906, 0x10}, {0x3901, 0x0a}, {0x3731, 0x12},
	{0x3600, 0x08}, {0x3601, 0x33}, {0x302d, 0x60},
	{0x3620, 0x52}, {0x371b, 0x20}, {0x471c, 0x50},
	{0x3a13, 0x43}, {0x3a18, 0x00}, {0x3a19, 0xf8},
	{0x3635, 0x13}, {0x3636, 0x03}, {0x3634, 0x40},
	{0x3622, 0x01}, {0x3c01, 0x34}, {0x3c04, 0x28},
	{0x3c05, 0x98}, {0x3c06, 0x00}, {0x3c07, 0x08},
	{0x3c08, 0x00}, {0x3c09, 0x1c}, {0x3c0a, 0x9c},
	{0x3c0b, 0x40}, {0x3810, 0x00}, {0x3811, 0x10},
	{0x3812, 0x00}, {0x3708, 0x64}, {0x4001, 0x02},
	{0x4005, 0x1a}, {0x3000, 0x00}, {0x3004, 0xff},
	{0x300e, 0x58}, {0x302e, 0x00}, {0x4300, 0x30},
	{0x501f, 0x00}, {0x440e, 0x00}, {0x5000, 0xa7},
	{0x3a0f, 0x30}, {0x3a10, 0x28}, {0x3a1b, 0x30}, // AEC Control
	{0x3a1e, 0x26}, {0x3a11, 0x60}, {0x3a1f, 0x14},
	{0x5800, 0x23}, {0x5801, 0x14}, {0x5802, 0x0f}, // lens correction
	{0x5803, 0x0f}, {0x5804, 0x12}, {0x5805, 0x26},
	{0x5806, 0x0c}, {0x5807, 0x08}, {0x5808, 0x05},
	{0x5809, 0x05}, {0x580a, 0x08}, {0x580b, 0x0d},
	{0x580c, 0x08}, {0x580d, 0x03}, {0x580e, 0x00},
	{0x580f, 0x00}, {0x5810, 0x03}, {0x5811, 0x09},
	{0x5812, 0x07}, {0x5813, 0x03}, {0x5814, 0x00},
	{0x5815, 0x01}, {0x5816, 0x03}, {0x5817, 0x08},
	{0x5818, 0x0d}, {0x5819, 0x08}, {0x581a, 0x05},
	{0x581b, 0x06}, {0x581c, 0x08}, {0x581d, 0x0e},
	{0x581e, 0x29}, {0x581f, 0x17}, {0x5820, 0x11},
	{0x5821, 0x11}, {0x5822, 0x15}, {0x5823, 0x28},
	{0x5824, 0x46}, {0x5825, 0x26}, {0x5826, 0x08},
	{0x5827, 0x26}, {0x5828, 0x64}, {0x5829, 0x26},
	{0x582a, 0x24}, {0x582b, 0x22}, {0x582c, 0x24},
	{0x582d, 0x24}, {0x582e, 0x06}, {0x582f, 0x22},
	{0x5830, 0x40}, {0x5831, 0x42}, {0x5832, 0x24},
	{0x5833, 0x26}, {0x5834, 0x24}, {0x5835, 0x22},
	{0x5836, 0x22}, {0x5837, 0x26}, {0x5838, 0x44},
	{0x5839, 0x24}, {0x583a, 0x26}, {0x583b, 0x28},
	{0x583c, 0x42}, {0x583d, 0xce}, {0x5180, 0xff}, // AWB control
	{0x5181, 0xf2}, {0x5182, 0x00}, {0x5183, 0x14},
	{0x5184, 0x25}, {0x5185, 0x24}, {0x5186, 0x09},
	{0x5187, 0x09}, {0x5188, 0x09}, {0x5189, 0x75},
	{0x518a, 0x54}, {0x518b, 0xe0}, {0x518c, 0xb2},
	{0x518d, 0x42}, {0x518e, 0x3d}, {0x518f, 0x56},
	{0x5190, 0x46}, {0x5191, 0xf8}, {0x5192, 0x04},
	{0x5193, 0x70}, {0x5194, 0xf0}, {0x5195, 0xf0},
	{0x5196, 0x03}, {0x5197, 0x01}, {0x5198, 0x04},
	{0x5199, 0x12}, {0x519a, 0x04}, {0x519b, 0x00},
	{0x519c, 0x06}, {0x519d, 0x82}, {0x519e, 0x38},
	{0x5480, 0x01}, {0x5481, 0x08}, {0x5482, 0x14}, // Gamma
	{0x5483, 0x28}, {0x5484, 0x51}, {0x5485, 0x65},
	{0x5486, 0x71}, {0x5487, 0x7d}, {0x5488, 0x87},
	{0x5489, 0x91}, {0x548a, 0x9a}, {0x548b, 0xaa},
	{0x548c, 0xb8}, {0x548d, 0xcd}, {0x548e, 0xdd},
	{0x548f, 0xea}, {0x5490, 0x1d}, {0x5381, 0x1e}, // color matrix
	{0x5382, 0x5b}, {0x5383, 0x08}, {0x5384, 0x0a},
	{0x5385, 0x7e}, {0x5386, 0x88}, {0x5387, 0x7c},
	{0x5388, 0x6c}, {0x5389, 0x10}, {0x538a, 0x01},
	{0x538b, 0x98}, {0x5580, 0x06}, {0x5583, 0x40}, // UV adjust
	{0x5584, 0x10}, {0x5589, 0x10}, {0x558a, 0x00},
	{0x558b, 0xf8}, {0x501d, 0x40}, {0x5300, 0x08}, // CIP
	{0x5301, 0x30}, {0x5302, 0x10}, {0x5303, 0x00},
	{0x5304, 0x08}, {0x5305, 0x30}, {0x5306, 0x08},
	{0x5307, 0x16}, {0x5309, 0x08}, {0x530a, 0x30},
	{0x530b, 0x04}, {0x530c, 0x06}, {0x5025, 0x00},
	{0x3008, 0x02}
};

static struct reg_value ov5640_480p_30fps_setting[] = {
	{0x3035, 0x11}, {0x3036, 0x38}, {0x3c07, 0x08},
	{0x3820, 0x46}, {0x3821, 0x01}, {0x3814, 0x31},
	{0x3815, 0x31}, {0x3800, 0x00}, {0x3801, 0x00},
	{0x3802, 0x00}, {0x3803, 0x04}, {0x3804, 0x0a},
	{0x3805, 0x3f}, {0x3806, 0x07}, {0x3807, 0x9b},
	{0x3808, 0x02}, {0x3809, 0x80}, {0x380a, 0x01},
	{0x380b, 0xe0}, {0x380c, 0x07}, {0x380d, 0x68},
	{0x380e, 0x03}, {0x380f, 0xd8}, {0x3813, 0x06},
	{0x3618, 0x00}, {0x3612, 0x29}, {0x3709, 0x52},
	{0x370c, 0x03}, {0x3a02, 0x03}, {0x3a03, 0xd8},
	{0x3a08, 0x01}, {0x3a09, 0x27}, {0x3a0a, 0x00},
	{0x3a0b, 0xf6}, {0x3a0e, 0x03}, {0x3a0d, 0x04},
	{0x3a14, 0x03}, {0x3a15, 0xd8}, {0x4004, 0x02},
	{0x3002, 0x1c}, {0x3006, 0xc3}, {0x4713, 0x03},
	{0x4407, 0x04}, {0x460b, 0x35}, {0x460c, 0x22},
	{0x4837, 0x22}, {0x3824, 0x02}, {0x5001, 0xa3},
	{0x3503, 0x00},
};

static struct reg_value ov5640_720p_30fps_setting[] = {
	{0x3035, 0x21}, {0x3036, 0x54}, {0x3c07, 0x07},
	{0x3820, 0x46}, {0x3821, 0x01}, {0x3814, 0x31},
	{0x3815, 0x31}, {0x3800, 0x00}, {0x3801, 0x00},
	{0x3802, 0x00}, {0x3803, 0xfa}, {0x3804, 0x0a},
	{0x3805, 0x3f}, {0x3806, 0x06}, {0x3807, 0xa9},
	{0x3808, 0x05}, {0x3809, 0x00}, {0x380a, 0x02},
	{0x380b, 0xd0}, {0x380c, 0x07}, {0x380d, 0x64},
	{0x380e, 0x02}, {0x380f, 0xe4}, {0x3813, 0x04},
	{0x3618, 0x00}, {0x3612, 0x29}, {0x3709, 0x52},
	{0x370c, 0x03}, {0x3a02, 0x02}, {0x3a03, 0xe0},
	{0x3a08, 0x00}, {0x3a09, 0x6f}, {0x3a0a, 0x00},
	{0x3a0b, 0x5c}, {0x3a0e, 0x06}, {0x3a0d, 0x08},
	{0x3a14, 0x02}, {0x3a15, 0xe0}, {0x4004, 0x02},
	{0x3002, 0x1c}, {0x3006, 0xc3}, {0x4713, 0x03},
	{0x4407, 0x04}, {0x460b, 0x37}, {0x460c, 0x20},
	{0x4837, 0x16}, {0x3824, 0x04}, {0x5001, 0x83},
	{0x3503, 0x00},
};

static struct ov5640_mode ov5640_support_modes[] = {
	[0] = {
		.width = 640,
		.height = 480,
		.fps = 30,
		.reg_list = ov5640_480p_30fps_setting,
		.reg_list_len = MY_OV5640_ARRAY_SIZE(ov5640_480p_30fps_setting),
	},
	[1] = {
		.width = 1280,
		.height = 720,
		.fps = 30,
		.reg_list = ov5640_720p_30fps_setting,
		.reg_list_len = MY_OV5640_ARRAY_SIZE(ov5640_720p_30fps_setting),
	}
};

static int i2c_ov5640_write(struct i2c_client *clit, u16 reg_addr,
    u8* buf, u8 len
)
{
    struct i2c_msg msg;
    int ret;
    u8 data[256];

    if(!clit || !buf || len == 0 ||len > OV5640_I2C_WRITE_LEN)
        return -EINVAL;

    data[0] = (reg_addr >> 8) & 0xFF;
    data[1] = reg_addr;
    memcpy(&data[2], buf, len);

    msg.addr = clit->addr;
    msg.flags = 0;
    msg.len = len + 2;
    msg.buf = data;

    ret = i2c_transfer(clit->adapter, &msg, 1);
    
    return ((ret == 1) ? 0 : (ret < 0) ? ret : -EIO);
} 

static int i2c_ov5640_read(struct i2c_client *clit, u16 reg_addr,
    u8* buf, u8 len
)
{
    struct i2c_msg msg[2];
    int ret;
    u8 reg_buf[2];

    if(!clit || !buf || len == 0 || len > OV5640_I2C_READ_LEN)
        return -EINVAL;

    reg_buf[0] = (reg_addr >> 8) & 0xFF;
    reg_buf[1] = reg_addr;
    msg[0].addr = clit->addr;
    msg[0].flags = 0;
    msg[0].len = 2;
    msg[0].buf = reg_buf;

    msg[1].addr = clit->addr;
    msg[1].flags = I2C_M_RD;
    msg[1].len = len;
    msg[1].buf = buf;
    ret = i2c_transfer(clit->adapter, &msg[0], 2);

    return ((ret == 2) ? 0 : (ret < 0) ? ret : -EIO);
}

static int ov5640_set_brightness(struct ov5640_dev* dev, int brightness)
{
	struct reg_value reg_data[5] = {
		{0x3212, 0x03}, {0x5587, 0x00},
		{0x5588, 0x00}, {0x3212, 0x13},
		{0x3212, 0xa3},
	};
	int i;

	if((brightness < -128) || (brightness > 127))	
		return -EINVAL;
	
	if(brightness >= 0) {
		if(0 == brightness) {
			reg_data[1].reg_val = 0;
		}else {
			reg_data[1].reg_val = brightness;
		}
		reg_data[2].reg_val = 0x01;
	}else {
		reg_data[1].reg_val = (-brightness);
		reg_data[2].reg_val = 0x09;
	}

    for(i = 0;i < 5;i++) {
        (void)i2c_ov5640_write(dev->client, reg_data[i].reg, &reg_data[i].reg_val, 1);
    }

	return 0;
}

static int ov5640_set_contrast(struct ov5640_dev* dev, int contrast)
{
	struct reg_value reg_data[5] = {
		{0x3212, 0x03}, {0x5586, 0x00},
		{0x5585, 0x00}, {0x3212, 0x13},
		{0x3212, 0xa3},
	};
	int level;
	int i;

	if((contrast < 0) || (contrast > 255))	
		return -EINVAL;
	
    if (contrast <= 36)       level = -3;
    else if (contrast <= 72)  level = -2;
    else if (contrast <= 108) level = -1;
    else if (contrast <= 144) level = 0;
    else if (contrast <= 180) level = +1;
    else if (contrast <= 216) level = +2;
    else                      level = +3;

    switch (level) {
        case -3: reg_data[1].reg_val = 0x14; reg_data[2].reg_val = 0x14; break;
        case -2: reg_data[1].reg_val = 0x18; reg_data[2].reg_val = 0x18; break;
        case -1: reg_data[1].reg_val = 0x1C; reg_data[2].reg_val = 0x1C; break;
        case 0:  reg_data[1].reg_val = 0x20; reg_data[2].reg_val = 0x00; break;
        case +1: reg_data[1].reg_val = 0x24; reg_data[2].reg_val = 0x10; break;
        case +2: reg_data[1].reg_val = 0x28; reg_data[2].reg_val = 0x18; break;
        case +3: reg_data[1].reg_val = 0x2C; reg_data[2].reg_val = 0x1C; break;
    }

    for(i = 0;i < 5;i++) {
        (void)i2c_ov5640_write(dev->client, reg_data[i].reg, &reg_data[i].reg_val, 1);
    }

	return 0;
}

static int ov5640_set_saturation(struct ov5640_dev* dev, int saturation)
{
	struct reg_value reg_data[14] = {
		{0x3212, 0x03}, {0x5381, 0x1c},
		{0x5382, 0x5a}, {0x5383, 0x06},
		{0x5384, 0x00}, {0x5385, 0x00},
		{0x5386, 0x00}, {0x5387, 0x00},
		{0x5388, 0x00}, {0x5389, 0x00},
		{0x538b, 0x98}, {0x538a, 0x01},
		{0x3212, 0x13}, {0x3212, 0xa3},
	};
	int level;
	int i;

	if((saturation < 0) || (saturation > 255))	
		return -EINVAL;
	
    if (saturation <= 36)       level = -3;
    else if (saturation <= 72)  level = -2;
    else if (saturation <= 108) level = -1;
    else if (saturation <= 144) level = 0;
    else if (saturation <= 180) level = +1;
    else if (saturation <= 216) level = +2;
    else						level = +3;

    switch (level) {
        case -3:
			reg_data[4].reg_val = 0x0c;
			reg_data[5].reg_val = 0x30;
			reg_data[6].reg_val = 0x3d;
			reg_data[7].reg_val = 0x3e;
			reg_data[8].reg_val = 0x3d;
			reg_data[9].reg_val = 0x01;
			break;
        case -2:
			reg_data[4].reg_val = 0x10;
			reg_data[5].reg_val = 0x3d;
			reg_data[6].reg_val = 0x4d;
			reg_data[7].reg_val = 0x4e;
			reg_data[8].reg_val = 0x4d;
			reg_data[9].reg_val = 0x01;
			break;
        case -1:
			reg_data[4].reg_val = 0x15;
			reg_data[5].reg_val = 0x52;
			reg_data[6].reg_val = 0x66;
			reg_data[7].reg_val = 0x68;
			reg_data[8].reg_val = 0x66;
			reg_data[9].reg_val = 0x02;
			break;
        case 0:
			reg_data[4].reg_val = 0x1a;
			reg_data[5].reg_val = 0x66;
			reg_data[6].reg_val = 0x80;
			reg_data[7].reg_val = 0x82;
			reg_data[8].reg_val = 0x80;
			reg_data[9].reg_val = 0x02;
			break;
        case +1:
			reg_data[4].reg_val = 0x1f;
			reg_data[5].reg_val = 0x7a;
			reg_data[6].reg_val = 0x9a;
			reg_data[7].reg_val = 0x9c;
			reg_data[8].reg_val = 0x9a;
			reg_data[9].reg_val = 0x02;
			break;
        case +2:
			reg_data[4].reg_val = 0x24;
			reg_data[5].reg_val = 0x8f;
			reg_data[6].reg_val = 0xb3;
			reg_data[7].reg_val = 0xb6;
			reg_data[8].reg_val = 0xb3;
			reg_data[9].reg_val = 0x03;
			break;
        case +3:
			reg_data[4].reg_val = 0x2b;
			reg_data[5].reg_val = 0xab;
			reg_data[6].reg_val = 0xd6;
			reg_data[7].reg_val = 0xda;
			reg_data[8].reg_val = 0xd6;
			reg_data[9].reg_val = 0x04;
			break;
		default:
			break;
    }

    for(i = 0;i < 14;i++) {
        (void)i2c_ov5640_write(dev->client, reg_data[i].reg, &reg_data[i].reg_val, 1);
    }

	return 0;
}

static void ov5640_global_init(struct ov5640_dev* dev)
{
    u8 i;
    u8 len;

    len = MY_OV5640_ARRAY_SIZE(ov5640_global_setting);
    for(i = 0;i < len;i++) {
        (void)i2c_ov5640_write(dev->client, ov5640_global_setting[i].reg,\
				&ov5640_global_setting[i].reg_val, 1);
    }
}

static void ov5640_set_mode(struct ov5640_dev* dev, struct ov5640_mode* mode)
{
    u8 i;

    for(i = 0;i < mode->reg_list_len;i++) {
        (void)i2c_ov5640_write(dev->client, mode->reg_list[i].reg, &mode->reg_list[i].reg_val, 1);
    }

	dev->current_mode = mode;
}

static int ov5640_s_stream(struct v4l2_subdev *sd, int enable)
{

	return 0;
}

static int ov5640_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *param)
{
	param->parm.capture.capability = ov5640.streamcap.capability;
	param->parm.capture.capturemode = ov5640.streamcap.capturemode;
	param->parm.capture.timeperframe.numerator = ov5640.streamcap.timeperframe.numerator;
	param->parm.capture.timeperframe.denominator = ov5640.streamcap.timeperframe.denominator;

	return 0;
}

static int ov5640_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *param)
{
	param->parm.capture.capability = ov5640.streamcap.capability;
	param->parm.capture.capturemode = ov5640.streamcap.capturemode;
	param->parm.capture.timeperframe.numerator = ov5640.streamcap.timeperframe.numerator;
	param->parm.capture.timeperframe.denominator = ov5640.streamcap.timeperframe.denominator;

	return 0;
}

static int ov5640_enum_mbus_fmt(struct v4l2_subdev *sd, unsigned int index,
	u32 *code)
{
	if(index > 0)
		return -EINVAL;

	*code = MEDIA_BUS_FMT_YUYV8_2X8;

	return 0;
}

static int ov5640_g_mbus_fmt(struct v4l2_subdev *sd,
	struct v4l2_mbus_framefmt *fmt)
{
	if(!ov5640.current_mode)
		return -EINVAL;

	fmt->width = ov5640.current_mode->width;
    fmt->height = ov5640.current_mode->height;
    fmt->code = MEDIA_BUS_FMT_YUYV8_2X8;
    fmt->field = V4L2_FIELD_NONE;

	return 0;
}

static int ov5640_s_mbus_fmt(struct v4l2_subdev *sd,
	struct v4l2_mbus_framefmt *fmt)
{
	struct ov5640_mode *mode = NULL;
	struct ov5640_mode *best_mode = NULL;
    int i;
	u32 min_diff = 3500;
	u32 diff;
    
    if (fmt->code != MEDIA_BUS_FMT_YUYV8_2X8) {
        fmt->code = MEDIA_BUS_FMT_YUYV8_2X8;
    }
    
    for (i = 0; i < OV5640_MODE_MAX; i++) {
        mode = &ov5640_support_modes[i];
    	diff = abs((int)mode->width - (int)fmt->width) +
				abs((int)mode->height - (int)fmt->height);
        
        if (diff < min_diff) {
            min_diff = diff;
            best_mode = mode;
        }
    }
    
    if (!best_mode)
        return -EINVAL;
    
	if(best_mode != ov5640.current_mode)
    	ov5640_set_mode(&ov5640, best_mode);
    
    fmt->width = best_mode->width;
    fmt->height = best_mode->height;
    fmt->field = V4L2_FIELD_NONE;

	return 0;
}

static int ov5640_enum_frame_size(struct v4l2_subdev *sd,
	struct v4l2_subdev_pad_config *cfg,
	struct v4l2_subdev_frame_size_enum *fse)
{
	if(fse->index >= OV5640_MODE_MAX)
		return -EINVAL;

	fse->min_width = ov5640_support_modes[fse->index].width;
	fse->max_width = ov5640_support_modes[fse->index].width;
	fse->min_height = ov5640_support_modes[fse->index].height;
	fse->max_height = ov5640_support_modes[fse->index].height;

	return 0;
}

static int ov5640_enum_frame_interval(struct v4l2_subdev *sd,
	struct v4l2_subdev_pad_config *cfg,
	struct v4l2_subdev_frame_interval_enum *fie)
{
	u8 i;
	u8 count = 0;

	if(fie->index >= OV5640_MODE_MAX)
		return -EINVAL;

	for(i = 0;i < OV5640_MODE_MAX;i++) {
		if((fie->width == ov5640_support_modes[i].width) &&\
				(fie->height == ov5640_support_modes[i].height)) {
			if(count == fie->index) {
				fie->interval.numerator = 1;
				fie->interval.denominator = ov5640_support_modes[i].fps;

				return 0;
			}
			count++;
		}
	}

	return -EINVAL;
}

static const struct v4l2_subdev_core_ops ov5640_subdev_core_ops;

static const struct v4l2_subdev_video_ops ov5640_subdev_video_ops = {
	.s_stream = ov5640_s_stream,
	.g_parm = ov5640_g_parm,
	.s_parm = ov5640_s_parm,
	.enum_mbus_fmt = ov5640_enum_mbus_fmt,
	.g_mbus_fmt	= ov5640_g_mbus_fmt,
	.s_mbus_fmt	= ov5640_s_mbus_fmt,
};

static const struct v4l2_subdev_pad_ops ov5640_subdev_pad_ops = {
	.enum_frame_size = ov5640_enum_frame_size,
	.enum_frame_interval = ov5640_enum_frame_interval,
};

static const struct v4l2_subdev_ops ov5640_subdev_ops = {
	.core = &ov5640_subdev_core_ops,
	.video = &ov5640_subdev_video_ops,
	.pad = &ov5640_subdev_pad_ops,
};

static int ov5640_g_volatile_ctrl(struct v4l2_ctrl *ctrl)
{

	return 0;
}

static int ov5640_s_ctrl(struct v4l2_ctrl *ctrl)
{
	int ret;

	switch (ctrl->id)
	{
	case V4L2_CID_BRIGHTNESS:
		ret = ov5640_set_brightness(&ov5640, ctrl->val);
		if(ret < 0)
			return ret;
		break;
	case V4L2_CID_CONTRAST:
		ret = ov5640_set_contrast(&ov5640, ctrl->val);
		if(ret < 0)
			return ret;
		break;
	case V4L2_CID_SATURATION:
		ov5640_set_saturation(&ov5640, ctrl->val);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static struct v4l2_ctrl_ops ov5640_ctrl_ops = {
	.g_volatile_ctrl = ov5640_g_volatile_ctrl,
	.s_ctrl = ov5640_s_ctrl,
};

static int ov5640_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct v4l2_ctrl *ctrl;
	struct reg_value reg_data[] = {
		{0x302c, 0x03}, {0x4740, 0x25},
	};
	int ret;
	u16 chip_id = 0;
    u8 data[2] = {0};
	u8 i;
	u8 len;

	ov5640.client = client;

    ov5640.reset_desc = devm_gpiod_get(&client->dev, "reset", GPIOD_OUT_LOW);
	if(IS_ERR(ov5640.reset_desc))	
		return PTR_ERR(ov5640.reset_desc);
	
    ov5640.pwdn_desc = devm_gpiod_get(&client->dev, "pwdn", GPIOD_OUT_LOW);
	if(IS_ERR(ov5640.pwdn_desc))	
		return PTR_ERR(ov5640.pwdn_desc);

    /* 拉低PWDN, 拉低RESET, 延迟1ms */
    gpiod_set_value(ov5640.reset_desc, 0);
    gpiod_set_value(ov5640.pwdn_desc, 0);
    mdelay(1);

    /* 拉高RESET, 延迟20ms */
    gpiod_set_value(ov5640.reset_desc, 1);
    mdelay(20);

	(void)i2c_ov5640_read(client, OV5640_CHIP_ID_HIGH_BYTE, data, 2);
	chip_id = (u16)(((u16)data[0] << 8) | (u16)data[1]);
	if(chip_id != OV5640_CHIP_ID) {
		dev_err(&client->dev, "read ov5640 chip id failed\n");
		return -ENODEV;
	}

	dev_info(&client->dev, "read ov5640 chip id successfully id=%#x\n", chip_id);

    data[0] = 0x11;
    (void)i2c_ov5640_write(client, OV5640_SCCB_SYSTEM_CTRL1, data, 1);
    data[0] = 0x82;
    (void)i2c_ov5640_write(client, OV5640_SYSTEM_CTROL0, data, 1);
    mdelay(5);

	ov5640.streamcap.capability = V4L2_CAP_TIMEPERFRAME;
	ov5640.streamcap.capturemode = 0;
	ov5640.streamcap.timeperframe.numerator = 1;
	ov5640.streamcap.timeperframe.denominator = 30;
    
    ov5640_global_init(&ov5640);
	ov5640_set_mode(&ov5640, &ov5640_support_modes[0]);

	len = MY_OV5640_ARRAY_SIZE(reg_data);
	for(i = 0;i < len;i++) {
		(void)i2c_ov5640_write(client, reg_data[i].reg, &reg_data[i].reg_val, 1);
	}

	ret = v4l2_ctrl_handler_init(&ov5640.ctrl_handler, 8);
	if(ret < 0) {
		dev_err(&client->dev, "failed to init ctrl handler\n");
		return ret;
	}

	ctrl = v4l2_ctrl_new_std(&ov5640.ctrl_handler, &ov5640_ctrl_ops,
			V4L2_CID_BRIGHTNESS, -128, 127, 1, 0);
	if(!ctrl) {
		dev_err(&client->dev, "failed to create brightness control\n");
		return -ENOMEM;
	}

	ctrl = v4l2_ctrl_new_std(&ov5640.ctrl_handler, &ov5640_ctrl_ops,\
			V4L2_CID_CONTRAST, 0, 255, 1, 0);
	if(!ctrl) {
		dev_err(&client->dev, "failed to create contrast control\n");
		return -ENOMEM;
	}

	ctrl = v4l2_ctrl_new_std(&ov5640.ctrl_handler, &ov5640_ctrl_ops,\
			V4L2_CID_SATURATION, 0, 255, 1, 0);
	if(!ctrl) {
		dev_err(&client->dev, "failed to create saturation control\n");
		return -ENOMEM;
	}

	ov5640.subdev.ctrl_handler = &ov5640.ctrl_handler;

	v4l2_i2c_subdev_init(&ov5640.subdev, ov5640.client, &ov5640_subdev_ops);
	ret = v4l2_async_register_subdev(&ov5640.subdev);
	if(ret < 0) {
		dev_err(&client->dev, "failed to register async subdev\n");
		return ret;
	}

    msleep(300);

	dev_info(&client->dev, "ov5640 probe\n");

    return 0;
}

static int ov5640_remove(struct i2c_client *client)
{
	v4l2_ctrl_handler_free(&ov5640.ctrl_handler);
	v4l2_async_unregister_subdev(&ov5640.subdev);
	gpiod_set_value(ov5640.reset_desc, 0);
	gpiod_set_value(ov5640.pwdn_desc, 0);

    return 0;
}

static const struct of_device_id ov5640_of_match[] = {
    {.compatible = "myboard,ov5640"},
    {},
};
MODULE_DEVICE_TABLE(of, ov5640_of_match);

static const struct i2c_device_id ov5640_id_table[] = {
    {"ov5640", 0},
    {},
};
MODULE_DEVICE_TABLE(i2c, ov5640_id_table);

static struct i2c_driver my_ov5640_i2c_driver = {
    .probe = ov5640_probe,
    .remove = ov5640_remove,
    .id_table = ov5640_id_table,
    .driver = {
        .name = "my_ov5640",
        .of_match_table = ov5640_of_match,
    },
};

module_i2c_driver(my_ov5640_i2c_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("My OV5640 I2C Driver");
