$sock = [System.Net.Sockets.Socket]::new(
    [System.Net.Sockets.AddressFamily]::InterNetwork,
    [System.Net.Sockets.SocketType]::Dgram,
    [System.Net.Sockets.ProtocolType]::Udp)
$sock.SetSocketOption(
    [System.Net.Sockets.SocketOptionLevel]::Socket,
    [System.Net.Sockets.SocketOptionName]::ReuseAddress, $true)
$sock.ReceiveBufferSize = 1048576
$sock.Bind([System.Net.IPEndPoint]::new([System.Net.IPAddress]::Any, 49152))
"bound to $($sock.LocalEndPoint)"

$buf = [byte[]]::new(2048)
$count = 0
$deadline = (Get-Date).AddSeconds(22)
while ((Get-Date) -lt $deadline) {
    if ($sock.Poll(1000000, [System.Net.Sockets.SelectMode]::SelectRead)) {
        $ep = [System.Net.EndPoint]([System.Net.IPEndPoint]::new([System.Net.IPAddress]::Any, 0))
        $n = $sock.ReceiveFrom($buf, [ref]$ep)
        $count++
        $msg = [System.Text.Encoding]::UTF8.GetString($buf, 0, $n)
        "$(Get-Date -Format HH:mm:ss.fff) from $ep -> $msg"
    }
}
"--- received $count packet(s) ---"
$sock.Close()
