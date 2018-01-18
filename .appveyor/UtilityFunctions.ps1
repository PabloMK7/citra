# Set-up Visual Studio Command Prompt environment for PowerShell
pushd "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\Common7\Tools\"
cmd /c "VsDevCmd.bat -arch=x64 & set" | foreach {
    if ($_ -match "=") {
        $v = $_.split("="); Set-Item -Force -Path "ENV:\$($v[0])" -Value "$($v[1])"
    }
}
popd

function Which ($search_path, $name) {
    ($search_path).Split(";") | Get-ChildItem -Filter $name | Select -First 1 -Exp FullName
}

function GetDeps ($search_path, $binary) {
    ((dumpbin /dependents $binary).Where({ $_ -match "dependencies:"}, "SkipUntil") | Select-String "[^ ]*\.dll").Matches | foreach {
        Which $search_path $_.Value
    }
}

function RecursivelyGetDeps ($search_path, $binary) {
    $final_deps = @()
    $deps_to_process = GetDeps $search_path $binary
    while ($deps_to_process.Count -gt 0) {
        $current, $deps_to_process = $deps_to_process
        if ($final_deps -contains $current) { continue }

        # Is this a system dll file?
        # We use the same algorithm that cmake uses to determine this.
        if ($current -match "$([regex]::Escape($env:SystemRoot))\\sys") { continue }
        if ($current -match "$([regex]::Escape($env:WinDir))\\sys") { continue }
        if ($current -match "\\msvc[^\\]+dll") { continue }
        if ($current -match "\\api-ms-win-[^\\]+dll") { continue }

        $final_deps += $current
        $new_deps = GetDeps $search_path $current
        $deps_to_process += ($new_deps | ?{-not ($final_deps -contains $_)})
    }
    return $final_deps
}
