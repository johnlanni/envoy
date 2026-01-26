# Cherry-pick 归档总结

## 基本信息
- **日期**: 2026-01-26
- **原始提交**: 5da3ebee29 Apply patch: 000-9-rsa2048.patch
- **源分支**: 1.27.7
- **目标分支**: 1.36.4
- **操作人**: Assistant

## 冲突概览
- 冲突文件数: 2
- 编译问题: 是
- 单测问题: 是

## 关键决策
1. **架构迁移适应**: 将代码从 `source/extensions/transport_sockets/tls/` 迁移到 `source/common/tls/`
2. **API变更适应**: 从直接构造 `ClientContextConfigImpl` 改为使用工厂方法 `ClientContextConfigImpl::create`
3. **指针解引用**: 由于工厂方法返回 `std::unique_ptr`，需要使用 `*client_context_config` 传递给需要引用的方法

## 注意事项
- 条件编译指令 `#if !defined(HIGRESS)` 已正确应用到RSA密钥验证逻辑
- 测试数据文件路径已从旧位置更新到新位置
- 所有API变更均已适应新版本的Envoy

## 相关文件
- [x] CHERRY_PICK_CONFLICT_RESOLUTION.md