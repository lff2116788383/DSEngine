using System;

namespace DSEngine;

/// <summary>
/// AOI (Area of Interest) policy for entity visibility culling.
/// </summary>
public enum AoiPolicy {
    /// <summary>All entities visible to all clients.</summary>
    Always = 0,
    /// <summary>Only entities within radius are visible.</summary>
    Distance = 1,
}

/// <summary>
/// High-level wrapper over the native replication server.
/// Manages server-authoritative entity replication, delta compression, AOI, and RPC dispatch.
/// </summary>
public sealed class ReplicationServer : IDisposable {
    private nint _handle;
    private bool _disposed;

    public ReplicationServer() {
        _handle = NativeRepl.dse_repl_server_create();
        if (_handle == 0) throw new InvalidOperationException("Failed to create ReplicationServer");
    }

    /// <summary>Initialize with transport and registry handles (from engine internals).</summary>
    public bool Init(nint transport, nint registry) {
        ThrowIfDisposed();
        return NativeRepl.dse_repl_server_init(_handle, transport, registry) != 0;
    }

    /// <summary>Mark an entity for replication. Returns assigned NetId (0 = failure).</summary>
    public uint MarkReplicated(uint entityId, uint ownerConnection) {
        ThrowIfDisposed();
        return NativeRepl.dse_repl_server_mark(_handle, entityId, ownerConnection);
    }

    /// <summary>Change entity ownership.</summary>
    public void SetOwner(uint entityId, uint ownerConnection) {
        ThrowIfDisposed();
        NativeRepl.dse_repl_server_set_owner(_handle, entityId, ownerConnection);
    }

    /// <summary>Remove entity from replication (sends despawn to all clients).</summary>
    public void Unreplicate(uint entityId) {
        ThrowIfDisposed();
        NativeRepl.dse_repl_server_unreplicate(_handle, entityId);
    }

    /// <summary>Process one replication tick: send snapshots/deltas, handle AOI.</summary>
    public void Tick() {
        ThrowIfDisposed();
        NativeRepl.dse_repl_server_tick(_handle);
    }

    /// <summary>Set AOI policy and radius (only used when policy = Distance).</summary>
    public void SetAoi(AoiPolicy policy, float radius = 100.0f) {
        ThrowIfDisposed();
        NativeRepl.dse_repl_server_set_aoi(_handle, (int)policy, radius);
    }

    /// <summary>Number of connected (handshake-complete) clients.</summary>
    public uint ClientCount {
        get { ThrowIfDisposed(); return NativeRepl.dse_repl_server_client_count(_handle); }
    }

    /// <summary>Current snapshot sequence number.</summary>
    public uint CurrentSeq {
        get { ThrowIfDisposed(); return NativeRepl.dse_repl_server_seq(_handle); }
    }

    /// <summary>Broadcast an RPC to all clients.</summary>
    public void BroadcastRpc(ushort rpcId, uint targetNetId, ReadOnlySpan<byte> payload) {
        ThrowIfDisposed();
        unsafe {
            fixed (byte* ptr = payload) {
                NativeRepl.dse_rpc_server_broadcast(_handle, rpcId, targetNetId,
                    (nint)ptr, (nuint)payload.Length);
            }
        }
    }

    /// <summary>Native handle (for advanced interop).</summary>
    public nint Handle => _handle;

    private void ThrowIfDisposed() {
        if (_disposed) throw new ObjectDisposedException(nameof(ReplicationServer));
    }

    public void Dispose() {
        if (!_disposed) {
            _disposed = true;
            if (_handle != 0) {
                NativeRepl.dse_repl_server_destroy(_handle);
                _handle = 0;
            }
        }
    }
}
