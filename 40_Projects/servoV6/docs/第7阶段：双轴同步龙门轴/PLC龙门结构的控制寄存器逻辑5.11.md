## PLC龙门结构的控制寄存器逻辑

现在梳理 PLC 中专门负责龙门结构的控制寄存器逻辑。

这些逻辑均由 PLC 的 LD 语言规定的。

## 使能轴X电机

```
// 使能电机
轴X1使能 := 使能轴X电机 AND NOT 设备急停;
轴X2使能 := 使能轴X电机 AND NOT 设备急停;

轴Y使能 := 使能轴Y电机 AND NOT 设备急停;
轴Z使能 := 使能轴Z电机 AND NOT 设备急停;
轴R使能 := 使能轴R电机 AND NOT 设备急停;
```

![image-20260511143831543](C:\Users\Administrator\AppData\Roaming\Typora\typora-user-images\image-20260511143831543.png)

![image-20260511143846357](C:\Users\Administrator\AppData\Roaming\Typora\typora-user-images\image-20260511143846357.png)

## 轴X联动使能

![image-20260511144039216](C:\Users\Administrator\AppData\Roaming\Typora\typora-user-images\image-20260511144039216.png)

```plc
// 轴X 绝对 与 相对 定位 接口 必须在X1 X2联动的情况下才能执行
轴X暂停沿触发(CLK := 轴X软件限位状态,Q => 轴X暂停触发结果);
IF  轴X软件限位状态 THEN 
	轴X1MoveDo := FALSE;
	轴X1MoveXDDo := false;
END_IF;

if 齿轮比到达 AND 
	轴X联动使能 AND NOT 解除关联完成 AND 轴X1CurSpeed = 0.0 AND 
	NOT 轴X1MoveDo AND NOT 轴X1MoveXDDo and not 轴X1HomeDo and NOT 轴X1ZeroDo and 
	NOT 轴X1JogForward AND NOT 轴X1JogBackward AND NOT X轴急停触发 THEN 
	
	IF not 轴X软件限位状态 and 
		轴X绝对定位距离 <> 轴X1CurPosition AND 轴X绝对定位触发 AND NOT 轴X相对定位触发 then
		轴X1MovePosition := 轴X绝对定位距离;
		轴X1MoveDo := TRUE;
	END_IF;
	轴X相对定位沿触发(CLK := 轴X相对定位触发,Q => 轴X相对定位沿触发结果);
	IF not 轴X软件限位状态 and 
		轴X相对定位距离 <> 0.0 AND not 轴X绝对定位触发 and 轴X相对定位沿触发结果 then
		轴X1MoveXDPosition := 轴X相对定位距离;
		轴X1MoveXDDo := TRUE;
	END_IF;
	
END_IF;

```

```plc
//轴 X2、X1 单独绝对定位控制

if NOT 轴X联动使能 THEN 
	IF 轴X1CurSpeed = 0.0 AND NOT 轴X1MoveDo AND NOT 轴X1MoveXDDo and not 轴X1HomeDo and NOT 轴X1ZeroDo and 
	NOT 轴X1JogForward AND NOT 轴X1JogBackward THEN 
		IF 轴X1单独绝对定位距离 <> 轴X1CurPosition AND 轴X1单独绝对定位触发 then
			轴X1MovePosition := 轴X1单独绝对定位距离;
			轴X1MoveDo := TRUE;
		END_IF;
	END_IF;
	
	IF 轴X2CurSpeed = 0.0 AND NOT 轴X2MoveDo AND NOT 轴X2MoveXDDo and not 轴X2HomeDo and NOT 轴X2ZeroDo and 
	NOT 轴X2JogForward AND NOT 轴X2JogBackward THEN 
		IF 轴X2单独绝对定位距离 <> 轴X2CurPosition AND 轴X2单独绝对定位触发 then
			轴X2MovePosition := 轴X2单独绝对定位距离;
			轴X2MoveDo := TRUE;
		END_IF;
	END_IF;
ELSE 
	轴X1单独绝对定位触发 := FALSE;
	轴X2单独绝对定位触发 := FALSE;
END_IF;

```

```plc
// 轴X 点动 接口
IF 轴X软件限位状态 THEN 	
	IF 轴X1CurPosition <= 轴X软件负限位 THEN  轴X1点动反转 := FALSE; END_IF;
	IF 轴X1CurPosition >= 轴X软件正限位 THEN  轴X1点动正转 := FALSE; end_if ;
END_IF;

if ((齿轮比到达 AND 
	轴X联动使能 AND NOT 解除关联完成)OR NOT 轴X联动使能) AND
	NOT 轴X1MoveDo AND NOT 轴X1MoveXDDo and not 轴X1HomeDo and NOT 轴X1ZeroDo AND 轴X1Status
	AND NOT X轴超差时间到 AND NOT X轴急停触发 THEN 
	轴X1JogBackward := 轴X1点动反转;
	轴X1JogForward := 轴X1点动正转;
END_IF;

IF NOT 齿轮比到达 AND NOT 轴X联动使能 AND NOT X轴超差时间到 AND 
	NOT 轴X2MoveDo AND NOT 轴X2MoveXDDo and not 轴X2HomeDo and NOT 轴X2ZeroDo AND 轴X2Status THEN 
	轴X2JogBackward := 轴X2点动反转;
	轴X2JogForward := 轴X2点动正转;
END_IF;
```

```plc
// 联合定位的时候，两轴的位置超差就需要紧急停机
if 齿轮比到达 AND 轴X联动使能 AND NOT 解除关联完成 THEN
	if ABS(轴X1CurPosition + 轴X2CurPosition) > 轴X超差阈值 THEN 
		X轴超差标志 := TRUE;
	ELSE 
		X轴超差标志 := FALSE;
	END_IF;
ELSE 
	X轴超差标志 := FALSE;
	轴X1E_Stop := FALSE;
end_if;

TONR(IN := X轴超差标志,PT := 20,R := ,Q => X轴超差时间到,ET => );
if X轴超差时间到 THEN
	X轴急停触发 := TRUE;
END_IF;
IF X轴急停触发 THEN 
	轴X绝对定位触发 := FALSE;
	轴X相对定位触发 := FALSE; 
	
	轴X1点动反转 := FALSE ;
	轴X1点动正转 := FALSE ;
	
	轴X1MoveDo := FALSE;
	轴X1MoveXDDo := FALSE;
	轴X1HomeDo := FALSE;
	轴X1ZeroDo := FALSE;
END_IF;


急停触发沿(CLK := X轴急停触发 OR 轴X软件限位状态,Q => 轴X1E_Stop);
```

```plc
IF 轴X状态显示 = 3 THEN 
	IF 轴X1指令错误 OR 轴X2指令错误 THEN 轴X报警代码 :=1; END_IF; // 指令执行报警
	IF 轴X1伺服错误码<>0 OR 轴X2伺服错误码<>0 THEN 轴X报警代码 :=2; END_IF; // 伺服报警
	IF 轴X1伺服DIStatus<>0 OR 轴X2伺服DIStatus<>0 OR 
		Axis_X1.bnslimit OR Axis_X1.bpslimit OR
		Axis_X2.bnslimit OR Axis_X2.bpslimit  THEN 
			轴X报警代码 :=3; 
			轴X联动使能 := FALSE;
	END_IF; // 限位报警
	IF X轴超差时间到 THEN 轴X报警代码 :=4; END_IF; // 超差报警
	
ELSE 
	轴X报警代码:=0;
END_IF;
```

## 轴X手动速度、轴X定位速度

```plc
// 联动加减速
关联加速度 := 2000.0;
关联减速度 := 2000.0;
解除关联减速度 := 2000.0;

// X轴速度关联 X1是主轴 X2是从轴
轴X1JogSpeed := 轴X手动速度;
轴X1MoveSpeed := 轴X定位速度;
轴X2MoveSpeed := 轴X1MoveSpeed ;
轴X2JogSpeed := 轴X1JogSpeed;

// Y轴速度关联
轴YJogSpeed := 轴Y手动速度;
轴YMoveSpeed := 轴Y定位速度;

// Z轴速度关联
轴ZJogSpeed := 轴Z手动速度;
轴ZMoveSpeed := 轴Z定位速度;

// Z轴速度关联
轴RJogSpeed := 轴R手动速度;
轴RMoveSpeed := 轴R定位速度;
```

```
// X轴速度关联 X1是主轴 X2是从轴
轴X1JogSpeed := 轴X手动速度;
轴X1MoveSpeed := 轴X定位速度;
轴X2MoveSpeed := 轴X1MoveSpeed ;
轴X2JogSpeed := 轴X1JogSpeed;
```

