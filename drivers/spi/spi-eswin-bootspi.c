// SPDX-License-Identifier: GPL-2.0
/*
 * ESWIN BootSpi Driver
 *
 * Copyright 2024, Beijing ESWIN Computing Technology Co., Ltd.. All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * Authors: HuangYiFeng<huangyifeng@eswincomputing.com>
 */

#include <linux/clk.h>
#include <linux/reset.h>
#include <linux/bitfield.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/mod_devicetable.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/spi/spi.h>
#include <linux/spi/spi-mem.h>
#include <linux/mtd/spi-nor.h>
#include <linux/sysfs.h>
#include <linux/kobject.h>
#include <linux/pm_runtime.h>

/* Register offsets */
#define ES_SPI_CSR_00			0x00	/*WRITE_STATUS_REG_TIME*/
#define ES_SPI_CSR_01			0x04	/*SPI_BUS_MODE*/
#define ES_SPI_CSR_02			0x08	/*ERASE_COUNTER_TAP*/
#define ES_SPI_CSR_03			0x0c	/*DMA_EN_HCLK_STATUS*/
#define ES_SPI_CSR_04			0x10	/*FAST_READ_CONTROL*/
#define ES_SPI_CSR_05			0x14	/*SPI_FLASH_WR_NUM*/
#define ES_SPI_CSR_06			0x18	/*SPI_FLASH_COMMAND*/
#define ES_SPI_CSR_07			0x1c	/*INTERRUPT_CONTROL*/
#define ES_SPI_CSR_08			0x20	/*DMA_REQUEST_TAP*/
#define ES_SPI_CSR_09			0x24	/*SPI_FLASH_WR_ADDRESS*/
#define ES_SPI_CSR_10			0x28	/*PAGE_PROGRAM_TIME*/
#define ES_SPI_CSR_11			0x2c	/*SECTOR_ERASE_TIME*/
#define ES_SPI_CSR_12			0x30	/*SMALL_BLOCK_ERASE_TIME*/
#define ES_SPI_CSR_13			0x34	/*LARGE_BLOCK_ERASE_TIME*/
#define ES_SPI_CSR_14			0x38	/*CHIP_ERASE_TIME*/
#define ES_SPI_CSR_15			0x3c	/*CHIP_DESELECT_TIME*/
#define ES_SPI_CSR_16			0x40	/*POWER_DOWN_TIME*/

/* Flash opcodes. */
#define SPINOR_OP_RDSR3		0x15	/* Read status register 3 */
#define SPINOR_OP_WRSR3		0x11	/* Write status register 3 */
#define SPINOR_BLOCK_LOCK		0x36	/* Individual Block/Sector Lock */
#define SPINOR_BLOCK_UNLOCK		0x39	/* Individual Block/Sector UnLock */
#define SPINOR_GLOBAL_BLOCK_LOCK		0x7E	/* global Block/Sector UnLock */
#define SPINOR_GLOBAL_BLOCK_UNLOCK		0x98	/* global Block/Sector UnLock */

#define ES_SYSCSR_SPIMODECFG			0x340

#define ES_CONCSR_SPI_INTSEL			0x3c0

#define SPI_COMMAND_VALID				0x01
#define SPI_COMMAND_MOVE_VALUE			0x00
#define SPI_COMMAND_CODE_FIELD_POSITION 0X06
#define SPI_COMMAND_MOVE_FIELD_POSITION 0X05
#define SPI_COMMAND_TYPE_FIELD_POSITION 0X01

/* Bit fields in CTRLR0 */
/*
 * Only present when SSI_MAX_XFER_SIZE=16. This is the default, and the only
 * option before version 3.23a.
 */
#define SPI_INTSEL_MASK			GENMASK(11, 10)
#define INT_ROUTED_U84			0x0
#define INT_ROUTED_LPCPU		0x1
#define INT_ROUTED_SCPU			0x3u

#define RX_TIMEOUT			5000		/* timeout in ms */

#define SPI_COMMAND_INIT_VALUE       0XFFFFC000
#define FLASH_PAGE_SIZE      0x100

typedef enum {
	SPI_FLASH_WR_BYTE  = 1,
	SPI_FLASH_WR_2BYTE = 2,
	SPI_FLASH_WR_WORD  = 4,
} SPI_FLASH_WR_NUM_T;

typedef enum {
	SPI_FAST_READ_DEFAULT = 0,
	SPI_FAST_READ_ENABLE  = 3 /*WHEN SPI QUAD0 OR DUAL MODE*/
} SPI_FAST_READ_CTL_T;

typedef enum { STANDARD_SPI = 0, DUAL_SPI, QUAD_SPI } SPI_BUS_MODE_T;

typedef enum {
	SPIC_CMD_TYPE_SPI_PROGRAM = 0,
	SPIC_CMD_TYPE_WRITE_STATUS_REGISTER,
	SPIC_CMD_TYPE_READ_STATUS_REGISTER,
	SPIC_CMD_TYPE_SECTOR_ERASE,
	SPIC_CMD_TYPE_BLOCK_ERASE_TYPE1,
	SPIC_CMD_TYPE_BLOCK_ERASE_TYPE2,
	SPIC_CMD_TYPE_CHIP_ERASE,
	SPIC_CMD_TYPE_POWER_DOWN,
	SPIC_CMD_TYPE_RELEASE_POWER_DOWM,
	SPIC_CMD_TYPE_ENTER_OR_EXIT_32BIT_MODE,
	SPIC_CMD_TYPE_READ_SECURITY_REG,
	SPIC_CMD_TYPE_ERASE_SECURITY_REG,
	SPIC_CMD_TYPE_WRITE_SECURITY_REG,
	SPIC_CMD_TYPE_READ_DATA,
	SPIC_CMD_TYPE_READ_MANUFACTURED_ID,
	SPIC_CMD_TYPE_READ_JEDEC_ID
} SPI_FLASH_COMMAND_TYPE_T;

#define SPIC_CMD_CODE_POWER_DOWN              0xb9
#define SPIC_CMD_CODE_RELEASE_POWER_DOWN       0xab
#define SPIC_CMD_CODE_ENABLE_RESET            0x66
#define SPIC_CMD_CODE_RESET                   0x99

struct es_spi_priv {
	struct clk *cfg_clk;
	struct clk *clk;
	struct reset_control *rstc;
	struct gpio_desc *cs_gpio;	/* External chip-select gpio */
	struct gpio_desc *wp_gpio;	/* External chip-write protection gpio */

	void __iomem *regs;
	void __iomem *sys_regs;
	void __iomem *flash_base;
	unsigned int freq;		/* Default frequency */
	unsigned int mode;

	const void *tx;
	u32 opcode;
	u32 cmd_type;
	u64 addr;
	void *rx;
	u32 fifo_len;			/* depth of the FIFO buffer */
	u32 max_xfer;			/* Maximum transfer size (in bits) */

	int bits_per_word;
	int len;
	bool wp_enabled;
	u8 tmode;			/* TR/TO/RO/EEPROM */
	u8 type;			/* SPI/SSP/MicroWire */
	struct spi_controller *master;
	struct device *dev;
	int irq;
};

uint8_t eswin_bootspi_read_flash_status_register(struct es_spi_priv *priv,
		uint8_t *register_data, int flash_cmd);

static inline u32 eswin_bootspi_read(struct es_spi_priv *priv, u32 offset)
{
	return readl(priv->regs + offset);
}

static inline void eswin_bootspi_write(struct es_spi_priv *priv, u32 offset, u32 val)
{
	writel(val, priv->regs + offset);
}

static inline u32 eswin_bootspi_data_read(struct es_spi_priv *priv, u32 offset)
{
	return readl(priv->flash_base + offset);
}

static inline void eswin_bootspi_data_write(struct es_spi_priv *priv, u32 offset, u32 val)
{
	writel(val, priv->flash_base + offset);
}

static int eswin_bootspi_wait_over(struct es_spi_priv *priv)
{
	u32 val;
	struct device *dev = priv->dev;
	uint8_t register_data = 0;
	unsigned long timeout = jiffies + msecs_to_jiffies(5000);  // 5 seconds timeout

	if (readl_poll_timeout(priv->regs + ES_SPI_CSR_06, val,
		(!(val & 0x1)), 10, RX_TIMEOUT * 1000)) {
			dev_err(dev, "eswin_bootspi_wait_over : timeout in waiting contronller busy status\n");
			return -ETIMEDOUT;
	}
	while (register_data & 0x1) {
		eswin_bootspi_read_flash_status_register(priv, &register_data, SPINOR_OP_RDSR);
		// Check for timeout
		if (time_after(jiffies, timeout)) {
		    dev_err(dev, "eswin_bootspi_wait_over : timeout in wait flash chip busy status!\n");
		    return -ETIMEDOUT;
		}
	}
	return 0;
}

/**
 * @brief spi read and write cfg
 */
static void eswin_bootspi_read_write_cfg(struct es_spi_priv *priv, u32 byte, u32 addr)
{
	eswin_bootspi_write(priv, ES_SPI_CSR_09, addr);
	eswin_bootspi_write(priv, ES_SPI_CSR_05, byte);
	eswin_bootspi_write(priv, ES_SPI_CSR_04, SPI_FAST_READ_DEFAULT);
	eswin_bootspi_write(priv, ES_SPI_CSR_01, STANDARD_SPI);
}

/**
 *  @brief write data from dest address to flash
 */
static void eswin_bootspi_send_data(struct es_spi_priv *priv,
	const u8 *dest, u32 size)
{
	u32 offset = 0;
	u8 *buff = (u8 *)dest;
	u32 data = 0;
	int i;
	struct device *dev = priv->dev;

	dev_dbg(dev,"wrtie spi data\n");
	while (size >= SPI_FLASH_WR_WORD) {
		data = (buff[0]) | (buff[1] << 8) |(buff[2] << 16) | (buff[3] << 24);
		for (i = 0; i < 4; i++) {
			dev_dbg(dev,"0x%x ", buff[i]);
		}
		dev_dbg(dev,"0x%x ", data);
		eswin_bootspi_data_write(priv, offset, data);
		offset = offset + SPI_FLASH_WR_WORD;
		size = size - SPI_FLASH_WR_WORD;
		buff = buff + SPI_FLASH_WR_WORD;
	}
	data = 0;
	if (size != 0) {
		for (i = 0; i < size; i++) {
			data |=buff[i] << (8 * i);
			dev_dbg(dev,"0x%x ", buff[i]);
		}
		dev_dbg(dev,"0x%x ", data);
		eswin_bootspi_data_write(priv, offset, data);
	}
}

/**
 *  @brief Read data from flash to dest address
 */
static void eswin_bootspi_recv_data(struct es_spi_priv *priv, u8 *dest, u32 size)
{
	u32 offset = 0;
	u8 *buff = NULL;
	u32 data = 0xFFFFFFFF;
	int i;
	struct device *dev = priv->dev;

	dev_dbg(dev,"read spi data\n");
	while (size >= SPI_FLASH_WR_WORD) {
		buff = (u8 *)dest;
		data = eswin_bootspi_data_read(priv, offset);
		dev_dbg(dev,"0x%x ", data);
		for (i = 0; i < SPI_FLASH_WR_WORD; i++) {
			*buff = (u8)(data >> (8 * i));
			dev_dbg(dev,"0x%x ", *buff);
			buff++;
		}
		dest = dest + SPI_FLASH_WR_WORD;
		offset = offset + SPI_FLASH_WR_WORD;
		size = size - SPI_FLASH_WR_WORD;
	}
	if (size != 0) {
		buff = (u8 *)dest;
		data = eswin_bootspi_data_read(priv, offset);
		for (i = 0; i < size; i++) {
			*buff = (u8)(data >> (8 * i));
			dev_dbg(dev,"0x%x ", *buff);
			buff++;
		}
	}
	dev_dbg(dev,"\n");
}


/**
 * @brief spi send command
 */
static void eswin_bootspi_cmd_cfg(struct es_spi_priv *priv, u32 code, u32 type)
{
	u32 command = eswin_bootspi_read(priv, ES_SPI_CSR_06);
	struct device *dev = priv->dev;

	command &= ~((0xFF << 6) | (0x1 << 5) | (0xF << 1) | 0x1);
	command |= ((code << SPI_COMMAND_CODE_FIELD_POSITION) |
		(SPI_COMMAND_MOVE_VALUE << SPI_COMMAND_MOVE_FIELD_POSITION) |
		(type << SPI_COMMAND_TYPE_FIELD_POSITION) | SPI_COMMAND_VALID);

	eswin_bootspi_write(priv, ES_SPI_CSR_06, command);
	dev_dbg(dev, "[%s %d]: write command 0x%x, read back command 0x%x\n",
		__func__,__LINE__, command, eswin_bootspi_read(priv, ES_SPI_CSR_06));
}
/**
 * @brief  spi write flash
 * @param [in]  offset: address of flash to be write
 * @param [in]  wr_dest: Address of data to be sent
 * @param [in]  size: size of flash to be write
 */
static void eswin_bootspi_writer(struct es_spi_priv *priv)
{
	u32 write_size = 0, offset, cmd_code;
	u32 cmd_type = priv->cmd_type;
	const u8 *wr_dest = priv->tx;
	int size = priv->len;

	offset = priv->addr;
	cmd_code = priv->opcode;

	if (size == 0) {
		// if(SPIC_CMD_TYPE_SECTOR_ERASE == cmd_type)
		{
			eswin_bootspi_read_write_cfg(priv, write_size, offset);
			eswin_bootspi_cmd_cfg(priv, cmd_code, cmd_type);
			eswin_bootspi_wait_over(priv);
		}
	}
	while (size > 0) {
		write_size = size;
		if (write_size > FLASH_PAGE_SIZE) {
			write_size = FLASH_PAGE_SIZE;
		}
		eswin_bootspi_read_write_cfg(priv, write_size, offset);
		eswin_bootspi_send_data(priv, wr_dest, write_size);
		eswin_bootspi_cmd_cfg(priv, cmd_code, cmd_type);
		eswin_bootspi_wait_over(priv);
		wr_dest += write_size;
		offset += write_size;
		size = size - write_size;
	}
}

static void eswin_bootspi_reader(struct es_spi_priv *priv)
{
	int read_size = 0;
	u32 offset = priv->addr;
	u32 cmd_code = priv->opcode;
	u32 cmd_type = priv->cmd_type;
	u8 *mem_dest = priv->rx;
	int size = priv->len;

	while (size > 0) {
		read_size = size;
		if (read_size > FLASH_PAGE_SIZE) {
			read_size = FLASH_PAGE_SIZE;
		}

		eswin_bootspi_read_write_cfg(priv, read_size, offset);
		eswin_bootspi_cmd_cfg(priv, cmd_code, cmd_type);
		eswin_bootspi_wait_over(priv);
		eswin_bootspi_recv_data(priv, mem_dest, read_size);
		mem_dest += read_size;
		offset += read_size;
		size = size - read_size;
	}
}

/*
 * We define external_cs_manage function as 'weak' as some targets
 * (like MSCC Ocelot) don't control the external CS pin using a GPIO
 * controller. These SoCs use specific registers to control by
 * software the SPI pins (and especially the CS).
 */

static void external_cs_manage(struct es_spi_priv *priv, bool on)
{
	gpiod_set_value(priv->cs_gpio, on ? 1 : 0);
}

/* The size of ctrl1 limits data transfers to 64K */
static int eswin_bootspi_adjust_op_size(struct spi_mem *mem,
					struct spi_mem_op *op)
{
	op->data.nbytes = min(op->data.nbytes, (unsigned int)SZ_64K);

	return 0;
}

/*
 * The controller only supports Standard SPI mode, Duall mode and
 * Quad mode. Double sanitize the ops here to avoid OOB access.
 */
static bool eswin_bootspi_supports_op(struct spi_mem *mem,
				      const struct spi_mem_op *op)
{
	return spi_mem_default_supports_op(mem, op);
}

uint8_t eswin_bootspi_read_flash_status_register(struct es_spi_priv *priv,
		uint8_t *register_data, int flash_cmd)
{
	u32 command;
	struct device *dev = priv->dev;

	memset(register_data, 0, sizeof(uint8_t));
	//Flash status register-2 is 1byte
	eswin_bootspi_read_write_cfg(priv, 1, 0);

	//Set SPI_FLASH_COMMAND
	command = eswin_bootspi_read(priv, ES_SPI_CSR_06);
	command &= ~((0xFF << 6) | (0x1 << 5) | (0xF << 1) | 0x1);
	command |= ((flash_cmd << SPI_COMMAND_CODE_FIELD_POSITION) |
			(SPI_COMMAND_MOVE_VALUE << SPI_COMMAND_MOVE_FIELD_POSITION) |
			(SPIC_CMD_TYPE_READ_STATUS_REGISTER << SPI_COMMAND_TYPE_FIELD_POSITION) | SPI_COMMAND_VALID);
	eswin_bootspi_write(priv, ES_SPI_CSR_06, command);

	//Wait command finish
	mdelay(10);

	//Read back data
	eswin_bootspi_recv_data(priv, register_data, 1);
	dev_dbg(dev, "[%s %d]: command 0x%x, status register_data 0x%x\n",__func__,__LINE__,
		command, *register_data);
	return 0;
}

static uint8_t eswin_bootspi_write_flash_status_register(struct es_spi_priv *priv,
		uint8_t register_data, int flash_cmd)
{
	u32 command;
	struct device *dev = priv->dev;

	//Flash status register-2 is 1byte
	eswin_bootspi_read_write_cfg(priv, 1, 0);
	eswin_bootspi_send_data(priv, &register_data, 1);

	command = eswin_bootspi_read(priv, ES_SPI_CSR_06);
	command &= ~((0xFF << 6) | (0x1 << 5) | (0xF << 1) | 0x1);
	command |= ((flash_cmd << SPI_COMMAND_CODE_FIELD_POSITION) |
			(SPI_COMMAND_MOVE_VALUE << SPI_COMMAND_MOVE_FIELD_POSITION) |
			(SPIC_CMD_TYPE_WRITE_STATUS_REGISTER << SPI_COMMAND_TYPE_FIELD_POSITION) | SPI_COMMAND_VALID);
	eswin_bootspi_write(priv, ES_SPI_CSR_06, command);

	//Wait command finish
	eswin_bootspi_wait_over(priv);
	dev_dbg(dev,"[%s %d]: command 0x%x, status register_data 0x%x\n",__func__,__LINE__,
			command, register_data);
	return 0;
}

static void eswin_bootspi_write_flash_global_block_lock_register(
	struct es_spi_priv *priv, int flash_cmd)
{
	u32 command;

	//Flash global block lock register not need data
	eswin_bootspi_read_write_cfg(priv, 1, 0);

	command = eswin_bootspi_read(priv, ES_SPI_CSR_06);
	command &= ~((0xFF << 6) | (0x1 << 5) | (0xF << 1) | 0x1);
	command |= ((flash_cmd << SPI_COMMAND_CODE_FIELD_POSITION) |
			(SPI_COMMAND_MOVE_VALUE << SPI_COMMAND_MOVE_FIELD_POSITION) |
			(SPIC_CMD_TYPE_CHIP_ERASE << SPI_COMMAND_TYPE_FIELD_POSITION) | SPI_COMMAND_VALID);
	eswin_bootspi_write(priv, ES_SPI_CSR_06, command);

	//Wait command finish
	eswin_bootspi_wait_over(priv);
	//printf("[%s %d]: command 0x%x\n",__func__,__LINE__, command);
}

static int eswin_bootspi_flash_global_wp_cfg(struct es_spi_priv *priv, int enable)
{
	uint8_t register_data, request_register_data;

	external_cs_manage(priv, false);

	//Update status register1
	eswin_bootspi_read_flash_status_register(priv, &register_data, SPINOR_OP_RDSR);
	request_register_data = register_data;
		/*
			  SRP SEC TB BP2 BP1 BP0 WEL BUSY
	 	*/
	request_register_data |= (1 << 5);  //TB 1, bottom
	request_register_data &= ~(1 << 6);  // SEC 0, 64K
	if (request_register_data != register_data) {
		eswin_bootspi_write_flash_status_register(priv, request_register_data, SPINOR_OP_WRSR);
	}

	//Update status register3
	eswin_bootspi_read_flash_status_register(priv, &register_data, SPINOR_OP_RDSR3);
	request_register_data = register_data;
		/*
			  R DRV1 DRV0 R R WPS R R
	 	*/
	request_register_data |= (1 << 2);   //WPS 1, individual block
	if (request_register_data != register_data) {
		eswin_bootspi_write_flash_status_register(priv, request_register_data, SPINOR_OP_WRSR3);
	}

	//Update global lock/unlock register
	if (enable) {
		eswin_bootspi_write_flash_global_block_lock_register(priv, SPINOR_GLOBAL_BLOCK_LOCK);
	} else {
		eswin_bootspi_write_flash_global_block_lock_register(priv, SPINOR_GLOBAL_BLOCK_UNLOCK);
	}

	external_cs_manage(priv, true);
	return 0;
}

/*
	0: disable write_protection
	1: enable write_protection
*/
static void eswin_bootspi_wp_cfg(struct es_spi_priv *priv, int enable)
{
	struct device *dev = priv->dev;

	dev_info(dev, "Boot spi flash write protection %s\n", enable ? "enabled" : "disabled");
	pm_runtime_get_sync(dev);
	if (enable) {
		eswin_bootspi_flash_global_wp_cfg(priv, enable);
		gpiod_set_value(priv->wp_gpio, enable); //gpio output low, enable protection
	} else {
		gpiod_set_value(priv->wp_gpio, enable); //gpio output high, disable protection
		eswin_bootspi_flash_global_wp_cfg(priv, enable);
	}
	pm_runtime_put_sync(dev);
}

static ssize_t wp_enable_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct es_spi_priv *priv = dev_get_drvdata(dev);
	return sprintf(buf, "%s\n", priv->wp_enabled ? "enabled" : "disabled");
}

static ssize_t wp_enable_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct es_spi_priv *priv = dev_get_drvdata(dev);
	unsigned long enable;
	int ret;

	ret = kstrtoul(buf, 10, &enable);
	if (ret)
		return ret;

	eswin_bootspi_wp_cfg(priv, enable);
	priv->wp_enabled = enable;
	return count;
}

static DEVICE_ATTR_RW(wp_enable);

#define ALIGNMENT_SIZE 0x10000    // 64KB alignment
static void eswin_bootspi_write_flash_individual_block_lock_register(
	struct es_spi_priv *priv, u32 addr, int flash_cmd)
{
	u32 command;

	eswin_bootspi_read_write_cfg(priv, 3, addr);

	command = eswin_bootspi_read(priv, ES_SPI_CSR_06);
	command &= ~((0xFF << 6) | (0x1 << 5) | (0xF << 1) | 0x1);
	command |= ((flash_cmd << SPI_COMMAND_CODE_FIELD_POSITION) |
			(SPI_COMMAND_MOVE_VALUE << SPI_COMMAND_MOVE_FIELD_POSITION) |
			(SPIC_CMD_TYPE_BLOCK_ERASE_TYPE2 << SPI_COMMAND_TYPE_FIELD_POSITION) | SPI_COMMAND_VALID);

	eswin_bootspi_write(priv, ES_SPI_CSR_06, command);

	//Wait command finish
	eswin_bootspi_wait_over(priv);

	//printk("[%s %d]: command 0x%x, addr 0x%x, flash_cmd 0x%x\n",__func__,__LINE__, command, addr, flash_cmd);
}

static int eswin_bootspi_region_wp_cfg(struct es_spi_priv *priv, u32 addr, u32 size, int enable)
{
	struct device *dev = priv->dev;
	uint32_t i;
	int flash_cmd;

	dev_info(dev, "Region write protection %s: addr=0x%x, size=0x%x\n",
	         enable ? "enabled" : "disabled", addr, size);

	pm_runtime_get_sync(dev);

	flash_cmd = enable ? SPINOR_BLOCK_LOCK : SPINOR_BLOCK_UNLOCK;

	external_cs_manage(priv, false);

	for (i = 0; i < size; i += ALIGNMENT_SIZE) {
		eswin_bootspi_write_flash_individual_block_lock_register(priv, addr + i, flash_cmd);
	}
	external_cs_manage(priv, true);

	pm_runtime_put_sync(dev);
	return 0;
}

static ssize_t region_wp_enable_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	struct es_spi_priv *priv = dev_get_drvdata(dev);
	u32 addr, size;
	unsigned long enable;
	int ret;

	// Parsing the input buffer
	ret = sscanf(buf, "%x %x %lu", &addr, &size, &enable);
	if (ret != 3) {
		dev_err(dev, "invalid format %s\n", buf);
		return -EINVAL;
	}

	// Check if the address is 64KB aligned
	if ((uintptr_t)addr % ALIGNMENT_SIZE != 0) {
		dev_err(dev, "addr 0x%x is not aligned to 64KB\n", addr);
		return -EINVAL;
	}

	// Check if the size is 64KB aligned
	if (size % ALIGNMENT_SIZE != 0) {
		dev_err(dev, "size 0x%x is not aligned to 64KB\n", size);
		return -EINVAL;
	}

	// Call the region write protection configuration function
	eswin_bootspi_region_wp_cfg(priv, addr, size, enable);

	return count;
}

static ssize_t region_wp_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	// This is a placeholder. You may want to display the status of region protection
	// For simplicity, we'll return a static message for now.
	return sprintf(buf, "Use echo <addr> <size> <enable> > region_wp_enable to configure region WP\n");
}

static DEVICE_ATTR_RW(region_wp_enable);

static int eswin_bootspi_exec_op(struct spi_mem *mem,
				 const struct spi_mem_op *op)
{
	bool read = op->data.dir == SPI_MEM_DATA_IN;
	int ret = 0;
	struct es_spi_priv *priv = spi_controller_get_devdata(mem->spi->controller);
	struct device *dev = priv->dev;

	priv->addr = op->addr.val;
	priv->opcode = op->cmd.opcode;

	dev_dbg(dev, "\n[%s %d]: addr=0x%llx opcode=0x%x\n", __func__,__LINE__,
		priv->addr, priv->opcode);

	if ( priv->opcode == SPINOR_OP_WREN
		|| priv->opcode == SPINOR_OP_WRDI)
		return 0;

	switch(priv->opcode) {
		case SPINOR_OP_RDID:
		case SPINOR_OP_RDSFDP:
			priv->cmd_type = SPIC_CMD_TYPE_READ_JEDEC_ID;
			break;
		case SPINOR_OP_BE_4K:
		case SPINOR_OP_BE_4K_PMC:
			priv->opcode = SPINOR_OP_BE_4K;
			priv->cmd_type = SPIC_CMD_TYPE_SECTOR_ERASE;
			break;
		case SPINOR_OP_BE_32K:
			priv->cmd_type = SPIC_CMD_TYPE_BLOCK_ERASE_TYPE1;
			break;
		case SPINOR_OP_SE:
			priv->cmd_type = SPIC_CMD_TYPE_BLOCK_ERASE_TYPE2;
			break;
		case SPINOR_OP_CHIP_ERASE:
			priv->cmd_type = SPIC_CMD_TYPE_CHIP_ERASE;
			break;
		case SPINOR_OP_PP:
		case SPINOR_OP_PP_1_1_4:
		case SPINOR_OP_PP_1_4_4:
		case SPINOR_OP_PP_1_1_8:
		case SPINOR_OP_PP_1_8_8:
			priv->opcode = SPINOR_OP_PP;
			priv->cmd_type = SPIC_CMD_TYPE_SPI_PROGRAM;
			break;
		case SPINOR_OP_READ:
		case SPINOR_OP_READ_FAST:
		case SPINOR_OP_READ_1_1_2:
		case SPINOR_OP_READ_1_2_2:
		case SPINOR_OP_READ_1_1_4:
		case SPINOR_OP_READ_1_4_4:
		case SPINOR_OP_READ_1_1_8:
		case SPINOR_OP_READ_1_8_8:
			priv->opcode = SPINOR_OP_READ;
			priv->cmd_type = SPIC_CMD_TYPE_READ_DATA;
			break;
		case SPINOR_OP_RDSR:
		case SPINOR_OP_RDSR2:
			priv->cmd_type = SPIC_CMD_TYPE_READ_STATUS_REGISTER;
			break;
		case SPINOR_OP_WRSR:
		case SPINOR_OP_WRSR2:
			priv->cmd_type = SPIC_CMD_TYPE_WRITE_STATUS_REGISTER;
			break;
		case SPIC_CMD_CODE_POWER_DOWN:
			priv->cmd_type = SPIC_CMD_TYPE_POWER_DOWN;
			break;
		case SPIC_CMD_CODE_RELEASE_POWER_DOWN:
			priv->cmd_type = SPIC_CMD_TYPE_RELEASE_POWER_DOWM;
			break;
		case SPIC_CMD_CODE_ENABLE_RESET:
		case SPIC_CMD_CODE_RESET:
			priv->cmd_type = SPIC_CMD_TYPE_SPI_PROGRAM;
			break;
		default:
			dev_warn(dev, "[%s %d]: unsupport opcode = 0x%x, return sucess directly!\n",
				__func__,__LINE__, priv->opcode);
			return 0;
	}

	dev_dbg(dev, "[%s %d]: data direction=%d, opcode = 0x%x, cmd_type 0x%x\n",
		__func__,__LINE__, op->data.dir, priv->opcode, priv->cmd_type);
/*
	if (priv->wp_enabled) {
		switch(priv->opcode) {
			case SPINOR_OP_BE_4K:
			case SPINOR_OP_BE_4K_PMC:
			case SPINOR_OP_BE_32K:
			case SPINOR_OP_SE:
			case SPINOR_OP_CHIP_ERASE:
			case SPINOR_OP_PP:
			case SPINOR_OP_PP_1_1_4:
			case SPINOR_OP_PP_1_4_4:
			case SPINOR_OP_PP_1_1_8:
			case SPINOR_OP_PP_1_8_8:
				dev_warn_ratelimited(dev, "Write protection is enabled, do not have permission to "
					"perform this operation(%d)!\n", priv->opcode);
				return -EACCES;
		}
	}
*/
	external_cs_manage(priv, false);

	if (read) {
		priv->rx = op->data.buf.in;
		priv->len = op->data.nbytes;
		dev_dbg(dev, "[%s %d]: read len = %u\n", __func__,__LINE__, op->data.nbytes);
		eswin_bootspi_reader(priv);
	} else {
		priv->tx = op->data.buf.out;
		priv->len = op->data.nbytes;
		/* Fill up the write fifo before starting the transfer */
		dev_dbg(dev, "[%s %d]: write len = 0x%x  tx_addr 0x%px\n", __func__,__LINE__,
			op->data.nbytes, priv->tx);
		eswin_bootspi_writer(priv);
		if (eswin_bootspi_wait_over(priv) < 0) {
			dev_err(dev, "eswin_bootspi_wait_over ETIMEDOUT\n");
			ret = -ETIMEDOUT;
		}
	}
	external_cs_manage(priv, true);
	dev_dbg(dev, "%u bytes xfered\n", op->data.nbytes);
	return ret;
}

static const struct spi_controller_mem_ops eswin_bootspi_mem_ops = {
	.adjust_op_size = eswin_bootspi_adjust_op_size,
	.supports_op = eswin_bootspi_supports_op,
	.exec_op = eswin_bootspi_exec_op,
};

static int eswin_bootspi_setup(struct spi_device *spi)
{
	struct es_spi_priv *priv = spi_controller_get_devdata(spi->controller);

	reset_control_deassert(priv->rstc);
	/*
	  When the bootrom starts from the boot SPI flash, the BootSpi contronller is in boot mode.
	  If the BootSpi contronller is switched to CPU mode here, the bootrom software will crash.
	  Therefore, this part of the code should be disabled.
	  In summary, the boot mode and CPU mode of the BootSpi contronller cannot coexist.
	*/
#if 0
	/* switch bootspi to cpu mode*/
	vaule = readl(priv->sys_regs + ES_SYSCSR_SPIMODECFG);
	vaule |= 0x1;
	writel(vaule, priv->sys_regs + ES_SYSCSR_SPIMODECFG);
#endif
	/* Basic HW init */
	eswin_bootspi_write(priv, ES_SPI_CSR_08, 0x0);
	return 0;
}

static int eswin_bootspi_probe(struct platform_device *pdev)
{
	struct es_spi_priv *priv;
	struct spi_controller *master;
	int ret = 0;
	struct device *dev = &pdev->dev;

	master = devm_spi_alloc_master(&pdev->dev, sizeof(*priv));
	if (!master)
		return -ENOMEM;

	master->mode_bits = SPI_CPOL | SPI_CPHA;
	master->flags = SPI_CONTROLLER_HALF_DUPLEX;
	master->setup = eswin_bootspi_setup;
	master->dev.of_node = pdev->dev.of_node;
	master->bits_per_word_mask = SPI_BPW_MASK(32) | SPI_BPW_MASK(16) |
				     SPI_BPW_MASK(8);
	master->mem_ops = &eswin_bootspi_mem_ops;
	master->num_chipselect = 1;
	master->auto_runtime_pm = true;

	priv = spi_controller_get_devdata(master);
	priv->master = master;
	priv->dev = &pdev->dev;
	platform_set_drvdata(pdev, priv);

	priv->regs = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(priv->regs)) {
		dev_err(dev, "%s %d: failed to map registers\n", __func__,__LINE__);
		return PTR_ERR(priv->regs);
	}

	priv->sys_regs = devm_platform_ioremap_resource(pdev, 1);
	if (IS_ERR(priv->sys_regs)) {
		dev_err(dev,"%s %d: failed to map sys registers\n", __func__, __LINE__);
		return PTR_ERR(priv->sys_regs);
	}

	priv->flash_base = devm_platform_ioremap_resource(pdev, 2);
	if (IS_ERR(priv->flash_base)) {
		dev_err(dev,"%s %d: failed to map sys registers\n", __func__, __LINE__);
		return PTR_ERR(priv->flash_base);
	}

	priv->cfg_clk = devm_clk_get(dev, "cfg_clk");
	if (IS_ERR(priv->cfg_clk)) {
		dev_err(dev, "%s %d:could not get cfg clk: %ld\n", __func__,__LINE__, PTR_ERR(priv->cfg_clk));
		return PTR_ERR(priv->cfg_clk);
	}
	priv->clk = devm_clk_get(dev, "clk");
	if (IS_ERR(priv->clk)) {
		dev_err(dev, "%s %d:could not get clk: %ld\n",__func__,__LINE__, PTR_ERR(priv->rstc));
		return PTR_ERR(priv->clk);
	}
	priv->rstc = devm_reset_control_get_optional_exclusive(dev, "rst");
	if (IS_ERR(priv->rstc)) {
		dev_err(dev, "%s %d:could not get rst: %ld\n", __func__,__LINE__, PTR_ERR(priv->rstc));
		return PTR_ERR(priv->rstc);
	}

	priv->cs_gpio = devm_gpiod_get(dev, "cs", GPIOD_OUT_LOW);
	if (IS_ERR(priv->cs_gpio)) {
		dev_err(dev, "%s %d: couldn't request gpio! (error %ld)\n", __func__,__LINE__,
			PTR_ERR(priv->cs_gpio));
		return PTR_ERR(priv->cs_gpio);
	}

	priv->wp_gpio = devm_gpiod_get(dev, "wp", GPIOD_OUT_HIGH);
	if (IS_ERR(priv->cs_gpio)) {
		dev_err(dev, "%s %d: couldn't request gpio! (error %ld)\n", __func__,__LINE__,
			PTR_ERR(priv->cs_gpio));
		return PTR_ERR(priv->cs_gpio);
	}
	priv->wp_enabled = 1;

	priv->max_xfer = 32;
	/* Currently only bits_per_word == 8 supported */
	priv->bits_per_word = 8;
	priv->tmode = 0; /* Tx & Rx */

	if (!priv->fifo_len) {
		priv->fifo_len = 256;
	}
	// Create sysfs node
	ret = device_create_file(&pdev->dev, &dev_attr_wp_enable);
	if (ret) {
		dev_err(&pdev->dev, "Failed to create wp_enable attribute ret %d\n", ret);
		return ret;
	}

	// Register the new region_wp_enable attribute
	ret = device_create_file(&pdev->dev, &dev_attr_region_wp_enable);
	if (ret) {
		device_remove_file(&pdev->dev, &dev_attr_wp_enable);
		return ret;
	}

	ret = clk_prepare_enable(priv->cfg_clk);
	if (ret) {
		dev_err(dev, "could not enable cfg clock: %d\n", ret);
		return ret;
	}

	ret = clk_prepare_enable(priv->clk);
	if (ret) {
		dev_err(dev, "could not enable clock: %d\n", ret);
		goto out_cfg_clk;
	}

	/* set rate to 50M*/
	ret = clk_set_rate(priv->clk, 50000000);
	if (ret) {
		dev_err(dev, "could not enable clock: %d\n", ret);
		goto out_clk;
	}
	pm_runtime_set_autosuspend_delay(dev, 2000);
	pm_runtime_use_autosuspend(dev);
	pm_runtime_get_noresume(dev);
	pm_runtime_set_active(dev);
	pm_runtime_enable(dev);

	ret = devm_spi_register_controller(dev, master);
	if (ret)
		goto out_register_controller;

	eswin_bootspi_wp_cfg(priv, 1);

	pm_runtime_mark_last_busy(dev);
	pm_runtime_put_autosuspend(dev);

	dev_info(&pdev->dev, "ssi_max_xfer_size %d, fifo_len %d, %s mode.\n",
			priv->max_xfer, priv->fifo_len, priv->irq ? "irq" : "polling");
	return 0;

out_register_controller:
	pm_runtime_dont_use_autosuspend(dev);
	pm_runtime_set_suspended(dev);
	pm_runtime_disable(dev);
out_clk:
	clk_disable_unprepare(priv->clk);
out_cfg_clk:
	clk_disable_unprepare(priv->cfg_clk);
	return ret;
}

static void eswin_bootspi_remove(struct platform_device *pdev)
{
	struct es_spi_priv *priv = platform_get_drvdata(pdev);

	pm_runtime_dont_use_autosuspend(&pdev->dev);
	pm_runtime_set_suspended(&pdev->dev);
	pm_runtime_disable(&pdev->dev);
	clk_disable_unprepare(priv->cfg_clk);
	clk_disable_unprepare(priv->clk);
}

static int __maybe_unused eswin_bootspi_runtime_resume(struct device *dev)
{
	struct es_spi_priv *priv = dev_get_drvdata(dev);
	int ret;

	ret = clk_prepare_enable(priv->cfg_clk);
	if (ret)
		return ret;

	ret = clk_prepare_enable(priv->clk);
	if (ret) {
		clk_disable_unprepare(priv->cfg_clk);
		return ret;
	}
	return 0;
}

static int __maybe_unused eswin_bootspi_runtime_suspend(struct device *dev)
{
	struct es_spi_priv *priv = dev_get_drvdata(dev);

	clk_disable_unprepare(priv->clk);
	clk_disable_unprepare(priv->cfg_clk);
	return 0;
}

static int __maybe_unused eswin_bootspi_suspend(struct device *dev)
{

	int ret;
	struct es_spi_priv *priv = dev_get_drvdata(dev);
	struct spi_controller *master = priv->master;

	ret = spi_controller_suspend(master);
	if (ret)
		return ret;

	if (!pm_runtime_suspended(dev)) {
		clk_disable_unprepare(priv->clk);
		clk_disable_unprepare(priv->cfg_clk);
	}
	return 0;
}

static int __maybe_unused eswin_bootspi_resume(struct device *dev)
{
	int ret;
	struct es_spi_priv *priv = dev_get_drvdata(dev);
	struct spi_controller *master = priv->master;

	if (!pm_runtime_suspended(dev)) {
		ret = clk_prepare_enable(priv->cfg_clk);
		if (ret < 0) {
			dev_err(dev, "failed to enable cfg_clk (%d)\n", ret);
			return ret;
		}

		ret = clk_prepare_enable(priv->clk);
		if (ret < 0) {
			dev_err(dev, "failed to enable clk (%d)\n", ret);
			clk_disable_unprepare(priv->cfg_clk);
			return ret;
		}
	}
	ret = spi_controller_resume(master);
	if (ret < 0) {
		clk_disable_unprepare(priv->cfg_clk);
		clk_disable_unprepare(priv->cfg_clk);
	}
	return ret;
}

static const struct dev_pm_ops eswin_bootspi_pm = {
	SET_RUNTIME_PM_OPS(eswin_bootspi_runtime_suspend,
				eswin_bootspi_runtime_resume, NULL)
	SET_SYSTEM_SLEEP_PM_OPS(eswin_bootspi_suspend, eswin_bootspi_resume)
};

static const struct of_device_id eswin_bootspi_of_match[] = {
	{ .compatible = "eswin,bootspi", .data = NULL},
	{ /* end of table */}
};
MODULE_DEVICE_TABLE(of, eswin_bootspi_of_match);

#ifdef CONFIG_ACPI
static const struct acpi_device_id eswin_bootspi_acpi_match[] = {
	{"eswin,bootspi", 0},
	{}
};
MODULE_DEVICE_TABLE(acpi, eswin_bootspi_acpi_match);
#endif

static struct platform_driver eswin_bootspi_driver = {
	.probe		= eswin_bootspi_probe,
	.remove		= eswin_bootspi_remove,
	.driver		= {
		.name	= "eswin-bootspi",
		.of_match_table = eswin_bootspi_of_match,
#ifdef CONFIG_ACPI
		.acpi_match_table = eswin_bootspi_acpi_match,
#endif
		.pm = &eswin_bootspi_pm,
	},
};
module_platform_driver(eswin_bootspi_driver);

MODULE_AUTHOR("Huangyifeng <huangyifeng@eswincomputing.com>");
MODULE_DESCRIPTION("Eswin Boot SPI Controller Driver for EIC770X SoCs");
MODULE_LICENSE("GPL v2");
