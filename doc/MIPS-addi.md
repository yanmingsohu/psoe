# 寄存器

REGISTER  NAME      USAGE
$0        $zero     常量0(constant value 0)
$1        $at       保留给汇编器(Reserved for assembler)
$2-$3     $v0-$v1   函数调用返回值(values for results and expression evaluation)
$4-$7     $a0-$a3   函数调用参数(arguments)
$8-$15    $t0-$t7   暂时的(或随便用的), 函数返回无需恢复
$16-$23   $s0-$s7   保存的(或如果用，需要SAVE/RESTORE的)(saved), 函数返回需要恢复
$24-$25   $t8-$t9   暂时的(或随便用的), 函数返回无需恢复
$26, 27   $k0, $k1  中断,异常处理程序参数
$28       $gp       全局指针(Global Pointer)
$29       $sp       堆栈指针(Stack Pointer)
$30       $fp(s8)   帧指针(Frame Pointer)
$31       $ra       返回地址(return address), 即调用方函数地址.


# 指令

https://blog.csdn.net/qq_41191281/article/details/85933985
https://blog.csdn.net/goodlinux/article/details/6731484
https://www.jianshu.com/p/ac2c9e7b1d8f


## 伪指示 [19]

* 指令（汇编程序生成多个指令以产生相同的结果）（伪指令没有操作码）

Call      Arg 1     Arg 2     Arg3                  Description
------------------------------------------------------------------------------
abs       rd        rs                  put the absolute value of rs into rd
b         label                         branch to label
beqz      rs        label               branch to label if (rs==0)
bge       rs        rt        label     branch to label if (rs>=rt)
blt       rs        rt        label     branch to label if (rs<rt)
bnez      rs        label               branch to label if (rs!=0)
la        rd        label               load address of word at label into rd
li        rd        number              load number into rd
move      rd        rs                  move rs to rd
mul       rd        rs        rt        rd = rs * rt
neg       rd        rs                  rd = - rs
not       rd        rs                  rs = bitwise logical negation of rd
rem       rd        rs        rt        rd = rs MOD rt
rol(ror)  rd        rs        rt        rd = rs rotated left(right) by rt
seq       rd        rs        rt        if (rs==rt) rd=1;else rd=0
sge       rd        rs        rt        if (rs>=rt) rd=1;else rd=0
sgt       rd        rs        rt        if (rs>rt) rd=1;else rd=0
sle       rd        rs        rt        if (rs<=rt) rd=1;else rd=0
sne       rd        rs        rt        if (rs!=rt) rd=1;else rd=0


## 格式

+-----------+-----------+------------+-----------+-------------+------------+
|           |           |            | Rd[15:11] | Shamt[10:6] | Funct[5:0] | R
|           | Rs[25:21] | Rt[20:16]  +-----------+-------------+------------+
| Op[31:26] |           |            |          Im[15:0]                    | I
|           +-----------+------------+--------------------------------------+         
|           |                  Addr[25:0]                                   | J
+-----------+---------------------------------------------------------------+


## R格式指令 [17]

* R格式指令为纯寄存器指令，所有的操作数（除移位外）均保存在寄存器中。Op字段均为0，使用funct字段区分指令

标记	op	    rs	            rt	            rd	              shamt	  funct
位数	31-26	  25-21	          20-16	          15-11	            10-6	  5-0
功能	操作符	源操作数寄存器1	源操作数寄存器2	目的操作数寄存器	位移量	操作符附加段

### 算数类指令 (6)

指令	op	    rs	rt	rd	shamt	funct	  功能
add	  000000	rs	rt	rd	00000	100000	rd=rs+rt
addu	000000	rs	rt	rd	00000	100001	rd=rs+rt（无符号数）
sub	  000000	rs	rt	rd	00000	100010	rd=rs-rt
subu	000000	rs	rt	rd	00000	100011	rd=rs+rt（无符号数）
slt	  000000	rs	rt	rd	00000	101010	rd=(rs<rt)?1:0
sltu	000000	rs	rt	rd	00000	101011	rd=(rs<rt)?1:0（无符号数）

### 逻辑类指令 (4)

指令	op	    rs	rt	rd	shamt	funct	  功能
and	  000000	rs	rt	rd	00000	100100	rd=rs&rt
or	  000000	rs	rt	rd	00000	100101	rd=rs|rt
xor	  000000	rs	rt	rd	00000	100110	rd=rs xor rd
nor	  000000	rs	rt	rd	00000	100111	rd=!(rs|rt)

### 位移类指令 (6)

指令	op	    rs	  rt	rd	shamt	funct	  功能
sll	  000000	00000	rt	rd	shamt	000000	rd=rt<<shamt
srl	  000000	00000	rt	rd	shamt	000010	rd=rt>>shamt
sra	  000000	00000	rt	rd	shamt	000011	rd=rt>>shamt（符号位保留）
sllv	000000	rs	  rt	rd	00000	000100	rd=rt<<rs
srlv	000000	rs	  rt	rd	00000	000110	rd=rt>>rs
srav	000000	rs	  rt	rd	00000	000111	rd=rt>>rs（符号位保留）

### 跳转指令 (1)
指令	op	    rs	rt	  rd	  shamt	funct	  功能
jr	  000000	rs	00000	00000	00000	001000	PC=rs


## I格式指令 [12]

* I格式指令为带立即数的指令，最多使用两个寄存器，同时包括了load/store指令。使用Op字段区分指令

标记	op	    rs	            rd	              im
位数	31-26	  25-21	          20-16	            15-0
功能	操作符	源操作数寄存器	目的操作数寄存器	立即数

### 算数指令 (4)

指令	op	    rs	rd	im	功能
addi	001000	rs	rd	im	rd=rs+im
addiu	001001	rs	rd	im	rd=rs+im（无符号数）
slti	001010	rs	rd	im	rd=(rs<im)?1:0
sltiu	001011	rs	rd	im	rd=(rs<im)?1:0（无符号数）

### 逻辑类指令 (3)

指令	op	    rs	rd	im	功能
andi	001100	rs	rd	im	rd=rs&im
ori	  001101	rs	rd	im	rd=rs|im
xori	001110	rs	rd	im	rd=rs xor im

### 载入类指令 (3)

指令	op	    rs	  rd	im	功能
lui	  001111	00000	rd	im	rt=im*65536
lw	  100011	rs	  rd	im	rt=memory[rs+im]
sw	  101011	rs	  rd	im	memory[rs+im]=rt

### 跳转类指令 (2)

指令	op	    rs	rd	im	功能
beq	  000100	rs	rd	im	PC=(rs==rt)?PC+4+im<<2:PC
bne	  000101	rs	rd	im	PC=(rs!=rt)?PC+4+im<<2:PC


## J格式 [2]

* J格式指令为长跳转指令，仅有一个立即数操作数。使用Op字段区分指令

位数	31-26	  25-0
功能	操作符	地址
j	    000010	addr	PC={(PC+4)[31,28],addr,00}
jal	  000011	addr	$31=PC;PC={(PC+4)[31,28],addr,00}
