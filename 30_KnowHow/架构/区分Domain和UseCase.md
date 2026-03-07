> **“业务规则” 和 “业务流程” 是两件不同的事情。**

- **Domain：业务规则**
- **UseCase：业务流程**

# 一、先理解 Domain：业务规则

## **Domain 像物理定律**

比如：

```
力 = 质量 × 加速度
```

它不关心：

- 谁在用
- 什么场景
- 什么时间

它只是 **规则本身**。

### 再用你的双轴控制举例

Domain 只关心：

```
Axis 可以
    enable()
    disable()
    moveAbsolute()
    jog()
    stop()
```

以及：

```
Position
Velocity
AxisState
```

这些 **业务概念和规则**。

它不关心：

```
什么时候双轴一起启动
什么时候先启动X1再启动X2
什么时候检测同步
```

这些是 **流程问题**。感觉就像是 PLC 放出来的一个个控制电机的小功能。

# 二、再理解 UseCase：业务流程

UseCase 层解决的问题是：

> **用户想完成什么事情？**

它描述的是：

- **业务流程**

例如：

### UseCase 示例

```
双轴同步启动
回零
点动
绝对位置移动
```

这些都是：

> **用户目标**

例如：

**UseCase：双轴同步启动**

伪代码：

```
class SyncMoveUseCase {
public:
    void execute(Position p1, Position p2) {

        axis1.enable();
        axis2.enable();

        axis1.moveTo(p1);
        axis2.moveTo(p2);

        waitUntilBothFinished();
    }
};
```

这里描述的是：

**流程**

```
1 启用X1
2 启用X2
3 下发位置
4 等待完成
```

# 三、最简单的一句话总结

如果只记一句话：

| 层      | 解决的问题         |
| ------- | ------------------ |
| Domain  | **系统是什么**     |
| UseCase | **系统做什么事情** |

换句话说：

```
Domain = 名词
UseCase = 动词
```

举例：

### Domain

```
Axis
Motor
Position
Velocity
```

### UseCase

```
JogAxis
MoveAxis
SyncMove
HomeAxis
```

# 四、为什么要这样分层？

因为：

**业务规则要稳定**

**业务流程会变化**

举个现实例子。

### Domain（稳定）

轴一定有：

```
位置
速度
运动
停止
```

这些永远不会变。

------

### UseCase（经常变）

例如：

今天流程：

```
先启动X1
再启动X2
```

明天需求改为：

```
两轴同时触发
```

或者：

```
加同步误差检测
```

流程变了。

但 **Axis 的规则没变**。

# 五、一个非常关键的设计原则

**UseCase 调用 Domain**

但：

**Domain 不知道 UseCase 存在**

依赖方向：

```
UseCase
   ↓
Domain
```

Domain 永远是 **最核心、最稳定的一层**。