# elib-key

零动态内存分配的嵌入式按键检测库，基于双层状态机架构。

## 特性

- **短按检测** - 按键释放后触发短按事件
- **连续点击** - 可配置最大连击次数（双击、三击等），带超时窗口
- **长按检测** - 可配置多级长按阈值，释放时在阈值范围内触发
- **组合键** - 支持短按组合、长按组合、长按保持+短按三种模式
- **事件消费** - 组合键触发时自动抑制参与按键的驱动事件
- **零动态分配** - 纯静态内存，适合资源受限的嵌入式环境
- **C99 标准** - 无外部依赖

## 目录结构

```
elib-key/
├── include/
│   ├── elib_key.h          # 公共 API 头文件
│   ├── elib_key_err.h      # 错误码定义
│   └── elib_key_types.h    # 类型定义和编译时限制
├── src/
│   ├── elib_key_core.h     # 内部头文件
│   ├── elib_key_core.c     # 驱动层实现（单键状态机）
│   └── elib_key_linkage.c  # 联动层实现（组合键检测）
├── test/
│   └── test_elib_key.c     # 单元测试
├── scripts/                # 构建和部署脚本
├── LICENSE
└── README.md
```

## API 参考

| 函数 | 说明 |
|------|------|
| `elib_key_init(ctx, cfg)` | 初始化驱动层 |
| `elib_key_deinit(ctx)` | 反初始化驱动层 |
| `elib_key_scan(ctx, elapsed_ms)` | 扫描所有按键，处理状态机和联动层 |
| `elib_key_linkage_init(ctx, cfg)` | 初始化联动层（可选） |
| `elib_key_linkage_deinit(ctx)` | 反初始化联动层 |

## 使用示例

```c
#include "elib_key.h"

static elib_key_ctx_t ctx;

/* 硬件读取回调 */
static int read_pin(uint8_t key_id) {
    return HAL_GPIO_ReadPin(key_pins[key_id]);
}

/* 事件回调 */
static void on_key_event(uint8_t key_id, elib_key_event_t event,
                          uint8_t click_count, uint8_t threshold_idx) {
    if (event == ELIB_KEY_EVENT_SHORT_CLICK) {
        printf("Key %d: %d clicks\n", key_id, click_count);
    } else if (event == ELIB_KEY_EVENT_LONG_PRESS) {
        printf("Key %d: long press (threshold %d)\n", key_id, threshold_idx);
    }
}

/* 按键配置 */
static const uint16_t thresholds[] = {1000, 3000};
static const elib_key_item_cfg_t keys[] = {
    { .key_id = 0, .max_click = 3, .multi_click_window_ms = 300,
      .long_press_thresholds = thresholds, .threshold_count = 2 },
};

static const elib_key_driver_cfg_t drv_cfg = {
    .keys = keys, .key_count = 1,
    .read_level = read_pin, .on_event = on_key_event,
};

int main(void) {
    elib_key_init(&ctx, &drv_cfg);

    while (1) {
        /* 每 10ms 调用一次 */
        elib_key_scan(&ctx, 10);
        delay_ms(10);
    }
}
```

## 编译测试

```bash
gcc -std=c99 -Wall -Wextra -Iinclude -o test_elib_key test/test_elib_key.c src/elib_key_core.c src/elib_key_linkage.c && ./test_elib_key
```

## 编译时限制

可在包含头文件前定义以下宏来调整限制：

```c
#define ELIB_KEY_MAX_KEYS        16  /* 最大按键数 */
#define ELIB_KEY_MAX_COMBOS      8   /* 最大组合键数 */
#define ELIB_KEY_MAX_THRESHOLDS  4   /* 每键最大长按阈值数 */
#define ELIB_KEY_MAX_COMBO_KEYS  4   /* 每组合最大参与键数 */
```

## 许可证

MIT License
