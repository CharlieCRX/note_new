根据部署脚本和启动脚本的分析，你的 cm_server 配置如下：

- **端口**：**80**（来自 `cm_server.sh` 中的 `-a 0:80`）
- **设备 IP**：`10.1.2.151` / `10.1.2.152` / `10.1.2.154`（来自 `deploy.sh`）

---

## 直接用 curl 测试（在终端执行）

把你当前测试的那台设备的 IP 替换 `$IP` 即可，端口是 80 可以省略：

```bash
IP=10.1.2.151    # 改成你的设备 IP

# 1. 获取设备信息 (GET)
curl -s http://$IP/api/device/info

# 2. 同步时间 (GET)
curl -s http://$IP/api/device/sync_time

# 3. 查询能否启动驯服 (GET)
curl -s http://$IP/api/discipline/can_start

# 4. 启动驯服训练 (POST, 需要 JSON body)
curl -s -X POST http://$IP/api/discipline/start \
  -H "Content-Type: application/json" \
  -d '{"ns_threshold":100,"time_constant":100}'

# 5. 查询驯服状态 (GET)
curl -s http://$IP/api/discipline/status

# 6. 打开/关闭驯服 (POST, 需要 JSON body)
curl -s -X POST http://$IP/api/discipline/enable \
  -H "Content-Type: application/json" \
  -d '{"enable":true}'
```

如果想让 JSON 格式化输出，管道给 `python3 -m json.tool` 即可，例如：
```bash
curl -s http://10.1.2.151/api/device/info | python3 -m json.tool
```

---

## 在 Apifox / Postman 中测试

| 项目     | 值                  |
| -------- | ------------------- |
| Base URL | `http://10.1.2.151` |

新建 6 个请求：

| #    | 方法 | 路径                        | Body                                       |
| ---- | ---- | --------------------------- | ------------------------------------------ |
| 1    | GET  | `/api/device/info`          | 无                                         |
| 2    | GET  | `/api/device/sync_time`     | 无                                         |
| 3    | GET  | `/api/discipline/can_start` | 无                                         |
| 4    | POST | `/api/discipline/start`     | `{"ns_threshold":100,"time_constant":100}` |
| 5    | GET  | `/api/discipline/status`    | 无                                         |
| 6    | POST | `/api/discipline/enable`    | `{"enable":true}`                          |

POST 请求需要设置 Header：`Content-Type: application/json`

先试试最简单的 GET 请求 `curl http://10.1.2.151/api/device/info`，看能否返回设备信息的 JSON。