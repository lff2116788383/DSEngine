namespace DSEngine;

/// <summary>
/// RPC routing target — determines direction and delivery mode.
/// </summary>
public enum RpcTarget {
    /// <summary>Client → Server (validated by server).</summary>
    Server = 0,
    /// <summary>Server → specific Client.</summary>
    Client = 1,
    /// <summary>Server → all relevant Clients (broadcast).</summary>
    Multicast = 2,
}
