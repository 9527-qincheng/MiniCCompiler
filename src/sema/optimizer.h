// ============================================================================
// MiniC 编译器 —— 四元式窥孔优化（挑战版 C4「编译优化」加分项）
// ----------------------------------------------------------------------------
// 定位：作用在 IRGenerator 产出的四元式序列上，不碰 AST，也不碰目标代码。
// 只做「无论如何都不会改变可观测行为」的删除，宁可少删也不删错：
//   - 判据是「被删掉的东西没有任何副作用，也没有人读它的结果」；
//   - 只要涉及读内存 / 调用 / 打印 / 返回 / 传参 的一律不动（保守处理）。
// 默认关闭，由命令行 -O / --optimize 打开（见 main.cpp），
// 保证不加 -O 时全部既有基线（.quad.txt / .asm / 运行 stdout）逐字不变。
//
// 前三轮（常量条件跳转 / 删不可达 / 删空跳）是纯窥孔，不需要控制流信息；
// 后两轮（删死临时量与死标号、基本块内的常量传播与公共子表达式与死存储）
// 需要先把序列切成基本块 —— 块内没有跳转进入，状态才能安全地向前传播。
// 一切跨块的状态在块边界与 CALL 处一律清空，宁可少优化也不猜。
// ============================================================================
#ifndef MINIC_SEMA_OPTIMIZER_H
#define MINIC_SEMA_OPTIMIZER_H

#include "sema/ir.h"

namespace minic_opt
{

// 每一类删除各删了多少条，用于 -O 时的统计输出与回归对比
struct OptStats
{
    int iFoldedBranch  = 0;   // 常量条件跳转：折叠成 JMP 或直接删除
    int iUnreachable   = 0;   // 无条件转移之后、下一个 LABEL 之前的不可达四元式
    int iRedundantJump = 0;   // 跳到紧邻下一条 LABEL 的空跳
    int iDeadTemp      = 0;   // 结果临时量无人使用的纯计算
    int iDeadLabel     = 0;   // 无人跳转到的标号定义
    int iConstProp     = 0;   // 基本块内已知常量的变量读被替换成常量
    int iCse           = 0;   // 基本块内重复表达式被替换成引用已有临时量
    int iDeadStore     = 0;   // 基本块内被后一次赋值覆盖、且中间没人读的赋值

    // 真正被删掉的四元式条数
    int Removed() const
    {
        return iFoldedBranch + iUnreachable + iRedundantJump + iDeadTemp +
               iDeadLabel + iDeadStore;
    }

    // 只是被改写（数量不变，但后续轮次可能因此再删掉一些）的操作数个数
    int Rewritten() const
    {
        return iConstProp + iCse;
    }

    int Total() const
    {
        return Removed() + Rewritten();
    }
};

// 原地优化一个四元式序列（一个过程的序列，或顶层语句的序列）。
// 需要符号表：判断「条件是不是常量 0」要查常量表里那个索引的真实取值，
// Operand 的 m_iLink 对常量而言是常量表下标，不是常量值本身。
// 返回值累加到 st 上，便于一个编译单元里多个过程汇总统计。
void Optimize(CodeList& codes, const SymbolTable& symtab, OptStats& st);

} // namespace minic_opt

#endif // MINIC_SEMA_OPTIMIZER_H
