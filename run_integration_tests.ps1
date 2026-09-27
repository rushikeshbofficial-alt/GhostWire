# Integration Test Suite for P2P File Transfer System
$ErrorActionPreference = "Continue"

Write-Host "=================================================="
Write-Host "   Starting P2P File Transfer Integration Tests   "
Write-Host "=================================================="

# Ensure directories exist
if (-not (Test-Path "shared")) { New-Item -ItemType Directory -Path "shared" | Out-Null }
if (-not (Test-Path "downloads")) { New-Item -ItemType Directory -Path "downloads" | Out-Null }

# Clean previous downloads
Get-ChildItem -Path "downloads" -Exclude ".gitkeep" | Remove-Item -Force

# Create test files in shared directory
[System.IO.File]::WriteAllText("$PWD/shared/hello.txt", "Hello from peer A`n")

# Create a 0-byte empty file to test zero-size transfer
[System.IO.File]::WriteAllBytes("$PWD/shared/empty.txt", @())

# Create a binary test file with random bytes (including 0x00 and 0xFF)
$binBytes = New-Object byte[] 65536
(New-Object Random 42).NextBytes($binBytes)
$binBytes[0] = 0
$binBytes[1] = 255
$binBytes[100] = 0
[System.IO.File]::WriteAllBytes("$PWD/shared/binary.bin", $binBytes)

# Create a 1MB file to test chunking across 8192-byte buffer boundaries
$largeBytes = New-Object byte[] 1048576
for ($i = 0; $i -lt 1048576; $i++) {
    $largeBytes[$i] = [byte]($i % 256)
}
[System.IO.File]::WriteAllBytes("$PWD/shared/large.dat", $largeBytes)

$passCount = 0
$totalCount = 0

function Check-Result($testName, $condition) {
    $script:totalCount++
    if ($condition) {
        Write-Host "  [PASS] $testName" -ForegroundColor Green
        $script:passCount++
    } else {
        Write-Host "  [FAIL] $testName" -ForegroundColor Red
    }
}

# Test 1: Error handling on missing shared directory
Write-Host "`nTest: Missing shared directory"
$output = & .\p2p.exe server 5001 ./invalid_nonexistent_directory 2>&1
Check-Result "Server rejects nonexistent directory with exit code 1" ($LASTEXITCODE -ne 0)

# Test 2: Bad connection (invalid port)
Write-Host "`nTest: Bad connection to unused port"
$output = & .\p2p.exe get 127.0.0.1 59999 hello.txt downloads/fail.txt 2>&1
Check-Result "Client reports connection failure cleanly" ($LASTEXITCODE -ne 0)

# Start Provider Server in background on port 5001
Write-Host "`nStarting provider server on port 5001..."
$serverProc = Start-Process -FilePath "$PWD\p2p.exe" -ArgumentList "server 5001 ./shared" -PassThru
Start-Sleep -Seconds 1

try {
    # Test 3: Localhost text file transfer (T05)
    Write-Host "`nTest: Localhost text transfer (hello.txt)"
    $outHello = & .\p2p.exe get 127.0.0.1 5001 hello.txt downloads/hello.txt 2>&1
    Write-Host ($outHello -join "`n")
    $srcHash = (Get-FileHash "shared/hello.txt" -Algorithm SHA256).Hash
    $dstHash = (Get-FileHash "downloads/hello.txt" -Algorithm SHA256).Hash
    Check-Result "T05: hello.txt transferred and SHA256 matches" ($srcHash -eq $dstHash)

    # Test 4: Binary file transfer (T06)
    Write-Host "`nTest: Binary file transfer (binary.bin)"
    $outBin = & .\p2p.exe get 127.0.0.1 5001 binary.bin downloads/binary.bin 2>&1
    Write-Host ($outBin -join "`n")
    $srcBinHash = (Get-FileHash "shared/binary.bin" -Algorithm SHA256).Hash
    $dstBinHash = (Get-FileHash "downloads/binary.bin" -Algorithm SHA256).Hash
    Check-Result "T06: binary.bin transferred and SHA256 matches" ($srcBinHash -eq $dstBinHash)

    # Test 5: Not found negative test (T07)
    Write-Host "`nTest: File not found (missing.txt)"
    $outMissing = & .\p2p.exe get 127.0.0.1 5001 missing.txt downloads/missing.txt 2>&1
    Write-Host ($outMissing -join "`n")
    $notFoundPrinted = ($outMissing -join "`n") -match "File not found on peer"
    Check-Result "T07: Server returns NOT_FOUND and client reports it" ($notFoundPrinted -and ($LASTEXITCODE -ne 0))

    # Test 6: Large file transfer across 8KB chunks (T08)
    Write-Host "`nTest: Large 1MB file transfer (large.dat)"
    $outLarge = & .\p2p.exe get 127.0.0.1 5001 large.dat downloads/large.dat 2>&1
    Write-Host ($outLarge -join "`n")
    $srcLargeHash = (Get-FileHash "shared/large.dat" -Algorithm SHA256).Hash
    $dstLargeHash = (Get-FileHash "downloads/large.dat" -Algorithm SHA256).Hash
    Check-Result "T08: 1MB file transferred via 8KB chunks and SHA256 matches" ($srcLargeHash -eq $dstLargeHash)

    # Test 7: Sequential requests (T09)
    Write-Host "`nTest: Sequential requests to verify provider stays alive"
    $outSeq = & .\p2p.exe get 127.0.0.1 5001 hello.txt downloads/hello_seq.txt 2>&1
    $seqHash = (Get-FileHash "downloads/hello_seq.txt" -Algorithm SHA256).Hash
    Check-Result "T09: Provider served sequential request without restarting" ($srcHash -eq $seqHash)

    # Test 8: Zero-byte empty file transfer
    Write-Host "`nTest: Zero-byte empty file transfer"
    $outEmpty = & .\p2p.exe get 127.0.0.1 5001 empty.txt downloads/empty.txt 2>&1
    Write-Host ($outEmpty -join "`n")
    $emptySrcHash = (Get-FileHash "shared/empty.txt" -Algorithm SHA256).Hash
    $emptyDstHash = (Get-FileHash "downloads/empty.txt" -Algorithm SHA256).Hash
    Check-Result "T11: Zero-byte file transferred and SHA256 matches" ($emptySrcHash -eq $emptyDstHash)

    # Test 9: Unopenable output path rejection and cleanup
    Write-Host "`nTest: Unopenable output path rejection"
    $outBadPath = & .\p2p.exe get 127.0.0.1 5001 hello.txt "nonexistent_subfolder_xyz/out.txt" 2>&1
    Write-Host ($outBadPath -join "`n")
    Check-Result "T12: Client cleanly rejects unopenable output path" ($LASTEXITCODE -ne 0)

    # Test 10: Path traversal rejection in requester
    Write-Host "`nTest: Path traversal filename rejection"
    $outTraversal = & .\p2p.exe get 127.0.0.1 5001 "../secret.txt" downloads/secret.txt 2>&1
    Write-Host ($outTraversal -join "`n")
    Check-Result "T13: Client rejects traversal filename" ($LASTEXITCODE -ne 0)

    # Test 11: Invalid port syntax rejection
    Write-Host "`nTest: Port validation"
    $outPort0 = & .\p2p.exe server 0 ./shared 2>&1
    $outPortAlpha = & .\p2p.exe server 5000abc ./shared 2>&1
    $outPortHigh = & .\p2p.exe get 127.0.0.1 70000 hello.txt downloads/out.txt 2>&1
    Check-Result "T14: Server and client strictly reject invalid port syntax" (($outPort0 -match "Invalid port") -and ($outPortAlpha -match "Invalid port") -and ($outPortHigh -match "Invalid port"))

    # Test 12: List command inspection
    Write-Host "`nTest: Catalog list command"
    $outList = & .\p2p.exe list ./shared 2>&1
    Write-Host ($outList -join "`n")
    Check-Result "T15: List command prints catalog entries cleanly" (($outList -join "`n") -match "Catalog inspection")

    # Test 13: Invalid IP address rejection (T10)
    Write-Host "`nTest: Bad connection to invalid IP (T10)"
    $outBadIP = & .\p2p.exe get 999.999.999.999 5001 hello.txt downloads/fail.txt 2>&1
    Check-Result "T10: Client cleanly reports connection failure for invalid IP" ($LASTEXITCODE -ne 0)

} finally {
    Write-Host "`nStopping provider server..."
    if ($serverProc -and -not $serverProc.HasExited) {
        Stop-Process -Id $serverProc.Id -Force
    }
}

Write-Host "`n=================================================="
Write-Host "Integration Test Summary: $passCount / $totalCount Passed"
Write-Host "=================================================="

if ($passCount -eq $totalCount) {
    exit 0
} else {
    exit 1
}
