/*
 * Copyright (c) 2020 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_kscan_gpio_demux

#include <device.h>
#include <drivers/kscan.h>
#include <drivers/gpio.h>
#include <logging/log.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

struct kscan_gpio_item_config {
    char *label;
    gpio_pin_t pin;
    gpio_flags_t flags;
};

#define _KSCAN_GPIO_ITEM_CFG_INIT(n, prop, idx)                                                    \
    {                                                                                              \
        .label = DT_INST_GPIO_LABEL_BY_IDX(n, prop, idx),                                          \
        .pin = DT_INST_GPIO_PIN_BY_IDX(n, prop, idx),                                              \
        .flags = DT_INST_GPIO_FLAGS_BY_IDX(n, prop, idx),                                          \
    },

#define _KSCAN_GPIO_ROW_CFG_INIT(idx, n) _KSCAN_GPIO_ITEM_CFG_INIT(n, row_gpios, idx)
#define _KSCAN_GPIO_COL_CFG_INIT(idx, n) _KSCAN_GPIO_ITEM_CFG_INIT(n, col_gpios, idx)

#if !defined(CONFIG_ZMK_KSCAN_DEMUX_POLLING)
static int kscan_gpio_config_interrupts(struct device **devices,
                                        const struct kscan_gpio_item_config *configs, size_t len,
                                        gpio_flags_t flags) {
    for (int i = 0; i < len; i++) {
        struct device *dev = devices[i];
        const struct kscan_gpio_item_config *cfg = &configs[i];

        int err = gpio_pin_interrupt_configure(dev, cfg->pin, flags);

        if (err) {
            LOG_ERR("Unable to enable matrix GPIO interrupt");
            return err;
        }
    }
    return 0;
}
#endif

#define IF_KSCAN_POLLED(a, b) COND_CODE_1(CONFIG_ZMK_KSCAN_DEMUX_POLLING, a, b)

#define INST_MATRIX_ROWS(n) DT_INST_PROP_LEN(n, row_gpios)
#define INST_MATRIX_COLS(n) DT_INST_PROP(n, total_cols)
#define INST_DEMUX_COLS(n) DT_INST_PROP_LEN(n, col_gpios)

// a if true, b if false
#define IF_IS_ROW2COL(n, a, b) COND_CODE_0(DT_ENUM_IDX(DT_DRV_INST(n), diode_direction), a, b)

// Below is set based on row2col or col2row.
// row2col is enum item 0, col2row is enum item 1
// If row2col, rows = outputs & cols = inputs
// TODO: NIBBLE is row2col, BUT active low with cols as outputs.

// Physical IO devices (GPIO pins)
#define INST_OUTPUT_LEN(n) IF_IS_ROW2COL(n, (INST_MATRIX_ROWS(n)), (INST_MATRIX_COLS(n)))
// Physical IO devices (GPIO pins)
#define INST_INPUT_LEN(n) IF_IS_ROW2COL(n, (INST_MATRIX_COLS(n)), (INST_MATRIX_ROWS(n)))

#define GPIO_INST_INIT(n)                                                                          \
    struct kscan_gpio_irq_callback_##n {                                                           \
        struct COND_CODE_0(DT_INST_PROP(n, debounce_period), (k_work), (k_delayed_work)) * work;   \
        struct gpio_callback callback;                                                             \
        struct device *dev;                                                                        \
    };                                                                                             \
                                                                                                   \
    static struct kscan_gpio_irq_callback_##n irq_callbacks_##n[INST_INPUT_LEN(n)];                \
                                                                                                   \
    struct kscan_gpio_config_##n {                                                                 \
        struct kscan_gpio_item_config rows[INST_MATRIX_ROWS(n)];                                   \
        struct kscan_gpio_item_config cols[INST_DEMUX_COLS(n)];                                    \
    };                                                                                             \
                                                                                                   \
    struct kscan_gpio_data_##n {                                                                   \
        kscan_callback_t callback;                                                                 \
        /* Only insert code if POLLING = True */                                                   \
        IF_KSCAN_POLLED((struct k_timer poll_timer;), ())                                          \
        struct COND_CODE_0(DT_INST_PROP(n, debounce_period), (k_work), (k_delayed_work)) work;     \
                                                                                                   \
        /* TODO: change to use total-cols*/                                                        \
        bool matrix_state[INST_MATRIX_ROWS(n)][INST_MATRIX_COLS(n)];                               \
        struct device *rows[INST_MATRIX_ROWS(n)];                                                  \
        struct device *cols[INST_DEMUX_COLS(n)];                                                   \
        struct device *dev;                                                                        \
    };                                                                                             \
    /* IO/GPIO SETUP */                                                                            \
    /* gpio_input_devices are PHYSICAL IO devices */                                               \
    static struct device **kscan_gpio_input_devices_##n(struct device *dev) {                      \
        struct kscan_gpio_data_##n *data = dev->driver_data;                                       \
        /* If row2col, rows = outputs & cols = inputs */                                           \
        return IF_IS_ROW2COL(n, (data->cols), (data->rows));                                       \
    }                                                                                              \
                                                                                                   \
    static const struct kscan_gpio_item_config *kscan_gpio_input_configs_##n(struct device *dev) { \
        const struct kscan_gpio_config_##n *cfg = dev->config_info;                                \
        /* If row2col, rows = outputs & cols = inputs */                                           \
        return IF_IS_ROW2COL(n, (cfg->cols), (cfg->rows));                                         \
        ;                                                                                          \
    }                                                                                              \
    /* gpio_output_devices are PHYSICAL IO devices */                                              \
    static struct device **kscan_gpio_output_devices_##n(struct device *dev) {                     \
        struct kscan_gpio_data_##n *data = dev->driver_data;                                       \
        /* If row2col, rows = outputs & cols = inputs */                                           \
        return IF_IS_ROW2COL(n, (data->rows), (data->cols));                                       \
    }                                                                                              \
                                                                                                   \
    static const struct kscan_gpio_item_config *kscan_gpio_output_configs_##n(                     \
        struct device *dev) {                                                                      \
        const struct kscan_gpio_config_##n *cfg = dev->config_info;                                \
        /* If row2col, rows = outputs & cols = inputs */                                           \
        return IF_IS_ROW2COL(n, (cfg->rows), (cfg->cols));                                         \
    }                                                                                              \
    /* INTERRUPT SETUP */                                                                          \
    /* Only insert code if POLLING = False */                                                      \
    IF_KSCAN_POLLED(                                                                               \
        (), (                                                                                      \
                static int kscan_gpio_enable_interrupts_##n(struct device *dev) {                  \
                    return kscan_gpio_config_interrupts(kscan_gpio_input_devices_##n(dev),         \
                                                        kscan_gpio_input_configs_##n(dev),         \
                                                        INST_INPUT_LEN(n), GPIO_INT_LEVEL_ACTIVE); \
                } static int kscan_gpio_disable_interrupts_##n(struct device *dev) {               \
                    return kscan_gpio_config_interrupts(kscan_gpio_input_devices_##n(dev),         \
                                                        kscan_gpio_input_configs_##n(dev),         \
                                                        INST_INPUT_LEN(n), GPIO_INT_DISABLE);      \
                }))                                                                                \
    /* POLLING SETUP */ /* Only insert code if POLLING = True */                                   \
    IF_KSCAN_POLLED((static void kscan_gpio_timer_handler(struct k_timer *timer) {                 \
                        struct kscan_gpio_data_##n *data =                                         \
                            CONTAINER_OF(timer, struct kscan_gpio_data_##n, poll_timer);           \
                        k_work_submit(&data->work.work);                                           \
                    }),                                                                            \
                    ())                                                                            \
                                                                                                   \
    /* Set the state of all output GPIOs */                                                        \
    static void kscan_gpio_set_output_state_##n(struct device *dev, int value) {                   \
        int err;                                                                                   \
        for (int i = 0; i < INST_DEMUX_COLS(n); i++) {                                             \
            struct device *in_dev = kscan_gpio_output_devices_##n(dev)[i];                         \
            const struct kscan_gpio_item_config *cfg = &kscan_gpio_output_configs_##n(dev)[i];     \
            if ((err = gpio_pin_set(in_dev, cfg->pin, value))) {                                   \
                LOG_DBG("FAILED TO SET OUTPUT %d to %d", cfg->pin, err);                           \
            }                                                                                      \
        }                                                                                          \
    }                                                                                              \
                                                                                                   \
    /* Update the matrix state variable at the indices i,o */                                      \
    /* TODO: change to use total-cols*/                                                            \
    static void kscan_gpio_set_matrix_state_##n(                                                   \
        bool state[INST_MATRIX_ROWS(n)][INST_MATRIX_COLS(n)], u32_t input_index,                   \
        u32_t output_index, bool value) {                                                          \
        state[IF_IS_ROW2COL(n, (output_index), (input_index))]                                     \
             [IF_IS_ROW2COL(n, (input_index), (output_index))] = value;                            \
    }                                                                                              \
                                                                                                   \
    /* Read the state of the input GPIOs */                                                        \
    /* This is the core matrix_scan func */                                                        \
    static int kscan_gpio_read_##n(struct device *dev) {                                           \
        bool submit_follow_up_read = false;                                                        \
        struct kscan_gpio_data_##n *data = dev->driver_data;                                       \
        /* TODO: change to use total-cols!*/                                                       \
        static bool read_state[INST_MATRIX_ROWS(n)][INST_MATRIX_COLS(n)];                          \
        /* Disable our interrupts temporarily while we scan, to avoid       */                     \
        /* re-entry while we iterate columns and set them active one by one */                     \
        /* to get pressed state for each matrix cell.                       */                     \
        /* TODO: Only needed during interrupt mode, but still won't work right... */               \
        kscan_gpio_set_output_state_##n(dev, 0);                                                   \
        int err;                                                                                   \
        for (int o = 0; o < INST_OUTPUT_LEN(n); o++) {                                             \
            /* Iterate over bits and set GPIOs accordingly */                                      \
            for (u8_t bit = 0; bit < INST_DEMUX_COLS(n); bit++) {                                  \
                u8_t state = (o & (0b1 << bit)) >> bit;                                            \
                struct device *out_dev = kscan_gpio_output_devices_##n(dev)[bit];                  \
                const struct kscan_gpio_item_config *out_cfg =                                     \
                    &kscan_gpio_output_configs_##n(dev)[bit];                                      \
                if ((err = gpio_pin_set(out_dev, out_cfg->pin, state))) {                          \
                    LOG_DBG("FAILED TO SET OUTPUT %d to %d", out_cfg->pin, err);                   \
                }                                                                                  \
            }                                                                                      \
                                                                                                   \
            for (int i = 0; i < INST_INPUT_LEN(n); i++) {                                          \
                /* Get the input device (port) */                                                  \
                struct device *in_dev = kscan_gpio_input_devices_##n(dev)[i];                      \
                /* Get the input device config (pin) */                                            \
                const struct kscan_gpio_item_config *in_cfg =                                      \
                    &kscan_gpio_input_configs_##n(dev)[i];                                         \
                kscan_gpio_set_matrix_state_##n(read_state, i, o,                                  \
                                                gpio_pin_get(in_dev, in_cfg->pin) > 0);            \
            }                                                                                      \
        }                                                                                          \
        /* Set all our outputs as active again. */                                                 \
        /* TODO: Only needed during interrupt mode, but still won't work right... */               \
        kscan_gpio_set_output_state_##n(dev, 1);                                                   \
        for (int r = 0; r < INST_MATRIX_ROWS(n); r++) {                                            \
            for (int c = 0; c < INST_MATRIX_COLS(n); c++) {                                        \
                bool pressed = read_state[r][c];                                                   \
                submit_follow_up_read = (submit_follow_up_read || pressed);                        \
                if (pressed != data->matrix_state[r][c]) {                                         \
                    LOG_DBG("Sending event at %d,%d state %s", r, c, (pressed ? "on" : "off"));    \
                    data->matrix_state[r][c] = pressed;                                            \
                    data->callback(dev, r, c, pressed);                                            \
                }                                                                                  \
            }                                                                                      \
        }                                                                                          \
        if (submit_follow_up_read) {                                                               \
            COND_CODE_0(DT_INST_PROP(n, debounce_period), ({ k_work_submit(&data->work); }), ({    \
                            k_delayed_work_cancel(&data->work);                                    \
                            k_delayed_work_submit(&data->work, K_MSEC(5));                         \
                        }))                                                                        \
        } else {                                                                                   \
            IF_KSCAN_POLLED((), (kscan_gpio_enable_interrupts_##n(dev);))                          \
        }                                                                                          \
        return 0;                                                                                  \
    }                                                                                              \
                                                                                                   \
    static void kscan_gpio_work_handler_##n(struct k_work *work) {                                 \
        struct kscan_gpio_data_##n *data = CONTAINER_OF(work, struct kscan_gpio_data_##n, work);   \
        kscan_gpio_read_##n(data->dev);                                                            \
    }                                                                                              \
                                                                                                   \
    static void kscan_gpio_irq_callback_handler_##n(struct device *dev, struct gpio_callback *cb,  \
                                                    gpio_port_pins_t pin) {                        \
        struct kscan_gpio_irq_callback_##n *data =                                                 \
            CONTAINER_OF(cb, struct kscan_gpio_irq_callback_##n, callback);                        \
        IF_KSCAN_POLLED((), (kscan_gpio_disable_interrupts_##n(data->dev);))                       \
        COND_CODE_0(DT_INST_PROP(n, debounce_period), ({ k_work_submit(data->work); }), ({         \
                        k_delayed_work_cancel(data->work);                                         \
                        k_delayed_work_submit(data->work,                                          \
                                              K_MSEC(DT_INST_PROP(n, debounce_period)));           \
                    }))                                                                            \
    }                                                                                              \
                                                                                                   \
    static struct kscan_gpio_data_##n kscan_gpio_data_##n = {                                      \
        .rows = {[INST_MATRIX_ROWS(n) - 1] = NULL}, .cols = {[INST_DEMUX_COLS(n) - 1] = NULL}};    \
                                                                                                   \
    /* KSCAN API configure function */                                                             \
    static int kscan_gpio_configure_##n(struct device *dev, kscan_callback_t callback) {           \
        LOG_DBG("KSCAN API configure");                                                            \
        struct kscan_gpio_data_##n *data = dev->driver_data;                                       \
        if (!callback) {                                                                           \
            return -EINVAL;                                                                        \
        }                                                                                          \
        data->callback = callback;                                                                 \
        LOG_DBG("Configured GPIO %d", n);                                                          \
        return 0;                                                                                  \
    };                                                                                             \
                                                                                                   \
    /* KSCAN API enable function */                                                                \
    static int kscan_gpio_enable_##n(struct device *dev) {                                         \
        LOG_DBG("KSCAN API enable");                                                               \
        /* TODO: change back to 10ms or less */                                                    \
        IF_KSCAN_POLLED((struct kscan_gpio_data_##n *data = dev->driver_data;                      \
                         k_timer_start(&data->poll_timer, K_MSEC(25), K_MSEC(25)); return 0;),     \
                        (int err = kscan_gpio_enable_interrupts_##n(dev);                          \
                         if (err) { return err; } return kscan_gpio_read_##n(dev);))               \
    };                                                                                             \
                                                                                                   \
    /* KSCAN API disable function */                                                               \
    static int kscan_gpio_disable_##n(struct device *dev) {                                        \
        LOG_DBG("KSCAN API disable");                                                              \
        IF_KSCAN_POLLED((struct kscan_gpio_data_##n *data = dev->driver_data;                      \
                         k_timer_stop(&data->poll_timer); return 0;),                              \
                        (return kscan_gpio_disable_interrupts_##n(dev);))                          \
    };                                                                                             \
                                                                                                   \
    /* GPIO init function*/                                                                        \
    static int kscan_gpio_init_##n(struct device *dev) {                                           \
        k_msleep(1000);                                                                            \
        LOG_DBG("KSCAN GPIO init");                                                                \
        IF_KSCAN_POLLED((LOG_DBG("KSCAN running in POLLED mode.");),                               \
                        (LOG_DBG("KSCAN running in INTERRUPT mode.");))                            \
        struct kscan_gpio_data_##n *data = dev->driver_data;                                       \
        int err;                                                                                   \
        /* configure input devices*/                                                               \
        struct device **input_devices = kscan_gpio_input_devices_##n(dev);                         \
        for (int i = 0; i < INST_INPUT_LEN(n); i++) {                                              \
            const struct kscan_gpio_item_config *in_cfg = &kscan_gpio_input_configs_##n(dev)[i];   \
            input_devices[i] = device_get_binding(in_cfg->label);                                  \
            if (!input_devices[i]) {                                                               \
                LOG_ERR("Unable to find input GPIO device");                                       \
                return -EINVAL;                                                                    \
            }                                                                                      \
            err = gpio_pin_configure(input_devices[i], in_cfg->pin, GPIO_INPUT | in_cfg->flags);   \
            if (err) {                                                                             \
                LOG_ERR("Unable to configure pin %d on %s for input", in_cfg->pin, in_cfg->label); \
                return err;                                                                        \
            } else {                                                                               \
                LOG_DBG("Configured pin %d on %s for input", in_cfg->pin, in_cfg->label);          \
            }                                                                                      \
            irq_callbacks_##n[i].work = &data->work;                                               \
            irq_callbacks_##n[i].dev = dev;                                                        \
            gpio_init_callback(&irq_callbacks_##n[i].callback,                                     \
                               kscan_gpio_irq_callback_handler_##n, BIT(in_cfg->pin));             \
            err = gpio_add_callback(input_devices[i], &irq_callbacks_##n[i].callback);             \
            if (err) {                                                                             \
                LOG_ERR("Error adding the callback to the column device");                         \
                return err;                                                                        \
            }                                                                                      \
        }                                                                                          \
        /* configure output devices*/                                                              \
        struct device **output_devices = kscan_gpio_output_devices_##n(dev);                       \
        for (int o = 0; o < INST_DEMUX_COLS(n); o++) {                                             \
            const struct kscan_gpio_item_config *out_cfg = &kscan_gpio_output_configs_##n(dev)[o]; \
            output_devices[o] = device_get_binding(out_cfg->label);                                \
            if (!output_devices[o]) {                                                              \
                LOG_ERR("Unable to find output GPIO device");                                      \
                return -EINVAL;                                                                    \
            }                                                                                      \
            err = gpio_pin_configure(output_devices[o], out_cfg->pin,                              \
                                     GPIO_OUTPUT_ACTIVE | out_cfg->flags);                         \
            if (err) {                                                                             \
                LOG_ERR("Unable to configure pin %d on %s for output", out_cfg->pin,               \
                        out_cfg->label);                                                           \
                return err;                                                                        \
            } else {                                                                               \
                LOG_DBG("Configured pin %d on %s for output", out_cfg->pin, out_cfg->label);       \
            }                                                                                      \
        }                                                                                          \
        data->dev = dev;                                                                           \
                                                                                                   \
        IF_KSCAN_POLLED((k_timer_init(&data->poll_timer, kscan_gpio_timer_handler, NULL);), ())    \
                                                                                                   \
        (COND_CODE_0(DT_INST_PROP(n, debounce_period), (k_work_init), (k_delayed_work_init)))(     \
            &data->work, kscan_gpio_work_handler_##n);                                             \
        return 0;                                                                                  \
    }                                                                                              \
                                                                                                   \
    static const struct kscan_driver_api gpio_driver_api_##n = {                                   \
        .config = kscan_gpio_configure_##n,                                                        \
        .enable_callback = kscan_gpio_enable_##n,                                                  \
        .disable_callback = kscan_gpio_disable_##n,                                                \
    };                                                                                             \
                                                                                                   \
    static const struct kscan_gpio_config_##n kscan_gpio_config_##n = {                            \
        .rows = {UTIL_LISTIFY(INST_MATRIX_ROWS(n), _KSCAN_GPIO_ROW_CFG_INIT, n)},                  \
        .cols = {UTIL_LISTIFY(INST_DEMUX_COLS(n), _KSCAN_GPIO_COL_CFG_INIT, n)},                   \
    };                                                                                             \
                                                                                                   \
    DEVICE_AND_API_INIT(kscan_gpio_##n, DT_INST_LABEL(n), kscan_gpio_init_##n,                     \
                        &kscan_gpio_data_##n, &kscan_gpio_config_##n, APPLICATION,                 \
                        CONFIG_APPLICATION_INIT_PRIORITY, &gpio_driver_api_##n);

DT_INST_FOREACH_STATUS_OKAY(GPIO_INST_INIT)

#endif /* DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT) */