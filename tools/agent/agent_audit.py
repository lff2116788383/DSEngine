"""
Agent Audit Log for DSEngine Editor.

SQLite-based persistent audit trail for all Agent operations.
Records every node execution, tool call, and state transition.
"""

import json
import os
import sqlite3
import time
from dataclasses import dataclass
from typing import Optional


@dataclass
class AuditEntry:
    session_id: str
    timestamp: float
    event_type: str       # "classify" | "plan" | "approve" | "execute" | "verify" | "tool_call" | "error" | "checkpoint" | "rollback"
    task_id: Optional[str]
    node_name: str
    input_data: str       # JSON
    output_data: str      # JSON
    tokens_used: int
    duration_ms: float
    error: Optional[str]
    specialist: Optional[str]


class AuditLog:
    """SQLite persistent audit log for Agent sessions."""

    def __init__(self, db_path: Optional[str] = None):
        if db_path is None:
            project_dir = os.environ.get("DSE_PROJECT_DIR", ".")
            dse_dir = os.path.join(project_dir, ".dse")
            os.makedirs(dse_dir, exist_ok=True)
            db_path = os.path.join(dse_dir, "agent_audit.db")
        self.db_path = db_path
        self.conn = sqlite3.connect(db_path)
        self._create_tables()

    def _create_tables(self):
        self.conn.execute("""
            CREATE TABLE IF NOT EXISTS audit_log (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                session_id TEXT NOT NULL,
                timestamp REAL NOT NULL,
                event_type TEXT NOT NULL,
                task_id TEXT,
                node_name TEXT NOT NULL,
                input_data TEXT,
                output_data TEXT,
                tokens_used INTEGER DEFAULT 0,
                duration_ms REAL DEFAULT 0,
                error TEXT,
                specialist TEXT
            )
        """)
        self.conn.execute("""
            CREATE INDEX IF NOT EXISTS idx_audit_session
            ON audit_log (session_id, timestamp)
        """)
        self.conn.commit()

    def log(self, entry: AuditEntry):
        """Insert an audit entry."""
        self.conn.execute("""
            INSERT INTO audit_log
            (session_id, timestamp, event_type, task_id, node_name,
             input_data, output_data, tokens_used, duration_ms, error, specialist)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        """, (
            entry.session_id, entry.timestamp, entry.event_type,
            entry.task_id, entry.node_name,
            entry.input_data, entry.output_data,
            entry.tokens_used, entry.duration_ms,
            entry.error, entry.specialist,
        ))
        self.conn.commit()

    def log_event(self, session_id: str, event_type: str, node_name: str,
                  task_id: Optional[str] = None,
                  input_data: Optional[dict] = None,
                  output_data: Optional[dict] = None,
                  tokens_used: int = 0,
                  duration_ms: float = 0,
                  error: Optional[str] = None,
                  specialist: Optional[str] = None):
        """Convenience method to log an event with dict data."""
        self.log(AuditEntry(
            session_id=session_id,
            timestamp=time.time(),
            event_type=event_type,
            task_id=task_id,
            node_name=node_name,
            input_data=json.dumps(input_data or {}, ensure_ascii=False),
            output_data=json.dumps(output_data or {}, ensure_ascii=False),
            tokens_used=tokens_used,
            duration_ms=duration_ms,
            error=error,
            specialist=specialist,
        ))

    def get_session_log(self, session_id: str) -> list[dict]:
        """Get all audit entries for a session."""
        rows = self.conn.execute("""
            SELECT timestamp, event_type, task_id, node_name,
                   input_data, output_data, tokens_used, duration_ms, error, specialist
            FROM audit_log
            WHERE session_id = ?
            ORDER BY timestamp
        """, (session_id,)).fetchall()

        return [
            {
                "time": r[0], "event": r[1], "task_id": r[2], "node": r[3],
                "input": r[4], "output": r[5],
                "tokens": r[6], "duration_ms": r[7],
                "error": r[8], "specialist": r[9],
            }
            for r in rows
        ]

    def get_task_log(self, session_id: str, task_id: str) -> list[dict]:
        """Get audit entries for a specific task."""
        rows = self.conn.execute("""
            SELECT timestamp, event_type, node_name,
                   input_data, output_data, tokens_used, duration_ms, error
            FROM audit_log
            WHERE session_id = ? AND task_id = ?
            ORDER BY timestamp
        """, (session_id, task_id)).fetchall()

        return [
            {
                "time": r[0], "event": r[1], "node": r[2],
                "input": r[3], "output": r[4],
                "tokens": r[5], "duration_ms": r[6], "error": r[7],
            }
            for r in rows
        ]

    def get_session_summary(self, session_id: str) -> dict:
        """Get summary statistics for a session."""
        row = self.conn.execute("""
            SELECT COUNT(*), SUM(tokens_used), SUM(duration_ms),
                   COUNT(CASE WHEN error IS NOT NULL THEN 1 END)
            FROM audit_log
            WHERE session_id = ?
        """, (session_id,)).fetchone()

        return {
            "total_events": row[0] or 0,
            "total_tokens": row[1] or 0,
            "total_duration_ms": row[2] or 0,
            "error_count": row[3] or 0,
        }

    def close(self):
        if self.conn:
            self.conn.close()
            self.conn = None
