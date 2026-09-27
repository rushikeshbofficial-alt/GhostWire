#!/bin/bash
set -e

echo "=================================================="
echo "   Starting P2P File Transfer Integration Tests   "
echo "=================================================="

# Ensure directories exist
mkdir -p shared downloads

# Clean previous downloads
rm -f downloads/*

# Prepare test files in shared directory
echo "Hello from peer A" > shared/hello.txt
touch shared/empty.txt

# Create a 64KB binary test file
head -c 65536 /dev/urandom > shared/binary.bin 2>/dev/null || dd if=/dev/urandom of=shared/binary.bin bs=1024 count=64 status=none

# Create a 1MB test file to test multiple 8192-byte chunks
dd if=/dev/urandom of=shared/large.dat bs=1024 count=1024 status=none

# Build project if not built
make all

echo ""
echo "Test 1: Missing shared directory rejection"
if ./p2p server 5002 ./nonexistent_dir 2>/dev/null; then
    echo "  [FAIL] Expected failure on nonexistent directory"
    exit 1
else
    echo "  [PASS] Server rejects nonexistent directory cleanly"
fi

echo ""
echo "Test 2: Bad connection error handling"
if ./p2p get 127.0.0.1 59999 hello.txt downloads/fail.txt 2>/dev/null; then
    echo "  [FAIL] Expected connection failure on unused port"
    exit 1
else
    echo "  [PASS] Requester reported connection failure cleanly"
fi

# Start Provider in background
echo ""
echo "Starting provider on port 5002 in background..."
./p2p server 5002 ./shared &
SERVER_PID=$!
sleep 1

# Cleanup hook to ensure server is killed when script exits
trap "kill -9 $SERVER_PID 2>/dev/null || true" EXIT

echo ""
echo "Test 3: Localhost text transfer (T05)"
./p2p get 127.0.0.1 5002 hello.txt downloads/hello.txt
cmp shared/hello.txt downloads/hello.txt
echo "  [PASS] T05: hello.txt byte comparison matches exactly"

echo ""
echo "Test 4: Binary file transfer (T06)"
./p2p get 127.0.0.1 5002 binary.bin downloads/binary.bin
cmp shared/binary.bin downloads/binary.bin
echo "  [PASS] T06: binary.bin byte comparison matches exactly"

echo ""
echo "Test 5: File not found rejection (T07)"
if ./p2p get 127.0.0.1 5002 missing.txt downloads/missing.txt; then
    echo "  [FAIL] Expected failure for missing.txt"
    exit 1
else
    echo "  [PASS] T07: File not found handled correctly"
fi

echo ""
echo "Test 6: Large 1MB file transfer (T08)"
./p2p get 127.0.0.1 5002 large.dat downloads/large.dat
cmp shared/large.dat downloads/large.dat
echo "  [PASS] T08: large.dat (1MB) byte comparison matches exactly"

echo ""
echo "Test 7: Sequential transfer (T09)"
./p2p get 127.0.0.1 5002 hello.txt downloads/hello_seq.txt
cmp shared/hello.txt downloads/hello_seq.txt
echo "  [PASS] T09: Provider handled multiple sequential requests"

echo ""
echo "Test 8: Zero-byte file transfer (T11)"
touch shared/empty.txt
./p2p get 127.0.0.1 5002 empty.txt downloads/empty.txt
cmp shared/empty.txt downloads/empty.txt
echo "  [PASS] T11: Zero-byte file transferred and matches"

echo ""
echo "Test 9: Unopenable output path rejection (T12)"
if ./p2p get 127.0.0.1 5002 hello.txt nonexistent_dir/out.txt 2>/dev/null; then
    echo "  [FAIL] Expected failure for unopenable output path"
    exit 1
else
    echo "  [PASS] T12: Unopenable output path rejected cleanly"
fi

echo ""
echo "Test 10: Path traversal rejection in requester (T13)"
if ./p2p get 127.0.0.1 5002 "../secret.txt" downloads/secret.txt 2>/dev/null; then
    echo "  [FAIL] Expected failure on path traversal filename"
    exit 1
else
    echo "  [PASS] T13: Traversal filename rejected cleanly"
fi

echo ""
echo "Test 11: Port syntax validation (T14)"
if ./p2p server 0 ./shared 2>/dev/null || ./p2p server 5000abc ./shared 2>/dev/null; then
    echo "  [FAIL] Server accepted invalid port syntax"
    exit 1
else
    echo "  [PASS] T14: Invalid port syntax rejected cleanly"
fi

echo ""
echo "Test 12: Catalog list command (T15)"
./p2p list ./shared
echo "  [PASS] T15: Catalog list command executed successfully"

echo ""
echo "Test 13: Bad connection to invalid IP (T10)"
if ./p2p get 999.999.999.999 5002 hello.txt downloads/fail.txt 2>/dev/null; then
    echo "  [FAIL] Expected failure for invalid IP"
    exit 1
else
    echo "  [PASS] T10: Bad connection to invalid IP handled cleanly"
fi

echo ""
echo "=================================================="
echo "All Integration Tests Passed Successfully!"
echo "=================================================="
