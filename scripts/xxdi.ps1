$file_name = ($args[0] -replace '\W', '_').ToLower()
$file_bytes = [System.IO.File]::ReadAllBytes($args[0])
$hex_array = New-Object string[] $file_bytes.Length
for ($i = 0; $i -lt $file_bytes.Length; $i++) {
    $hex_array[$i] = '0x{0:x2}' -f $file_bytes[$i]
}
$hex_string = [string]::Join(', ', $hex_array)
"unsigned char $file_name[] = { $hex_string };"
"unsigned int ${file_name}_len = $($file_bytes.Length);"