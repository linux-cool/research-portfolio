# Research Portfolio - 研究项目展示网站

一个现代化的个人研究项目展示网站，使用纯 HTML、CSS 和 JavaScript 构建，支持 GitHub Pages 部署。

## ✨ 功能特性

- 🎨 **现代化设计** - 采用 Material Design 风格，界面简洁美观
- 📱 **完全响应式** - 支持各种设备尺寸，移动端友好
- 🔍 **智能搜索** - 支持项目标题、描述、标签的全文搜索
- 🏷️ **分类筛选** - 按研究领域和年份进行项目筛选
- 📊 **动态统计** - 实时显示项目数量、研究领域等统计信息
- 💻 **项目详情** - 点击项目卡片查看详细信息和描述
- 🌐 **多语言支持** - 支持中文界面（代码注释使用英文）

## 🚀 快速开始

### 本地开发

1. 克隆项目到本地
```bash
git clone <your-repo-url>
cd research-portfolio
```

2. 使用本地服务器运行（推荐）
```bash
# 使用 Python 3
python -m http.server 8000

# 或使用 Node.js
npx serve .

# 或使用 PHP
php -S localhost:8000
```

3. 在浏览器中访问 `http://localhost:8000`

### GitHub Pages 部署

1. 将项目推送到 GitHub 仓库
2. 在仓库设置中启用 GitHub Pages
3. 选择部署分支（通常是 `main` 或 `gh-pages`）
4. 等待部署完成，访问生成的 URL

## 📁 项目结构

```
research-portfolio/
├── index.html          # 主页面
├── css/
│   ├── style.css       # 主样式文件
│   └── responsive.css  # 响应式样式
├── js/
│   └── main.js        # 主要功能脚本
├── projects/
│   └── projects.json  # 项目数据文件
├── images/            # 图片资源目录
└── README.md          # 项目说明文档
```

## 🎯 自定义配置

### 添加新项目

在 `projects/projects.json` 文件中添加新的项目数据：

```json
{
  "id": 9,
  "title": "项目标题",
  "description": "项目描述",
  "category": "研究领域",
  "year": 2025,
  "tags": ["标签1", "标签2"],
  "image": "🆕",
  "details": "详细的项目描述..."
}
```

### 修改个人信息

编辑 `index.html` 文件中的联系信息部分：

```html
<div class="contact-item">
    <i class="fas fa-envelope"></i>
    <span>Email: your-email@example.com</span>
</div>
```

### 自定义样式

修改 `css/style.css` 文件来自定义颜色、字体等样式：

```css
:root {
    --primary-color: #2563eb;
    --secondary-color: #1e40af;
    --text-color: #333;
    --background-color: #fafafa;
}
```

## 🛠️ 技术栈

- **前端**: HTML5, CSS3, JavaScript (ES6+)
- **样式**: CSS Grid, Flexbox, CSS Variables
- **图标**: Font Awesome 6.0
- **字体**: Google Fonts (Inter)
- **部署**: GitHub Pages

## 📱 浏览器支持

- Chrome 60+
- Firefox 55+
- Safari 12+
- Edge 79+

## 🤝 贡献

欢迎提交 Issue 和 Pull Request 来改进这个项目！

## 📄 许可证

MIT License - 详见 [LICENSE](LICENSE) 文件

## 🙏 致谢

- [Font Awesome](https://fontawesome.com/) - 图标库
- [Google Fonts](https://fonts.google.com/) - 字体服务
- [GitHub Pages](https://pages.github.com/) - 免费托管服务

---

**构建于 2025 年，致力于知识分享和学术交流** 🚀
