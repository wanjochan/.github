#!/usr/bin/env python3
"""JavaScript/Node.js execution plugin for AI MCP service"""

import subprocess
import tempfile
import os
import traceback

class JavaScriptPlugin:
    """JavaScript/Node.js code executor plugin"""
    
    @property
    def name(self) -> str:
        return "js"
    
    @property
    def description(self) -> str:
        return "Execute JavaScript/Node.js code"
    
    def __init__(self):
        """Initialize JavaScript executor"""
        self.node_available = self._check_node()
    
    def _check_node(self) -> bool:
        """Check if Node.js is available"""
        try:
            result = subprocess.run(
                ["node", "--version"],
                capture_output=True,
                text=True,
                timeout=5
            )
            return result.returncode == 0
        except Exception:
            return False
    
    async def execute(self, code: str) -> str:
        """Execute JavaScript code using Node.js"""
        if not self.node_available:
            return "Error: Node.js is not available. Please install Node.js to execute JavaScript code."
        
        try:
            # Create temporary file for the code
            with tempfile.NamedTemporaryFile(mode='w', suffix='.js', delete=False) as f:
                f.write(code)
                temp_file = f.name
            
            try:
                # Execute the code with Node.js
                result = subprocess.run(
                    ["node", temp_file],
                    capture_output=True,
                    text=True,
                    timeout=30,
                    env={**os.environ, 'NODE_NO_WARNINGS': '1'}
                )
                
                # Combine stdout and stderr
                output = result.stdout
                if result.stderr and not result.stderr.startswith('(node:'):
                    output += f"\n{result.stderr}"
                
                # Check for execution errors
                if result.returncode != 0:
                    return f"Error: JavaScript execution failed\n{output}"
                
                return output if output else "Code executed successfully (no output)"
                
            finally:
                # Clean up temporary file
                try:
                    os.unlink(temp_file)
                except Exception:
                    pass
                    
        except subprocess.TimeoutExpired:
            return "Error: JavaScript code execution timed out (30 seconds)"
        except Exception as e:
            return f"Error: {str(e)}\n{traceback.format_exc()}"

# Export plugin class
Plugin = JavaScriptPlugin