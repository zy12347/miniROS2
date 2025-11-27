# 生成 API 文档

## 使用 Doxygen 生成文档

### 前置要求

安装 Doxygen：
```bash
# Ubuntu/Debian
sudo apt-get install doxygen

# macOS
brew install doxygen
```

### 生成文档

1. 确保 Doxygen 配置文件 `Doxyfile` 存在

2. 运行 Doxygen：
```bash
doxygen Doxyfile
```

3. 查看生成的文档：
```bash
# HTML 文档
open docs/html/index.html  # macOS
xdg-open docs/html/index.html  # Linux
```

### 文档输出

- **HTML 文档**: `docs/html/` - 可在浏览器中查看
- **API 参考**: `docs/API_REFERENCE.md` - Markdown 格式的 API 参考

### 文档内容

生成的文档包括：
- 所有公共类和函数的详细说明
- 代码示例
- 类继承关系图
- 文件列表和索引

### 更新文档

修改代码后，重新运行 `doxygen Doxyfile` 即可更新文档。

