# CLAUDE.md - ESP32 Starter Journey Guidelines

> 本项目规范详见 [**AGENTS.md**](./AGENTS.md)。以下为快速参考：

- **主控与配置**: ESP32-WROOM-32E (8MB Flash + 2MB PSRAM), ST7789 1.69" LCD, CST816S Touch.
- **环境**: ESP-IDF v6.0.2 / FreeRTOS / LVGL v9.
- **关卡交付规范**:
  1. `main/app_main.c` 为当前关卡入口，保持清晰注释。
  2. 关卡验证成功后在 [`ESP32_小白入门实战学习计划.md`](./ESP32_小白入门实战学习计划.md) 勾选 `[x]`。
  3. 提交 Commit: `git commit -m "feat(level-<N>): ..."`
  4. 打 Tag 并推送: `git tag -a v<N>.0-level-<N> -m "..." && git push origin main --tags`
