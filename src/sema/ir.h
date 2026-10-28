// ============================================================================
// MiniC 编译器 —— 中间代码模块（Quad / Operand / QuadOp / IRGenerator）
// 第 4.5 节数据结构 + 第 6.5 节接口契约
// Q6 定稿：操作数引用式（表索引），打印时按索引转原文（OperandToString）
//
// 【2026-10-27 改造】操作码体系按参考书《编译器设计之路》（裘巍）表 5-2
//   「NPIR 的操作符」重建，采用与书完全一致的编码规则：
//      高 5 位 = 大类编号（21 大类），低 5 位 = 类内成员编号
//  这样「大类」即书的 MIR 操作符（与目标机无关），「大类 + 宽度后缀」即 LIR
//  操作符（涉及字长与物理存储），一个 QuadOp 同时承载两层语义：
//      QuadCat(op)     → 大类（MIR），如 CAT_ADD
//      QuadOpName(op)  → 带宽度后缀的完整助记符（LIR），如 "ADD_4" / "ADD_4F"
//  改动理由与取舍见 docs/interface.md 第 25 条。
// ============================================================================
#ifndef MINIC_SEMA_IR_H
#define MINIC_SEMA_IR_H

#include "lexer/token.h"
#include "parser/ast.h"
#include "symbol/symbol.h"

#include <map>
#include <stack>
#include <string>
#include <vector>
#include <ostream>

// ----------------------------------------------------------------------------
// 4.5.1 操作码 QuadOp
//   参考书表 5-2 的 21 大类；编码规则同书：高 5 位大类，低 5 位类内成员。
//   注：MiniC 只产生其中一部分（见各条注释的「MiniC 产生」标记），其余成员
//   按书完整保留，便于与表 5-2 逐条对照（答辩时可直接指认差异）。
// ----------------------------------------------------------------------------
namespace QuadCat
{
// 21 大类编号（即表的栏目顺序）；高 5 位用它，故必须 <= 31
enum : int
{
    CAT_NONE   = 0,    // 无
    CAT_EQU    = 1,    // 相等（EQU）
    CAT_NEQU   = 2,    // 不等（NEQU）
    CAT_LS     = 3,    // 小于（LS）
    CAT_MR     = 4,    // 大于（MR）
    CAT_LE     = 5,    // 小于或等于（LE）
    CAT_ME     = 6,    // 大于或等于（ME）
    CAT_IN     = 7,    // 集合包含（IN）
    CAT_SUB    = 8,    // 减法（SUB）
    CAT_ADD    = 9,    // 加法（ADD）
    CAT_OR     = 10,   // 或（OR）
    CAT_XOR    = 11,   // 异或（XOR）
    CAT_MUL    = 12,   // 乘法（MUL）
    CAT_DIV    = 13,   // 除法（DIV）
    CAT_MOD    = 14,   // 取模（MOD）
    CAT_SHR    = 15,   // 右移（SHR）
    CAT_SHL    = 16,   // 左移（SHL）
    CAT_AND    = 17,   // 与（AND）
    CAT_NEG    = 18,   // 取负（NEG）
    CAT_NOT    = 19,   // 取非（NOT）
    CAT_ASSIGN = 20,   // 赋值（ASSIGN）
    CAT_OTHER  = 21    // 其他（跳转/调用/地址……）
};
} // namespace QuadCat

enum class QuadOp : int
{
    Q_NONE = 0,

    // ---- 1. 相等 EQU（9 个成员，同表 5-2）----
    //   MiniC 产生：EQU_1（char/bool 比较）、EQU_4（int 比较）、EQU_4F（float 比较）
    //   EQU_4F 由书的 EQU_4F 直接对应；EQU_1 对应书的 EQU_1
    Q_EQU_1  = (QuadCat::CAT_EQU << 5) | 1,   // 相等，1 字节
    Q_EQU_2  = (QuadCat::CAT_EQU << 5) | 2,   // 相等，2 字节
    Q_EQU_4  = (QuadCat::CAT_EQU << 5) | 3,   // 相等，4 字节
    Q_EQU_8  = (QuadCat::CAT_EQU << 5) | 4,   // 相等，8 字节
    Q_EQU_4F = (QuadCat::CAT_EQU << 5) | 5,   // 相等，4 字节浮点数
    Q_EQU_8F = (QuadCat::CAT_EQU << 5) | 6,   // 相等，8 字节浮点数
    Q_EQU_S  = (QuadCat::CAT_EQU << 5) | 7,   // 相等，字符串
    Q_EQU_32 = (QuadCat::CAT_EQU << 5) | 8,   // 相等，32 字节
    Q_EQU_A  = (QuadCat::CAT_EQU << 5) | 9,   // 相等，数组

    // ---- 2. 不等 NEQU（9 个成员）----
    //   MiniC 产生：NEQU_1、NEQU_4、NEQU_4F
    Q_NEQU_1  = (QuadCat::CAT_NEQU << 5) | 1,
    Q_NEQU_2  = (QuadCat::CAT_NEQU << 5) | 2,
    Q_NEQU_4  = (QuadCat::CAT_NEQU << 5) | 3,
    Q_NEQU_8  = (QuadCat::CAT_NEQU << 5) | 4,
    Q_NEQU_4F = (QuadCat::CAT_NEQU << 5) | 5,
    Q_NEQU_8F = (QuadCat::CAT_NEQU << 5) | 6,
    Q_NEQU_S  = (QuadCat::CAT_NEQU << 5) | 7,
    Q_NEQU_32 = (QuadCat::CAT_NEQU << 5) | 8,
    Q_NEQU_A  = (QuadCat::CAT_NEQU << 5) | 9,

    // ---- 3. 小于 LS（10 个成员，含无符号变体）----
    //   MiniC 产生：LS_1、LS_4、LS_4F
    Q_LS_1  = (QuadCat::CAT_LS << 5) | 1,
    Q_LS_2  = (QuadCat::CAT_LS << 5) | 2,
    Q_LS_4  = (QuadCat::CAT_LS << 5) | 3,
    Q_LS_8  = (QuadCat::CAT_LS << 5) | 4,
    Q_LS_4F = (QuadCat::CAT_LS << 5) | 5,
    Q_LS_8F = (QuadCat::CAT_LS << 5) | 6,
    Q_LS_S  = (QuadCat::CAT_LS << 5) | 7,
    Q_LS_1U = (QuadCat::CAT_LS << 5) | 8,    // 1 字节无符号
    Q_LS_2U = (QuadCat::CAT_LS << 5) | 9,    // 2 字节无符号
    Q_LS_4U = (QuadCat::CAT_LS << 5) | 10,   // 4 字节无符号

    // ---- 4. 大于 MR（10 个成员）----
    //   MiniC 产生：MR_1、MR_4、MR_4F
    Q_MR_1  = (QuadCat::CAT_MR << 5) | 1,
    Q_MR_2  = (QuadCat::CAT_MR << 5) | 2,
    Q_MR_4  = (QuadCat::CAT_MR << 5) | 3,
    Q_MR_8  = (QuadCat::CAT_MR << 5) | 4,
    Q_MR_4F = (QuadCat::CAT_MR << 5) | 5,
    Q_MR_8F = (QuadCat::CAT_MR << 5) | 6,
    Q_MR_S  = (QuadCat::CAT_MR << 5) | 7,
    Q_MR_1U = (QuadCat::CAT_MR << 5) | 8,
    Q_MR_2U = (QuadCat::CAT_MR << 5) | 9,
    Q_MR_4U = (QuadCat::CAT_MR << 5) | 10,

    // ---- 5. 小于或等于 LE（10 个成员）----
    //   MiniC 产生：LE_1、LE_4、LE_4F
    Q_LE_1  = (QuadCat::CAT_LE << 5) | 1,
    Q_LE_2  = (QuadCat::CAT_LE << 5) | 2,
    Q_LE_4  = (QuadCat::CAT_LE << 5) | 3,
    Q_LE_8  = (QuadCat::CAT_LE << 5) | 4,
    Q_LE_4F = (QuadCat::CAT_LE << 5) | 5,
    Q_LE_8F = (QuadCat::CAT_LE << 5) | 6,
    Q_LE_S  = (QuadCat::CAT_LE << 5) | 7,
    Q_LE_1U = (QuadCat::CAT_LE << 5) | 8,
    Q_LE_2U = (QuadCat::CAT_LE << 5) | 9,
    Q_LE_4U = (QuadCat::CAT_LE << 5) | 10,

    // ---- 6. 大于或等于 ME（10 个成员）----
    //   MiniC 产生：ME_1、ME_4、ME_4F
    Q_ME_1  = (QuadCat::CAT_ME << 5) | 1,
    Q_ME_2  = (QuadCat::CAT_ME << 5) | 2,
    Q_ME_4  = (QuadCat::CAT_ME << 5) | 3,
    Q_ME_8  = (QuadCat::CAT_ME << 5) | 4,
    Q_ME_4F = (QuadCat::CAT_ME << 5) | 5,
    Q_ME_8F = (QuadCat::CAT_ME << 5) | 6,
    Q_ME_S  = (QuadCat::CAT_ME << 5) | 7,
    Q_ME_1U = (QuadCat::CAT_ME << 5) | 8,
    Q_ME_2U = (QuadCat::CAT_ME << 5) | 9,
    Q_ME_4U = (QuadCat::CAT_ME << 5) | 10,

    // ---- 7. 集合包含 IN（8 个成员）----
    //   MiniC 不产生（无集合类型）
    Q_IN_C  = (QuadCat::CAT_IN << 5) | 1,    // CHAR 值
    Q_IN_B  = (QuadCat::CAT_IN << 5) | 2,    // BOOLEAN 值
    Q_IN_SH = (QuadCat::CAT_IN << 5) | 3,    // SHORTINT 值
    Q_IN_1  = (QuadCat::CAT_IN << 5) | 4,    // INTEGER 值
    Q_IN_SM = (QuadCat::CAT_IN << 5) | 5,    // SMALLINT 值
    Q_IN_W  = (QuadCat::CAT_IN << 5) | 6,    // WORD 值
    Q_IN_LW = (QuadCat::CAT_IN << 5) | 7,    // LONGWORD 值
    Q_IN_E  = (QuadCat::CAT_IN << 5) | 8,    // ENUM 值

    // ---- 8. 减法 SUB（5 个成员）----
    //   MiniC 产生：SUB_4（整型）、SUB_4F（单精度）
    //   注：SUB_4F 是按书中 _4F 命名先例补的（书只有 SUB_8F，因为 NeoPascal 浮点为 8 字节）
    Q_SUB_2   = (QuadCat::CAT_SUB << 5) | 1,
    Q_SUB_4   = (QuadCat::CAT_SUB << 5) | 2,
    Q_SUB_8   = (QuadCat::CAT_SUB << 5) | 3,
    Q_SUB_8F  = (QuadCat::CAT_SUB << 5) | 4,
    Q_SUB_SET = (QuadCat::CAT_SUB << 5) | 5,   // 集合差
    Q_SUB_4F  = (QuadCat::CAT_SUB << 5) | 6,   // 【MiniC 扩展】单精度减法

    // ---- 9. 加法 ADD（5 个成员 + 1 扩展）----
    //   MiniC 产生：ADD_4、ADD_4F
    Q_ADD_S   = (QuadCat::CAT_ADD << 5) | 1,   // 字符串连接
    Q_ADD_4   = (QuadCat::CAT_ADD << 5) | 2,
    Q_ADD_8F  = (QuadCat::CAT_ADD << 5) | 3,
    Q_ADD_SET = (QuadCat::CAT_ADD << 5) | 4,   // 集合并
    Q_ADD_2   = (QuadCat::CAT_ADD << 5) | 5,
    Q_ADD_4F  = (QuadCat::CAT_ADD << 5) | 6,   // 【MiniC 扩展】单精度加法

    // ---- 10. 或 OR（位或，4 个成员）----
    //   MiniC 不产生（逻辑 || 走短路分支，不生成位或）
    Q_OR_0 = (QuadCat::CAT_OR << 5) | 1,   // 位或
    Q_OR_1 = (QuadCat::CAT_OR << 5) | 2,
    Q_OR_2 = (QuadCat::CAT_OR << 5) | 3,
    Q_OR_4 = (QuadCat::CAT_OR << 5) | 4,

    // ---- 11. 异或 XOR（4 个成员）----
    Q_XOR_0 = (QuadCat::CAT_XOR << 5) | 1,   // 位异或
    Q_XOR_1 = (QuadCat::CAT_XOR << 5) | 2,
    Q_XOR_2 = (QuadCat::CAT_XOR << 5) | 3,
    Q_XOR_4 = (QuadCat::CAT_XOR << 5) | 4,

    // ---- 12. 乘法 MUL（3 个成员 + 1 扩展）----
    //   MiniC 产生：MUL_4、MUL_4F
    Q_MUL_4   = (QuadCat::CAT_MUL << 5) | 1,
    Q_MUL_8F  = (QuadCat::CAT_MUL << 5) | 2,
    Q_MUL_SET = (QuadCat::CAT_MUL << 5) | 3,   // 集合交
    Q_MUL_4F  = (QuadCat::CAT_MUL << 5) | 4,   // 【MiniC 扩展】单精度乘法

    // ---- 13. 除法 DIV（2 个成员 + 1 扩展）----
    //   MiniC 产生：DIV_4、DIV_4F
    Q_DIV_4  = (QuadCat::CAT_DIV << 5) | 1,
    Q_DIV_8F = (QuadCat::CAT_DIV << 5) | 2,
    Q_DIV_4F = (QuadCat::CAT_DIV << 5) | 3,    // 【MiniC 扩展】单精度除法

    // ---- 14. 取模 MOD（1 个成员）----
    //   MiniC 不产生（语言无 % 运算符）
    Q_MOD_4 = (QuadCat::CAT_MOD << 5) | 1,

    // ---- 15. 右移 SHR（6 个成员）----
    //   MiniC 不产生（语言无位移运算符）
    Q_SHR_1  = (QuadCat::CAT_SHR << 5) | 1,
    Q_SHR_2  = (QuadCat::CAT_SHR << 5) | 2,
    Q_SHR_4  = (QuadCat::CAT_SHR << 5) | 3,
    Q_SHR_1U = (QuadCat::CAT_SHR << 5) | 4,
    Q_SHR_2U = (QuadCat::CAT_SHR << 5) | 5,
    Q_SHR_4U = (QuadCat::CAT_SHR << 5) | 6,

    // ---- 16. 左移 SHL（6 个成员）----
    //   MiniC 不产生
    Q_SHL_1  = (QuadCat::CAT_SHL << 5) | 1,
    Q_SHL_2  = (QuadCat::CAT_SHL << 5) | 2,
    Q_SHL_4  = (QuadCat::CAT_SHL << 5) | 3,
    Q_SHL_1U = (QuadCat::CAT_SHL << 5) | 4,
    Q_SHL_2U = (QuadCat::CAT_SHL << 5) | 5,
    Q_SHL_4U = (QuadCat::CAT_SHL << 5) | 6,

    // ---- 17. 与 AND（位与，4 个成员）----
    //   MiniC 不产生（逻辑 && 走短路分支）
    Q_AND_0 = (QuadCat::CAT_AND << 5) | 1,   // 位与
    Q_AND_1 = (QuadCat::CAT_AND << 5) | 2,
    Q_AND_2 = (QuadCat::CAT_AND << 5) | 3,
    Q_AND_4 = (QuadCat::CAT_AND << 5) | 4,

    // ---- 18. 取负 NEG（5 个成员）----
    //   MiniC 产生：NEG_4（整型）、NEG_4F（单精度）
    Q_NEG_1  = (QuadCat::CAT_NEG << 5) | 1,
    Q_NEG_2  = (QuadCat::CAT_NEG << 5) | 2,
    Q_NEG_4  = (QuadCat::CAT_NEG << 5) | 3,
    Q_NEG_4F = (QuadCat::CAT_NEG << 5) | 4,
    Q_NEG_8F = (QuadCat::CAT_NEG << 5) | 5,

    // ---- 19. 取非 NOT（4 个成员）----
    //   MiniC 产生：NOT_4（逻辑非 !x，按 4 字节整型域求 (x == 0)）
    Q_NOT_0 = (QuadCat::CAT_NOT << 5) | 1,   // 位非
    Q_NOT_1 = (QuadCat::CAT_NOT << 5) | 2,
    Q_NOT_2 = (QuadCat::CAT_NOT << 5) | 3,
    Q_NOT_4 = (QuadCat::CAT_NOT << 5) | 4,

    // ---- 20. 赋值 ASSIGN（7 个成员 + 1 扩展）----
    //   MiniC 产生：ASSIGN_1（char）、ASSIGN_4（int/bool）、ASSIGN_4F（float）
    Q_ASSIGN_1  = (QuadCat::CAT_ASSIGN << 5) | 1,
    Q_ASSIGN_2  = (QuadCat::CAT_ASSIGN << 5) | 2,
    Q_ASSIGN_4  = (QuadCat::CAT_ASSIGN << 5) | 3,
    Q_ASSIGN_8  = (QuadCat::CAT_ASSIGN << 5) | 4,
    Q_ASSIGN_32 = (QuadCat::CAT_ASSIGN << 5) | 5,
    Q_ASSIGN_N  = (QuadCat::CAT_ASSIGN << 5) | 6,   // 多字节
    Q_ASSIGN_S  = (QuadCat::CAT_ASSIGN << 5) | 7,   // 字符串
    Q_ASSIGN_4F = (QuadCat::CAT_ASSIGN << 5) | 8,   // 【MiniC 扩展】单精度赋值

    // ---- 21. 其他（15 个成员 + MiniC 需要的 8 个扩展）----
    //   MiniC 产生：JMP、LABEL、PARA、RETV、CALL、JT、JNT，
    //              以及 INDEX_1/INDEX_4/INDEX_4F、PRINT_1/PRINT_4/PRINT_4F、ITOF、FTOI
    //   MiniC 不产生：JE_1、JE_2、JE_4、CALLPTR、ASM、GETADDR、SET_ADDITEM、GETPROCADDR
    Q_JMP        = (QuadCat::CAT_OTHER << 5) | 1,    // 无条件跳转
    Q_LABEL      = (QuadCat::CAT_OTHER << 5) | 2,    // 标号定义
    Q_PARA       = (QuadCat::CAT_OTHER << 5) | 3,    // 传递参数
    Q_RETV       = (QuadCat::CAT_OTHER << 5) | 4,    // 传递返回值（return 语句）
    Q_CALL       = (QuadCat::CAT_OTHER << 5) | 5,    // 函数调用
    Q_JT         = (QuadCat::CAT_OTHER << 5) | 6,    // 条件真跳转（非零继续）
    Q_JNT        = (QuadCat::CAT_OTHER << 5) | 7,    // 条件假跳转（为零则跳）
    Q_JE_1       = (QuadCat::CAT_OTHER << 5) | 8,    // 1 字节相等跳转
    Q_JE_2       = (QuadCat::CAT_OTHER << 5) | 9,    // 2 字节相等跳转
    Q_JE_4       = (QuadCat::CAT_OTHER << 5) | 10,   // 4 字节相等跳转
    Q_CALLPTR    = (QuadCat::CAT_OTHER << 5) | 11,   // 调用函数指针
    Q_ASM        = (QuadCat::CAT_OTHER << 5) | 12,   // 汇编
    Q_GETADDR    = (QuadCat::CAT_OTHER << 5) | 13,   // 获得变量地址
    Q_SET_ADDITEM= (QuadCat::CAT_OTHER << 5) | 14,   // 元素加入集合
    Q_GETPROCADDR= (QuadCat::CAT_OTHER << 5) | 15,   // 获得函数地址
    // 【MiniC 扩展】表 5-2 没有、但 MiniC 必须有的操作码。
    //   照书的命名精神，凡涉及访存宽度的都给宽度后缀；书中用 GETADDR + 偏移或
    //   CALL 运行时例程表达这些语义，MiniC 直接做成操作码。
    Q_INDEX_1    = (QuadCat::CAT_OTHER << 5) | 16,   // 数组元素（char，1 字节）
    Q_INDEX_4    = (QuadCat::CAT_OTHER << 5) | 17,   // 数组元素（int/bool，4 字节）
    Q_INDEX_4F   = (QuadCat::CAT_OTHER << 5) | 18,   // 数组元素（float，4 字节单精度）
    Q_PRINT_1    = (QuadCat::CAT_OTHER << 5) | 19,   // print 一个 char
    Q_PRINT_4    = (QuadCat::CAT_OTHER << 5) | 20,   // print 一个 int/bool
    Q_PRINT_4F   = (QuadCat::CAT_OTHER << 5) | 21,   // print 一个 float
    Q_ITOF       = (QuadCat::CAT_OTHER << 5) | 22,   // int → float（书靠换宽度操作符表达）
    Q_FTOI       = (QuadCat::CAT_OTHER << 5) | 23    // float → int（同上）
};

// 编码规则（同表 5-2 的说明：高 5 位大类，低 5 位类内成员）
inline int QuadCatOf(QuadOp op)
{
    return static_cast<int>(op) >> 5;
}
inline int QuadMemberOf(QuadOp op)
{
    return static_cast<int>(op) & 0x1F;
}
inline QuadOp MakeQuadOp(int cat, int member)
{
    return static_cast<QuadOp>((cat << 5) | (member & 0x1F));
}

// 大类名（表的栏目缩写）与类内成员名
const char* QuadCatName(int cat);                       // "EQU" / "ADD" / "OTHER" …
const char* QuadMemberName(int cat, int member);        // "ADD_4" / "ASSIGN_4F" …
// 完整助记符（LIR 形态）：直接返回成员名，如 "ADD_4"、"ASSIGN_4F"、"JMP"
std::string QuadOpName(QuadOp op);
// 栈/内存宽度判断（表 5-2 的宽度后缀落到 x86-64 的字节数）
bool QuadOpIsFloat(QuadOp op);                          // 是否 _4F / _8F 成员
int  QuadOpByteWidth(QuadOp op);                        // 1 / 2 / 4 / 8

// 按「大类 + 操作数类型」选带宽度后缀的操作码（表 5-2 的 LIR 层选择规则）：
//   float → _4F，char → _1，int/bool → _4；该大类没有对应成员时回落到 _4。
//   cat 取 QuadCat::CAT_EQU … CAT_ASSIGN（1~20）。
QuadOp QuadOpForType(int cat, TokenType t);
QuadOp QuadIndexOp(TokenType t);     // INDEX_1 / INDEX_4 / INDEX_4F
QuadOp QuadPrintOp(TokenType t);     // PRINT_1 / PRINT_4 / PRINT_4F

// 判断两个操作码是否同一大类（MIR 层比较，用于 codegen 分派）
inline bool SameQuadCat(QuadOp a, QuadOp b)
{
    return QuadCatOf(a) == QuadCatOf(b);
}

// ----------------------------------------------------------------------------
// 4.5.2 操作数 Operand（参考书 OpInfo：引用式，UD/DU 链暂不启用）
// ----------------------------------------------------------------------------
enum class OperandKind
{
    OP_NONE,    // 空操作数
    OP_CONST,   // 常量（m_iLink = 常量表位序）
    OP_VAR,     // 变量（m_iLink = 变量表位序）
    OP_TEMP,    // 临时变量（m_iLink = 临时编号 _Txxx）
    OP_LABEL    // 标号（m_iLink = 标号编号 Lxxx）
};

class Operand
{
public:
    OperandKind m_kind;      // 操作数种类（参考书 m_iType）
    int         m_iLink;     // 链接索引（常量表/变量表/临时编号/标号编号）
    TokenType   m_type;      // 数据类型：TOKEN_INT / TOKEN_FLOAT（目标代码定字节宽度）
    bool        m_bRef;      // 是否为间接引用（挑战版指针/数组用）
    int         m_iDetailType; // 详细类型信息（栈式类型链用）

    Operand() : m_kind(OperandKind::OP_NONE), m_iLink(-1),
                m_type(TokenType::TOKEN_EOF), m_bRef(false), m_iDetailType(0) {}

    // 便捷构造
    static Operand Const(int idx, TokenType t);   // 常量
    static Operand Var(int idx, TokenType t);     // 变量
    static Operand Temp(int idx, TokenType t);    // 临时变量
    static Operand Label(int idx);                // 标号
};

// ----------------------------------------------------------------------------
// 4.5.3 四元式 Quad（参考书 IRCode：m_eOpType/m_Op1/m_Op2/m_Rslt）
// ----------------------------------------------------------------------------
class Quad
{
public:
    QuadOp      m_eOp;       // 操作码（参考书 m_eOpType）
    Operand     m_Op1;       // arg1（参考书 m_Op1）
    Operand     m_Op2;       // arg2（参考书 m_Op2）
    Operand     m_Rslt;      // res （参考书 m_Rslt）
    LineInfo    m_loc;       // 对应源码位置

    Quad(QuadOp op, const Operand& op1, const Operand& op2, const Operand& rslt)
        : m_eOp(op), m_Op1(op1), m_Op2(op2), m_Rslt(rslt) {}
    void Print(std::ostream& os) const;   // 打印四元式（交付物）
};

using CodeList = std::vector<Quad>;

// ----------------------------------------------------------------------------
// 4.5.4 中间代码生成器 IRGenerator（第 6 章接口契约）
// ----------------------------------------------------------------------------
class IRGenerator
{
private:
    int         m_iTmpVarCounter;   // 临时变量计数器（_T1,_T2,...）
    int         m_iLabelCounter;    // 标号计数器（L1,L2,...）
    CodeList    m_Codes;            // 当前过程四元式序列
    SymbolTable& m_symtab;          // 绑定符号表（确定操作数类型，第 6 章契约）
    std::vector<TokenType> m_TmpTypes; // 临时量类型登记（与临时编号一一对应，供类型转换判断）

    // ---- 阶段 07 内部辅助（新增私有方法，不属第 6 章公开契约）----
    Operand EvalExpr(const ASTNodePtr& node);      // 表达式 → Operand（含类型转换插入）
    void    GenStmt(const ASTNodePtr& node);       // 语句 → 四元式
    void    GenFuncDef(FuncDefNode* fn);           // 函数定义：翻译函数体并 FlushToProc
    TokenType OperandType(const Operand& op) const; // 取操作数类型（常量表/变量表/临时量表）
    Operand IntToFloatConv(const Operand& op, const LineInfo& loc); // 插入 ITOF
    Operand FloatToIntConv(const Operand& op, const LineInfo& loc); // 插入 FTOI
    Operand ConstInt(int value, const LineInfo& loc); // 取整型常量 0/1（无则登记）
    Operand ConstFloat(double value, const LineInfo& loc); // 取浮点常量（无则登记）
    // ---- 常量折叠（挑战版 C4 / 设计文档 §8.1 TypeSysInfo + OpConstSemantic）----
    // 取操作数的编译期常量值：bFloat 指示取值用 fVal 还是 iVal
    bool    ConstValueOf(const Operand& op, bool& bFloat, long long& iVal, double& fVal) const;
    // 二元折叠：两侧都是常量时在编译期求值；不可折叠（含除零、溢出）返回 OP_NONE
    Operand FoldBinary(TokenType op, const Operand& lo, const Operand& ro, const LineInfo& loc);
    // 自增自减（挑战版 ++/--）：生成「旧值快照 + 原位 ±1」两条四元式，returnOld 决定返回哪个
    Operand GenIncDec(const UnOpNode* u, bool bReturnOld);
    void    BuildParamTypeTable(const ASTNodePtr& root); // 扫描 AST 建「函数名 → 形参类型表」
    // 形参类型查询：按「被调函数名 + 实参序号」取形参声明类型（供 Q_PARA 插类型转换）
    TokenType ParamTypeOf(const std::string& procName, int pos) const;
    // 形参类型表：函数名 → 形参类型列表（下标 0 = 第 1 个形参）
    // 由 Generate 入口按 AST 的 FuncDefNode/ParamNode 建表（ParamEntry 无过程归属，符号表查不到）
    std::map<std::string, std::vector<TokenType>> m_ParamTypes;
    // 当前正在翻译的函数的返回类型（GenFuncDef 设置；顶层语句为 TOKEN_NONE）
    // 供 return 语句插类型转换（P3 缺陷修复：float 函数 return 整型表达式要插 ITOF）
    TokenType   m_curRetType = TokenType::TOKEN_NONE;

    // ---- 宽度后缀选择（表 5-2 的 LIR 层）----
    // 按操作数类型选带宽度后缀的操作码：int/bool→4、char→1、float→4F
    // cat 为 1~20 的大类号（CAT_EQU … CAT_ASSIGN），返回该类的宽度成员
    // 二元运算：按两侧类型（已统一 promoted）选操作码；用于算术与关系

public:
    explicit IRGenerator(SymbolTable& symtab);

    // --- 第 6 章接口契约 ---
    void    Generate(const ASTNodePtr& root);            // 以程序根节点生成代码
    int     GetTmpVar(TokenType t);                     // 申请临时变量
    int     GetTmpLabel();                              // 申请标号（挑战版）
    void    EmitIR(QuadOp op, const Operand& op1, const Operand& op2, const Operand& rslt);
    void    EmitIR(QuadOp op, const Operand& op1, const Operand& op2, const Operand& rslt,
                   const LineInfo& loc);                // 阶段 07 新增重载：携带源码位置
    const CodeList& GetCodes() const;                    // 输出四元式序列
    // 优化器专用：以可变引用取当前序列。必须是原地改同一个 vector ——
    // CCodeGen 构造时绑的就是这个成员的 const 引用，换成新对象会让它读到旧内容。
    CodeList&       GetCodesMutable();
    std::string OperandToString(const Operand& op) const; // Q6：操作数索引转原文

    // --- 第 4 章参考书成员/扩展 ---
    std::stack<Operand>  m_OperandStack;   // 操作数栈
    std::stack<int>      m_OperationStack; // 运算符栈
    void FlushToProc(ProcEntry& proc);     // 挂到过程表项（挑战版）
};

#endif // MINIC_SEMA_IR_H
