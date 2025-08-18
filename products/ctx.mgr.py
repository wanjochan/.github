#!/usr/bin/env python3
"""
Manager plugin for Context MCP service
Provides management views for Cyber company using unified ctx_store
Implements memory lifecycle with gradual decay (like human forgetting)

=== PRODUCT REQUIREMENTS DOCUMENT (PRD) ===
Last Updated: 2025-08-17

CORE PRINCIPLE:
ctx.mgr is a lightweight management interface for the ctx_store (DuckDB) backend.
It provides multi-dimensional data organization WITHOUT duplicating storage.

ARCHITECTURE:
┌─────────────┐
│  ctx.mgr.py │ → Management Interface (THIS FILE)
└──────┬──────┘
       ↓ Direct calls
┌──────────────┐
│ ctx_store.py │ → Storage Layer (DuckDB singleton)
└──────┬───────┘
       ↓
┌──────────────┐
│ unified.db   │ → Single source of truth
└──────────────┘

DESIGN REQUIREMENTS:
1. SINGLE DATABASE: Only use ctx_store's unified.db (via DuckDB)
2. NO ADAPTERS: Remove CTXAdapter and all migration code
3. NO REDUNDANCY: Delete ctx_v2.db, context.db, manager.db, etc.
4. DIRECT ACCESS: Call ctx_store APIs directly, no intermediate layers
5. FILE SYSTEM: Use directories only for large files/backups, not primary storage

SIMPLIFICATION GOALS:
- Remove all "new system" checks and adapters
- Use ctx_store.get_store() directly for all operations
- Keep total storage < 5MB (excluding legitimate user data)
- Maintain clear separation: memory in DB, large files on disk

API CONTRACT:
- ctx('mgr', 'dimensions') → List dimensions
- ctx('mgr', 'add <dim> <content>') → Add to dimension  
- ctx('mgr', 'query <text>') → Search across dimensions
- ctx('mgr', 'cleanup') → Remove old/redundant data

FORBIDDEN:
- Creating new database files
- Implementing adapter layers
- Duplicating data between systems
- Over-engineering simple operations

=== END OF PRD ===
"""

import json
import yaml
from pathlib import Path
from datetime import datetime, timedelta
from typing import Dict, List, Any, Optional
import ctx_store as store
import shutil
import os

# PRD REQUIREMENT: NO ADAPTERS - Direct use of ctx_store only


class Plugin:
    """Cyber company management - answers queries about company operations"""
    
    @property
    def name(self) -> str:
        return "mgr"
    
    @property
    def description(self) -> str:
        return "Cyber company management views with memory lifecycle"
    
    def __init__(self):
        """Initialize manager using unified ctx_store"""
        # PRD REQUIREMENT: DIRECT ACCESS - Call ctx_store APIs directly
        self.store = store.get_store()
        
        # Model integration - graceful degradation if unavailable
        self.model_helper = None
        self.model_enabled = False
        try:
            from ctx_model_integration import ModelIntegration
            self.model_helper = ModelIntegration()
            self.model_enabled = self.model_helper.load_model()
            if self.model_enabled:
                print("[ctx.mgr] Model integration enabled (10% initial usage)")
        except ImportError:
            # Silent fallback - no model, no problem
            pass
        except Exception as e:
            # Log but continue without model
            print(f"[ctx.mgr] Model integration: {e}, continuing without")
            
        self.base_path = Path.home() / ".ai_context"
        self.archive_path = self.base_path / "archive"
        self.archive_path.mkdir(exist_ok=True)
        
        # Memory lifecycle thresholds
        self.lifecycle = {
            'hot_hours': 24,      # Full attention (100% recall)
            'warm_days': 7,       # Reduced attention (70% recall)
            'cold_days': 30,      # Low attention (30% recall)
            'archive_days': 90,   # Move to archive (needs explicit retrieval)
            'forget_days': 365    # Complete forgetting (delete from archive)
        }
        
        # Memory decay factors (simulate attention/recall decline)
        self.decay = {
            'hot': 1.0,      # 100% recall probability
            'warm': 0.7,     # 70% recall probability
            'cold': 0.3,     # 30% recall probability
            'archived': 0.0  # 0% (needs explicit retrieval)
        }
        
        # Track initial state
        self._track_current_state()
    
    def _track_current_state(self):
        """Track current company state using ctx_store"""
        # Track dimensions
        dimensions = self._count_dimensions()
        self.store.track_dimension("company_overview", {
            'dimensions': dimensions,
            'total_dimensions': len(dimensions),
            'timestamp': datetime.now().isoformat()
        })
        
        # Sync file index
        self.store.sync_file_index()
    
    async def execute(self, query: str) -> str:
        """Answer management queries in natural language"""
        original_query = query.strip()
        query = original_query.lower()
        self.last_query = original_query  # Save for format detection
        
        # === Quick commands ===
        if query in ['', 'help', '帮助', '?']:
            return self._help()
        elif query in ['all', '全部', 'summary', '总结']:
            return self._get_summary()
        
        # === Dimension management ===
        if "多少维度" in query or "哪些维度" in query or "dimensions" in query or "维度" in query:
            return self._get_dimensions()
        
        # === Task queue ===
        if "任务" in query or "task" in query or "todo" in query:
            return self._get_tasks()
        
        # === Memory overview ===
        if "全景" in query or "overview" in query or "记忆" in query:
            return self._get_memory_overview()
        
        # === Current focus ===
        if "焦点" in query or "focus" in query or "现在" in query:
            return self._get_current_focus()
        
        # === Active items ===
        if "活跃" in query or "active" in query or "进行中" in query:
            return self._get_active_items()
        
        # === Team status ===
        if "团队" in query or "分身" in query or "bot" in query or "team" in query or "智能体" in query or "agent" in query:
            return self._get_team_status()
        
        # === Decision history ===
        if "决策" in query or "decision" in query or "决定" in query:
            # Record major decision (HubX suggestion)
            if "record" in query or "记录" in query:
                return self._record_decision(original_query)
            return self._get_decisions()
        
        # === Issue tracking ===
        if "问题" in query or "issue" in query or "problem" in query:
            return self._get_issues()
        
        # === Statistics ===
        if "统计" in query or "stats" in query or "数量" in query:
            return self._get_statistics()
        
        # === CTX System Resources ===
        if "ctx" in query and ("资源" in query or "resource" in query or "storage" in query or "管理" in query):
            return self._get_ctx_resources()
        
        # === Timeline ===
        if "时间" in query or "timeline" in query or "历史" in query:
            return self._get_timeline()
        
        # === Products and Projects ===
        if "产品" in query or "product" in query:
            return self._get_products()
        elif "项目" in query or "project" in query:
            return self._get_projects()
        
        # === Auto Error Prevention (自动错误预防) ===
        if "prevent" in query or ("before" in query and "check" in query):
            return self._auto_error_prevention(query)
        
        # === Lessons and Rules (经验教训) ===
        if "lesson" in query or "教训" in query or "rule" in query:
            if "add" in query or "添加" in query or "记住" in query:
                return self._add_lesson(original_query)
            elif "list" in query or "列出" in query or "查看" in query:
                return self._list_lessons()
            elif "check" in query or "检查" in query:
                return self._check_against_rules(original_query)
        
        # === Specs (技术规范) ===
        if "spec" in query or "规范" in query or "specification" in query:
            if "add" in query or "添加" in query or "定义" in query:
                return self._add_spec(original_query)
            elif "list" in query or "查看" in query or "列出" in query:
                return self._list_specs()
            elif "get" in query or "获取" in query or "查询" in query:
                return self._get_spec(original_query)
        
        # === Dimension Relations (维度关联) ===  
        if "relation" in query or "关联" in query or "关系" in query or "连接" in query:
            return self._analyze_dimension_relations()
        
        # === Archive and Memory Management ===
        if "archive" in query or "归档" in query:
            if "status" in query:
                return self._archive_status()
            elif "retrieve" in query or "恢复" in query:
                return self._retrieve_from_archive(original_query)
            else:
                return self._archive_old(query)
        elif "forget" in query or "遗忘" in query:
            return self._forget_old(query)
        elif "decay" in query or "衰减" in query:
            return self._decay_memory()
        elif "consolidate" in query or "整合" in query:
            return self._consolidate_memory()
        elif "cleanup" in query or "清理" in query:
            return self._cleanup_memory()
        elif "lifecycle" in query or "生命周期" in query:
            return self._show_lifecycle()
        
        # Default: try to recall about the query
        # Use model enhancement if available
        enhanced_query = original_query
        if self.model_enabled and self.model_helper:
            try:
                enhanced_query = self.model_helper.enhance_query(original_query)
                if enhanced_query != original_query:
                    print(f"[ctx.mgr] Query enhanced by model: {original_query} -> {enhanced_query}")
            except Exception as e:
                # Silent degradation - continue with original query
                print(f"[ctx.mgr] Model enhancement failed: {e}, using original query")
                enhanced_query = original_query
        
        results = self.store.recall(enhanced_query, limit=5)
        
        # If model helped find results, increase its usage
        if results and self.model_enabled and enhanced_query != original_query:
            self.model_helper.increase_usage()
            
        if results:
            output = f"Found {len(results)} related items:\n\n"
            for item in results:
                output += f"• {item.get('what', 'Unknown')}\n"
                if item.get('details'):
                    output += f"  {str(item['details'])[:100]}...\n"
            return output
        
        return self._help()
    
    def _get_dimensions(self) -> str:
        """Get company dimensions using ctx_store"""
        stored_dimensions = self.store.get_all_dimensions()
        fs_dimensions = self._count_dimensions()
        
        # Dimension name mappings (English -> Chinese)
        dim_names = {
            'memory': '记忆',
            'lesson_learned': '经验教训',
            'technical_spec': '技术规范',
            'focus': '焦点',
            'project': '项目',
            'product': '产品',
            'rules': '规则',
            'specs': '规范',
            'working': '工作区',
            'test': '测试',
            'updates': '更新',
            'snapshots': '快照',
            'ctx_resources': 'CTX资源',
            'team': '团队',
            'agent': '智能体',
            'task': '任务',
            'decision': '决策',
            'issue': '问题',
            'archive': '归档',
            'longterm': '长期记忆'
        }
        
        # Merge dimensions
        all_dimensions = {}
        for dim, info in stored_dimensions.items():
            if dim.startswith("dimension_"):
                dim_name = dim.replace("dimension_", "")
                all_dimensions[dim_name] = info.get('latest_count', 0)
        
        for dim, count in fs_dimensions.items():
            if dim not in all_dimensions or all_dimensions[dim] < count:
                all_dimensions[dim] = count
        
        # Track state
        self.store.track_dimension("dimensions_snapshot", {
            'dimensions': all_dimensions,
            'total': len(all_dimensions),
            'timestamp': datetime.now().isoformat()
        })
        
        if not all_dimensions:
            return "No dimensions"
        
        # Sort by count
        sorted_dims = sorted(all_dimensions.items(), key=lambda x: x[1], reverse=True)
        
        # Check if detailed table format is requested
        query = self.last_query if hasattr(self, 'last_query') else ''
        if '表格' in query or 'table' in query or '详细' in query or 'detail' in query:
            # Table format output with perfect alignment
            # Each column width is determined by the border: 16, 14, 9 chars respectively
            
            # Design table with widths that match actual content
            output = "╭──────────────┬─────────────┬──────╮\n"
            output += "│ 维度 Dimension │ 中文名 Name │ 数量 # │\n" 
            output += "├──────────────┼─────────────┼──────┤\n"
            
            for dim, count in sorted_dims[:15]:
                chinese = dim_names.get(dim, dim)
                
                # Column widths: 14, 13, 8 chars (from border)
                
                # Column 1: 14 chars
                col1 = f" {dim[:12]:<12} "
                
                # Column 2: 13 chars (handle Chinese display width)
                chinese_display_width = sum(2 if ord(c) > 127 else 1 for c in chinese)
                spaces_needed = 11 - chinese_display_width  # 11 display positions + 2 spaces = 13
                col2 = f" {chinese}{' ' * max(0, spaces_needed)} "
                
                # Column 3: 8 chars
                col3 = f" {count:>4} "
                
                # Ensure exact widths match the border
                col1 = col1[:14].ljust(14)
                col2 = col2[:13].ljust(13)
                col3 = col3[:8].ljust(8)
                
                output += f"│{col1}│{col2}│{col3}│\n"
            
            output += "╰──────────────┴─────────────┴──────╯\n"
            if len(sorted_dims) > 15:
                output += f"... 还有 {len(sorted_dims)-15} 个维度"
        else:
            # Compact format with Chinese names
            output = "Dimensions: "
            dim_strs = []
            for dim, count in sorted_dims[:10]:
                chinese = dim_names.get(dim, dim)
                dim_strs.append(f"{dim}({chinese}):{count}")
            output += " | ".join(dim_strs)
            if len(sorted_dims) > 10:
                output += f" +{len(sorted_dims)-10} more"
        
        return output
    
    def _get_tasks(self) -> str:
        """Get task queue status using ctx_store"""
        # Query tasks from store
        todos = self.store.recall("todo", limit=20)
        tasks = self.store.recall("task", limit=20)
        
        active = []
        pending = []
        completed = []
        
        for item in todos + tasks:
            content = item.get('what', '')
            details = item.get('details', {})
            
            # Check status
            if isinstance(details, dict):
                status = details.get('status', '')
                if status == 'completed' or 'completed' in content.lower():
                    completed.append(content)
                elif status == 'active' or 'active' in content.lower():
                    active.append(content)
                else:
                    pending.append(content)
            elif 'completed' in content.lower() or 'done' in content.lower():
                completed.append(content)
            elif 'active' in content.lower() or 'doing' in content.lower():
                active.append(content)
            else:
                pending.append(content)
        
        output = "任务队列状态:\n\n"
        
        if active:
            output += f"进行中 ({len(active)}):\n"
            for task in active[:5]:
                output += f"  → {task}\n"
        
        if pending:
            output += f"\n待处理 ({len(pending)}):\n"
            for task in pending[:5]:
                output += f"  ○ {task}\n"
        
        if completed:
            output += f"\n已完成 ({len(completed)}):\n"
            for task in completed[:3]:
                output += f"  ✓ {task}\n"
        
        if not (active or pending or completed):
            output = "暂无任务记录\n"
        
        return output
    
    def _track_task_start(self, task_name: str) -> str:
        """Simple task tracking - start a task (HubX practical suggestion)"""
        if not task_name:
            return "请指定任务名称"
        
        # Save task start event using adapter
        metadata = {
            'status': 'in_progress',
            'started_at': datetime.now().isoformat(),
            'type': 'task_tracking'
        }
        
        ctx_id = self.store.save('task', f"Task Started: {task_name}", metadata)
        
        return f"✓ Task started: {task_name}"
    
    def _track_task_done(self, task_name: str) -> str:
        """Simple task tracking - complete a task (HubX practical suggestion)"""
        if not task_name:
            return "请指定任务名称"
        
        # Save task completion event using adapter
        metadata = {
            'status': 'completed',
            'completed_at': datetime.now().isoformat(),
            'type': 'task_tracking'
        }
        
        ctx_id = self.store.save('task', f"Task Completed: {task_name}", metadata)
        
        return f"✓ Task completed: {task_name}"
    
    def _get_memory_overview(self) -> str:
        """Get memory overview using ctx_store"""
        summary = self.store.get_summary()
        
        output = "记忆全景图:\n\n"
        output += f"总记忆数: {summary['total_contexts']}\n\n"
        
        output += "分类分布:\n"
        for type_name, count in summary['by_type'].items():
            output += f"  • {type_name}: {count}\n"
        
        output += "\n最近记忆:\n"
        for ctx in summary['recent']:
            what = ctx.get('what', 'Unknown')
            output += f"  - {what[:50]}...\n"
        
        # Add file statistics
        file_stats = summary.get('file_stats', {})
        if file_stats:
            output += f"\n存储统计:\n"
            total_files = sum(s['files'] for s in file_stats.values())
            total_size = sum(s['size_kb'] for s in file_stats.values())
            output += f"  文件数: {total_files}\n"
            output += f"  总大小: {total_size:.1f} KB\n"
        
        # Add dimension statistics
        dimensions = summary.get('dimensions', {})
        if dimensions:
            output += f"\n维度追踪: {len(dimensions)} 个维度\n"
        
        return output
    
    def _get_current_focus(self) -> str:
        """Get current focus using ctx_store"""
        # Query focus-related items
        focus_items = self.store.recall("focus", limit=5)
        current_items = self.store.recall("current", limit=5)
        now_items = self.store.recall("now", limit=5)
        
        all_focus = focus_items + current_items + now_items
        
        if not all_focus:
            return "当前无明确焦点\n"
        
        # Sort by time
        all_focus.sort(key=lambda x: x.get('when', ''), reverse=True)
        
        output = "当前焦点:\n\n"
        latest = all_focus[0]
        output += f"主要: {latest.get('what', 'Unknown Focus')}\n"
        output += f"时间: {latest.get('when', 'unknown')}\n"
        
        if latest.get('details'):
            output += f"详情: {latest['details']}\n"
        
        if len(all_focus) > 1:
            output += "\n相关焦点:\n"
            for item in all_focus[1:4]:
                output += f"  • {item.get('what', 'Unknown')}\n"
        
        return output
    
    def _get_active_items(self) -> str:
        """Get active items using ctx_store"""
        active_keywords = ["active", "进行中", "doing", "working", "current"]
        
        active_items = []
        for keyword in active_keywords:
            items = self.store.recall(keyword, limit=10)
            active_items.extend(items)
        
        # Deduplicate
        seen = set()
        unique_items = []
        for item in active_items:
            key = item.get('what', '')
            if key and key not in seen:
                seen.add(key)
                unique_items.append(item)
        
        output = f"活跃项目 ({len(unique_items)}):\n\n"
        for item in unique_items[:10]:
            output += f"• {item.get('what', 'Unknown')}\n"
            if item.get('details'):
                output += f"  {str(item['details'])[:50]}...\n"
        
        if not unique_items:
            output = "暂无活跃项目\n"
        
        return output
    
    def _get_team_status(self) -> str:
        """Get team/agent status using ctx_store"""
        team_members = ["bot0", "bot1", "hub", "cursor", "gem"]
        
        team_status = {}
        for member in team_members:
            statuses = self.store.recall(member, limit=1)
            if statuses:
                latest = statuses[0]
                team_status[member] = {
                    'status': latest.get('what', 'unknown'),
                    'when': latest.get('when', 'unknown')
                }
        
        output = "团队状态:\n\n"
        for member, info in team_status.items():
            output += f"{member}:\n"
            output += f"  状态: {info['status']}\n"
            output += f"  更新: {info['when']}\n"
        
        if not team_status:
            output += "暂无团队状态记录\n"
        
        return output
    
    def _record_decision(self, query: str) -> str:
        """Record a major decision with reasoning (HubX practical suggestion)"""
        # Extract decision from query
        decision_text = query
        for prefix in ['decision record', 'record decision', '记录决策', '决策记录']:
            if prefix in query.lower():
                decision_text = query.lower().replace(prefix, '').strip()
                break
        
        if not decision_text:
            return "请指定决策内容，格式: decision record <决策> [理由]"
        
        # Parse decision and reasoning
        parts = decision_text.split(' because ', 1)
        if len(parts) == 1:
            parts = decision_text.split(' 因为 ', 1)
        
        decision = parts[0].strip()
        reasoning = parts[1].strip() if len(parts) > 1 else ''
        
        # Save decision using adapter
        metadata = {
            'type': 'major_decision',
            'reasoning': reasoning,
            'recorded_at': datetime.now().isoformat(),
            'importance': 0.9,
            'decay_exempt': True  # Important decisions should not decay
        }
        
        ctx_id = self.store.save('decision', f"Decision: {decision}", metadata)
        
        output = f"✓ Decision recorded: {decision}"
        if reasoning:
            output += f"\n  Reasoning: {reasoning}"
        
        return output
    
    def _get_decisions(self) -> str:
        """Get decision history using ctx_store"""
        decisions = []
        decision_keywords = ["决策", "decision", "决定", "choice", "选择"]
        
        for keyword in decision_keywords:
            items = self.store.recall(keyword, limit=10)
            decisions.extend(items)
        
        # Deduplicate
        seen = set()
        unique_decisions = []
        for dec in decisions:
            key = dec.get('what', '')
            if key and key not in seen:
                seen.add(key)
                unique_decisions.append(dec)
        
        output = f"决策历史 ({len(unique_decisions)}):\n\n"
        
        for dec in unique_decisions[:10]:
            output += f"• {dec['what']}\n"
            if dec.get('details'):
                output += f"  理由: {dec['details']}\n"
        
        if not unique_decisions:
            output = "暂无决策记录\n"
        
        return output
    
    def _get_issues(self) -> str:
        """Get issue tracking using ctx_store"""
        issue_keywords = ["issue", "problem", "问题", "bug", "error"]
        
        issues = []
        for keyword in issue_keywords:
            items = self.store.recall(keyword, limit=10)
            issues.extend(items)
        
        # Categorize issues
        open_issues = []
        closed_issues = []
        
        for issue in issues:
            content = issue.get('what', '').lower()
            if any(word in content for word in ['fixed', 'resolved', '解决', 'closed']):
                closed_issues.append(issue)
            else:
                open_issues.append(issue)
        
        output = f"问题追踪 ({len(issues)}):\n\n"
        
        if open_issues:
            output += f"待解决 ({len(open_issues)}):\n"
            for issue in open_issues[:5]:
                output += f"  ⚠ {issue['what']}\n"
        
        if closed_issues:
            output += f"\n已解决 ({len(closed_issues)}):\n"
            for issue in closed_issues[:3]:
                output += f"  ✓ {issue['what']}\n"
        
        if not issues:
            output = "暂无问题记录\n"
        
        return output
    
    def _get_statistics(self) -> str:
        """Get statistics using ctx_store"""
        summary = self.store.get_summary()
        
        output = "系统统计:\n\n"
        output += f"总记录数: {summary['total_contexts']}\n"
        
        # Type distribution
        output += "\n类型分布:\n"
        total = summary['total_contexts']
        for type_name, count in summary['by_type'].items():
            percentage = (count / total * 100) if total > 0 else 0
            output += f"  {type_name}: {count} ({percentage:.1f}%)\n"
        
        # File statistics
        file_stats = summary.get('file_stats', {})
        if file_stats:
            output += "\n文件统计:\n"
            for layer, stats in file_stats.items():
                output += f"  {layer}: {stats['files']} files, {stats['size_kb']:.1f} KB\n"
        
        # Dimension statistics
        dimensions = summary.get('dimensions', {})
        if dimensions:
            output += f"\n维度统计: {len(dimensions)} 个活跃维度\n"
        
        return output
    
    def _get_timeline(self) -> str:
        """Get timeline view using ctx_store"""
        summary = self.store.get_summary()
        recent = summary.get('recent', [])
        
        output = "时间线:\n\n"
        
        for ctx in recent[:20]:
            time = ctx.get('when', 'unknown')
            what = ctx.get('what', '')
            
            # Simplify time display
            if 'T' in time:
                time = time.split('T')[1][:5]  # Show HH:MM only
            
            output += f"[{time}] {what[:60]}\n"
        
        if not recent:
            output = "暂无时间线记录\n"
        
        return output
    
    def _get_products(self) -> str:
        """Get product information"""
        products = self.store.recall("product", limit=20)
        
        output = "产品状态:\n\n"
        for product in products:
            output += f"• {product.get('what', 'Unknown Product')}\n"
            if product.get('details'):
                details = product['details']
                if isinstance(details, dict):
                    if 'version' in details:
                        output += f"  版本: {details['version']}\n"
                    if 'status' in details:
                        output += f"  状态: {details['status']}\n"
        
        if not products:
            # Try to load from file
            status_file = self.base_path / "project" / "current_status.yml"
            if status_file.exists():
                with open(status_file, 'r') as f:
                    data = yaml.safe_load(f)
                    if 'products' in data:
                        output = "产品状态 (from file):\n\n"
                        for product in data['products']:
                            output += f"• {product.get('name', 'Unknown')}\n"
                            output += f"  状态: {product.get('status', 'Unknown')}\n"
                            output += f"  版本: {product.get('version', 'Unknown')}\n"
        
        return output
    
    def _get_projects(self) -> str:
        """Get project information"""
        projects = self.store.recall("project", limit=20)
        
        output = "项目状态:\n\n"
        for project in projects:
            output += f"• {project.get('what', 'Unknown Project')}\n"
            if project.get('details'):
                details = project['details']
                if isinstance(details, dict):
                    if 'status' in details:
                        output += f"  状态: {details['status']}\n"
                    if 'description' in details:
                        output += f"  描述: {details['description']}\n"
        
        if not projects:
            # Try to load from file
            status_file = self.base_path / "project" / "current_status.yml"
            if status_file.exists():
                with open(status_file, 'r') as f:
                    data = yaml.safe_load(f)
                    if 'projects' in data:
                        output = "项目状态 (from file):\n\n"
                        for project in data['projects']:
                            output += f"• {project.get('name', 'Unknown')}\n"
                            output += f"  状态: {project.get('status', 'Unknown')}\n"
                            output += f"  描述: {project.get('description', 'Unknown')}\n"
        
        return output
    
    def _get_summary(self) -> str:
        """Get comprehensive company summary using ctx_store"""
        summary = self.store.get_summary()
        
        # Compact professional format
        output = "Corp Status | "
        output += f"Contexts:{summary['total_contexts']} "
        
        # Dimensions
        dimensions = summary.get('dimensions', {})
        output += f"Dims:{len(dimensions)} "
        
        # Tasks
        tasks = self.store.recall("task", limit=20)
        active_tasks = [t for t in tasks if 'active' in str(t).lower()]
        if active_tasks:
            output += f"Tasks:{len(active_tasks)} "
        
        # Agents
        team_members = ["bot0", "bot1", "hub"]
        active_members = []
        for member in team_members:
            if self.store.recall(member, limit=1):
                active_members.append(member)
        if active_members:
            output += f"Agents:[{','.join(active_members)}] "
        
        # Storage
        file_stats = summary.get('file_stats', {})
        if file_stats:
            total_size = sum(s['size_kb'] for s in file_stats.values())
            output += f"Storage:{total_size:.0f}KB"
        
        return output
    
    def _get_ctx_resources(self) -> str:
        """Get CTX system resource usage as a dimension"""
        import os
        
        base_path = self.base_path
        total_size = 0
        file_counts = {}
        file_sizes = {}
        
        # Scan all CTX directories
        directories = {
            'contexts': base_path,
            'rules': base_path / 'rules',
            'specs': base_path / 'specs',
            'archive': base_path / 'archive',
            'focus': base_path / 'focus',
            'working': base_path / 'working',
            'longterm': base_path / 'longterm'
        }
        
        for dir_name, dir_path in directories.items():
            if dir_path.exists():
                files = list(dir_path.glob('**/*'))
                files = [f for f in files if f.is_file()]
                file_counts[dir_name] = len(files)
                size = sum(f.stat().st_size for f in files if f.is_file())
                file_sizes[dir_name] = size
                total_size += size
        
        # Get DuckDB file size
        db_file = base_path / 'unified.db'
        if db_file.exists():
            db_size = db_file.stat().st_size
            file_sizes['database'] = db_size
            total_size += db_size
            file_counts['database'] = 1
        
        # Track as dimension
        self.store.track_dimension('ctx_resources', {
            'total_files': sum(file_counts.values()),
            'total_size_kb': total_size / 1024,
            'by_directory': file_counts,
            'sizes_kb': {k: v/1024 for k, v in file_sizes.items()},
            'timestamp': datetime.now().isoformat()
        })
        
        # Compact output
        output = f"CTX Resources: "
        output += f"Files:{sum(file_counts.values())} "
        output += f"Size:{total_size/1024:.1f}KB | "
        
        # Show top directories by file count
        sorted_dirs = sorted(file_counts.items(), key=lambda x: x[1], reverse=True)
        for dir_name, count in sorted_dirs[:3]:
            if count > 0:
                output += f"{dir_name}:{count} "
        
        # Add database size if significant
        if 'database' in file_sizes and file_sizes['database'] > 1024:
            output += f"DB:{file_sizes['database']/1024:.1f}KB"
        
        return output.strip()
    
    def _count_dimensions(self) -> Dict[str, int]:
        """Count dimensions from filesystem"""
        dimensions = {}
        for path in self.base_path.iterdir():
            if path.is_dir() and not path.name.startswith('.'):
                count = len(list(path.glob('*')))
                if count > 0:
                    dimensions[path.name] = count
        return dimensions
    
    # === Dimension Relations Analysis ===
    
    def _analyze_dimension_relations(self) -> str:
        """Analyze relationships between dimensions"""
        # Try to get relations from store
        try:
            # Check if store has relation methods
            if hasattr(self.store, 'get_relations'):
                db_relations = self.store.get_relations()
                dimensions = self.store.get_dimensions()
                
                if db_relations:
                    output = "🔗 维度关系图谱 (from database):\n\n"
                    for rel in db_relations[:10]:
                        output += f"{rel['source']} → {rel['target']}\n"
                        output += f"  类型: {rel['type']}, 强度: {rel['strength']:.1f}\n\n"
                    return output
        except Exception as e:
            # Fall back to filesystem analysis
            pass
        
        # Fall back to old implementation
        relations = []
        dimensions = self._count_dimensions()
        
        # Define known relationship patterns
        relation_patterns = [
            ('focus', 'project', '焦点→项目', '从关注点转化为具体项目'),
            ('project', 'product', '项目→产品', '项目成果转化为产品'),
            ('lesson_learned', 'rules', '教训→规则', '经验总结成规则'),
            ('rules', 'technical_spec', '规则→规范', '规则提炼为技术规范'),
            ('memory', 'snapshots', '记忆→快照', '重要记忆存为快照'),
            ('working', 'test', '工作→测试', '工作成果进入测试'),
            ('test', 'updates', '测试→更新', '测试反馈驱动更新')
        ]
        
        output = "🔗 维度关系图谱:\n\n"
        
        for source, target, label, desc in relation_patterns:
            if source in dimensions and target in dimensions:
                s_count = dimensions[source]
                t_count = dimensions[target]
                relations.append({
                    'source': source,
                    'target': target,
                    'label': label,
                    'strength': min(s_count, t_count)
                })
                output += f"{label}:\n"
                output += f"  {source}({s_count}) → {target}({t_count})\n"
                output += f"  {desc}\n\n"
        
        # Identify isolated dimensions
        connected = set()
        for r in relations:
            connected.add(r['source'])
            connected.add(r['target'])
        
        isolated = set(dimensions.keys()) - connected
        if isolated:
            output += "\n📍 独立维度（需要建立关联）:\n"
            for dim in isolated:
                output += f"  • {dim}: {dimensions[dim]} items\n"
        
        return output
    
    # === Auto Error Prevention System ===
    
    def _auto_error_prevention(self, query: str) -> str:
        """Auto check for error prevention before operations"""
        # Extract operation keywords from query
        operation_keywords = self._extract_operation_keywords(query)
        
        # Query related lessons and rules
        prevention_items = []
        
        # Search in lesson_learned directory specifically
        lesson_dir = self.base_path / "lesson_learned"
        if lesson_dir.exists():
            for keyword in operation_keywords:
                # Read from lesson files directly
                for lesson_file in lesson_dir.glob("*"):
                    try:
                        content = lesson_file.read_text(encoding='utf-8')
                        if keyword.lower() in content.lower():
                            prevention_items.append({
                                'what': lesson_file.stem.replace('_', ' '),
                                'details': content[:200],
                                'source': 'lesson_learned'
                            })
                    except:
                        continue
        
        # Also check general memory
        for keyword in operation_keywords:
            rule_items = self.store.recall(keyword, limit=3)
            prevention_items.extend(rule_items)
        
        if not prevention_items:
            return f"🛡️ 无相关错误预防规则: {', '.join(operation_keywords)}"
        
        output = "🛡️ 错误预防检查:\n\n"
        seen_rules = set()
        
        for item in prevention_items:
            rule_text = item.get('what', '')
            if rule_text and rule_text not in seen_rules:
                seen_rules.add(rule_text)
                output += f"⚠️  {rule_text}\n"
                if item.get('details'):
                    details = str(item['details'])[:100]
                    output += f"   详情: {details}{'...' if len(str(item['details'])) > 100 else ''}\n"
                output += "\n"
        
        output += f"总计 {len(seen_rules)} 条相关预防规则"
        return output
    
    def _extract_operation_keywords(self, query: str) -> list:
        """Extract operation keywords for error prevention"""
        # Common operation patterns that might have associated lessons
        operation_patterns = {
            'table': ['表格', 'table', '对齐', 'align', '边框', 'border'],
            'tmux': ['tmux', '窗口', 'window', '会话', 'session'],
            'chinese': ['中文', 'chinese', '字符', 'char', '宽度', 'width'],
            'mcp': ['mcp', '工具', 'tool', '调用', 'call'],
            'commit': ['commit', '提交', 'git', '版本'],
            'file': ['文件', 'file', '读取', 'read', '写入', 'write'],
            'network': ['网络', 'network', 'api', 'http', '连接'],
            'error': ['错误', 'error', 'bug', '失败', 'fail'],
            'display': ['显示', 'display', '渲染', 'render', '格式'],
            'encoding': ['编码', 'encoding', 'utf8', 'ascii']
        }
        
        keywords = []
        query_lower = query.lower()
        
        for category, patterns in operation_patterns.items():
            if any(pattern in query_lower for pattern in patterns):
                keywords.append(category)
                keywords.extend(patterns[:2])  # Add first 2 patterns
        
        # Also extract direct keywords from query (longer words)
        words = query_lower.split()
        keywords.extend([w for w in words if len(w) > 3 and w not in ['prevent', 'before', 'check', 'error']])
        
        return list(set(keywords))[:10]  # Limit to 10 keywords
    
    # === Lessons and Rules Management ===
    
    def _add_lesson(self, query: str) -> str:
        """Add a lesson learned or rule to remember"""
        # Extract the lesson from query
        lesson_text = query
        for prefix in ['lesson add', 'rule add', '添加教训', '记住']:
            if prefix in query.lower():
                lesson_text = query.lower().replace(prefix, '').strip()
                break
        
        if not lesson_text:
            return "Please specify the lesson or rule to remember"
        
        # Use adapter for saving
        metadata = {
            'type': 'lesson',
            'category': 'general',  # Will be updated below
            'importance': 0.9,
            'decay_exempt': True,
            'created_at': datetime.now().isoformat()
        }
        
        # Categorize the lesson
        category = 'general'
        if 'document' in lesson_text or 'file' in lesson_text or '文档' in lesson_text or '.md' in lesson_text:
            category = 'documentation'
        elif 'code' in lesson_text or '代码' in lesson_text:
            category = 'coding'
        elif 'commit' in lesson_text or 'git' in lesson_text:
            category = 'git'
        elif 'test' in lesson_text or '测试' in lesson_text:
            category = 'testing'
        
        # Store as high-importance rule that won't decay
        metadata = {
            'category': category,
            'learned_at': datetime.now().isoformat(),
            'importance': 'critical',
            'decay_exempt': True  # This won't decay
        }
        
        ctx_id = self.store.save(
            'lesson_learned',
            f"RULE: {lesson_text}",
            metadata
        )
        
        # Mark as very important (only if using old system)
        if hasattr(self.store, 'mark_important'):
            self.store.mark_important(ctx_id, 1.0)
        
        # Also save to rules file for persistence
        rules_file = self.base_path / 'rules' / f'{category}_rules.yml'
        rules_file.parent.mkdir(exist_ok=True)
        
        rules = []
        if rules_file.exists():
            with open(rules_file, 'r') as f:
                data = yaml.safe_load(f)
                if data and 'rules' in data:
                    rules = data['rules']
        
        rules.append({
            'text': lesson_text,
            'added': datetime.now().isoformat(),
            'id': ctx_id
        })
        
        with open(rules_file, 'w') as f:
            yaml.dump({'category': category, 'rules': rules}, f, allow_unicode=True)
        
        return f"✓ Rule added to '{category}': {lesson_text[:50]}..."
    
    def _list_lessons(self) -> str:
        """List all remembered lessons and rules"""
        # Get from memory
        lessons = self.store.recall('RULE:', limit=50)
        
        # Also check rules directory
        rules_dir = self.base_path / 'rules'
        if not rules_dir.exists():
            rules_dir.mkdir(exist_ok=True)
        
        output = "Remembered Rules:\n"
        
        # Group by category
        by_category = {}
        for lesson in lessons:
            details = lesson.get('details', {})
            category = details.get('category', 'general')
            if category not in by_category:
                by_category[category] = []
            by_category[category].append(lesson['what'].replace('RULE: ', ''))
        
        # Also load from files
        for rules_file in rules_dir.glob('*_rules.yml'):
            with open(rules_file, 'r') as f:
                data = yaml.safe_load(f)
                if data:
                    category = data.get('category', 'unknown')
                    if category not in by_category:
                        by_category[category] = []
                    for rule in data.get('rules', []):
                        text = rule.get('text', '')
                        if text and text not in by_category[category]:
                            by_category[category].append(text)
        
        # Compact display
        for category, rules in by_category.items():
            output += f"[{category}] "
            output += f"{len(rules)} rules | "
        
        return output.rstrip(' | ')
    
    def _check_against_rules(self, query: str) -> str:
        """Check an action against remembered rules"""
        # Extract what to check
        check_text = query.lower()
        for prefix in ['check', 'rule check', '检查']:
            check_text = check_text.replace(prefix, '').strip()
        
        if not check_text:
            return "Specify what to check"
        
        # Load rules from files (more reliable than database)
        all_rules = []
        rules_dir = self.base_path / 'rules'
        if rules_dir.exists():
            for rules_file in rules_dir.glob('*_rules.yml'):
                with open(rules_file, 'r') as f:
                    data = yaml.safe_load(f)
                    if data and 'rules' in data:
                        for rule in data['rules']:
                            all_rules.append({
                                'category': data['category'],
                                'text': rule['text']
                            })
        
        violations = []
        warnings = []
        
        for rule in all_rules:
            rule_text = rule['text'].lower()
            category = rule['category']
            
            # Check for violations - improved logic
            # Documentation rules
            if category == 'documentation' or any(kw in rule_text for kw in ['document', 'readme', '.md', '.txt', 'report']):
                if any(kw in check_text for kw in ['readme', '.md', '.txt', 'document', 'report', '文档']):
                    violations.append(f"[{category}] {rule['text'][:60]}...")
            
            # Flattery rules
            elif any(kw in rule_text for kw in ['谄媚', 'flattery', '说得对']):
                if any(kw in check_text for kw in ['你说得对', '您说得对', '说得对', 'you are right', "you're right"]):
                    violations.append(f"[{category}] {rule['text'][:60]}...")
            
            # Git rules
            elif category == 'git' and 'commit' in rule_text:
                if 'commit' in check_text or '提交' in check_text:
                    warnings.append(f"[{category}] {rule['text'][:50]}...")
        
        if violations:
            return f"⚠ Violations: {'; '.join(violations[:3])}"
        elif warnings:
            return f"⚡ Potential issues: {'; '.join(warnings[:2])}"
        return "✓ No violations"
    
    # === Technical Specs Management ===
    
    def _add_spec(self, query: str) -> str:
        """Add a technical specification or standard"""
        # Keep original case for content
        original_query = query
        
        # Extract spec from query - use original case
        spec_text = original_query
        for prefix in ['spec add', 'specification add', '添加规范', '定义规范']:
            if prefix in query.lower():
                # Find the prefix position and remove it
                idx = query.lower().find(prefix)
                if idx >= 0:
                    spec_text = original_query[idx + len(prefix):].strip()
                    break
        
        if not spec_text:
            return "Please specify the spec name and definition"
        
        # Parse spec name and content
        parts = spec_text.split(' ', 1)
        if len(parts) < 2:
            return "Format: spec add <name> <definition>"
        
        spec_name = parts[0]
        spec_definition = parts[1]
        
        # Categorize the spec
        category = 'general'
        if 'api' in spec_name or 'interface' in spec_name:
            category = 'api'
        elif 'protocol' in spec_name or 'format' in spec_name:
            category = 'protocol'
        elif 'design' in spec_name or 'architecture' in spec_name:
            category = 'design'
        elif 'standard' in spec_name or 'convention' in spec_name:
            category = 'standard'
        
        # Use adapter to save spec
        metadata = {
            'name': spec_name,
            'definition': spec_definition,
            'category': category,
            'type': 'specification',
            'decay_exempt': True,
            'created': datetime.now().isoformat()
        }
        
        ctx_id = self.store.save('technical_spec', f"SPEC:{spec_name} - {spec_definition}", metadata)
        
        # Also save to specs directory for persistence
        specs_file = self.base_path / 'specs' / f'{category}_specs.yml'
        specs_file.parent.mkdir(exist_ok=True)
        
        specs = []
        if specs_file.exists():
            with open(specs_file, 'r') as f:
                data = yaml.safe_load(f)
                if data and 'specs' in data:
                    specs = data['specs']
        
        specs.append({
            'name': spec_name,
            'definition': spec_definition,
            'created': datetime.now().isoformat()
        })
        
        with open(specs_file, 'w') as f:
            yaml.dump({'category': category, 'specs': specs}, f, allow_unicode=True)
        
        return f"✓ Spec added [{category}]: {spec_name}"
    
    def _list_specs(self) -> str:
        """List all technical specifications"""
        # Get specs from memory
        specs = self.store.recall('SPEC:', limit=50)
        
        # Also check specs directory
        specs_dir = self.base_path / 'specs'
        if not specs_dir.exists():
            specs_dir.mkdir(exist_ok=True)
        
        # Group by category
        by_category = {}
        
        for spec in specs:
            details = spec.get('details', {})
            category = details.get('category', 'general')
            name = details.get('name', spec['what'].replace('SPEC:', ''))
            if category not in by_category:
                by_category[category] = []
            by_category[category].append(name)
        
        # Also check files
        for specs_file in specs_dir.glob('*_specs.yml'):
            with open(specs_file, 'r') as f:
                data = yaml.safe_load(f)
                if data:
                    category = data.get('category', 'general')
                    if category not in by_category:
                        by_category[category] = []
                    for spec in data.get('specs', []):
                        name = spec.get('name', '')
                        if name and name not in by_category[category]:
                            by_category[category].append(name)
        
        if not by_category:
            return "No specs defined"
        
        # Compact output
        output = "Specs: "
        for category, names in by_category.items():
            output += f"[{category}] {len(names)} | "
        
        return output.rstrip(' | ')
    
    def _get_spec(self, query: str) -> str:
        """Get a specific technical specification"""
        # Extract spec name from query
        spec_name = query.lower()
        for prefix in ['spec get', 'get spec', '获取规范', '查询规范']:
            spec_name = spec_name.replace(prefix, '').strip()
        
        if not spec_name:
            return "Specify which spec to get"
        
        # Search for the spec
        specs = self.store.recall(f'SPEC:{spec_name}', limit=5)
        
        if not specs:
            # Try partial match
            specs = self.store.recall(f'SPEC:', limit=100)
            matching = []
            for spec in specs:
                details = spec.get('details', {})
                name = details.get('name', '')
                if spec_name in name.lower():
                    matching.append(spec)
            specs = matching[:5]
        
        if not specs:
            return f"Spec '{spec_name}' not found"
        
        # Return the spec definition
        output = ""
        for spec in specs:
            details = spec.get('details', {})
            name = details.get('name', 'Unknown')
            definition = details.get('definition', 'No definition')
            category = details.get('category', 'general')
            output += f"[{category}] {name}:\n{definition}\n\n"
        
        return output.strip()
    
    # === Memory Lifecycle Management ===
    
    def _archive_status(self) -> str:
        """Get archive and memory lifecycle status with decay information"""
        summary = self.store.get_summary()
        now = datetime.now()
        hot = warm = cold = to_archive = 0
        
        # Count contexts by age and decay state
        contexts = self.store.query("""
            SELECT id, created_at, metadata FROM contexts
        """)
        
        for ctx_id, created, metadata in contexts:
            age = now - created
            # Check decay level from metadata
            meta = json.loads(metadata) if metadata else {}
            decay = meta.get('decay', 1.0)
            
            if age < timedelta(hours=self.lifecycle['hot_hours']):
                hot += 1
            elif age < timedelta(days=self.lifecycle['warm_days']):
                warm += 1
            elif age < timedelta(days=self.lifecycle['cold_days']):
                cold += 1
            elif age < timedelta(days=self.lifecycle['archive_days']):
                to_archive += 1
        
        archived_files = len(list(self.archive_path.glob("**/*.yml")))
        
        # Show decay levels
        return f"Memory: Hot({int(self.decay['hot']*100)}%):{hot} Warm({int(self.decay['warm']*100)}%):{warm} Cold({int(self.decay['cold']*100)}%):{cold} ToArchive:{to_archive} Archived:{archived_files}"
    
    def _decay_memory(self) -> str:
        """Apply decay to memories based on age (simulate forgetting)"""
        now = datetime.now()
        decayed = 0
        
        # Get all contexts with their age
        contexts = self.store.query("""
            SELECT id, created_at, metadata FROM contexts
        """)
        
        for ctx_id, created, metadata in contexts:
            age = now - created
            meta = json.loads(metadata) if metadata else {}
            current_decay = meta.get('decay', 1.0)
            new_decay = current_decay
            
            # Check if decay exempt (rules/lessons)
            if meta.get('decay_exempt', False):
                new_decay = 1.0  # Always 100% recall for rules
            else:
                # Apply decay based on age
                if age > timedelta(days=self.lifecycle['cold_days']):
                    new_decay = self.decay['cold']
                elif age > timedelta(days=self.lifecycle['warm_days']):
                    new_decay = self.decay['warm']
                elif age > timedelta(hours=self.lifecycle['hot_hours']):
                    new_decay = self.decay['warm']
                else:
                    new_decay = self.decay['hot']
            
            # Update decay if changed
            if new_decay != current_decay:
                meta['decay'] = new_decay
                meta['decay_updated'] = now.isoformat()
                # Update metadata with decay factor
                self.store._execute_safe("""
                    UPDATE contexts 
                    SET metadata = ?
                    WHERE id = ?
                """, (json.dumps(meta), ctx_id))
                decayed += 1
        
        return f"Decayed:{decayed}"
    
    def _consolidate_memory(self) -> str:
        """Consolidate similar low-attention memories"""
        consolidated = 0
        
        # Find cold memories of same type to consolidate
        cold_cutoff = datetime.now() - timedelta(days=self.lifecycle['cold_days'])
        
        types = self.store.query("SELECT DISTINCT type FROM contexts")
        for ctx_type, in types:
            cold_contexts = self.store.query("""
                SELECT id, content FROM contexts 
                WHERE type = ? AND created_at < ?
                ORDER BY created_at
            """, (ctx_type, cold_cutoff))
            
            if len(cold_contexts) > 10:  # Consolidate if many cold items
                summary_items = []
                ids_to_decay = []  # Don't remove, just mark as consolidated
                
                for ctx_id, content in cold_contexts[:-5]:  # Keep recent 5
                    data = json.loads(content)
                    summary_items.append(data.get('what', ''))
                    ids_to_decay.append(ctx_id)
                
                if summary_items:
                    # Create consolidated memory
                    metadata = {
                        'items': summary_items[:20], 
                        'count': len(summary_items),
                        'consolidated_at': datetime.now().isoformat()
                    }
                    self.store.save(
                        f"{ctx_type}_consolidated",
                        f"Consolidated {ctx_type} memories",
                        metadata
                    )
                    
                    # Mark originals with very low decay (not delete)
                    for ctx_id in ids_to_decay:
                        self.store.mark_important(ctx_id, 0.1)  # Very low importance
                    
                    consolidated += len(ids_to_decay)
        
        return f"Consolidated:{consolidated}"
    
    def _archive_old(self, args: str) -> str:
        """Archive memories that have decayed below threshold (complete forgetting)"""
        days = self.lifecycle['archive_days']
        if "days" in args:
            try:
                import re
                match = re.search(r'\d+', args)
                if match:
                    days = int(match.group())
            except:
                pass
        
        # Archive memories older than threshold OR with very low decay
        cutoff = datetime.now() - timedelta(days=days)
        
        # Query for old or heavily decayed memories
        old_contexts = self.store.query("""
            SELECT id, content, type, metadata FROM contexts
            WHERE created_at < ? 
            OR json_extract(metadata, '$.decay') < 0.2
            OR json_extract(metadata, '$.importance') < 0.2
        """, (cutoff,))
        
        archived = 0
        for ctx_id, content, ctx_type, metadata in old_contexts:
            # Move memory file to archive (not delete from memory yet)
            memory_file = self.base_path / ctx_type / f"{ctx_id}.yml"
            archive_file = self.archive_path / ctx_type / f"{ctx_id}.yml"
            archive_file.parent.mkdir(exist_ok=True)
            
            # Prepare archived data
            data = json.loads(content)
            meta = json.loads(metadata) if metadata else {}
            data['archived_at'] = datetime.now().isoformat()
            data['final_decay'] = meta.get('decay', 0.1)
            data['retrieval_count'] = 0  # Track if retrieved from archive
            
            # Write to archive
            with open(archive_file, 'w') as f:
                yaml.dump(data, f, allow_unicode=True)
            
            # Move file if it exists
            if memory_file.exists():
                shutil.move(str(memory_file), str(archive_file.parent))
            
            # Remove from active memory
            self.store.forget(ctx_id)
            archived += 1
        
        return f"Archived:{archived} (>{days}d or decay<20%)"
    
    def _forget_old(self, args: str) -> str:
        """Completely forget very old archived memories (permanent deletion)"""
        days = self.lifecycle['forget_days']
        if "days" in args:
            try:
                import re
                match = re.search(r'\d+', args)
                if match:
                    days = int(match.group())
            except:
                pass
        
        forgotten = 0
        cutoff = datetime.now() - timedelta(days=days)
        
        for archive_file in self.archive_path.glob("**/*.yml"):
            # Check archive age and retrieval count
            if datetime.fromtimestamp(archive_file.stat().st_mtime) < cutoff:
                # Check if it was never retrieved
                with open(archive_file, 'r') as f:
                    data = yaml.safe_load(f)
                    retrieval_count = data.get('retrieval_count', 0)
                
                # Only forget if old AND rarely accessed
                if retrieval_count < 2:  # Retrieved less than 2 times
                    archive_file.unlink()
                    forgotten += 1
        
        return f"Forgotten:{forgotten} (>{days}d & retrieval<2)"
    
    def _retrieve_from_archive(self, query: str) -> str:
        """Retrieve memory from archive back to active memory"""
        # Extract search term from query
        search_term = query.lower().replace("archive retrieve", "").replace("归档恢复", "").strip()
        
        if not search_term:
            return "Specify search term for retrieval"
        
        retrieved = []
        for archive_file in self.archive_path.glob("**/*.yml"):
            with open(archive_file, 'r') as f:
                data = yaml.safe_load(f)
            
            # Search in archived content
            if search_term in str(data).lower():
                # Update retrieval count
                data['retrieval_count'] = data.get('retrieval_count', 0) + 1
                data['last_retrieved'] = datetime.now().isoformat()
                
                # Write back to archive with updated count
                with open(archive_file, 'w') as f:
                    yaml.dump(data, f, allow_unicode=True)
                
                # Restore to active memory
                what = data.get('what', 'Retrieved archive')
                details = data.get('details')
                ctx_type = data.get('type', 'retrieved')
                
                ctx_id = self.store.save(ctx_type, what, details)
                retrieved.append(what[:50])
        
        if retrieved:
            return f"Retrieved {len(retrieved)}: " + " | ".join(retrieved[:5])
        return f"No archived items for '{search_term}'"
    
    def _cleanup_memory(self) -> str:
        """Run full memory lifecycle: decay -> consolidate -> archive -> forget"""
        results = []
        results.append(self._decay_memory())       # Apply attention decay
        results.append(self._consolidate_memory()) # Consolidate similar memories
        results.append(self._archive_old(""))      # Archive fully decayed
        results.append(self._forget_old(""))       # Delete very old archives
        return " | ".join(results)
    
    def _show_lifecycle(self) -> str:
        """Show memory lifecycle with decay factors"""
        return (
            f"Lifecycle: Hot({int(self.decay['hot']*100)}%,<{self.lifecycle['hot_hours']}h)→"
            f"Warm({int(self.decay['warm']*100)}%,<{self.lifecycle['warm_days']}d)→"
            f"Cold({int(self.decay['cold']*100)}%,<{self.lifecycle['cold_days']}d)→"
            f"Archive(0%,<{self.lifecycle['archive_days']}d)→"
            f"Forget(>{self.lifecycle['forget_days']}d)"
        )
    
    def _help(self) -> str:
        """Return help information"""
        return """虚拟公司管理查询:

自然语言查询示例:
  - "我们现在有多少维度在管理"
  - "任务队列情况"
  - "整体记忆全景"
  - "当前焦点是什么"
  - "有哪些活跃项目"
  - "团队/智能体状态如何"
  - "最近的决策"
  - "有什么问题需要解决"
  - "公司统计信息"
  - "显示时间线"
  - "产品状态"
  - "项目进展"
  - "archive status" - 归档状态
  - "decay" - 应用记忆衰减
  - "consolidate" - 整合记忆
  - "archive 30" - 归档30天前记忆
  - "forget 90" - 遗忘90天前归档
  - "cleanup" - 完整清理周期
  - "lifecycle" - 显示生命周期
  - "archive retrieve <query>" - 从归档恢复
  - "lesson add <rule>" - 添加教训/规则
  - "lesson list" - 查看所有教训规则
  - "rule check <action>" - 检查是否违反规则
  - "spec add <name> <definition>" - 添加技术规范
  - "spec list" - 查看所有技术规范
  - "spec get <name>" - 查询特定规范
  - "ctx resources" - 查看CTX系统资源使用情况

公司维度:
  • 核心维度: 项目、产品、任务、资源、智能体、规范、文档、程序
  • 运营维度: 决策、知识、问题、流程
  • 新兴维度: 动态发现和增长中...

记忆生命周期:
  • Hot (100%): < 24小时, 完全注意力
  • Warm (70%): < 7天, 注意力下降
  • Cold (30%): < 30天, 低注意力
  • Archive (0%): < 90天, 需要主动恢复
  • Forget: > 365天, 完全遗忘
  • Rules: 永久记忆, 不会衰减

使用方式:
  ctx("mgr", "你的查询")
  
示例:
  ctx("mgr", "公司有多少维度")
  ctx("mgr", "智能体状态")
  ctx("mgr", "项目进展")
  ctx("mgr", "archive status")
  ctx("mgr", "cleanup")"""
