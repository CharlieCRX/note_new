负责CSAC相关的开发记录。从7月7号开始。

7月10号基本结束。

加入了新的接口：

| 功能描述              | URI 路径                     | 请求方式 | 请求内容类型       | 说明                                        |
| --------------------- | ---------------------------- | -------- | ------------------ | ------------------------------------------- |
| 打开/关闭驯服训练     | `/api/discipline/enable`     | POST     | `application/json` | 主动打开或关闭驯服训练                      |
| 启用铷钟为ADC时钟源   | `/api/device/ruclock_en`     | POST     | `application/json` | 设置ADC时钟源为内部时钟或铷钟外部时钟       |
| 查询铷钟ADC时钟源状态 | `/api/device/ruclock_status` | GET      | N/A                | 查询当前ADC时钟源是内部时钟还是铷钟外部时钟 |

UI流程

```
             ┌──────────────┐
             │   页面启动   │
             └──────┬───────┘
                    │
                    ▼
      GET /api/device/ruclock_status
                    │
        ┌───────────┴────────────┐
        │                        │
        │ 已启用铷钟             │ 未启用铷钟
        │                        │
        ▼                        ▼
进入下一步               显示"启用铷钟"按钮
                                │
                                ▼
          POST /api/device/ruclock_en
                                │
                                ▼
                     再次查询 ruclock_status
                                │
                                ▼
                     启用成功后进入下一步
                                │
                                ▼
        GET /api/discipline/can_start
                                │
               ┌────────────────┴───────────────┐
               │                                │
               │ 可以启动                        │ 不满足条件
               │                                │
               ▼                                ▼
      启用"开始驯服"按钮              显示不能训练原因
               │
               ▼
POST /api/discipline/enable(enable=true)
               │
               ▼
POST /api/discipline/start
               │
               ▼
        周期查询 status
               │
               │
      GET /api/discipline/status
               │
               ▼
     更新训练进度、状态、误差等
               │
               │
      用户点击停止训练
               │
               ▼
POST /api/discipline/enable(enable=false)
               │
               ▼
        停止轮询 status
```

