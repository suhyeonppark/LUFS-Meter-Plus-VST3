$udp = [System.Net.Sockets.UdpClient]::new(49152)
$udp.Client.ReceiveTimeout = 1500
$endpoint = [System.Net.IPEndPoint]::new([System.Net.IPAddress]::Any, 0)
$count = 0
$deadline = (Get-Date).AddSeconds(20)
while ((Get-Date) -lt $deadline) {
    try {
        $bytes = $udp.Receive([ref]$endpoint)
        $count++
        $msg = [System.Text.Encoding]::UTF8.GetString($bytes)
        "$(Get-Date -Format HH:mm:ss.fff) from $($endpoint.Address):$($endpoint.Port) -> $msg"
    } catch [System.Net.Sockets.SocketException] {
        # timeout — keep waiting until deadline1
    }
}
"--- received $count packet(s) over 20s ---"
$udp.Close()
