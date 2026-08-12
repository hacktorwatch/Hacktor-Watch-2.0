#include "panel.h"

#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/drivers/fuel_gauge.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>
#include <lvgl.h>
#include <lvgl_zephyr.h>

#define DISPLAY_NODE DT_CHOSEN(zephyr_display)
#define TOUCH_NODE DT_CHOSEN(zephyr_touch)
#define IMU_NODE DT_NODELABEL(imu)
#define FUEL_GAUGE_NODE DT_NODELABEL(fuel_gauge)
#define BACKLIGHT_NODE DT_NODELABEL(lcd_backlight)
#define POWER_ENABLE_NODE DT_NODELABEL(lcd_power_enable)
#define CHARGER_STATUS_NODE DT_NODELABEL(charger_status)

#define TICK_MINOR_DIAMETER 4
#define TICK_MAJOR_DIAMETER 8
#define DATE_LABEL_Y_OFFSET 50
#define STEP_LABEL_Y_OFFSET 50
#define STEP_ICON_GAP 6
#define BATTERY_ICON_WIDTH 20
#define BATTERY_ICON_HEIGHT 12
#define BATTERY_ICON_BODY_WIDTH 16
#define BATTERY_ICON_TERMINAL_WIDTH 3
#define BATTERY_ICON_TERMINAL_HEIGHT 6
#define BATTERY_ICON_GAP 6
#define BATTERY_ICON_INNER_PADDING 0
#define BATTERY_REFRESH_MS 10000
#define HOUR_HAND_LENGTH 65
#define MINUTE_HAND_LENGTH 95
#define SECOND_HAND_LENGTH 100
#define SECOND_HAND_TAIL 0
#define HOUR_HAND_WIDTH 5
#define MINUTE_HAND_WIDTH 5
#define SECOND_HAND_WIDTH 3
#define CENTER_CAP_DIAMETER 12
#define INNER_CAP_DIAMETER 2
#define SECOND_HAND_PERIOD_MS 20
#define DISPLAY_POWER_TIMEOUT_MS 30000
#define DISPLAY_POWER_SETTLE_MS 250
#define DISPLAY_LOOP_SLEEP_MAX_MS 100
#define DISPLAY_LOOP_SLEEP_MIN_MS 10
#define UI_LOOP_SLEEP_MAX_MS 20
#define UI_LOOP_SLEEP_MIN_MS 5
#define UI_LOOP_SLEEP_OFF_MS 100
#define UI_THREAD_STACK_SIZE 4096
#define UI_THREAD_PRIORITY 5
#define STEP_REFRESH_MS 1000
#define IMU_FUNC_CFG_ACCESS_REG 0x01
#define IMU_EMB_FUNC_EN_A_REG 0x04
#define IMU_EMB_FUNC_EN_B_REG 0x05
#define IMU_WHO_AM_I_REG 0x0F
#define IMU_CTRL1_XL_REG 0x10
#define IMU_CTRL3_C_REG 0x12
#define IMU_CTRL9_XL_REG 0x18
#define IMU_CTRL10_C_REG 0x19
#define IMU_WAKE_UP_SRC_REG 0x1B
#define IMU_STATUS_REG 0x1E
#define IMU_STEP_COUNTER_L_REG 0x4B
#define IMU_STEP_COUNTER_H_REG 0x4C
#define IMU_TAP_CFG_REG 0x58
#define IMU_WAKE_UP_THS_REG 0x5B
#define IMU_WAKE_UP_DUR_REG 0x5C
#define IMU_MD1_CFG_REG 0x5E
#define IMU_MD2_CFG_REG 0x5F
#define IMU_WHO_AM_I_VALUE 0x6A
#define IMU_WAKE_THRESHOLD 0x06 /* lower value = more sensitive wrist wake */
#define IMU_WAKE_COOLDOWN_MS 2000
#define CLOCK_RED_HEX 0x0000FF
#define CLOCK_WHITE_HEX 0xFFFFFF
#define BATTERY_BLUE_HEX 0xFFCF6F

struct date_info {
	int year;
	int month;
	int day;
	int wday;
};

static const char *const weekday_names[7] = {
	"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};

static const char *const month_names[12] = {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun",
	"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

static const struct i2c_dt_spec imu = I2C_DT_SPEC_GET(IMU_NODE);
static const struct gpio_dt_spec lcd_backlight = GPIO_DT_SPEC_GET(BACKLIGHT_NODE, gpios);
static const struct gpio_dt_spec lcd_power_enable = GPIO_DT_SPEC_GET(POWER_ENABLE_NODE, gpios);
static const struct gpio_dt_spec charger_status =
	GPIO_DT_SPEC_GET(CHARGER_STATUS_NODE, gpios);

static K_SEM_DEFINE(display_wake_sem, 0, 1);
static atomic_t button_wake_pending;
static atomic_t imu_wake_pending;
K_THREAD_STACK_DEFINE(ui_thread_stack, UI_THREAD_STACK_SIZE);

static struct display_capabilities display_cap;
static struct date_info start_date;
static lv_obj_t *date_label;
static lv_obj_t *battery_label;
static lv_obj_t *battery_icon_body;
static lv_obj_t *battery_icon_fill;
static lv_obj_t *battery_icon_terminal;
static lv_obj_t *battery_bolt;
static lv_obj_t *step_label;
static lv_obj_t *step_icon_obj;
static lv_obj_t *hour_hand;
static lv_obj_t *minute_hand;
static lv_obj_t *second_hand;
static lv_obj_t *hour_dot;
static lv_obj_t *minute_dot;
static lv_obj_t *second_dot;
static lv_point_precise_t hour_hand_points[2];
static lv_point_precise_t minute_hand_points[2];
static lv_point_precise_t second_hand_points[2];
static uint64_t base_count_ms;
static int64_t boot_uptime_ms;
static int32_t last_rendered_day = -1;
static bool display_active;
static int64_t display_deadline_ms;
static bool charger_status_ready;
static bool imu_ready;
static bool fuel_gauge_ready;
static bool pedometer_ready;
static uint16_t imu_addr = DT_REG_ADDR(IMU_NODE);
static bool battery_charging;
static uint8_t battery_percent;
static uint16_t step_count;
static int64_t last_battery_poll_ms = -BATTERY_REFRESH_MS;
static int64_t last_imu_wake_ms = -IMU_WAKE_COOLDOWN_MS;
static int64_t last_step_poll_ms = -STEP_REFRESH_MS;
static uint8_t last_imu_wake_src;
static uint8_t last_imu_status;
static struct k_thread ui_thread_data;
static lv_timer_t *clock_timer;
static lv_point_precise_t battery_bolt_points[] = {
	{5, 1},
	{8, 1},
	{6, 5},
	{9, 5},
	{6, 10},
};

extern int do_device_init(const struct device *dev);
extern const lv_image_dsc_t step_icon;

static lv_color_t clock_red(void)
{
	return lv_color_hex(CLOCK_RED_HEX);
}

static lv_color_t clock_white(void)
{
	return lv_color_hex(CLOCK_WHITE_HEX);
}

static lv_color_t battery_indicator_color(void)
{
	if (!fuel_gauge_ready) {
		return clock_white();
	}

	if (battery_percent < 15U) {
		return clock_red();
	}

	if (battery_percent > 90U) {
		return lv_color_hex(BATTERY_BLUE_HEX);
	}

	return clock_white();
}

static void wake_input_cb(struct input_event *evt, void *user_data)
{
	ARG_UNUSED(user_data);

	if (evt->type != INPUT_EV_KEY || evt->value == 0) {
		return;
	}

	if (evt->code == INPUT_KEY_0) {
		atomic_set(&button_wake_pending, 1);
		k_sem_give(&display_wake_sem);
	} else if (evt->code == INPUT_KEY_WAKEUP) {
		atomic_set(&imu_wake_pending, 1);
		k_sem_give(&display_wake_sem);
	}
}
INPUT_CALLBACK_DEFINE(NULL, wake_input_cb, NULL);

static int is_leap_year(int year)
{
	return ((year % 4) == 0 && (year % 100) != 0) || ((year % 400) == 0);
}

static int days_in_month(int year, int month)
{
	static const uint8_t month_days[12] = {
		31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
	};

	if (month == 2 && is_leap_year(year)) {
		return 29;
	}

	return month_days[month - 1];
}

static int calculate_weekday(int year, int month, int day)
{
	int adjusted_year = year;
	int adjusted_month = month;

	if (adjusted_month < 3) {
		adjusted_month += 12;
		adjusted_year--;
	}

	return (day + (13 * (adjusted_month + 1)) / 5 + adjusted_year +
		adjusted_year / 4 - adjusted_year / 100 + adjusted_year / 400 + 6) % 7;
}

static int month_from_abbrev(const char *date_str)
{
	switch (date_str[0]) {
	case 'J':
		return date_str[1] == 'a' ? 1 : (date_str[2] == 'n' ? 6 : 7);
	case 'F':
		return 2;
	case 'M':
		return date_str[2] == 'r' ? 3 : 5;
	case 'A':
		return date_str[1] == 'p' ? 4 : 8;
	case 'S':
		return 9;
	case 'O':
		return 10;
	case 'N':
		return 11;
	case 'D':
		return 12;
	default:
		return 1;
	}
}

static struct date_info compile_date_info(void)
{
	const char *build_date = __DATE__;
	struct date_info info;

	info.month = month_from_abbrev(build_date);
	info.day = (build_date[4] == ' ' ? 0 : (build_date[4] - '0') * 10) + (build_date[5] - '0');
	info.year = (build_date[7] - '0') * 1000 + (build_date[8] - '0') * 100 +
		    (build_date[9] - '0') * 10 + (build_date[10] - '0');
	info.wday = calculate_weekday(info.year, info.month, info.day);

	return info;
}

static uint32_t compile_time_seconds(void)
{
	const char *build_time = __TIME__;
	const uint32_t hour = (build_time[0] - '0') * 10U + (build_time[1] - '0');
	const uint32_t minute = (build_time[3] - '0') * 10U + (build_time[4] - '0');
	const uint32_t second = (build_time[6] - '0') * 10U + (build_time[7] - '0');

	return hour * 3600U + minute * 60U + second;
}

static struct date_info advance_days(struct date_info date, int32_t days)
{
	while (days-- > 0) {
		date.day++;
		date.wday = (date.wday + 1) % 7;

		if (date.day > days_in_month(date.year, date.month)) {
			date.day = 1;
			date.month++;
			if (date.month > 12) {
				date.month = 1;
				date.year++;
			}
		}
	}

	return date;
}

static uint64_t current_clock_ms(void)
{
	const int64_t elapsed_ms = k_uptime_get() - boot_uptime_ms;

	return base_count_ms + MAX(elapsed_ms, 0);
}

static int32_t face_center_x(void)
{
	return display_cap.x_resolution / 2;
}

static int32_t face_center_y(void)
{
	return display_cap.y_resolution / 2;
}

static int32_t polar_x(int32_t radius, int32_t position)
{
	const int32_t angle = (position * 6) - 90;

	return face_center_x() + ((radius * lv_trigo_cos(angle)) >> LV_TRIGO_SHIFT);
}

static int32_t polar_y(int32_t radius, int32_t position)
{
	const int32_t angle = (position * 6) - 90;

	return face_center_y() + ((radius * lv_trigo_sin(angle)) >> LV_TRIGO_SHIFT);
}

static void place_circle(lv_obj_t *obj, int32_t center_x, int32_t center_y, int32_t diameter)
{
	lv_obj_set_size(obj, diameter, diameter);
	lv_obj_set_pos(obj, center_x - (diameter / 2), center_y - (diameter / 2));
}

static void update_date_label(int32_t day_index)
{
	struct date_info current_date;

	if (last_rendered_day == day_index) {
		return;
	}

	last_rendered_day = day_index;
	current_date = advance_days(start_date, day_index);

	lv_label_set_text_fmt(date_label, "%s %02d %s",
			      weekday_names[current_date.wday],
			      current_date.day,
			      month_names[current_date.month - 1]);
}

static void update_battery_label(void)
{
	const int32_t center_x = face_center_x() / 2;
	const int32_t center_y = face_center_y();
	const int32_t fill_height = BATTERY_ICON_HEIGHT - (BATTERY_ICON_INNER_PADDING * 2);
	const int32_t fill_width =
		((BATTERY_ICON_BODY_WIDTH - (BATTERY_ICON_INNER_PADDING * 2)) * battery_percent) / 100;
	const lv_color_t indicator_color = battery_indicator_color();
	int32_t label_width;
	int32_t label_height;
	int32_t total_width;
	int32_t origin_x;
	int32_t body_y;

	if (battery_label == NULL) {
		return;
	}

	if (!fuel_gauge_ready) {
		lv_label_set_text(battery_label, "--%");
	} else {
		lv_label_set_text_fmt(battery_label, "%u%%", battery_percent);
	}

	lv_obj_update_layout(battery_label);
	label_width = lv_obj_get_width(battery_label);
	label_height = lv_obj_get_height(battery_label);
	lv_obj_set_style_text_color(battery_label, indicator_color, 0);
	total_width = BATTERY_ICON_WIDTH + BATTERY_ICON_GAP + label_width;
	origin_x = center_x - (total_width / 2);
	body_y = center_y - (BATTERY_ICON_HEIGHT / 2);

	lv_obj_set_pos(battery_label,
		       origin_x + BATTERY_ICON_WIDTH + BATTERY_ICON_GAP,
		       center_y - (label_height / 2));

	if (battery_icon_body != NULL) {
		lv_obj_set_pos(battery_icon_body, origin_x, body_y);
		lv_obj_set_style_border_color(battery_icon_body, indicator_color, 0);
	}

	if (battery_icon_terminal != NULL) {
		lv_obj_set_pos(battery_icon_terminal,
			       origin_x + BATTERY_ICON_WIDTH - BATTERY_ICON_TERMINAL_WIDTH,
			       center_y - (BATTERY_ICON_TERMINAL_HEIGHT / 2));
		lv_obj_set_style_bg_color(battery_icon_terminal, indicator_color, 0);
	}

	if (battery_icon_fill != NULL) {
		if (fill_width > 0) {
			lv_obj_remove_flag(battery_icon_fill, LV_OBJ_FLAG_HIDDEN);
			lv_obj_set_size(battery_icon_fill, fill_width, fill_height);
			lv_obj_set_pos(battery_icon_fill,
				       BATTERY_ICON_INNER_PADDING,
				       BATTERY_ICON_INNER_PADDING);
			lv_obj_set_style_bg_color(battery_icon_fill, indicator_color, 0);
		} else {
			lv_obj_add_flag(battery_icon_fill, LV_OBJ_FLAG_HIDDEN);
		}
	}

	if (battery_bolt != NULL) {
		if (battery_charging) {
			lv_obj_remove_flag(battery_bolt, LV_OBJ_FLAG_HIDDEN);
		} else {
			lv_obj_add_flag(battery_bolt, LV_OBJ_FLAG_HIDDEN);
		}
	}
}

static void update_step_label(void)
{
	int32_t label_width;
	int32_t label_height;
	int32_t total_width;
	int32_t origin_x;
	int32_t center_y;

	if (step_label == NULL) {
		return;
	}

	if (!pedometer_ready) {
		lv_label_set_text(step_label, "--");
	} else {
		lv_label_set_text_fmt(step_label, "%u", step_count);
	}

	lv_obj_update_layout(step_label);
	label_width = lv_obj_get_width(step_label);
	label_height = lv_obj_get_height(step_label);
	total_width = label_width + STEP_ICON_GAP + step_icon.header.w;
	origin_x = face_center_x() - (total_width / 2);
	center_y = face_center_y() + STEP_LABEL_Y_OFFSET;

	lv_obj_set_pos(step_label, origin_x, center_y - (label_height / 2));

	if (step_icon_obj != NULL) {
		lv_obj_set_pos(step_icon_obj,
			       origin_x + label_width + STEP_ICON_GAP,
			       center_y - (step_icon.header.h / 2));
	}
}

static void update_indicator_dot(lv_obj_t *obj, int32_t position, lv_color_t color)
{
	const bool major_tick = (position % 5) == 0;
	const int32_t tick_radius = (MIN(display_cap.x_resolution, display_cap.y_resolution) / 2 * 100) / 104;
	const int32_t diameter = major_tick ? TICK_MAJOR_DIAMETER : TICK_MINOR_DIAMETER;

	lv_obj_set_style_bg_color(obj, color, 0);
	place_circle(obj, polar_x(tick_radius, position), polar_y(tick_radius, position), diameter);
}

static void update_hands(uint64_t time_ms)
{
	const int32_t total_seconds = time_ms / 1000;
	const int32_t seconds = total_seconds % 60;
	const int32_t total_minutes = total_seconds / 60;
	const int32_t minutes = total_minutes % 60;
	const int32_t hours = (total_minutes / 60) % 24;
	const int32_t hour_index = hours % 12;
	const int32_t day_index = total_seconds / 86400;
	const int32_t hour_angle = ((time_ms / 120000) % 360) - 90;
	const int32_t minute_angle = ((time_ms / 10000) % 360) - 90;
	const int32_t second_angle = ((time_ms * 6) / 1000 % 360) - 90;

	hour_hand_points[0].x = face_center_x();
	hour_hand_points[0].y = face_center_y();
	hour_hand_points[1].x = face_center_x() +
		((HOUR_HAND_LENGTH * lv_trigo_cos(hour_angle)) >> LV_TRIGO_SHIFT);
	hour_hand_points[1].y = face_center_y() +
		((HOUR_HAND_LENGTH * lv_trigo_sin(hour_angle)) >> LV_TRIGO_SHIFT);

	minute_hand_points[0].x = face_center_x();
	minute_hand_points[0].y = face_center_y();
	minute_hand_points[1].x = face_center_x() +
		((MINUTE_HAND_LENGTH * lv_trigo_cos(minute_angle)) >> LV_TRIGO_SHIFT);
	minute_hand_points[1].y = face_center_y() +
		((MINUTE_HAND_LENGTH * lv_trigo_sin(minute_angle)) >> LV_TRIGO_SHIFT);

	second_hand_points[0].x = face_center_x() -
		((SECOND_HAND_TAIL * lv_trigo_cos(second_angle)) >> LV_TRIGO_SHIFT);
	second_hand_points[0].y = face_center_y() -
		((SECOND_HAND_TAIL * lv_trigo_sin(second_angle)) >> LV_TRIGO_SHIFT);
	second_hand_points[1].x = face_center_x() +
		((SECOND_HAND_LENGTH * lv_trigo_cos(second_angle)) >> LV_TRIGO_SHIFT);
	second_hand_points[1].y = face_center_y() +
		((SECOND_HAND_LENGTH * lv_trigo_sin(second_angle)) >> LV_TRIGO_SHIFT);

	lv_line_set_points_mutable(hour_hand, hour_hand_points, 2);
	lv_line_set_points_mutable(minute_hand, minute_hand_points, 2);
	lv_line_set_points_mutable(second_hand, second_hand_points, 2);

	update_indicator_dot(hour_dot, (hour_index * 5) % 60, clock_red());
	update_indicator_dot(minute_dot, minutes, clock_red());
	update_indicator_dot(second_dot, seconds, clock_red());
	update_date_label(day_index);
}

static void drain_display_wake_events(void)
{
	while (k_sem_take(&display_wake_sem, K_NO_WAIT) == 0) {
	}
}

static void set_display_deadline(void)
{
	display_deadline_ms = k_uptime_get() + DISPLAY_POWER_TIMEOUT_MS;
}

static int set_display_gpio(const struct gpio_dt_spec *gpio, int value, const char *name)
{
	const int ret = gpio_pin_set_dt(gpio, value);

	if (ret < 0) {
		printk("%s GPIO update failed: %d\n", name, ret);
	}

	return ret;
}

static int refresh_display_contents(void)
{
	lvgl_lock();
	update_hands(current_clock_ms());
	lv_obj_invalidate(lv_screen_active());
	lv_refr_now(NULL);
	lvgl_unlock();

	return 0;
}

static void set_clock_timer_paused(bool paused)
{
	if (clock_timer == NULL) {
		return;
	}

	if (paused) {
		lv_timer_pause(clock_timer);
		return;
	}

	lv_timer_resume(clock_timer);
	lv_timer_ready(clock_timer);
}

static void ui_thread_entry(void *arg1, void *arg2, void *arg3)
{
	uint32_t sleep_ms;

	ARG_UNUSED(arg1);
	ARG_UNUSED(arg2);
	ARG_UNUSED(arg3);

	while (1) {
		if (!display_active) {
			k_sleep(K_MSEC(UI_LOOP_SLEEP_OFF_MS));
			continue;
		}

		lvgl_lock();
		sleep_ms = lv_timer_handler();
		lvgl_unlock();

		if (sleep_ms == LV_NO_TIMER_READY || sleep_ms > UI_LOOP_SLEEP_MAX_MS) {
			sleep_ms = UI_LOOP_SLEEP_MAX_MS;
		}
		if (sleep_ms < UI_LOOP_SLEEP_MIN_MS) {
			sleep_ms = UI_LOOP_SLEEP_MIN_MS;
		}

		k_sleep(K_MSEC(sleep_ms));
	}
}

static bool take_wake_flag(atomic_t *flag)
{
	return atomic_set(flag, 0) != 0;
}

static int imu_write_register(uint8_t reg, uint8_t value)
{
	return i2c_reg_write_byte(imu.bus, imu_addr, reg, value);
}

static int imu_read_register(uint8_t reg, uint8_t *value)
{
	return i2c_reg_read_byte(imu.bus, imu_addr, reg, value);
}

static void clear_imu_interrupt_source(void)
{
	uint8_t wake_src;

	(void)imu_read_register(IMU_WAKE_UP_SRC_REG, &wake_src);
}

static int read_step_counter(uint16_t *steps)
{
	uint8_t lo;
	uint8_t hi;
	int ret;

	ret = imu_read_register(IMU_STEP_COUNTER_L_REG, &lo);
	if (ret < 0) {
		return ret;
	}

	ret = imu_read_register(IMU_STEP_COUNTER_H_REG, &hi);
	if (ret < 0) {
		return ret;
	}

	*steps = ((uint16_t)hi << 8) | lo;
	return 0;
}

static int enable_imu_pedometer(void)
{
	uint16_t steps = 0;
	int ret;
	int exit_ret;

	ret = imu_write_register(IMU_FUNC_CFG_ACCESS_REG, 0x80);
	if (ret < 0) {
		return ret;
	}

	ret = imu_write_register(IMU_EMB_FUNC_EN_A_REG, 0x00);
	if (ret < 0) {
		goto exit_bank;
	}

	ret = imu_write_register(IMU_EMB_FUNC_EN_B_REG, 0x10);

exit_bank:
	exit_ret = imu_write_register(IMU_FUNC_CFG_ACCESS_REG, 0x00);
	if (ret < 0) {
		return ret;
	}
	if (exit_ret < 0) {
		return exit_ret;
	}

	ret = imu_write_register(IMU_CTRL10_C_REG, 0x6C);
	if (ret < 0) {
		return ret;
	}

	k_sleep(K_MSEC(5));

	ret = imu_write_register(IMU_CTRL10_C_REG, 0x3C);
	if (ret < 0) {
		return ret;
	}

	ret = read_step_counter(&steps);
	if (ret < 0) {
		return ret;
	}

	step_count = steps;
	last_step_poll_ms = k_uptime_get();
	pedometer_ready = true;
	printk("IMU pedometer ready, steps=%u\n", step_count);

	return 0;
}

static void poll_step_counter(bool force_refresh)
{
	uint16_t steps;
	int64_t now_ms;
	int ret;

	if (!pedometer_ready) {
		return;
	}

	now_ms = k_uptime_get();
	if (!force_refresh && (now_ms - last_step_poll_ms) < STEP_REFRESH_MS) {
		return;
	}

	ret = read_step_counter(&steps);
	last_step_poll_ms = now_ms;
	if (ret < 0) {
		printk("Step counter read failed: %d\n", ret);
		return;
	}

	if (!force_refresh && steps == step_count) {
		return;
	}

	step_count = steps;

	lvgl_lock();
	update_step_label();
	lvgl_unlock();
}

static int init_imu_wake_gesture(void)
{
	uint8_t who_am_i = 0;
	const uint16_t candidates[] = {
		DT_REG_ADDR(IMU_NODE),
		DT_REG_ADDR(IMU_NODE) == 0x6A ? 0x6B : 0x6A,
	};
	bool found = false;
	int ret;
	size_t i;

	if (!device_is_ready(imu.bus)) {
		printk("IMU I2C bus is not ready\n");
		return -ENODEV;
	}

	for (i = 0; i < ARRAY_SIZE(candidates); ++i) {
		imu_addr = candidates[i];
		ret = imu_read_register(IMU_WHO_AM_I_REG, &who_am_i);
		if (ret == 0 && who_am_i == IMU_WHO_AM_I_VALUE) {
			found = true;
			break;
		}
	}

	if (!found) {
		printk("IMU not detected on 0x6A/0x6B\n");
		return -ENODEV;
	}

	ret = imu_write_register(IMU_CTRL3_C_REG, 0x44);
	if (ret < 0) {
		return ret;
	}
	ret = imu_write_register(IMU_CTRL1_XL_REG, 0x40);
	if (ret < 0) {
		return ret;
	}
	ret = imu_write_register(IMU_CTRL9_XL_REG, 0x38);
	if (ret < 0) {
		return ret;
	}
	ret = imu_write_register(IMU_TAP_CFG_REG, 0x80);
	if (ret < 0) {
		return ret;
	}
	ret = imu_write_register(IMU_WAKE_UP_THS_REG, IMU_WAKE_THRESHOLD);
	if (ret < 0) {
		return ret;
	}
	ret = imu_write_register(IMU_WAKE_UP_DUR_REG, 0x00);
	if (ret < 0) {
		return ret;
	}
	ret = imu_write_register(IMU_MD1_CFG_REG, 0x20);
	if (ret < 0) {
		return ret;
	}
	ret = imu_write_register(IMU_MD2_CFG_REG, 0x20);
	if (ret < 0) {
		return ret;
	}

	clear_imu_interrupt_source();
	last_imu_wake_src = 0;
	last_imu_status = 0;
	imu_ready = true;
	printk("IMU wake gesture ready at 0x%02x\n", imu_addr);

	ret = enable_imu_pedometer();
	if (ret < 0) {
		printk("IMU pedometer enable failed: %d\n", ret);
	}

	return 0;
}

static bool process_imu_wake_event(void)
{
	int64_t now_ms;
	int ret;

	if (!take_wake_flag(&imu_wake_pending) || !imu_ready) {
		return false;
	}

	ret = imu_read_register(IMU_WAKE_UP_SRC_REG, &last_imu_wake_src);
	if (ret < 0) {
		printk("IMU WAKE_UP_SRC read failed: %d\n", ret);
		last_imu_wake_src = 0;
	}

	ret = imu_read_register(IMU_STATUS_REG, &last_imu_status);
	if (ret < 0) {
		printk("IMU STATUS_REG read failed: %d\n", ret);
		last_imu_status = 0;
	}

	clear_imu_interrupt_source();

	now_ms = k_uptime_get();
	if ((now_ms - last_imu_wake_ms) < IMU_WAKE_COOLDOWN_MS) {
		return false;
	}

	last_imu_wake_ms = now_ms;
	return true;
}

static int init_touch_device(bool reinit)
{
	const struct device *touch = DEVICE_DT_GET(TOUCH_NODE);
	int ret;

	if (reinit) {
		ret = do_device_init(touch);
	} else if (!device_is_ready(touch)) {
		ret = device_init(touch);
	} else {
		return 0;
	}

	if (ret < 0) {
		printk("Touch init failed: %d\n", ret);
		return ret;
	}

	return 0;
}

static int init_charger_status_gpio(void)
{
	int ret;

	if (!gpio_is_ready_dt(&charger_status)) {
		printk("Charge status GPIO is not ready\n");
		return -ENODEV;
	}

	ret = gpio_pin_configure_dt(&charger_status, GPIO_INPUT);
	if (ret < 0) {
		printk("Charge status GPIO setup failed: %d\n", ret);
		return ret;
	}

	charger_status_ready = true;
	return 0;
}

static int init_fuel_gauge_device(bool reinit)
{
	const struct device *fuel_gauge = DEVICE_DT_GET(FUEL_GAUGE_NODE);
	int ret;

	if (reinit) {
		ret = do_device_init(fuel_gauge);
	} else if (!device_is_ready(fuel_gauge)) {
		ret = device_init(fuel_gauge);
	} else {
		fuel_gauge_ready = true;
		return 0;
	}

	if (ret < 0) {
		fuel_gauge_ready = false;
		printk("Fuel gauge init failed: %d\n", ret);
		return ret;
	}

	fuel_gauge_ready = true;
	return 0;
}

static void poll_battery_level(bool force_refresh)
{
	const struct device *fuel_gauge = DEVICE_DT_GET(FUEL_GAUGE_NODE);
	union fuel_gauge_prop_val val;
	int64_t now_ms;
	int ret;

	if (!fuel_gauge_ready) {
		return;
	}

	now_ms = k_uptime_get();
	if (!force_refresh && (now_ms - last_battery_poll_ms) < BATTERY_REFRESH_MS) {
		return;
	}

	ret = fuel_gauge_get_prop(fuel_gauge, FUEL_GAUGE_RELATIVE_STATE_OF_CHARGE, &val);
	last_battery_poll_ms = now_ms;
	if (ret < 0) {
		fuel_gauge_ready = false;
		printk("Battery percent read failed: %d\n", ret);
		lvgl_lock();
		update_battery_label();
		lvgl_unlock();
		return;
	}

	battery_percent = MIN(val.relative_state_of_charge, 100U);

	lvgl_lock();
	update_battery_label();
	lvgl_unlock();
}

static void poll_charging_state(bool force_refresh)
{
	bool charging;
	int ret;

	if (!charger_status_ready) {
		return;
	}

	ret = gpio_pin_get_dt(&charger_status);
	if (ret < 0) {
		printk("Charge status read failed: %d\n", ret);
		return;
	}

	charging = ret > 0;
	if (!force_refresh && charging == battery_charging) {
		return;
	}

	battery_charging = charging;

	lvgl_lock();
	update_battery_label();
	lvgl_unlock();
}

static int power_on_display(const struct device *display, bool reinit)
{
	int ret;

	ret = set_display_gpio(&lcd_power_enable, 1, "LCD power");
	if (ret < 0) {
		return ret;
	}

	k_sleep(K_MSEC(DISPLAY_POWER_SETTLE_MS));

	if (reinit) {
		ret = do_device_init(display);
		if (ret < 0) {
			printk("Display re-init failed: %d\n", ret);
			return ret;
		}

		display_get_capabilities(display, &display_cap);
	}

	ret = init_touch_device(reinit);
	if (ret < 0) {
		printk("Continuing without touch input\n");
	}

	ret = init_fuel_gauge_device(reinit);
	if (ret < 0) {
		printk("Continuing without battery gauge\n");
	}

	poll_charging_state(true);
	poll_battery_level(true);
	poll_step_counter(true);

	ret = display_blanking_off(display);
	if (ret < 0 && ret != -ENOSYS) {
		printk("Failed to enable display output: %d\n", ret);
		return ret;
	}

	ret = refresh_display_contents();
	if (ret < 0) {
		return ret;
	}

	ret = set_display_gpio(&lcd_backlight, 1, "LCD backlight");
	if (ret < 0) {
		return ret;
	}

	display_active = true;
	set_display_deadline();

	lvgl_lock();
	set_clock_timer_paused(false);
	lvgl_unlock();

	return 0;
}

static void power_off_display(const struct device *display)
{
	int ret;

	display_active = false;
	display_deadline_ms = 0;

	lvgl_lock();
	set_clock_timer_paused(true);
	lvgl_unlock();

	ret = display_blanking_on(display);
	if (ret < 0 && ret != -ENOSYS) {
		printk("Failed to blank display: %d\n", ret);
	}

	(void)set_display_gpio(&lcd_backlight, 0, "LCD backlight");
	k_sleep(K_MSEC(20));
	(void)set_display_gpio(&lcd_power_enable, 0, "LCD power");

	fuel_gauge_ready = false;
}

static uint32_t next_active_sleep_ms(void)
{
	int64_t remaining_ms = display_deadline_ms - k_uptime_get();

	if (remaining_ms <= 0) {
		return 0;
	}

	if (remaining_ms > DISPLAY_LOOP_SLEEP_MAX_MS) {
		return DISPLAY_LOOP_SLEEP_MAX_MS;
	}

	if (remaining_ms < DISPLAY_LOOP_SLEEP_MIN_MS) {
		return MAX(remaining_ms, 1);
	}

	return remaining_ms;
}

static void clock_timer_cb(lv_timer_t *timer)
{
	ARG_UNUSED(timer);

	if (!display_active) {
		return;
	}

	update_hands(current_clock_ms());
}

static int cmd_app_status(const struct shell *sh, size_t argc, char **argv)
{
	const uint64_t time_ms = current_clock_ms();
	const int32_t total_seconds = time_ms / 1000;
	const int32_t seconds = total_seconds % 60;
	const int32_t total_minutes = total_seconds / 60;
	const int32_t minutes = total_minutes % 60;
	const int32_t hours = (total_minutes / 60) % 24;

	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	shell_print(sh, "display: %ux%u orientation=%u power=%s timeout_ms=%lld",
		    display_cap.x_resolution,
		    display_cap.y_resolution,
		    display_cap.current_orientation,
		    display_active ? "on" : "off",
		    display_active ? MAX(display_deadline_ms - k_uptime_get(), 0) : 0);
	shell_print(sh, "imu: %s addr=0x%02x wake_src=0x%02x status=0x%02x",
		    imu_ready ? "ready" : "off",
		    imu_addr,
		    last_imu_wake_src,
		    last_imu_status);
	shell_print(sh, "battery: %s percent=%u",
		    fuel_gauge_ready ? "ready" : "off",
		    battery_percent);
	shell_print(sh, "charging: %s gpio=%s",
		    battery_charging ? "yes" : "no",
		    charger_status_ready ? "ready" : "off");
	shell_print(sh, "pedometer: %s steps=%u",
		    pedometer_ready ? "ready" : "off",
		    step_count);
	shell_print(sh, "clock: %02d:%02d:%02d", hours, minutes, seconds);

	return 0;
}
SHELL_CMD_REGISTER(app, NULL, "Show current display and clock status", cmd_app_status);

static int init_display(const struct device **display_dev)
{
	const struct device *display = DEVICE_DT_GET(DISPLAY_NODE);
	int ret;

	if (!gpio_is_ready_dt(&lcd_backlight)) {
		printk("LCD backlight GPIO is not ready\n");
		return -ENODEV;
	}

	if (!gpio_is_ready_dt(&lcd_power_enable)) {
		printk("LCD power GPIO is not ready\n");
		return -ENODEV;
	}

	ret = init_charger_status_gpio();
	if (ret < 0) {
		printk("Continuing without charge status GPIO\n");
	}

	ret = gpio_pin_configure_dt(&lcd_power_enable, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
		printk("Power GPIO setup failed: %d\n", ret);
		return ret;
	}

	ret = gpio_pin_configure_dt(&lcd_backlight, GPIO_OUTPUT_INACTIVE);
	if (ret < 0) {
		printk("Backlight GPIO setup failed: %d\n", ret);
		return ret;
	}

	k_sleep(K_MSEC(DISPLAY_POWER_SETTLE_MS));

	if (!device_is_ready(display)) {
		ret = device_init(display);
		if (ret < 0) {
			printk("Display init failed: %d\n", ret);
			return ret;
		}
	}

	if (!device_is_ready(display)) {
		printk("Display device is not ready\n");
		return -ENODEV;
	}

	display_get_capabilities(display, &display_cap);

	*display_dev = display;
	return 0;
}

static lv_obj_t *create_marker(lv_obj_t *parent, lv_color_t color)
{
	lv_obj_t *marker = lv_obj_create(parent);

	if (marker == NULL) {
		return NULL;
	}

	lv_obj_remove_style_all(marker);
	lv_obj_set_style_radius(marker, LV_RADIUS_CIRCLE, 0);
	lv_obj_set_style_bg_color(marker, color, 0);
	lv_obj_set_style_bg_opa(marker, LV_OPA_COVER, 0);

	return marker;
}

static int create_tick_dot(lv_obj_t *parent, int32_t position)
{
	const bool major_tick = (position % 5) == 0;
	const int32_t tick_radius = (MIN(display_cap.x_resolution, display_cap.y_resolution) / 2 * 100) / 104;
	const int32_t diameter = major_tick ? TICK_MAJOR_DIAMETER : TICK_MINOR_DIAMETER;
	lv_obj_t *tick = create_marker(parent, clock_white());

	if (tick == NULL) {
		return -ENOMEM;
	}

	place_circle(tick, polar_x(tick_radius, position), polar_y(tick_radius, position), diameter);
	return 0;
}

static int create_cardinal_label(lv_obj_t *parent, int value, int32_t position)
{
	const int32_t label_radius = (MIN(display_cap.x_resolution, display_cap.y_resolution) / 2 * 100) / 130;
	lv_obj_t *label = lv_label_create(parent);

	if (label == NULL) {
		return -ENOMEM;
	}

	lv_obj_set_style_text_font(label, &lv_font_montserrat_20, 0);
	lv_obj_set_style_text_color(label, clock_white(), 0);
	lv_label_set_text_fmt(label, "%d", value);
	lv_obj_update_layout(label);
	lv_obj_set_pos(label,
		       polar_x(label_radius, position) - (lv_obj_get_width(label) / 2),
		       polar_y(label_radius, position) - (lv_obj_get_height(label) / 2) + 3);
	return 0;
}

static int init_ui(void)
{
	lv_obj_t *screen;
	lv_obj_t *face;
	lv_obj_t *center_cap;
	lv_obj_t *inner_cap;
	int32_t face_size;
	int position;
	int ret;

	screen = lv_screen_active();
	lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
	lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

	face_size = MIN(display_cap.x_resolution, display_cap.y_resolution);

	face = lv_obj_create(screen);
	if (face == NULL) {
		printk("LVGL allocation failed: face\n");
		return -ENOMEM;
	}

	lv_obj_remove_style_all(face);
	lv_obj_set_size(face, face_size, face_size);
	lv_obj_set_style_radius(face, LV_RADIUS_CIRCLE, 0);
	lv_obj_set_style_bg_color(face, lv_color_black(), 0);
	lv_obj_set_style_bg_opa(face, LV_OPA_COVER, 0);
	lv_obj_center(face);

	for (position = 1; position <= 60; ++position) {
		ret = create_tick_dot(screen, position % 60);
		if (ret < 0) {
			printk("LVGL allocation failed: tick %d\n", position);
			return ret;
		}
	}

	ret = create_cardinal_label(screen, 12, 0);
	if (ret < 0) {
		printk("LVGL allocation failed: label 12\n");
		return ret;
	}
	ret = create_cardinal_label(screen, 3, 15);
	if (ret < 0) {
		printk("LVGL allocation failed: label 3\n");
		return ret;
	}
	ret = create_cardinal_label(screen, 6, 30);
	if (ret < 0) {
		printk("LVGL allocation failed: label 6\n");
		return ret;
	}

	date_label = lv_label_create(screen);
	if (date_label == NULL) {
		printk("LVGL allocation failed: date label\n");
		return -ENOMEM;
	}
	lv_obj_set_style_text_font(date_label, &lv_font_montserrat_14, 0);
	lv_obj_set_style_text_color(date_label, clock_white(), 0);
	lv_obj_align(date_label, LV_ALIGN_CENTER, 0, -DATE_LABEL_Y_OFFSET);
	update_date_label(0);

	battery_label = lv_label_create(screen);
	if (battery_label == NULL) {
		printk("LVGL allocation failed: battery label\n");
		return -ENOMEM;
	}
	lv_obj_set_style_text_font(battery_label, &lv_font_montserrat_14, 0);
	lv_obj_set_style_text_color(battery_label, clock_white(), 0);

	battery_icon_body = lv_obj_create(screen);
	if (battery_icon_body == NULL) {
		printk("LVGL allocation failed: battery body\n");
		return -ENOMEM;
	}
	lv_obj_remove_style_all(battery_icon_body);
	lv_obj_set_size(battery_icon_body, BATTERY_ICON_BODY_WIDTH, BATTERY_ICON_HEIGHT);
	lv_obj_set_style_bg_opa(battery_icon_body, LV_OPA_TRANSP, 0);
	lv_obj_set_style_border_width(battery_icon_body, 1, 0);
	lv_obj_set_style_border_color(battery_icon_body, clock_white(), 0);
	lv_obj_set_style_radius(battery_icon_body, 1, 0);

	battery_icon_fill = lv_obj_create(battery_icon_body);
	if (battery_icon_fill == NULL) {
		printk("LVGL allocation failed: battery fill\n");
		return -ENOMEM;
	}
	lv_obj_remove_style_all(battery_icon_fill);
	lv_obj_set_style_bg_opa(battery_icon_fill, LV_OPA_COVER, 0);
	lv_obj_set_style_radius(battery_icon_fill, 0, 0);

	battery_icon_terminal = lv_obj_create(screen);
	if (battery_icon_terminal == NULL) {
		printk("LVGL allocation failed: battery terminal\n");
		return -ENOMEM;
	}
	lv_obj_remove_style_all(battery_icon_terminal);
	lv_obj_set_size(battery_icon_terminal,
			BATTERY_ICON_TERMINAL_WIDTH,
			BATTERY_ICON_TERMINAL_HEIGHT);
	lv_obj_set_style_bg_color(battery_icon_terminal, clock_white(), 0);
	lv_obj_set_style_bg_opa(battery_icon_terminal, LV_OPA_COVER, 0);
	lv_obj_set_style_radius(battery_icon_terminal, 0, 0);

	battery_bolt = lv_line_create(battery_icon_body);
	if (battery_bolt == NULL) {
		printk("LVGL allocation failed: battery bolt\n");
		return -ENOMEM;
	}
	lv_obj_remove_style_all(battery_bolt);
	lv_obj_set_style_line_width(battery_bolt, 2, 0);
	lv_obj_set_style_line_color(battery_bolt, clock_red(), 0);
	lv_obj_set_style_line_rounded(battery_bolt, false, 0);
	lv_line_set_points_mutable(battery_bolt, battery_bolt_points, ARRAY_SIZE(battery_bolt_points));
	lv_obj_set_pos(battery_bolt, 0, 0);
	lv_obj_set_size(battery_bolt, BATTERY_ICON_BODY_WIDTH, BATTERY_ICON_HEIGHT);
	lv_obj_add_flag(battery_bolt, LV_OBJ_FLAG_HIDDEN);

	update_battery_label();

	step_label = lv_label_create(screen);
	if (step_label == NULL) {
		printk("LVGL allocation failed: step label\n");
		return -ENOMEM;
	}
	lv_obj_set_style_text_font(step_label, &lv_font_montserrat_14, 0);
	lv_obj_set_style_text_color(step_label, clock_white(), 0);

	step_icon_obj = lv_image_create(screen);
	if (step_icon_obj == NULL) {
		printk("LVGL allocation failed: step icon\n");
		return -ENOMEM;
	}
	lv_image_set_src(step_icon_obj, &step_icon);
	update_step_label();

	hour_hand = lv_line_create(screen);
	if (hour_hand == NULL) {
		printk("LVGL allocation failed: hour hand\n");
		return -ENOMEM;
	}
	lv_obj_remove_style_all(hour_hand);
	lv_obj_set_style_line_width(hour_hand, HOUR_HAND_WIDTH, 0);
	lv_obj_set_style_line_rounded(hour_hand, true, 0);
	lv_obj_set_style_line_color(hour_hand, clock_white(), 0);

	minute_hand = lv_line_create(screen);
	if (minute_hand == NULL) {
		printk("LVGL allocation failed: minute hand\n");
		return -ENOMEM;
	}
	lv_obj_remove_style_all(minute_hand);
	lv_obj_set_style_line_width(minute_hand, MINUTE_HAND_WIDTH, 0);
	lv_obj_set_style_line_rounded(minute_hand, true, 0);
	lv_obj_set_style_line_color(minute_hand, clock_red(), 0);

	second_hand = lv_line_create(screen);
	if (second_hand == NULL) {
		printk("LVGL allocation failed: second hand\n");
		return -ENOMEM;
	}
	lv_obj_remove_style_all(second_hand);
	lv_obj_set_style_line_width(second_hand, SECOND_HAND_WIDTH, 0);
	lv_obj_set_style_line_rounded(second_hand, true, 0);
	lv_obj_set_style_line_color(second_hand, clock_red(), 0);

	hour_dot = create_marker(screen, clock_red());
	if (hour_dot == NULL) {
		printk("LVGL allocation failed: hour dot\n");
		return -ENOMEM;
	}
	minute_dot = create_marker(screen, clock_red());
	if (minute_dot == NULL) {
		printk("LVGL allocation failed: minute dot\n");
		return -ENOMEM;
	}
	second_dot = create_marker(screen, clock_red());
	if (second_dot == NULL) {
		printk("LVGL allocation failed: second dot\n");
		return -ENOMEM;
	}

	center_cap = create_marker(screen, clock_red());
	if (center_cap == NULL) {
		printk("LVGL allocation failed: center cap\n");
		return -ENOMEM;
	}
	place_circle(center_cap, face_center_x(), face_center_y(), CENTER_CAP_DIAMETER);

	inner_cap = create_marker(screen, lv_color_black());
	if (inner_cap == NULL) {
		printk("LVGL allocation failed: inner cap\n");
		return -ENOMEM;
	}
	lv_obj_set_style_border_width(inner_cap, 1, 0);
	lv_obj_set_style_border_color(inner_cap, clock_white(), 0);
	place_circle(inner_cap, face_center_x(), face_center_y(), INNER_CAP_DIAMETER);

	update_hands(current_clock_ms());
	clock_timer = lv_timer_create(clock_timer_cb, SECOND_HAND_PERIOD_MS, NULL);
	if (clock_timer == NULL) {
		printk("LVGL allocation failed: clock timer\n");
		return -ENOMEM;
	}
	set_clock_timer_paused(true);

	return 0;
}

int panel_run(void)
{
	const struct device *display;
	int ret;
	uint32_t sleep_ms;

	ret = init_display(&display);
	if (ret < 0) {
		return 0;
	}

	ret = init_imu_wake_gesture();
	if (ret < 0) {
		printk("IMU wake init failed: %d\n", ret);
	}

	start_date = compile_date_info();
	base_count_ms = (uint64_t)compile_time_seconds() * 1000ULL;
	boot_uptime_ms = k_uptime_get();
	last_rendered_day = -1;

	ret = lvgl_init();
	if (ret < 0) {
		printk("LVGL init failed: %d\n", ret);
		return 0;
	}

	lvgl_lock();
	ret = init_ui();
	lvgl_unlock();

	if (ret < 0) {
		return 0;
	}

	ret = power_on_display(display, false);
	if (ret < 0) {
		return 0;
	}

	k_thread_create(&ui_thread_data, ui_thread_stack, K_THREAD_STACK_SIZEOF(ui_thread_stack),
			ui_thread_entry, NULL, NULL, NULL, UI_THREAD_PRIORITY, 0, K_NO_WAIT);
	k_thread_name_set(&ui_thread_data, "lvgl_ui");

	while (1) {
		bool button_wake;
		bool imu_wake;

		if (!display_active) {
			(void)k_sem_take(&display_wake_sem, K_FOREVER);
			drain_display_wake_events();

			button_wake = take_wake_flag(&button_wake_pending);
			imu_wake = process_imu_wake_event();
			if (!button_wake && !imu_wake) {
				continue;
			}

			ret = power_on_display(display, true);
			if (ret < 0) {
				k_sleep(K_MSEC(100));
			}
			continue;
		}

		poll_charging_state(false);
		poll_battery_level(false);
		poll_step_counter(false);
		sleep_ms = next_active_sleep_ms();
		if (sleep_ms == 0U) {
			power_off_display(display);
			continue;
		}

		ret = k_sem_take(&display_wake_sem, K_MSEC(sleep_ms));
		if (ret == 0) {
			drain_display_wake_events();
			button_wake = take_wake_flag(&button_wake_pending);
			imu_wake = process_imu_wake_event();
			if (button_wake || imu_wake) {
				set_display_deadline();
			}
			continue;
		}

		if (k_uptime_get() >= display_deadline_ms) {
			power_off_display(display);
		}
	}
}
