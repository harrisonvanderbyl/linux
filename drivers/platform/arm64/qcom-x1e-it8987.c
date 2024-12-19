// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2024 Maya Matuszczyk <maccraft123mc@gmail.com>
 */
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/input.h>
#include <linux/input/sparse-keymap.h>

#define EC_IRQ_REASON_REG 0x05
#define EC_SUSPEND_RESUME_REG 0x23
#define EC_IRQ_ENABLE_REG 0x35

#define EC_NOTIFY_SUSPEND_ENTER 0x01
#define EC_NOTIFY_SUSPEND_EXIT 0x00
#define EC_NOTIFY_SCREEN_OFF 0x03
#define EC_NOTIFY_SCREEN_ON 0x04

#define EC_IRQ_MICMUTE_BUTTON 0x04
#define EC_IRQ_FAN1_STATUS_CHANGE 0x30
#define EC_IRQ_FAN2_STATUS_CHANGE 0x31
#define EC_IRQ_FAN1_SPEED_CHANGE 0x32
#define EC_IRQ_FAN2_SPEED_CHANGE 0x33
#define EC_IRQ_COMPLETED_LUT_UPDATE 0x34
#define EC_IRQ_COMPLETED_FAN_PROFILE_SWITCH 0x35
#define EC_IRQ_THERMISTOR_1_TEMP_THRESHOLD_CROSS 0x36
#define EC_IRQ_THERMISTOR_2_TEMP_THRESHOLD_CROSS 0x37
#define EC_IRQ_THERMISTOR_3_TEMP_THRESHOLD_CROSS 0x38
#define EC_IRQ_THERMISTOR_4_TEMP_THRESHOLD_CROSS 0x39
#define EC_IRQ_THERMISTOR_5_TEMP_THRESHOLD_CROSS 0x3a
#define EC_IRQ_THERMISTOR_6_TEMP_THRESHOLD_CROSS 0x3b
#define EC_IRQ_THERMISTOR_7_TEMP_THRESHOLD_CROSS 0x3c
#define EC_IRQ_RECOVERED_FROM_RESET 0x3d

struct qcom_x1e_it8987_ec {
	struct i2c_client *client;
	struct input_dev *idev;
	struct mutex lock;
};

static irqreturn_t qcom_x1e_it8987_ec_irq(int irq, void *data)
{
	struct qcom_x1e_it8987_ec *ec = data;
	struct device *dev = &ec->client->dev;
	int val;

	guard(mutex)(&ec->lock);

	val = i2c_smbus_read_byte_data(ec->client, EC_IRQ_REASON_REG);
	if (val < 0) {
		dev_err(dev, "Failed to get EC IRQ reason: %d\n", val);
		return IRQ_HANDLED;
	}

	dev_info(dev, "Unhandled EC IRQ reason: %d\n", val);

	return IRQ_HANDLED;
}

static int qcom_x1e_it8987_ec_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct qcom_x1e_it8987_ec *ec;
	int ret;

	ec = devm_kzalloc(dev, sizeof(*ec), GFP_KERNEL);
	if (!ec)
		return -ENOMEM;

	mutex_init(&ec->lock);
	ec->client = client;

	ret = devm_request_threaded_irq(dev, client->irq,
					NULL, qcom_x1e_it8987_ec_irq,
					IRQF_ONESHOT, "qcom_x1e_it8987_ec", ec);
	if (ret < 0)
		return dev_err_probe(dev, ret, "Unable to request irq\n");

	ret = i2c_smbus_write_byte_data(client, EC_IRQ_ENABLE_REG, 0x01);
	if (ret < 0)
		return dev_err_probe(dev, ret, "Failed to enable interrupts\n");

	return 0;
}

static void qcom_x1e_it8987_ec_remove(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	int ret;

	ret = i2c_smbus_write_byte_data(client, EC_IRQ_ENABLE_REG, 0x00);
	if (ret < 0)
		dev_err(dev, "Failed to disable interrupts: %d\n", ret);
}

static int qcom_x1e_it8987_ec_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	int ret;

	ret = i2c_smbus_write_byte_data(client, EC_SUSPEND_RESUME_REG, EC_NOTIFY_SCREEN_OFF);
	if (ret)
		return ret;

	ret = i2c_smbus_write_byte_data(client, EC_SUSPEND_RESUME_REG, EC_NOTIFY_SUSPEND_ENTER);
	if (ret)
		return ret;

	return 0;
}

static int qcom_x1e_it8987_ec_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	int ret;

	ret = i2c_smbus_write_byte_data(client, EC_SUSPEND_RESUME_REG, EC_NOTIFY_SUSPEND_EXIT);
	if (ret)
		return ret;

	ret = i2c_smbus_write_byte_data(client, EC_SUSPEND_RESUME_REG, EC_NOTIFY_SCREEN_ON);
	if (ret)
		return ret;

	return 0;
}

static const struct of_device_id qcom_x1e_it8987_ec_of_match[] = {
	{ .compatible = "lenovo,yoga-slim7x-ec" },
	{ .compatible = "qcom,x1e-it9897-ec" },
	{}
};
MODULE_DEVICE_TABLE(of, qcom_x1e_it8987_ec_of_match);

static const struct i2c_device_id qcom_x1e_it8987_ec_i2c_id_table[] = {
	{ "qcom-x1e-it8987-ec", },
	{}
};
MODULE_DEVICE_TABLE(i2c, qcom_x1e_it8987_ec_i2c_id_table);

static DEFINE_SIMPLE_DEV_PM_OPS(qcom_x1e_it8987_ec_pm_ops,
		qcom_x1e_it8987_ec_suspend,
		qcom_x1e_it8987_ec_resume);

static struct i2c_driver qcom_x1e_it8987_ec_i2c_driver = {
	.driver = {
		.name = "yoga-slim7x-ec",
		.of_match_table = qcom_x1e_it8987_ec_of_match,
		.pm = &qcom_x1e_it8987_ec_pm_ops
	},
	.probe = qcom_x1e_it8987_ec_probe,
	.remove = qcom_x1e_it8987_ec_remove,
	.id_table = qcom_x1e_it8987_ec_i2c_id_table,
};
module_i2c_driver(qcom_x1e_it8987_ec_i2c_driver);

MODULE_DESCRIPTION("Lenovo Yoga Slim 7x Embedded Controller");
MODULE_LICENSE("GPL");
