using System;

namespace DSEngine;

/// <summary>
/// High-level wrapper over the native replication client.
/// Handles protocol handshake, receives snapshots/deltas, sends input and RPC.
/// </summary>
public sealed class ReplicationClient : IDisposable {
    private nint _handle;
    private bool _disposed;

    public ReplicationClient() {
        _handle = NativeRepl.dse_repl_client_create();
        if (_handle == 0) throw new InvalidOperationException("Failed to create ReplicationClient");
    }

    /// <summary>Initialize with transport and registry handles (from engine internals).</summary>
    public bool Init(nint transport, nint registry) {
        ThrowIfDisposed();
        return NativeRepl.dse_repl_client_init(_handle, transport, registry) != 0;
    }

    /// <summary>Send movement input for a replicated entity.</summary>
    public void SendMove(uint netId, float dx, float dy, float dz) {
        ThrowIfDisposed();
        NativeRepl.dse_repl_client_send_move(_handle, netId, dx, dy, dz);
    }

    /// <summary>Send movement input using a Vector3 delta.</summary>
    public void SendMove(uint netId, Vector3 delta) {
        SendMove(netId, delta.X, delta.Y, delta.Z);
    }

    /// <summary>Whether the protocol handshake has completed.</summary>
    public bool IsConnected {
        get { ThrowIfDisposed(); return NativeRepl.dse_repl_client_connected(_handle) != 0; }
    }

    /// <summary>Number of mirrored entities on the client.</summary>
    public uint MirrorCount {
        get { ThrowIfDisposed(); return NativeRepl.dse_repl_client_mirror_count(_handle); }
    }

    /// <summary>Resolve a NetId to a local entity ID. Returns 0xFFFFFFFF if unknown.</summary>
    public uint ToEntity(uint netId) {
        ThrowIfDisposed();
        return NativeRepl.dse_repl_client_to_entity(_handle, netId);
    }

    /// <summary>Send an RPC to the server.</summary>
    public bool SendRpc(ushort rpcId, uint targetNetId, ReadOnlySpan<byte> payload) {
        ThrowIfDisposed();
        unsafe {
            fixed (byte* ptr = payload) {
                return NativeRepl.dse_rpc_client_send(_handle, rpcId, targetNetId,
                    (nint)ptr, (nuint)payload.Length) != 0;
            }
        }
    }

    /// <summary>Native handle (for advanced interop).</summary>
    public nint Handle => _handle;

    private void ThrowIfDisposed() {
        if (_disposed) throw new ObjectDisposedException(nameof(ReplicationClient));
    }

    public void Dispose() {
        if (!_disposed) {
            _disposed = true;
            if (_handle != 0) {
                NativeRepl.dse_repl_client_destroy(_handle);
                _handle = 0;
            }
        }
    }
}
