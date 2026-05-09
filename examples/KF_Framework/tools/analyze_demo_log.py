"""
深度分析 demo_play.log 二进制格式
"""
import struct
import os

INPUT_PATH = r"c:\Users\wenbilin\Desktop\temp_analysis\KF_Framework\data\input\demo_play.log"

def main():
    with open(INPUT_PATH, "rb") as f:
        data = f.read()

    sz = len(data)
    print(f"File size: {sz} bytes")
    print(f"  sz/36 = {sz/36:.4f} (remainder {sz%36})")
    print(f"  sz/34 = {sz/34:.4f} (remainder {sz%34})")
    print(f"  sz/32 = {sz/32:.4f} (remainder {sz%32})")
    print(f"  sz/40 = {sz/40:.4f} (remainder {sz%40})")
    
    # Check if file has trailing zeros
    trailing_zeros = 0
    for i in range(sz-1, -1, -1):
        if data[i] == 0:
            trailing_zeros += 1
        else:
            break
    print(f"\nTrailing zero bytes: {trailing_zeros}")
    actual_data_len = sz - trailing_zeros
    print(f"Actual data length (non-trailing-zero): {actual_data_len}")
    print(f"  actual/36 = {actual_data_len/36:.4f} (remainder {actual_data_len%36})")
    print(f"  actual/34 = {actual_data_len/34:.4f} (remainder {actual_data_len%34})")
    
    # Print first 5 potential frames at 36-byte stride
    print("\n--- First 5 frames (36-byte stride) ---")
    for i in range(min(5, sz//36)):
        off = i * 36
        chunk = data[off:off+36]
        print(f"  Frame {i} (offset {off}): {chunk.hex()}")
        mh, mv, rh, rv, zoom = struct.unpack_from("<5f", data, off)
        ps, ts, rs = struct.unpack_from("<3i", data, off+20)
        eof = struct.unpack_from("<h", data, off+32)[0]
        pad = struct.unpack_from("<H", data, off+34)[0]
        print(f"    mh={mh:.4f} mv={mv:.4f} rh={rh:.4f} rv={rv:.4f} zoom={zoom:.4f}")
        print(f"    press={ps} trigger={ts} release={rs} eof={eof} pad={pad}")
    
    # Print first 5 potential frames at 34-byte stride 
    print("\n--- First 5 frames (34-byte stride) ---")
    for i in range(min(5, sz//34)):
        off = i * 34
        chunk = data[off:off+34]
        print(f"  Frame {i} (offset {off}): {chunk.hex()}")
        mh, mv, rh, rv, zoom = struct.unpack_from("<5f", data, off)
        ps, ts, rs = struct.unpack_from("<3i", data, off+20)
        eof = struct.unpack_from("<h", data, off+32)[0]
        print(f"    mh={mh:.4f} mv={mv:.4f} rh={rh:.4f} rv={rv:.4f} zoom={zoom:.4f}")
        print(f"    press={ps} trigger={ts} release={rs} eof={eof}")
    
    # Look for end_of_file marker (short value = 1) at various stride sizes
    print("\n--- Searching for end_of_file=1 marker ---")
    for frame_size in [34, 36, 38, 40]:
        for i in range(sz // frame_size):
            off = i * frame_size + 32  # end_of_file at offset 32 in struct
            if off + 2 <= sz:
                eof = struct.unpack_from("<h", data, off)[0]
                if eof == 1:
                    print(f"  frame_size={frame_size}: eof=1 at frame {i} (byte offset {i*frame_size})")
                    # Verify this frame
                    foff = i * frame_size
                    mh, mv, rh, rv, zoom = struct.unpack_from("<5f", data, foff)
                    ps, ts, rs = struct.unpack_from("<3i", data, foff+20)
                    print(f"    data: mh={mh:.4f} mv={mv:.4f} ps={ps} ts={ts} rs={rs}")
                    break
    
    # Check if cereal adds a size header (8 bytes for uint64_t size)
    # cereal::BinaryOutputArchive sometimes writes size before data
    print("\n--- Check if cereal adds 8-byte size header ---")
    header_u64 = struct.unpack_from("<Q", data, 0)[0]
    print(f"  First 8 bytes as uint64: {header_u64}")
    if header_u64 == 34 or header_u64 == 36:
        print(f"  *** Looks like cereal size header! size={header_u64}")
        # Try reading after 8-byte header
        off = 8
        remaining = sz - 8
        print(f"  Remaining: {remaining} bytes, /36={remaining/36:.4f}, /34={remaining/34:.4f}")
    
    # Try alternative: maybe cereal saveBinary prepends a size_t (8 bytes on x64)
    # format: [size:8][data:size] per call
    print("\n--- Try cereal with size prefix per saveBinary call ---")
    off = 0
    frame_count = 0
    while off < sz:
        if off + 8 > sz:
            break
        chunk_size = struct.unpack_from("<Q", data, off)[0]
        if chunk_size > 100 or chunk_size == 0:
            if frame_count == 0:
                print(f"  First chunk_size={chunk_size} at offset 0 -- not a valid size prefix")
            break
        off += 8
        if off + chunk_size > sz:
            break
        # Read the frame data
        if chunk_size >= 34:
            mh, mv, rh, rv, zoom = struct.unpack_from("<5f", data, off)
            ps, ts, rs = struct.unpack_from("<3i", data, off+20)
            eof = struct.unpack_from("<h", data, off+32)[0]
            if frame_count < 3:
                print(f"  Frame {frame_count} (size={chunk_size}): mh={mh:.4f} mv={mv:.4f} rh={rh:.4f} rv={rv:.4f} ps={ps} ts={ts} rs={rs} eof={eof}")
            if eof == 1:
                print(f"  *** Found eof=1 at frame {frame_count}")
                frame_count += 1
                break
        off += chunk_size
        frame_count += 1
    if frame_count > 0:
        print(f"  Total frames with size-prefix format: {frame_count}, final offset: {off}")
    
    # Scan for first non-zero float in the file
    print("\n--- First non-zero values scan ---")
    found = 0
    for i in range(0, sz-4, 4):
        val = struct.unpack_from("<f", data, i)[0]
        if abs(val) > 0.0001 and abs(val) < 1000:
            print(f"  Offset {i}: float = {val:.6f} (hex: {data[i:i+4].hex()})")
            found += 1
            if found >= 20:
                break
    if found == 0:
        print("  No non-zero floats found in entire file!")
        # Check for non-zero bytes at all
        nz_bytes = sum(1 for b in data if b != 0)
        print(f"  Non-zero bytes in file: {nz_bytes}/{sz}")

if __name__ == "__main__":
    main()
