# Herbiverse 数据更新与重现指南

## 项目概述

Herbiverse是一个结构化的香草植物数据库，包含可食用香草和有毒植物的详细信息。本文档提供数据增量更新（upsert）和项目重现的完整指南。

## 核心数据结构

### 1. 数据文件组织
```
Herbiverse/
├── data/              # 原始数据文件 (*.raw.json)
├── json/              # 处理后的JSON文件
├── markdown/          # 生成的Markdown文档
├── images/            # 植物图片
├── references/        # 参考文献
├── scripts/           # 处理脚本
│   ├── herb_collector.py    # 数据收集
│   ├── herb_processor.py    # 数据处理
│   ├── image_fetcher.py     # 图片获取
│   └── source_validator.py  # 来源验证
├── templates/         # 模板文件
├── config.json        # 项目配置
├── schema.json        # 数据验证模式
└── index.json         # 综合索引

```

### 2. 数据条目必需字段

每个植物条目必须包含以下字段：

```json
{
  "id": "herb-name",              // 小写字母和连字符
  "cn_name": "中文名",
  "en_name": "English Name",
  "scientific_name": "Genus species",
  "category": "edible|non_edible",
  "edible_parts": ["leaves", "stem", "flower", "root", "seed", "fruit", "bark"],
  "toxicity": {
    "status": "safe|caution|toxic|external_only",
    "toxic_parts": [],
    "notes": "安全说明",
    "sources": ["权威来源"]
  },
  "cultivation": {
    "hardiness": "USDA zones",
    "sun": "光照需求",
    "soil": "土壤类型",
    "pH": "pH范围",
    "watering": "浇水需求",
    "propagation": ["繁殖方法"],
    "companion": ["伴生植物"],
    "pests": ["常见病虫害"]
  },
  "varieties": [{"name": "品种名"}],
  "chemistry": [
    {
      "compound": "化合物名",
      "class": "化学类别",
      "role": "作用",
      "notes": "说明",
      "sources": ["来源"]
    }
  ],
  "culinary_uses": ["烹饪用途"],
  "storage": "储存方法",
  "lookalikes": [
    {
      "name": "相似植物",
      "how_to_tell": "区分方法"
    }
  ],
  "references": [
    {
      "title": "文献标题",
      "org": "机构",
      "year": 2024,
      "url": "链接"
    }
  ],
  "tags": ["标签"]
}
```

### 3. 富文本扩展字段（1200+字要求）

```json
{
  "key_features": "关键特征简述",
  "difficulty": "栽培难度",
  "morphology_content": "形态学详细描述（200+字）",
  "chemistry_content": "化学成分深入分析",
  "culinary_content": "烹饪应用完整指南",
  "varieties_content": "品种详细对比",
  "storage_methods": "多种储存技术",
  "toxicology_content": "毒理学完整信息（有毒植物必需）",
  "lookalike_comparison": "相似植物详细区分"
}
```

## 数据更新流程（Upsert）

### 步骤1：添加新植物条目

```bash
# 1. 创建原始数据文件
cat > data/new_herb.raw.json << 'EOF'
{
  "id": "new-herb",
  "cn_name": "新香草",
  "en_name": "New Herb",
  "scientific_name": "Newus herbius",
  "category": "edible",
  # ... 完整数据结构
}
EOF

# 2. 处理单个条目
python scripts/herb_processor.py --id new-herb

# 3. 或处理所有条目
python scripts/herb_processor.py --all
```

### 步骤2：更新现有条目

```bash
# 1. 编辑原始数据
vim data/existing_herb.raw.json

# 2. 重新处理
python scripts/herb_processor.py --id existing-herb

# 3. 验证更新
python scripts/source_validator.py --id existing-herb
```

### 步骤3：更新索引

```python
# 运行索引更新脚本
import json
import glob

def update_index():
    index = {
        "metadata": {
            "version": "1.0",
            "last_updated": "2025-01-14",
            "description": "Herbiverse项目植物索引系统"
        },
        "alphabetical_index": [],
        "chinese_index": [],
        "category_index": {"edible": [], "non_edible": []},
        "usage_index": {"culinary": [], "medicinal": [], "ornamental": [], "aromatic": []},
        "difficulty_index": {"easy": [], "medium": [], "hard": []},
        "statistics": {}
    }
    
    # 遍历所有JSON文件
    for file in glob.glob("json/*.json"):
        if not file.endswith('.draft.json'):
            with open(file) as f:
                herb = json.load(f)
                # 添加到各个索引
                # ... 索引逻辑
    
    with open("index.json", "w") as f:
        json.dump(index, f, indent=2, ensure_ascii=False)

update_index()
```

## 项目重现步骤

### 1. 环境准备

```bash
# 克隆或创建项目目录
mkdir -p /workspace/self-evolve-ai/projects/Herbiverse
cd /workspace/self-evolve-ai/projects/Herbiverse

# 创建目录结构
mkdir -p {data,json,markdown,images,references,scripts,templates}

# 安装依赖
pip install --user jsonschema jinja2 requests beautifulsoup4
```

### 2. 初始化配置文件

```bash
# 创建 config.json
cat > config.json << 'EOF'
{
  "project": "Herbiverse",
  "version": "1.0.0",
  "languages": ["zh-CN", "en"],
  "minWordCount": 1200,
  "trustedSources": {
    "priority1": ["RHS", "Kew Gardens", "USDA", "Missouri Botanical Garden"],
    "priority2": ["PubChem", "ChemSpider", "WHO", "EFSA", "FDA"],
    "priority3": ["Wikipedia", "Academic Papers", "Traditional References"]
  },
  "requiredSections": [
    "morphology", "cultivation", "chemistry", "culinary", "safety"
  ]
}
EOF

# 创建 schema.json（从现有项目复制）
cp /path/to/existing/schema.json ./schema.json
```

### 3. 批量数据处理

```bash
# 处理所有原始数据
for file in data/*.raw.json; do
    herb_id=$(basename "$file" .raw.json)
    echo "Processing $herb_id..."
    python scripts/herb_processor.py --id "$herb_id"
done

# 生成索引
python -c "
import json
import os
from pathlib import Path

# 读取所有处理后的JSON
herbs = []
for f in Path('json').glob('*.json'):
    if not f.name.endswith('.draft.json'):
        with open(f) as file:
            herbs.append(json.load(file))

# 生成各种索引
# ... 索引生成逻辑

print(f'Indexed {len(herbs)} herbs')
"
```

### 4. 质量验证

```bash
# 验证数据完整性
python scripts/source_validator.py --all

# 检查字数要求
for file in markdown/*.md; do
    wc -w "$file"
done | awk '$1 < 1200 {print "需要扩充: " $2}'

# 验证schema合规性
python -c "
import json
import jsonschema
from pathlib import Path

with open('schema.json') as f:
    schema = json.load(f)

errors = []
for file in Path('json').glob('*.json'):
    if not file.name.endswith('.draft.json'):
        with open(file) as f:
            try:
                jsonschema.validate(json.load(f), schema)
            except Exception as e:
                errors.append(f'{file.name}: {e}')

if errors:
    for e in errors:
        print(e)
else:
    print('所有文件通过schema验证')
"
```

## 数据质量标准

### 可食用香草要求
- 详细的烹饪应用（至少5种用途）
- 储存方法（新鲜、干燥、冷冻）
- 品种对比（风味差异）
- 栽培难度评级
- 伴生植物建议

### 有毒植物要求
- ⚠️ 明确的毒性警告
- 详细的中毒症状（按时间进展）
- 急救措施（步骤化）
- 致死剂量（如已知）
- 易混淆植物的明确区分

### 化学成分要求
- 至少3-4种主要化合物
- 化学类别标注
- 生物活性说明
- 含量范围（如已知）
- 参考来源

### 参考文献要求
- 优先级1：Kew Gardens, USDA, WHO
- 优先级2：PubChem, 学术期刊
- 优先级3：传统文献、百科全书
- 每个条目至少3个权威来源

## 常见问题处理

### 1. Schema验证失败

```bash
# ID格式错误（包含下划线）
"id": "herb_name"  # ❌ 错误
"id": "herb-name"  # ✅ 正确

# 毒性状态值错误
"status": "extremely_toxic"  # ❌ 错误
"status": "toxic"            # ✅ 正确

# 可食部位枚举错误
"edible_parts": ["leaves", "roots"]  # ❌ roots应为root
"edible_parts": ["leaves", "root"]   # ✅ 正确
```

### 2. 字数不足处理

```python
# 扩充内容的重点领域
rich_text_fields = {
    "morphology_content": "添加更多形态细节、季节变化、生长阶段",
    "chemistry_content": "深入解释化合物作用机制、生物合成途径",
    "culinary_content": "增加地域料理差异、历史演变、现代创新",
    "toxicology_content": "详述毒理机制、历史案例、解毒方法"
}
```

### 3. 图片处理

```python
# 使用 image_fetcher.py 获取CC授权图片
python scripts/image_fetcher.py --herb "herb-name" --license "CC-BY"
```

## 自动化脚本

### 完整的upsert脚本

```bash
#!/bin/bash
# upsert_herb.sh - 添加或更新香草条目

HERB_ID=$1
if [ -z "$HERB_ID" ]; then
    echo "Usage: ./upsert_herb.sh <herb-id>"
    exit 1
fi

echo "Processing $HERB_ID..."

# 1. 验证raw.json存在
if [ ! -f "data/${HERB_ID}.raw.json" ]; then
    echo "Error: data/${HERB_ID}.raw.json not found"
    exit 1
fi

# 2. 处理数据
python scripts/herb_processor.py --id "$HERB_ID"

# 3. 更新索引
python -c "
import json
# ... 索引更新逻辑
print('Index updated')
"

# 4. 验证结果
if [ -f "json/${HERB_ID}.json" ]; then
    echo "✅ Successfully processed $HERB_ID"
    echo "   - JSON: json/${HERB_ID}.json"
    echo "   - Markdown: markdown/${HERB_ID}.md"
else
    echo "⚠️  Draft created: json/${HERB_ID}.draft.json"
    echo "   Please fix validation errors"
fi
```

## 维护建议

1. **定期更新**：每季度检查科学文献更新
2. **版本控制**：使用Git跟踪所有数据变更
3. **备份策略**：定期备份raw.json原始数据
4. **质量审核**：新条目需要交叉验证来源
5. **用户反馈**：建立反馈机制收集使用建议

## 联系信息

项目维护：Herbiverse Team
更新日期：2025-01-14
版本：v1.0

---

使用本指南可以：
- 🔄 增量更新数据（upsert）
- 🔁 完整重现项目
- ✅ 保证数据质量
- 📊 维护索引一致性