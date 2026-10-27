# elib-key

零动态内存分配的嵌入式按键检测库，基于双层状态机架构。

## 特性

- **短按检测** - 按键释放后触发短按事件
- **连续点击** - 可配置最大连击次数（双击、三击等），带超时窗口
- **长按检测** - 多级长按阈值，释放时落在阈值区间内触发
- **多击 + 长按并存** - 同一按键可同时配置连击与长按，长按终结序列
- **组合键** - 支持短按组合、长按组合、长按保持+短按三种模式
- **原始按下/松开回调** - 可注册（可为 NULL）的即时边沿回调，不受消费/无效判定抑制
- **长按逐档到点回调** - 按住期间每跨过一档阈值触发一次（可为 NULL），支持升级式交互
- **事件消费** - 组合键触发时自动抑制参与按键的驱动事件
- **零动态分配** - 纯静态内存，适合资源受限的嵌入式环境
- **C99 标准** - 无外部依赖

## 快速开始

最小可运行的单击检测：只用必需的回调。

```c
#include "elib_key.h"

static elib_key_ctx_t ctx;

/* 硬件抽象：返回引脚电平，1 = 按下，0 = 松开 */
static int read_pin(uint8_t key_id) {
    return HAL_GPIO_ReadPin(key_pins[key_id]);
}

/* 分类事件回调（必需）：短按/长按都在释放时在这里报到 */
static void on_event(uint8_t key_id, elib_key_event_t event,
                     uint8_t click_count, uint8_t threshold_idx) {
    if (event == ELIB_KEY_EVENT_SHORT_CLICK) {
        printf("key %d short click (x%d)\n", key_id, click_count);
    } else if (event == ELIB_KEY_EVENT_LONG_PRESS) {
        printf("key %d long press (level %d)\n", key_id, threshold_idx);
    }
}

/* 单阈值 {1000}：按住 <1000ms 短按；>=1000ms 判无效（没有长按）。
 * 需要长按见下方"长按"场景 */
static const uint16_t thresholds[] = {1000};
static const elib_key_item_cfg_t keys[] = {
    { .key_id = 0, .max_click = 1, .multi_click_window_ms = 0,
      .long_press_thresholds = thresholds, .threshold_count = 1 },
};

static elib_key_driver_cfg_t cfg = {
    .keys = keys, .key_count = 1,
    .read_level = read_pin,
    .on_event = on_event,
    /* .on_press / .on_release / .on_long_hold 可省略（零初始化即 NULL） */
};

int main(void) {
    elib_key_init(&ctx, &cfg);

    while (1) {
        elib_key_scan(&ctx, 10);   /* 建议固定 10ms 左右轮询 */
        HAL_Delay(10);
    }
}
```

## 快捷配置宏

`elib_key_macros.h` 提供一组配置模板宏：**数组元素个数全部自动推导**（不会数错阈值数/键数），且每个配置项一行。文件作用域使用（`static const` 全局表），因为 `ELIB_KEY_KEY` 内嵌 C99 文件作用域复合字面量：

```c
#include "elib_key.h"   /* 已包含 elib_key_macros.h */

/* ---- 单键条目 ---- */
static const elib_key_item_cfg_t keys[] = {
    ELIB_KEY_KEY(0, 2, 300, 1000, 3000),   /* id=0, 双击, 300ms窗, 长按档 */
    ELIB_KEY_KEY(1, 1, 0,   500),          /* id=1, 单击 + 一档长按 */
    ELIB_KEY_KEY_CLICKS_ONLY(2, 3, 300),   /* id=2, 纯三击, 无长按（大数兜底档） */
    ELIB_KEY_KEY_SIMPLE(3, thr_arr3),      /* 复用已声明的阈值数组 */
    ELIB_KEY_KEY_CLICKS(4, 2, 300, thr_arr4),
};

/* ---- 组合键条目 ---- */
static const uint8_t keys_ab[] = {0, 1};
static const uint8_t keys_a[]  = {0};
static const uint8_t keys_b[]  = {1};
static const elib_key_combo_cfg_t combos[] = {
    ELIB_KEY_COMBO_SHORT(0, keys_ab, 500),
    /* AB 长按组合：总手势窗 12000（两键按下 + 松开都要落在首键按下后
     * 12000ms 内）；共同按住窗口 {1000, 3000}，末档 3000 为封顶（按住
     * 超过 3000 再松手判无效）；无共同按住窗需求用 _LONG_BINARY */
    ELIB_KEY_COMBO_LONG(1, keys_ab, 12000, 1000, 3000),
    /* 窗口也可以复用已声明的数组（窗口计数自动推导） */
    /* ELIB_KEY_COMBO_LONG_SIMPLE(1, keys_ab, 12000, ab_windows), */
    ELIB_KEY_COMBO_HOLD_PRESS(2, keys_a, keys_b),  /* 无 window_ms 参数 */
};

/* ---- 顶层配置 ---- */
static const elib_key_driver_cfg_t drv =
    ELIB_KEY_DRIVER_CFG(keys, read_pin, on_event);          /* 仅必需回调 */
    /* 可选回调版: ELIB_KEY_DRIVER_CFG_FULL(keys, read_pin, on_event,
     *                                        on_press, on_release, on_long_hold) */
static const elib_key_linkage_cfg_t link =
    ELIB_KEY_LINKAGE_CFG(combos, on_combo);
```

| 宏 | 参数 | 说明 |
|----|------|------|
| `ELIB_KEY_KEY` | `(id, clicks, window_ms, ...阈值)` | 阈值内联可变参；末档为封顶值，纯多击时给大数（如 `60000`） |
| `ELIB_KEY_KEY_CLICKS_ONLY` | `(id, clicks, window_ms)` | 无长按意图，自动用不可达兜底档 `{65535}` |
| `ELIB_KEY_KEY_SIMPLE` | `(id, thr_arr)` | 单击键，复用已声明阈值数组 |
| `ELIB_KEY_KEY_CLICKS` | `(id, clicks, window_ms, thr_arr)` | 多击键，复用已声明阈值数组 |
| `ELIB_KEY_COMBO_SHORT` | `(id, keys_arr, 总手势窗)` | `combo_count` 自动推导 |
| `ELIB_KEY_COMBO_LONG` | `(id, keys_arr, 总手势窗, ...共同按住窗口)` | 窗口内联可变参，每个窗口到点触发 `on_combo_hold`；末档为封顶值 |
| `ELIB_KEY_COMBO_LONG_SIMPLE` | `(id, keys_arr, 总手势窗, win_arr)` | 复用已声明的窗口数组 |
| `ELIB_KEY_COMBO_LONG_BINARY` | `(id, keys_arr, 总手势窗)` | 无共同按住窗口的二值长按组合（见下） |
| `ELIB_KEY_COMBO_HOLD_PRESS` | `(id, hold_arr, press_arr)` | 两个表的计数都自动推导 |
| `ELIB_KEY_DRIVER_CFG(_FULL)` | ... | 驱动层顶层配置，`key_count` 自动推导 |
| `ELIB_KEY_LINKAGE_CFG` | `(combos_arr, combo_cb)` | `combo_count` 自动推导 |

限制与注意：

- **仅限文件作用域**（全局/静态表），不能在函数内动态构建（复合字面量在块作用域是栈生命周期）
- `ELIB_KEY_KEY` 的可变参数阈值必须**升序**，与裸写法同一约束
- 组合宏与枚举值同名（`ELIB_KEY_COMBO_SHORT` 既是枚举值又是函数式宏）：函数式宏只在带括号调用时展开，`.mode = ELIB_KEY_COMBO_SHORT` 处仍是枚举值，互不干扰
- 单键场景章节保留裸结构体写法，便于理解每个字段的含义；完整示例与实际工程建议直接用宏

## 事件与回调一览

| 回调 | 必需 | 触发时机 |
|------|:----:|----------|
| `on_event(key_id, event, clicks, level)` | 是 | 释放后分类结果：`SHORT_CLICK` / `LONG_PRESS` |
| `on_press(key_id)` | 否 | 按下沿，扫描到即触发（原始边沿） |
| `on_release(key_id)` | 否 | 松开沿（原始边沿，无效释放/被组合消费也触发） |
| `on_long_hold(key_id, level)` | 否 | 按住期间每跨过一档阈值（长按到点、升级式）；组合声明期内静默 |
| `on_combo(combo_id, mode)` | 否 | 组合匹配成功/释放触发（联动层） |
| `on_combo_hold(combo_id, level)` | 否 | 带窗口长按组合的共同按住时长每跨过一档窗口（到点、升级式）；`ELIB_KEY_LINKAGE_CFG` 生成的结构需手动补 `.on_combo_hold = ...` |

回调都可以是 NULL；回调内部**禁止再调用本库**（不可重入）。

## 单键场景

### 短按 + 多击（双击/三击…）

```c
/* 10000ms 仅是兜底——纯多击键通常不需要长按，阈值给大值即可 */
static const uint16_t thresholds[] = {10000};
static const elib_key_item_cfg_t keys[] = {
    { .key_id = 0, .max_click = 3, .multi_click_window_ms = 300,
      .long_press_thresholds = thresholds, .threshold_count = 1 },
};

static void on_event(uint8_t key_id, elib_key_event_t event,
                     uint8_t click_count, uint8_t threshold_idx) {
    if (event == ELIB_KEY_EVENT_SHORT_CLICK) {
        switch (click_count) {
        case 1: do_open_menu(); break;   /* 单击 */
        case 2: do_select();    break;   /* 双击 */
        case 3: do_back();      break;   /* 三击 */
        }
    }
}
```

行为：每次点击松手后进入 300ms 等待窗口攒连击；攒满 `max_click`(3) 立即触发；窗口超时按实击数触发（双击后就停手 → `SHORT_CLICK(2)`）。

### 多击与长按并存（同一按键）

```c
/* 长按区间 [1000, 3000)；3000ms 起为无效（末档是封顶值） */
static const uint16_t thresholds[] = {1000, 3000};
static const elib_key_item_cfg_t keys[] = {
    { .key_id = 0, .max_click = 2, .multi_click_window_ms = 300,
      .long_press_thresholds = thresholds, .threshold_count = 2 },
};

static void on_event(uint8_t key_id, elib_key_event_t event,
                     uint8_t click_count, uint8_t threshold_idx) {
    if (event == ELIB_KEY_EVENT_SHORT_CLICK) {
        if (click_count == 1) toggle_light();
        else if (click_count == 2) next_color();
    } else if (event == ELIB_KEY_EVENT_LONG_PRESS) {
        enter_setting_mode();      /* 按住 1~3 秒 */
    }
}
```

规则：任意一次释放落入长按区间，立即发 `LONG_PRESS` 并**丢弃尚在等待的连击**（点一下→长按住 → 只报长按，不会先报单击）。持时超过末档（3000ms）判无效、无事件——若希望"按多久都算长按"，把末档配成大数如 `{1000, 3000, 60000}`。

### 多窗口长按（释放型，一次按压报一次）

```c
/* <1000 短按；[1000,3000) 档0；[3000,5000) 档1；>=5000 无效 */
static const uint16_t thresholds[] = {1000, 3000, 5000};

/* 释放时 threshold_idx 告诉你落在第几档 */
static void on_event(...
    else if (event == ELIB_KEY_EVENT_LONG_PRESS) {
        if (threshold_idx == 0) open_quick_menu();
        else if (threshold_idx == 1) open_full_menu();
    }
```

### 长按逐档到点（按住不松，升级式交互）

`on_long_hold` 在持时跨过每一档阈值的瞬间触发，不需要等松手：

```c
 thresholds {1000, 3000, 5000}

 /* 按住 10 秒强制关机这类"越按越狠"的交互 */
static void on_long_hold(uint8_t key_id, uint8_t threshold_idx) {
    switch (threshold_idx) {
    case 0: show_progress_1s();   break;   /* 1s: 开始提示 */
    case 1: show_progress_3s();   break;   /* 3s: 加强提示 */
    case 2: show_progress_5s();   break;   /* 5s: 开始倒计时 */
    }
    /* 甚至可以做"每再加一档升级一次"的电动车长按调速 */
}
```

注意：跨过**最后一档**也会通知（哪怕这次释放之后被判无效）；与释放时的 `LONG_PRESS` 事件并存，二者按需取用。

### 原始按下/松开回调

按住/抬起的即时通知，常用于背光、指示灯、或配对长按-释放间的手势：

```c
static void on_press(uint8_t key_id) {
    backlight_on(key_id);          /* 手指碰到立刻亮，不必等分类 */
}

static void on_release(uint8_t key_id) {
    backlight_off(key_id);
    /* 注意：无论这次释放最终是短按、长按还是被判无效，on_release 都会触发 */
}

cfg.on_press = on_press;
cfg.on_release = on_release;
```

## 组合键（可选联动层）

组合键需要额外初始化联动层；不初始化时按键仍然工作（含单手势互斥仲裁，开销只是每扫描几个位测试）：

```c
static const uint8_t combo_keys_ab[] = {0, 1};
static const uint8_t combo_keys_a[]  = {0};
static const uint8_t combo_keys_b[]  = {1};

static const elib_key_combo_cfg_t combos[] = {
    /* 优先级 = 配置数组顺序，先匹配先赢 */
    { .mode = ELIB_KEY_COMBO_SHORT,       /* A、B 都"短按" */
      .combo_keys = combo_keys_ab, .combo_count = 2,
      .window_ms = 500, .combo_id = 0 },
    { .mode = ELIB_KEY_COMBO_HOLD_PRESS,  /* 按住 A + 单击 B */
      .hold_keys = combo_keys_a,  .hold_count = 1,
      .press_keys = combo_keys_b, .press_count = 1,
      .combo_id = 1 },
};

static void on_combo(uint8_t combo_id, elib_key_combo_mode_t mode) {
    if (combo_id == 0) lock_device();         /* A+B 同按 */
    else if (combo_id == 1) capture_screenshot();  /* A 挂住 + 拍 B */
}

static const elib_key_linkage_cfg_t link = {
    .combos = combos, .combo_count = 2, .on_combo = on_combo,
};

elib_key_init(&ctx, &cfg);                 /* cfg 见"快速开始" */
elib_key_linkage_init(&ctx, &link);        /* 在 elib_key_init 之后调用 */
```

三种模式与触发条件（**手势互斥语义**见下）：

| 模式 | 触发条件 | 使用要点 |
|------|----------|----------|
| `COMBO_SHORT` | 所有参与键的"待分类短按"**同时在场**：全员同扫描松开，或某键已在自己的连击等待窗里；且**全员按下都在总手势窗内**（超窗只使该条组合不匹配，各键自己的手势照常） | 想容忍先后按下，给参与键配 `multi_click_window_ms > 0`（建议 100~300） |
| `COMBO_LONG`（带窗口） | **全员按下**（且按下跨度在总手势窗内）的瞬间声明（该时刻 = 共同按住计时起点 + 总窗倒计时起点）；共同按住每跨过一档窗口 → `on_combo_hold(combo_id, level)` 到点；**全员松开**时若仍在总窗内，按共同按住时长分类：落在某档窗口内 → 触发组合，**≥ 末档或总窗已超时** → 判无效什么都不发 | 与单键长按完全同构（多窗口 + 到点回调 + 释放分类）；声明后参与键的单键输出整个手势期间被取消（不回放） |
| `COMBO_LONG`（无窗口） | 所有参与键都按住且各自超过首档阈值 → 声明；**全员松开**时若仍在总窗内触发（二值，无档位） | 旧版兼容行为；声明点晚于按下点，所以总手势窗必须大于首档阈值（只想约束松手不必约束按下跨度时，`window_ms` 填 `0` 关闭总窗最简单） |
| `COMBO_HOLD_PRESS` | `hold_keys` 都按住且超过各自首档阈值，且 `press_keys` 的短按被捕获；**不参与总手势窗**（hold 键本就先于 press 键按下） | 触发后 hold 键保持静默直到全部松开（不会误触发其长按/短按） |

注意：`window_ms` 字段是**总手势窗**——从首个参与键按下计时，所有参与键的按下和（对 `COMBO_LONG`）松开都要落在这段时间窗内；`window_ms == 0` 表示不启用总窗约束。超窗的组合只是**失去匹配资格**，先到键区的手势照常运行；优先级 = 配置数组顺序，先匹配先锁定。

### 带窗口长按组合详解

与单键长按（`on_long_hold` + 释放分类）完全同构，只是计时对象从单键 hold 时间变为**共同按住时长**；此外 `window_ms` 给出**总手势窗**（从首个参与键按下计时）：

- **按下窗**：全员按下必须发生在总手势窗内（以最后一个参与键按下那一刻校验此前跨度）；超窗 → 该组合不声明，先按键的单键手势**不被劫持**，迟到键按无效键忽略
- **起点**：最后一个参与键按下的瞬间（全员按下该刻）。先按的键单独按了多久**不计入**共同按住计时；最后的键按下时，共同计时从 0 开始
- **到点**：共同按住每跨过一档窗口（`long_press_thresholds` 数组）触发一次 `on_combo_hold(combo_id, level)`（level 从 0 起）；需要 `linkage_cfg` 里补上可选的 `.on_combo_hold`
- **释放窗**：全员松开也必须发生在总手势窗内（从首个参与键按下算总账）；一旦超窗，手势即被"定罪"——到点回调整体静默，保持声明至全员松手，结算时**什么都不发**
- **释放分类**：全员松开且总窗仍活着时按"共同按住时长"分类——落在 `[窗口 k, 窗口 k+1)` 内 → `on_combo(combo_id, COMBO_LONG)`；**≥ 末档窗口 → 判无效**（组合与参与键的单键输出全部不发）
- **参与键取消规则**与二值版一致：声明那一刻起，所有参与键的单键长按分类与 `on_long_hold` 升级静默，且**不回放**（想要单键动作必须全松开后重新按）

示例（A 长按复位机器、AB 长按进工厂模式，总手势窗 12000、共同按住窗口 {3000, 5000}）：

```
A 按下───────► B 按下 ● 起点由此算（共同按住 = 0）
               │
               ├ 共同按住 3000 → on_combo_hold(组合, 0)
               │
A 先松手 ──────┤ （计时继续，直到最后一人松手）
               │
B 松手 ────────┤ 共同按住落在 [3000, 5000) → on_combo 触发工厂模式
```

A 独自按住的阶段（B 加入前）A 自己的到点回调照常触发；B 加入进入组合后，A 不再有任何单键输出。

## 手势互斥（单通道语义）

库强制保证**同一时刻只存在一种手势**，任何时刻只有一种输出（单个按键的结果，或一个组合），不存在两种情况同时发生：

- **互斥**：某手势进行中（按键按住/连击等待/组合已声明）时，新按下的键只有两条路——能和当前按键集合构成某条组合**且按下跨度不超其总手势窗** → 手势**升级为该组合**，此后所有参与键的单键结果（短按/连击/长按/升级回调）被取消；无组合匹配（或超窗失格）→ 该键被**忽略**（松手也不产生任何分类事件，但不会影响进行中的手势）
- **同扫描同时按下且无组合匹配**：全部无效（宁可不发）
- **声明后反悔**：想要单键动作，必须全松开后重新单独按
- `on_press`/`on_release` 原始回调是**未分类观察者**，任何时候都正常触发，不受上述规则抑制

安全场景示例（A 长按复位机器、B 长按复位滤芯、A+B 长按进工厂模式）：按住 A 途中手滑碰到 B → B 被忽略，A 的复位照常执行；两键都按住且都过首档 → 声明进组合，A/B 的单键动作彻底取消，全员松手只报一次工厂模式。

补充：

- 组合声明/触发后参与键的**分类事件被消费**（不重复报）；但 `on_press`/`on_release` 原始回调不受影响
- 组合可以**重复触发**：触发后通道释放，可再次匹配
- **被"忽略"的键**：分类事件与其 `on_long_hold` 升级回调静默，直到它松手；下次按下才可能是新手势

## 完整示例（双键，含组合与全部回调）

```c
#include "elib_key.h"
#include <stdio.h>

static elib_key_ctx_t ctx;

static int read_pin(uint8_t key_id) {
    /* 按工程实际替换 HAL 读取 */
    return HAL_GPIO_ReadPin(KEY_A_PIN + key_id);
}

static void on_press(uint8_t key_id)  { backlight_on(key_id);  }
static void on_release(uint8_t key_id){ backlight_off(key_id); }

static void on_event(uint8_t key_id, elib_key_event_t event,
                     uint8_t click_count, uint8_t threshold_idx) {
    if (event == ELIB_KEY_EVENT_SHORT_CLICK && key_id == 0) {
        if (click_count == 1)      toggle_led();
        else if (click_count >= 2) run_selftest();
    } else if (event == ELIB_KEY_EVENT_LONG_PRESS) {
        save_settings();
    }
}

static void on_long_hold(uint8_t key_id, uint8_t threshold_idx) {
    if (key_id == 0 && threshold_idx >= 1) {
        show_force_reset_warning();      /* 按住超 3s 提示重置 */
    }
}

static void on_combo(uint8_t combo_id, elib_key_combo_mode_t mode) {
    if (combo_id == 0) enter_factory_mode();
    else if (combo_id == 1) restore_defaults();
}

/* A 键：多击 + 长按并存；B 键：仅单击 */
static const elib_key_item_cfg_t keys[] = {
    ELIB_KEY_KEY(0, 2, 300, 1000, 3000),
    ELIB_KEY_KEY(1, 1, 0,   1000),
};

static const uint8_t combo_keys_ab[] = {0, 1};
static const uint8_t combo_keys_a[]  = {0};
static const uint8_t combo_keys_b[]  = {1};
static const elib_key_combo_cfg_t combos[] = {
    ELIB_KEY_COMBO_SHORT(0, combo_keys_ab, 500),
    ELIB_KEY_COMBO_HOLD_PRESS(1, combo_keys_a, combo_keys_b),
};

static const elib_key_driver_cfg_t drv =
    ELIB_KEY_DRIVER_CFG_FULL(keys, read_pin, on_event,
                             on_press, on_release, on_long_hold);

static const elib_key_linkage_cfg_t link =
    ELIB_KEY_LINKAGE_CFG(combos, on_combo);

int main(void) {
    elib_key_init(&ctx, &drv);
    elib_key_linkage_init(&ctx, &link);

    while (1) {
        elib_key_scan(&ctx, 10);
        HAL_Delay(10);       /* 平台相关延迟 */
    }
}
```

## 行为细节与注意事项

- **事件模型**：短按/长按/全部分类事件都在**松开时**触发；`on_press`/`on_release`/`on_long_hold` 是唯一即时回调
- **长按末档封顶**：持时 >= `thresholds[threshold_count-1]` 判无效。想要逐级升级不被末档截断，用 `on_long_hold`，或配一个大数兜底档
- **无消抖**：本库不处理机械抖动。抖动（常见 5~20ms）会产生多余的 press/release 甚至假连击，请在硬件层面（RC 滤波）或读取回调层面做消抖
- **手势互斥是硬约束**：同一时刻只有一种手势（见"手势互斥"节）；依赖并发独立按键的应用不适合本库
- **全员松开 = 全复位**：无论之前发生过什么（组合声明、声明失败、判无效、键被忽略），只要所有键都松开并经过一次扫描，库状态完全干净（claimed/follower/owner/手势槽全部归零）；某键被机械卡住按住期间其他键持续互斥，该键一松手全系统自愈
- **声明期内的点击不外漏**：组合参与键在 declared 手势内的快速点击会被整体作废，不会转化成后续连击等待窗里的 `SHORT_CLICK` 事件
- **窗口末档给 `65535` = 释放分类永不触发**：`on_combo_hold` 到点回调照常工作，但松手永远落在"超末档无效"分支，等效纯到点用法
- **计时容量**：单键计时用 `uint16_t`，持时/间隔上限约 65.5 秒；单次 `elapsed_ms` 最大 65535，轮询周期请远小于此
- **配置数组**：所有配置可放 `const`/flash；阈值数组必须升序且 1~`ELIB_KEY_MAX_THRESHOLDS` 个
- **详细行为规范**：`docs/superpowers/specs/2026-05-12-elib-key-design.md`

## 编译时限制

可在包含头文件前定义以下宏来调整限制：

```c
#define ELIB_KEY_MAX_KEYS        16  /* 最大按键数 */
#define ELIB_KEY_MAX_COMBOS      8   /* 最大组合键数 */
#define ELIB_KEY_MAX_THRESHOLDS  4   /* 每键最大长按阈值数 */
#define ELIB_KEY_MAX_COMBO_KEYS  4   /* 每组合最大参与键数 */
```

## 目录结构

```
elib-key/
├── include/
│   ├── elib_key.h          # 公共 API 头文件（umbrella）
│   ├── elib_key_err.h      # 错误码定义
│   ├── elib_key_macros.h   # 快捷配置宏
│   └── elib_key_types.h    # 类型定义和编译时限制
├── src/
│   ├── elib_key_core.h     # 内部头文件
│   ├── elib_key_core.c     # 驱动层实现（单键状态机）
│   └── elib_key_linkage.c  # 联动层实现（组合键检测）
├── test/
│   └── test_elib_key.c     # 单元测试
├── docs/                   # 设计规范与实施计划
├── scripts/                # 构建和部署脚本
├── LICENSE
└── README.md
```

## 编译测试

```bash
gcc -std=c99 -Wall -Wextra -Iinclude -o test_elib_key test/test_elib_key.c src/elib_key_core.c src/elib_key_linkage.c && ./test_elib_key
```

集成到工程：拷贝 `include/` 与 `src/` 下的 7 个文件即可，无外部依赖。

## 许可证

MIT License
