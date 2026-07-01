// Network replication P/Invoke declarations — corresponds to repl_c_api.h
using System.Runtime.InteropServices;

namespace DSEngine;

/// <summary>P/Invoke declarations for dse_repl_* / dse_rpc_* C ABI.</summary>
internal static partial class NativeRepl {
    const string Lib = "dse_engine";

    // ─── Server ──────────────────────────────────────────────────────────

    [LibraryImport(Lib, EntryPoint = "dse_repl_server_create")]
    internal static partial nint dse_repl_server_create();

    [LibraryImport(Lib, EntryPoint = "dse_repl_server_destroy")]
    internal static partial void dse_repl_server_destroy(nint srv);

    [LibraryImport(Lib, EntryPoint = "dse_repl_server_init")]
    internal static partial int dse_repl_server_init(nint srv, nint transport, nint registry);

    [LibraryImport(Lib, EntryPoint = "dse_repl_server_mark")]
    internal static partial uint dse_repl_server_mark(nint srv, uint entity, uint ownerConn);

    [LibraryImport(Lib, EntryPoint = "dse_repl_server_set_owner")]
    internal static partial void dse_repl_server_set_owner(nint srv, uint entity, uint ownerConn);

    [LibraryImport(Lib, EntryPoint = "dse_repl_server_unreplicate")]
    internal static partial void dse_repl_server_unreplicate(nint srv, uint entity);

    [LibraryImport(Lib, EntryPoint = "dse_repl_server_tick")]
    internal static partial void dse_repl_server_tick(nint srv);

    [LibraryImport(Lib, EntryPoint = "dse_repl_server_set_aoi")]
    internal static partial void dse_repl_server_set_aoi(nint srv, int policy, float radius);

    [LibraryImport(Lib, EntryPoint = "dse_repl_server_client_count")]
    internal static partial uint dse_repl_server_client_count(nint srv);

    [LibraryImport(Lib, EntryPoint = "dse_repl_server_seq")]
    internal static partial uint dse_repl_server_seq(nint srv);

    // ─── Client ──────────────────────────────────────────────────────────

    [LibraryImport(Lib, EntryPoint = "dse_repl_client_create")]
    internal static partial nint dse_repl_client_create();

    [LibraryImport(Lib, EntryPoint = "dse_repl_client_destroy")]
    internal static partial void dse_repl_client_destroy(nint cli);

    [LibraryImport(Lib, EntryPoint = "dse_repl_client_init")]
    internal static partial int dse_repl_client_init(nint cli, nint transport, nint registry);

    [LibraryImport(Lib, EntryPoint = "dse_repl_client_send_move")]
    internal static partial void dse_repl_client_send_move(nint cli, uint netId, float dx, float dy, float dz);

    [LibraryImport(Lib, EntryPoint = "dse_repl_client_connected")]
    internal static partial int dse_repl_client_connected(nint cli);

    [LibraryImport(Lib, EntryPoint = "dse_repl_client_mirror_count")]
    internal static partial uint dse_repl_client_mirror_count(nint cli);

    [LibraryImport(Lib, EntryPoint = "dse_repl_client_to_entity")]
    internal static partial uint dse_repl_client_to_entity(nint cli, uint netId);

    // ─── RPC ─────────────────────────────────────────────────────────────

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    internal delegate int RpcHandlerDelegate(uint sender, uint target, nint payload, nuint len, nint userdata);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    internal delegate int RpcValidatorDelegate(uint sender, uint target, nint userdata);

    [LibraryImport(Lib, EntryPoint = "dse_rpc_server_register", StringMarshalling = StringMarshalling.Utf8)]
    internal static partial ushort dse_rpc_server_register(nint srv, string name, int target,
        nint handler, nint validator, nint userdata);

    [LibraryImport(Lib, EntryPoint = "dse_rpc_client_register", StringMarshalling = StringMarshalling.Utf8)]
    internal static partial ushort dse_rpc_client_register(nint cli, string name, int target,
        nint handler, nint userdata);

    [LibraryImport(Lib, EntryPoint = "dse_rpc_client_send")]
    internal static partial int dse_rpc_client_send(nint cli, ushort rpcId, uint targetNetId,
        nint payload, nuint payloadSize);

    [LibraryImport(Lib, EntryPoint = "dse_rpc_server_broadcast")]
    internal static partial void dse_rpc_server_broadcast(nint srv, ushort rpcId, uint targetNetId,
        nint payload, nuint payloadSize);
}
