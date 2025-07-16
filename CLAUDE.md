# CLAUDE.md - 开发原则

1.  **测试驱动开发 (TDD)**：所有核心功能在编写前，都应先有测试思路。
2.  **遵循计划**：严格按照 `PLAN.md` 中的任务清单（checklist）逐步推进，一次只做一件事。
3.  **技术栈**：
    * 硬件: ESP32
    * 开发框架: ESP-IDF v5.4
    * 语言: C/C++, HTML, JavaScript
4.  **频繁提交**：每完成 `PLAN.md` 中的一个小任务，就进行一次 git 提交。
5.  **清晰沟通**：向 AI 助手下达指令时，必须明确、具体，避免模糊不清的请求。
6.  **会话管理**：在每次会话开始时，使用 `/load` 加载上下文；在会话结束时，更新 `memory-bank/activeContext.md` 和 `memory-bank/progress.md`。
