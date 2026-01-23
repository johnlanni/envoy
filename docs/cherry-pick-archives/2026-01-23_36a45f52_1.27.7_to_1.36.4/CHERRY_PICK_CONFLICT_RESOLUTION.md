# Cherry-pick 冲突解决方案文档

## 概述

**提交信息**: `36a45f5258` - Apply patch: 000-6-redis-async-client.patch
**源分支**: 1.27.7
**目标分支**: 1.36.4
**冲突文件数**: 2 个

## 冲突文件列表

| 序号 | 文件路径 | 冲突位置 | 冲突类型 |
|------|----------|----------|----------|
| 1 | `test/mocks/upstream/thread_local_cluster.cc` | 行 23-37 | 新增代码冲突 |
| 2 | `test/extensions/filters/network/common/redis/codec_impl_test.cc` | 行 434-1019 | 新增代码冲突 |

---

## 详细冲突分析与解决方案

### 1. `test/mocks/upstream/thread_local_cluster.cc`

#### 冲突点 1 (行 23-37)

**冲突描述**: 两个分支在 `MockThreadLocalCluster` 构造函数末尾各自添加了不同的代码。

- **HEAD (1.36.4)**: 添加了 `dropOverload()`、`dropCategory()`、`setDropOverload()`、`setDropCategory()` 四个 ON_CALL 设置
  ```cpp
  ON_CALL(*this, dropOverload()).WillByDefault(Return(cluster_.drop_overload_));
  ON_CALL(*this, dropCategory()).WillByDefault(ReturnRef(cluster_.drop_category_));
  ON_CALL(*this, setDropOverload(_)).WillByDefault(Invoke([this](UnitFloat drop_overload) -> void {
    cluster_.drop_overload_ = drop_overload;
  }));
  ON_CALL(*this, setDropCategory(_))
      .WillByDefault(Invoke([this](absl::string_view drop_category) -> void {
        cluster_.drop_category_ = drop_category;
      }));
  ```

- **Incoming (000-6 patch)**: 添加了 HIGRESS 条件编译块，包含 `redisAsyncClient()` 的 ON_CALL 设置
  ```cpp
  #if defined(HIGRESS)
    ON_CALL(*this, redisAsyncClient()).WillByDefault(ReturnRef(redis_async_client_));
  #endif
  ```

**解决方案**: ✅ 合并双方 - 保留两边的代码

```cpp
  ON_CALL(*this, httpAsyncClient()).WillByDefault(ReturnRef(async_client_));
  ON_CALL(*this, dropOverload()).WillByDefault(Return(cluster_.drop_overload_));
  ON_CALL(*this, dropCategory()).WillByDefault(ReturnRef(cluster_.drop_category_));
  ON_CALL(*this, setDropOverload(_)).WillByDefault(Invoke([this](UnitFloat drop_overload) -> void {
    cluster_.drop_overload_ = drop_overload;
  }));
  ON_CALL(*this, setDropCategory(_))
      .WillByDefault(Invoke([this](absl::string_view drop_category) -> void {
        cluster_.drop_category_ = drop_category;
      }));
#if defined(HIGRESS)
  ON_CALL(*this, redisAsyncClient()).WillByDefault(ReturnRef(redis_async_client_));
#endif
}
```

---

### 2. `test/extensions/filters/network/common/redis/codec_impl_test.cc`

#### 冲突点 1 (行 434-1019)

**冲突描述**: 两个分支在 `InvalidBulkStringExpectLF` 测试之后添加了不同的代码。

- **HEAD (1.36.4)**: 添加了大量 `InlineCommand*` 测试用例（约 420 行），这些测试位于 HIGRESS 宏之外，是 1.36 版本新增的功能测试
  - `InlineCommandSingleWord`
  - `InlineCommandMultipleWords`
  - `InlineCommandNumericWord`
  - ... 等约 30 个测试用例
  - 最后是 `InvalidInterjectedInlineCommand`

- **Incoming (000-6 patch)**: 添加了 `#if defined(HIGRESS)` 条件编译块，包含：
  - `RedisRawEncoderDecoderImplTest` 类定义
  - 约 17 个 `RedisRawEncoderDecoderImplTest` 测试用例
  - `#endif` 结束条件编译

**架构差异说明**:
- 1.36.4 没有 `#if defined(HIGRESS)` 块
- 1.27.7 的 `RedisRawEncoderDecoderImplTest` 测试位于 HIGRESS 宏内
- 1.36.4 的 `InlineCommand*` 测试位于普通代码区域（非条件编译）

**解决方案**: ✅ 合并双方

正确的代码结构应该是：
1. 保留 HEAD 的所有 `InlineCommand*` 测试（在 HIGRESS 宏外）
2. 在 `InlineCommand*` 测试之后添加 `#if defined(HIGRESS)` 块
3. 在 HIGRESS 块内添加 `RedisRawEncoderDecoderImplTest` 类和测试

```cpp
// ... InlineCommand* 测试 (来自 HEAD) ...

TEST_F(RedisEncoderDecoderImplTest, InvalidInterjectedInlineCommand) {
  buffer_.add("*1\r\nECHO\r\n");
  EXPECT_THROW(decoder_.decode(buffer_), ProtocolError);
}

#if defined(HIGRESS)
class RedisRawEncoderDecoderImplTest : public testing::Test, RawDecoderCallbacks {
public:
  RedisRawEncoderDecoderImplTest() : decoder_(*this) {}

  void onRawResponse(std::string&& response) override { decoded_values_.push_back(response); }

  RawEncoderImpl encoder_;
  RawDecoderImpl decoder_;
  Buffer::OwnedImpl buffer_;
  std::vector<std::string> decoded_values_;
};

// ... RedisRawEncoderDecoderImplTest 测试用例 ...

TEST_F(RedisRawEncoderDecoderImplTest, InvalidBulkStringExpectLF) {
  buffer_.add("$1\r\na\ra");
  EXPECT_THROW(decoder_.decode(buffer_), ProtocolError);
}
#endif
} // namespace Redis
} // namespace Common
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
```

---

## 解决步骤总结

### ✅ 可直接合并的冲突

以下文件可以采用合并双方策略解决:

1. **`test/mocks/upstream/thread_local_cluster.cc`** - 合并双方代码，先保留 HEAD 的 dropOverload 相关代码，再添加 HIGRESS 条件编译块

2. **`test/extensions/filters/network/common/redis/codec_impl_test.cc`** - 合并双方代码：
   - 保留 HEAD 的所有 `InlineCommand*` 测试
   - 在末尾添加 HIGRESS 条件编译块和 `RedisRawEncoderDecoderImplTest` 类及测试

### ⚠️ 需要注意的点

1. 确保 `#if defined(HIGRESS)` 宏正确放置
2. 确保 namespace 闭合正确
3. 确保没有重复的测试用例

---

## 风险评估

| 风险等级 | 文件 | 说明 |
|----------|------|------|
| 🟢 低 | `test/mocks/upstream/thread_local_cluster.cc` | 简单的代码合并，两边代码互不干扰 |
| 🟢 低 | `test/extensions/filters/network/common/redis/codec_impl_test.cc` | 两边代码互不干扰，只是位置需要调整 |

---

## 验证清单

- [ ] 编译通过
- [ ] 单元测试通过
- [ ] 功能验证通过
- [ ] 条件编译正确（HIGRESS 宏）
- [ ] namespace 闭合正确

---

## 建议的工作流程

1. **第一阶段**: 手动解决 2 个冲突文件
2. **第二阶段**: 执行 `git cherry-pick --continue`
3. **第三阶段**: 编译验证
4. **第四阶段**: 运行相关单元测试
