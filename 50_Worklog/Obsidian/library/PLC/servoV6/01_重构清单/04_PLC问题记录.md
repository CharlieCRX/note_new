## 运动状态

当前逻辑：

```LD
IF NOT 使能轴控[i] THEN
    运动状态[i] := 0;
ELSIF 轴JogForward[i] THEN
    运动状态[i] := 2;
...
ELSE
    运动状态[i] := 1;
END_IF;
```

这会导致：

```
使能轴控=ON、MC_Power=OFF
→ 运动状态仍然等于1
```

与“1=电机实际使能”不一致。

建议改成：

```
IF NOT 轴Status[i] THEN
    运动状态[i] := 0;
ELSIF 轴JogForward[i] THEN
    运动状态[i] := 2;
ELSIF 轴JogBackward[i] THEN
    运动状态[i] := 3;
ELSIF 轴MoveDo[i] THEN
    运动状态[i] := 4;
ELSIF 轴MoveXDDo[i] THEN
    运动状态[i] := 5;
ELSE
    运动状态[i] := 1;
END_IF;
```

这样上位机才能可靠使用`运动状态`代替直接读取`MC_Power.Status`。