# Herbiverse 香草植物数据库

## 项目简介

Herbiverse是一个综合性的香草植物数据库项目，致力于收集、整理和分享关于香草植物的详细信息。项目包含植物的形态特征、栽培方法、化学成分、烹饪用途、毒性信息等全方位数据，为园艺爱好者、厨师、研究人员提供可靠的参考资料。

### 项目特色
- **科学准确**: 基于权威植物学数据库和科学文献
- **实用导向**: 包含栽培难度、烹饪应用、储存方法等实用信息
- **安全第一**: 详细标注有毒植物，提供安全使用指导
- **多语言支持**: 提供中英文对照的植物名称和描述
- **结构化数据**: JSON格式便于程序化处理和查询

## 目录结构

```
Herbiverse/
├── index.json                 # 项目索引文件
├── README.md                 # 项目说明文档
├── schema.json               # 数据结构规范
├── json/                     # 植物JSON数据文件
│   ├── basil.json           # 罗勒
│   ├── mint.json            # 薄荷
│   ├── cilantro.json        # 香菜/芫荽
│   ├── parsley.json         # 欧芹
│   ├── chives.json          # 韭葱/细香葱
│   ├── dill.json            # 莳萝/刁草
│   ├── oregano.json         # 牛至/披萨草
│   ├── thyme.json           # 百里香
│   ├── rosemary.json        # 迷迭香
│   ├── sage.json            # 鼠尾草
│   ├── oleander.json        # 夹竹桃【有毒】
│   └── foxglove.json        # 毛地黄【有毒】
├── markdown/                # Markdown格式文档
├── data/                    # 原始数据
├── images/                  # 植物图片
├── references/              # 参考文献
├── templates/               # 模板文件
└── scripts/                 # 处理脚本
    ├── herb_collector.py    # 数据收集
    ├── herb_processor.py    # 数据处理
    ├── image_fetcher.py     # 图片获取
    └── source_validator.py  # 来源验证
```

## 香草图鉴完整列表

### 🌿 可食用香草植物 (Edible Herbs)

#### 经典西式香草
1. **[罗勒 (Basil)](markdown/basil.md)** - *Ocimum basilicum*
   - 难度: 容易 | 特色: 意式料理经典，制作青酱必备

2. **[月桂叶 (Bay Leaf)](markdown/bay_leaf.md)** - *Laurus nobilis*
   - 难度: 中等 | 特色: 炖煮增香，地中海料理基础

3. **[细叶芹/雪维菜 (Chervil)](markdown/chervil.md)** - *Anthriscus cerefolium*
   - 难度: 中等 | 特色: 法式细香草之一，口感细腻

4. **[韭葱/细香葱 (Chives)](markdown/chives.md)** - *Allium schoenoprasum*
   - 难度: 极易 | 特色: 温和葱香，装饰首选

5. **[香菜/芫荽 (Cilantro)](markdown/cilantro.md)** - *Coriandrum sativum*
   - 难度: 中等 | 特色: 亚洲料理必备，叶籽两用

6. **[莳萝/刁草 (Dill)](markdown/dill.md)** - *Anethum graveolens*
   - 难度: 容易 | 特色: 北欧料理灵魂，腌制专用

7. **[马郁兰 (Marjoram)](markdown/marjoram.md)** - *Origanum majorana*
   - 难度: 容易 | 特色: 温和版牛至，欧洲香肠调料

8. **[薄荷 (Mint)](markdown/mint.md)** - *Mentha spp.*
   - 难度: 容易 | 特色: 清凉感强烈，茶饮甜点搭配

9. **[牛至/披萨草 (Oregano)](markdown/oregano.md)** - *Origanum vulgare*
   - 难度: 容易 | 特色: 披萨意面经典调料

10. **[欧芹 (Parsley)](markdown/parsley.md)** - *Petroselinum crispum*
    - 难度: 中等 | 特色: 地中海料理基础，营养丰富

11. **[迷迭香 (Rosemary)](markdown/rosemary.md)** - *Rosmarinus officinalis*
    - 难度: 容易 | 特色: 松香浓郁，烤肉专用

12. **[鼠尾草 (Sage)](markdown/sage.md)** - *Salvia officinalis*
    - 难度: 容易 | 特色: 感恩节火鸡调料，意式黄油酱

13. **[龙蒿 (Tarragon)](markdown/tarragon.md)** - *Artemisia dracunculus*
    - 难度: 中等 | 特色: 法式料理之王，贝亚恩酱必备

14. **[百里香 (Thyme)](markdown/thyme.md)** - *Thymus vulgaris*
    - 难度: 容易 | 特色: 法式香草束核心成员

#### 亚洲特色香草
15. **[刺芫荽 (Culantro)](markdown/culantro.md)** - *Eryngium foetidum*
    - 难度: 中等 | 特色: 加勒比和拉美料理，香菜加强版

16. **[柠檬草 (Lemongrass)](markdown/lemongrass.md)** - *Cymbopogon citratus*
    - 难度: 容易 | 特色: 东南亚汤品灵魂，冬阴功必备

17. **[青柠叶 (Kaffir Lime Leaf)](markdown/kaffir_lime_leaf.md)** - *Citrus hystrix*
    - 难度: 中等 | 特色: 泰式咖喱精髓，独特柑橘香

18. **[紫苏 (Shiso)](markdown/shiso.md)** - *Perilla frutescens*
    - 难度: 容易 | 特色: 日韩料理特色，寿司伴侣

19. **[越南香菜 (Vietnamese Coriander)](markdown/vietnamese_coriander.md)** - *Persicaria odorata*
    - 难度: 容易 | 特色: 越南河粉标配，耐热香菜替代品

#### 药用香草
20. **[香蜂草 (Lemon Balm)](markdown/lemon_balm.md)** - *Melissa officinalis*
    - 难度: 容易 | 特色: 柠檬香味，安神助眠茶

### ☠️ 有毒植物 (Toxic Plants)

⚠️ **严重警告: 以下植物含有剧毒成分，仅供观赏和教育用途，严禁食用或药用**

21. **[颠茄 (Belladonna)](markdown/belladonna.md)** - *Atropa belladonna* 💀
    - 毒性: 极毒 | 含阿托品等生物碱，可致命

22. **[蓖麻 (Castor Bean)](markdown/castor_bean.md)** - *Ricinus communis* 💀
    - 毒性: 极毒 | 含蓖麻毒素，世界最毒植物之一

23. **[聚合草 (Comfrey)](markdown/comfrey.md)** - *Symphytum officinale* ⚠️
    - 毒性: 肝毒性 | 含吡咯里西啶生物碱

24. **[曼陀罗 (Datura)](markdown/datura.md)** - *Datura stramonium* 💀
    - 毒性: 极毒 | 含莨菪碱，致幻致命

25. **[毛地黄 (Foxglove)](markdown/foxglove.md)** - *Digitalis purpurea* 💀
    - 毒性: 心脏毒性 | 含洋地黄毒苷

26. **[铃兰 (Lily of the Valley)](markdown/lily_of_the_valley.md)** - *Convallaria majalis* 💀
    - 毒性: 心脏毒性 | 含铃兰毒苷

27. **[夹竹桃 (Oleander)](markdown/oleander.md)** - *Nerium oleander* 💀
    - 毒性: 极毒 | 含强心苷，全株有毒

28. **[毒芹 (Poison Hemlock)](markdown/poison_hemlock.md)** - *Conium maculatum* 💀
    - 毒性: 极毒 | 含毒芹碱，苏格拉底之死

29. **[芸香 (Rue)](markdown/rue.md)** - *Ruta graveolens* ⚠️
    - 毒性: 光毒性 | 可致严重皮肤灼伤

30. **[艾菊 (Tansy)](markdown/tansy.md)** - *Tanacetum vulgare* ⚠️
    - 毒性: 神经毒性 | 含侧柏酮

31. **[水毒芹 (Water Hemlock)](markdown/water_hemlock.md)** - *Cicuta maculata* 💀
    - 毒性: 极毒 | 北美最毒植物

32. **[苦艾 (Wormwood)](markdown/wormwood.md)** - *Artemisia absinthium* ⚠️
    - 毒性: 神经毒性 | 含侧柏酮，苦艾酒原料

### 毒性等级说明
- 💀 **极毒**: 可致命，严禁接触
- ⚠️ **有毒**: 可致严重中毒，需谨慎处理
- 📝 **限制使用**: 某些部位或剂量有毒

## 越南粉与东南亚常用香草

### 越南河粉 (Phở) 的香草传统

越南河粉是越南最具代表性的料理之一，其灵魂不仅在于清澈鲜美的汤头和滑嫩的米粉，更在于那一盘新鲜香草的完美搭配。传统的越南河粉香草拼盘通常包含以下几种:

**核心香草组合:**
- **薄荷 (Mint)** - 提供清凉感，平衡汤头的油腻
- **香菜 (Cilantro)** - 增添清香层次，是东南亚料理的经典
- **罗勒 (Thai Basil)** - 使用泰式罗勒，茴香味更浓，耐热性好
- **韭葱 (Chives)** - 提供温和的葱香

**其他常见配菜:**
- **豆芽菜** - 增加爽脆口感
- **柠檬片** - 提供酸味调节
- **小辣椒** - 调节辣度

### 东南亚料理香草文化

东南亚地区的热带气候孕育了丰富的香草文化，新鲜香草在当地料理中扮演着不可或缺的角色:

**泰式料理:**
- 泰式罗勒 (Thai Basil) - 耐热性强，适合热炒
- 柠檬香茅 - 汤品和咖喱的必备
- 泰式薄荷 - 沙拉和冷菜专用

**越南料理:**
- 新鲜香菜 - 几乎所有菜品的标配
- 薄荷叶 - 春卷和河粉的经典搭配
- 紫苏叶 - 烤肉和火锅的完美配菜

**印尼马来料理:**
- 椰浆香草组合 - 配合各种咖喱
- 香兰叶 - 甜点和饮品的天然香料

### 使用建议

1. **新鲜优先**: 东南亚料理强调香草的新鲜度，干燥香草无法替代
2. **现加现用**: 香草应在食用前最后加入，保持最佳香气
3. **平衡搭配**: 不同香草的组合能创造层次丰富的口感
4. **因地制宜**: 根据当地可获得的香草品种进行调整

## 使用说明

### 数据格式

每个植物条目包含以下标准字段:
- `id`: 唯一标识符
- `cn_name`: 中文名称
- `en_name`: 英文名称  
- `scientific_name`: 学名
- `category`: 分类 (edible/non_edible)
- `edible_parts`: 可食部位
- `toxicity`: 毒性信息
- `cultivation`: 栽培信息
- `chemistry`: 化学成分
- `culinary_uses`: 烹饪用途
- `storage`: 储存方法
- `references`: 参考文献

### 索引文件使用

`index.json`提供多种索引方式:

1. **alphabetical_index**: 按英文名A-Z排序
2. **chinese_index**: 按中文拼音排序
3. **category_index**: 按可食性分类
4. **usage_index**: 按用途分类 (烹饪/药用/芳香/观赏)
5. **difficulty_index**: 按栽培难度分类
6. **statistics**: 数据库统计信息

### 安全注意事项

⚠️ **重要提醒**:
- 有毒植物已明确标注，切勿误食
- 使用前请仔细阅读毒性信息
- 如有过敏史，请谨慎使用新香草
- 孕期哺乳期人群请咨询专业医师
- 儿童使用需成人监督

### 数据引用

如使用本项目数据，请注明来源:
```
数据来源: Herbiverse香草植物数据库
访问时间: [访问日期]
项目地址: /workspace/self-evolve-ai/projects/Herbiverse/
```

## 贡献指南

欢迎提交新的植物数据或改进现有信息:

1. **数据准确性**: 确保信息来源可靠
2. **格式规范**: 遵循项目JSON Schema
3. **安全第一**: 毒性信息必须准确标注
4. **多语言**: 提供中英文对照信息
5. **引用来源**: 标注权威参考文献

## 许可协议

本项目采用知识共享协议，供学习研究使用。商业用途请联系项目维护者。

---

*最后更新: 2025-01-14*
*项目状态: 活跃开发中*
*当前版本: v1.0*