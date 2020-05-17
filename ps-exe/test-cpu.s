	.file	1 "test-cpu.c"

 # GNU C 2.7.2.3 [AL 1.1, MM 40] BSD Mips

 # Cc1 defaults:

 # Cc1 arguments (-G value = 8, Cpu = 3000, ISA = 1):
 # -mfp32 -mgp32 -quiet -dumpbase -membedded-data -mips1 -o

gcc2_compiled.:
__gnu_compiled_c:
	.rdata
	.align	2
$LC0:
	.ascii	"a= \000"
	.align	2
$LC1:
	.ascii	"b= \000"
	.align	2
$LC2:
	.ascii	"a+b= \000"
	.align	2
$LC3:
	.ascii	"a-b= \000"
	.align	2
$LC4:
	.ascii	"a*b= \000"
	.align	2
$LC5:
	.ascii	"a/b= \000"
	.align	2
$LC6:
	.ascii	"a^b= \000"
	.align	2
$LC7:
	.ascii	"a&b= \000"
	.align	2
$LC8:
	.ascii	"a|b= \000"
	.align	2
$LC9:
	.ascii	"a<<3 = \000"
	.align	2
$LC10:
	.ascii	"a>>3 = \000"
	.align	2
$LC11:
	.ascii	"e>>16 = \000"
	.align	2
$LC12:
	.ascii	"e<<2 = \000"
	.align	2
$LC13:
	.ascii	"f+g = \000"
	.align	2
$LC14:
	.ascii	"f-g = \000"
	.align	2
$LC15:
	.ascii	"\n"
	.ascii	"PSexe Exit\n\n\000"
	.text
	.align	2
	.globl	main
	.ent	main
main:
	.frame	$fp,56,$31		# vars= 32, regs= 2/0, args= 16, extra= 0
	.mask	0xc0000000,-4
	.fmask	0x00000000,0
	subu	$sp,$sp,56
	sw	$31,52($sp)
	sw	$fp,48($sp)
	move	$fp,$sp
	jal	__main
	li	$2,-16777200			# 0xff000010
	sw	$2,16($fp)
	li	$2,0x03000100		# 50331904
	sw	$2,20($fp)
	li	$2,-268435456			# 0xf0000000
	sw	$2,32($fp)
	li	$2,-1			# 0xffffffff
	sb	$2,36($fp)
	li	$2,0x00000001		# 1
	sb	$2,37($fp)
	la	$4,$LC0
	lw	$5,16($fp)
	jal	pf
	la	$4,$LC1
	lw	$5,20($fp)
	jal	pf
	lw	$2,16($fp)
	lw	$3,20($fp)
	addu	$2,$2,$3
	la	$4,$LC2
	move	$5,$2
	jal	pf
	lw	$2,16($fp)
	lw	$3,20($fp)
	subu	$2,$2,$3
	la	$4,$LC3
	move	$5,$2
	jal	pf
	lw	$2,16($fp)
	lw	$3,20($fp)
	mult	$2,$3
	mflo	$6
	sw	$6,40($fp)
	lw	$6,40($fp)
	sw	$6,24($fp)
	la	$4,$LC4
	lw	$5,24($fp)
	jal	pf
	lw	$2,16($fp)
	lw	$3,20($fp)
	div	$2,$2,$3
	sw	$2,28($fp)
	la	$4,$LC5
	lw	$5,28($fp)
	jal	pf
	lw	$2,16($fp)
	lw	$3,20($fp)
	xor	$2,$2,$3
	la	$4,$LC6
	move	$5,$2
	jal	pf
	lw	$2,16($fp)
	lw	$3,20($fp)
	and	$2,$2,$3
	la	$4,$LC7
	move	$5,$2
	jal	pf
	lw	$2,16($fp)
	lw	$3,20($fp)
	or	$2,$2,$3
	la	$4,$LC8
	move	$5,$2
	jal	pf
	lw	$3,16($fp)
	sll	$2,$3,3
	la	$4,$LC9
	move	$5,$2
	jal	pf
	lw	$3,16($fp)
	sra	$2,$3,3
	la	$4,$LC10
	move	$5,$2
	jal	pf
	lw	$3,32($fp)
	srl	$2,$3,16
	la	$4,$LC11
	move	$5,$2
	jal	pf
	lw	$3,32($fp)
	sll	$2,$3,2
	la	$4,$LC12
	move	$5,$2
	jal	pf
	lbu	$2,36($fp)
	lbu	$3,37($fp)
	addu	$2,$2,$3
	andi	$3,$2,0x00ff
	la	$4,$LC13
	move	$5,$3
	jal	pf
	lbu	$2,36($fp)
	lbu	$3,37($fp)
	subu	$2,$2,$3
	andi	$3,$2,0x00ff
	la	$4,$LC14
	move	$5,$3
	jal	pf
	la	$4,$LC15
	jal	print
	move	$2,$0
	j	$L1
$L1:
	move	$sp,$fp			# sp not trusted here
	lw	$31,52($sp)
	lw	$fp,48($sp)
	addu	$sp,$sp,56
	j	$31
	.end	main
	.rdata
	.align	2
$LC16:
	.ascii	"\n\000"
	.text
	.align	2
	.globl	pf
	.ent	pf
pf:
	.frame	$fp,24,$31		# vars= 0, regs= 2/0, args= 16, extra= 0
	.mask	0xc0000000,-4
	.fmask	0x00000000,0
	subu	$sp,$sp,24
	sw	$31,20($sp)
	sw	$fp,16($sp)
	move	$fp,$sp
	sw	$4,24($fp)
	sw	$5,28($fp)
	lw	$4,24($fp)
	jal	print
	lw	$4,28($fp)
	jal	pnumi
	la	$4,$LC16
	jal	print
$L2:
	move	$sp,$fp			# sp not trusted here
	lw	$31,20($sp)
	lw	$fp,16($sp)
	addu	$sp,$sp,24
	j	$31
	.end	pf
	.align	2
	.globl	print
	.ent	print
print:
	.frame	$fp,24,$31		# vars= 16, regs= 1/0, args= 0, extra= 0
	.mask	0x40000000,-8
	.fmask	0x00000000,0
	subu	$sp,$sp,24
	sw	$fp,16($sp)
	move	$fp,$sp
	sw	$4,24($fp)
	li	$2,0x1f801050		# 528486480
	sw	$2,0($fp)
	sw	$0,4($fp)
	lw	$2,24($fp)
	lw	$3,4($fp)
	addu	$2,$2,$3
	lbu	$3,0($2)
	sb	$3,8($fp)
$L4:
	lb	$2,8($fp)
	bne	$2,$0,$L7
	j	$L5
$L7:
	lw	$2,0($fp)
	lb	$3,8($fp)
	sw	$3,0($2)
$L6:
	lw	$2,4($fp)
	addu	$3,$2,1
	move	$2,$3
	sw	$2,4($fp)
	lw	$3,24($fp)
	addu	$2,$2,$3
	lbu	$3,0($2)
	sb	$3,8($fp)
	j	$L4
$L5:
$L3:
	move	$sp,$fp			# sp not trusted here
	lw	$fp,16($sp)
	addu	$sp,$sp,24
	j	$31
	.end	print
	.rdata
	.align	2
$LC17:
	.ascii	"0x\000"
	.align	2
$LC18:
	.ascii	"0123456789ABCDEF\000"
	.text
	.align	2
	.globl	pnumi
	.ent	pnumi
pnumi:
	.frame	$fp,48,$31		# vars= 24, regs= 2/0, args= 16, extra= 0
	.mask	0xc0000000,-4
	.fmask	0x00000000,0
	subu	$sp,$sp,48
	sw	$31,44($sp)
	sw	$fp,40($sp)
	move	$fp,$sp
	sw	$4,48($fp)
	addu	$2,$fp,16
	la	$3,$LC17
	lb	$7,0($3)
	lb	$8,1($3)
	lb	$9,2($3)
	sb	$7,0($2)
	sb	$8,1($2)
	sb	$9,2($2)
	addu	$2,$fp,19
	move	$4,$2
	move	$5,$0
	li	$6,0x00000008		# 8
	jal	memset
	la	$2,$LC18
	sw	$2,32($fp)
	lw	$3,48($fp)
	sra	$2,$3,28
	andi	$3,$2,0x000f
	lw	$4,32($fp)
	addu	$2,$3,$4
	lbu	$3,0($2)
	sb	$3,18($fp)
	lw	$3,48($fp)
	sra	$2,$3,24
	andi	$3,$2,0x000f
	lw	$4,32($fp)
	addu	$2,$3,$4
	lbu	$3,0($2)
	sb	$3,19($fp)
	lw	$3,48($fp)
	sra	$2,$3,20
	andi	$3,$2,0x000f
	lw	$4,32($fp)
	addu	$2,$3,$4
	lbu	$3,0($2)
	sb	$3,20($fp)
	lw	$3,48($fp)
	sra	$2,$3,16
	andi	$3,$2,0x000f
	lw	$4,32($fp)
	addu	$2,$3,$4
	lbu	$3,0($2)
	sb	$3,21($fp)
	lw	$3,48($fp)
	sra	$2,$3,12
	andi	$3,$2,0x000f
	lw	$4,32($fp)
	addu	$2,$3,$4
	lbu	$3,0($2)
	sb	$3,22($fp)
	lw	$3,48($fp)
	sra	$2,$3,8
	andi	$3,$2,0x000f
	lw	$4,32($fp)
	addu	$2,$3,$4
	lbu	$3,0($2)
	sb	$3,23($fp)
	lw	$3,48($fp)
	sra	$2,$3,4
	andi	$3,$2,0x000f
	lw	$4,32($fp)
	addu	$2,$3,$4
	lbu	$3,0($2)
	sb	$3,24($fp)
	lw	$3,48($fp)
	andi	$2,$3,0x000f
	lw	$3,32($fp)
	addu	$2,$2,$3
	lbu	$3,0($2)
	sb	$3,25($fp)
	sb	$0,26($fp)
	addu	$4,$fp,16
	jal	print
$L8:
	move	$sp,$fp			# sp not trusted here
	lw	$31,44($sp)
	lw	$fp,40($sp)
	addu	$sp,$sp,48
	j	$31
	.end	pnumi
	.align	2
	.globl	__main
	.ent	__main
__main:
	.frame	$fp,8,$31		# vars= 0, regs= 1/0, args= 0, extra= 0
	.mask	0x40000000,-8
	.fmask	0x00000000,0
	subu	$sp,$sp,8
	sw	$fp,0($sp)
	move	$fp,$sp
	j	$L9
$L9:
	move	$sp,$fp			# sp not trusted here
	lw	$fp,0($sp)
	addu	$sp,$sp,8
	j	$31
	.end	__main
	.align	2
	.globl	memset
	.ent	memset
memset:
	.frame	$fp,16,$31		# vars= 8, regs= 1/0, args= 0, extra= 0
	.mask	0x40000000,-8
	.fmask	0x00000000,0
	subu	$sp,$sp,16
	sw	$fp,8($sp)
	move	$fp,$sp
	sw	$4,16($fp)
	sw	$5,20($fp)
	sw	$6,24($fp)
	lw	$2,16($fp)
	sw	$2,0($fp)
	sw	$0,4($fp)
$L11:
	lw	$2,4($fp)
	lw	$3,24($fp)
	slt	$2,$2,$3
	bne	$2,$0,$L14
	j	$L12
$L14:
	lw	$2,0($fp)
	lw	$3,4($fp)
	addu	$2,$2,$3
	lbu	$3,20($fp)
	sb	$3,0($2)
$L13:
	lw	$3,4($fp)
	addu	$2,$3,1
	move	$3,$2
	sw	$3,4($fp)
	j	$L11
$L12:
	lw	$2,16($fp)
	j	$L10
$L10:
	move	$sp,$fp			# sp not trusted here
	lw	$fp,8($sp)
	addu	$sp,$sp,16
	j	$31
	.end	memset
