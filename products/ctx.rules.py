#!/usr/bin/env python3
"""
Rule Engine plugin for Context MCP service
Lightweight rule execution system to prevent AI hallucination and forgetfulness
"""

import json
import time
from pathlib import Path
from datetime import datetime
from typing import Dict, List, Any, Optional

class Plugin:
    """Rule-based execution engine for AIX virtual company"""
    
    @property
    def name(self) -> str:
        return "rules"
    
    @property
    def description(self) -> str:
        return "Lightweight rule engine for preventing errors and enforcing workflows"
    
    def __init__(self):
        """Initialize rule engine"""
        self.base_path = Path.home() / ".ai_context" / "rules"
        self.base_path.mkdir(parents=True, exist_ok=True)
        
        # Rule categories with priorities
        self.rule_categories = {
            'critical': 100,  # Must never violate
            'workflow': 80,   # Standard workflow rules
            'best_practice': 60,  # Recommended practices
            'optimization': 40,  # Performance optimizations
        }
        
        # Load rules from storage
        self.rules = self._load_rules()
        
        # Violation tracking
        self.violations_path = self.base_path / "violations.json"
        self.violations = self._load_violations()
    
    def _load_rules(self) -> Dict[str, List[Dict]]:
        """Load rules from storage"""
        rules = {}
        for category in self.rule_categories:
            rule_file = self.base_path / f"{category}.json"
            if rule_file.exists():
                with open(rule_file, 'r') as f:
                    rules[category] = json.load(f)
            else:
                rules[category] = []
        
        # Initialize with critical default rules if empty
        if not rules['critical']:
            rules['critical'] = [
                {
                    'id': 'no_parallel_tasks',
                    'condition': 'api_slow',
                    'action': 'forbid_parallel_tasks',
                    'description': 'Never use parallel Task tool when API is slow'
                },
                {
                    'id': 'process_singleton',
                    'condition': 'mcp_service_start',
                    'action': 'check_existing_process',
                    'description': 'Only one instance of each MCP service allowed'
                },
                {
                    'id': 'record_errors',
                    'condition': 'error_occurred',
                    'action': 'save_to_memory',
                    'description': 'Always record errors to prevent repetition'
                }
            ]
            self._save_rules(rules)
        
        return rules
    
    def _save_rules(self, rules: Dict[str, List[Dict]]):
        """Save rules to storage"""
        for category, rule_list in rules.items():
            rule_file = self.base_path / f"{category}.json"
            with open(rule_file, 'w') as f:
                json.dump(rule_list, f, indent=2)
    
    def _load_violations(self) -> List[Dict]:
        """Load violation history"""
        if self.violations_path.exists():
            with open(self.violations_path, 'r') as f:
                return json.load(f)
        return []
    
    def _save_violations(self):
        """Save violation history"""
        with open(self.violations_path, 'w') as f:
            json.dump(self.violations[-100:], f, indent=2)  # Keep last 100
    
    async def execute(self, command: str) -> str:
        """Execute rule commands"""
        parts = command.split(maxsplit=2)
        if not parts:
            return self._help()
        
        action = parts[0].lower()
        
        # Route to appropriate handler
        handlers = {
            'check': self._check_action,
            'add': self._add_rule,
            'list': self._list_rules,
            'validate': self._validate_workflow,
            'violations': self._show_violations,
            'enforce': self._enforce_rules,
            'status': self._status,
            'help': self._help
        }
        
        handler = handlers.get(action, self._help)
        return handler(parts[1:] if len(parts) > 1 else [])
    
    def _check_action(self, args: List[str]) -> str:
        """Check if an action violates any rules"""
        if not args:
            return "Usage: check <action_description>"
        
        action = ' '.join(args)
        violations = []
        
        # Check against all rules
        for category, priority in self.rule_categories.items():
            for rule in self.rules.get(category, []):
                if self._matches_rule(action, rule):
                    violations.append({
                        'category': category,
                        'priority': priority,
                        'rule': rule['id'],
                        'description': rule['description']
                    })
        
        if violations:
            # Record violation
            self.violations.append({
                'timestamp': datetime.now().isoformat(),
                'action': action,
                'violations': violations
            })
            self._save_violations()
            
            # Format response
            response = f"⚠️ Action violates {len(violations)} rule(s):\n"
            for v in sorted(violations, key=lambda x: -x['priority']):
                response += f"  [{v['category']}] {v['description']}\n"
            response += "\n❌ Action BLOCKED"
            return response
        
        return "✅ Action allowed - no rule violations"
    
    def _matches_rule(self, action: str, rule: Dict) -> bool:
        """Check if action matches rule condition"""
        action_lower = action.lower()
        condition = rule.get('condition', '').lower()
        
        # Simple keyword matching for now
        keywords = {
            'api_slow': ['parallel', 'task tool', 'multiple task', 'concurrent'],
            'mcp_service_start': ['mcp', 'server', 'start', 'launch'],
            'error_occurred': ['error', 'failed', 'exception', 'traceback']
        }
        
        if condition in keywords:
            return any(kw in action_lower for kw in keywords[condition])
        
        return condition in action_lower
    
    def _add_rule(self, args: List[str]) -> str:
        """Add a new rule"""
        if len(args) < 2:
            return "Usage: add <category> <rule_description>"
        
        category = args[0].lower()
        if category not in self.rule_categories:
            return f"Invalid category. Must be one of: {', '.join(self.rule_categories)}"
        
        description = ' '.join(args[1:])
        
        # Create rule
        rule = {
            'id': f"rule_{int(time.time())}",
            'condition': description.split(':')[0] if ':' in description else description,
            'action': 'check_and_warn',
            'description': description,
            'created': datetime.now().isoformat()
        }
        
        self.rules[category].append(rule)
        self._save_rules(self.rules)
        
        return f"✅ Rule added to {category}: {rule['id']}"
    
    def _list_rules(self, args: List[str]) -> str:
        """List all rules"""
        response = "=== Active Rules ===\n"
        
        for category in sorted(self.rule_categories, key=lambda x: -self.rule_categories[x]):
            rules = self.rules.get(category, [])
            if rules:
                response += f"\n[{category.upper()}] (Priority: {self.rule_categories[category]})\n"
                for rule in rules:
                    response += f"  • {rule['id']}: {rule['description']}\n"
        
        return response
    
    def _validate_workflow(self, args: List[str]) -> str:
        """Validate current workflow state"""
        checks = []
        
        # Check process health
        import subprocess
        try:
            result = subprocess.run(['ps', 'aux'], capture_output=True, text=True)
            mcp_processes = len([l for l in result.stdout.split('\n') if '.py server' in l])
            checks.append(f"{'✅' if mcp_processes < 10 else '⚠️'} MCP processes: {mcp_processes}")
        except:
            checks.append("❌ Could not check processes")
        
        # Check violations
        recent_violations = len([v for v in self.violations 
                                if datetime.fromisoformat(v['timestamp']) > 
                                datetime.now().replace(hour=0, minute=0)])
        checks.append(f"{'✅' if recent_violations == 0 else '⚠️'} Violations today: {recent_violations}")
        
        return "=== Workflow Validation ===\n" + '\n'.join(checks)
    
    def _show_violations(self, args: List[str]) -> str:
        """Show recent violations"""
        if not self.violations:
            return "No violations recorded"
        
        response = "=== Recent Violations ===\n"
        for v in self.violations[-5:]:
            response += f"\n{v['timestamp']}\n"
            response += f"Action: {v['action']}\n"
            response += f"Violations: {len(v['violations'])}\n"
        
        return response
    
    def _enforce_rules(self, args: List[str]) -> str:
        """Enforce rules by cleaning up violations"""
        actions_taken = []
        
        # Kill duplicate MCP processes
        import subprocess
        try:
            result = subprocess.run(['ps', 'aux'], capture_output=True, text=True)
            processes = {}
            for line in result.stdout.split('\n'):
                if '.py server' in line:
                    parts = line.split()
                    if len(parts) > 10:
                        pid = parts[1]
                        cmd = ' '.join(parts[10:])
                        if cmd not in processes:
                            processes[cmd] = []
                        processes[cmd].append(pid)
            
            for cmd, pids in processes.items():
                if len(pids) > 1:
                    # Keep newest, kill others
                    for pid in pids[:-1]:
                        subprocess.run(['kill', '-9', pid])
                        actions_taken.append(f"Killed duplicate process {pid}")
        except:
            pass
        
        if actions_taken:
            return "=== Rules Enforced ===\n" + '\n'.join(actions_taken)
        return "No rule violations to enforce"
    
    def _status(self, args: List[str]) -> str:
        """Show rule engine status"""
        total_rules = sum(len(rules) for rules in self.rules.values())
        return f"""=== Rule Engine Status ===
Total rules: {total_rules}
Categories: {', '.join(self.rule_categories)}
Violations today: {len([v for v in self.violations if datetime.fromisoformat(v['timestamp']).date() == datetime.now().date()])}
Status: Active"""
    
    def _help(self, args: List[str] = None) -> str:
        """Show help"""
        return """Rule Engine Commands:
  check <action> - Check if action violates rules
  add <category> <rule> - Add new rule
  list - List all active rules
  validate - Validate current workflow
  violations - Show recent violations
  enforce - Enforce rules (cleanup)
  status - Show engine status
  
Categories: critical, workflow, best_practice, optimization"""