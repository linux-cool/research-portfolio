# 部署指南 - Deployment Guide

本指南将帮助您将 Research Portfolio 网站部署到 GitHub Pages 或其他托管服务。

## 🚀 GitHub Pages 部署（推荐）

### 1. 创建 GitHub 仓库

1. 访问 [GitHub](https://github.com) 并登录您的账户
2. 点击右上角的 "+" 按钮，选择 "New repository"
3. 填写仓库信息：
   - **Repository name**: `research-portfolio` (或您喜欢的名称)
   - **Description**: `个人研究项目展示网站`
   - **Visibility**: 选择 Public 或 Private
   - **不要**勾选 "Add a README file" (我们已经有了)
4. 点击 "Create repository"

### 2. 上传项目文件

#### 方法一：使用 Git 命令行（推荐）

```bash
# 进入项目目录
cd research-portfolio

# 初始化 Git 仓库
git init

# 添加远程仓库
git remote add origin https://github.com/yourusername/research-portfolio.git

# 添加所有文件
git add .

# 提交更改
git commit -m "Initial commit: Research Portfolio website"

# 推送到主分支
git branch -M main
git push -u origin main
```

#### 方法二：使用 GitHub Desktop

1. 下载并安装 [GitHub Desktop](https://desktop.github.com/)
2. 登录您的 GitHub 账户
3. 选择 "Clone a repository from the Internet"
4. 选择您刚创建的仓库
5. 选择本地保存路径
6. 将项目文件复制到该目录
7. 在 GitHub Desktop 中提交并推送更改

### 3. 启用 GitHub Pages

1. 在您的 GitHub 仓库页面，点击 "Settings" 标签
2. 在左侧菜单中找到 "Pages"
3. 在 "Source" 部分：
   - 选择 "Deploy from a branch"
   - 选择 "main" 分支
   - 选择 "/(root)" 文件夹
4. 点击 "Save"
5. 等待几分钟，GitHub 将显示您的网站 URL

### 4. 自定义域名（可选）

如果您有自己的域名：

1. 在 "Custom domain" 字段中输入您的域名
2. 点击 "Save"
3. 在您的域名提供商处添加 CNAME 记录，指向 `yourusername.github.io`
4. 等待 DNS 传播（可能需要几小时到 24 小时）

## 🌐 其他托管服务

### Netlify

1. 访问 [Netlify](https://netlify.com) 并注册账户
2. 点击 "New site from Git"
3. 选择 GitHub 并授权访问
4. 选择您的仓库
5. 部署设置：
   - **Build command**: 留空（静态网站）
   - **Publish directory**: `.` (根目录)
6. 点击 "Deploy site"

### Vercel

1. 访问 [Vercel](https://vercel.com) 并注册账户
2. 点击 "New Project"
3. 导入您的 GitHub 仓库
4. 部署设置：
   - **Framework Preset**: Other
   - **Root Directory**: `./`
5. 点击 "Deploy"

### 传统虚拟主机

1. 将所有项目文件上传到您的虚拟主机
2. 确保 `index.html` 在网站根目录
3. 检查文件权限设置

## 🔧 部署后配置

### 1. 更新联系信息

编辑 `index.html` 文件中的联系信息：

```html
<div class="contact-item">
    <i class="fas fa-envelope"></i>
    <span>Email: your-real-email@example.com</span>
</div>
<div class="contact-item">
    <i class="fab fa-github"></i>
    <span>GitHub: github.com/yourusername</span>
</div>
```

### 2. 添加 Google Analytics（可选）

在 `index.html` 的 `<head>` 部分添加：

```html
<!-- Google Analytics -->
<script async src="https://www.googletagmanager.com/gtag/js?id=GA_MEASUREMENT_ID"></script>
<script>
  window.dataLayer = window.dataLayer || [];
  function gtag(){dataLayer.push(arguments);}
  gtag('js', new Date());
  gtag('config', 'GA_MEASUREMENT_ID');
</script>
```

### 3. 添加网站图标

1. 准备一个 32x32 或 64x64 像素的图标文件
2. 将图标文件命名为 `favicon.ico` 并放在项目根目录
3. 在 `index.html` 的 `<head>` 部分添加：

```html
<link rel="icon" type="image/x-icon" href="favicon.ico">
```

## 📱 测试部署

### 1. 功能测试

- [ ] 网站能够正常加载
- [ ] 导航菜单工作正常
- [ ] 搜索和筛选功能正常
- [ ] 项目详情模态框正常显示
- [ ] 响应式设计在不同设备上正常

### 2. 性能测试

- [ ] 页面加载速度
- [ ] 图片加载优化
- [ ] 移动端性能

### 3. SEO 测试

- [ ] 页面标题和描述
- [ ] 结构化数据
- [ ] 移动端友好性

## 🚨 常见问题

### 1. 网站无法访问

- 检查 GitHub Pages 是否已启用
- 确认部署分支和目录设置正确
- 等待几分钟让部署完成

### 2. 样式或脚本无法加载

- 检查文件路径是否正确
- 确认所有文件都已上传
- 检查浏览器控制台是否有错误

### 3. 搜索功能不工作

- 确认 `projects.json` 文件存在且格式正确
- 检查浏览器控制台是否有 JavaScript 错误
- 验证文件路径和权限设置

## 🔄 更新网站

### 1. 添加新项目

1. 编辑 `projects/projects.json` 文件
2. 添加新的项目数据
3. 提交并推送更改：

```bash
git add .
git commit -m "Add new research project"
git push
```

### 2. 修改样式或功能

1. 编辑相应的 CSS 或 JavaScript 文件
2. 测试本地更改
3. 提交并推送更改

## 📚 更多资源

- [GitHub Pages 官方文档](https://pages.github.com/)
- [Netlify 部署指南](https://docs.netlify.com/)
- [Vercel 部署指南](https://vercel.com/docs)
- [Web 性能优化指南](https://web.dev/performance/)

---

**部署成功后，您的个人研究项目展示网站就可以在互联网上访问了！** 🎉

如果您在部署过程中遇到任何问题，请检查：
1. 文件路径和权限设置
2. 浏览器控制台的错误信息
3. 托管服务的部署日志
