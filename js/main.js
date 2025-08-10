// Main JavaScript functionality for Research Portfolio

class ResearchPortfolio {
    constructor() {
        this.projects = [];
        this.filteredProjects = [];
        this.currentFilters = {
            search: '',
            category: '',
            year: ''
        };
        
        this.init();
    }
    
    async init() {
        await this.loadProjects();
        this.setupEventListeners();
        this.renderProjects();
        this.updateStats();
        this.setupNavigation();
    }
    
    async loadProjects() {
        try {
            const response = await fetch('projects/projects.json');
            this.projects = await response.json();
            this.filteredProjects = [...this.projects];
        } catch (error) {
            console.error('Error loading projects:', error);
            // Fallback to sample data if JSON file is not available
            this.projects = this.getSampleProjects();
            this.filteredProjects = [...this.projects];
        }
    }
    
    getSampleProjects() {
        return [
            {
                id: 1,
                title: "机器学习在医疗诊断中的应用",
                description: "研究如何利用机器学习算法提高医疗诊断的准确性和效率，包括图像识别、自然语言处理等技术在医疗领域的应用。",
                category: "人工智能",
                year: 2024,
                tags: ["机器学习", "医疗", "图像识别", "Python"],
                image: "🧠",
                details: "这是一个关于机器学习在医疗领域应用的深入研究项目。我们使用了多种算法，包括卷积神经网络、循环神经网络等，来分析医疗图像和文本数据。项目取得了显著的成果，诊断准确率提升了15%。",
                folder: "machine-learning-medical-diagnosis"
            },
            {
                id: 2,
                title: "区块链技术在供应链管理中的创新应用",
                description: "探索区块链技术如何改善供应链的透明度、可追溯性和安全性，构建去中心化的供应链管理系统。",
                category: "区块链",
                year: 2024,
                tags: ["区块链", "供应链", "智能合约", "Solidity"],
                image: "🔗",
                details: "区块链供应链管理项目旨在解决传统供应链中的信任问题和信息不对称。通过智能合约和分布式账本技术，我们建立了一个透明、可追溯的供应链网络，显著提高了运营效率和安全性。",
                folder: "blockchain-supply-chain"
            },
            {
                id: 3,
                title: "量子计算在密码学中的影响研究",
                description: "分析量子计算对现有密码学体系的影响，研究后量子密码学的发展趋势和解决方案。",
                category: "量子计算",
                year: 2023,
                tags: ["量子计算", "密码学", "后量子密码", "数学"],
                image: "⚛️",
                details: "随着量子计算技术的发展，传统的RSA和椭圆曲线密码学面临着被破解的风险。本研究深入分析了量子算法对现有密码学的影响，并探索了后量子密码学的发展方向。",
                folder: "quantum-cryptography"
            },
            {
                id: 4,
                title: "生物信息学中的大数据分析方法",
                description: "开发新的算法和工具来处理和分析大规模生物数据，包括基因组学、蛋白质组学等领域的数据挖掘技术。",
                category: "生物信息学",
                year: 2023,
                tags: ["生物信息学", "大数据", "基因组学", "R语言"],
                image: "🧬",
                details: "生物信息学大数据分析项目专注于开发高效的数据处理算法。我们处理了来自多个研究机构的基因组数据，开发了新的数据挖掘工具，为生物医学研究提供了重要的技术支持。",
                folder: "bioinformatics-big-data"
            },
            {
                id: 5,
                title: "可再生能源系统的智能优化",
                description: "研究如何利用人工智能和物联网技术优化可再生能源系统的运行效率，包括太阳能、风能等清洁能源的智能管理。",
                category: "可再生能源",
                year: 2024,
                tags: ["可再生能源", "物联网", "智能优化", "机器学习"],
                image: "☀️",
                details: "智能可再生能源系统项目结合了物联网传感器、机器学习算法和预测模型，实现了对太阳能和风能系统的实时监控和智能优化。系统能够根据天气预测和历史数据自动调整运行参数，提高了能源利用效率。",
                folder: "renewable-energy-optimization"
            },
            {
                id: 6,
                title: "虚拟现实在教育领域的应用研究",
                description: "探索虚拟现实技术如何改变传统教育模式，提高学习效果和学生的参与度，特别是在科学、工程等实践性学科中的应用。",
                category: "虚拟现实",
                year: 2023,
                tags: ["虚拟现实", "教育", "沉浸式学习", "Unity"],
                image: "🥽",
                details: "VR教育应用研究项目开发了多个沉浸式学习环境，包括虚拟实验室、历史场景重现等。通过对比实验，我们发现VR技术能够显著提高学生的学习兴趣和知识 retention 率。",
                folder: "vr-education-applications"
            }
        ];
    }
    
    setupEventListeners() {
        // Search functionality
        const searchInput = document.getElementById('search-input');
        searchInput.addEventListener('input', (e) => {
            this.currentFilters.search = e.target.value;
            this.filterProjects();
        });
        
        // Category filter
        const categoryFilter = document.getElementById('category-filter');
        categoryFilter.addEventListener('change', (e) => {
            this.currentFilters.category = e.target.value;
            this.filterProjects();
        });
        
        // Year filter
        const yearFilter = document.getElementById('year-filter');
        yearFilter.addEventListener('change', (e) => {
            this.currentFilters.year = e.target.value;
            this.filterProjects();
        });
        
        // Clear filters
        const clearBtn = document.getElementById('clear-filters');
        clearBtn.addEventListener('change', () => {
            this.clearFilters();
        });
        
        // Modal functionality
        this.setupModal();
    }
    
    setupNavigation() {
        const navToggle = document.getElementById('nav-toggle');
        const navMenu = document.getElementById('nav-menu');
        
        navToggle.addEventListener('click', () => {
            navMenu.classList.toggle('active');
            navToggle.classList.toggle('active');
        });
        
        // Close mobile menu when clicking on a link
        const navLinks = document.querySelectorAll('.nav-link');
        navLinks.forEach(link => {
            link.addEventListener('click', () => {
                navMenu.classList.remove('active');
                navToggle.classList.remove('active');
            });
        });
    }
    
    setupModal() {
        const modal = document.getElementById('project-modal');
        const closeBtn = document.querySelector('.close');
        
        closeBtn.addEventListener('click', () => {
            modal.style.display = 'none';
        });
        
        window.addEventListener('click', (e) => {
            if (e.target === modal) {
                modal.style.display = 'none';
            }
        });
    }
    
    filterProjects() {
        this.filteredProjects = this.projects.filter(project => {
            const matchesSearch = !this.currentFilters.search || 
                project.title.toLowerCase().includes(this.currentFilters.search.toLowerCase()) ||
                project.description.toLowerCase().includes(this.currentFilters.search.toLowerCase()) ||
                project.tags.some(tag => tag.toLowerCase().includes(this.currentFilters.search.toLowerCase()));
            
            const matchesCategory = !this.currentFilters.category || 
                project.category === this.currentFilters.category;
            
            const matchesYear = !this.currentFilters.year || 
                project.year.toString() === this.currentFilters.year;
            
            return matchesSearch && matchesCategory && matchesYear;
        });
        
        this.renderProjects();
        this.updateFilters();
    }
    
    clearFilters() {
        this.currentFilters = {
            search: '',
            category: '',
            year: ''
        };
        
        document.getElementById('search-input').value = '';
        document.getElementById('category-filter').value = '';
        document.getElementById('year-filter').value = '';
        
        this.filteredProjects = [...this.projects];
        this.renderProjects();
        this.updateFilters();
    }
    
    updateFilters() {
        // Update category filter options
        const categoryFilter = document.getElementById('category-filter');
        const categories = [...new Set(this.projects.map(p => p.category))];
        
        categoryFilter.innerHTML = '<option value="">所有领域</option>';
        categories.forEach(category => {
            const option = document.createElement('option');
            option.value = category;
            option.textContent = category;
            categoryFilter.appendChild(option);
        });
        
        // Update year filter options
        const yearFilter = document.getElementById('year-filter');
        const years = [...new Set(this.projects.map(p => p.year))].sort((a, b) => b - a);
        
        yearFilter.innerHTML = '<option value="">所有年份</option>';
        years.forEach(year => {
            const option = document.createElement('option');
            option.value = year;
            option.textContent = year;
            yearFilter.appendChild(option);
        });
    }
    
    renderProjects() {
        const projectsGrid = document.getElementById('projects-grid');
        const noResults = document.getElementById('no-results');
        
        if (this.filteredProjects.length === 0) {
            projectsGrid.innerHTML = '';
            noResults.style.display = 'block';
            return;
        }
        
        noResults.style.display = 'none';
        
        projectsGrid.innerHTML = this.filteredProjects.map(project => `
            <div class="project-card">
                <div class="project-image">
                    ${project.image}
                </div>
                <div class="project-content">
                    <h3 class="project-title">${project.title}</h3>
                    <p class="project-description">${project.description}</p>
                    <div class="project-meta">
                        <span class="project-category">${project.category}</span>
                        <span class="project-year">${project.year}</span>
                    </div>
                    <div class="project-tags">
                        ${project.tags.map(tag => `<span class="project-tag">${tag}</span>`).join('')}
                    </div>
                    <div class="project-actions">
                        <button class="btn btn-primary" onclick="portfolio.showProjectDetails(${project.id})">
                            <span class="btn-icon">📋</span>
                            查看详情
                        </button>
                        ${project.folder ? `
                            <button class="btn btn-secondary" onclick="portfolio.openProjectFolder('${project.folder}')">
                                <span class="btn-icon">📁</span>
                                打开文件夹
                            </button>
                        ` : ''}
                    </div>
                </div>
            </div>
        `).join('');
    }
    
    showProjectDetails(projectId) {
        const project = this.projects.find(p => p.id === projectId);
        if (!project) return;
        
        const modal = document.getElementById('project-modal');
        const modalContent = document.getElementById('modal-content');
        
        modalContent.innerHTML = `
            <div class="project-details">
                <h2>${project.title}</h2>
                <div class="project-meta-details">
                    <span class="project-category">${project.category}</span>
                    <span class="project-year">${project.year}</span>
                </div>
                <p class="project-description-full">${project.details}</p>
                <div class="project-tags-full">
                    <strong>技术标签：</strong>
                    ${project.tags.map(tag => `<span class="project-tag">${tag}</span>`).join('')}
                </div>
                <div class="project-actions-modal">
                    ${project.folder ? `
                        <button class="btn btn-primary" onclick="portfolio.openProjectFolder('${project.folder}')">
                            <span class="btn-icon">📁</span>
                            打开项目文件夹
                        </button>
                    ` : ''}
                    <button class="btn btn-secondary" onclick="portfolio.closeModal()">
                        <span class="btn-icon">✕</span>
                        关闭
                    </button>
                </div>
            </div>
        `;
        
        modal.style.display = 'block';
    }
    
    updateStats() {
        const projectCount = document.getElementById('project-count');
        const categoryCount = document.getElementById('category-count');
        const yearCount = document.getElementById('year-count');
        
        if (projectCount) projectCount.textContent = this.projects.length;
        if (categoryCount) categoryCount.textContent = [...new Set(this.projects.map(p => p.category))].length;
        if (yearCount) yearCount.textContent = [...new Set(this.projects.map(p => p.year))].length;
    }

    openProjectFolder(folderName) {
        // 构建文件夹路径
        const folderPath = `projects/${folderName}`;
        
        // 检查文件夹是否存在
        fetch(folderPath + '/README.md')
            .then(response => {
                if (response.ok) {
                    // 如果文件夹存在，尝试打开
                    this.navigateToFolder(folderPath);
                } else {
                    // 如果文件夹不存在，显示提示
                    this.showFolderNotFoundMessage(folderName);
                }
            })
            .catch(error => {
                console.error('Error checking folder:', error);
                this.showFolderNotFoundMessage(folderName);
            });
    }
    
    navigateToFolder(folderPath) {
        try {
            // 尝试使用文件协议打开文件夹
            // 注意：由于浏览器安全限制，直接打开本地文件夹可能受限
            // 这里我们提供多种方式供用户选择
            
            const message = `
                <div class="folder-navigation">
                    <h3>📁 打开文件夹: ${folderPath.split('/').pop()}</h3>
                    <p>由于浏览器安全限制，无法直接打开本地文件夹。请选择以下方式之一：</p>
                    <div class="navigation-options">
                        <button class="btn btn-primary" onclick="portfolio.copyFolderPath('${folderPath}')">
                            📋 复制文件夹路径
                        </button>
                        <button class="btn btn-secondary" onclick="portfolio.openInFileManager('${folderPath}')">
                            🔧 在文件管理器中打开
                        </button>
                        <button class="btn btn-secondary" onclick="portfolio.showFolderContents('${folderPath}')">
                            📖 查看文件夹内容
                        </button>
                    </div>
                </div>
            `;
            
            this.showCustomModal('文件夹导航', message);
        } catch (error) {
            console.error('Error navigating to folder:', error);
            this.showFolderNotFoundMessage(folderPath.split('/').pop());
        }
    }
    
    copyFolderPath(folderPath) {
        // 复制文件夹路径到剪贴板
        navigator.clipboard.writeText(folderPath).then(() => {
            this.showToast('文件夹路径已复制到剪贴板！', 'success');
        }).catch(() => {
            // 如果剪贴板API不可用，使用传统方法
            const textArea = document.createElement('textarea');
            textArea.value = folderPath;
            document.body.appendChild(textArea);
            textArea.select();
            document.execCommand('copy');
            document.body.removeChild(textArea);
            this.showToast('文件夹路径已复制到剪贴板！', 'success');
        });
    }
    
    openInFileManager(folderPath) {
        // 尝试使用系统默认文件管理器打开
        try {
            // 在Linux系统上，尝试使用xdg-open
            if (navigator.platform.indexOf('Linux') !== -1) {
                // 这里可以尝试调用系统命令，但需要用户授权
                this.showToast('请在终端中运行: xdg-open ' + folderPath, 'info');
            } else {
                this.showToast('请手动在文件管理器中打开: ' + folderPath, 'info');
            }
        } catch (error) {
            this.showToast('无法自动打开文件管理器，请手动导航到: ' + folderPath, 'info');
        }
    }
    
    async showFolderContents(folderPath) {
        try {
            // 尝试读取文件夹内容
            const response = await fetch(folderPath + '/README.md');
            if (response.ok) {
                const content = await response.text();
                const message = `
                    <div class="folder-contents">
                        <h3>📁 ${folderPath.split('/').pop()} 文件夹内容</h3>
                        <div class="readme-content">
                            <pre>${content}</pre>
                        </div>
                        <div class="folder-actions">
                            <button class="btn btn-primary" onclick="portfolio.copyFolderPath('${folderPath}')">
                                📋 复制路径
                            </button>
                            <button class="btn btn-secondary" onclick="portfolio.closeCustomModal()">
                                关闭
                            </button>
                        </div>
                    </div>
                `;
                this.showCustomModal('文件夹内容', message);
            } else {
                this.showToast('无法读取文件夹内容', 'error');
            }
        } catch (error) {
            this.showToast('读取文件夹内容时出错', 'error');
        }
    }
    
    showFolderNotFoundMessage(folderName) {
        this.showToast(`文件夹 "${folderName}" 不存在或无法访问`, 'error');
    }
    
    showCustomModal(title, content) {
        // 创建自定义模态框
        let modal = document.getElementById('custom-modal');
        if (!modal) {
            modal = document.createElement('div');
            modal.id = 'custom-modal';
            modal.className = 'modal custom-modal';
            modal.innerHTML = `
                <div class="modal-content custom-modal-content">
                    <div class="modal-header">
                        <h2 id="custom-modal-title"></h2>
                        <span class="close" onclick="portfolio.closeCustomModal()">&times;</span>
                    </div>
                    <div id="custom-modal-body"></div>
                </div>
            `;
            document.body.appendChild(modal);
        }
        
        document.getElementById('custom-modal-title').textContent = title;
        document.getElementById('custom-modal-body').innerHTML = content;
        modal.style.display = 'block';
    }
    
    closeCustomModal() {
        const modal = document.getElementById('custom-modal');
        if (modal) {
            modal.style.display = 'none';
        }
    }
    
    closeModal() {
        const modal = document.getElementById('project-modal');
        if (modal) {
            modal.style.display = 'none';
        }
    }
    
    showToast(message, type = 'info') {
        // 创建toast通知
        const toast = document.createElement('div');
        toast.className = `toast toast-${type}`;
        toast.innerHTML = `
            <span class="toast-message">${message}</span>
            <button class="toast-close" onclick="this.parentElement.remove()">&times;</button>
        `;
        
        document.body.appendChild(toast);
        
        // 自动移除
        setTimeout(() => {
            if (toast.parentElement) {
                toast.remove();
            }
        }, 5000);
    }
}

// Initialize the portfolio when DOM is loaded
document.addEventListener('DOMContentLoaded', () => {
    window.portfolio = new ResearchPortfolio();
});

// Smooth scrolling for navigation links
document.querySelectorAll('a[href^="#"]').forEach(anchor => {
    anchor.addEventListener('click', function (e) {
        e.preventDefault();
        const target = document.querySelector(this.getAttribute('href'));
        if (target) {
            target.scrollIntoView({
                behavior: 'smooth',
                block: 'start'
            });
        }
    });
});

// Add loading animation
window.addEventListener('load', () => {
    document.body.classList.add('loaded');
});

// Add scroll effect for header
window.addEventListener('scroll', () => {
    const header = document.querySelector('.header');
    if (window.scrollY > 100) {
        header.style.background = 'rgba(255, 255, 255, 0.98)';
    } else {
        header.style.background = 'rgba(255, 255, 255, 0.95)';
    }
});