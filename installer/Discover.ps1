function Get-Property([string]$Id,[string]$Key) {
    (Get-PnpDeviceProperty -InstanceId $Id -KeyName $Key -ErrorAction Stop).Data
}
function Get-PodglowPhysicalRadio([string]$Virtual,[string]$Vendor,[string]$Product) {
    if ($Virtual -notmatch '\\WPODS[0-9A-F]+$') { return $Virtual }
    $physical=[string](Get-ItemProperty -LiteralPath 'HKLM:\SOFTWARE\WPods\Service' -Name RadioInstanceId -ErrorAction SilentlyContinue).RadioInstanceId
    if ($physical -notmatch ('^USB\\VID_'+[regex]::Escape($Vendor)+'&PID_'+[regex]::Escape($Product)+'\\[^\r\n]+$') -or $physical -match '\\WPODS') {
        throw 'Podglow is active, but its matching physical Bluetooth adapter could not be verified.'
    }
    $service=[string](Get-Property $physical 'DEVPKEY_Device_Service')
    if ($service -notin @('WINUSB','BTHUSB')) { throw 'Unsupported Podglow physical-adapter driver.' }
    return $physical
}
function Get-NativeMouseTwins($Device) {
    if ($Device.VirtualAdapter -eq $Device.Adapter) { return }
    foreach ($node in @(Get-PnpDevice -ErrorAction Stop | Where-Object {
        $_.InstanceId -match '^BTHENUM\\\{00001124-.*PID&0323\\' -and
        $_.InstanceId -like ('*'+$Device.Address+'_C*') -and $_.InstanceId -ne $Device.Instance
    })) {
        try {
            if (Get-Property $node.InstanceId 'DEVPKEY_Device_IsPresent') { continue }
            $ancestor=$node.InstanceId
            for($depth=0;$depth -lt 16;$depth++) {
                if($ancestor -ieq $Device.Adapter){$node.InstanceId;break}
                $ancestor=[string](Get-Property $ancestor 'DEVPKEY_Device_Parent')
            }
        } catch { continue }
    }
}
function Get-GlideMouseDevices {
    $mice = @(Get-PnpDevice -PresentOnly -ErrorAction Stop | Where-Object {
        $_.InstanceId -match '^BTHENUM\\\{00001124-.*VID&0001004C_PID&0323\\'
    })
    foreach ($mouse in $mice) {
        $match = [regex]::Match($mouse.InstanceId,'([0-9A-Fa-f]{12})_C[0-9A-Fa-f]+$')
        if (!$match.Success) { continue }
        $ancestor = $mouse.InstanceId
        for ($depth=0; $depth -lt 16; $depth++) {
            if ($ancestor -match '^USB\\VID_([0-9A-F]{4})&PID_([0-9A-F]{4})\\') { break }
            $ancestor = [string](Get-Property $ancestor 'DEVPKEY_Device_Parent')
        }
        if ($ancestor -notmatch '^USB\\VID_([0-9A-F]{4})&PID_([0-9A-F]{4})\\') { continue }
        $adapterVendor=$Matches[1];$adapterProduct=$Matches[2]
        $virtualAdapter=$ancestor
        $ancestor=Get-PodglowPhysicalRadio $ancestor $adapterVendor $adapterProduct
        $hub = [string](Get-Property $ancestor 'DEVPKEY_Device_Parent')
        # Version 0.1 supports a direct USB root-hub child only. Do not widen to
        # class filters or silently select a different hub on unsupported PCs.
        if ($hub -notmatch '^USB\\ROOT_HUB') { continue }
        $port = [int](Get-Property $ancestor 'DEVPKEY_Device_Address')
        if ($port -lt 1 -or $port -gt 127) { continue }
        $driverInf = '';try {$driverInf=[string](Get-Property $mouse.InstanceId 'DEVPKEY_Device_DriverInfPath')}catch{}
        $filters = @((Get-ItemProperty -LiteralPath ('HKLM:\SYSTEM\CurrentControlSet\Enum\'+$mouse.InstanceId) -Name LowerFilters -ErrorAction SilentlyContinue).LowerFilters | Where-Object {$_})
        [pscustomobject]@{Name='Magic Mouse (USB-C)';Address=$match.Groups[1].Value.ToUpperInvariant();Instance=$mouse.InstanceId;Adapter=$ancestor;VirtualAdapter=$virtualAdapter;Hub=$hub;Port=$port;Vid=$adapterVendor;Pid=$adapterProduct;DriverInf=$driverInf;AppleBound=($filters -contains 'applewirelessmouse')}
    }
}
