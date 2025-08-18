#!/usr/bin/env python3
"""
Context Store - Unified DuckDB service layer for context management
Provides single connection management and natural APIs for all plugins
"""

import json
import yaml
from pathlib import Path
from datetime import datetime
from typing import Dict, List, Any, Optional, Union
import duckdb
import threading
from contextlib import contextmanager


class ContextStore:
    """Unified DuckDB service with connection pooling and natural APIs"""
    
    # Class-level connection management
    _instance = None
    _lock = threading.Lock()
    _db_connection = None
    
    def __new__(cls, base_path: Path = None):
        """Singleton pattern to ensure single DuckDB connection"""
        if cls._instance is None:
            with cls._lock:
                if cls._instance is None:
                    cls._instance = super().__new__(cls)
        return cls._instance
    
    def __init__(self, base_path: Path = None):
        """Initialize store with single DuckDB connection"""
        # Only initialize once
        if hasattr(self, '_initialized'):
            return
        
        self.base_path = base_path or (Path.home() / ".ai_context")
        self.base_path.mkdir(parents=True, exist_ok=True)
        
        # Initialize single DuckDB connection
        self._init_database()
        self._initialized = True
    
    def _init_database(self):
        """Initialize database with all required tables"""
        # Use single shared connection
        db_path = self.base_path / "unified.db"
        
        try:
            # Try to connect with normal mode
            self._db_connection = duckdb.connect(str(db_path))
        except Exception as e:
            # If locked, use read-only mode
            try:
                self._db_connection = duckdb.connect(str(db_path), read_only=True)
                self._read_only = True
            except:
                # Last resort: in-memory database
                self._db_connection = duckdb.connect(":memory:")
                self._read_only = False
        
        # Create all tables
        self._create_all_tables()
    
    def _create_all_tables(self):
        """Create all required tables for the system"""
        
        # Main contexts table
        self._execute_safe("""
            CREATE TABLE IF NOT EXISTS contexts (
                id TEXT PRIMARY KEY,
                content TEXT,
                type TEXT,
                created_at TIMESTAMP,
                updated_at TIMESTAMP,
                metadata TEXT,
                dimension TEXT,
                activation_score FLOAT DEFAULT 1.0
            )
        """)
        
        # Dimension tracking for management views
        self._execute_safe("""
            CREATE TABLE IF NOT EXISTS dimension_tracking (
                dimension TEXT,
                timestamp TIMESTAMP,
                state TEXT,
                record_count INTEGER,
                PRIMARY KEY (dimension, timestamp)
            )
        """)
        
        # Company state tracking
        self._execute_safe("""
            CREATE TABLE IF NOT EXISTS company_state (
                aspect TEXT PRIMARY KEY,
                state TEXT,
                updated_at TIMESTAMP
            )
        """)
        
        # File index for filesystem sync
        self._execute_safe("""
            CREATE TABLE IF NOT EXISTS file_index (
                path TEXT PRIMARY KEY,
                type TEXT,
                layer TEXT,
                size INTEGER,
                modified TIMESTAMP,
                content_hash TEXT
            )
        """)
        
        # Create useful views
        self._create_views()
    
    def _create_views(self):
        """Create database views for quick access"""
        
        # Dimension summary view with JSON error handling
        self._execute_safe("""
            CREATE OR REPLACE VIEW dimension_summary AS
            SELECT 
                dimension,
                COUNT(*) as snapshots,
                MIN(timestamp) as first_seen,
                MAX(timestamp) as last_seen,
                CASE 
                    WHEN json_valid(MAX(state)) 
                    THEN json_extract(MAX(state), '$.record_count')
                    ELSE 0
                END as latest_count
            FROM dimension_tracking
            GROUP BY dimension
        """)
        
        # Recent activity view
        self._execute_safe("""
            CREATE OR REPLACE VIEW recent_activity AS
            SELECT 
                id,
                type,
                json_extract(content, '$.what') as what,
                updated_at
            FROM contexts
            ORDER BY updated_at DESC
            LIMIT 100
        """)
    
    def _execute_safe(self, query: str, params: tuple = None):
        """Execute query safely, handling read-only mode"""
        try:
            if params:
                return self._db_connection.execute(query, params)
            else:
                return self._db_connection.execute(query)
        except Exception as e:
            # Silently fail for write operations in read-only mode
            if "read-only" in str(e).lower():
                return None
            # Re-raise other exceptions
            raise
    
    @contextmanager
    def transaction(self):
        """Context manager for transactions"""
        try:
            self._db_connection.execute("BEGIN TRANSACTION")
            yield self
            self._db_connection.execute("COMMIT")
        except Exception as e:
            self._db_connection.execute("ROLLBACK")
            raise
    
    # === Core Memory APIs ===
    
    def remember(self, what: str, details: Any = None, context_type: str = "general") -> str:
        """Remember something - returns ID for later recall"""
        context_id = f"{context_type}_{datetime.now().timestamp()}"
        
        content = {
            "what": what,
            "details": details,
            "type": context_type,
            "when": datetime.now().isoformat()
        }
        
        result = self._execute_safe("""
            INSERT OR REPLACE INTO contexts 
            (id, content, type, created_at, updated_at, metadata, dimension, activation_score)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?)
        """, (
            context_id,
            json.dumps(content, ensure_ascii=False),
            context_type,
            datetime.now(),
            datetime.now(),
            json.dumps({"importance": 1.0}),
            context_type,  # Use context_type as dimension
            1.0  # Default activation score
        ))
        
        # Also save to file for transparency
        if result:
            self._persist_to_file(context_id, content, context_type)
        
        return context_id
    
    def recall(self, about: str, limit: int = 10) -> List[Dict]:
        """Recall contexts about something"""
        result = self._execute_safe("""
            SELECT id, content FROM contexts
            WHERE content LIKE ?
            ORDER BY updated_at DESC
            LIMIT ?
        """, (f'%{about}%', limit))
        
        if result:
            contexts = []
            for _, content in result.fetchall():
                try:
                    # Try to parse as JSON
                    data = json.loads(content)
                    # Ensure 'what' key exists for backward compatibility
                    if 'what' not in data:
                        # Handle migrated or legacy data
                        if isinstance(content, str) and content == "migrated_from_ctx_v2":
                            data = {'what': 'Migrated context', 'details': {'migration': 'ctx_v2'}}
                        else:
                            data = {'what': str(content)[:100], 'details': content}
                    contexts.append(data)
                except (json.JSONDecodeError, TypeError):
                    # Handle non-JSON content (legacy or corrupted data)
                    contexts.append({
                        'what': str(content)[:100] if content else 'Unknown',
                        'details': content
                    })
            return contexts
        return []
    
    def forget(self, context_id: str):
        """Remove a context"""
        self._execute_safe("DELETE FROM contexts WHERE id = ?", (context_id,))
        
        # Also remove file
        for file in self.base_path.rglob(f"{context_id}.yml"):
            file.unlink()
    
    # === Dimension Management APIs ===
    
    def track_dimension(self, dimension: str, state: Dict) -> str:
        """Track state for a specific dimension"""
        timestamp = datetime.now()
        record_count = state.get('count', 0)
        
        self._execute_safe("""
            INSERT OR REPLACE INTO dimension_tracking 
            VALUES (?, ?, ?, ?)
        """, (dimension, timestamp, json.dumps(state), record_count))
        
        return f"dimension_{dimension}_{timestamp.timestamp()}"
    
    def get_dimension_state(self, dimension: str) -> Optional[Dict]:
        """Get latest state for a dimension"""
        result = self._execute_safe("""
            SELECT state FROM dimension_tracking
            WHERE dimension = ?
            ORDER BY timestamp DESC
            LIMIT 1
        """, (dimension,))
        
        if result:
            row = result.fetchone()
            if row:
                return json.loads(row[0])
        return None
    
    def get_all_dimensions(self) -> Dict[str, Any]:
        """Get summary of all dimensions"""
        result = self._execute_safe("""
            SELECT * FROM dimension_summary
        """)
        
        if result:
            dimensions = {}
            for row in result.fetchall():
                dimensions[row[0]] = {
                    'snapshots': row[1],
                    'first_seen': row[2],
                    'last_seen': row[3],
                    'latest_count': row[4]
                }
            return dimensions
        return {}
    
    # === Company State Management ===
    
    def update_company_state(self, aspect: str, state: Dict):
        """Update company state for a specific aspect"""
        self._execute_safe("""
            INSERT OR REPLACE INTO company_state
            VALUES (?, ?, ?)
        """, (aspect, json.dumps(state), datetime.now()))
    
    def get_company_state(self, aspect: str = None) -> Dict:
        """Get company state (all or specific aspect)"""
        if aspect:
            result = self._execute_safe("""
                SELECT state FROM company_state
                WHERE aspect = ?
            """, (aspect,))
            
            if result:
                row = result.fetchone()
                if row:
                    return json.loads(row[0])
            return {}
        else:
            # Get all aspects
            result = self._execute_safe("""
                SELECT aspect, state FROM company_state
            """)
            
            if result:
                states = {}
                for aspect, state in result.fetchall():
                    states[aspect] = json.loads(state)
                return states
            return {}
    
    # === File System Integration ===
    
    def sync_file_index(self, layer: str = None):
        """Sync file system to index"""
        import hashlib
        
        layers = [layer] if layer else ['focus', 'working', 'longterm']
        
        for layer_name in layers:
            layer_dir = self.base_path / layer_name
            if layer_dir.exists():
                for file_path in layer_dir.glob('*.yml'):
                    # Calculate file hash
                    with open(file_path, 'rb') as f:
                        content_hash = hashlib.md5(f.read()).hexdigest()
                    
                    # Update index
                    self._execute_safe("""
                        INSERT OR REPLACE INTO file_index 
                        VALUES (?, ?, ?, ?, ?, ?)
                    """, (
                        str(file_path),
                        'yml',
                        layer_name,
                        file_path.stat().st_size,
                        datetime.fromtimestamp(file_path.stat().st_mtime),
                        content_hash
                    ))
    
    def get_file_stats(self) -> Dict:
        """Get file system statistics"""
        result = self._execute_safe("""
            SELECT 
                layer,
                COUNT(*) as file_count,
                SUM(size) as total_size
            FROM file_index
            GROUP BY layer
        """)
        
        if result:
            stats = {}
            for layer, count, size in result.fetchall():
                stats[layer] = {
                    'files': count,
                    'size_kb': size / 1024 if size else 0
                }
            return stats
        return {}
    
    # === Query and Analysis APIs ===
    
    def query(self, sql: str, params: tuple = None) -> List[Any]:
        """Execute raw SQL query (read-only)"""
        result = self._execute_safe(sql, params)
        if result:
            return result.fetchall()
        return []
    
    def get_summary(self) -> Dict:
        """Get comprehensive system summary"""
        total = self._execute_safe("SELECT COUNT(*) FROM contexts")
        total_count = total.fetchone()[0] if total else 0
        
        by_type = self._execute_safe("""
            SELECT type, COUNT(*) 
            FROM contexts 
            GROUP BY type
        """)
        
        recent = self._execute_safe("""
            SELECT content 
            FROM contexts 
            ORDER BY updated_at DESC 
            LIMIT 5
        """)
        
        dimensions = self.get_all_dimensions()
        file_stats = self.get_file_stats()
        
        # Process by_type results
        by_type_dict = {}
        if by_type:
            try:
                by_type_dict = dict(by_type.fetchall())
            except Exception:
                by_type_dict = {}
        
        # Parse recent contexts with error handling
        recent_contexts = []
        if recent:
            try:
                for content, in recent.fetchall():
                    try:
                        data = json.loads(content)
                        # Ensure 'what' key exists
                        if 'what' not in data:
                            if content == "migrated_from_ctx_v2":
                                data = {'what': 'Migrated context', 'details': {'migration': 'ctx_v2'}}
                            else:
                                data = {'what': str(content)[:100], 'details': content}
                        recent_contexts.append(data)
                    except (json.JSONDecodeError, TypeError):
                        # Handle non-JSON content
                        recent_contexts.append({
                            'what': str(content)[:100] if content else 'Unknown',
                            'details': content
                        })
            except Exception:
                recent_contexts = []
        
        return {
            "total_contexts": total_count,
            "by_type": by_type_dict,
            "recent": recent_contexts,
            "dimensions": dimensions,
            "file_stats": file_stats
        }
    
    # === Utility Methods ===
    
    def _persist_to_file(self, context_id: str, content: Dict, context_type: str):
        """Persist context to YAML file"""
        file_path = self.base_path / context_type / f"{context_id}.yml"
        file_path.parent.mkdir(exist_ok=True)
        
        with open(file_path, 'w', encoding='utf-8') as f:
            yaml.dump(content, f, allow_unicode=True)
    
    def mark_important(self, context_id: str, importance: float = 1.0):
        """Mark a context as important"""
        # Get current metadata
        result = self._execute_safe("""
            SELECT metadata FROM contexts WHERE id = ?
        """, (context_id,))
        
        if result:
            row = result.fetchone()
            if row:
                meta = json.loads(row[0]) if row[0] else {}
                meta['importance'] = importance
                # Update with new metadata
                self._execute_safe("""
                    UPDATE contexts 
                    SET metadata = ?
                    WHERE id = ?
                """, (json.dumps(meta), context_id))
    
    def get_important(self, limit: int = 10) -> List[Dict]:
        """Get most important contexts"""
        result = self._execute_safe("""
            SELECT content FROM contexts
            WHERE json_extract(metadata, '$.importance') > 0.5
            ORDER BY json_extract(metadata, '$.importance') DESC
            LIMIT ?
        """, (limit,))
        
        if result:
            return [json.loads(content) for content, in result.fetchall()]
        return []
    
    def save(self, context_type: str, what: str, details: Any = None) -> str:
        """Save method for compatibility with mgr plugin - delegates to remember"""
        return self.remember(what, details, context_type)
    
    def close(self):
        """Close database connection (rarely needed due to singleton)"""
        if self._db_connection:
            self._db_connection.close()
            self._db_connection = None


# Singleton instance for shared access
_store = None

def get_store(base_path: Path = None) -> ContextStore:
    """Get or create the shared context store"""
    global _store
    if _store is None:
        _store = ContextStore(base_path)
    return _store


# === Helper functions for natural usage ===

def remember(what: str, **kwargs) -> str:
    """Quick remember function"""
    return get_store().remember(what, **kwargs)

def recall(about: str, **kwargs) -> List[Dict]:
    """Quick recall function"""
    return get_store().recall(about, **kwargs)

def track_dimension(dimension: str, state: Dict):
    """Quick dimension tracking"""
    return get_store().track_dimension(dimension, state)

def query(sql: str, params: tuple = None) -> List[Any]:
    """Quick query function"""
    return get_store().query(sql, params)