// SPDX-License-Identifier: GPL-2.0+
/*
 * hwmon driver for Aquacomputer Quadro fan controller
 *
 * The Quadro sends HID reports (with ID 0x01) every second to report sensor values
 * (temperatures, fan speeds, voltage, current and power). It responds to
 * Get_Report requests, but returns a dummy value of no use.
 *
 * Copyright 2021 Leonard Anderweit <leonard.anderweit@gmail.com>
 */

#include <asm/unaligned.h>
#include <linux/debugfs.h>
#include <linux/hid.h>
#include <linux/hwmon.h>
#include <linux/jiffies.h>
#include <linux/module.h>
#include <linux/seq_file.h>

#define DRIVER_NAME			"aquacomputer-quadro"

#define QUADRO_STATUS_REPORT_ID	0x01
#define QUADRO_STATUS_UPDATE_INTERVAL	(2 * HZ) /* In seconds */

/* Register offsets for the Quadro */

#define QUADRO_SERIAL_FIRST_PART	3
#define QUADRO_SERIAL_SECOND_PART	5
#define QUADRO_FIRMWARE_VERSION	13
#define QUADRO_POWER_CYCLES		24

#define QUADRO_TEMP1			52
#define QUADRO_TEMP2			54
#define QUADRO_TEMP3			56
#define QUADRO_TEMP4			58

#define QUADRO_FLOW_SPEED		110
#define QUADRO_FAN1_SPEED		120
#define QUADRO_FAN2_SPEED		133
#define QUADRO_FAN3_SPEED		146
#define QUADRO_FAN4_SPEED		159
		
#define QUADRO_FAN1_POWER		118
#define QUADRO_FAN2_POWER		131
#define QUADRO_FAN3_POWER		144
#define QUADRO_FAN4_POWER		157

#define QUADRO_VOLTAGE			108
#define QUADRO_FAN1_VOLTAGE		114
#define QUADRO_FAN2_VOLTAGE		127
#define QUADRO_FAN3_VOLTAGE		140
#define QUADRO_FAN4_VOLTAGE		153

#define QUADRO_FAN1_CURRENT		116
#define QUADRO_FAN2_CURRENT		129
#define QUADRO_FAN3_CURRENT		142
#define QUADRO_FAN4_CURRENT		155

/* Labels for provided values */

#define L_TEMP1				"Temp1"
#define L_TEMP2				"Temp2"
#define L_TEMP3				"Temp3"
#define L_TEMP4				"Temp4"

#define L_FLOW_SPEED		"Flow speed [l/h]"
#define L_FAN1_SPEED		"Fan1 speed"
#define L_FAN2_SPEED		"Fan2 speed"
#define L_FAN3_SPEED		"Fan3 speed"
#define L_FAN4_SPEED		"Fan4 speed"

#define L_FAN1_POWER		"Fan1 power"
#define L_FAN2_POWER		"Fan2 power"
#define L_FAN3_POWER		"Fan3 power"
#define L_FAN4_POWER		"Fan4 power"

#define L_QUADRO_VOLTAGE	"VCC"
#define L_FAN1_VOLTAGE		"Fan1 voltage"
#define L_FAN2_VOLTAGE		"Fan2 voltage"
#define L_FAN3_VOLTAGE		"Fan3 voltage"
#define L_FAN4_VOLTAGE		"Fan4 voltage"

#define L_FAN1_CURRENT		"Fan1 current"
#define L_FAN2_CURRENT		"Fan2 current"
#define L_FAN3_CURRENT		"Fan3 current"
#define L_FAN4_CURRENT		"Fan4 current"

static const char *const label_temps[] = {
	L_TEMP1,
	L_TEMP2,
	L_TEMP3,
	L_TEMP4,
};

static const char *const label_speeds[] = {
	L_FLOW_SPEED,
	L_FAN1_SPEED,
	L_FAN2_SPEED,
	L_FAN3_SPEED,
	L_FAN4_SPEED,
};

static const char *const label_power[] = {
	L_FAN1_POWER,
	L_FAN2_POWER,
	L_FAN3_POWER,
	L_FAN4_POWER,
};

static const char *const label_voltages[] = {
	L_QUADRO_VOLTAGE,
	L_FAN1_VOLTAGE,
	L_FAN2_VOLTAGE,
	L_FAN3_VOLTAGE,
	L_FAN4_VOLTAGE,
};

static const char *const label_current[] = {
	L_FAN1_CURRENT,
	L_FAN2_CURRENT,
	L_FAN3_CURRENT,
	L_FAN4_CURRENT,
};

struct quadro_data {
	struct hid_device *hdev;
	struct device *hwmon_dev;
	struct dentry *debugfs;
	s32 temp_input[4];
	u16 speed_input[5];
	u32 power_input[4];
	u16 voltage_input[5];
	u16 current_input[4];
	u32 serial_number[2];
	u16 firmware_version;
	u32 power_cycles; /* How many times the device was powered on */
	unsigned long updated;
};

static umode_t quadro_is_visible(const void *data, enum hwmon_sensor_types type, u32 attr,
				 int channel)
{
	return 0444;
}

static int quadro_read(struct device *dev, enum hwmon_sensor_types type, u32 attr, int channel,
		       long *val)
{
	struct quadro_data *priv = dev_get_drvdata(dev);

	if (time_after(jiffies, priv->updated + QUADRO_STATUS_UPDATE_INTERVAL))
		return -ENODATA;

	switch (type) {
	case hwmon_temp:
		*val = priv->temp_input[channel];
		break;
	case hwmon_fan:
		*val = priv->speed_input[channel];
		break;
	case hwmon_power:
		*val = priv->power_input[channel];
		break;
	case hwmon_in:
		*val = priv->voltage_input[channel];
		break;
	case hwmon_curr:
		*val = priv->current_input[channel];
		break;
	default:
		return -EOPNOTSUPP;
	}

	return 0;
}

static int quadro_read_string(struct device *dev, enum hwmon_sensor_types type, u32 attr,
			      int channel, const char **str)
{
	switch (type) {
	case hwmon_temp:
		*str = label_temps[channel];
		break;
	case hwmon_fan:
		*str = label_speeds[channel];
		break;
	case hwmon_power:
		*str = label_power[channel];
		break;
	case hwmon_in:
		*str = label_voltages[channel];
		break;
	case hwmon_curr:
		*str = label_current[channel];
		break;
	default:
		return -EOPNOTSUPP;
	}

	return 0;
}

static const struct hwmon_ops quadro_hwmon_ops = {
	.is_visible = quadro_is_visible,
	.read = quadro_read,
	.read_string = quadro_read_string,
};

static const struct hwmon_channel_info *quadro_info[] = {
	HWMON_CHANNEL_INFO(temp, HWMON_T_INPUT | HWMON_T_LABEL, HWMON_T_INPUT | HWMON_T_LABEL,
	 			HWMON_T_INPUT | HWMON_T_LABEL, HWMON_T_INPUT | HWMON_T_LABEL),
	HWMON_CHANNEL_INFO(fan, HWMON_F_INPUT | HWMON_F_LABEL, HWMON_F_INPUT | HWMON_F_LABEL,
				HWMON_F_INPUT | HWMON_F_LABEL, HWMON_F_INPUT | HWMON_F_LABEL, HWMON_F_INPUT | HWMON_F_LABEL),
	HWMON_CHANNEL_INFO(power, HWMON_P_INPUT | HWMON_P_LABEL, HWMON_P_INPUT | HWMON_P_LABEL,
				HWMON_P_INPUT | HWMON_P_LABEL, HWMON_P_INPUT | HWMON_P_LABEL),
	HWMON_CHANNEL_INFO(in, HWMON_I_INPUT | HWMON_I_LABEL, HWMON_I_INPUT | HWMON_I_LABEL,
			   	HWMON_I_INPUT | HWMON_I_LABEL, HWMON_I_INPUT | HWMON_I_LABEL, HWMON_I_INPUT | HWMON_I_LABEL),
	HWMON_CHANNEL_INFO(curr, HWMON_C_INPUT | HWMON_C_LABEL, HWMON_C_INPUT | HWMON_C_LABEL,
				HWMON_C_INPUT | HWMON_C_LABEL, HWMON_C_INPUT | HWMON_C_LABEL),
	NULL
};

static const struct hwmon_chip_info quadro_chip_info = {
	.ops = &quadro_hwmon_ops,
	.info = quadro_info,
};

static int quadro_raw_event(struct hid_device *hdev, struct hid_report *report, u8 *data, int size)
{
	struct quadro_data *priv;

	if (report->id != QUADRO_STATUS_REPORT_ID)
		return 0;

	priv = hid_get_drvdata(hdev);

	/* Info provided with every report */

	priv->serial_number[0] = get_unaligned_be16(data + QUADRO_SERIAL_FIRST_PART);
	priv->serial_number[1] = get_unaligned_be16(data + QUADRO_SERIAL_SECOND_PART);

	priv->firmware_version = get_unaligned_be16(data + QUADRO_FIRMWARE_VERSION);
	priv->power_cycles = get_unaligned_be32(data + QUADRO_POWER_CYCLES);

	/* Sensor readings */

	priv->temp_input[0] = get_unaligned_be16(data + QUADRO_TEMP1) * 10;
	priv->temp_input[1] = get_unaligned_be16(data + QUADRO_TEMP2) * 10;
	priv->temp_input[2] = get_unaligned_be16(data + QUADRO_TEMP3) * 10;
	priv->temp_input[3] = get_unaligned_be16(data + QUADRO_TEMP4) * 10;

	priv->speed_input[0] = get_unaligned_be16(data + QUADRO_FLOW_SPEED) / 10;
	priv->speed_input[1] = get_unaligned_be16(data + QUADRO_FAN1_SPEED);
	priv->speed_input[2] = get_unaligned_be16(data + QUADRO_FAN2_SPEED);
	priv->speed_input[3] = get_unaligned_be16(data + QUADRO_FAN3_SPEED);
	priv->speed_input[4] = get_unaligned_be16(data + QUADRO_FAN4_SPEED);

	priv->power_input[0] = get_unaligned_be16(data + QUADRO_FAN1_POWER) * 10000;
	priv->power_input[1] = get_unaligned_be16(data + QUADRO_FAN2_POWER) * 10000;
	priv->power_input[2] = get_unaligned_be16(data + QUADRO_FAN3_POWER) * 10000;
	priv->power_input[3] = get_unaligned_be16(data + QUADRO_FAN4_POWER) * 10000;

	priv->voltage_input[0] = get_unaligned_be16(data + QUADRO_VOLTAGE) * 10;
	priv->voltage_input[1] = get_unaligned_be16(data + QUADRO_FAN1_VOLTAGE) * 10;
	priv->voltage_input[2] = get_unaligned_be16(data + QUADRO_FAN2_VOLTAGE) * 10;
	priv->voltage_input[3] = get_unaligned_be16(data + QUADRO_FAN3_VOLTAGE) * 10;
	priv->voltage_input[4] = get_unaligned_be16(data + QUADRO_FAN4_VOLTAGE) * 10;

	priv->current_input[0] = get_unaligned_be16(data + QUADRO_FAN1_CURRENT);
	priv->current_input[1] = get_unaligned_be16(data + QUADRO_FAN2_CURRENT);
	priv->current_input[2] = get_unaligned_be16(data + QUADRO_FAN3_CURRENT);
	priv->current_input[3] = get_unaligned_be16(data + QUADRO_FAN4_CURRENT);

	priv->updated = jiffies;

	return 0;
}

#ifdef CONFIG_DEBUG_FS

static int serial_number_show(struct seq_file *seqf, void *unused)
{
	struct quadro_data *priv = seqf->private;

	seq_printf(seqf, "%05u-%05u\n", priv->serial_number[0], priv->serial_number[1]);

	return 0;
}
DEFINE_SHOW_ATTRIBUTE(serial_number);

static int firmware_version_show(struct seq_file *seqf, void *unused)
{
	struct quadro_data *priv = seqf->private;

	seq_printf(seqf, "%u\n", priv->firmware_version);

	return 0;
}
DEFINE_SHOW_ATTRIBUTE(firmware_version);

static int power_cycles_show(struct seq_file *seqf, void *unused)
{
	struct quadro_data *priv = seqf->private;

	seq_printf(seqf, "%u\n", priv->power_cycles);

	return 0;
}
DEFINE_SHOW_ATTRIBUTE(power_cycles);

static void quadro_debugfs_init(struct quadro_data *priv)
{
	char name[32];

	scnprintf(name, sizeof(name), "%s-%s", DRIVER_NAME, dev_name(&priv->hdev->dev));

	priv->debugfs = debugfs_create_dir(name, NULL);
	debugfs_create_file("serial_number", 0444, priv->debugfs, priv, &serial_number_fops);
	debugfs_create_file("firmware_version", 0444, priv->debugfs, priv, &firmware_version_fops);
	debugfs_create_file("power_cycles", 0444, priv->debugfs, priv, &power_cycles_fops);
}

#else

static void quadro_debugfs_init(struct quadro_data *priv)
{
}

#endif

static int quadro_probe(struct hid_device *hdev, const struct hid_device_id *id)
{
	struct quadro_data *priv;
	int ret;

	priv = devm_kzalloc(&hdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->hdev = hdev;
	hid_set_drvdata(hdev, priv);

	priv->updated = jiffies - QUADRO_STATUS_UPDATE_INTERVAL;

	ret = hid_parse(hdev);
	if (ret)
		return ret;

	ret = hid_hw_start(hdev, HID_CONNECT_HIDRAW);
	if (ret)
		return ret;

	ret = hid_hw_open(hdev);
	if (ret)
		goto fail_and_stop;

	priv->hwmon_dev = hwmon_device_register_with_info(&hdev->dev, "quadro", priv,
							  &quadro_chip_info, NULL);

	if (IS_ERR(priv->hwmon_dev)) {
		ret = PTR_ERR(priv->hwmon_dev);
		goto fail_and_close;
	}

	quadro_debugfs_init(priv);

	return 0;

fail_and_close:
	hid_hw_close(hdev);
fail_and_stop:
	hid_hw_stop(hdev);
	return ret;
}

static void quadro_remove(struct hid_device *hdev)
{
	struct quadro_data *priv = hid_get_drvdata(hdev);

	debugfs_remove_recursive(priv->debugfs);
	hwmon_device_unregister(priv->hwmon_dev);

	hid_hw_close(hdev);
	hid_hw_stop(hdev);
}

static const struct hid_device_id quadro_table[] = {
	{ HID_USB_DEVICE(0x0c70, 0xf00d) }, /* Aquacomputer Quadro */
	{},
};

MODULE_DEVICE_TABLE(hid, quadro_table);

static struct hid_driver quadro_driver = {
	.name = DRIVER_NAME,
	.id_table = quadro_table,
	.probe = quadro_probe,
	.remove = quadro_remove,
	.raw_event = quadro_raw_event,
};

static int __init quadro_init(void)
{
	return hid_register_driver(&quadro_driver);
}

static void __exit quadro_exit(void)
{
	hid_unregister_driver(&quadro_driver);
}

/* Request to initialize after the HID bus to ensure it's not being loaded before */

late_initcall(quadro_init);
module_exit(quadro_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Leonard Anderweit <leonard.anderweit@gmail.com>");
MODULE_DESCRIPTION("Hwmon driver for Aquacomputer Quadro");