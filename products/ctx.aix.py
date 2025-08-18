#!/usr/bin/env python3
"""
AIX plugin for Context MCP service
Provides AI eXecutor functionality as a ctx plugin
Enables direct Python API calls without bash complications
"""

import subprocess
import json
import time
from pathlib import Path
from typing import Dict, List, Any, Optional
import sys
import os

# Add ccx to path for aix imports
sys.path.insert(0, str(Path(__file__).parent.parent / "ccx"))


class Plugin:
    """AIX virtual agent management via ctx system"""
    
    @property
    def name(self) -> str:
        return "aix"
    
    @property
    def description(self) -> str:
        return "AI eXecutor - Manage AI agent sessions without bash complications"
    
    def __init__(self):
        """Initialize AIX plugin"""
        self.aix_path = Path(__file__).parent.parent / "ccx" / "aix.py"
        if not self.aix_path.exists():
            raise FileNotFoundError(f"aix.py not found at {self.aix_path}")
    
    async def execute(self, code: str) -> str:
        """
        Execute AIX commands - async wrapper for compatibility
        """
        return self.query(code)
    
    def query(self, query: str) -> str:
        """
        Handle AIX commands via ctx interface
        
        Examples:
            ctx("aix", "list")
            ctx("aix", "send bot0 完成任务了吗")
            ctx("aix", "read bot1")
            ctx("aix", "new bot2 --backend claude")
            ctx("aix", "restart hub")
        """
        
        if not query:
            return self._help()
        
        # Parse command
        parts = query.split(maxsplit=1)
        command = parts[0].lower()
        args = parts[1] if len(parts) > 1 else ""
        
        # Route to appropriate handler
        handlers = {
            "list": self._list_sessions,
            "send": self._send_message,
            "read": self._read_session,
            "kill": self._kill_session,
            "new": self._new_session,
            "restart": self._restart_session,
            "attach": self._attach_session,
            "help": self._help,
            "status": self._status,
        }
        
        handler = handlers.get(command, self._unknown_command)
        return handler(args)
    
    def _execute_aix(self, *args) -> tuple[bool, str]:
        """Execute aix.py with arguments and return result"""
        try:
            cmd = ["python3", str(self.aix_path)] + list(args)
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=30,
                cwd=str(self.aix_path.parent.parent)  # Run from project root
            )
            
            output = result.stdout.strip()
            if result.returncode != 0 and result.stderr:
                output = f"Error: {result.stderr.strip()}"
                return False, output
            
            return True, output
            
        except subprocess.TimeoutExpired:
            return False, "Command timed out"
        except Exception as e:
            return False, f"Execution error: {str(e)}"
    
    def _list_sessions(self, args: str) -> str:
        """List all AI sessions"""
        success, output = self._execute_aix("--clone-list")
        return output
    
    def _send_message(self, args: str) -> str:
        """Send message to session(s)
        Format: send <session> <message>
        """
        if not args:
            return "Usage: send <session> <message>"
        
        parts = args.split(maxsplit=1)
        if len(parts) < 2:
            return "Error: Specify both session and message"
        
        session = parts[0]
        message = parts[1]
        
        # No need to escape - Python handles it properly
        success, output = self._execute_aix("--clone-send", session, message)
        return output
    
    def _read_session(self, args: str) -> str:
        """Read session output
        Format: read <session>
        """
        session = args.strip() if args else ""
        if not session:
            return "Usage: read <session>"
        
        success, output = self._execute_aix("--clone-read", session)
        
        # Truncate very long outputs
        if len(output) > 2000:
            output = output[:2000] + "\n... (truncated, use direct aix for full output)"
        
        return output
    
    def _kill_session(self, args: str) -> str:
        """Kill a session
        Format: kill <session>
        """
        session = args.strip() if args else ""
        if not session:
            return "Usage: kill <session>"
        
        success, output = self._execute_aix("--clone-kill", session)
        return output
    
    def _new_session(self, args: str) -> str:
        """Create new session
        Format: new <session> [--backend <backend>] [--role <role>]
        """
        if not args:
            return "Usage: new <session> [--backend <backend>] [--role <role>]"
        
        # Parse arguments
        parts = args.split()
        session = parts[0]
        
        aix_args = ["--clone-new", session]
        
        # Parse optional arguments
        i = 1
        while i < len(parts):
            if parts[i] == "--backend" and i + 1 < len(parts):
                aix_args.extend(["--backend", parts[i + 1]])
                i += 2
            elif parts[i] == "--role" and i + 1 < len(parts):
                # Collect rest as role
                role = " ".join(parts[i + 1:])
                aix_args.extend(["--clone-role", role])
                break
            else:
                i += 1
        
        success, output = self._execute_aix(*aix_args)
        return output
    
    def _restart_session(self, args: str) -> str:
        """Restart a session
        Format: restart <session>
        """
        session = args.strip() if args else ""
        if not session:
            return "Usage: restart <session>"
        
        success, output = self._execute_aix("--clone-restart", session)
        return output
    
    def _attach_session(self, args: str) -> str:
        """Attach to session (informational only via ctx)
        Format: attach <session>
        """
        session = args.strip() if args else ""
        if not session:
            return "Usage: attach <session>"
        
        return f"Cannot attach via ctx interface. Use: ./ccx/aix.py --clone-attach {session}"
    
    def _status(self, args: str) -> str:
        """Get detailed status of all sessions"""
        success, list_output = self._execute_aix("--clone-list")
        
        if not success:
            return list_output
        
        # Parse session list
        lines = list_output.split('\n')
        sessions = []
        for line in lines:
            if '•' in line:
                # Extract session name
                parts = line.split('(')
                if parts:
                    name = parts[0].replace('•', '').strip()
                    sessions.append(name)
        
        if not sessions:
            return "No active sessions"
        
        output = ["=== AI Agent Status Dashboard ===\n"]
        
        # Get status of each session
        for session in sessions:
            output.append(f"\n📍 {session}:")
            
            # Get last few lines of session
            success, session_output = self._execute_aix("--clone-read", session)
            if success and session_output:
                # Get last meaningful line
                lines = session_output.split('\n')
                last_lines = [l for l in lines[-5:] if l.strip() and not l.startswith('[')]
                if last_lines:
                    output.append(f"   Status: {last_lines[-1][:100]}")
                else:
                    output.append("   Status: Active")
            else:
                output.append("   Status: Unknown")
        
        return "\n".join(output)
    
    def _help(self, args: str = "") -> str:
        """Show help information"""
        return """AIX Plugin Commands (via ctx):

Basic Usage:
  ctx("aix", "list")                    - List all AI sessions
  ctx("aix", "send bot0 消息内容")      - Send message to session
  ctx("aix", "read bot1")               - Read session output
  ctx("aix", "status")                  - Detailed status of all sessions

Session Management:
  ctx("aix", "new bot2 --backend claude --role 开发者")
  ctx("aix", "restart hub")
  ctx("aix", "kill bot0")

Advantages over bash:
  ✅ No shell escaping issues
  ✅ Direct Python strings (supports code blocks)
  ✅ Integrated with ctx system
  ✅ Cleaner error handling

Example - Send complex message:
  from ctx import ctx
  ctx("aix", '''send bot0 修复这个函数：
  def process(data):
      return data.strip()
  ''')
"""
    
    def _unknown_command(self, args: str) -> str:
        """Handle unknown commands"""
        return f"Unknown command. Use: ctx('aix', 'help') for available commands"


# Quick test
if __name__ == "__main__":
    plugin = Plugin()
    
    # Test commands
    print("Testing AIX plugin...")
    print("\n1. List sessions:")
    print(plugin.query("list"))
    
    print("\n2. Help:")
    print(plugin.query("help"))