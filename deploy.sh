#!/bin/bash

# 🚀 GitHub Pages 快速部署脚本
# Usage: ./deploy.sh [repository-url]

set -e  # 出错时停止脚本

echo "🚀 Research Portfolio 部署脚本"
echo "================================"

# 检查是否提供了仓库URL
if [ $# -eq 0 ]; then
    echo "❌ 请提供您的GitHub仓库URL"
    echo "用法: ./deploy.sh https://github.com/yourusername/research-portfolio.git"
    echo ""
    echo "或者手动执行以下命令："
    echo "1. git remote add origin https://github.com/yourusername/research-portfolio.git"
    echo "2. git push -u origin main"
    exit 1
fi

REPO_URL=$1

echo "📁 当前目录: $(pwd)"
echo "🔗 仓库URL: $REPO_URL"
echo ""

# 检查Git状态
if [ ! -d ".git" ]; then
    echo "❌ 当前目录不是Git仓库"
    exit 1
fi

# 检查是否有未提交的更改
if [ -n "$(git status --porcelain)" ]; then
    echo "⚠️  检测到未提交的更改，正在提交..."
    git add .
    git commit -m "Pre-deployment updates"
fi

# 检查是否已添加远程仓库
if ! git remote | grep -q origin; then
    echo "🔗 添加远程仓库..."
    git remote add origin "$REPO_URL"
else
    echo "✅ 远程仓库已存在"
    # 更新远程仓库URL（如果不同）
    current_url=$(git remote get-url origin)
    if [ "$current_url" != "$REPO_URL" ]; then
        echo "🔄 更新远程仓库URL..."
        git remote set-url origin "$REPO_URL"
    fi
fi

# 推送到GitHub
echo "📤 推送代码到GitHub..."
if git push -u origin main; then
    echo ""
    echo "🎉 代码推送成功！"
    echo ""
    echo "📋 接下来的步骤："
    echo "1. 访问您的GitHub仓库"
    echo "2. 点击 Settings → Pages"
    echo "3. Source 选择 'Deploy from a branch'"
    echo "4. 分支选择 'main'，文件夹选择 '/ (root)'"
    echo "5. 点击 Save 等待部署完成"
    echo ""
    echo "🌐 部署完成后，您的网站将在以下地址可用："
    
    # 从仓库URL提取用户名和仓库名
    if [[ $REPO_URL =~ github\.com[:/]([^/]+)/([^/]+)\.git$ ]]; then
        USERNAME=${BASH_REMATCH[1]}
        REPONAME=${BASH_REMATCH[2]}
        echo "   https://$USERNAME.github.io/$REPONAME/"
    else
        echo "   https://yourusername.github.io/repository-name/"
    fi
    
    echo ""
    echo "📖 查看部署检查清单: cat DEPLOY_CHECKLIST.md"
    echo "📚 详细部署指南: cat DEPLOYMENT.md"
    
else
    echo ""
    echo "❌ 推送失败！"
    echo "请检查："
    echo "1. GitHub仓库URL是否正确"
    echo "2. 是否有推送权限"
    echo "3. 网络连接是否正常"
    echo ""
    echo "🔧 手动推送命令："
    echo "   git remote add origin $REPO_URL"
    echo "   git push -u origin main"
    exit 1
fi
