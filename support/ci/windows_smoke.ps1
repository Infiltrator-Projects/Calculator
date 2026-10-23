param(
    [Parameter(Mandatory = $true)]
    [string]$Executable
)

$ErrorActionPreference = 'Stop'
$exe = (Resolve-Path $Executable).Path

Add-Type @"
using System;
using System.Runtime.InteropServices;
using System.Text;

[StructLayout(LayoutKind.Sequential)]
public struct RECT {
    public int Left;
    public int Top;
    public int Right;
    public int Bottom;
}

[StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
public struct LOGFONT {
    public int lfHeight;
    public int lfWidth;
    public int lfEscapement;
    public int lfOrientation;
    public int lfWeight;
    public byte lfItalic;
    public byte lfUnderline;
    public byte lfStrikeOut;
    public byte lfCharSet;
    public byte lfOutPrecision;
    public byte lfClipPrecision;
    public byte lfQuality;
    public byte lfPitchAndFamily;
    [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 32)]
    public string lfFaceName;
}

public static class CalcSmoke {
    public delegate bool EnumChildProc(IntPtr hwnd, IntPtr lParam);

    [DllImport("user32.dll")]
    public static extern bool EnumChildWindows(IntPtr parent, EnumChildProc callback, IntPtr lParam);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern int GetClassName(IntPtr hwnd, StringBuilder className, int maxCount);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern int GetWindowText(IntPtr hwnd, StringBuilder text, int maxCount);

    [DllImport("user32.dll")]
    public static extern bool IsWindowVisible(IntPtr hwnd);

    [DllImport("user32.dll")]
    public static extern IntPtr SendMessage(IntPtr hwnd, uint msg, IntPtr wParam, IntPtr lParam);

    [DllImport("user32.dll", EntryPoint = "SendMessageW", CharSet = CharSet.Unicode)]
    public static extern IntPtr SendMessageText(IntPtr hwnd, uint msg, IntPtr wParam, string lParam);

    [DllImport("user32.dll", EntryPoint = "SendMessageW", CharSet = CharSet.Unicode)]
    public static extern IntPtr SendMessageReadText(IntPtr hwnd, uint msg, IntPtr wParam, StringBuilder lParam);

    [DllImport("user32.dll")]
    public static extern IntPtr GetDlgItem(IntPtr hwnd, int id);

    [DllImport("user32.dll")]
    public static extern bool GetClientRect(IntPtr hwnd, out RECT rect);

    [DllImport("user32.dll")]
    public static extern bool GetWindowRect(IntPtr hwnd, out RECT rect);

    [DllImport("gdi32.dll", CharSet = CharSet.Unicode)]
    public static extern int GetObject(IntPtr handle, int size, out LOGFONT logfont);
}
"@

$process = Start-Process $exe -PassThru
try {
    $null = $process.WaitForInputIdle(5000)
    for ($attempt = 0; $attempt -lt 20; $attempt++) {
        Start-Sleep -Milliseconds 100
        $process.Refresh()
        if ($process.HasExited) {
            throw "Windows calculator exited during startup with code $($process.ExitCode)"
        }
        if ($process.MainWindowHandle -ne [IntPtr]::Zero) { break }
    }
    if ($process.MainWindowHandle -eq [IntPtr]::Zero) {
        throw 'Windows calculator did not create a main window'
    }

    $buttons = [System.Collections.Generic.List[System.IntPtr]]::new()
    $children = [System.Collections.Generic.List[System.IntPtr]]::new()
    $callback = [CalcSmoke+EnumChildProc]{
        param([IntPtr]$hwnd, [IntPtr]$lParam)
        $children.Add($hwnd)
        $name = [System.Text.StringBuilder]::new(64)
        [void][CalcSmoke]::GetClassName($hwnd, $name, $name.Capacity)
        if ($name.ToString() -ieq 'Button') { $buttons.Add($hwnd) }
        return $true
    }
    [void][CalcSmoke]::EnumChildWindows($process.MainWindowHandle, $callback, [IntPtr]::Zero)

    if ($buttons.Count -lt 100) {
        throw "Windows keypad smoke test found only $($buttons.Count) child buttons"
    }

    $client = New-Object RECT
    if (-not [CalcSmoke]::GetClientRect($process.MainWindowHandle, [ref]$client)) {
        throw 'Could not read Windows calculator client geometry'
    }
    $clientWidth = $client.Right - $client.Left
    $clientHeight = $client.Bottom - $client.Top
    if ($clientWidth -gt 380 -or $clientHeight -gt 630) {
        throw "Windows calculator default client area regressed to ${clientWidth}x${clientHeight}"
    }

    function Get-ButtonByText([string]$label) {
        foreach ($hwnd in $buttons) {
            $text = [System.Text.StringBuilder]::new(64)
            [void][CalcSmoke]::GetWindowText($hwnd, $text, $text.Capacity)
            if ($text.ToString() -ceq $label) { return $hwnd }
        }
        return [IntPtr]::Zero
    }

    function Get-VisibleButtonByText([string]$label) {
        foreach ($hwnd in $buttons) {
            if (-not [CalcSmoke]::IsWindowVisible($hwnd)) { continue }
            $text = [System.Text.StringBuilder]::new(64)
            [void][CalcSmoke]::GetWindowText($hwnd, $text, $text.Capacity)
            if ($text.ToString() -ceq $label) { return $hwnd }
        }
        return [IntPtr]::Zero
    }

    function Get-VisibleButtonCount {
        $count = 0
        foreach ($hwnd in $buttons) {
            if ([CalcSmoke]::IsWindowVisible($hwnd)) { $count++ }
        }
        return $count
    }

    function Get-EditText([IntPtr]$hwnd) {
        # GetWindowText cannot read an Edit control in another process.
        # WM_GETTEXTLENGTH / WM_GETTEXT use Windows' system marshalling.
        $length = [CalcSmoke]::SendMessage(
            $hwnd, 0x000E, [IntPtr]::Zero, [IntPtr]::Zero).ToInt64()
        if ($length -lt 0 -or $length -gt 8192) {
            throw "Invalid Windows expression length: $length"
        }
        $buffer = [System.Text.StringBuilder]::new([int]($length + 1))
        $copied = [CalcSmoke]::SendMessageReadText(
            $hwnd, 0x000D, [IntPtr]$buffer.Capacity, $buffer).ToInt64()
        if ($copied -ne $length) {
            throw "Windows expression text changed while reading ($copied/$length)"
        }
        return $buffer.ToString()
    }

    $theme = Get-ButtonByText 'System'
    $precision = Get-ButtonByText '50d'
    $standard = Get-ButtonByText 'Standard'
    $scientific = Get-ButtonByText 'Scientific'
    $programmer = Get-ButtonByText 'Programmer'
    $tools = Get-ButtonByText 'Tools'
    $results = Get-ButtonByText 'Results'
    $bases = Get-ButtonByText 'Bases'
    if ($theme -eq [IntPtr]::Zero -or $precision -eq [IntPtr]::Zero -or
        $standard -eq [IntPtr]::Zero -or $scientific -eq [IntPtr]::Zero -or
        $programmer -eq [IntPtr]::Zero -or $tools -eq [IntPtr]::Zero -or
        $results -eq [IntPtr]::Zero -or $bases -eq [IntPtr]::Zero) {
        throw 'Windows theme/precision/mode/Tools/Results/Bases buttons were not found'
    }

    [void][CalcSmoke]::SendMessage($theme, 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero)
    Start-Sleep -Milliseconds 80
    $themeText = [System.Text.StringBuilder]::new(64)
    [void][CalcSmoke]::GetWindowText($theme, $themeText, $themeText.Capacity)
    if ($themeText.ToString() -ne 'Day') {
        throw "Windows theme cycle failed: System did not advance to Day"
    }
    [void][CalcSmoke]::SendMessage($theme, 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero)
    Start-Sleep -Milliseconds 80
    [void]$themeText.Clear()
    [void][CalcSmoke]::GetWindowText($theme, $themeText, $themeText.Capacity)
    if ($themeText.ToString() -ne 'Night') {
        throw "Windows theme cycle failed: Day did not advance to Night"
    }
    [void][CalcSmoke]::SendMessage($theme, 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero)
    Start-Sleep -Milliseconds 80

    $initialVisible = Get-VisibleButtonCount
    if (-not [CalcSmoke]::IsWindowVisible($tools) -or
        -not [CalcSmoke]::IsWindowVisible($results) -or
        [CalcSmoke]::IsWindowVisible($bases)) {
        throw 'Windows Standard mode must show Tools/Results and hide Bases'
    }

    [void][CalcSmoke]::SendMessage($scientific, 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero)
    Start-Sleep -Milliseconds 100
    $scientificVisible = Get-VisibleButtonCount
    if (-not [CalcSmoke]::IsWindowVisible($results) -or [CalcSmoke]::IsWindowVisible($bases)) {
        throw 'Windows Scientific mode must show Results and hide Bases'
    }

    [void][CalcSmoke]::SendMessage($programmer, 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero)
    Start-Sleep -Milliseconds 100
    $programmerVisible = Get-VisibleButtonCount
    if ([CalcSmoke]::IsWindowVisible($results) -or -not [CalcSmoke]::IsWindowVisible($bases)) {
        throw 'Windows Programmer mode must hide Results and show Bases'
    }

    [void][CalcSmoke]::SendMessage($standard, 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero)
    Start-Sleep -Milliseconds 100
    $standardVisible = Get-VisibleButtonCount
    if (-not [CalcSmoke]::IsWindowVisible($results) -or [CalcSmoke]::IsWindowVisible($bases)) {
        throw 'Windows Standard mode did not restore Results/Bases visibility'
    }

    if ($initialVisible -ne 37 -or $scientificVisible -ne 48 -or
        $programmerVisible -ne 56 -or $standardVisible -ne 37) {
        throw "Windows mode switching visibility contract failed: initial=$initialVisible scientific=$scientificVisible programmer=$programmerVisible standard=$standardVisible"
    }

    $memoryButton = Get-VisibleButtonByText 'MC'
    $percentButton = Get-VisibleButtonByText '%'
    if ($memoryButton -eq [IntPtr]::Zero -or $percentButton -eq [IntPtr]::Zero) {
        throw 'Windows standard memory/operator controls were not found'
    }
    $memoryRect = New-Object RECT
    $percentRect = New-Object RECT
    [void][CalcSmoke]::GetWindowRect($memoryButton, [ref]$memoryRect)
    [void][CalcSmoke]::GetWindowRect($percentButton, [ref]$percentRect)
    if ($memoryRect.Top -ge $percentRect.Top -or ($percentRect.Top - $memoryRect.Bottom) -gt 8) {
        throw 'Windows standard memory strip is not compactly separated above the keypad'
    }

    $three = Get-VisibleButtonByText '3'
    $plus = Get-VisibleButtonByText '+'
    $equals = Get-VisibleButtonByText '='
    if ($three -eq [IntPtr]::Zero -or $plus -eq [IntPtr]::Zero -or $equals -eq [IntPtr]::Zero) {
        throw 'Windows standard arithmetic buttons were not found'
    }
    [void][CalcSmoke]::SendMessage($three, 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero)
    [void][CalcSmoke]::SendMessage($plus, 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero)
    [void][CalcSmoke]::SendMessage($three, 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero)
    [void][CalcSmoke]::SendMessage($equals, 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero)
    Start-Sleep -Milliseconds 100

    $result = [CalcSmoke]::GetDlgItem($process.MainWindowHandle, 1020)
    if ($result -eq [IntPtr]::Zero) { throw 'Windows result control was not found' }
    $resultClassName = [System.Text.StringBuilder]::new(64)
    $resultText = [System.Text.StringBuilder]::new(128)
    [void][CalcSmoke]::GetClassName($result, $resultClassName, $resultClassName.Capacity)
    [void][CalcSmoke]::GetWindowText($result, $resultText, $resultText.Capacity)
    if ($resultText.ToString() -cne '6') {
        throw "Windows UI arithmetic smoke test failed: 3 + 3 displayed '$($resultText.ToString())'"
    }
    if ($resultClassName.ToString() -ine 'Static') {
        throw "Windows result copy surface regressed: expected clickable Static"
    }

    $entry = [IntPtr]::Zero
    foreach ($hwnd in $children) {
        $class = [System.Text.StringBuilder]::new(64)
        [void][CalcSmoke]::GetClassName($hwnd, $class, $class.Capacity)
        if ($class.ToString() -ieq 'Edit' -and [CalcSmoke]::IsWindowVisible($hwnd)) { $entry = $hwnd; break }
    }
    if ($entry -eq [IntPtr]::Zero) { throw 'Windows expression control was not found' }
    $four = Get-VisibleButtonByText '4'
    $five = Get-VisibleButtonByText '5'
    $ce = Get-VisibleButtonByText 'CE'
    if ($four -eq [IntPtr]::Zero -or $five -eq [IntPtr]::Zero -or $ce -eq [IntPtr]::Zero) {
        throw 'Windows editing controls were not found'
    }
    [void][CalcSmoke]::SendMessageText($entry, 0x000C, [IntPtr]::Zero, "2$([char]0x00D7)3")
    [void][CalcSmoke]::SendMessage($entry, 0x00B1, [IntPtr]2, [IntPtr]2)
    [void][CalcSmoke]::SendMessage($four, 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero)
    $edited = Get-EditText $entry
    if ($edited -cne "2$([char]0x00D7)43") {
        throw "Unicode keypad insertion failed: '$edited'"
    }
    [void][CalcSmoke]::SendMessage($five, 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero)
    $edited = Get-EditText $entry
    if ($edited -cne "2$([char]0x00D7)453") { throw "Unicode caret restoration failed: '$edited'" }
    [void][CalcSmoke]::SendMessage($equals, 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero)
    [void][CalcSmoke]::GetWindowText($result, $resultText, $resultText.Capacity)
    if ($resultText.ToString() -cne '906') { throw 'Unicode expression arithmetic failed' }
    [void][CalcSmoke]::SendMessageText($entry, 0x000C, [IntPtr]::Zero, '2+3')
    [void][CalcSmoke]::SendMessage($entry, 0x00B1, [IntPtr]::Zero, [IntPtr]::Zero)
    [void][CalcSmoke]::SendMessage($ce, 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero)
    [void][CalcSmoke]::GetWindowText($result, $resultText, $resultText.Capacity)
    if ($resultText.ToString() -cne '3') { throw 'CE retained-result display failed' }

    $title = [IntPtr]::Zero
    foreach ($hwnd in $children) {
        $name = [System.Text.StringBuilder]::new(64)
        $text = [System.Text.StringBuilder]::new(128)
        [void][CalcSmoke]::GetClassName($hwnd, $name, $name.Capacity)
        [void][CalcSmoke]::GetWindowText($hwnd, $text, $text.Capacity)
        if ($name.ToString() -ieq 'Static' -and $text.ToString() -ceq 'Calculator') {
            $title = $hwnd
            break
        }
    }
    if ($title -eq [IntPtr]::Zero) { throw 'Windows title control was not found' }

    $fontHandle = [CalcSmoke]::SendMessage($title, 0x0031, [IntPtr]::Zero, [IntPtr]::Zero)
    $font = New-Object LOGFONT
    $fontSize = [Runtime.InteropServices.Marshal]::SizeOf([type][LOGFONT])
    if ($fontHandle -eq [IntPtr]::Zero -or
        [CalcSmoke]::GetObject($fontHandle, $fontSize, [ref]$font) -eq 0) {
        throw 'Windows title font could not be inspected'
    }
    if ($font.lfFaceName -ne 'MB Corpo A Title Cond WEB') {
        throw "Windows did not use the required MB Corpo title font: $($font.lfFaceName)"
    }

    $resultFontHandle = [CalcSmoke]::SendMessage(
        $result, 0x0031, [IntPtr]::Zero, [IntPtr]::Zero)
    $resultFont = New-Object LOGFONT
    if ($resultFontHandle -eq [IntPtr]::Zero -or
        [CalcSmoke]::GetObject($resultFontHandle, $fontSize, [ref]$resultFont) -eq 0) {
        throw 'Windows result font could not be inspected'
    }
    if ($resultFont.lfFaceName -ne 'MB Corpo A Screen2 Cond WEB') {
        throw "Windows result was not set in the dedicated Screen2 readout face: $($resultFont.lfFaceName)"
    }

    Write-Host "Windows UI smoke passed: ${clientWidth}x${clientHeight}; theme, layout, arithmetic, MB Corpo brand and Screen2 result fonts are correct."
}
finally {
    if ($null -ne $process -and -not $process.HasExited) {
        Stop-Process -Id $process.Id -Force
    }
}

# Verify that the published executable remains genuinely standalone.
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) { throw 'vswhere.exe not found' }
$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if ([string]::IsNullOrWhiteSpace($vs)) { throw 'Visual Studio C++ tools not found' }
$dumpbin = Get-ChildItem "$vs\VC\Tools\MSVC\*\bin\Hostx64\x64\dumpbin.exe" |
    Sort-Object FullName -Descending |
    Select-Object -First 1
if ($null -eq $dumpbin) { throw 'dumpbin.exe not found' }

$imports = (& $dumpbin.FullName /dependents $exe | Out-String)
$forbidden = @(
    'libgcc', 'libstdc++', 'libwinpthread',
    'libgtk', 'libglib', 'libgobject', 'libgio',
    'libpango', 'libcairo', 'libharfbuzz',
    'libfontconfig', 'libfreetype',
    'VCRUNTIME', 'MSVCP'
)
foreach ($name in $forbidden) {
    if ($imports -match [regex]::Escape($name)) {
        throw "Standalone Windows boundary violated by dependency: $name"
    }
}
Write-Host 'Windows standalone dependency boundary passed.'
